// D3DUSAGE_AUTOGENMIPMAP + IDirect3DBaseTexture9::GenerateMipSubLevels: D3D9 makes the DRIVER own the
// mip chain below the top level. The runtime exposes only level 0 to the app (GetLevelCount() reports 1),
// creates the driver resource with MipLevels=1 plus D3DDDI_RESOURCEFLAGS.AutogenMipmap, and issues
// pfnGenerateMipSubLevels (device-func-table slot 64) whenever the top level changes -- so the driver has
// to both allocate the sublevels itself and refill them by downsampling.
//
// The discriminator is the texel content, not just "is something there": level 0 is a one-texel RED/GREEN
// checkerboard, whose box-filtered average is (R=128, G=128, B=0). That separates all three interesting
// outcomes -- a correctly generated chain reads mid yellow, an allocated-but-never-filled chain reads
// BLACK (the zero-initialised backing), and a driver that ignored the autogen flag entirely allocates one
// level and reads back the pure RED or GREEN of the checkerboard itself.
//
// Level 0 is filled through a D3DPOOL_SYSTEMMEM staging texture and UpdateTexture rather than a direct
// lock, because a D3DPOOL_DEFAULT texture is not lockable in D3D9 -- the same staging pattern
// d3d9_updatetexture_test.cpp covers.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // UV rides in COLOR0 (D3DFVF_DIFFUSE), the same convention d3d9_miptexture_test.cpp uses to sidestep
    // this suite's documented TEXCOORD0-interpolation quirk.
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
    constexpr int kTexSize = 64; // 64x64 -> a 7-level chain (64,32,16,8,4,2,1)
    constexpr int kQuadLeft = 64;
    constexpr int kQuadTop = 48;
    constexpr int kQuadRight = 576;
    constexpr int kQuadBottom = 432;

    float to_ndc_x(const int screen_x)
    {
        return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f;
    }

    float to_ndc_y(const int screen_y)
    {
        return 1.0f - static_cast<float>(screen_y) / (kCanvasHeight / 2);
    }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    // Refills the quad buffer, mapping the fixed screen rect onto an arbitrary UV rect so a sub-pass can
    // aim at one specific texel (the level-0 control) or at the whole texture (the mip passes).
    void set_quad(IDirect3DVertexBuffer9* vb, const float u0, const float v0, const float u1, const float v1)
    {
        const auto uv_color = [](const float u, const float v) {
            return D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(v * 255.0f + 0.5f), 0);
        };
        Vertex* v = nullptr;
        if (SUCCEEDED(vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), D3DLOCK_DISCARD)) && v)
        {
            v[0] = {to_ndc_x(kQuadLeft), to_ndc_y(kQuadTop), 0.5f, uv_color(u0, v0)};
            v[1] = {to_ndc_x(kQuadRight), to_ndc_y(kQuadTop), 0.5f, uv_color(u1, v0)};
            v[2] = {to_ndc_x(kQuadRight), to_ndc_y(kQuadBottom), 0.5f, uv_color(u1, v1)};
            v[3] = {to_ndc_x(kQuadLeft), to_ndc_y(kQuadBottom), 0.5f, uv_color(u0, v1)};
            vb->Unlock();
        }
    }

    int run_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const DWORD max_mip_level, const int exp_b, const int exp_g, const int exp_r,
                 const char* label)
    {
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        dev->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, max_mip_level);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();

        D3DLOCKED_RECT lr{};
        const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-autogen-mipmap-test] FAIL: %s rt LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hlr));
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        const int cx = (kQuadLeft + kQuadRight) / 2;
        const int cy = (kQuadTop + kQuadBottom) / 2;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const unsigned char* p = base + cy * kStride + cx * 4;
        printf("[d3d9-autogen-mipmap-test] %s pixel=B=%02X G=%02X R=%02X (expected B=%02X G=%02X R=%02X)\n", label, p[0], p[1], p[2], exp_b,
               exp_g, exp_r);
        int failed = 0;
        if (!channel_close(p[0], exp_b, 4) || !channel_close(p[1], exp_g, 4) || !channel_close(p[2], exp_r, 4))
        {
            printf("[d3d9-autogen-mipmap-test] FAIL: %s\n", label);
            failed = 1;
        }
        else
        {
            printf("[d3d9-autogen-mipmap-test] PASS: %s\n", label);
        }
        rt->UnlockRect();
        return failed;
    }

    void release_all(IDirect3DTexture9* staging, IDirect3DTexture9* autogen, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb,
                     IDirect3DIndexBuffer9* ib, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev,
                     IDirect3D9* d3d)
    {
        if (staging)
        {
            staging->Release();
        }
        if (autogen)
        {
            autogen->Release();
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
    printf("[d3d9-autogen-mipmap-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9autogenmipmaptest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "autogen-mipmap-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-autogen-mipmap-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    // The driver has to advertise D3DFORMAT_OP_AUTOGENMIPMAP for the format, or the runtime answers
    // D3DOK_NOAUTOGEN (a SUCCESS code) here and silently strips the usage bit from the texture it creates.
    const HRESULT hcf = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP,
                                               D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
    printf("[d3d9-autogen-mipmap-test] CheckDeviceFormat(AUTOGENMIPMAP) hr=0x%08lx\n", static_cast<unsigned long>(hcf));

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
    printf("[d3d9-autogen-mipmap-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                              &vs_blob, nullptr);
    HRESULT hpsc =
        D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &ps_blob, nullptr);
    printf("[d3d9-autogen-mipmap-test] D3DCompile(vs) hr=0x%08lx (ps) hr=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        printf("[d3d9-autogen-mipmap-test] FAIL: shader creation hr=0x%08lx/0x%08lx\n", static_cast<unsigned long>(hcvs),
               static_cast<unsigned long>(hcps));
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Levels=0 asks the runtime for the full chain; with D3DUSAGE_AUTOGENMIPMAP that chain is the
    // driver's, and the app still only ever sees level 0.
    IDirect3DTexture9* autogen = nullptr;
    HRESULT hca = dev->CreateTexture(kTexSize, kTexSize, 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &autogen, nullptr);
    IDirect3DTexture9* staging = nullptr;
    HRESULT hcs = dev->CreateTexture(kTexSize, kTexSize, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &staging, nullptr);
    printf("[d3d9-autogen-mipmap-test] CreateTexture(AUTOGENMIPMAP) hr=0x%08lx / (SYSTEMMEM staging) hr=0x%08lx\n",
           static_cast<unsigned long>(hca), static_cast<unsigned long>(hcs));
    if (FAILED(hca) || FAILED(hcs) || !autogen || !staging)
    {
        printf("[d3d9-autogen-mipmap-test] FAIL: texture creation failed\n");
        release_all(staging, autogen, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    printf("[d3d9-autogen-mipmap-test] autogen GetLevelCount()=%lu (D3D9 exposes only level 0)\n",
           static_cast<unsigned long>(autogen->GetLevelCount()));

    {
        D3DLOCKED_RECT lr{};
        HRESULT htl = staging->LockRect(0, &lr, nullptr, 0);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-autogen-mipmap-test] FAIL: staging LockRect hr=0x%08lx\n", static_cast<unsigned long>(htl));
            release_all(staging, autogen, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
            return 1;
        }
        auto* bits = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < kTexSize; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(bits + static_cast<size_t>(y) * lr.Pitch);
            for (int x = 0; x < kTexSize; ++x)
            {
                row[x] = ((x + y) & 1) ? D3DCOLOR_ARGB(255, 0, 255, 0) : D3DCOLOR_ARGB(255, 255, 0, 0);
            }
        }
        staging->UnlockRect(0);
    }

    HRESULT hut = dev->UpdateTexture(staging, autogen);
    autogen->GenerateMipSubLevels();
    printf("[d3d9-autogen-mipmap-test] UpdateTexture hr=0x%08lx, GenerateMipSubLevels issued\n", static_cast<unsigned long>(hut));

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-autogen-mipmap-test] FAIL: render target creation hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
        release_all(staging, autogen, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), D3DUSAGE_DYNAMIC, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    if (!vb || !ib)
    {
        printf("[d3d9-autogen-mipmap-test] FAIL: vertex/index buffer creation failed\n");
        release_all(staging, autogen, rt, vb, ib, vs, ps, dev, d3d);
        return 1;
    }
    {
        constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
        WORD* idx = nullptr;
        if (SUCCEEDED(ib->Lock(0, sizeof(kQuadIndices), reinterpret_cast<void**>(&idx), 0)) && idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, autogen);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;
    if (FAILED(hut))
    {
        printf("[d3d9-autogen-mipmap-test] FAIL: UpdateTexture returned an error\n");
        ++failures;
    }

    // Control: level 0 must still be the raw checkerboard, aimed at exactly one texel (32,32), which is
    // RED. Guards against a "the whole texture became grey" confound making the mip passes pass for the
    // wrong reason.
    constexpr float kTexel = 1.0f / kTexSize;
    set_quad(vb, 32.0f * kTexel + 0.25f * kTexel, 32.0f * kTexel + 0.25f * kTexel, 32.0f * kTexel + 0.75f * kTexel,
             32.0f * kTexel + 0.75f * kTexel);
    failures += run_pass(dev, rt, 0, /*B*/ 0, /*G*/ 0, /*R*/ 255, "level0 is the raw RED/GREEN checkerboard");

    // Every generated level is the box-filtered average of the checkerboard: R=G=128, B=0.
    set_quad(vb, 0.0f, 0.0f, 1.0f, 1.0f);
    failures += run_pass(dev, rt, 1, /*B*/ 0, /*G*/ 128, /*R*/ 128, "level1 is the box-filtered average");
    failures += run_pass(dev, rt, 3, /*B*/ 0, /*G*/ 128, /*R*/ 128, "level3 is the box-filtered average");
    failures += run_pass(dev, rt, 6, /*B*/ 0, /*G*/ 128, /*R*/ 128, "level6 (1x1) is the box-filtered average");

    release_all(staging, autogen, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-autogen-mipmap-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
