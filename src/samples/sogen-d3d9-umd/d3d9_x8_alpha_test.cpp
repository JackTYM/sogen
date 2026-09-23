// D3D9-over-Vulkan alpha-less-format test: proves D3DFMT_X8R8G8B8 samples with A=1.0 regardless of
// what the fourth byte holds. D3D9 defines that byte as undefined padding -- a surface created as
// X8R8G8B8 has no alpha channel at all, so a sampler always reads 1.0 there. Vulkan has no alpha-less
// 32bpp BGRA format, so d3d9_format_to_vulkan maps it to B8G8R8A8_UNORM, whose fourth byte IS the
// alpha channel; without a VK_COMPONENT_SWIZZLE_ONE on the sampled image view the shader reads
// whatever the app left in the padding byte. Anything that only ever wrote RGB leaves zero there, so
// every SRCALPHA-blended draw of such a texture comes out fully transparent -- a silent wrong-pixel
// failure with no HRESULT and no dropped draw.
//
// Design: two 640x480 solid RED textures whose fourth byte is zero in both cases, differing only in
// declared format -- one D3DFMT_X8R8G8B8, one D3DFMT_A8R8G8B8. Each is drawn as its own quad over a
// BLUE-cleared render target with SRCALPHA/INVSRCALPHA blending and a ps_2_0 that returns tex2D(s0)
// unchanged. The X8 quad must come out RED (alpha forced to 1.0, source fully replaces the target);
// the A8 quad must come out BLUE (alpha genuinely 0, source contributes nothing). The A8 quad is the
// control: it fails if blending itself is broken, which keeps a passing X8 check from being vacuous.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // Same VS convention as d3d9_multitexture_test.cpp: UV rides in the COLOR0 channel rather than a
    // dedicated TEXCOORD0 varying, sidestepping the TEXCOORD0-interpolation issue that test documents.
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

    // Returns the sampled texel unchanged, alpha included -- the blend stage is what turns the sampled
    // alpha into a visible difference.
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
    constexpr int kQuadW = 200;
    constexpr int kQuadH = 200;
    constexpr int kX8QuadLeft = 60;
    constexpr int kA8QuadLeft = 380;
    constexpr int kQuadTop = 140;

    float to_ndc_x(const int screen_x)
    {
        return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f;
    }

    float to_ndc_y(const int screen_y)
    {
        return 1.0f - static_cast<float>(screen_y) / (kCanvasHeight / 2);
    }

    void fill_quad(Vertex* out, const int left, const int top, const int right, const int bottom, const float z)
    {
        const auto uv_color = [](const float u, const float v) {
            return D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(v * 255.0f + 0.5f), 0);
        };
        out[0] = {to_ndc_x(left), to_ndc_y(top), z, uv_color(0.0f, 0.0f)};
        out[1] = {to_ndc_x(right), to_ndc_y(top), z, uv_color(1.0f, 0.0f)};
        out[2] = {to_ndc_x(right), to_ndc_y(bottom), z, uv_color(1.0f, 1.0f)};
        out[3] = {to_ndc_x(left), to_ndc_y(bottom), z, uv_color(0.0f, 1.0f)};
    }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    // Pitch is not populated by this UMD's Lock DDI (the same known gap d3d9_texture_test.cpp
    // documents), so a hardcoded tightly-packed 32bpp stride is used.
    void fill_solid(void* bits, const DWORD packed)
    {
        constexpr LONG kStride = kCanvasWidth * 4;
        auto* base = static_cast<unsigned char*>(bits);
        for (int y = 0; y < kCanvasHeight; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * kStride);
            for (int x = 0; x < kCanvasWidth; ++x)
            {
                row[x] = packed;
            }
        }
    }

    IDirect3DTexture9* make_zero_alpha_red_texture(IDirect3DDevice9* dev, const D3DFORMAT format, const char* label)
    {
        IDirect3DTexture9* tex = nullptr;
        const HRESULT hct = dev->CreateTexture(kCanvasWidth, kCanvasHeight, 1, D3DUSAGE_DYNAMIC, format, D3DPOOL_DEFAULT, &tex, nullptr);
        printf("[d3d9-x8-alpha-test] CreateTexture(%s) hr=0x%08lx tex=%p\n", label, static_cast<unsigned long>(hct),
               static_cast<void*>(tex));
        if (FAILED(hct) || !tex)
        {
            return nullptr;
        }
        D3DLOCKED_RECT lr{};
        const HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-x8-alpha-test] LockRect(%s) hr=0x%08lx pBits=%p\n", label, static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            tex->Release();
            return nullptr;
        }
        // 0x00FF0000: BGRA byte order B=0 G=0 R=255, fourth byte 0. For X8R8G8B8 that byte is padding
        // D3D9 says to ignore; for A8R8G8B8 it is a real alpha of zero.
        fill_solid(lr.pBits, 0x00FF0000u);
        tex->UnlockRect(0);
        return tex;
    }

    void release_all(IDirect3DTexture9* tex_x8, IDirect3DTexture9* tex_a8, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb_x8,
                     IDirect3DVertexBuffer9* vb_a8, IDirect3DIndexBuffer9* ib, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps,
                     IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (tex_x8)
        {
            tex_x8->Release();
        }
        if (tex_a8)
        {
            tex_a8->Release();
        }
        if (rt)
        {
            rt->Release();
        }
        if (vb_x8)
        {
            vb_x8->Release();
        }
        if (vb_a8)
        {
            vb_a8->Release();
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

    IDirect3DVertexBuffer9* make_quad_buffer(IDirect3DDevice9* dev, const int left)
    {
        IDirect3DVertexBuffer9* vb = nullptr;
        dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
        if (!vb)
        {
            return nullptr;
        }
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            fill_quad(v, left, kQuadTop, left + kQuadW, kQuadTop + kQuadH, 0.5f);
            vb->Unlock();
        }
        return vb;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-x8-alpha-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9x8alphatest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "x8-alpha-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth, kCanvasHeight,
                                nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-x8-alpha-test] FAIL: Direct3DCreate9 returned null\n");
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
    const HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-x8-alpha-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    const HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                                    &vs_blob, &vs_errors);
    printf("[d3d9-x8-alpha-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-x8-alpha-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    const HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0,
                                    &ps_blob, &ps_errors);
    printf("[d3d9-x8-alpha-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-x8-alpha-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    const HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    IDirect3DPixelShader9* ps = nullptr;
    const HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-x8-alpha-test] CreateVertexShader hr=0x%08lx CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs),
           static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DTexture9* tex_x8 = make_zero_alpha_red_texture(dev, D3DFMT_X8R8G8B8, "X8R8G8B8");
    IDirect3DTexture9* tex_a8 = make_zero_alpha_red_texture(dev, D3DFMT_A8R8G8B8, "A8R8G8B8/control");
    if (!tex_x8 || !tex_a8)
    {
        printf("[d3d9-x8-alpha-test] FAIL: texture creation failed\n");
        release_all(tex_x8, tex_a8, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    const HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-x8-alpha-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-x8-alpha-test] FAIL: render target creation failed\n");
        release_all(tex_x8, tex_a8, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    IDirect3DVertexBuffer9* vb_x8 = make_quad_buffer(dev, kX8QuadLeft);
    IDirect3DVertexBuffer9* vb_a8 = make_quad_buffer(dev, kA8QuadLeft);

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

    if (!vb_x8 || !vb_a8 || !ib)
    {
        printf("[d3d9-x8-alpha-test] FAIL: vertex/index buffer creation failed\n");
        release_all(tex_x8, tex_a8, rt, vb_x8, vb_a8, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
    dev->SetTexture(0, tex_x8);
    dev->SetStreamSource(0, vb_x8, 0, sizeof(Vertex));
    const HRESULT hd_x8 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    dev->SetTexture(0, tex_a8);
    dev->SetStreamSource(0, vb_a8, 0, sizeof(Vertex));
    const HRESULT hd_a8 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-x8-alpha-test] DrawIndexedPrimitive x8 hr=0x%08lx a8 hr=0x%08lx\n", static_cast<unsigned long>(hd_x8),
           static_cast<unsigned long>(hd_a8));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-x8-alpha-test] LockRect(rt) hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        const auto* px_x8 = base + (kQuadTop + kQuadH / 2) * kStride + (kX8QuadLeft + kQuadW / 2) * 4;
        const auto* px_a8 = base + (kQuadTop + kQuadH / 2) * kStride + (kA8QuadLeft + kQuadW / 2) * 4;
        printf("[d3d9-x8-alpha-test] X8 quad pixel=B=%02X G=%02X R=%02X (expected RED: B=00 G=00 R=FF)\n", px_x8[0], px_x8[1], px_x8[2]);
        printf("[d3d9-x8-alpha-test] A8 quad pixel=B=%02X G=%02X R=%02X (expected BLUE: B=FF G=00 R=00)\n", px_a8[0], px_a8[1], px_a8[2]);
        if (!channel_close(px_x8[0], 0, 2) || !channel_close(px_x8[1], 0, 2) || !channel_close(px_x8[2], 255, 2))
        {
            printf("[d3d9-x8-alpha-test] FAIL: X8R8G8B8 texture did not sample with alpha=1.0 -- the padding byte "
                   "leaked into the blend\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-x8-alpha-test] PASS: X8R8G8B8 sampled with alpha=1.0 despite a zero padding byte\n");
        }
        if (!channel_close(px_a8[0], 255, 2) || !channel_close(px_a8[1], 0, 2) || !channel_close(px_a8[2], 0, 2))
        {
            printf("[d3d9-x8-alpha-test] FAIL: A8R8G8B8 control quad is not the clear colour -- SRCALPHA blending "
                   "with alpha=0 is itself broken, so the X8 check above proves nothing\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-x8-alpha-test] PASS: A8R8G8B8 control blended away at alpha=0\n");
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-x8-alpha-test] FAIL: rt LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(tex_x8, tex_a8, rt, vb_x8, vb_a8, ib, vs, ps, dev, d3d);

    printf("[d3d9-x8-alpha-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
