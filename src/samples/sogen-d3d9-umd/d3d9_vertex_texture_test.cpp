// D3D9-over-Vulkan vertex-texture-fetch (VTF) test: proves a real vs_3_0 VERTEX shader samples a
// texture bound to D3DVERTEXTEXTURESAMPLER0 and uses the sampled value to DISPLACE a vertex position --
// the SM3.0 vertex-texture path wired in d3d9_shader_translator.cpp (VS combined-image-samplers into
// descriptor set 0) and d3d9_host.cpp (vs_bindings sampler slots, descriptor-pool sizing, and
// execute_draw's per-stage vertex-texture upload/descriptor-write loop keyed off bound_textures[257+k]).
//
// Discriminator design (a genuine proof the VERTEX shader consumed real texel data, not fakeable by a
// pixel shader): one triangle with two fixed base vertices and one "apex" vertex. Each vertex carries
// its own TEXCOORD0, and the vs_3_0 samples a tiny 2x2 A16B16G16R16F "heightmap" with tex2Dlod at that
// per-vertex UV, then subtracts height*scale from the vertex's NDC Y (moving it toward the top of the
// screen). The two base vertices sample a texel whose height is 0.0 (they stay put); the apex samples a
// DIFFERENT texel whose height is 1.0, so ONLY the apex is displaced -- from baseline screen y=300 up to
// screen y=100. Because only the vertex whose UV points at the high texel moves, a correct result cannot
// be produced by any constant offset or by a pixel shader: it requires the vertex stage to fetch the
// texel selected by that vertex's own UV.
//
// Probes (BGRA readback of an off-screen X8R8G8B8 render target):
//  - P_HIGH (320,180): ABOVE the un-displaced triangle's apex (y=300) but well INSIDE the displaced
//    triangle (apex y=100). Reads ORANGE only if the apex was really displaced by the sampled height;
//    reads the CLEAR color if VTF silently degraded (translation failure -> whole draw skipped) or if
//    the vertex was not displaced. THIS IS THE DISCRIMINATOR.
//  - P_BASE (320,370): inside the triangle near its base regardless of displacement -- a control that
//    reads ORANGE whenever the draw happened at all (tells "drawn but not displaced" apart from "draw
//    skipped entirely").
//  - P_CORNER (30,30): outside every triangle in both cases -- must always stay CLEAR.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>

namespace
{
    // vs_3_0: samples the heightmap at the vertex's own UV and displaces NDC Y by height*kDispScale.
    // tex2Dlod (explicit LOD 0) is the SM3.0 vertex-texture-fetch instruction (texldl) -- ps_2_0/vs_2_0
    // cannot express it, so this genuinely requires the vertex-texture path. The output color is a fixed
    // ORANGE so the pixel stage does no sampling of its own; the whole test isolates VS-side texture
    // fetch. kDispScale is baked to match the C++ analytic prediction below.
    const char* const k_vertex_shader_hlsl = R"(
sampler2D heightmap : register(s0);
struct VSInput  { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOutput { float4 pos : POSITION; float4 color : COLOR0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    float h = tex2Dlod(heightmap, float4(input.uv, 0.0, 0.0)).r;
    float3 p = input.pos;
    p.y = p.y - h * 0.8333333;
    output.pos = float4(p, 1.0);
    output.color = float4(1.0, 0.5, 0.0, 1.0);
    return output;
}
)";

    // Pixel shader just passes the interpolated (constant) vertex color through -- no sampling here.
    const char* const k_pixel_shader_hlsl = R"(
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return input.color;
}
)";

    struct Vertex
    {
        float x, y, z;
        float u, v;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_TEX1;
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Clear color (dark blue) -- distinct from the ORANGE triangle so a covered vs. uncovered probe is
    // unambiguous.
    constexpr int kClearR = 0, kClearG = 0, kClearB = 64;

    float to_ndc_x(const int screen_x) { return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f; }

    // NDC Y uses this pipeline's unflipped Vulkan convention: y=-1 at the TOP of the screen, y=+1 at the
    // bottom (see d3d9_texture_test.cpp / d3d9_texcoord_test.cpp for the live-confirmed finding). So a
    // smaller screen Y is a more-negative NDC Y, and the VS subtracting from Y moves the apex UP.
    float to_ndc_y(const int screen_y) { return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f; }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    // IEEE-754 binary16 encoder -- only 0.0 and 1.0 are needed here, but a real encoder keeps the intent
    // clear and avoids magic hex constants. 0.0 -> 0x0000, 1.0 -> 0x3C00.
    uint16_t float_to_half(const float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        const uint32_t sign = (bits >> 16) & 0x8000u;
        const int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFFu) - 127 + 15;
        const uint32_t mantissa = bits & 0x7FFFFFu;
        if (exponent <= 0)
        {
            return static_cast<uint16_t>(sign); // flush subnormals/zero to signed zero (enough here)
        }
        if (exponent >= 0x1F)
        {
            return static_cast<uint16_t>(sign | 0x7C00u); // inf/overflow
        }
        return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
    }

    void release_all(IDirect3DTexture9* tex, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
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
    printf("[d3d9-vertex-texture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9vertextexturetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "vertex-texture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-vertex-texture-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-vertex-texture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    // Explicit caps check: vertex-texture fetch requires vs_3_0. If the SM3.0 caps didn't take, bail with a
    // clear message rather than failing obscurely later.
    D3DCAPS9 caps{};
    dev->GetDeviceCaps(&caps);
    printf("[d3d9-vertex-texture-test] VertexShaderVersion=0x%08lx (need >= vs_3_0=0x%08lx)\n",
           static_cast<unsigned long>(caps.VertexShaderVersion), static_cast<unsigned long>(D3DVS_VERSION(3, 0)));
    if (caps.VertexShaderVersion < D3DVS_VERSION(3, 0))
    {
        printf("[d3d9-vertex-texture-test] FAIL: vs_3_0 not reported\n");
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_3_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-vertex-texture-test] D3DCompile(vs_3_0) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-vertex-texture-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_3_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-vertex-texture-test] D3DCompile(ps_3_0) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-vertex-texture-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-vertex-texture-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-vertex-texture-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // 2x2 A16B16G16R16F heightmap. Texel (col 0, row 0) has R=0.0 (low, no displacement); texel
    // (col 1, row 0) has R=1.0 (high, full displacement). Alpha=1.0 in every texel; G/B unused. With
    // POINT filtering (the default and the only mode real D3D9 allows for VTF) and CLAMP/WRAP addressing,
    // a UV at a texel center reads that texel's exact half-float value with no interpolation.
    // D3DPOOL_MANAGED (usage 0) so the texture is lockable -- a non-dynamic D3DPOOL_DEFAULT texture is
    // not (same lockable-pool choice as d3d9_managed_texture_test.cpp).
    constexpr int kTexW = 2;
    constexpr int kTexH = 2;
    IDirect3DTexture9* tex = nullptr;
    HRESULT hct = dev->CreateTexture(kTexW, kTexH, 1, 0, D3DFMT_A16B16G16R16F, D3DPOOL_MANAGED, &tex, nullptr);
    printf("[d3d9-vertex-texture-test] CreateTexture(2x2 A16B16G16R16F) hr=0x%08lx tex=%p\n",
           static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        printf("[d3d9-vertex-texture-test] FAIL: heightmap CreateTexture failed\n");
        release_all(nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-vertex-texture-test] heightmap LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(htl),
               lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-vertex-texture-test] FAIL: heightmap LockRect failed\n");
            release_all(tex, nullptr, nullptr, vs, ps, dev, d3d);
            return 1;
        }
        // Backing is tightly packed at 8 bytes/texel (Pitch is not populated by this UMD's Lock DDI --
        // same known gap the other texture tests document), so write with a hardcoded 2-texel stride.
        constexpr LONG kStride = kTexW * 4 * static_cast<LONG>(sizeof(uint16_t)); // 4 halfs per texel
        const uint16_t h0 = float_to_half(0.0f);
        const uint16_t h1 = float_to_half(1.0f);
        const uint16_t one = float_to_half(1.0f);
        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < kTexH; ++y)
        {
            auto* row = reinterpret_cast<uint16_t*>(base + y * kStride);
            for (int x = 0; x < kTexW; ++x)
            {
                const uint16_t height = (x == 1) ? h1 : h0; // col 1 = high, col 0 = low
                uint16_t* texel = row + x * 4;
                texel[0] = height; // R = height
                texel[1] = h0;     // G
                texel[2] = h0;     // B
                texel[3] = one;    // A
            }
        }
        tex->UnlockRect(0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt,
                                           nullptr);
    printf("[d3d9-vertex-texture-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-vertex-texture-test] FAIL: render target creation failed\n");
        release_all(tex, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // Triangle: two base vertices (uv -> low texel, height 0, not displaced) at screen (220,400) and
    // (420,400); apex (uv -> high texel, height 1) at baseline screen (320,300). The VS moves the apex up
    // by 0.8333 NDC == 200 screen px, landing it at screen y=100. Base UV = center of texel (0,0) =
    // (0.25,0.25); apex UV = center of texel (1,0) = (0.75,0.25).
    constexpr float kLowU = 0.25f, kLowV = 0.25f;  // texel (col 0,row 0) -> height 0
    constexpr float kHighU = 0.75f, kHighV = 0.25f; // texel (col 1,row 0) -> height 1
    const Vertex verts[3] = {
        {to_ndc_x(220), to_ndc_y(400), 0.5f, kLowU, kLowV},  // base-left
        {to_ndc_x(420), to_ndc_y(400), 0.5f, kLowU, kLowV},  // base-right
        {to_ndc_x(320), to_ndc_y(300), 0.5f, kHighU, kHighV}, // apex (baseline y=300, displaced to y=100)
    };

    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(sizeof(verts), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (!vb)
    {
        printf("[d3d9-vertex-texture-test] FAIL: vertex buffer creation failed\n");
        release_all(tex, rt, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        void* p = nullptr;
        vb->Lock(0, sizeof(verts), &p, 0);
        if (p)
        {
            std::memcpy(p, verts, sizeof(verts));
            vb->Unlock();
        }
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    // Bind the heightmap to the VERTEX sampler (D3DVERTEXTEXTURESAMPLER0 == 257) -- NOT the pixel sampler.
    // The pixel shader never samples, so a wrong binding here shows up directly as an un-displaced apex.
    HRESULT hst = dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, tex);
    printf("[d3d9-vertex-texture-test] SetTexture(D3DVERTEXTEXTURESAMPLER0) hr=0x%08lx\n",
           static_cast<unsigned long>(hst));
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(kClearR, kClearG, kClearB), 1.0f, 0);
    HRESULT hd = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
    printf("[d3d9-vertex-texture-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-vertex-texture-test] rt LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        // ORANGE = (R=255, G=128, B=0) -> BGRA bytes B=00 G=80 R=FF.
        auto expect_orange = [&](const char* name, const int x, const int y) {
            const unsigned char* p = pixel_at(x, y);
            printf("[d3d9-vertex-texture-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X (expected ORANGE B=00 G=80 R=FF)\n",
                   name, x, y, p[0], p[1], p[2]);
            if (!channel_close(p[0], 0, 4) || !channel_close(p[1], 128, 6) || !channel_close(p[2], 255, 4))
            {
                printf("[d3d9-vertex-texture-test] FAIL: %s is not ORANGE\n", name);
                ++failures;
                return;
            }
            printf("[d3d9-vertex-texture-test] PASS: %s is ORANGE\n", name);
        };
        auto expect_clear = [&](const char* name, const int x, const int y) {
            const unsigned char* p = pixel_at(x, y);
            printf("[d3d9-vertex-texture-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X (expected CLEAR B=%02X G=%02X R=%02X)\n",
                   name, x, y, p[0], p[1], p[2], kClearB, kClearG, kClearR);
            if (!channel_close(p[0], kClearB, 4) || !channel_close(p[1], kClearG, 4) || !channel_close(p[2], kClearR, 4))
            {
                printf("[d3d9-vertex-texture-test] FAIL: %s is not the CLEAR color\n", name);
                ++failures;
                return;
            }
            printf("[d3d9-vertex-texture-test] PASS: %s is the CLEAR color\n", name);
        };

        // THE DISCRIMINATOR: (320,180) is above the un-displaced apex (y=300) and only inside the triangle
        // if the apex was really displaced up to y=100 by the sampled height. ORANGE here == the vertex
        // shader fetched the high texel and moved the apex.
        expect_orange("P_HIGH(displaced-only)", 320, 180);
        // Control: near the base, inside the triangle whether or not the apex moved -- ORANGE confirms the
        // draw happened at all (so a CLEAR P_HIGH would mean "drawn but not displaced", not "not drawn").
        expect_orange("P_BASE(control)", 320, 370);
        // Sanity: a corner outside every triangle stays the clear color.
        expect_clear("P_CORNER(background)", 30, 30);

        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-vertex-texture-test] FAIL: rt LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(tex, rt, vb, vs, ps, dev, d3d);

    printf("[d3d9-vertex-texture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
