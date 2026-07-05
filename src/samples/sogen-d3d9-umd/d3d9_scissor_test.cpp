// D3DRS_SCISSORTESTENABLE + SetScissorRect regression test (Task 2 of
// .claude/plans/ scissor-rect plan; see Task 1's commit c517c685 for the host-side change this
// proves). Before Task 1, execute_draw unconditionally forced a full-render-target-extent scissor
// on every draw regardless of app state, so SetScissorRect was silently a no-op; this test's own
// discriminator check (see below) is specifically designed to FAIL under that old behavior.
//
// Fixed-function D3DFVF_XYZRHW|D3DFVF_DIFFUSE path only -- no shader needed, matching
// d3d9_triangle_test.cpp's established pattern for screen-space pre-transformed geometry.
//
// Sub-pass 1 (scissor enabled): clear a 640x480 off-screen render target BLUE (canvas sized to match
// pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION, same as d3d9_texture_test.cpp), enable
// D3DRS_SCISSORTESTENABLE, set the scissor rect to the center third {213,160,427,320}, then draw a
// FULL-SCREEN RED quad. Real scissor clipping must leave the center RED (inside the rect) but the
// corners still BLUE (outside it) -- the old, pre-Task-1 code would paint the entire RT red,
// failing the corner checks.
//
// Sub-pass 2 (scissor disabled, regression safety): re-clear the same RT BLUE, disable
// D3DRS_SCISSORTESTENABLE, redraw the identical full-screen RED quad. Every checkpoint (center +
// corners) must now be RED, proving the default/no-scissor path still fills the whole RT.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    struct FvfVertex
    {
        float x, y, z, rhw;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) -- every
    // resource this UMD creates is actually backed by a 640x480 surface regardless of the size the app
    // requests, so the canvas here is sized to match rather than hit that gap.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Public D3DRENDERSTATETYPE value (d3d9types.h) -- not otherwise declared by this test's headers.
    constexpr D3DRENDERSTATETYPE kD3DRS_SCISSORTESTENABLE = static_cast<D3DRENDERSTATETYPE>(174);

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    struct Check
    {
        const char* name;
        int col, row;
        int expected_b, expected_g, expected_r;
    };

    constexpr int kRedB = 0, kRedG = 0, kRedR = 255;
    constexpr int kBlueB = 255, kBlueG = 0, kBlueR = 0;

    // Center (320,240) is inside the {213,160,427,320} scissor rect (the canvas's center third); the
    // two corners near (10,10) and (630,470) are outside it.
    const Check kScissorEnabledChecks[3] = {
        {"center (inside scissor rect)", 320, 240, kRedB, kRedG, kRedR},
        {"corner near (10,10) (outside scissor rect)", 10, 10, kBlueB, kBlueG, kBlueR},
        {"corner near (630,470) (outside scissor rect)", 630, 470, kBlueB, kBlueG, kBlueR},
    };

    const Check kScissorDisabledChecks[3] = {
        {"center", 320, 240, kRedB, kRedG, kRedR},
        {"corner (10,10)", 10, 10, kRedB, kRedG, kRedR},
        {"corner (630,470)", 630, 470, kRedB, kRedG, kRedR},
    };

    int run_checks(IDirect3DSurface9* rt, const char* pass_name, const Check* checks, const int check_count)
    {
        int failures = 0;
        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-scissor-test] %s LockRect hr=0x%08lx pBits=%p\n", pass_name, static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-scissor-test] FAIL: %s LockRect failed\n", pass_name);
            return check_count;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        for (int i = 0; i < check_count; ++i)
        {
            const Check& c = checks[i];
            const unsigned char* p = pixel_at(c.col, c.row);
            printf("[d3d9-scissor-test] %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name, c.name, c.col, c.row,
                   p[0], p[1], p[2], p[3]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) ||
                !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-scissor-test] FAIL: %s %s does not match the expected color\n", pass_name, c.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-scissor-test] PASS: %s %s matches the expected color\n", pass_name, c.name);
            }
        }

        rt->UnlockRect();
        return failures;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-scissor-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9scissortest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "scissor-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-scissor-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-scissor-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-scissor-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-scissor-test] FAIL: render target creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // Full-canvas RED quad, screen-space pre-transformed (XYZRHW), so no scissor-independent NDC
    // transform is involved -- the quad geometrically covers every pixel from (0,0) to
    // (kCanvasWidth,kCanvasHeight) regardless of scissor state.
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(4 * sizeof(FvfVertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-scissor-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb), static_cast<void*>(vb));
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-scissor-test] FAIL: vertex buffer creation failed\n");
        rt->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }
    {
        FvfVertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(FvfVertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            const DWORD red = D3DCOLOR_XRGB(255, 0, 0);
            v[0] = {0.0f, 0.0f, 0.5f, 1.0f, red};
            v[1] = {static_cast<float>(kCanvasWidth), 0.0f, 0.5f, 1.0f, red};
            v[2] = {static_cast<float>(kCanvasWidth), static_cast<float>(kCanvasHeight), 0.5f, 1.0f, red};
            v[3] = {0.0f, static_cast<float>(kCanvasHeight), 0.5f, 1.0f, red};
            vb->Unlock();
        }
    }

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hcib = dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    printf("[d3d9-scissor-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib), static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-scissor-test] FAIL: index buffer creation failed\n");
        vb->Release();
        rt->Release();
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
    dev->SetStreamSource(0, vb, 0, sizeof(FvfVertex));
    dev->SetIndices(ib);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    int failures = 0;

    // Sub-pass 1: D3DRS_SCISSORTESTENABLE=TRUE, scissor rect covers the center third of the RT.
    {
        RECT scissor_rect{213, 160, 427, 320};
        HRESULT hsst = dev->SetRenderState(kD3DRS_SCISSORTESTENABLE, TRUE);
        printf("[d3d9-scissor-test] SetRenderState(SCISSORTESTENABLE, TRUE) hr=0x%08lx\n", static_cast<unsigned long>(hsst));
        HRESULT hsr = dev->SetScissorRect(&scissor_rect);
        printf("[d3d9-scissor-test] SetScissorRect({213,160,427,320}) hr=0x%08lx\n", static_cast<unsigned long>(hsr));

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-scissor-test] DrawIndexedPrimitive(scissor-enabled) hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += run_checks(rt, "scissor-enabled", kScissorEnabledChecks, 3);
    }

    // Sub-pass 2: D3DRS_SCISSORTESTENABLE=FALSE (regression safety) -- the same full-screen quad must
    // fill the whole RT again, exactly like every other guest test's default (no-scissor) behavior.
    {
        HRESULT hsst = dev->SetRenderState(kD3DRS_SCISSORTESTENABLE, FALSE);
        printf("[d3d9-scissor-test] SetRenderState(SCISSORTESTENABLE, FALSE) hr=0x%08lx\n", static_cast<unsigned long>(hsst));

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        printf("[d3d9-scissor-test] DrawIndexedPrimitive(scissor-disabled) hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += run_checks(rt, "scissor-disabled", kScissorDisabledChecks, 3);
    }

    ib->Release();
    vb->Release();
    rt->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-scissor-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
