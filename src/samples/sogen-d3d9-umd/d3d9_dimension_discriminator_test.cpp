// D3D9-over-Vulkan resource-dimension discriminator (proof for the D3DDDIARG_CREATERESOURCE RE fix).
//
// Before the fix, umd_CreateResource hardcoded every resource to 640x480; now it reads the real
// Width/Height from the RE'd struct. This test creates a deliberately NON-640x480, non-power-of-two-
// friendly texture (256x128) whose content is defined in ABSOLUTE TEXEL coordinates, then renders it
// across a full 640x480 render target and reads back real rendered pixels. The checks are analytic and
// resolution-sensitive: each asserts a rendered pixel value that is ONLY correct if the texture is
// genuinely 256x128 end-to-end, and that would necessarily be WRONG if the texture were silently still
// 640x480 (as the old hardcoded path produced -- a 640-wide upload stride shears the guest's 256-wide
// rows, and rows past 128 read unwritten memory).
//
// Texture layout (256x128, A8R8G8B8), everything BLACK except two 32x32 markers centered at x=128:
//   TOP marker  RED  : texels x in [112,144), y in [8,40)   -> center texel (128, 24)
//   BOTTOM marker BLUE: texels x in [112,144), y in [88,120) -> center texel (128,104)
// A full-screen quad maps UV (0,0)..(1,1) across the whole 640x480 RT, so RT pixel (X,Y) samples texel
// (X/640*256, Y/480*128). The four checks:
//   (320, 90) == RED   : texel (128, 24)  -- the TOP marker sits at u=0.5,v=0.19 <=> a 256x128 texture.
//   (128, 24) == BLACK : if the texture were 640x480, the RED marker would land here (u=0.2,v=0.05);
//                        it is black, so the texture is NOT 640x480.
//   (320,390) == BLUE  : texel (128,104) -- proves the real HEIGHT is 128 (row 104 exists and maps to
//                        v=0.81; at 480 tall it would map to v=0.22 -> screen row ~104, not 390).
//   (320,240) == BLACK : texel (128, 64)  -- background between the markers (not all-white/garbage).

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
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

    // UV is carried through the vertex COLOR0 channel (R=u, G=v), matching d3d9_texture_test.cpp's own
    // workaround for the TEXCOORD0-V interpolation gap documented there.
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
    constexpr int kTexWidth = 256;
    constexpr int kTexHeight = 128;

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
    printf("[d3d9-dim-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9dimtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "dim-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-dim-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-dim-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    HRESULT hvsc =
        D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0, &vs_blob, nullptr);
    HRESULT hpsc =
        D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &ps_blob, nullptr);
    printf("[d3d9-dim-test] D3DCompile vs=0x%08lx ps=0x%08lx\n", static_cast<unsigned long>(hvsc), static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
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
        printf("[d3d9-dim-test] FAIL: shader creation failed\n");
        return 1;
    }

    // The discriminating resource: a 256x128 texture, NOT the 640x480 the old UMD hardcoded.
    IDirect3DTexture9* tex = nullptr;
    HRESULT hct = dev->CreateTexture(kTexWidth, kTexHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    printf("[d3d9-dim-test] CreateTexture %dx%d hr=0x%08lx tex=%p\n", kTexWidth, kTexHeight, static_cast<unsigned long>(hct),
           static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        printf("[d3d9-dim-test] FAIL: CreateTexture failed\n");
        return 1;
    }
    {
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-dim-test] Texture LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-dim-test] FAIL: texture LockRect failed\n");
            return 1;
        }
        // Pitch is not populated by this UMD's Lock DDI (documented gap); backing is tightly packed 32bpp.
        constexpr LONG kStride = kTexWidth * 4;
        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < kTexHeight; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * kStride);
            for (int x = 0; x < kTexWidth; ++x)
            {
                DWORD color = D3DCOLOR_ARGB(255, 0, 0, 0); // background BLACK
                if (x >= 112 && x < 144 && y >= 8 && y < 40)
                {
                    color = D3DCOLOR_ARGB(255, 255, 0, 0); // TOP marker RED
                }
                else if (x >= 112 && x < 144 && y >= 88 && y < 120)
                {
                    color = D3DCOLOR_ARGB(255, 0, 0, 255); // BOTTOM marker BLUE
                }
                row[x] = color;
            }
        }
        tex->UnlockRect(0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-dim-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-dim-test] FAIL: render target creation failed\n");
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // One full-screen quad: NDC corners span the whole RT, UV (0,0) top-left .. (1,1) bottom-right.
    const auto uv_color = [](const float u, const float v) {
        return D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(v * 255.0f + 0.5f), 0);
    };
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            v[0] = {to_ndc_x(0), to_ndc_y(0), 0.5f, uv_color(0.0f, 0.0f)};
            v[1] = {to_ndc_x(kCanvasWidth), to_ndc_y(0), 0.5f, uv_color(1.0f, 0.0f)};
            v[2] = {to_ndc_x(kCanvasWidth), to_ndc_y(kCanvasHeight), 0.5f, uv_color(1.0f, 1.0f)};
            v[3] = {to_ndc_x(0), to_ndc_y(kCanvasHeight), 0.5f, uv_color(0.0f, 1.0f)};
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
        printf("[d3d9-dim-test] FAIL: vertex/index buffer creation failed\n");
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

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 128, 0), 1.0f, 0); // clear GREEN (distinct from markers/bg)
    HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-dim-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-dim-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        auto check = [&](const char* name, int col, int row, int eb, int eg, int er) {
            const unsigned char* p = pixel_at(col, row);
            printf("[d3d9-dim-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X (expected B=%02X G=%02X R=%02X)\n", name, col, row,
                   p[0], p[1], p[2], eb, eg, er);
            if (!channel_close(p[0], eb, 4) || !channel_close(p[1], eg, 4) || !channel_close(p[2], er, 4))
            {
                printf("[d3d9-dim-test] FAIL: %s mismatch\n", name);
                ++failures;
            }
            else
            {
                printf("[d3d9-dim-test] PASS: %s\n", name);
            }
        };

        // TOP marker RED sampled at u=0.5,v=0.19 -> texel (128,24): only true for a 256x128 texture.
        check("top-marker-256x128", 320, 90, 0, 0, 255);
        // Where the RED marker WOULD be if the texture were 640x480 (u=0.2,v=0.05): must be BLACK now.
        check("no-marker-at-640x480-pos", 128, 24, 0, 0, 0);
        // BOTTOM marker BLUE at texel (128,104): proves real height is 128 (v=0.81, not 0.22).
        check("bottom-marker-height-128", 320, 390, 255, 0, 0);
        // Background between markers, texel (128,64): not all-white / not sheared garbage.
        check("background-between-markers", 320, 240, 0, 0, 0);

        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-dim-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    rt->Release();
    tex->Release();
    vb->Release();
    ib->Release();
    vs->Release();
    ps->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-dim-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
