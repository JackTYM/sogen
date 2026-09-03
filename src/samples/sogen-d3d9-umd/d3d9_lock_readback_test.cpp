// A texture Lock has to hand the app the subresource's *current* bytes, not just a scratch buffer to
// write into: D3D9 apps routinely lock a populated surface, overwrite part of it, and unlock, expecting
// the rest to survive. The driver satisfies that by having the host fill the app-facing buffer directly
// (d3d9_command_protocol.hpp's lock_request::data_address), so a break anywhere along that path -- wrong
// address, missing copy, wrong length -- silently replaces the untouched region with whatever the
// allocation happened to contain. Nothing else in this suite reads a lock's contents back: every other
// texture test only ever writes a full surface, which passes just as well when the read side returns
// garbage.
//
// Four properties, each checked against the exact bytes written rather than a tolerance:
//   1. a full-surface write-only lock's bytes survive to the next lock;
//   2. a lock through which the app rewrites only a sub-region preserves everything outside it;
//   3. a D3DLOCK_DISCARD lock's bytes reach the surface like any other write;
//   4. the GPU samples the final content, so the checks above cannot pass on a CPU-side buffer the
//      upload path never delivered.
//
// Property 2 deliberately drives its sub-region through a whole-surface LockRect rather than a RECT-
// scoped one: this driver does not implement D3DDDIARG_LOCK's Area field yet (a RECT lock returns the
// surface origin, verified live 2026-09-03), which is a separate gap from anything this test covers and
// would mask the read-side property with an addressing failure.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // UV rides in COLOR0 (D3DFVF_DIFFUSE), the convention the rest of this suite uses to sidestep its
    // documented TEXCOORD0-interpolation quirk.
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; float4 color : COLOR0; };
struct VSOutput { float4 pos : POSITION; float4 color : COLOR0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.color = input.color;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
sampler2D s0 : register(s0);
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return float4(tex2D(s0, input.color.rg).rgb, 1.0);
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    struct Rgb
    {
        int r, g, b;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;
    constexpr int kTexSize = 16;
    constexpr int kRectLeft = 4;
    constexpr int kRectTop = 4;
    constexpr int kRectRight = 12;
    constexpr int kRectBottom = 12;
    constexpr int kProbeGrid = 4;
    constexpr int kProbeCount = kProbeGrid * kProbeGrid;

    bool in_rect(const int x, const int y)
    {
        return x >= kRectLeft && x < kRectRight && y >= kRectTop && y < kRectBottom;
    }

    // Three per-texel patterns, each a bijection of (x, y) in every channel pair, so a texel that ends up
    // holding a neighbour's value -- or another pattern's -- is a hard mismatch rather than a near miss.
    Rgb pattern_base(const int x, const int y)
    {
        return {16 * x + 8, 16 * y + 8, 128};
    }

    Rgb pattern_rect(const int x, const int y)
    {
        return {255 - (16 * x + 8), 255 - (16 * y + 8), 32};
    }

    Rgb pattern_discard(const int x, const int y)
    {
        return {16 * y + 8, 200, 16 * x + 8};
    }

    Rgb expected_final(const int x, const int y)
    {
        return in_rect(x, y) ? pattern_rect(x, y) : pattern_base(x, y);
    }

    DWORD to_argb(const Rgb c)
    {
        return D3DCOLOR_ARGB(255, c.r, c.g, c.b);
    }

    // Writes `pattern` into the [left, right) x [top, bottom) texel range of an already-locked whole
    // surface, leaving every texel outside that range exactly as the lock delivered it.
    void write_subregion(const D3DLOCKED_RECT& lr, const int left, const int top, const int right, const int bottom,
                         Rgb (*pattern)(int, int))
    {
        const LONG pitch = lr.Pitch != 0 ? lr.Pitch : static_cast<LONG>(kTexSize * 4);
        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (int y = top; y < bottom; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + static_cast<size_t>(y) * pitch);
            for (int x = left; x < right; ++x)
            {
                row[x] = to_argb(pattern(x, y));
            }
        }
    }

    // Compares every texel of an already-locked full surface against `expected`, reporting the first
    // mismatch. Returns the number of mismatching texels.
    int check_surface(const D3DLOCKED_RECT& lr, Rgb (*expected)(int, int), const char* label)
    {
        const LONG pitch = lr.Pitch != 0 ? lr.Pitch : static_cast<LONG>(kTexSize * 4);
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        int bad = 0;
        int first_x = -1;
        int first_y = -1;
        DWORD first_got = 0;
        DWORD first_want = 0;
        for (int y = 0; y < kTexSize; ++y)
        {
            const auto* row = reinterpret_cast<const DWORD*>(base + static_cast<size_t>(y) * pitch);
            for (int x = 0; x < kTexSize; ++x)
            {
                const DWORD want = to_argb(expected(x, y)) & 0x00FFFFFF;
                const DWORD got = row[x] & 0x00FFFFFF;
                if (got != want)
                {
                    if (bad == 0)
                    {
                        first_x = x;
                        first_y = y;
                        first_got = got;
                        first_want = want;
                    }
                    ++bad;
                }
            }
        }
        if (bad == 0)
        {
            printf("[d3d9-lock-readback-test] PASS: %s -- all %d texels match\n", label, kTexSize * kTexSize);
        }
        else
        {
            printf("[d3d9-lock-readback-test] FAIL: %s -- %d/%d texels differ, first at (%d,%d) got=0x%06lX want=0x%06lX\n", label, bad,
                   kTexSize * kTexSize, first_x, first_y, static_cast<unsigned long>(first_got), static_cast<unsigned long>(first_want));
        }
        return bad;
    }

    float to_ndc_x(const int screen_x)
    {
        return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f;
    }

    float to_ndc_y(const int screen_y)
    {
        return 1.0f - static_cast<float>(screen_y) / (kCanvasHeight / 2);
    }

    // One quad per probe texel, all four vertices carrying that texel's own centre UV, so the quad is a
    // flat readout of exactly one texel and no filtering or interpolation can blend a neighbour in.
    void fill_quad(Vertex* out, const int left, const int top, const int right, const int bottom, const float u, const float v)
    {
        const DWORD uv = D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(v * 255.0f + 0.5f), 0);
        out[0] = {to_ndc_x(left), to_ndc_y(top), 0.5f, uv};
        out[1] = {to_ndc_x(right), to_ndc_y(top), 0.5f, uv};
        out[2] = {to_ndc_x(right), to_ndc_y(bottom), 0.5f, uv};
        out[3] = {to_ndc_x(left), to_ndc_y(bottom), 0.5f, uv};
    }

    struct QuadRect
    {
        int left, top, right, bottom;
    };
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-lock-readback-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9lockreadbacktest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "lock-readback-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-lock-readback-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = kCanvasWidth;
    pp.BackBufferHeight = kCanvasHeight;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-lock-readback-test] CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    const HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                                    &vs_blob, nullptr);
    const HRESULT hpsc =
        D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &ps_blob, nullptr);
    printf("[d3d9-lock-readback-test] D3DCompile vs=0x%08lx ps=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    if (!vs || !ps)
    {
        printf("[d3d9-lock-readback-test] FAIL: shader creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DTexture9* tex = nullptr;
    const HRESULT hct = dev->CreateTexture(kTexSize, kTexSize, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    IDirect3DTexture9* discard_tex = nullptr;
    const HRESULT hcd =
        dev->CreateTexture(kTexSize, kTexSize, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &discard_tex, nullptr);
    printf("[d3d9-lock-readback-test] CreateTexture hr=0x%08lx/0x%08lx\n", static_cast<unsigned long>(hct),
           static_cast<unsigned long>(hcd));
    if (FAILED(hct) || !tex || FAILED(hcd) || !discard_tex)
    {
        printf("[d3d9-lock-readback-test] FAIL: CreateTexture failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    int failures = 0;
    D3DLOCKED_RECT lr{};

    // 1. Full-surface write-only lock, then a second full lock that must see exactly those bytes.
    HRESULT hl = tex->LockRect(0, &lr, nullptr, 0);
    printf("[d3d9-lock-readback-test] LockRect(full, write) hr=0x%08lx pBits=%p Pitch=%ld\n", static_cast<unsigned long>(hl), lr.pBits,
           lr.Pitch);
    if (FAILED(hl) || !lr.pBits)
    {
        printf("[d3d9-lock-readback-test] FAIL: LockRect(full, write) failed\n");
        return 1;
    }
    write_subregion(lr, 0, 0, kTexSize, kTexSize, pattern_base);
    tex->UnlockRect(0);

    hl = tex->LockRect(0, &lr, nullptr, 0);
    if (FAILED(hl) || !lr.pBits)
    {
        printf("[d3d9-lock-readback-test] FAIL: LockRect(full, readback) hr=0x%08lx\n", static_cast<unsigned long>(hl));
        ++failures;
    }
    else
    {
        failures += check_surface(lr, pattern_base, "full-lock readback") != 0 ? 1 : 0;
        tex->UnlockRect(0);
    }

    // 2. Sub-region rewrite: overwrite only the inner rectangle, then confirm both that it took the new
    // pattern and that every texel outside it still holds the original one.
    hl = tex->LockRect(0, &lr, nullptr, 0);
    printf("[d3d9-lock-readback-test] LockRect(full, sub-region rewrite) hr=0x%08lx pBits=%p Pitch=%ld\n", static_cast<unsigned long>(hl),
           lr.pBits, lr.Pitch);
    if (FAILED(hl) || !lr.pBits)
    {
        printf("[d3d9-lock-readback-test] FAIL: LockRect(full, sub-region rewrite) failed\n");
        ++failures;
    }
    else
    {
        write_subregion(lr, kRectLeft, kRectTop, kRectRight, kRectBottom, pattern_rect);
        tex->UnlockRect(0);

        hl = tex->LockRect(0, &lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hl) || !lr.pBits)
        {
            printf("[d3d9-lock-readback-test] FAIL: LockRect(full, readonly) hr=0x%08lx\n", static_cast<unsigned long>(hl));
            ++failures;
        }
        else
        {
            failures += check_surface(lr, expected_final, "sub-region read-modify-write") != 0 ? 1 : 0;
            tex->UnlockRect(0);
        }
    }

    // 3. D3DLOCK_DISCARD: the app is handed a buffer whose previous contents it promises not to read,
    // but everything it does write still has to land.
    hl = discard_tex->LockRect(0, &lr, nullptr, 0);
    if (FAILED(hl) || !lr.pBits)
    {
        printf("[d3d9-lock-readback-test] FAIL: LockRect(discard-tex, seed) hr=0x%08lx\n", static_cast<unsigned long>(hl));
        ++failures;
    }
    else
    {
        write_subregion(lr, 0, 0, kTexSize, kTexSize, pattern_base);
        discard_tex->UnlockRect(0);

        hl = discard_tex->LockRect(0, &lr, nullptr, D3DLOCK_DISCARD);
        printf("[d3d9-lock-readback-test] LockRect(discard) hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hl), lr.pBits);
        if (FAILED(hl) || !lr.pBits)
        {
            printf("[d3d9-lock-readback-test] FAIL: LockRect(discard) failed\n");
            ++failures;
        }
        else
        {
            write_subregion(lr, 0, 0, kTexSize, kTexSize, pattern_discard);
            discard_tex->UnlockRect(0);

            hl = discard_tex->LockRect(0, &lr, nullptr, D3DLOCK_READONLY);
            if (FAILED(hl) || !lr.pBits)
            {
                printf("[d3d9-lock-readback-test] FAIL: LockRect(discard, readback) hr=0x%08lx\n", static_cast<unsigned long>(hl));
                ++failures;
            }
            else
            {
                failures += check_surface(lr, pattern_discard, "discard-lock write-back") != 0 ? 1 : 0;
                discard_tex->UnlockRect(0);
            }
        }
    }

    // 4. The same final content, read back out of the GPU image this time: one quad per probe texel,
    // each sampling that texel's own centre.
    QuadRect quads[kProbeCount]{};
    int probe_x[kProbeCount]{};
    int probe_y[kProbeCount]{};
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(kProbeCount * 4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    IDirect3DSurface9* rt = nullptr;
    const HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    if (!vb || !ib || FAILED(hcrt) || !rt)
    {
        printf("[d3d9-lock-readback-test] FAIL: render resource creation failed (vb=%p ib=%p rt=%p)\n", static_cast<void*>(vb),
               static_cast<void*>(ib), static_cast<void*>(rt));
        return 1;
    }

    Vertex* v = nullptr;
    vb->Lock(0, kProbeCount * 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
    if (!v)
    {
        printf("[d3d9-lock-readback-test] FAIL: vertex buffer Lock failed\n");
        return 1;
    }
    for (int j = 0; j < kProbeGrid; ++j)
    {
        for (int i = 0; i < kProbeGrid; ++i)
        {
            const int index = j * kProbeGrid + i;
            probe_x[index] = 2 + 4 * i;
            probe_y[index] = 2 + 4 * j;
            quads[index] = {i * 160 + 20, j * 120 + 20, i * 160 + 140, j * 120 + 100};
            fill_quad(v + index * 4, quads[index].left, quads[index].top, quads[index].right, quads[index].bottom,
                      (static_cast<float>(probe_x[index]) + 0.5f) / kTexSize, (static_cast<float>(probe_y[index]) + 0.5f) / kTexSize);
        }
    }
    vb->Unlock();

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    WORD* idx = nullptr;
    ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0);
    if (!idx)
    {
        printf("[d3d9-lock-readback-test] FAIL: index buffer Lock failed\n");
        return 1;
    }
    std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
    ib->Unlock();

    dev->SetRenderTarget(0, rt);
    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, tex);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    for (int i = 0; i < kProbeCount; ++i)
    {
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, i * 4, 0, 4, 0, 2);
    }
    dev->EndScene();

    D3DLOCKED_RECT rt_lr{};
    const HRESULT hrl = rt->LockRect(&rt_lr, nullptr, D3DLOCK_READONLY);
    if (FAILED(hrl) || !rt_lr.pBits)
    {
        printf("[d3d9-lock-readback-test] FAIL: render target LockRect hr=0x%08lx\n", static_cast<unsigned long>(hrl));
        ++failures;
    }
    else
    {
        const LONG pitch = rt_lr.Pitch != 0 ? rt_lr.Pitch : static_cast<LONG>(kCanvasWidth * 4);
        for (int i = 0; i < kProbeCount; ++i)
        {
            const int cx = (quads[i].left + quads[i].right) / 2;
            const int cy = (quads[i].top + quads[i].bottom) / 2;
            const auto* p = static_cast<const unsigned char*>(rt_lr.pBits) + static_cast<size_t>(cy) * pitch + static_cast<size_t>(cx) * 4;
            const Rgb want = expected_final(probe_x[i], probe_y[i]);
            const bool ok = std::abs(static_cast<int>(p[2]) - want.r) <= 2 && std::abs(static_cast<int>(p[1]) - want.g) <= 2 &&
                            std::abs(static_cast<int>(p[0]) - want.b) <= 2;
            printf("[d3d9-lock-readback-test] %s: gpu texel(%d,%d) R=%02X G=%02X B=%02X expected R=%02X G=%02X B=%02X\n",
                   ok ? "PASS" : "FAIL", probe_x[i], probe_y[i], p[2], p[1], p[0], want.r, want.g, want.b);
            if (!ok)
            {
                ++failures;
            }
        }
        rt->UnlockRect();
    }

    rt->Release();
    ib->Release();
    vb->Release();
    discard_tex->Release();
    tex->Release();
    ps->Release();
    vs->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-lock-readback-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
