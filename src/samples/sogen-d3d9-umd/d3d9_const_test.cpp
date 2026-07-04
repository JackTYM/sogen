// D3D9-over-Vulkan constant-register (c#) end-to-end test (see .claude/plans/jazzy-giggling-cloud.md).
//
// Pixel shader returns a solid color read from PS constant register c0 (set via
// SetPixelShaderConstantF), proving set-1/binding-0 UBO delivery. Vertex shader applies a 4x4
// scale matrix packed across VS constant registers c1-c4 (set via SetVertexShaderConstantF) to the
// vertex position, proving set-0/binding-0 UBO delivery. Draws to an off-screen render target
// (mirroring d3d9_triangle_test.cpp's CreateRenderTarget/LockRect pattern) and analytically verifies
// two pixels: one that must show the exact c0 color (inside the scaled-down triangle), and one that
// must show the background clear color instead of the triangle color (inside the original,
// unscaled triangle's bounds but outside the scaled-down triangle -- only true if the VS-side
// constant matrix actually applied).

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
float4x4 worldViewProj : register(c1);
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = mul(float4(input.pos, 1.0), worldViewProj);
    output.color = input.color;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 pixelConstant : register(c0);
float4 main(PSInput input) : COLOR0
{
    return pixelConstant;
}
)";

    // Distinctive, non-trivial c0 value -- not pure red/green/blue/white/black, so a broken pipeline
    // that happens to produce a plausible-looking color can't accidentally pass.
    constexpr float kPsConst[4] = {0.75f, 0.25f, 0.6f, 1.0f};

    // c1-c4: a diagonal scale-by-0.5 matrix (diagonal 0.5, 0.5, 0.5, 1.0). Diagonal matrices are
    // their own transpose, so this is immune to row-major-vs-column-major register packing
    // ambiguity in mul(vector, matrix) -- the result is identical either way.
    constexpr float kVsConst[16] = {
        0.5f, 0.0f, 0.0f, 0.0f, // c1
        0.0f, 0.5f, 0.0f, 0.0f, // c2
        0.0f, 0.0f, 0.5f, 0.0f, // c3
        0.0f, 0.0f, 0.0f, 1.0f, // c4
    };

    bool channel_close(const unsigned char actual, const int expected)
    {
        return std::abs(static_cast<int>(actual) - expected) <= 2;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-const-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9consttest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "const-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                                nullptr, nullptr, wc.hInstance, nullptr);
    printf("[d3d9-const-test] hwnd=%p\n", static_cast<void*>(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-const-test] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-const-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-const-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-const-test] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-const-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-const-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
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
    printf("[d3d9-const-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-const-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-const-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));

    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-const-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));

    vs_blob->Release();
    ps_blob->Release();

    if (FAILED(hcvs) || FAILED(hcps))
    {
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(3 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-const-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb), static_cast<void*>(vb));
    if (!vb)
    {
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    // Isoceles triangle symmetric about x=0, apex at top. At NDC y=0 (the horizontal midline, whose
    // mapped screen row is identical whether or not the render pipeline flips Y), the unscaled
    // triangle spans x in [-0.3, 0.3] and, after the VS applies the 0.5x scale from c1-c4, spans
    // x in [-0.15, 0.15]. Sampling along this midline makes the analytic check immune to any Y-flip
    // convention difference between D3D9 and Vulkan clip space.
    Vertex* verts = nullptr;
    vb->Lock(0, 3 * sizeof(Vertex), reinterpret_cast<void**>(&verts), 0);
    if (verts)
    {
        verts[0] = {0.0f, 0.6f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)};
        verts[1] = {0.6f, -0.6f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)};
        verts[2] = {-0.6f, -0.6f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)};
        vb->Unlock();
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(640, 480, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-const-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-const-test] FAIL: CreateRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
        vb->Release();
        vs->Release();
        ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    HRESULT hsrt = dev->SetRenderTarget(0, rt);
    printf("[d3d9-const-test] SetRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hsrt));

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);

    HRESULT hsvc = dev->SetVertexShaderConstantF(1, kVsConst, 4);
    printf("[d3d9-const-test] SetVertexShaderConstantF hr=0x%08lx\n", static_cast<unsigned long>(hsvc));
    HRESULT hspc = dev->SetPixelShaderConstantF(0, kPsConst, 1);
    printf("[d3d9-const-test] SetPixelShaderConstantF hr=0x%08lx\n", static_cast<unsigned long>(hspc));

    dev->BeginScene();
    // Background distinct from kPsConst's (0.75, 0.25, 0.6) magenta-ish tint, so the two are never
    // confusable in the pixel check below.
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
    HRESULT hdp = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
    printf("[d3d9-const-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdp));
    dev->EndScene();

    int failures = 0;

    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-const-test] LockRect hr=0x%08lx pBits=%p Pitch=%ld\n", static_cast<unsigned long>(hlr), lr.pBits,
           static_cast<long>(lr.Pitch));
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);

        // D3DLOCKED_RECT::Pitch is not populated by the UMD's Lock DDI implementation (the real
        // internal DDI struct's Pitch-equivalent field hasn't been RE'd yet -- see d3d9_ddi.hpp's
        // D3DDDIARG_LOCK comment; it prints as 0 above). The render target's backing store is known
        // tightly packed (32bpp, no row padding -- d3d9_host.cpp sizes it as width*height*4 and
        // reads/writes it flat), so compute the row stride directly instead of trusting lr.Pitch,
        // the same implicit assumption d3d9_triangle_test.cpp's single-pixel(0,0) check already relies on.
        constexpr LONG kStride = 640 * 4;

        // Point A: (col=320, row=240) -> NDC (~0.0, ~0.0), well inside the scaled-down triangle
        // (x in [-0.15, 0.15] at this row). Must show the exact PS constant color from c0.
        const unsigned char* pixel_a = base + 240 * kStride + 320 * 4;
        const int expected_b_a = static_cast<int>(kPsConst[2] * 255.0f + 0.5f);
        const int expected_g_a = static_cast<int>(kPsConst[1] * 255.0f + 0.5f);
        const int expected_r_a = static_cast<int>(kPsConst[0] * 255.0f + 0.5f);
        const int expected_a_a = static_cast<int>(kPsConst[3] * 255.0f + 0.5f);
        printf("[d3d9-const-test] pixelA(320,240)=B=%02X G=%02X R=%02X A=%02X (expected c0 B=%02X G=%02X R=%02X A=%02X)\n",
               pixel_a[0], pixel_a[1], pixel_a[2], pixel_a[3], expected_b_a, expected_g_a, expected_r_a, expected_a_a);
        if (!channel_close(pixel_a[0], expected_b_a) || !channel_close(pixel_a[1], expected_g_a) ||
            !channel_close(pixel_a[2], expected_r_a) || !channel_close(pixel_a[3], expected_a_a))
        {
            printf("[d3d9-const-test] FAIL: pixelA does not match PS constant c0 -- PS constant register did not "
                   "reach the shader\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-const-test] PASS: pixelA matches PS constant c0 exactly\n");
        }

        // Point B: (col=392, row=240) -> NDC (~0.226, ~0.0). Inside the ORIGINAL, unscaled triangle's
        // bounds (x in [-0.3, 0.3] at this row) but outside the scaled-down triangle's bounds (x in
        // [-0.15, 0.15]). Must show the background clear color -- only true if the VS-side c1-c4
        // scale matrix actually shrank the triangle.
        const unsigned char* pixel_b = base + 240 * kStride + 392 * 4;
        constexpr int expected_b_b = 0xFF;
        constexpr int expected_g_b = 0x80;
        constexpr int expected_r_b = 0x40;
        printf("[d3d9-const-test] pixelB(392,240)=B=%02X G=%02X R=%02X A=%02X (expected background B=%02X G=%02X "
               "R=%02X)\n",
               pixel_b[0], pixel_b[1], pixel_b[2], pixel_b[3], expected_b_b, expected_g_b, expected_r_b);
        if (!channel_close(pixel_b[0], expected_b_b) || !channel_close(pixel_b[1], expected_g_b) ||
            !channel_close(pixel_b[2], expected_r_b))
        {
            printf("[d3d9-const-test] FAIL: pixelB does not match background -- VS constant-register scale matrix "
                   "did not apply\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-const-test] PASS: pixelB matches background, confirming the VS-side scale applied\n");
        }

        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-const-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    rt->Release();
    vb->Release();
    vs->Release();
    ps->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-const-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
