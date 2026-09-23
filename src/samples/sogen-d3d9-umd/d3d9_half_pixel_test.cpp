// D3D9-over-Vulkan pixel-centre convention test: proves a screen-aligned quad drawn with D3D9's own
// half-pixel compensation maps texels to pixels one-to-one.
//
// D3D9 rasterizes with the pixel CENTRE at integer screen coordinates; Vulkan (like D3D10+) puts it at
// integer+0.5. An app that wants an exact texel-to-pixel blit therefore shifts its quad by -0.5 pixels
// -- the documented D3D9 recipe (MSDN, "Directly Mapping Texels to Pixels") -- and every D3D9-to-modern
// translation layer has to add that half pixel back in the viewport transform. Without it, two things
// go wrong at once and neither raises an error: every sample lands exactly between two texels, so a
// LINEAR sampler returns a 50/50 blend instead of the texel (a full-screen post-process chain quietly
// becomes a chain of extra blurs), and the quad's far edge falls exactly on the last pixel's centre,
// which the top-left fill rule excludes, so the last row and column are never written at all.
//
// Design: a 32x32 source texture whose texels carry their own coordinates (R = x, G = y, both scaled)
// plus a 1-texel checkerboard in blue that any half-texel blend collapses to mid-grey. It is drawn
// 1:1 over a 32x32 render target through a quad positioned by the exact -0.5 pixel recipe, sampled
// LINEAR (POINT would hide the blend), and every destination texel is compared against its source.
// The render target is pre-cleared to a colour that appears in no source texel, so an unwritten edge
// is reported as its own distinct failure rather than as a wrong colour.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOutput { float4 pos : POSITION; float2 uv : TEXCOORD0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.uv = input.uv;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
sampler2D s0 : register(s0);
struct PSInput { float4 pos : POSITION; float2 uv : TEXCOORD0; };
float4 main(PSInput input) : COLOR0
{
    return tex2D(s0, input.uv);
}
)";

    struct Vertex
    {
        float x, y, z;
        float u, v;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_TEX1;
    constexpr int kSize = 32;
    constexpr int kStride = kSize * 4;
    constexpr BYTE kClearB = 0x7B;
    constexpr BYTE kClearG = 0x2D;
    constexpr BYTE kClearR = 0xC5;

    BYTE source_r(const int x)
    {
        return static_cast<BYTE>(x * 8 + 4);
    }

    BYTE source_g(const int y)
    {
        return static_cast<BYTE>(y * 8 + 4);
    }

    BYTE source_b(const int x, const int y)
    {
        return ((x ^ y) & 1) != 0 ? 0xFA : 0x05;
    }

    // The D3D9 recipe: a quad meant to cover pixels 0..N-1 exactly spans screen coordinates -0.5 to
    // N-0.5, because D3D9 pixel centres sit at integer coordinates.
    float to_ndc_x(const float screen_x)
    {
        return 2.0f * (screen_x - 0.5f) / kSize - 1.0f;
    }

    float to_ndc_y(const float screen_y)
    {
        return 1.0f - 2.0f * (screen_y - 0.5f) / kSize;
    }

    void release_all(IDirect3DTexture9* tex, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (tex)
        {
            tex->Release();
        }
        if (rt)
        {
            rt->Release();
        }
        if (vb)
        {
            vb->Release();
        }
        if (ib)
        {
            ib->Release();
        }
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        if (dev)
        {
            dev->Release();
        }
        if (d3d)
        {
            d3d->Release();
        }
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-half-pixel-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9halfpixeltest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "half-pixel-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 240, nullptr, nullptr,
                                wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-half-pixel-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    const HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-half-pixel-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    const HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                                    &vs_blob, &vs_errors);
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    const HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0,
                                    &ps_blob, &ps_errors);
    printf("[d3d9-half-pixel-test] D3DCompile vs hr=0x%08lx ps hr=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-half-pixel-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
        }
        if (ps_errors)
        {
            printf("[d3d9-half-pixel-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
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
        printf("[d3d9-half-pixel-test] FAIL: shader creation failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DTexture9* tex = nullptr;
    const HRESULT hct = dev->CreateTexture(kSize, kSize, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    printf("[d3d9-half-pixel-test] CreateTexture hr=0x%08lx tex=%p\n", static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    D3DLOCKED_RECT src_lr{};
    if (FAILED(tex->LockRect(0, &src_lr, nullptr, 0)) || !src_lr.pBits)
    {
        printf("[d3d9-half-pixel-test] FAIL: source LockRect failed\n");
        release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    for (int y = 0; y < kSize; ++y)
    {
        auto* row = static_cast<unsigned char*>(src_lr.pBits) + static_cast<size_t>(y) * src_lr.Pitch;
        for (int x = 0; x < kSize; ++x)
        {
            row[x * 4 + 0] = source_b(x, y);
            row[x * 4 + 1] = source_g(y);
            row[x * 4 + 2] = source_r(x);
            row[x * 4 + 3] = 0xFF;
        }
    }
    tex->UnlockRect(0);

    IDirect3DSurface9* rt = nullptr;
    const HRESULT hcrt = dev->CreateRenderTarget(kSize, kSize, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-half-pixel-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            constexpr float kFar = static_cast<float>(kSize);
            v[0] = {to_ndc_x(0.0f), to_ndc_y(0.0f), 0.5f, 0.0f, 0.0f};
            v[1] = {to_ndc_x(kFar), to_ndc_y(0.0f), 0.5f, 1.0f, 0.0f};
            v[2] = {to_ndc_x(kFar), to_ndc_y(kFar), 0.5f, 1.0f, 1.0f};
            v[3] = {to_ndc_x(0.0f), to_ndc_y(kFar), 0.5f, 0.0f, 1.0f};
            vb->Unlock();
        }
    }

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    if (ib)
    {
        WORD* idx = nullptr;
        ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0);
        if (idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
    }
    if (!vb || !ib)
    {
        printf("[d3d9-half-pixel-test] FAIL: buffer creation failed\n");
        release_all(tex, rt, vb, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetIndices(ib);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, tex);
    // LINEAR, not POINT: point sampling snaps a half-texel error back onto the intended texel and
    // would hide exactly the bug under test.
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(kClearR, kClearG, kClearB), 1.0f, 0);
    const HRESULT hdraw = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    dev->EndScene();
    printf("[d3d9-half-pixel-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdraw));

    int failures = 0;
    D3DLOCKED_RECT lr{};
    const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-half-pixel-test] LockRect(rt) hr=0x%08lx pBits=%p pitch=%ld\n", static_cast<unsigned long>(hlr), lr.pBits,
           static_cast<long>(lr.Pitch));
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const LONG pitch = lr.Pitch > 0 ? lr.Pitch : kStride;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        int unwritten = 0;
        int mismatched = 0;
        int first_bad_x = -1;
        int first_bad_y = -1;
        for (int y = 0; y < kSize; ++y)
        {
            const auto* row = base + static_cast<size_t>(y) * pitch;
            for (int x = 0; x < kSize; ++x)
            {
                const unsigned char b = row[x * 4 + 0];
                const unsigned char g = row[x * 4 + 1];
                const unsigned char r = row[x * 4 + 2];
                if (b == kClearB && g == kClearG && r == kClearR)
                {
                    ++unwritten;
                    continue;
                }
                const int dr = static_cast<int>(r) - static_cast<int>(source_r(x));
                const int dg = static_cast<int>(g) - static_cast<int>(source_g(y));
                const int db = static_cast<int>(b) - static_cast<int>(source_b(x, y));
                if (std::abs(dr) > 2 || std::abs(dg) > 2 || std::abs(db) > 2)
                {
                    if (first_bad_x < 0)
                    {
                        first_bad_x = x;
                        first_bad_y = y;
                        printf("[d3d9-half-pixel-test] first mismatch at (%d,%d): got B=%02X G=%02X R=%02X "
                               "expected B=%02X G=%02X R=%02X\n",
                               x, y, b, g, r, source_b(x, y), source_g(y), source_r(x));
                    }
                    ++mismatched;
                }
            }
        }
        printf("[d3d9-half-pixel-test] texels=%d unwritten=%d mismatched=%d\n", kSize * kSize, unwritten, mismatched);
        if (unwritten != 0)
        {
            printf("[d3d9-half-pixel-test] FAIL: %d texels were never written -- the quad's far edge lands on the "
                   "last pixel's centre, which the fill rule excludes\n",
                   unwritten);
            ++failures;
        }
        else
        {
            printf("[d3d9-half-pixel-test] PASS: every texel of the render target was written\n");
        }
        if (mismatched != 0)
        {
            printf("[d3d9-half-pixel-test] FAIL: %d texels do not match their source texel -- samples are landing "
                   "between texels instead of on them\n",
                   mismatched);
            ++failures;
        }
        else
        {
            printf("[d3d9-half-pixel-test] PASS: every texel matches its source one-to-one\n");
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-half-pixel-test] FAIL: rt LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(tex, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-half-pixel-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
