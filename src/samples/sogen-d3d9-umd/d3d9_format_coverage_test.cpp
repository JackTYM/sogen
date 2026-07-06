// D3DFORMAT-coverage regression test for the g_formats FORMATOP expansion (sogen_d3d9_umd.cpp).
//
// Before that change, real d3d9.dll's CheckDeviceFormat/CreateTexture/CreateRenderTarget rejected
// every format the UMD did not advertise -- so each sub-pass below is a genuine BEFORE/AFTER
// discriminator: it would have failed at creation time (D3DERR_NOTAVAILABLE / D3DERR_INVALIDCALL)
// with the pre-expansion table. Each sub-pass drives the format all the way through a real render and
// checks a read-back pixel, not just the creation HRESULT.
//
// All three source textures are tiny 4x4 solid-color textures (D3DPOOL_MANAGED, the same real upload
// path d3d9_managed_texture_test.cpp proves) sampled with POINT/CLAMP across a full-screen quad, so
// every pixel of the render target decodes to that one solid color. The shared VS/PS pair, the UV-via-
// D3DFVF_DIFFUSE-color trick, and the off-screen-RT-plus-LockRect readback all mirror
// d3d9_texture_test.cpp's proven path (see its header for why UV rides the color channel).
//
// 1. DXT5 texture (FMT_OP_TEXTURE): a 4x4 BC3 block encoding solid RED, sampled -> the GPU's BC3
//    decode must reproduce RED. Proves compressed-texture advertisement + host BC3 upload/decode.
// 2. A8R8G8B8 render target (RT_TEX upgrade): render a solid CYAN 4x4 A8R8G8B8 source texture INTO an
//    A8R8G8B8 render target (previously un-creatable -- CreateRenderTarget returned D3DERR), then read
//    it back. A8R8G8B8 is host-side B8G8R8A8_UNORM (4 bytes/texel), so the readback path needs no
//    change; only the FORMATOP advertisement gated it.
// 3. L8 texture (FMT_OP_TEXTURE): a 4x4 single-channel luminance texture (value 200). Host maps L8 ->
//    VK_FORMAT_R8_UNORM sampled with IDENTITY swizzle, so the value lands in R and G/B read 0 -- a
//    single-channel format sampled and confirmed.
// 4. R5G6B5 negative discriminator (FMT_OP_TEXTURE only): CreateTexture must SUCCEED but
//    CreateRenderTarget must FAIL. R5G6B5 is 2 bytes/texel host-side while every RT readback/Present/
//    ColorFill path hardcodes 4 bytes/texel BGRA8, so R5G6B5 is advertised texture-only; before that fix
//    the row carried RT_TEX and the RT creation would have silently succeeded with corrupt output.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>

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
sampler2D s0 : register(s0);
float4 tint : register(c0);
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return tex2D(s0, input.color.rg) * tint;
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

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    void release_all(IDirect3DTexture9* t0, IDirect3DTexture9* t1, IDirect3DTexture9* t2, IDirect3DSurface9* rtX8,
                     IDirect3DSurface9* rtA8, IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (t0)
        {
            t0->Release();
        }
        if (t1)
        {
            t1->Release();
        }
        if (t2)
        {
            t2->Release();
        }
        if (rtX8)
        {
            rtX8->Release();
        }
        if (rtA8)
        {
            rtA8->Release();
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

    // A 4x4 solid-color texture in `format` (MANAGED). `block` points at the format's tightly-packed
    // 4x4 payload (16 bytes DXT5/L8-... callers pass the right size). Returns null on any failure.
    IDirect3DTexture9* make_solid_4x4(IDirect3DDevice9* dev, D3DFORMAT format, const void* block, size_t block_size,
                                      const char* label)
    {
        IDirect3DTexture9* tex = nullptr;
        HRESULT hc = dev->CreateTexture(4, 4, 1, 0, format, D3DPOOL_MANAGED, &tex, nullptr);
        printf("[d3d9-format-coverage-test] CreateTexture(%s) hr=0x%08lx tex=%p\n", label, static_cast<unsigned long>(hc),
               static_cast<void*>(tex));
        if (FAILED(hc) || !tex)
        {
            return nullptr;
        }
        D3DLOCKED_RECT lr{};
        HRESULT hl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-format-coverage-test] LockRect(%s) hr=0x%08lx pBits=%p\n", label, static_cast<unsigned long>(hl),
               lr.pBits);
        if (FAILED(hl) || !lr.pBits)
        {
            tex->Release();
            return nullptr;
        }
        // 4x4 backing is tightly packed (one BC block, or 4x4 R8, or 4x4 BGRA) -- write the payload
        // contiguously (this UMD does not reliably populate D3DLOCKED_RECT::Pitch, same gap the other
        // tests document).
        std::memcpy(lr.pBits, block, block_size);
        tex->UnlockRect(0);
        return tex;
    }

    // Full-screen quad, UV corners (0,0)-(1,1) packed into D3DFVF_DIFFUSE (R=u*255, G=v*255) exactly as
    // d3d9_texture_test.cpp does. For a solid texture the UV is immaterial, but reuse the proven path.
    void fill_fullscreen_quad(Vertex* v)
    {
        const auto uv = [](float u, float w) {
            return D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(w * 255.0f + 0.5f), 0);
        };
        v[0] = {-1.0f, -1.0f, 0.5f, uv(0.0f, 0.0f)};
        v[1] = {1.0f, -1.0f, 0.5f, uv(1.0f, 0.0f)};
        v[2] = {1.0f, 1.0f, 0.5f, uv(1.0f, 1.0f)};
        v[3] = {-1.0f, 1.0f, 0.5f, uv(0.0f, 1.0f)};
    }

    // Bind `tex`, target `rt`, clear to a neutral background, draw the full-screen quad, read back the
    // center pixel. Returns the four BGRA bytes via out[]. Returns false on any draw/lock failure.
    bool render_and_read_center(IDirect3DDevice9* dev, IDirect3DTexture9* tex, IDirect3DSurface9* rt, unsigned char out[4],
                                const char* label)
    {
        dev->SetRenderTarget(0, rt);
        dev->SetTexture(0, tex);
        const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        dev->SetPixelShaderConstantF(0, tint, 1);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(30, 30, 30), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();
        printf("[d3d9-format-coverage-test] DrawIndexedPrimitive(%s) hr=0x%08lx\n", label, static_cast<unsigned long>(hd));

        D3DLOCKED_RECT lr{};
        HRESULT hl = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hl) || !lr.pBits)
        {
            printf("[d3d9-format-coverage-test] FAIL(%s): RT LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hl));
            return false;
        }
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        const unsigned char* p = base + (kCanvasHeight / 2) * kStride + (kCanvasWidth / 2) * 4;
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        out[3] = p[3];
        rt->UnlockRect();
        return true;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-format-coverage-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9formatcoveragetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "format-coverage-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-format-coverage-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-format-coverage-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, nullptr);
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, nullptr);
    printf("[d3d9-format-coverage-test] D3DCompile(vs) hr=0x%08lx D3DCompile(ps) hr=0x%08lx\n",
           static_cast<unsigned long>(hvsc), static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc) || !vs_blob || !ps_blob)
    {
        if (vs_blob)
        {
            vs_blob->Release();
        }
        if (ps_blob)
        {
            ps_blob->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    printf("[d3d9-format-coverage-test] CreateVertexShader hr=0x%08lx CreatePixelShader hr=0x%08lx\n",
           static_cast<unsigned long>(hcvs), static_cast<unsigned long>(hcps));
    if (FAILED(hcvs) || FAILED(hcps) || !vs || !ps)
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // --- Source textures ---------------------------------------------------------------------------
    // DXT5 (BC3) solid RED: [alpha block: a0=a1=0xFF, indices 0 -> alpha 255 everywhere]
    // [color block: color0=RED(565 0xF800), color1=0, indices 0 -> color0 everywhere]. 0xF800 decodes
    // to 8-bit (255,0,0).
    const unsigned char kDxt5Red[16] = {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char kL8[16];
    std::memset(kL8, 0xC8, sizeof(kL8)); // luminance 200
    // A8R8G8B8 solid CYAN (0,255,255), stored little-endian BGRA per texel.
    unsigned int kCyan[16];
    for (int i = 0; i < 16; ++i)
    {
        kCyan[i] = D3DCOLOR_ARGB(255, 0, 255, 255);
    }

    IDirect3DTexture9* texDxt5 = make_solid_4x4(dev, D3DFMT_DXT5, kDxt5Red, sizeof(kDxt5Red), "DXT5");
    IDirect3DTexture9* texRgba = make_solid_4x4(dev, D3DFMT_A8R8G8B8, kCyan, sizeof(kCyan), "A8R8G8B8-src");
    IDirect3DTexture9* texL8 = make_solid_4x4(dev, D3DFMT_L8, kL8, sizeof(kL8), "L8");

    // --- Render targets ----------------------------------------------------------------------------
    IDirect3DSurface9* rtX8 = nullptr;
    HRESULT hrtx = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE,
                                           &rtX8, nullptr);
    IDirect3DSurface9* rtA8 = nullptr;
    HRESULT hrta = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE,
                                           &rtA8, nullptr);
    printf("[d3d9-format-coverage-test] CreateRenderTarget(X8R8G8B8) hr=0x%08lx surf=%p\n",
           static_cast<unsigned long>(hrtx), static_cast<void*>(rtX8));
    printf("[d3d9-format-coverage-test] CreateRenderTarget(A8R8G8B8) hr=0x%08lx surf=%p\n",
           static_cast<unsigned long>(hrta), static_cast<void*>(rtA8));

    // --- Geometry ----------------------------------------------------------------------------------
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            fill_fullscreen_quad(v);
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

    if (!texDxt5 || !texRgba || !texL8 || FAILED(hrtx) || !rtX8 || FAILED(hrta) || !rtA8 || !vb || !ib)
    {
        printf("[d3d9-format-coverage-test] FAILED\n");
        release_all(texDxt5, texRgba, texL8, rtX8, rtA8, vb, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;
    unsigned char px[4];

    // Sub-pass 1: DXT5 solid RED -> X8R8G8B8 RT.
    if (render_and_read_center(dev, texDxt5, rtX8, px, "DXT5"))
    {
        printf("[d3d9-format-coverage-test] DXT5 center B=%02X G=%02X R=%02X A=%02X (expected RED B=00 G=00 R=FF)\n",
               px[0], px[1], px[2], px[3]);
        if (channel_close(px[0], 0, 4) && channel_close(px[1], 0, 4) && channel_close(px[2], 255, 4))
        {
            printf("[d3d9-format-coverage-test] PASS: DXT5 block decoded to the expected RED\n");
        }
        else
        {
            printf("[d3d9-format-coverage-test] FAIL: DXT5 decoded color is wrong\n");
            ++failures;
        }
    }
    else
    {
        ++failures;
    }

    // Sub-pass 2: A8R8G8B8 render target (source = CYAN A8R8G8B8 texture).
    if (render_and_read_center(dev, texRgba, rtA8, px, "A8R8G8B8-RT"))
    {
        printf("[d3d9-format-coverage-test] A8R8G8B8-RT center B=%02X G=%02X R=%02X A=%02X (expected CYAN B=FF G=FF "
               "R=00)\n",
               px[0], px[1], px[2], px[3]);
        if (channel_close(px[0], 255, 4) && channel_close(px[1], 255, 4) && channel_close(px[2], 0, 4))
        {
            printf("[d3d9-format-coverage-test] PASS: A8R8G8B8 render target rendered and read back CYAN\n");
        }
        else
        {
            printf("[d3d9-format-coverage-test] FAIL: A8R8G8B8 render target color is wrong\n");
            ++failures;
        }
    }
    else
    {
        ++failures;
    }

    // Sub-pass 3: L8 luminance 200 -> X8R8G8B8 RT. Host maps L8 -> R8_UNORM (identity swizzle), so the
    // value lands in R; G/B read 0.
    if (render_and_read_center(dev, texL8, rtX8, px, "L8"))
    {
        printf("[d3d9-format-coverage-test] L8 center B=%02X G=%02X R=%02X A=%02X (expected R=C8 G=00 B=00)\n", px[0],
               px[1], px[2], px[3]);
        if (channel_close(px[2], 200, 4) && channel_close(px[1], 0, 4) && channel_close(px[0], 0, 4))
        {
            printf("[d3d9-format-coverage-test] PASS: L8 single-channel value sampled into R\n");
        }
        else
        {
            printf("[d3d9-format-coverage-test] FAIL: L8 sampled value is wrong\n");
            ++failures;
        }
    }
    else
    {
        ++failures;
    }

    // Sub-pass 4 (negative discriminator): R5G6B5 is advertised TEXTURE-ONLY -- CreateTexture must
    // succeed, but CreateRenderTarget must FAIL. R5G6B5 is 2 bytes/texel host-side, and every RT
    // readback/Present/ColorFill path hardcodes 4 bytes/texel BGRA8, so a renderable R5G6B5 RT would read
    // back corrupted; the fix scoped its FORMATOP row to FMT_OP_TEXTURE only. Before that fix the row
    // carried RT_TEX and this CreateRenderTarget would have SUCCEEDED (silently producing garbage), so a
    // FAILED HRESULT here is the exact before/after proof the RT capability was withdrawn.
    IDirect3DTexture9* texR5 = nullptr;
    HRESULT hr5tex = dev->CreateTexture(4, 4, 1, 0, D3DFMT_R5G6B5, D3DPOOL_MANAGED, &texR5, nullptr);
    IDirect3DSurface9* rtR5 = nullptr;
    HRESULT hr5rt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_R5G6B5, D3DMULTISAMPLE_NONE, 0, TRUE,
                                            &rtR5, nullptr);
    printf("[d3d9-format-coverage-test] R5G6B5 CreateTexture hr=0x%08lx tex=%p / CreateRenderTarget hr=0x%08lx surf=%p\n",
           static_cast<unsigned long>(hr5tex), static_cast<void*>(texR5), static_cast<unsigned long>(hr5rt),
           static_cast<void*>(rtR5));
    if (SUCCEEDED(hr5tex) && texR5 && FAILED(hr5rt) && !rtR5)
    {
        printf("[d3d9-format-coverage-test] PASS: R5G6B5 is texture-creatable but render-target creation is refused\n");
    }
    else
    {
        printf("[d3d9-format-coverage-test] FAIL: R5G6B5 texture/render-target advertisement is wrong (RT must fail)\n");
        ++failures;
    }
    if (texR5)
    {
        texR5->Release();
    }
    if (rtR5)
    {
        rtR5->Release();
    }

    release_all(texDxt5, texRgba, texL8, rtX8, rtA8, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-format-coverage-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
