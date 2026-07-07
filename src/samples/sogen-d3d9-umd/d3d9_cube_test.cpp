// D3D9-over-Vulkan cube-texture SAMPLING test: proves a real IDirect3DCubeTexture9 gets each of its six
// faces' distinct pixel data all the way to the GPU (per-face LockRect(D3DCUBEMAP_FACE_*, 0) -> UMD
// D3DDDIARG_LOCK::SubResourceIndex == face*mip_levels+level -> host per-subresource backing -> a single
// 6-array-layer VK_IMAGE_VIEW_TYPE_CUBE image) and that texCUBE() selects the correct face for a given
// sample direction.
//
// A 64x64 single-mip cube is filled with a DIFFERENT solid color per face, in the standard D3D9 face
// order +X/-X/+Y/-Y/+Z/-Z: RED/GREEN/BLUE/YELLOW/MAGENTA/CYAN. Each face is written through its own real
// LockRect(D3DCUBEMAP_FACE_POSITIVE_X + f, 0, ...)/UnlockRect(...) call.
//
// The discriminator is six sub-passes, each pointing texCUBE at one specific face's center direction via
// a float3 pixel-shader CONSTANT (SetPixelShaderConstantF, register c0) -- (1,0,0)/(-1,0,0)/(0,1,0)/
// (0,-1,0)/(0,0,1)/(0,0,-1) -- and asserting the read-back center pixel equals THAT face's own color. If
// the flattened subresource index or the Vulkan array-layer assignment were wrong (swapped, or every
// face reading layer 0), multiple sub-passes would read back the SAME wrong color instead of six
// distinct, correct ones -- so each sub-pass is checked against its own expected color, not merely
// "not the clear color".

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // Passthrough VS (same D3DFVF_XYZ|D3DFVF_DIFFUSE vertex shape the host's ensure_programmable_pipeline
    // recognizes); the per-vertex color is unused -- the sample direction comes from PS constant c0.
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
samplerCUBE s0 : register(s0);
float3 g_dir : register(c0);
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return float4(texCUBE(s0, g_dir).rgb, 1.0);
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
    constexpr int kCubeSize = 64;

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    void release_all(IDirect3DCubeTexture9* tex, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb,
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

    // Write one solid color into an already-locked, tightly-packed 32bpp A8R8G8B8 cube face. Pitch is not
    // populated by this UMD's Lock DDI (same known gap d3d9_miptexture_test.cpp documents), so the stride
    // is computed from the face's own width.
    void fill_face(void* bits, const int size, const DWORD argb)
    {
        const LONG stride = size * 4;
        auto* base = static_cast<unsigned char*>(bits);
        for (int y = 0; y < size; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * stride);
            for (int x = 0; x < size; ++x)
            {
                row[x] = argb;
            }
        }
    }

    // One sub-pass: point texCUBE at a face-center direction (PS constant c0), clear, draw the full-screen
    // quad, read back the center pixel, and check it matches that face's expected color. Returns the
    // number of failed checks (0/1).
    int run_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const float dir_x, const float dir_y, const float dir_z,
                 const int exp_b, const int exp_g, const int exp_r, const char* label)
    {
        const float dir[4] = {dir_x, dir_y, dir_z, 0.0f};
        dev->SetPixelShaderConstantF(0, dir, 1);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();
        printf("[d3d9-cube-test] %s DrawIndexedPrimitive hr=0x%08lx\n", label, static_cast<unsigned long>(hd));

        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-cube-test] FAIL: %s rt LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hlr));
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        const int cx = kCanvasWidth / 2;
        const int cy = kCanvasHeight / 2;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const unsigned char* p = base + cy * kStride + cx * 4;
        printf("[d3d9-cube-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X (expected B=%02X G=%02X R=%02X)\n", label, cx,
               cy, p[0], p[1], p[2], p[3], exp_b, exp_g, exp_r);
        int failed = 0;
        if (!channel_close(p[0], exp_b, 4) || !channel_close(p[1], exp_g, 4) || !channel_close(p[2], exp_r, 4))
        {
            printf("[d3d9-cube-test] FAIL: %s did not sample the expected cube face\n", label);
            failed = 1;
        }
        else
        {
            printf("[d3d9-cube-test] PASS: %s sampled the expected cube face\n", label);
        }
        rt->UnlockRect();
        return failed;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-cube-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9cubetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "cube-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-cube-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-cube-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-cube-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-cube-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-cube-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-cube-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-cube-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-cube-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // 64x64 single-mip cube. D3DUSAGE_DYNAMIC + D3DPOOL_DEFAULT makes each face lockable (the same
    // proven lockable-texture path d3d9_miptexture_test.cpp uses for 2D); the creation-only
    // d3d9_cube_volume_test.cpp used Usage=0 because it never locked.
    IDirect3DCubeTexture9* tex = nullptr;
    HRESULT hct = dev->CreateCubeTexture(kCubeSize, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    printf("[d3d9-cube-test] CreateCubeTexture(%d, 1 level) hr=0x%08lx tex=%p\n", kCubeSize,
           static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        printf("[d3d9-cube-test] FAIL: CreateCubeTexture failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Faces 0..5 in the standard D3D9 order +X/-X/+Y/-Y/+Z/-Z.
    const DWORD kFaceColors[6] = {
        D3DCOLOR_ARGB(255, 255, 0, 0),   // +X: RED
        D3DCOLOR_ARGB(255, 0, 255, 0),   // -X: GREEN
        D3DCOLOR_ARGB(255, 0, 0, 255),   // +Y: BLUE
        D3DCOLOR_ARGB(255, 255, 255, 0), // -Y: YELLOW
        D3DCOLOR_ARGB(255, 255, 0, 255), // +Z: MAGENTA
        D3DCOLOR_ARGB(255, 0, 255, 255), // -Z: CYAN
    };
    for (int f = 0; f < 6; ++f)
    {
        D3DLOCKED_RECT lr{};
        const D3DCUBEMAP_FACES face = static_cast<D3DCUBEMAP_FACES>(D3DCUBEMAP_FACE_POSITIVE_X + f);
        HRESULT htl = tex->LockRect(face, 0, &lr, nullptr, 0);
        printf("[d3d9-cube-test] LockRect(face=%d) hr=0x%08lx pBits=%p\n", f, static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-cube-test] FAIL: LockRect(face=%d) failed\n", f);
            release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
            return 1;
        }
        fill_face(lr.pBits, kCubeSize, kFaceColors[f]);
        tex->UnlockRect(face, 0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-cube-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-cube-test] FAIL: render target creation failed\n");
        release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // Full-screen quad in NDC -- fixed geometry, only the c0 direction changes per sub-pass.
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
        printf("[d3d9-cube-test] FAIL: vertex/index buffer creation failed\n");
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
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;
    // Six sub-passes, one per face-center direction, each asserting its OWN face color.
    failures += run_pass(dev, rt, 1.0f, 0.0f, 0.0f, /*B*/ 0, /*G*/ 0, /*R*/ 255, "+X(RED)");
    failures += run_pass(dev, rt, -1.0f, 0.0f, 0.0f, /*B*/ 0, /*G*/ 255, /*R*/ 0, "-X(GREEN)");
    failures += run_pass(dev, rt, 0.0f, 1.0f, 0.0f, /*B*/ 255, /*G*/ 0, /*R*/ 0, "+Y(BLUE)");
    failures += run_pass(dev, rt, 0.0f, -1.0f, 0.0f, /*B*/ 0, /*G*/ 255, /*R*/ 255, "-Y(YELLOW)");
    failures += run_pass(dev, rt, 0.0f, 0.0f, 1.0f, /*B*/ 255, /*G*/ 0, /*R*/ 255, "+Z(MAGENTA)");
    failures += run_pass(dev, rt, 0.0f, 0.0f, -1.0f, /*B*/ 255, /*G*/ 255, /*R*/ 0, "-Z(CYAN)");

    release_all(tex, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-cube-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
