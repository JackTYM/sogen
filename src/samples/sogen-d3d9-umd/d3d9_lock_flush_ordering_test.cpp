// D3D9-over-Vulkan Lock/flush-ordering test.
//
// Exercises the exact hazard the flush-cascade fix (bridge_call's needs_flush parameter,
// g_batch_touched_resources/g_bound_* in sogen_d3d9_umd.cpp) has to get right: SetStreamSource+
// DrawIndexedPrimitive batch into g_d3d9_command_batch without reaching the host immediately, and the
// actual vertex upload happens only when the batch is later flushed. If a D3DLOCK_DISCARD Lock() on
// the SAME vertex buffer is allowed to overwrite it with new data BEFORE that still-pending draw's
// flush runs, the draw would wrongly pick up the new (post-discard) data instead of the data that was
// live when DrawIndexedPrimitive was called -- silent, load-dependent corruption.
//
// Structure deliberately mirrors d3d9_scissor_test.cpp's already-proven-working draw/readback pattern
// (same D3DFVF_XYZRHW quad-via-index-buffer shape, same D3DRS_CULLMODE/D3DRS_ZENABLE setup, same
// CreateRenderTarget+LockRect readback) rather than a hand-rolled one, specifically to keep this test
// isolated to the ONE thing it's meant to prove (flush ordering) instead of also re-deriving basic
// fixed-function rendering correctness that other tests already cover.
//
// Content A: a full-canvas RED quad, drawn via DrawIndexedPrimitive (batched, not yet flushed).
// Content B: immediately after, with A's draw still unflushed, Discard-Lock the SAME vertex buffer and
// overwrite it with a small quad confined to one corner, in GREEN. If flush ordering is correct, A's
// draw is flushed (and therefore executes with A's full-canvas geometry) before B ever reaches the
// host, so the center pixel reads RED. If flush ordering is broken, A's draw executes against B's
// data instead -- B never covers the center, so the center would incorrectly read the BLUE clear
// color instead.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-lock-flush-ordering] start\n");

    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9lockflushordering";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "lock-flush-ordering", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                 kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-lock-flush-ordering] FAIL: Direct3DCreate9 returned null\n");
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
        printf("[d3d9-lock-flush-ordering] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-lock-flush-ordering] FAIL: CreateRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    struct FvfVertex
    {
        float x, y, z, rhw;
        DWORD color;
    };
    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(4 * sizeof(FvfVertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-lock-flush-ordering] FAIL: CreateVertexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcvb));
        rt->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    // Content A: full-canvas RED quad.
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
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-lock-flush-ordering] FAIL: CreateIndexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcib));
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

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
    HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-lock-flush-ordering] DrawIndexedPrimitive(content A, full-canvas RED) hr=0x%08lx\n", static_cast<unsigned long>(hd));

    // Force a genuine intervening flush between the draw above and the Discard-Lock below, via an
    // unrelated resource creation (pfnCreateResource always flushes unconditionally -- see bridge_call's
    // default needs_flush=true). This is the exact scenario that broke the first draft of the flush-
    // cascade fix: the SetStreamSource/DrawIndexedPrimitive batch gets drained here, so a naive
    // "clear the touched-resource set on every flush" design would orphan the draw's real dependency,
    // wrongly concluding vb is untouched by the time the Discard-Lock below runs.
    IDirect3DTexture9* dummy_tex = nullptr;
    dev->CreateTexture(4, 4, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &dummy_tex, nullptr);
    if (dummy_tex)
    {
        dummy_tex->Release();
    }

    // Immediately, with the draw above still unflushed, Discard-Lock the SAME vertex buffer and
    // overwrite it with content B: a small quad confined to the top-left 40x40 corner, in GREEN --
    // nowhere near the center pixel this test checks. If the flush-cascade fix incorrectly skips the
    // flush here, the pending draw executes later against B's data instead of A's, and the center
    // stays at the BLUE clear color instead of being covered by A's full-canvas red.
    FvfVertex* verts_b = nullptr;
    HRESULT hlock2 = vb->Lock(0, 4 * sizeof(FvfVertex), reinterpret_cast<void**>(&verts_b), D3DLOCK_DISCARD);
    printf("[d3d9-lock-flush-ordering] Lock(DISCARD, content B) hr=0x%08lx\n", static_cast<unsigned long>(hlock2));
    if (SUCCEEDED(hlock2) && verts_b)
    {
        const DWORD green = D3DCOLOR_XRGB(0, 255, 0);
        verts_b[0] = {0.0f, 0.0f, 0.5f, 1.0f, green};
        verts_b[1] = {40.0f, 0.0f, 0.5f, 1.0f, green};
        verts_b[2] = {40.0f, 40.0f, 0.5f, 1.0f, green};
        verts_b[3] = {0.0f, 40.0f, 0.5f, 1.0f, green};
        vb->Unlock();
    }

    dev->EndScene();

    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    bool pass = false;
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const int x = 320;
        const int y = 240;
        const auto* row = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(y) * lr.Pitch;
        const unsigned char b = row[x * 4 + 0];
        const unsigned char g = row[x * 4 + 1];
        const unsigned char r = row[x * 4 + 2];
        printf("[d3d9-lock-flush-ordering] pixel(320,240)=B=%02X G=%02X R=%02X (expected content-A RED: B=00 G=00 R=FF)\n", b, g, r);
        pass = (r > 200 && g < 50 && b < 50);
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-lock-flush-ordering] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
    }

    if (pass)
    {
        printf("[d3d9-lock-flush-ordering] PASS: draw correctly used pre-Discard content A, flush ordering intact\n");
    }
    else
    {
        printf("[d3d9-lock-flush-ordering] FAIL: draw did not use content A -- flush ordering broken by the Discard-Lock\n");
    }

    ib->Release();
    vb->Release();
    rt->Release();
    dev->Release();
    d3d->Release();
    return pass ? 0 : 1;
}
