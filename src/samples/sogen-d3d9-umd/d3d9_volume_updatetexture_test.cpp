// IDirect3DDevice9::UpdateTexture on a VOLUME texture: a D3DPOOL_SYSTEMMEM volume is filled slice by
// slice through LockBox/UnlockBox, marked with AddDirtyBox, pushed into a D3DPOOL_DEFAULT volume with
// UpdateTexture, and the DEFAULT volume is then sampled with tex3D at each depth slice's center.
//
// This is the volume-texture arm of the same staging pattern d3d9_updatetexture_test.cpp covers for 2D,
// and it is exactly how Modern Warfare 2 populates its ambient light-grid volume (a 256x256x4
// D3DFMT_A8R8G8B8 SYSTEMMEM master and its DEFAULT-pool sampled copy, synced 233 times in one measured
// gameplay session). The runtime routes a volume's UpdateTexture to pfnVolBlt, NOT pfnTexBlt, so a
// driver that implements only the latter leaves the sampled copy permanently black -- which is what
// this UMD did, and what made MW2's world geometry render with no ambient term at all.
//
// The discriminator is per-slice: each depth slice gets its own color, so the destination being copied
// at a depth extent of 1 (only slice 0 arriving) is distinguishable from it never being written at all,
// and both are distinguishable from a correct copy.

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

    void release_all(IDirect3DVolumeTexture9* staging, IDirect3DVolumeTexture9* sampled, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb,
                     IDirect3DIndexBuffer9* ib, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev,
                     IDirect3D9* d3d)
    {
        if (staging)
        {
            staging->Release();
        }
        if (sampled)
        {
            sampled->Release();
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

    int run_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const float w, const int exp_b, const int exp_g, const int exp_r,
                 const char* label)
    {
        const float coord[4] = {0.5f, 0.5f, w, 0.0f};
        dev->SetPixelShaderConstantF(0, coord, 1);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();

        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-volume-updatetexture-test] FAIL: %s rt LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hlr));
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        const int cx = kCanvasWidth / 2;
        const int cy = kCanvasHeight / 2;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const unsigned char* p = base + cy * kStride + cx * 4;
        printf("[d3d9-volume-updatetexture-test] %s (w=%.3f) pixel=B=%02X G=%02X R=%02X (expected B=%02X G=%02X R=%02X)\n", label, w, p[0],
               p[1], p[2], exp_b, exp_g, exp_r);
        int failed = 0;
        if (!channel_close(p[0], exp_b, 4) || !channel_close(p[1], exp_g, 4) || !channel_close(p[2], exp_r, 4))
        {
            printf("[d3d9-volume-updatetexture-test] FAIL: %s did not receive the staging volume's slice\n", label);
            failed = 1;
        }
        else
        {
            printf("[d3d9-volume-updatetexture-test] PASS: %s\n", label);
        }
        rt->UnlockRect();
        return failed;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-volume-updatetexture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9volupdatetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "volume-updatetexture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-volume-updatetexture-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-volume-updatetexture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                              &vs_blob, &vs_errors);
    printf("[d3d9-volume-updatetexture-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-volume-updatetexture-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &ps_blob,
                              &ps_errors);
    printf("[d3d9-volume-updatetexture-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-volume-updatetexture-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    printf("[d3d9-volume-updatetexture-test] CreateVertexShader hr=0x%08lx CreatePixelShader hr=0x%08lx\n",
           static_cast<unsigned long>(hcvs), static_cast<unsigned long>(hcps));
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DVolumeTexture9* staging = nullptr;
    HRESULT hcs = dev->CreateVolumeTexture(kVolWidth, kVolHeight, kVolDepth, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &staging, nullptr);
    IDirect3DVolumeTexture9* sampled = nullptr;
    HRESULT hcd = dev->CreateVolumeTexture(kVolWidth, kVolHeight, kVolDepth, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &sampled, nullptr);
    printf("[d3d9-volume-updatetexture-test] CreateVolumeTexture(SYSTEMMEM) hr=0x%08lx / (DEFAULT) hr=0x%08lx\n",
           static_cast<unsigned long>(hcs), static_cast<unsigned long>(hcd));
    if (FAILED(hcs) || FAILED(hcd) || !staging || !sampled)
    {
        printf("[d3d9-volume-updatetexture-test] FAIL: volume texture creation failed\n");
        release_all(staging, sampled, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
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
        HRESULT htl = staging->LockBox(0, &lb, nullptr, 0);
        printf("[d3d9-volume-updatetexture-test] staging LockBox hr=0x%08lx pBits=%p RowPitch=%ld SlicePitch=%ld\n",
               static_cast<unsigned long>(htl), lb.pBits, lb.RowPitch, lb.SlicePitch);
        if (FAILED(htl) || !lb.pBits)
        {
            printf("[d3d9-volume-updatetexture-test] FAIL: staging LockBox failed\n");
            release_all(staging, sampled, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
            return 1;
        }
        // Tightly-packed slice-major layout, self-computed: this UMD reports RowPitch but leaves
        // D3DLOCKED_BOX::SlicePitch unpopulated (see d3d9_ddi.hpp's D3DDDIARG_LOCK), and the host backs
        // the subresource exactly this way.
        const LONG row_pitch = kVolWidth * 4;
        const LONG slice_pitch = row_pitch * kVolHeight;
        auto* base = static_cast<unsigned char*>(lb.pBits);
        for (int d = 0; d < kVolDepth; ++d)
        {
            for (int y = 0; y < kVolHeight; ++y)
            {
                auto* row = reinterpret_cast<DWORD*>(base + d * slice_pitch + y * row_pitch);
                for (int x = 0; x < kVolWidth; ++x)
                {
                    row[x] = kSliceColors[d];
                }
            }
        }
        staging->UnlockBox(0);
    }

    HRESULT hadb = staging->AddDirtyBox(nullptr);
    HRESULT hut = dev->UpdateTexture(staging, sampled);
    printf("[d3d9-volume-updatetexture-test] AddDirtyBox hr=0x%08lx UpdateTexture hr=0x%08lx\n", static_cast<unsigned long>(hadb),
           static_cast<unsigned long>(hut));

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-volume-updatetexture-test] FAIL: render target creation failed\n");
        release_all(staging, sampled, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

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
        printf("[d3d9-volume-updatetexture-test] FAIL: vertex/index buffer creation failed\n");
        release_all(staging, sampled, rt, vb, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, sampled);
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
    if (FAILED(hut))
    {
        printf("[d3d9-volume-updatetexture-test] FAIL: UpdateTexture returned an error\n");
        ++failures;
    }
    failures += run_pass(dev, rt, (0 + 0.5f) / kVolDepth, /*B*/ 0, /*G*/ 0, /*R*/ 255, "slice0(RED)");
    failures += run_pass(dev, rt, (1 + 0.5f) / kVolDepth, /*B*/ 0, /*G*/ 255, /*R*/ 0, "slice1(GREEN)");
    failures += run_pass(dev, rt, (2 + 0.5f) / kVolDepth, /*B*/ 255, /*G*/ 0, /*R*/ 0, "slice2(BLUE)");
    failures += run_pass(dev, rt, (3 + 0.5f) / kVolDepth, /*B*/ 0, /*G*/ 255, /*R*/ 255, "slice3(YELLOW)");

    release_all(staging, sampled, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-volume-updatetexture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
