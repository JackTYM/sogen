// D3DPOOL_MANAGED vertex and index buffers end to end -- the buffer half of the MANAGED pool, which
// nothing else in this suite covered. A managed buffer is the one resource kind whose contents the D3D9
// runtime keeps in a system-memory master and pushes to the driver behind the app's back, through
// pfnBufBlt (device-func-table slot 17, the vertex/index-buffer counterpart of pfnTexBlt/pfnVolBlt); the
// slot was an unwired no-op stub until this test's companion change, so this is also the regression guard
// for that wiring -- a driver that mis-parses D3DDDIARG_BUFFERBLT and copies into the wrong resource,
// or over the wrong range, corrupts the very geometry being drawn here.
//
// Each sub-pass rewrites the buffers and redraws, so a stale or partially-synced copy shows up as the
// PREVIOUS pass's colour rather than the current one -- a much sharper signal than a single draw.
// D3DUSAGE_WRITEONLY is deliberate and required: a D3DPOOL_MANAGED buffer without it currently fails
// creation inside the runtime itself (E_FAIL), which this test reports rather than silently working
// around.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;
    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    void write_quad(IDirect3DVertexBuffer9* vb, const DWORD color)
    {
        Vertex* v = nullptr;
        if (SUCCEEDED(vb->Lock(0, 0, reinterpret_cast<void**>(&v), 0)) && v)
        {
            v[0] = {-0.9f, -0.9f, 0.5f, color};
            v[1] = {0.9f, -0.9f, 0.5f, color};
            v[2] = {0.9f, 0.9f, 0.5f, color};
            v[3] = {-0.9f, 0.9f, 0.5f, color};
            vb->Unlock();
        }
    }

    int run_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const int exp_b, const int exp_g, const int exp_r, const char* label)
    {
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        const HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();

        D3DLOCKED_RECT lr{};
        const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hd) || FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-managed-buffer-test] FAIL: %s draw hr=0x%08lx rt LockRect hr=0x%08lx\n", label, static_cast<unsigned long>(hd),
                   static_cast<unsigned long>(hlr));
            return 1;
        }
        constexpr LONG kStride = kCanvasWidth * 4;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const unsigned char* centre = base + (kCanvasHeight / 2) * kStride + (kCanvasWidth / 2) * 4;
        // A corner well outside the quad must stay the clear colour: proves the index buffer really
        // describes the two triangles it was written with, not a stale or over-copied index range.
        const unsigned char* corner = base + 4 * kStride + 4 * 4;
        printf("[d3d9-managed-buffer-test] %s centre=B=%02X G=%02X R=%02X (expected B=%02X G=%02X R=%02X) corner=B=%02X G=%02X R=%02X\n",
               label, centre[0], centre[1], centre[2], exp_b, exp_g, exp_r, corner[0], corner[1], corner[2]);
        int failed = 0;
        if (!channel_close(centre[0], exp_b, 4) || !channel_close(centre[1], exp_g, 4) || !channel_close(centre[2], exp_r, 4))
        {
            printf("[d3d9-managed-buffer-test] FAIL: %s centre pixel wrong\n", label);
            failed = 1;
        }
        else if (corner[0] != 0 || corner[1] != 0 || corner[2] != 0)
        {
            printf("[d3d9-managed-buffer-test] FAIL: %s corner pixel is not the clear colour\n", label);
            failed = 1;
        }
        else
        {
            printf("[d3d9-managed-buffer-test] PASS: %s\n", label);
        }
        rt->UnlockRect();
        return failed;
    }

    void release_all(IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib, IDirect3DSurface9* rt, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (vb)
        {
            vb->Release();
        }
        if (ib)
        {
            ib->Release();
        }
        if (rt)
        {
            rt->Release();
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
    printf("[d3d9-managed-buffer-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9managedbuffertest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "managed-buffer-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-managed-buffer-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-managed-buffer-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hvb = dev->CreateVertexBuffer(4 * sizeof(Vertex), D3DUSAGE_WRITEONLY, kFvf, D3DPOOL_MANAGED, &vb, nullptr);
    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hib = dev->CreateIndexBuffer(sizeof(kQuadIndices), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, nullptr);
    printf("[d3d9-managed-buffer-test] CreateVertexBuffer(MANAGED) hr=0x%08lx / CreateIndexBuffer(MANAGED) hr=0x%08lx\n",
           static_cast<unsigned long>(hvb), static_cast<unsigned long>(hib));
    if (FAILED(hvb) || FAILED(hib) || !vb || !ib)
    {
        printf("[d3d9-managed-buffer-test] FAIL: managed buffer creation failed\n");
        release_all(vb, ib, nullptr, dev, d3d);
        return 1;
    }

    {
        WORD* idx = nullptr;
        if (SUCCEEDED(ib->Lock(0, 0, reinterpret_cast<void**>(&idx), 0)) && idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
        else
        {
            printf("[d3d9-managed-buffer-test] FAIL: managed index buffer Lock failed\n");
            release_all(vb, ib, nullptr, dev, d3d);
            return 1;
        }
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-managed-buffer-test] FAIL: render target creation hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
        release_all(vb, ib, nullptr, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;

    write_quad(vb, D3DCOLOR_ARGB(255, 0, 255, 0));
    failures += run_pass(dev, rt, /*B*/ 0, /*G*/ 255, /*R*/ 0, "first write (GREEN)");

    // PreLoad is the app's explicit "make this current in video memory" request; the runtime turns it
    // straight into a pfnBufBlt.
    vb->PreLoad();
    ib->PreLoad();
    failures += run_pass(dev, rt, /*B*/ 0, /*G*/ 255, /*R*/ 0, "after PreLoad (still GREEN)");

    write_quad(vb, D3DCOLOR_ARGB(255, 255, 0, 0));
    failures += run_pass(dev, rt, /*B*/ 0, /*G*/ 0, /*R*/ 255, "rewrite (RED, not the stale GREEN)");

    // Eviction drops the video-memory copy; the next draw has to reconstitute it from the master.
    dev->EvictManagedResources();
    failures += run_pass(dev, rt, /*B*/ 0, /*G*/ 0, /*R*/ 255, "after EvictManagedResources (still RED)");

    write_quad(vb, D3DCOLOR_ARGB(255, 0, 0, 255));
    failures += run_pass(dev, rt, /*B*/ 255, /*G*/ 0, /*R*/ 0, "rewrite after eviction (BLUE)");

    // A sub-range lock: only vertex 0 changes, so the other three must keep the previous colour. The
    // centre of a Gouraud-shaded quad with one yellow and three blue corners is the average of the two
    // triangles' interpolants at that point -- (0,0,255) and (255,255,0) meet at 1/4 weight on v0.
    {
        Vertex* v = nullptr;
        if (SUCCEEDED(vb->Lock(0, sizeof(Vertex), reinterpret_cast<void**>(&v), 0)) && v)
        {
            v[0].color = D3DCOLOR_ARGB(255, 255, 255, 0);
            vb->Unlock();
        }
    }
    failures += run_pass(dev, rt, /*B*/ 128, /*G*/ 127, /*R*/ 127, "partial-range rewrite (v0 only)");

    release_all(vb, ib, rt, dev, d3d);

    printf("[d3d9-managed-buffer-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
