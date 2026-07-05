// D3DPOOL_MANAGED regression test (see .claude/plans/jazzy-giggling-cloud.md, "Fix the
// D3DPOOL_MANAGED double-resource-creation bug"). Exercises a texture created with REAL
// D3DPOOL_MANAGED (not d3d9_texture_test.cpp's D3DUSAGE_DYNAMIC+D3DPOOL_DEFAULT workaround):
// CreateTexture(..., D3DPOOL_MANAGED, ...), LockRect a distinctive solid color into it, UnlockRect,
// SetTexture, draw a textured quad, and read back the rendered pixel.
//
// STATUS (2026-07-04): CONFIRMED PERMANENTLY FAILING, not fixable through this driver's own DDI
// surface. Three layers of live-RE, each verified independently:
//   1. The original double-pfnCreateResource/pfnTexBlt sync mechanism this test was written to prove
//      fixed is real, expected D3D9 architecture, and correctly handled (sogen_d3d9_umd.cpp's
//      umd_TexBlt / d3d9_host::tex_blt).
//   2. pfnLock/pfnUnlock never carry the app's real pixel writes for a D3DPOOL_MANAGED texture's
//      "sysmem master" copy at all -- CBaseDevice::CanDriverManageResource is unconditionally false for
//      any real D3DDDI/WDDM driver (d3d9.dll's own QueryLHDDICaps hardcodes D3DCAPS2_CANMANAGERESOURCE
//      off on every CreateDevice, live-verified by watching it get stripped even after this driver's own
//      GetCaps sets the bit).
//   3. pfnTexBlt's real argument struct (fully decompiled from the genuine caller, CD3DDDIDX10::TexBlt --
//      see D3DDDIARG_TEXBLT in d3d9_ddi.hpp) carries no pixel-data pointer either, and a full live trace
//      of every DDI call this driver receives across this test's entire run confirms no other call does.
// The real MANAGED-pool sysmem pixel data is structurally never exposed to this (or any) driver through
// any DDI call for this resource kind -- see umd_TexBlt's comment in sogen_d3d9_umd.cpp for the full
// trail. Kept in the tree (not deleted) as the regression vehicle proving this is understood, not
// merely unencountered: the sampled pixel is expected to stay black, never magenta, until a
// fundamentally different mechanism (not a DDI-surface fix) is found.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // Same UV-through-COLOR0 workaround d3d9_texture_test.cpp uses -- this test is only exercising
    // the D3DPOOL_MANAGED bug, not the separate, still-open TEXCOORD0 interpolation bug.
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
    return tex2D(s0, input.color.rg);
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    float to_ndc_x(const int screen_x)
    {
        return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f;
    }

    float to_ndc_y(const int screen_y)
    {
        return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f;
    }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-managed-texture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9managedtexturetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "managed-texture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-managed-texture-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-managed-texture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-managed-texture-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-managed-texture-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
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
    printf("[d3d9-managed-texture-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-managed-texture-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-managed-texture-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-managed-texture-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        if (vs) vs->Release();
        if (ps) ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    // The bug this test targets: CreateTexture(..., D3DPOOL_MANAGED, ...) -- no D3DUSAGE_DYNAMIC/
    // D3DPOOL_DEFAULT workaround, unlike d3d9_texture_test.cpp. Solid magenta (255,0,255), distinctive
    // against the clear color and every other test's texture colors.
    IDirect3DTexture9* tex = nullptr;
    HRESULT hct = dev->CreateTexture(kCanvasWidth, kCanvasHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr);
    printf("[d3d9-managed-texture-test] CreateTexture(D3DPOOL_MANAGED) hr=0x%08lx tex=%p\n", static_cast<unsigned long>(hct),
           static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        printf("[d3d9-managed-texture-test] FAIL: CreateTexture failed\n");
        if (vs) vs->Release();
        if (ps) ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }
    {
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-managed-texture-test] Texture LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-managed-texture-test] FAIL: texture LockRect failed\n");
            tex->Release();
            if (vs) vs->Release();
            if (ps) ps->Release();
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
                row[x] = D3DCOLOR_ARGB(255, 255, 0, 255); // solid magenta
            }
        }
        tex->UnlockRect(0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-managed-texture-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-managed-texture-test] FAIL: render target creation failed\n");
        tex->Release();
        if (vs) vs->Release();
        if (ps) ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    constexpr int kQuadW = 200;
    constexpr int kQuadH = 160;
    Vertex verts[4];
    const auto uv_color = [](const float u, const float v) {
        return D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(v * 255.0f + 0.5f), 0);
    };
    constexpr int kLeft = 40, kTop = 40;
    verts[0] = {to_ndc_x(kLeft), to_ndc_y(kTop), 0.5f, uv_color(0.0f, 0.0f)};
    verts[1] = {to_ndc_x(kLeft + kQuadW), to_ndc_y(kTop), 0.5f, uv_color(1.0f, 0.0f)};
    verts[2] = {to_ndc_x(kLeft + kQuadW), to_ndc_y(kTop + kQuadH), 0.5f, uv_color(1.0f, 1.0f)};
    verts[3] = {to_ndc_x(kLeft), to_ndc_y(kTop + kQuadH), 0.5f, uv_color(0.0f, 1.0f)};

    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            std::memcpy(v, verts, sizeof(verts));
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
        printf("[d3d9-managed-texture-test] FAIL: vertex/index buffer creation failed\n");
        if (vb) vb->Release();
        if (ib) ib->Release();
        rt->Release();
        tex->Release();
        if (vs) vs->Release();
        if (ps) ps->Release();
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

    constexpr float kClearR = 64.0f / 255.0f;
    constexpr float kClearG = 128.0f / 255.0f;
    constexpr float kClearB = 255.0f / 255.0f;
    (void)kClearR;
    (void)kClearG;
    (void)kClearB;

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
    HRESULT hd0 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-managed-texture-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd0));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-managed-texture-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        // Center of the quad: (140, 120) safely inside the drawn rect (40,40)-(240,200). Must read
        // back the texture's solid magenta (255,0,255), NOT black/transparent (the bug's symptom) and
        // NOT the clear color (which would mean the quad never drew at all).
        const unsigned char* p = pixel_at(140, 120);
        printf("[d3d9-managed-texture-test] textured pixel(140,120)=B=%02X G=%02X R=%02X A=%02X (expected MAGENTA "
               "B=FF G=00 R=FF)\n",
               p[0], p[1], p[2], p[3]);
        if (!channel_close(p[0], 255, 2) || !channel_close(p[1], 0, 2) || !channel_close(p[2], 255, 2))
        {
            printf("[d3d9-managed-texture-test] EXPECTED FAILURE (known, permanent limitation -- see this "
                   "file's header comment and umd_TexBlt's comment in sogen_d3d9_umd.cpp): pixel does not "
                   "match the MANAGED texture's known color\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-managed-texture-test] PASS: pixel matches the real D3DPOOL_MANAGED texture's color "
                   "exactly\n");
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-managed-texture-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    rt->Release();
    tex->Release();
    vb->Release();
    ib->Release();
    if (vs) vs->Release();
    if (ps) ps->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-managed-texture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
