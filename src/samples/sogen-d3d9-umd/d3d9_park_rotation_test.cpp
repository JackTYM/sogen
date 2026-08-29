// D3D9-over-Vulkan batch-slot rotation test.
//
// Exercises the one path where a recorded draw has to wait for the GPU before it can record
// anything: execute_draw's batch-management step rotates to the other batch slot on a render-target
// change, and that slot's previous submission may still be executing. The host answers that wait by
// parking the guest thread (recorded_command_would_block + await_host_condition) instead of blocking
// in vkWaitForFences with the emulator kernel lock held, which docs/multi-vcpu-design.md section 7.2
// forbids. This is the gate for that park -- and, since it also passes against the older blocking
// wait, for whatever else may replace it.
//
// A park is only correct if the draw it stopped in front of is re-issued exactly once, against
// whatever state is live when the thread wakes. The three ways to get that wrong -- dropping the
// blocked draw, replaying records the escape had already executed, and letting a draw land on a
// render target other than the one it was recorded against -- all leave every other D3D9 test green,
// because those tests draw one colour repeatedly and cannot tell one draw from twelve.
//
// So this counts draws in the framebuffer instead of checking a colour. Additive ONE/ONE blending
// plus a single visible quad per draw carrying an increment of 1 makes the final channel value the
// exact number of draws that reached that target: a dropped draw reads back one low, a replayed
// record reads back high, and a draw that landed on the wrong target shows up in that target's OTHER
// channel, which must stay zero.
//
// Additive blending is also what keeps the workload GPU-bound: a tile-based GPU discards identical
// opaque full-canvas quads as hidden surfaces and then retires every batch long before the next
// rotation asks for its slot back. The remaining quads in each draw carry colour zero -- they cost a
// full-canvas blend each, which is what holds the GPU behind the CPU, while adding nothing to the
// count.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Quads per draw. Only quad 0 carries the increment; the rest exist purely as GPU fill work.
    constexpr int kQuadsPerDraw = 1024;
    constexpr int kVertsPerDraw = kQuadsPerDraw * 4;
    constexpr int kIndicesPerDraw = kQuadsPerDraw * 6;

    // Render-target switches per round; they alternate, so each target sees half of them.
    constexpr int kSwitchesPerRound = 40;
    constexpr int kRounds = 4;
    // Draws recorded between two switches, per round. Varying it per round means a round cannot pass
    // on the previous round's readback, and keeps every expected total inside an 8-bit channel.
    constexpr int kDrawsPerTarget[kRounds] = {8, 9, 10, 11};

    struct FvfVertex
    {
        float x, y, z, rhw;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

    void fill_quads(FvfVertex* v, const DWORD increment)
    {
        for (int q = 0; q < kQuadsPerDraw; ++q)
        {
            const DWORD color = q == 0 ? increment : 0u;
            FvfVertex* p = v + q * 4;
            p[0] = {0.0f, 0.0f, 0.5f, 1.0f, color};
            p[1] = {static_cast<float>(kCanvasWidth), 0.0f, 0.5f, 1.0f, color};
            p[2] = {static_cast<float>(kCanvasWidth), static_cast<float>(kCanvasHeight), 0.5f, 1.0f, color};
            p[3] = {0.0f, static_cast<float>(kCanvasHeight), 0.5f, 1.0f, color};
        }
    }

    bool fill_vertex_buffer(IDirect3DVertexBuffer9* vb, const DWORD increment)
    {
        FvfVertex* verts = nullptr;
        if (FAILED(vb->Lock(0, kVertsPerDraw * sizeof(FvfVertex), reinterpret_cast<void**>(&verts), D3DLOCK_DISCARD)) || !verts)
        {
            return false;
        }
        fill_quads(verts, increment);
        vb->Unlock();
        return true;
    }

    // expected_r / expected_b are draw counts, not colours: target A accumulates red and target B
    // blue, so the channel a target does not own must read back zero.
    bool check_target(IDirect3DSurface9* rt, const int expected_r, const int expected_b, const char* label, const int round)
    {
        D3DLOCKED_RECT lr{};
        const HRESULT hr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !lr.pBits)
        {
            printf("[d3d9-park-rotation] FAIL: round %d target %s LockRect hr=0x%08lx\n", round, label, static_cast<unsigned long>(hr));
            return false;
        }

        constexpr int kSampleX[5] = {8, 160, 320, 480, 631};
        constexpr int kSampleY[5] = {8, 120, 240, 360, 471};
        bool ok = true;
        for (int sy = 0; sy < 5 && ok; ++sy)
        {
            for (int sx = 0; sx < 5 && ok; ++sx)
            {
                const auto* row = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(kSampleY[sy]) * lr.Pitch;
                const int b = row[kSampleX[sx] * 4 + 0];
                const int g = row[kSampleX[sx] * 4 + 1];
                const int r = row[kSampleX[sx] * 4 + 2];
                if (r != expected_r || b != expected_b || g != 0)
                {
                    printf(
                        "[d3d9-park-rotation] FAIL: round %d target %s pixel(%d,%d) counted R=%d G=%d B=%d draws, expected R=%d G=0 B=%d "
                        "-- a draw was dropped, replayed, or landed on the wrong target across a slot-rotation park\n",
                        round, label, kSampleX[sx], kSampleY[sy], r, g, b, expected_r, expected_b);
                    ok = false;
                }
            }
        }
        rt->UnlockRect();
        return ok;
    }
}

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-park-rotation] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9parkrotation";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "park-rotation", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth, kCanvasHeight,
                                nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-park-rotation] FAIL: Direct3DCreate9 returned null\n");
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
        printf("[d3d9-park-rotation] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* targets[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i)
    {
        const HRESULT hcrt =
            dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &targets[i], nullptr);
        if (FAILED(hcrt) || !targets[i])
        {
            printf("[d3d9-park-rotation] FAIL: CreateRenderTarget %d hr=0x%08lx\n", i, static_cast<unsigned long>(hcrt));
            dev->Release();
            d3d->Release();
            return 1;
        }
    }

    // One vertex buffer per target, so the alternating loop never has to Lock between switches: the
    // whole round records as pure state changes plus draws, which is what keeps the batches big
    // enough for the rotations to find the GPU still busy.
    IDirect3DVertexBuffer9* buffers[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i)
    {
        const HRESULT hcvb = dev->CreateVertexBuffer(kVertsPerDraw * sizeof(FvfVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, kFvf,
                                                     D3DPOOL_DEFAULT, &buffers[i], nullptr);
        if (FAILED(hcvb) || !buffers[i])
        {
            printf("[d3d9-park-rotation] FAIL: CreateVertexBuffer %d hr=0x%08lx\n", i, static_cast<unsigned long>(hcvb));
            dev->Release();
            d3d->Release();
            return 1;
        }
    }

    IDirect3DIndexBuffer9* ib = nullptr;
    const HRESULT hcib = dev->CreateIndexBuffer(kIndicesPerDraw * sizeof(DWORD), 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib, nullptr);
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-park-rotation] FAIL: CreateIndexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcib));
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

    // Target A's increment is one step of red, target B's one step of blue.
    if (!fill_vertex_buffer(buffers[0], D3DCOLOR_ARGB(0, 1, 0, 0)) || !fill_vertex_buffer(buffers[1], D3DCOLOR_ARGB(0, 0, 0, 1)))
    {
        printf("[d3d9-park-rotation] FAIL: vertex buffer fill failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetIndices(ib);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

    bool all_pass = true;
    for (int round = 0; round < kRounds && all_pass; ++round)
    {
        const int draws_per_target = kDrawsPerTarget[round];

        dev->BeginScene();
        for (int i = 0; i < 2; ++i)
        {
            dev->SetRenderTarget(0, targets[i]);
            dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        }

        for (int s = 0; s < kSwitchesPerRound; ++s)
        {
            const int which = s & 1;
            dev->SetRenderTarget(0, targets[which]);
            dev->SetStreamSource(0, buffers[which], 0, sizeof(FvfVertex));
            for (int d = 0; d < draws_per_target; ++d)
            {
                dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, kVertsPerDraw, 0, kQuadsPerDraw * 2);
            }
        }
        dev->EndScene();

        // Present, not just EndScene: the UMD only drains its recorded-command batch on a call that
        // needs the host to observe it, and a LockRect of a render target that is no longer BOUND is
        // not one of them -- umd_Lock's resource_currently_referenced check does not recognise a
        // target the still-pending batch drew into. That is a separate, pre-existing UMD bug (a
        // readback of an unbound render target with pending draws returns stale pixels); presenting
        // between the drawing and the readback keeps this test aimed at the park instead of it.
        dev->Present(nullptr, nullptr, nullptr, nullptr);

        const int expected = (kSwitchesPerRound / 2) * draws_per_target;
        if (!check_target(targets[0], expected, 0, "A", round) || !check_target(targets[1], 0, expected, "B", round))
        {
            all_pass = false;
            break;
        }

        printf("[d3d9-park-rotation] PASS: round %d, %d render-target switches, both targets counted exactly %d draws\n", round,
               kSwitchesPerRound, expected);
    }

    if (all_pass)
    {
        printf("[d3d9-park-rotation] ALL CHECKS PASSED\n");
    }

    ib->Release();
    for (auto* vb : buffers)
    {
        vb->Release();
    }
    for (auto* rt : targets)
    {
        rt->Release();
    }
    dev->Release();
    d3d->Release();
    return all_pass ? 0 : 1;
}
