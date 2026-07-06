// Pipeline-cache-key regression test (commits 3809d1c8/5dd05caa: "key pipeline caches by full
// RT/depth/vertex shape, not just VS/PS"). Before that fix, ensure_programmable_pipeline's VkPipeline
// cache key was ONLY `(vertex_shader<<32)|pixel_shader` -- it ignored the bound render-target
// COUNT/formats and depth format, even though those are ALSO baked into the built pipeline
// (VkPipelineRenderingCreateInfo::pColorAttachmentFormats/colorAttachmentCount, see
// create_graphics_pipeline in vulkan_host.cpp). So a real app that draws once with 1 bound RT using a
// given VS/PS pair, then rebinds to 2 bound RTs using the SAME VS/PS pair, would get back the STALE
// 1-color-attachment pipeline from the first draw's cache entry and use it against a 2-attachment
// vkCmdBeginRendering scope -- a real attachment-count mismatch between the pipeline object and the
// render pass it's used in.
//
// Discriminator (must fail under the OLD key, pass under the fixed key): compile ONE vs_2_0/ps_2_0
// pair -- a trivial NDC-passthrough VS (D3DFVF_XYZ|D3DFVF_DIFFUSE, the 16-byte fixed-function-fallback
// shape ensure_programmable_pipeline already recognizes, same as d3d9_mrt_test.cpp/d3d9_multistream_
// test.cpp -- deliberately not exercising the vertex-decl parser here, this test is only about the
// RT-shape cache-key dimension) and a PS that writes a single, distinctive solid color (RED) to COLOR0
// ONLY -- no COLOR1 output, unlike d3d9_mrt_test.cpp's 2-output PS.
//
//   Sub-pass 1: RT0 bound alone (SetRenderTarget(0, RT0), no slot 1, no depth). Clear RT0 BLUE, draw
//   the full-screen quad with the VS/PS pair. This builds (and, under the fix, caches) a 1-color-
//   attachment pipeline for this exact VS/PS pair. RT0 must read back RED.
//
//   Sub-pass 2 (the actual discriminator): rebind to BOTH RT0 (slot 0) and a new RT1 (slot 1) -- i.e.
//   SAME VS/PS pair (the exact same IDirect3DVertexShader9*/IDirect3DPixelShader9*, never recreated),
//   DIFFERENT bound-RT shape (1 color attachment -> 2). Clear both BLUE, draw the same quad again.
//   Under the OLD bug, ensure_programmable_pipeline's key collapses to the same
//   (vertex_shader<<32)|pixel_shader value as sub-pass 1, so this draw would reuse sub-pass 1's STALE
//   1-attachment VkPipeline against a 2-attachment dynamic-rendering scope -- undefined/wrong behavior
//   for BOTH attachments, not just RT1 (this is why RT0-stays-RED-with-2-RTs-bound is this test's
//   primary, unambiguous check, not merely "RT1 has some particular color"). Under the fix, the
//   different color_formats array produces a genuinely distinct cache key, so a fresh, correctly-shaped
//   2-attachment pipeline gets built and used instead.
//
// What should RT1 read back as? Read from the actual code (create_graphics_pipeline in
// vulkan_host.cpp, translate_d3d9_shader_pair in d3d9_shader_translator.cpp): the fragment SPIR-V is
// compiled directly from the ps_2_0 tokens with no output-signature remapping applied to the PS side
// (see d3d9_shader_translator.cpp's own comment on why varying_map_info is deliberately not passed for
// the PS compile stage) -- since this PS only writes oC0, its compiled SPIR-V module declares exactly
// one fragment output, at Location 0. The pipeline is still built with a 2-entry color_formats array
// (create_graphics_pipeline builds one VkPipelineColorBlendAttachmentState per entry in color_formats,
// and VkPipelineRenderingCreateInfo::colorAttachmentCount is set to color_formats.size()
// unconditionally), so this is a pipeline with 2 color attachments whose fragment shader only writes
// one of them -- exactly the case Vulkan's fragment-output-interface matching is designed for: an
// attachment with no matching shader output location is simply never written by that draw call, so its
// contents are left exactly as they were before the draw (its BLUE clear color here), not filled with
// some fallback value. So RT1's correct, expected post-draw color under a correct/fixed
// implementation is BLUE (unchanged from its own Clear), not RED and not undefined garbage.

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

    const char* const k_pixel_shader_hlsl = R"(
float4 main() : COLOR0
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    // Matches d3d9_mrt_test.cpp/d3d9_multistream_test.cpp's established shape -- the one 16-byte
    // vertex layout ensure_programmable_pipeline already recognizes with no real vertex declaration
    // bound (see its own comment for why only this shape plus the 20-byte D3DFVF_XYZ|D3DFVF_TEX1 shape
    // are wired up without real vertex-decl parsing).
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) -- every
    // resource this UMD creates is actually backed by a 640x480 surface regardless of the size the app
    // requests, same approach as d3d9_mrt_test.cpp/d3d9_multistream_test.cpp.
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

    int check_surface_uniform(IDirect3DSurface9* surf, const char* surf_name, const char* pass_name,
                               const int expected_b, const int expected_g, const int expected_r)
    {
        int failures = 0;
        D3DLOCKED_RECT lr{};
        HRESULT hlr = surf->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-pipeline-cache-test] %s %s LockRect hr=0x%08lx pBits=%p\n", pass_name, surf_name,
               static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-pipeline-cache-test] FAIL: %s %s LockRect failed\n", pass_name, surf_name);
            return kCheckPointCount;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        for (const auto& pt : kCheckPoints)
        {
            const unsigned char* p = pixel_at(pt.col, pt.row);
            printf("[d3d9-pipeline-cache-test] %s %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name,
                   surf_name, pt.name, pt.col, pt.row, p[0], p[1], p[2], p[3]);
            if (!channel_close(p[0], expected_b, 2) || !channel_close(p[1], expected_g, 2) ||
                !channel_close(p[2], expected_r, 2))
            {
                printf("[d3d9-pipeline-cache-test] FAIL: %s %s %s does not match the expected color\n", pass_name,
                       surf_name, pt.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-pipeline-cache-test] PASS: %s %s %s matches the expected color\n", pass_name, surf_name,
                       pt.name);
            }
        }

        surf->UnlockRect();
        return failures;
    }

    void release_all(IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib, IDirect3DSurface9* rt0,
                     IDirect3DSurface9* rt1, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps,
                     IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (ib)
        {
            ib->Release();
        }
        if (vb)
        {
            vb->Release();
        }
        if (rt1)
        {
            rt1->Release();
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
    printf("[d3d9-pipeline-cache-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9pipelinecachetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "pipeline-cache-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-pipeline-cache-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-pipeline-cache-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
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
    printf("[d3d9-pipeline-cache-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-pipeline-cache-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-pipeline-cache-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-pipeline-cache-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-pipeline-cache-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-pipeline-cache-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // D3DFMT_X8R8G8B8, not D3DFMT_A8R8G8B8: the UMD's FORMATOP table only flags X8R8G8B8 with
    // FMT_OP_OFFSCREEN_RENDERTARGET (see d3d9_mrt_test.cpp's header comment for the full account).
    IDirect3DSurface9* rt0 = nullptr;
    HRESULT hcrt0 = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                            TRUE, &rt0, nullptr);
    printf("[d3d9-pipeline-cache-test] CreateRenderTarget(RT0) hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt0),
           static_cast<void*>(rt0));
    if (FAILED(hcrt0) || !rt0)
    {
        printf("[d3d9-pipeline-cache-test] FAIL: RT0 creation failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Full-screen quad in NDC (-1,-1)-(1,1) -- covers the whole viewport regardless of Y convention.
    // Vertex color is irrelevant (the PS ignores its input and writes a constant color) but
    // D3DFVF_DIFFUSE is part of the recognized 16-byte vertex layout, so it's populated anyway.
    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-pipeline-cache-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-pipeline-cache-test] FAIL: vertex buffer creation failed\n");
        release_all(nullptr, nullptr, rt0, nullptr, vs, ps, dev, d3d);
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
    printf("[d3d9-pipeline-cache-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib),
           static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-pipeline-cache-test] FAIL: index buffer creation failed\n");
        release_all(vb, nullptr, rt0, nullptr, vs, ps, dev, d3d);
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

    int failures = 0;

    // Sub-pass 1: RT0 bound ALONE (1 color attachment). Builds/caches a 1-attachment pipeline for this
    // exact VS/PS pair.
    dev->SetRenderTarget(0, rt0);
    {
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 255), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-pipeline-cache-test] sub-pass 1 DrawIndexedPrimitive hr=0x%08lx\n",
               static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += check_surface_uniform(rt0, "RT0", "sub-pass-1 (1 RT bound)", 0, 0, 255);
    }

    // Sub-pass 2 (the discriminator): create RT1, rebind to BOTH RT0 (slot 0) and RT1 (slot 1) -- SAME
    // vs/ps, DIFFERENT bound-RT-count/shape (1 -> 2) -- and draw again.
    IDirect3DSurface9* rt1 = nullptr;
    HRESULT hcrt1 = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                            TRUE, &rt1, nullptr);
    printf("[d3d9-pipeline-cache-test] CreateRenderTarget(RT1) hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt1),
           static_cast<void*>(rt1));
    if (FAILED(hcrt1) || !rt1)
    {
        printf("[d3d9-pipeline-cache-test] FAIL: RT1 creation failed\n");
        release_all(vb, ib, rt0, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    HRESULT hsrt0 = dev->SetRenderTarget(0, rt0);
    HRESULT hsrt1 = dev->SetRenderTarget(1, rt1);
    printf("[d3d9-pipeline-cache-test] SetRenderTarget(0,RT0) hr=0x%08lx SetRenderTarget(1,RT1) hr=0x%08lx\n",
           static_cast<unsigned long>(hsrt0), static_cast<unsigned long>(hsrt1));
    {
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 255), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-pipeline-cache-test] sub-pass 2 DrawIndexedPrimitive hr=0x%08lx\n",
               static_cast<unsigned long>(hd));
        dev->EndScene();

        // Primary, unambiguous discriminator: RT0 (the attachment the PS DOES write, oC0) must still be
        // RED with 2 RTs bound. The old bug's failure mode was a stale 1-attachment VkPipeline reused
        // against a 2-attachment rendering scope -- an attachment-count mismatch between the pipeline
        // object and the render pass, which can corrupt attachment 0's own output, not just leave
        // attachment 1 wrong.
        failures += check_surface_uniform(rt0, "RT0", "sub-pass-2 (2 RTs bound)", 0, 0, 255);

        // Secondary check: RT1 has no matching PS output (oC1 was never written -- see this file's
        // header comment on Vulkan's fragment-output-interface matching), so it must be left exactly as
        // its own Clear() left it: BLUE, unmodified by this draw.
        failures += check_surface_uniform(rt1, "RT1", "sub-pass-2 (2 RTs bound)", 255, 0, 0);
    }

    release_all(vb, ib, rt0, rt1, vs, ps, dev, d3d);

    printf("[d3d9-pipeline-cache-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
