// D3D9-over-Vulkan real mip-mapping test: proves a texture created with several REAL mip levels gets
// each level's distinct pixel data all the way to the GPU (per-subresource LockRect(level) -> host
// per-mip backing -> per-level vkCmdCopyBufferToImage) AND that the sampler actually selects a
// non-zero level (real LOD range, not the old hardcoded min_lod=max_lod=0 pinning).
//
// A 64x64 3-level texture is filled with a DIFFERENT solid color per level: level 0 (64x64) = RED,
// level 1 (32x32) = GREEN, level 2 (16x16) = BLUE. Each level is written through its own real
// LockRect(level, ...)/UnlockRect(level) call, so the per-mip-level path (UMD D3DDDIARG_LOCK::
// SubResourceIndex -> wire subresource -> host extra_mips[level]) is exercised end to end.
//
// The discriminator is forcing the GPU to sample a SPECIFIC, non-zero level and reading back that
// level's own color -- not level 0's. Three sub-passes use D3DSAMP_MIPFILTER=D3DTEXF_NONE with
// D3DSAMP_MAXMIPLEVEL = 0/1/2 to pin the sampler to exactly one level each (host build_sampler maps
// MIPFILTER=NONE + MAXMIPLEVEL=k to min_lod==max_lod==k), so the readback must be RED/GREEN/BLUE
// respectively. If per-level upload were broken (only level 0 reaching the GPU) the GREEN and BLUE
// sub-passes would read garbage/level-0 red; if the sampler LOD were still pinned to 0 (the old code)
// all three would read RED. A fourth sub-pass drives GENUINE minification (a tiny on-screen quad
// sampling the full texture with D3DSAMP_MIPFILTER=D3DTEXF_POINT and the full LOD range) so the GPU's
// own screen-space-derivative LOD picks the smallest level -- BLUE -- with no MAXMIPLEVEL clamp.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // Same VS/PS convention as d3d9_texture_test.cpp / d3d9_multitexture_test.cpp: UV is carried in the
    // COLOR0 channel (D3DFVF_DIFFUSE) to sidestep the TEXCOORD0-interpolation quirk that test documents.
    // The PS just samples s0 -- each mip level is a solid color, so the exact UV is irrelevant.
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

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;
    constexpr int kTexSize = 64;   // level 0 dimensions
    constexpr int kMipLevels = 3;  // 64x64, 32x32, 16x16

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

    void release_all(IDirect3DTexture9* tex, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb,
                     IDirect3DIndexBuffer9* ib, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps,
                     IDirect3DDevice9* dev, IDirect3D9* d3d)
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

    // Write one solid color into an already-locked, tightly-packed 32bpp A8R8G8B8 mip level. Pitch is
    // not populated by this UMD's Lock DDI (same known gap d3d9_texture_test.cpp documents), so the
    // stride is computed from the level's own width.
    void fill_level(void* bits, const int width, const int height, const DWORD argb)
    {
        const LONG stride = width * 4;
        auto* base = static_cast<unsigned char*>(bits);
        for (int y = 0; y < height; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * stride);
            for (int x = 0; x < width; ++x)
            {
                row[x] = argb;
            }
        }
    }

    // Refill the shared 4-vertex quad buffer to a new screen rect (D3DLOCK_DISCARD -- default-pool
    // dynamic buffer), so each sub-pass can pick its own quad size.
    void set_quad(IDirect3DVertexBuffer9* vb, const int left, const int top, const int right, const int bottom)
    {
        Vertex* v = nullptr;
        if (SUCCEEDED(vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), D3DLOCK_DISCARD)) && v)
        {
            fill_quad(v, left, top, right, bottom, 0.5f);
            vb->Unlock();
        }
    }

    // One sub-pass: bind sampler LOD state, clear, draw the current quad, read back its center pixel,
    // and check it matches the expected per-level color. Returns the number of failed checks (0/1).
    int run_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const int center_x, const int center_y,
                 const DWORD mip_filter, const DWORD max_mip_level, const int exp_b, const int exp_g, const int exp_r,
                 const char* label)
    {
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, mip_filter);
        dev->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, max_mip_level);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();
        printf("[d3d9-miptexture-test] %s DrawIndexedPrimitive hr=0x%08lx\n", label, static_cast<unsigned long>(hd));

        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-miptexture-test] FAIL: %s rt LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hlr));
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const unsigned char* p = base + center_y * kStride + center_x * 4;
        printf("[d3d9-miptexture-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X (expected B=%02X G=%02X R=%02X)\n", label,
               center_x, center_y, p[0], p[1], p[2], p[3], exp_b, exp_g, exp_r);
        int failed = 0;
        if (!channel_close(p[0], exp_b, 4) || !channel_close(p[1], exp_g, 4) || !channel_close(p[2], exp_r, 4))
        {
            printf("[d3d9-miptexture-test] FAIL: %s did not sample the expected mip level's color\n", label);
            failed = 1;
        }
        else
        {
            printf("[d3d9-miptexture-test] PASS: %s sampled the expected mip level\n", label);
        }
        rt->UnlockRect();
        return failed;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-miptexture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9miptexturetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "miptexture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-miptexture-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-miptexture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-miptexture-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-miptexture-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-miptexture-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-miptexture-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-miptexture-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-miptexture-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // 64x64 texture with 3 real mip levels. Each level gets a distinct solid color written through its
    // own LockRect(level) call -- this is the per-subresource path under test.
    IDirect3DTexture9* tex = nullptr;
    HRESULT hct =
        dev->CreateTexture(kTexSize, kTexSize, kMipLevels, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    printf("[d3d9-miptexture-test] CreateTexture(%dx%d, %d levels) hr=0x%08lx tex=%p\n", kTexSize, kTexSize, kMipLevels,
           static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        printf("[d3d9-miptexture-test] FAIL: CreateTexture failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    const DWORD kLevelColors[kMipLevels] = {
        D3DCOLOR_ARGB(255, 255, 0, 0), // level 0: RED
        D3DCOLOR_ARGB(255, 0, 255, 0), // level 1: GREEN
        D3DCOLOR_ARGB(255, 0, 0, 255), // level 2: BLUE
    };
    for (int level = 0; level < kMipLevels; ++level)
    {
        const int level_size = kTexSize >> level;
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(level, &lr, nullptr, 0);
        printf("[d3d9-miptexture-test] LockRect(level=%d, %dx%d) hr=0x%08lx pBits=%p\n", level, level_size, level_size,
               static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-miptexture-test] FAIL: LockRect(level=%d) failed\n", level);
            release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
            return 1;
        }
        fill_level(lr.pBits, level_size, level_size, kLevelColors[level]);
        tex->UnlockRect(level);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-miptexture-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-miptexture-test] FAIL: render target creation failed\n");
        release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), D3DUSAGE_DYNAMIC, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    if (ib)
    {
        WORD* idx = nullptr;
        if (SUCCEEDED(ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0)) && idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
    }
    if (!vb || !ib)
    {
        printf("[d3d9-miptexture-test] FAIL: vertex/index buffer creation failed\n");
        release_all(tex, rt, vb, ib, vs, ps, dev, d3d);
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
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;

    // A large centered quad -- geometry-independent for the MAXMIPLEVEL-forced passes below.
    constexpr int kBigL = 170, kBigT = 120, kBigR = 470, kBigB = 360;
    const int big_cx = (kBigL + kBigR) / 2;
    const int big_cy = (kBigT + kBigB) / 2;
    set_quad(vb, kBigL, kBigT, kBigR, kBigB);

    // Pinned-level passes: MIPFILTER=NONE + MAXMIPLEVEL=k forces exactly level k. Reading back level
    // 1's GREEN and level 2's BLUE (not level 0's RED) is the decisive proof that each level's own
    // pixel data reached the GPU and is what the sampler returns.
    failures += run_pass(dev, rt, big_cx, big_cy, D3DTEXF_NONE, 0, /*B*/ 0, /*G*/ 0, /*R*/ 255, "level0-forced(RED)");
    failures += run_pass(dev, rt, big_cx, big_cy, D3DTEXF_NONE, 1, /*B*/ 0, /*G*/ 255, /*R*/ 0, "level1-forced(GREEN)");
    failures += run_pass(dev, rt, big_cx, big_cy, D3DTEXF_NONE, 2, /*B*/ 255, /*G*/ 0, /*R*/ 0, "level2-forced(BLUE)");

    // Genuine minification: a 16x16-screen quad samples the whole 64x64 texture across UV 0..1, so the
    // GPU's own LOD (log2(64/16) = 2) selects the smallest level with no MAXMIPLEVEL clamp -- must be
    // BLUE. This exercises real screen-space-derivative mip selection, not just the LOD clamp.
    constexpr int kMinL = 40, kMinT = 40, kMinR = 56, kMinB = 56;
    set_quad(vb, kMinL, kMinT, kMinR, kMinB);
    failures += run_pass(dev, rt, (kMinL + kMinR) / 2, (kMinT + kMinB) / 2, D3DTEXF_POINT, 0, /*B*/ 255, /*G*/ 0, /*R*/ 0,
                         "minified(BLUE)");

    release_all(tex, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-miptexture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
