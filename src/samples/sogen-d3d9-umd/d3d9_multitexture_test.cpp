// D3D9-over-Vulkan multi-sampler pixel-shader test: proves a single pixel shader can sample TWO
// distinct textures bound to different D3D9 sampler registers (s0 and s1) in one draw. Before the
// multi-sampler binding work (see d3d9_shader_translator.cpp's ps_sampler_bindings and
// d3d9_host.cpp's ps_bindings), the translator declared only s0's combined-image-sampler; a pixel
// shader that referenced s1 made vkd3d_shader_compile fail (the s1 sampler variable never resolved).
// This is a HOST-side failure the guest never observes directly: CreatePixelShader/DrawIndexedPrimitive
// both still report success (hr=0) from the app's point of view -- translate_d3d9_shader_pair returns
// false, ensure_programmable_pipeline returns nullptr, and execute_draw silently skips the draw,
// leaving whatever the render target was cleared to. So this exact test would NOT have failed at
// CreatePixelShader/first-draw time; it would have rendered the CLEAR color instead of YELLOW -- a
// silent wrong-pixel failure, not a crash. It is still a clean before/after discriminator for that fix,
// just via graceful degradation rather than a hard failure.
//
// Design: two solid-color textures -- texA is pure RED (1,0,0), texB is pure GREEN (0,1,0) -- bound
// to s0 and s1 respectively. One quad is drawn with a real D3DCompile()'d ps_2_0 that samples both
// and outputs s0.rgb + s1.rgb. RED + GREEN = YELLOW (1,1,0), a third color distinct from either
// input, so a correct combined result is unambiguous. The rendered pixel is read back and checked
// against YELLOW analytically. Solid textures make the check independent of UV interpolation (any
// texel of either texture is the same color), isolating this test to the sampler-binding path.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // Same VS as d3d9_texture_test.cpp: passes position through and carries UV in the COLOR0 channel
    // (D3DFVF_DIFFUSE) rather than a dedicated TEXCOORD0 varying -- see that test for the confirmed
    // TEXCOORD0-interpolation bug this convention sidesteps. UV is irrelevant here (solid textures),
    // but keeping the vertex/varying shape identical avoids reintroducing that unrelated issue.
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

    // Samples BOTH s0 and s1 -- this is the discriminator. s1 forces the translator to emit a second
    // combined-image-sampler binding; without the multi-sampler fix, compiling this PS to SPIR-V fails.
    const char* const k_pixel_shader_hlsl = R"(
sampler2D s0 : register(s0);
sampler2D s1 : register(s1);
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    float4 a = tex2D(s0, input.color.rg);
    float4 b = tex2D(s1, input.color.rg);
    return float4(a.rgb + b.rgb, 1.0);
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

    // Fill an already-locked, tightly-packed 32bpp A8R8G8B8 texture with one solid color. Pitch is not
    // populated by this UMD's Lock DDI (same known gap d3d9_texture_test.cpp documents), so a hardcoded
    // stride is used.
    void fill_solid(void* bits, const DWORD argb)
    {
        constexpr LONG kStride = kCanvasWidth * 4;
        auto* base = static_cast<unsigned char*>(bits);
        for (int y = 0; y < kCanvasHeight; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * kStride);
            for (int x = 0; x < kCanvasWidth; ++x)
            {
                row[x] = argb;
            }
        }
    }

    void release_all(IDirect3DTexture9* tex_a, IDirect3DTexture9* tex_b, IDirect3DSurface9* rt,
                     IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib, IDirect3DVertexShader9* vs,
                     IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (tex_a)
        {
            tex_a->Release();
        }
        if (tex_b)
        {
            tex_b->Release();
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

    // Create a 640x480 solid-color A8R8G8B8 texture (dynamic, D3DPOOL_DEFAULT), locking and filling it.
    IDirect3DTexture9* make_solid_texture(IDirect3DDevice9* dev, const DWORD argb, const char* label)
    {
        IDirect3DTexture9* tex = nullptr;
        HRESULT hct = dev->CreateTexture(kCanvasWidth, kCanvasHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                         &tex, nullptr);
        printf("[d3d9-multitexture-test] CreateTexture(%s) hr=0x%08lx tex=%p\n", label, static_cast<unsigned long>(hct),
               static_cast<void*>(tex));
        if (FAILED(hct) || !tex)
        {
            return nullptr;
        }
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-multitexture-test] LockRect(%s) hr=0x%08lx pBits=%p\n", label, static_cast<unsigned long>(htl),
               lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            tex->Release();
            return nullptr;
        }
        fill_solid(lr.pBits, argb);
        tex->UnlockRect(0);
        return tex;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-multitexture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9multitexturetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "multitexture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-multitexture-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-multitexture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-multitexture-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-multitexture-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-multitexture-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-multitexture-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-multitexture-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-multitexture-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DTexture9* tex_a = make_solid_texture(dev, D3DCOLOR_ARGB(255, 255, 0, 0), "RED/s0");
    IDirect3DTexture9* tex_b = make_solid_texture(dev, D3DCOLOR_ARGB(255, 0, 255, 0), "GREEN/s1");
    if (!tex_a || !tex_b)
    {
        printf("[d3d9-multitexture-test] FAIL: solid texture creation failed\n");
        release_all(tex_a, tex_b, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt,
                                           nullptr);
    printf("[d3d9-multitexture-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-multitexture-test] FAIL: render target creation failed\n");
        release_all(tex_a, tex_b, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    constexpr int kQuadW = 300;
    constexpr int kQuadH = 240;
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            fill_quad(v, 170, 120, 170 + kQuadW, 120 + kQuadH, 0.5f);
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
        printf("[d3d9-multitexture-test] FAIL: vertex/index buffer creation failed\n");
        release_all(tex_a, tex_b, rt, vb, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, tex_a);
    dev->SetTexture(1, tex_b);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-multitexture-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-multitexture-test] LockRect(rt) hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        // Center of the quad -- both textures are solid, so any covered pixel is s0(RED)+s1(GREEN).
        const unsigned char* p = base + (120 + kQuadH / 2) * kStride + (170 + kQuadW / 2) * 4;
        printf("[d3d9-multitexture-test] center pixel=B=%02X G=%02X R=%02X A=%02X (expected YELLOW: B=00 G=FF R=FF)\n",
               p[0], p[1], p[2], p[3]);
        if (!channel_close(p[0], 0, 2) || !channel_close(p[1], 255, 2) || !channel_close(p[2], 255, 2))
        {
            printf("[d3d9-multitexture-test] FAIL: combined pixel is not the RED+GREEN=YELLOW sum of both samplers\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-multitexture-test] PASS: s0(RED)+s1(GREEN) combined to YELLOW -- both samplers bound "
                   "correctly\n");
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-multitexture-test] FAIL: rt LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(tex_a, tex_b, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-multitexture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
