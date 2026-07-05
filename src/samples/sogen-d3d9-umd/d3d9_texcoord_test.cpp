// TEXCOORD0 varying-interpolation regression test (see .claude/plans/jazzy-giggling-cloud.md, "Fix
// the TEXCOORD0 varying-interpolation bug"). Exercises a genuine D3DFVF_XYZ|D3DFVF_TEX1 vertex format
// with a real TEXCOORD0 PS input sampling a real texture via tex2D() -- NOT d3d9_texture_test.cpp's
// D3DFVF_DIFFUSE-packed UV (COLOR0.rg) workaround.
//
// STATUS (2026-07-04): investigated as a carried-forward bug ("U interpolates correctly, V
// consistently does not"), but does NOT reproduce against the current host. Live evidence:
//   1. A scratch diagnostic (visualize-the-interpolant PS, `return float4(input.uv, 0, 1)`) against
//      the exact quad shape/position and exact UV figures the original report cited (expected 0.25 and
//      0.75) reads back both U and V correctly (within +-1/255 rounding) at every sampled point,
//      including asymmetric (non-center) screen locations.
//   2. The one concrete lead (d3d9_shader_translator.cpp passing varying_map_info to the VS
//      compile_stage call but nullptr to the PS one) was tried both ways: passing the same
//      varying_map_info to the PS call too produces byte-identical SPIR-V and byte-identical rendered
//      pixels. This is expected, not surprising -- vkd3d-shader's own ir.c only ever applies
//      vsir_program_remap_output_signature (the transform varying_map_info drives) when
//      `shader_version.type != VKD3D_SHADER_TYPE_PIXEL`; a PS's own output has no "next stage" to
//      remap for, so the asymmetry in d3d9_shader_translator.cpp is correct API usage, not a bug.
// Most likely (but circumstantial -- the original scratch diagnostic no longer exists to re-run
// directly, see HANDOFF_MACBOOK.md section 20.4) explanation: the original report predates (or was
// never re-checked against) this session's separate "real Y-flip bug -- in the new test itself, not
// the host" finding -- an inverted screen-Y-to-NDC convention in a test's own geometry placement
// produces exactly this "U fine, V looks wrong in a non-trivial way" symptom, without touching varying
// interpolation at all. This test, using the corrected convention, is the permanent regression vehicle
// proving real TEXCOORD0 sampling works; d3d9_texture_test.cpp keeps its COLOR0-packed workaround
// unchanged as its own, separately-proven-correct path.

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
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    float to_ndc_x(const int screen_x) { return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f; }

    // NDC Y: this pipeline's viewport uses Vulkan's own (unflipped) convention -- NDC y=-1 at the top of
    // the screen, y=+1 at the bottom (see d3d9_texture_test.cpp's to_ndc_y comment for the live-confirmed
    // finding this mirrors).
    float to_ndc_y(const int screen_y) { return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f; }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-texcoord-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9texcoordtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "texcoord-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-texcoord-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-texcoord-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-texcoord-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-texcoord-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-texcoord-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-texcoord-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-texcoord-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-texcoord-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    // Same procedural 640x480 four-quadrant texture as d3d9_texture_test.cpp (RED top-left, GREEN
    // top-right, BLUE bottom-left, WHITE bottom-right) -- sized to match pfnCreateResource's hardcoded
    // 640x480 KNOWN LIMITATION, same as that test.
    IDirect3DTexture9* tex = nullptr;
    HRESULT hct = dev->CreateTexture(kCanvasWidth, kCanvasHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex,
                                     nullptr);
    printf("[d3d9-texcoord-test] CreateTexture hr=0x%08lx tex=%p\n", static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }
    {
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-texcoord-test] Texture LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-texcoord-test] FAIL: texture LockRect failed\n");
            tex->Release();
            if (vs)
            {
                vs->Release();
            }
            if (ps)
            {
                ps->Release();
            }
            dev->Release();
            d3d->Release();
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < kCanvasHeight; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * kStride);
            for (int x = 0; x < kCanvasWidth; ++x)
            {
                DWORD color;
                if (x < kCanvasWidth / 2 && y < kCanvasHeight / 2)
                {
                    color = D3DCOLOR_ARGB(255, 255, 0, 0); // top-left: RED
                }
                else if (x >= kCanvasWidth / 2 && y < kCanvasHeight / 2)
                {
                    color = D3DCOLOR_ARGB(255, 0, 255, 0); // top-right: GREEN
                }
                else if (x < kCanvasWidth / 2 && y >= kCanvasHeight / 2)
                {
                    color = D3DCOLOR_ARGB(255, 0, 0, 255); // bottom-left: BLUE
                }
                else
                {
                    color = D3DCOLOR_ARGB(255, 255, 255, 255); // bottom-right: WHITE
                }
                row[x] = color;
            }
        }
        tex->UnlockRect(0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-texcoord-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-texcoord-test] FAIL: render target creation failed\n");
        tex->Release();
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // One quad at screen (40,40)-(240,200), local UV (0,0)-(1,1) across it via a genuine TEXCOORD0
    // varying (not COLOR0-packed) -- local UV (0.25,0.25)/(0.75,0.25)/(0.25,0.75)/(0.75,0.75) land in
    // all four texture quadrants, independently proving both U and V interpolate correctly (a swapped
    // or one-axis-broken interpolant would land in the wrong quadrant for at least one of these points).
    constexpr int kLeft = 40, kTop = 40, kRight = 240, kBottom = 200;
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            v[0] = {to_ndc_x(kLeft), to_ndc_y(kTop), 0.5f, 0.0f, 0.0f};
            v[1] = {to_ndc_x(kRight), to_ndc_y(kTop), 0.5f, 1.0f, 0.0f};
            v[2] = {to_ndc_x(kRight), to_ndc_y(kBottom), 0.5f, 1.0f, 1.0f};
            v[3] = {to_ndc_x(kLeft), to_ndc_y(kBottom), 0.5f, 0.0f, 1.0f};
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
        printf("[d3d9-texcoord-test] FAIL: vertex/index buffer creation failed\n");
        if (vb)
        {
            vb->Release();
        }
        if (ib)
        {
            ib->Release();
        }
        rt->Release();
        tex->Release();
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

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

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-texcoord-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-texcoord-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        struct Check
        {
            const char* name;
            int col, row;
            int expected_b, expected_g, expected_r;
        };
        // Screen points for local UV (0.25,0.25)/(0.75,0.25)/(0.25,0.75)/(0.75,0.75) within the
        // (40,40)-(240,200) quad.
        const Check checks[4] = {
            {"u=0.25,v=0.25 (RED)", 90, 80, 0, 0, 255},
            {"u=0.75,v=0.25 (GREEN)", 190, 80, 0, 255, 0},
            {"u=0.25,v=0.75 (BLUE)", 90, 160, 255, 0, 0},
            {"u=0.75,v=0.75 (WHITE)", 190, 160, 255, 255, 255},
        };
        for (const auto& c : checks)
        {
            const unsigned char* p = pixel_at(c.col, c.row);
            printf("[d3d9-texcoord-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", c.name, c.col, c.row, p[0], p[1],
                   p[2], p[3]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) ||
                !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-texcoord-test] FAIL: %s does not match the expected texture quadrant\n", c.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-texcoord-test] PASS: %s matches the expected texture quadrant\n", c.name);
            }
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-texcoord-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    rt->Release();
    tex->Release();
    vb->Release();
    ib->Release();
    if (vs)
    {
        vs->Release();
    }
    if (ps)
    {
        ps->Release();
    }
    dev->Release();
    d3d->Release();

    printf("[d3d9-texcoord-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
