// D3D9-over-Vulkan volume-texture SAMPLING test: proves a real IDirect3DVolumeTexture9 gets all of its
// depth slices' distinct pixel data to the GPU (one LockBox(0) writing every slice -> UMD
// D3DDDIARG_LOCK subresource 0 spanning the whole depth extent -> host single VK_IMAGE_TYPE_3D image with
// a VK_IMAGE_VIEW_TYPE_3D view) and that tex3D() selects the correct depth slice for a given w.
//
// A 32x32x4 single-mip volume is filled with a DIFFERENT solid color per depth slice: slice 0 = RED,
// slice 1 = GREEN, slice 2 = BLUE, slice 3 = YELLOW. All four slices are written in ONE LockBox(0) call:
// the host backs subresource 0 as one tightly-packed slice-major block (slice d at byte offset
// d*width*height*4), so each slice is written at pBits + d*(width*height*4) using a self-computed
// RowPitch/SlicePitch (this UMD's Lock DDI does not populate the D3DLOCKED_BOX pitches -- same known gap
// d3d9_miptexture_test.cpp documents for D3DLOCKED_RECT).
//
// The discriminator is four sub-passes, each sampling tex3D at the texture center (u,v = 0.5,0.5) with w
// chosen to land inside one specific slice -- w = (d + 0.5) / 4 -- via a float3 pixel-shader CONSTANT
// (SetPixelShaderConstantF, register c0), asserting the read-back center pixel equals THAT slice's own
// color. If the depth extent were collapsed to 1, or the sampler always read slice 0, every sub-pass
// would read back the SAME (slice-0) color instead of four distinct, correct ones -- so each sub-pass is
// checked against its own expected color.

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

    const char* const k_pixel_shader_hlsl = R"(
sampler3D s0 : register(s0);
float3 g_coord : register(c0);
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return float4(tex3D(s0, g_coord).rgb, 1.0);
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
    constexpr int kVolWidth = 32;
    constexpr int kVolHeight = 32;
    constexpr int kVolDepth = 4;

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    void release_all(IDirect3DVolumeTexture9* vol, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb,
                     IDirect3DIndexBuffer9* ib, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps,
                     IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (vol)
        {
            vol->Release();
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

    // One sub-pass: point tex3D at a slice-center coord (PS constant c0), clear, draw the full-screen
    // quad, read back the center pixel, and check it matches that slice's expected color. Returns the
    // number of failed checks (0/1).
    int run_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const float w, const int exp_b, const int exp_g,
                 const int exp_r, const char* label)
    {
        const float coord[4] = {0.5f, 0.5f, w, 0.0f};
        dev->SetPixelShaderConstantF(0, coord, 1);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();
        printf("[d3d9-volume-test] %s DrawIndexedPrimitive hr=0x%08lx\n", label, static_cast<unsigned long>(hd));

        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-volume-test] FAIL: %s rt LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hlr));
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        const int cx = kCanvasWidth / 2;
        const int cy = kCanvasHeight / 2;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const unsigned char* p = base + cy * kStride + cx * 4;
        printf("[d3d9-volume-test] %s (w=%.3f) pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X (expected B=%02X G=%02X R=%02X)\n",
               label, w, cx, cy, p[0], p[1], p[2], p[3], exp_b, exp_g, exp_r);
        int failed = 0;
        if (!channel_close(p[0], exp_b, 4) || !channel_close(p[1], exp_g, 4) || !channel_close(p[2], exp_r, 4))
        {
            printf("[d3d9-volume-test] FAIL: %s did not sample the expected depth slice\n", label);
            failed = 1;
        }
        else
        {
            printf("[d3d9-volume-test] PASS: %s sampled the expected depth slice\n", label);
        }
        rt->UnlockRect();
        return failed;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-volume-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9volumetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "volume-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-volume-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-volume-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-volume-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-volume-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-volume-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-volume-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-volume-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-volume-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // 32x32x4 single-mip volume. D3DUSAGE_DYNAMIC + D3DPOOL_DEFAULT makes it lockable (same proven path
    // d3d9_miptexture_test.cpp uses for 2D); the creation-only d3d9_cube_volume_test.cpp used Usage=0
    // because it never locked.
    IDirect3DVolumeTexture9* vol = nullptr;
    HRESULT hcv = dev->CreateVolumeTexture(kVolWidth, kVolHeight, kVolDepth, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                           D3DPOOL_DEFAULT, &vol, nullptr);
    printf("[d3d9-volume-test] CreateVolumeTexture(%dx%dx%d, 1 level) hr=0x%08lx vol=%p\n", kVolWidth, kVolHeight,
           kVolDepth, static_cast<unsigned long>(hcv), static_cast<void*>(vol));
    if (FAILED(hcv) || !vol)
    {
        printf("[d3d9-volume-test] FAIL: CreateVolumeTexture failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    const DWORD kSliceColors[kVolDepth] = {
        D3DCOLOR_ARGB(255, 255, 0, 0),   // slice 0: RED
        D3DCOLOR_ARGB(255, 0, 255, 0),   // slice 1: GREEN
        D3DCOLOR_ARGB(255, 0, 0, 255),   // slice 2: BLUE
        D3DCOLOR_ARGB(255, 255, 255, 0), // slice 3: YELLOW
    };
    {
        D3DLOCKED_BOX lb{};
        HRESULT htl = vol->LockBox(0, &lb, nullptr, 0);
        printf("[d3d9-volume-test] LockBox(0) hr=0x%08lx pBits=%p RowPitch=%ld SlicePitch=%ld\n",
               static_cast<unsigned long>(htl), lb.pBits, lb.RowPitch, lb.SlicePitch);
        if (FAILED(htl) || !lb.pBits)
        {
            printf("[d3d9-volume-test] FAIL: LockBox failed\n");
            release_all(vol, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
            return 1;
        }
        // The host backs subresource 0 as one tightly-packed slice-major block, so use self-computed
        // pitches (not lb.RowPitch/lb.SlicePitch, which this UMD's Lock DDI leaves unpopulated).
        const LONG row_pitch = kVolWidth * 4;
        const LONG slice_pitch = row_pitch * kVolHeight;
        auto* base = static_cast<unsigned char*>(lb.pBits);
        for (int d = 0; d < kVolDepth; ++d)
        {
            auto* slice = base + d * slice_pitch;
            for (int y = 0; y < kVolHeight; ++y)
            {
                auto* row = reinterpret_cast<DWORD*>(slice + y * row_pitch);
                for (int x = 0; x < kVolWidth; ++x)
                {
                    row[x] = kSliceColors[d];
                }
            }
        }
        vol->UnlockBox(0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-volume-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-volume-test] FAIL: render target creation failed\n");
        release_all(vol, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // Full-screen quad in NDC -- fixed geometry, only the c0 coordinate changes per sub-pass.
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), D3DUSAGE_DYNAMIC, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        if (SUCCEEDED(vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), D3DLOCK_DISCARD)) && v)
        {
            v[0] = {-1.0f, -1.0f, 0.5f, 0xFFFFFFFF};
            v[1] = {1.0f, -1.0f, 0.5f, 0xFFFFFFFF};
            v[2] = {1.0f, 1.0f, 0.5f, 0xFFFFFFFF};
            v[3] = {-1.0f, 1.0f, 0.5f, 0xFFFFFFFF};
            vb->Unlock();
        }
    }

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
        printf("[d3d9-volume-test] FAIL: vertex/index buffer creation failed\n");
        release_all(vol, rt, vb, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, vol);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;
    // Four sub-passes, one per depth slice (w = (d + 0.5) / 4), each asserting its OWN slice color.
    failures += run_pass(dev, rt, (0 + 0.5f) / kVolDepth, /*B*/ 0, /*G*/ 0, /*R*/ 255, "slice0(RED)");
    failures += run_pass(dev, rt, (1 + 0.5f) / kVolDepth, /*B*/ 0, /*G*/ 255, /*R*/ 0, "slice1(GREEN)");
    failures += run_pass(dev, rt, (2 + 0.5f) / kVolDepth, /*B*/ 255, /*G*/ 0, /*R*/ 0, "slice2(BLUE)");
    failures += run_pass(dev, rt, (3 + 0.5f) / kVolDepth, /*B*/ 0, /*G*/ 255, /*R*/ 255, "slice3(YELLOW)");

    release_all(vol, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-volume-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
