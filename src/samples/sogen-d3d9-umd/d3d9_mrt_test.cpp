// Multiple-render-target (MRT) regression test (Task 5 of .claude/plans/jazzy-giggling-cloud.md;
// see Task 3's commits 527d5775/d82de78f and Task 4's commit 87548935 for the host-side changes this
// proves). Before Task 3, execute_draw only ever rendered into RT slot 0, silently ignoring any other
// bound render target; before Task 4, Clear(D3DCLEAR_TARGET) only ever cleared RT slot 0 too. Both
// checks below are designed to FAIL under that old, pre-Task-3/4 behavior.
//
// A real ps_2_0 pixel shader (D3D9's ps_2_0 ISA defines oC0-oC3 explicitly -- MRT output is not gated
// behind ps_3_0/SM3; it is gated behind D3DCAPS9::NumSimultaneousRTs, a device cap this UMD already
// reports as 4) writes two distinct constant colors to a struct return with COLOR0/COLOR1 semantics.
//
// This needs a REAL vertex shader too, not fixed-function: execute_draw (d3d9_host.cpp) only takes the
// programmable-pipeline path (ensure_programmable_pipeline, the one that actually knows about a bound
// PS) when BOTH state_.vertex_shader != 0 AND state_.pixel_shader != 0; with a fixed-function VS
// (vertex_shader == 0), execute_draw falls straight to the hardcoded one-RT-output fixed-function
// pipeline and the bound PS is silently never applied at all (confirmed empirically while building this
// test: a D3DFVF_XYZRHW/no-VS version of this test read back pure black on both RTs after the draw).
// So the VS here is a trivial NDC passthrough, matching d3d9_const_test.cpp/d3d9_int_bool_const_test.cpp's
// established D3DFVF_XYZ|D3DFVF_DIFFUSE (16-byte stride) shape, which ensure_programmable_pipeline
// already recognizes -- see its own comment for why only this one vertex layout (plus the 20-byte
// D3DFVF_XYZ|D3DFVF_TEX1 shape) is wired up (no real vertex-declaration parsing yet, Task 7/8).
//
// Per the cache-gap caveat in the task plan (ensure_pipeline/ensure_programmable_pipeline's pipeline
// cache does not key on RT-count/format), RT0+RT1 are bound ONCE at the very start and never rebound
// or changed for the rest of the test -- both sub-passes below reuse the exact same bound-RT shape.
//
// Sub-pass 1 (real draw): draw a full-screen quad with the 2-output PS. RT0 must come back entirely
// RED (oC0), RT1 must come back entirely GREEN (oC1). Discriminator: the old code only renders into
// RT0, so RT1 would retain its pre-draw (uninitialized/black) contents instead of GREEN.
//
// Sub-pass 2 (Clear, same RTs still bound, regression safety for Task 4): Clear(D3DCLEAR_TARGET,
// yellow) with no rect. Both RT0 and RT1 must now come back entirely YELLOW. Discriminator: the old
// code only clears RT0, so RT1 would stay GREEN (its sub-pass-1 draw color) instead of turning YELLOW.

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
struct PSOutput { float4 c0 : COLOR0; float4 c1 : COLOR1; };
PSOutput main()
{
    PSOutput o;
    o.c0 = float4(1.0, 0.0, 0.0, 1.0);
    o.c1 = float4(0.0, 1.0, 0.0, 1.0);
    return o;
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    // Matches d3d9_const_test.cpp/d3d9_int_bool_const_test.cpp's established shape -- the one 16-byte
    // vertex layout ensure_programmable_pipeline already recognizes (see this file's header comment).
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) -- every
    // resource this UMD creates is actually backed by a 640x480 surface regardless of the size the app
    // requests, so both render targets here are sized to match rather than hit that gap (same approach
    // as d3d9_scissor_test.cpp).
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
        printf("[d3d9-mrt-test] %s %s LockRect hr=0x%08lx pBits=%p\n", pass_name, surf_name,
               static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-mrt-test] FAIL: %s %s LockRect failed\n", pass_name, surf_name);
            return kCheckPointCount;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        for (const auto& pt : kCheckPoints)
        {
            const unsigned char* p = pixel_at(pt.col, pt.row);
            printf("[d3d9-mrt-test] %s %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name, surf_name,
                   pt.name, pt.col, pt.row, p[0], p[1], p[2], p[3]);
            if (!channel_close(p[0], expected_b, 2) || !channel_close(p[1], expected_g, 2) ||
                !channel_close(p[2], expected_r, 2))
            {
                printf("[d3d9-mrt-test] FAIL: %s %s %s does not match the expected color\n", pass_name, surf_name,
                       pt.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-mrt-test] PASS: %s %s %s matches the expected color\n", pass_name, surf_name,
                       pt.name);
            }
        }

        surf->UnlockRect();
        return failures;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-mrt-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9mrttest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "mrt-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                 kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-mrt-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-mrt-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-mrt-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-mrt-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
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
    printf("[d3d9-mrt-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-mrt-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-mrt-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-mrt-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
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

    // D3DFMT_X8R8G8B8, not D3DFMT_A8R8G8B8: the UMD's FORMATOP table (sogen_d3d9_umd.cpp's g_formats)
    // only flags X8R8G8B8 with FMT_OP_OFFSCREEN_RENDERTARGET -- A8R8G8B8 is flagged FMT_OP_TEXTURE only.
    // Requesting an A8R8G8B8 render target fails client-side in the real d3d9.dll runtime itself
    // (D3DERR_INVALIDCALL, confirmed live) before ever reaching this driver -- a deliberate, documented
    // "minimal format set" scoping choice (see docs/d3d9-roadmap.md), not a bug, and out of scope to
    // widen here (test-only task). The test's checks are RGB-only, so the missing real alpha channel is
    // irrelevant.
    IDirect3DSurface9* rt0 = nullptr;
    HRESULT hcrt0 =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt0, nullptr);
    printf("[d3d9-mrt-test] CreateRenderTarget(RT0) hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt0),
           static_cast<void*>(rt0));
    if (FAILED(hcrt0) || !rt0)
    {
        printf("[d3d9-mrt-test] FAIL: RT0 creation failed\n");
        vs->Release();
        ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt1 = nullptr;
    HRESULT hcrt1 =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt1, nullptr);
    printf("[d3d9-mrt-test] CreateRenderTarget(RT1) hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt1),
           static_cast<void*>(rt1));
    if (FAILED(hcrt1) || !rt1)
    {
        printf("[d3d9-mrt-test] FAIL: RT1 creation failed\n");
        rt0->Release();
        vs->Release();
        ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    // Bind BOTH render targets ONCE, here, and never change this binding for the rest of the test --
    // see this file's header comment on the pipeline-cache RT-shape caveat.
    HRESULT hsrt0 = dev->SetRenderTarget(0, rt0);
    HRESULT hsrt1 = dev->SetRenderTarget(1, rt1);
    printf("[d3d9-mrt-test] SetRenderTarget(0,RT0) hr=0x%08lx SetRenderTarget(1,RT1) hr=0x%08lx\n",
           static_cast<unsigned long>(hsrt0), static_cast<unsigned long>(hsrt1));

    // Full-screen quad in NDC (-1,-1)-(1,1) -- covers the whole viewport regardless of which Y
    // convention (flipped or not) this pipeline uses, since all four corners are at the extremes.
    // Vertex color is irrelevant (the PS ignores its input and writes constant colors) but D3DFVF_
    // DIFFUSE is part of the recognized 16-byte vertex layout, so it's populated anyway.
    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-mrt-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-mrt-test] FAIL: vertex buffer creation failed\n");
        rt1->Release();
        rt0->Release();
        vs->Release();
        ps->Release();
        dev->Release();
        d3d->Release();
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
    printf("[d3d9-mrt-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib),
           static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-mrt-test] FAIL: index buffer creation failed\n");
        vb->Release();
        rt1->Release();
        rt0->Release();
        vs->Release();
        ps->Release();
        dev->Release();
        d3d->Release();
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

    // Sub-pass 1: real draw with the 2-output PS -- RT0 must be RED, RT1 must be GREEN.
    {
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-mrt-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += check_surface_uniform(rt0, "RT0", "draw", 0, 0, 255);
        failures += check_surface_uniform(rt1, "RT1", "draw", 0, 255, 0);
    }

    // Sub-pass 2: Clear(D3DCLEAR_TARGET, yellow) with RT0+RT1 STILL bound, unchanged -- both must now
    // be entirely YELLOW.
    {
        dev->BeginScene();
        HRESULT hcl = dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 255, 255, 0), 1.0f, 0);
        printf("[d3d9-mrt-test] Clear(yellow) hr=0x%08lx\n", static_cast<unsigned long>(hcl));
        dev->EndScene();

        failures += check_surface_uniform(rt0, "RT0", "clear", 0, 255, 255);
        failures += check_surface_uniform(rt1, "RT1", "clear", 0, 255, 255);
    }

    ib->Release();
    vb->Release();
    rt1->Release();
    rt0->Release();
    vs->Release();
    ps->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-mrt-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
