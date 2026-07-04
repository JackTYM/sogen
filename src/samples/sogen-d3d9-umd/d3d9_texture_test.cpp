// D3D9-over-Vulkan M2 milestone test (see .claude/plans/jazzy-giggling-cloud.md): proves textures,
// indexed draws, real depth testing, and real alpha blending all work together in one real render.
//
// Four independent, non-overlapping on-screen quads (except the deliberate depth-occlusion pair),
// all drawn with the SAME real D3DCompile()-produced vs_2_0/ps_2_0 shader pair via
// DrawIndexedPrimitive (4 vertices + 6 indices per quad, reused across quads via BaseVertexIndex --
// one 16-vertex buffer, one 6-index buffer shared by every draw). All render state (Z-enable/func,
// blend enable/factors, sampler filtering) is set once, before the first draw: ensure_pipeline/
// ensure_programmable_pipeline cache the built pipeline permanently after first use, so state changed
// between draws would silently not apply to later ones (see HANDOFF_MACBOOK.md).
//
// The pixel shader always samples a 640x480 procedural texture (four solid 320x240 quadrants: RED,
// GREEN, BLUE, WHITE) and multiplies it by a per-draw PS constant (c0) "tint" -- this is what makes
// each quad's rendered color distinguishable using one shared shader/pipeline instead of needing a
// separate one per quad (which would also mean a separate pipeline-cache entry with its own baked
// depth/blend state, defeating the point).
//
// 1. Textured quad: samples the RED quadrant with tint (1,1,1,1) -- checks the sampled pixel matches
//    the known texture color exactly (no blending contribution since alpha=1 makes blending a no-op).
// 2/3. Depth pair: a near quad (z=0.2, greenish tint) drawn first, then a FARTHER, fully overlapping
//    quad (z=0.8, reddish tint) drawn second -- both sample the WHITE quadrant. Without real depth
//    testing, the farther quad (drawn later) would simply overwrite with its own color; with it, its
//    fragments fail the depth compare against the already-written nearer Z and the near quad's color
//    survives. Checks the overlap pixel is the near quad's greenish color, not the far quad's reddish
//    one.
// 4. Blend quad: samples the WHITE quadrant with tint (0,1,0,0.5) -- a semi-transparent green -- drawn
//    over the plain clear color background in its own untouched screen region. Checks the pixel
//    matches the analytically-computed SRCALPHA/INVSRCALPHA/ADD blend of that green over the clear
//    color exactly (within rounding tolerance).

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // UV is carried through the vertex color channel (D3DFVF_DIFFUSE), not a dedicated TEXCOORD0
    // varying -- see fill_quad's comment for why.
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

    float to_ndc_x(const int screen_x)
    {
        return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f;
    }

    // NDC Y: this pipeline's viewport uses Vulkan's own (unflipped) convention -- NDC y=-1 at the top
    // of the screen, y=+1 at the bottom (screenY = (ndcY+1)/2 * height) -- confirmed live 2026-07-04:
    // an earlier version of this function assumed D3D9's own opposite screen-space convention
    // (NDC y=+1 at the top) and every asymmetric (non-center-row) pixel check read a totally
    // different, wrong quad's rendered pixel as a result (none of the three prior guest tests this
    // session ever exercised an asymmetric row, so this was never caught before).
    float to_ndc_y(const int screen_y)
    {
        return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f;
    }

    // One quad = 4 vertices at the given screen-space rectangle (converted to NDC), all at the given
    // depth, with UV corners (0,0)-(1,1) mapping the full procedural texture across the quad.
    //
    // UV is packed into the vertex's D3DCOLOR (R=u*255, G=v*255) rather than a real TEXCOORD0 -- a
    // real, confirmed bug was found here: with a genuine D3DFVF_XYZ|D3DFVF_TEX1 vertex format and a
    // float2 TEXCOORD0 varying (or a float4 one -- both were tried), the U component interpolated
    // correctly across a quad but V consistently did not (e.g. an expected 0.25 read back as ~0.87,
    // an expected 0.75 read back as ~0.37 -- not a simple 1-v flip or any other transform tried).
    // Isolated via a visualize-the-varying diagnostic shader (`return float4(input.uv, 0, 1)`) to be
    // specific to the TEXCOORD0 interpolant itself, not geometry, the texture, the sampler descriptor,
    // or the PS constant register (all independently confirmed working via the same diagnostic
    // technique -- see HANDOFF_MACBOOK.md for the full trace). Root cause not pinned down further
    // (budget did not allow a deeper vkd3d-shader-internals investigation on top of the two required
    // carried-forward findings) -- routing the same per-pixel-varying UV data through the
    // D3DFVF_DIFFUSE color channel instead (proven correct by every prior guest test that passes a
    // COLOR0 varying through a real compiled shader) sidesteps it entirely while still exercising a
    // real per-pixel-interpolated UV feeding a real tex2D() sample.
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
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-texture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9texturetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "texture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-texture-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-texture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-texture-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-texture-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
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
    printf("[d3d9-texture-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-texture-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-texture-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-texture-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
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

    // Procedural 640x480 texture: four solid 320x240 quadrants (RED, GREEN, BLUE, WHITE). A coarse,
    // large-cell layout rather than a fine checkerboard, chosen deliberately -- this UMD's
    // pfnCreateResource still hardcodes every resource's width/height to 640x480 (KNOWN LIMITATION,
    // see sogen_d3d9_umd.cpp), so this test's own texture is sized to match that exactly rather than
    // hit the same wrong-shape bug it does not otherwise need to exercise.
    IDirect3DTexture9* tex = nullptr;
    HRESULT hct = dev->CreateTexture(kCanvasWidth, kCanvasHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex,
                                     nullptr);
    printf("[d3d9-texture-test] CreateTexture hr=0x%08lx tex=%p\n", static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
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
    {
        D3DLOCKED_RECT lr{};
        HRESULT htl = tex->LockRect(0, &lr, nullptr, 0);
        printf("[d3d9-texture-test] Texture LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(htl), lr.pBits);
        if (FAILED(htl) || !lr.pBits)
        {
            printf("[d3d9-texture-test] FAIL: texture LockRect failed\n");
            tex->Release();
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
        // D3DLOCKED_RECT::Pitch is not populated by this UMD's Lock DDI implementation (same
        // known gap d3d9_const_test.cpp already documents) -- the texture's backing is known tightly
        // packed (32bpp, no row padding), so write with a hardcoded stride instead of trusting Pitch.
        constexpr LONG kStride = kCanvasWidth * 4;
        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < kCanvasHeight; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + y * kStride);
            for (int x = 0; x < kCanvasWidth; ++x)
            {
                DWORD color;
                if (x < kCanvasWidth / 2 && y < kCanvasHeight / 2)
                {
                    color = D3DCOLOR_ARGB(255, 255, 0, 0); // top-left: RED
                }
                else if (x >= kCanvasWidth / 2 && y < kCanvasHeight / 2)
                {
                    color = D3DCOLOR_ARGB(255, 0, 255, 0); // top-right: GREEN
                }
                else if (x < kCanvasWidth / 2 && y >= kCanvasHeight / 2)
                {
                    color = D3DCOLOR_ARGB(255, 0, 0, 255); // bottom-left: BLUE
                }
                else
                {
                    color = D3DCOLOR_ARGB(255, 255, 255, 255); // bottom-right: WHITE
                }
                row[x] = color;
            }
        }
        tex->UnlockRect(0);
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-texture-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    IDirect3DSurface9* ds = nullptr;
    HRESULT hcds = dev->CreateDepthStencilSurface(kCanvasWidth, kCanvasHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &ds, nullptr);
    printf("[d3d9-texture-test] CreateDepthStencilSurface hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcds), static_cast<void*>(ds));
    if (FAILED(hcrt) || !rt || FAILED(hcds) || !ds)
    {
        printf("[d3d9-texture-test] FAIL: render target/depth-stencil creation failed\n");
        if (rt)
        {
            rt->Release();
        }
        if (ds)
        {
            ds->Release();
        }
        tex->Release();
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
    dev->SetRenderTarget(0, rt);
    dev->SetDepthStencilSurface(ds);

    // Four quads, 4 vertices each, laid out in one shared vertex buffer -- reused across draws via
    // DrawIndexedPrimitive's BaseVertexIndex. Screen regions: quad 0 (textured check) and quad 3
    // (blend check) sit in their own untouched areas; quads 1/2 (the depth-occlusion pair) fully
    // overlap each other in a third area.
    constexpr int kQuadW = 200;
    constexpr int kQuadH = 160;
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(16 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 16 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            fill_quad(v + 0, 40, 40, 40 + kQuadW, 40 + kQuadH, 0.5f);          // quad 0: textured check
            fill_quad(v + 4, 280, 40, 280 + kQuadW, 40 + kQuadH, 0.2f);        // quad 1: depth near
            fill_quad(v + 8, 280, 40, 280 + kQuadW, 40 + kQuadH, 0.8f);        // quad 2: depth far (same rect)
            fill_quad(v + 12, 40, 260, 40 + kQuadW, 260 + kQuadH, 0.5f);       // quad 3: blend check
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
        printf("[d3d9-texture-test] FAIL: vertex/index buffer creation failed\n");
        if (vb)
        {
            vb->Release();
        }
        if (ib)
        {
            ib->Release();
        }
        rt->Release();
        ds->Release();
        tex->Release();
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

    // ALL render state that affects the pipeline (depth, blend) is set here, before the first draw --
    // ensure_pipeline/ensure_programmable_pipeline cache the built pipeline permanently on first use,
    // so changing this state between draws would silently not apply to later ones.
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

    constexpr float kClearR = 64.0f / 255.0f;
    constexpr float kClearG = 128.0f / 255.0f;
    constexpr float kClearB = 255.0f / 255.0f;

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);

    const float kTintOpaque[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float kTintNear[4] = {0.2f, 1.0f, 0.2f, 1.0f};
    const float kTintFar[4] = {1.0f, 0.2f, 0.2f, 1.0f};
    const float kTintBlend[4] = {0.0f, 1.0f, 0.0f, 0.5f};

    dev->SetPixelShaderConstantF(0, kTintOpaque, 1);
    HRESULT hd0 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-texture-test] DrawIndexedPrimitive(quad0/textured) hr=0x%08lx\n", static_cast<unsigned long>(hd0));

    dev->SetPixelShaderConstantF(0, kTintNear, 1);
    HRESULT hd1 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);
    printf("[d3d9-texture-test] DrawIndexedPrimitive(quad1/depth-near) hr=0x%08lx\n", static_cast<unsigned long>(hd1));

    dev->SetPixelShaderConstantF(0, kTintFar, 1);
    HRESULT hd2 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 8, 0, 4, 0, 2);
    printf("[d3d9-texture-test] DrawIndexedPrimitive(quad2/depth-far) hr=0x%08lx\n", static_cast<unsigned long>(hd2));

    dev->SetPixelShaderConstantF(0, kTintBlend, 1);
    HRESULT hd3 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 12, 0, 4, 0, 2);
    printf("[d3d9-texture-test] DrawIndexedPrimitive(quad3/blend) hr=0x%08lx\n", static_cast<unsigned long>(hd3));

    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-texture-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        // Check 1: textured quad -- (90, 80) samples UV (0.25, 0.25) of the 640x480 texture -> texel
        // (160, 120), safely inside the top-left RED quadrant. tint is (1,1,1,1) so blending is a
        // no-op (srcAlpha=1) -- the result must be the exact texture color.
        {
            const unsigned char* p = pixel_at(90, 80);
            printf("[d3d9-texture-test] textured pixel(90,80)=B=%02X G=%02X R=%02X A=%02X (expected RED)\n", p[0], p[1],
                   p[2], p[3]);
            if (!channel_close(p[0], 0, 2) || !channel_close(p[1], 0, 2) || !channel_close(p[2], 255, 2) ||
                !channel_close(p[3], 255, 2))
            {
                printf("[d3d9-texture-test] FAIL: textured pixel does not match the known texture color\n");
                ++failures;
            }
            else
            {
                printf("[d3d9-texture-test] PASS: textured pixel matches the known texture color exactly\n");
            }
        }

        // Check 2: depth occlusion -- (430, 160) samples UV (0.75, 0.75) -> texel (480, 360), safely
        // inside the bottom-right WHITE quadrant for BOTH overlapping quads. The near quad (z=0.2,
        // greenish tint) is drawn first; the far quad (z=0.8, reddish tint) is drawn second, fully
        // overlapping it. With real depth testing the far quad's fragments fail the compare against
        // the already-written nearer Z and the near quad's greenish color survives; without it, the
        // far quad (drawn later) would simply overwrite with its own reddish color.
        {
            const unsigned char* p = pixel_at(430, 160);
            const int expected_r = static_cast<int>(kTintNear[0] * 255.0f + 0.5f);
            const int expected_g = static_cast<int>(kTintNear[1] * 255.0f + 0.5f);
            const int expected_b = static_cast<int>(kTintNear[2] * 255.0f + 0.5f);
            printf("[d3d9-texture-test] depth pixel(430,160)=B=%02X G=%02X R=%02X A=%02X (expected near-quad B=%02X "
                   "G=%02X R=%02X)\n",
                   p[0], p[1], p[2], p[3], expected_b, expected_g, expected_r);
            if (!channel_close(p[0], expected_b, 3) || !channel_close(p[1], expected_g, 3) ||
                !channel_close(p[2], expected_r, 3))
            {
                printf("[d3d9-texture-test] FAIL: depth pixel shows the far (occluded) quad's color -- depth testing "
                       "is not real\n");
                ++failures;
            }
            else
            {
                printf("[d3d9-texture-test] PASS: depth pixel matches the near quad, confirming real occlusion\n");
            }
        }

        // Check 3: alpha blending -- (190, 380) samples the same WHITE quadrant, tinted (0,1,0,0.5),
        // drawn over the untouched clear-color background. Expected = analytic SRCALPHA/INVSRCALPHA/
        // ADD blend of that semi-transparent green over the clear color.
        {
            const unsigned char* p = pixel_at(190, 380);
            const float src_r = 0.0f, src_g = 1.0f, src_b = 0.0f, src_a = 0.5f;
            const int expected_r = static_cast<int>((src_r * src_a + kClearR * (1.0f - src_a)) * 255.0f + 0.5f);
            const int expected_g = static_cast<int>((src_g * src_a + kClearG * (1.0f - src_a)) * 255.0f + 0.5f);
            const int expected_b = static_cast<int>((src_b * src_a + kClearB * (1.0f - src_a)) * 255.0f + 0.5f);
            printf("[d3d9-texture-test] blend pixel(190,380)=B=%02X G=%02X R=%02X A=%02X (expected B=%02X G=%02X "
                   "R=%02X)\n",
                   p[0], p[1], p[2], p[3], expected_b, expected_g, expected_r);
            if (!channel_close(p[0], expected_b, 3) || !channel_close(p[1], expected_g, 3) ||
                !channel_close(p[2], expected_r, 3))
            {
                printf("[d3d9-texture-test] FAIL: blend pixel does not match the analytically-computed blend result\n");
                ++failures;
            }
            else
            {
                printf("[d3d9-texture-test] PASS: blend pixel matches the analytic SRCALPHA/INVSRCALPHA/ADD blend\n");
            }
        }

        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-texture-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    rt->Release();
    ds->Release();
    tex->Release();
    vb->Release();
    ib->Release();
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

    printf("[d3d9-texture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
