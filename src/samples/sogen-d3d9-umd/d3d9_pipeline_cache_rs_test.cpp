// Pipeline-cache-key gap test for RENDER STATE (not RT shape -- that's d3d9_pipeline_cache_test.cpp's
// job). `pipeline_cache_key` (d3d9_host.hpp) is keyed by vertex_shader/pixel_shader/color_formats[4]/
// depth_format/vertex_shape -- it does NOT include any D3DRS_* render-state fields, even though
// build_depth_state/build_blend_state (d3d9_host.cpp) read D3DRS_ZENABLE/ZWRITEENABLE/ZFUNC and
// D3DRS_ALPHABLENDENABLE/SRCBLEND/DESTBLEND/BLENDOP out of the SAME app-accumulated render_state map and
// bake the result as STATIC (non-dynamic) pipeline state into every VkPipeline ensure_programmable_
// pipeline builds (see create_graphics_pipeline's depth/blend parameters in vulkan_host.cpp). Since
// ensure_programmable_pipeline's cache lookup (d3d9_host.cpp, `this->programmable_pipelines_.find(key)`)
// is a pure key lookup with no comparison against the CURRENT render_state at all, a second draw with the
// exact same VS/PS pair, bound-RT shape, and vertex shape as an earlier draw -- but a DIFFERENT blend (or
// depth) render-state setting in between -- collapses to the SAME key and gets back the FIRST draw's
// stale pipeline object, silently rendering with the WRONG baked blend/depth state.
//
// Discriminator (must fail against the current, unfixed cache key; must pass once render-state fields
// are folded into pipeline_cache_key): compile ONE vs_2_0/ps_2_0 pair -- the same NDC-passthrough VS as
// d3d9_pipeline_cache_test.cpp (D3DFVF_XYZ|D3DFVF_DIFFUSE, the 16-byte fixed-function-fallback shape, no
// vertex-decl parser involved) and a PS that always outputs a constant, distinctive, semi-transparent
// color -- solid GREEN at alpha 0.5 (float4(0,1,0,0.5)), hardcoded directly in the PS with no shader
// constants needed, deliberately simpler than d3d9_texture_test.cpp's tint-via-c0 approach since this
// test only needs ONE fixed color, not a per-draw parameter.
//
//   Sub-pass 1 (opaque): D3DRS_ALPHABLENDENABLE left at its real default (disabled -- build_blend_state's
//   own comment confirms unset/0 always yields blend_enable=0). Bind RT0, clear it BLACK, draw the
//   full-screen quad with the VS/PS pair. This builds/caches a blend-DISABLED pipeline for this exact
//   VS/PS/RT-shape/vertex-shape key. With blending off, the PS's RGB output is written straight through
//   regardless of its alpha -- RT0 must read back solid GREEN (unblended).
//
//   Sub-pass 2 (the actual discriminator): WITHOUT recreating the VS/PS objects (same IDirect3D
//   VertexShader9*/IDirect3DPixelShader9* the whole test), set D3DRS_ALPHABLENDENABLE=TRUE,
//   D3DRS_SRCBLEND=D3DBLEND_SRCALPHA, D3DRS_DESTBLEND=D3DBLEND_INVSRCALPHA. Re-clear the SAME RT0 BLACK
//   and draw the SAME quad again with the SAME VS/PS pair -- VS/PS/RT-shape/vertex-shape are all
//   identical to sub-pass 1, so pipeline_cache_key collapses to the same value. Under the OLD/buggy key,
//   ensure_programmable_pipeline's find() hits sub-pass 1's cache entry and returns its blend-DISABLED
//   VkPipeline, so this draw renders exactly like sub-pass 1 again -- solid GREEN, alpha ignored. Under a
//   fixed key that also folds in the blend render state, this draw needs (and gets) a genuinely different,
//   freshly-built blend-ENABLED pipeline, and the analytically-correct SRCALPHA/INVSRCALPHA blend of
//   GREEN(a=0.5) over the BLACK clear applies: result = src*a + dst*(1-a) = (0,1,0)*0.5 + (0,0,0)*0.5 =
//   (0, 0.5, 0), i.e. G≈128, R=0, B=0. This test asserts THAT correctly-blended value -- which is exactly
//   what should FAIL against the current, unfixed host code (the old code keeps rendering solid GREEN,
//   G=255, not G≈128), proving the render-state cache-key gap is real and reachable.
//
// No depth-state sub-check: keeping this test to the one, unambiguous blend discriminator above (per this
// task's own instructions) rather than complicating it with a second bound depth-stencil surface and a
// second discriminator.

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
struct VSOutput { float4 pos : POSITION; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    return output;
}
)";

    // Constant, distinctive, semi-transparent color -- GREEN at alpha 0.5. Hardcoded directly (no shader
    // constants needed): this test only ever needs the one fixed color, drawn twice under two different
    // blend render states.
    const char* const k_pixel_shader_hlsl = R"(
float4 main() : COLOR0
{
    return float4(0.0, 1.0, 0.0, 0.5);
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    // Matches d3d9_pipeline_cache_test.cpp/d3d9_mrt_test.cpp/d3d9_const_test.cpp's established shape --
    // the one 16-byte vertex layout ensure_programmable_pipeline already recognizes with no real vertex
    // declaration bound.
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) -- every
    // resource this UMD creates is actually backed by a 640x480 surface regardless of the size the app
    // requests, same approach as every other guest test in this directory.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    struct Point
    {
        const char* name;
        int col, row;
    };

    constexpr Point kCheckPoints[3] = {
        {"top-left corner", 10, 10},
        {"center", 320, 240},
        {"bottom-right corner", 630, 470},
    };
    constexpr int kCheckPointCount = 3;

    int check_surface_uniform(IDirect3DSurface9* surf, const char* pass_name, const int expected_b,
                               const int expected_g, const int expected_r, const int tolerance)
    {
        int failures = 0;
        D3DLOCKED_RECT lr{};
        HRESULT hlr = surf->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-pipeline-cache-rs-test] %s RT0 LockRect hr=0x%08lx pBits=%p\n", pass_name,
               static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-pipeline-cache-rs-test] FAIL: %s RT0 LockRect failed\n", pass_name);
            return kCheckPointCount;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        for (const auto& pt : kCheckPoints)
        {
            const unsigned char* p = pixel_at(pt.col, pt.row);
            printf("[d3d9-pipeline-cache-rs-test] %s RT0 %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name,
                   pt.name, pt.col, pt.row, p[0], p[1], p[2], p[3]);
            if (!channel_close(p[0], expected_b, tolerance) || !channel_close(p[1], expected_g, tolerance) ||
                !channel_close(p[2], expected_r, tolerance))
            {
                printf("[d3d9-pipeline-cache-rs-test] FAIL: %s RT0 %s does not match the expected color\n", pass_name,
                       pt.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-pipeline-cache-rs-test] PASS: %s RT0 %s matches the expected color\n", pass_name,
                       pt.name);
            }
        }

        surf->UnlockRect();
        return failures;
    }

    void release_all(IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib, IDirect3DSurface9* rt0,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev,
                     IDirect3D9* d3d)
    {
        if (ib)
        {
            ib->Release();
        }
        if (vb)
        {
            vb->Release();
        }
        if (rt0)
        {
            rt0->Release();
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
    printf("[d3d9-pipeline-cache-rs-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9pipelinecachersrtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "pipeline-cache-rs-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-pipeline-cache-rs-test] FAIL: Direct3DCreate9 returned null\n");
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
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
                                   &dev);
    printf("[d3d9-pipeline-cache-rs-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-pipeline-cache-rs-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-pipeline-cache-rs-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-pipeline-cache-rs-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-pipeline-cache-rs-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-pipeline-cache-rs-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-pipeline-cache-rs-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // D3DFMT_X8R8G8B8, not D3DFMT_A8R8G8B8: the UMD's FORMATOP table only flags X8R8G8B8 with
    // FMT_OP_OFFSCREEN_RENDERTARGET (see d3d9_mrt_test.cpp's header comment for the full account).
    IDirect3DSurface9* rt0 = nullptr;
    HRESULT hcrt0 = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                            TRUE, &rt0, nullptr);
    printf("[d3d9-pipeline-cache-rs-test] CreateRenderTarget(RT0) hr=0x%08lx surf=%p\n",
           static_cast<unsigned long>(hcrt0), static_cast<void*>(rt0));
    if (FAILED(hcrt0) || !rt0)
    {
        printf("[d3d9-pipeline-cache-rs-test] FAIL: RT0 creation failed\n");
        release_all(nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Full-screen quad in NDC (-1,-1)-(1,1) -- covers the whole viewport regardless of Y convention.
    // Vertex color is irrelevant (the PS ignores its input and writes a constant color) but
    // D3DFVF_DIFFUSE is part of the recognized 16-byte vertex layout, so it's populated anyway.
    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-pipeline-cache-rs-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-pipeline-cache-rs-test] FAIL: vertex buffer creation failed\n");
        release_all(nullptr, nullptr, rt0, vs, ps, dev, d3d);
        return 1;
    }
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            const DWORD white = D3DCOLOR_XRGB(255, 255, 255);
            v[0] = {-1.0f, -1.0f, 0.5f, white};
            v[1] = {1.0f, -1.0f, 0.5f, white};
            v[2] = {1.0f, 1.0f, 0.5f, white};
            v[3] = {-1.0f, 1.0f, 0.5f, white};
            vb->Unlock();
        }
    }

    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hcib = dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    printf("[d3d9-pipeline-cache-rs-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib),
           static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-pipeline-cache-rs-test] FAIL: index buffer creation failed\n");
        release_all(vb, nullptr, rt0, vs, ps, dev, d3d);
        return 1;
    }
    {
        WORD* idx = nullptr;
        ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0);
        if (idx)
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
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderTarget(0, rt0);

    int failures = 0;

    // Sub-pass 1 (opaque): D3DRS_ALPHABLENDENABLE explicitly FALSE (the real default -- see build_blend_
    // state's own comment) -- builds/caches a blend-DISABLED pipeline for this exact VS/PS/RT-shape/
    // vertex-shape key. With blending off, the PS's RGB is written straight through -- solid GREEN.
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    {
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-pipeline-cache-rs-test] sub-pass 1 (blend disabled) DrawIndexedPrimitive hr=0x%08lx\n",
               static_cast<unsigned long>(hd));
        dev->EndScene();

        // Unblended: solid GREEN (B=0x00, G=0xFF, R=0x00).
        failures += check_surface_uniform(rt0, "sub-pass-1 (blend disabled)", 0, 255, 0, 2);
    }

    // Sub-pass 2 (the discriminator): SAME vs/ps (never recreated), SAME RT0, SAME vertex shape -- only
    // the blend render state changes (disabled -> SRCALPHA/INVSRCALPHA enabled). pipeline_cache_key is
    // unchanged from sub-pass 1's, so under the current, unfixed cache key this draw incorrectly reuses
    // sub-pass 1's blend-DISABLED VkPipeline.
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    {
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-pipeline-cache-rs-test] sub-pass 2 (blend enabled) DrawIndexedPrimitive hr=0x%08lx\n",
               static_cast<unsigned long>(hd));
        dev->EndScene();

        // Analytically-correct SRCALPHA/INVSRCALPHA blend of GREEN(a=0.5) over the BLACK clear:
        // result = src*a + dst*(1-a) = (0,1,0)*0.5 + (0,0,0)*0.5 = (0, 0.5, 0).
        const float src_r = 0.0f, src_g = 1.0f, src_b = 0.0f, src_a = 0.5f;
        const float dst_r = 0.0f, dst_g = 0.0f, dst_b = 0.0f;
        const int expected_r = static_cast<int>((src_r * src_a + dst_r * (1.0f - src_a)) * 255.0f + 0.5f);
        const int expected_g = static_cast<int>((src_g * src_a + dst_g * (1.0f - src_a)) * 255.0f + 0.5f);
        const int expected_b = static_cast<int>((src_b * src_a + dst_b * (1.0f - src_a)) * 255.0f + 0.5f);
        printf("[d3d9-pipeline-cache-rs-test] sub-pass 2 expected (analytic blend) B=%02X G=%02X R=%02X\n",
               expected_b, expected_g, expected_r);
        failures += check_surface_uniform(rt0, "sub-pass-2 (blend enabled)", expected_b, expected_g, expected_r, 3);
    }

    release_all(vb, ib, rt0, vs, ps, dev, d3d);

    printf("[d3d9-pipeline-cache-rs-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
