// D3D9-over-Vulkan Flush/GPU-sync test.
//
// Exercises the one guarantee ioctl_d3d9_flush exists to provide: when the UMD issues a GPU sync,
// every draw the host has already recorded must have FINISHED on the GPU before the guest's next
// instruction runs. Nothing else in the D3D9 test set covers it -- the ordering tests all check that
// the host observes commands in the right ORDER, which stays true even if the flush never waits at
// all. Losing the wait is invisible to every one of them, and invisible in normal gameplay right up
// until a frame renders from vertices the CPU had already overwritten.
//
// This is the gate for any future change to how that wait is implemented. The flush currently blocks
// in vkWaitForFences with the emulator kernel lock held, which docs/multi-vcpu-design.md section 7.2
// forbids; the obvious repair is to park the guest thread on a fence poll instead. A park that is
// skipped, or whose predicate reports completion early, keeps every existing test green and only
// shows up here.
//
// The trigger is a plain Lock() (no D3DLOCK_DISCARD / D3DLOCK_NOOVERWRITE) on a
// D3DPOOL_DEFAULT + D3DUSAGE_DYNAMIC vertex buffer. That combination is the UMD's direct-mapped
// ring-buffer path, where the GPU reads the guest's bytes straight out of host-visible memory with no
// intervening copy, and an unqualified lock intent is defined as "wait for the GPU" -- exactly the
// sync_gpu() call site this test is aimed at (sogen_d3d9_umd.cpp, classify_lock_intent).
//
// Per round: draw the whole vertex batch kDrawsPerRound times in colour A, then Lock(0) the SAME
// vertex buffer and overwrite it with colour B, then read the render target back. Nothing is ever
// drawn with colour B. If the sync is real, every one of those draws consumed colour A and the canvas
// is uniformly A. If the guest resumed early, the draws still queued on the GPU fetch the overwritten
// vertices instead and the canvas comes back colour B -- the same silent, load-dependent corruption a
// missing GPU wait causes on real hardware.
//
// Discrimination verified rather than assumed: with flush_pending() cut down to submit_batch_async()
// alone (submit, never wait) the test fails 3/3 on round 0, and passes 4/4 unmodified.
//
// Structure follows d3d9_lock_flush_ordering_test.cpp's proven draw/readback shape (XYZRHW quads via
// an index buffer, CreateRenderTarget + LockRect readback) so the test stays isolated to the sync
// guarantee instead of re-deriving basic rendering correctness other tests already cover.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // The workload has to keep the GPU reading VERTICES, not just shading, for as long as possible.
    // Apple's tile-based GPU runs the whole render pass's vertex/tiling phase up front, so a handful of
    // full-canvas quads has its vertex data consumed within microseconds of submit no matter how much
    // fragment work follows -- overwriting it afterwards then changes nothing. A large index-driven
    // batch redrawn many times keeps real vertex fetch in flight instead.
    constexpr int kQuadsPerDraw = 8192;
    constexpr int kVertsPerDraw = kQuadsPerDraw * 4;
    constexpr int kIndicesPerDraw = kQuadsPerDraw * 6;
    constexpr int kDrawsPerRound = 120;
    constexpr int kRounds = 3;

    struct FvfVertex
    {
        float x, y, z, rhw;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

    // Every quad covers the whole canvas, so the frame reads uniformly as the colour the vertices
    // carried, whichever quad happened to win -- the check is "which data did the GPU fetch", not
    // "which quad landed on top".
    void fill_quads(FvfVertex* v, const DWORD color)
    {
        for (int q = 0; q < kQuadsPerDraw; ++q)
        {
            FvfVertex* p = v + q * 4;
            p[0] = {0.0f, 0.0f, 0.5f, 1.0f, color};
            p[1] = {static_cast<float>(kCanvasWidth), 0.0f, 0.5f, 1.0f, color};
            p[2] = {static_cast<float>(kCanvasWidth), static_cast<float>(kCanvasHeight), 0.5f, 1.0f, color};
            p[3] = {0.0f, static_cast<float>(kCanvasHeight), 0.5f, 1.0f, color};
        }
    }
}

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-gpu-sync] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9gpusync";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "gpu-sync", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth, kCanvasHeight,
                                nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-gpu-sync] FAIL: Direct3DCreate9 returned null\n");
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
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-gpu-sync] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-gpu-sync] FAIL: CreateRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // D3DPOOL_DEFAULT + D3DUSAGE_DYNAMIC is exactly the host's direct-mapped-buffer condition
    // (d3d9_host.cpp, eligible_for_direct_buffer), which is what routes Lock() through the UMD's
    // sync_gpu() path this test needs.
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(kVertsPerDraw * sizeof(FvfVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, kFvf, D3DPOOL_DEFAULT,
                                           &vb, nullptr);
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-gpu-sync] FAIL: CreateVertexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcvb));
        rt->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hcib = dev->CreateIndexBuffer(kIndicesPerDraw * sizeof(DWORD), 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib, nullptr);
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-gpu-sync] FAIL: CreateIndexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcib));
        vb->Release();
        rt->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }
    {
        DWORD* idx = nullptr;
        ib->Lock(0, kIndicesPerDraw * sizeof(DWORD), reinterpret_cast<void**>(&idx), 0);
        if (idx)
        {
            for (int q = 0; q < kQuadsPerDraw; ++q)
            {
                const DWORD base = static_cast<DWORD>(q) * 4;
                DWORD* t = idx + q * 6;
                t[0] = base;
                t[1] = base + 1;
                t[2] = base + 2;
                t[3] = base;
                t[4] = base + 2;
                t[5] = base + 3;
            }
            ib->Unlock();
        }
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(FvfVertex));
    dev->SetIndices(ib);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    // Colour A is what every draw must render; colour B is only ever written into the vertex buffer
    // after the sync, never drawn. Each round uses a different pair so a round cannot accidentally pass
    // on the previous round's leftover pixels.
    struct round_colors
    {
        DWORD a;
        DWORD b;
        const char* a_name;
    };

    constexpr round_colors kColors[kRounds] = {
        {D3DCOLOR_XRGB(255, 0, 0), D3DCOLOR_XRGB(0, 255, 0), "RED"},
        {D3DCOLOR_XRGB(0, 0, 255), D3DCOLOR_XRGB(255, 255, 0), "BLUE"},
        {D3DCOLOR_XRGB(0, 255, 255), D3DCOLOR_XRGB(255, 0, 255), "CYAN"},
    };

    bool all_pass = true;
    for (int round = 0; round < kRounds; ++round)
    {
        const round_colors& colors = kColors[round];

        FvfVertex* verts_a = nullptr;
        if (FAILED(vb->Lock(0, kVertsPerDraw * sizeof(FvfVertex), reinterpret_cast<void**>(&verts_a), D3DLOCK_DISCARD)) || !verts_a)
        {
            printf("[d3d9-gpu-sync] FAIL: round %d content-A Lock failed\n", round);
            all_pass = false;
            break;
        }
        fill_quads(verts_a, colors.a);
        vb->Unlock();

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        for (int i = 0; i < kDrawsPerRound; ++i)
        {
            dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, kVertsPerDraw, 0, kQuadsPerDraw * 2);
        }

        // The sync. An unqualified lock intent on a direct-mapped dynamic buffer means "the GPU is done
        // with these bytes", so the UMD issues ioctl_d3d9_flush here and must not return until the draws
        // above have completed. The overwrite that follows is only legal because of that promise.
        FvfVertex* verts_b = nullptr;
        const HRESULT hlock = vb->Lock(0, kVertsPerDraw * sizeof(FvfVertex), reinterpret_cast<void**>(&verts_b), 0);
        if (FAILED(hlock) || !verts_b)
        {
            printf("[d3d9-gpu-sync] FAIL: round %d sync Lock hr=0x%08lx\n", round, static_cast<unsigned long>(hlock));
            dev->EndScene();
            all_pass = false;
            break;
        }
        fill_quads(verts_b, colors.b);
        vb->Unlock();

        dev->EndScene();

        D3DLOCKED_RECT lr{};
        const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-gpu-sync] FAIL: round %d LockRect hr=0x%08lx\n", round, static_cast<unsigned long>(hlr));
            all_pass = false;
            break;
        }

        const unsigned char expect_b = static_cast<unsigned char>(colors.a & 0xFF);
        const unsigned char expect_g = static_cast<unsigned char>((colors.a >> 8) & 0xFF);
        const unsigned char expect_r = static_cast<unsigned char>((colors.a >> 16) & 0xFF);

        // Sample a spread of the canvas rather than one pixel: a partially-corrupted frame (only the
        // draws still queued at sync time picked up colour B) leaves correct pixels behind, and a
        // single-pixel check could land on one of them.
        constexpr int kSampleX[5] = {8, 160, 320, 480, 631};
        constexpr int kSampleY[5] = {8, 120, 240, 360, 471};
        bool round_pass = true;
        for (int sy = 0; sy < 5 && round_pass; ++sy)
        {
            for (int sx = 0; sx < 5 && round_pass; ++sx)
            {
                const auto* row = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(kSampleY[sy]) * lr.Pitch;
                const unsigned char b = row[kSampleX[sx] * 4 + 0];
                const unsigned char g = row[kSampleX[sx] * 4 + 1];
                const unsigned char r = row[kSampleX[sx] * 4 + 2];
                if (b != expect_b || g != expect_g || r != expect_r)
                {
                    printf("[d3d9-gpu-sync] FAIL: round %d pixel(%d,%d)=B=%02X G=%02X R=%02X, expected %s B=%02X G=%02X R=%02X "
                           "-- a draw read vertices written after the GPU sync\n",
                           round, kSampleX[sx], kSampleY[sy], b, g, r, colors.a_name, expect_b, expect_g, expect_r);
                    round_pass = false;
                }
            }
        }
        rt->UnlockRect();

        if (round_pass)
        {
            printf("[d3d9-gpu-sync] PASS: round %d, all %d draws rendered %s -- the GPU sync really waited\n", round, kDrawsPerRound,
                   colors.a_name);
        }
        else
        {
            all_pass = false;
            break;
        }
    }

    if (all_pass)
    {
        printf("[d3d9-gpu-sync] ALL CHECKS PASSED\n");
    }

    ib->Release();
    vb->Release();
    rt->Release();
    dev->Release();
    d3d->Release();
    return all_pass ? 0 : 1;
}
