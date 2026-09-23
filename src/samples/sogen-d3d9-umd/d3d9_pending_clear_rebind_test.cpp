// Pending-clear / render-target-rebind ordering test.
//
// d3d9_host.cpp defers a Clear(D3DCLEAR_TARGET) that arrives with no dynamic-rendering instance open:
// instead of recording a vkCmdClearColorImage right away it parks the colour on pending_clear_ and lets
// whichever render-pass instance opens next consume it as a free LOAD_OP_CLEAR. The value is therefore
// realized arbitrarily later than the Clear() call that produced it -- at the next draw's fresh
// instance, at the next Clear() on the same batch, or at submit_batch_async if the batch closes with
// neither having happened.
//
// The D3D9 render-target-slot setters used to drain the whole GPU on every rebind, which made that
// deferral invisible: nothing could get between a Clear and its realization. Now they only update state
// (slot 0 and depth-stencil rebinds rotate the batch on the next draw, an MRT slot 1-3 rebind just
// reopens a render-pass instance inside the same batch), so a rebind CAN land in that window. If the
// realization sites resolved the clear against the live bindings rather than the ones the Clear() named,
// the colour would be painted onto whatever surface happens to be bound at realization time.
//
// Each sub-pass below rebinds MRT slot 1 while a colour Clear is still deferred, and exercises one of
// the three realization sites:
//
//   1. batch close   -- Clear{A,B}, rebind slot 1 to C, read back. B must take the clear, C must not.
//   2. draw fold     -- Clear{A,B}, rebind slot 1 to C, draw. B must take the clear; C must keep its
//                       prior contents outside the drawn quad rather than the clear colour.
//   3. clear overlap -- Clear{A,B}, rebind slot 1 to C, Clear{A,C}. Both clears must land on their own
//                       targets; the first must not be silently dropped by the second.
//
// Sub-passes 2 and 3 are the ones that discriminate: resolving the pending clear against the live
// bindings instead of a snapshot fails four of their checks (measured -- B keeps its pre-Clear contents
// in both, and C wrongly takes the clear colour in 2). Sub-pass 1 does not, because the read-back's
// flush reaches the host before the rebind command does; it is kept as an ordering-independent
// assertion of the same invariant, not as a discriminator.
//
// Sub-pass 2 needs the same real vs_2_0/ps_2_0 pair as d3d9_mrt_test.cpp: execute_draw only takes the
// programmable path (the one that writes more than RT slot 0) when both a vertex and a pixel shader are
// bound, and MRT output needs a PS that actually declares oC0/oC1. The bound-RT COUNT and format stay
// constant (two X8R8G8B8 slots) for the whole test, per the pipeline-cache caveat d3d9_mrt_test.cpp
// documents -- only WHICH surface sits in slot 1 ever changes, which is the whole point here.

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

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

    // pfnCreateResource backs every resource with a 640x480 surface regardless of the requested size
    // (KNOWN LIMITATION in sogen_d3d9_umd.cpp), so everything here is sized to match.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    struct Color
    {
        const char* name;
        int b, g, r;
    };

    constexpr Color kBlack{"BLACK", 0, 0, 0};
    constexpr Color kGreen{"GREEN", 0, 255, 0};
    constexpr Color kWhite{"WHITE", 255, 255, 255};
    constexpr Color kRed{"RED", 0, 0, 255};
    constexpr Color kBlue{"BLUE", 255, 0, 0};
    constexpr Color kMagenta{"MAGENTA", 255, 0, 255};
    constexpr Color kCyan{"CYAN", 255, 255, 0};

    D3DCOLOR argb(const Color& c)
    {
        return D3DCOLOR_ARGB(255, c.r, c.g, c.b);
    }

    bool channel_close(const unsigned char actual, const int expected)
    {
        return std::abs(static_cast<int>(actual) - expected) <= 2;
    }

    // Reads one pixel back and compares it against `expected`. col/row is always picked so it lies
    // OUTSIDE sub-pass 2's centred quad, i.e. it only ever observes clear/fill results.
    int check_pixel(IDirect3DSurface9* surf, const char* surf_name, const char* pass_name, const int col, const int row,
                    const Color& expected)
    {
        D3DLOCKED_RECT lr{};
        const HRESULT hlr = surf->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-pending-clear-rebind] FAIL: %s %s LockRect hr=0x%08lx\n", pass_name, surf_name, static_cast<unsigned long>(hlr));
            return 1;
        }

        const auto* p = static_cast<const unsigned char*>(lr.pBits) + row * (kCanvasWidth * 4) + col * 4;
        const bool ok = channel_close(p[0], expected.b) && channel_close(p[1], expected.g) && channel_close(p[2], expected.r);
        printf("[d3d9-pending-clear-rebind] %s: %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X (expected %s: B=%02X G=%02X R=%02X)\n",
               ok ? "PASS" : "FAIL", pass_name, surf_name, col, row, p[0], p[1], p[2], expected.name, expected.b, expected.g, expected.r);
        surf->UnlockRect();
        return ok ? 0 : 1;
    }

    IDirect3DSurface9* create_rt(IDirect3DDevice9* dev, const char* name)
    {
        // X8R8G8B8, not A8R8G8B8: only X8R8G8B8 carries FMT_OP_OFFSCREEN_RENDERTARGET in the UMD's
        // format table, so an A8R8G8B8 render target is refused by the real runtime before it ever
        // reaches this driver (see d3d9_mrt_test.cpp's own note).
        IDirect3DSurface9* surf = nullptr;
        const HRESULT hr =
            dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &surf, nullptr);
        printf("[d3d9-pending-clear-rebind] CreateRenderTarget(%s) hr=0x%08lx surf=%p\n", name, static_cast<unsigned long>(hr),
               static_cast<void*>(surf));
        return FAILED(hr) ? nullptr : surf;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-pending-clear-rebind] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9pendingclearrebind";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "pending-clear-rebind", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-pending-clear-rebind] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-pending-clear-rebind] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                              &vs_blob, nullptr);
    HRESULT hpsc =
        D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &ps_blob, nullptr);
    printf("[d3d9-pending-clear-rebind] D3DCompile vs=0x%08lx ps=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        printf("[d3d9-pending-clear-rebind] FAIL: shader compilation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    if (!vs || !ps)
    {
        printf("[d3d9-pending-clear-rebind] FAIL: shader creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt_a = create_rt(dev, "A");
    IDirect3DSurface9* rt_b = create_rt(dev, "B");
    IDirect3DSurface9* rt_c = create_rt(dev, "C");
    if (!rt_a || !rt_b || !rt_c)
    {
        printf("[d3d9-pending-clear-rebind] FAIL: render target creation failed\n");
        return 1;
    }

    // Centred quad, deliberately smaller than the canvas so every check point below (10,10) sits
    // outside it and only ever observes clear/fill results, never the draw's own output.
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    if (!vb || !ib)
    {
        printf("[d3d9-pending-clear-rebind] FAIL: geometry creation failed\n");
        return 1;
    }
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            const DWORD white = D3DCOLOR_XRGB(255, 255, 255);
            v[0] = {-0.5f, -0.5f, 0.5f, white};
            v[1] = {0.5f, -0.5f, 0.5f, white};
            v[2] = {0.5f, 0.5f, 0.5f, white};
            v[3] = {-0.5f, 0.5f, 0.5f, white};
            vb->Unlock();
        }
        WORD* idx = nullptr;
        ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0);
        if (idx)
        {
            const WORD quad[6] = {0, 1, 2, 0, 2, 3};
            std::memcpy(idx, quad, sizeof(quad));
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

    // Distinct starting contents so "took the clear" and "kept what it had" are always distinguishable.
    // ColorFill drains the batch, so each sub-pass starts from a known, fully settled state.
    auto prime = [&] {
        dev->ColorFill(rt_a, nullptr, argb(kBlack));
        dev->ColorFill(rt_b, nullptr, argb(kGreen));
        dev->ColorFill(rt_c, nullptr, argb(kWhite));
    };

    // Sub-pass 1: the clear is realized by the batch closing (the read-back's own flush), with no draw
    // and no second clear in between.
    {
        prime();
        dev->SetRenderTarget(0, rt_a);
        dev->SetRenderTarget(1, rt_b);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, argb(kRed), 1.0f, 0);
        dev->SetRenderTarget(1, rt_c);
        dev->EndScene();

        failures += check_pixel(rt_a, "A", "batch-close", 10, 10, kRed);
        failures += check_pixel(rt_b, "B(cleared, then unbound)", "batch-close", 10, 10, kRed);
        failures += check_pixel(rt_c, "C(bound after the Clear)", "batch-close", 10, 10, kWhite);
    }

    // Sub-pass 2: the clear would be folded into the next draw's fresh render-pass instance as a
    // LOAD_OP_CLEAR -- but that instance renders into {A,C}, not the {A,B} the Clear named.
    {
        prime();
        dev->SetRenderTarget(0, rt_a);
        dev->SetRenderTarget(1, rt_b);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, argb(kBlue), 1.0f, 0);
        dev->SetRenderTarget(1, rt_c);
        const HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-pending-clear-rebind] draw-fold DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += check_pixel(rt_a, "A", "draw-fold", 10, 10, kBlue);
        failures += check_pixel(rt_b, "B(cleared, then unbound)", "draw-fold", 10, 10, kBlue);
        failures += check_pixel(rt_c, "C(bound after the Clear)", "draw-fold", 10, 10, kWhite);
    }

    // Sub-pass 3: a second Clear arrives while the first is still deferred against a different set --
    // the first must be realized on its own targets, not overwritten and dropped.
    {
        prime();
        dev->SetRenderTarget(0, rt_a);
        dev->SetRenderTarget(1, rt_b);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, argb(kMagenta), 1.0f, 0);
        dev->SetRenderTarget(1, rt_c);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, argb(kCyan), 1.0f, 0);
        dev->EndScene();

        failures += check_pixel(rt_a, "A", "clear-overlap", 10, 10, kCyan);
        failures += check_pixel(rt_b, "B(first Clear's target)", "clear-overlap", 10, 10, kMagenta);
        failures += check_pixel(rt_c, "C(second Clear's target)", "clear-overlap", 10, 10, kCyan);
    }

    dev->SetRenderTarget(1, nullptr);
    ib->Release();
    vb->Release();
    rt_c->Release();
    rt_b->Release();
    rt_a->Release();
    ps->Release();
    vs->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-pending-clear-rebind] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
