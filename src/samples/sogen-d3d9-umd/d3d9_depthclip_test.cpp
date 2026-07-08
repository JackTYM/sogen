// Depth-clip (D3DRS_CLIPPING) discriminator for the shared vulkan_host::create_graphics_pipeline
// depthClampEnable wiring. Draws a full-screen fixed-function quad whose vertices all sit BEYOND the
// far clip plane (clip-space z = 1.5 with rhw = 1, so z > w), twice, into an off-screen render target:
//
//   Pass A: D3DRS_CLIPPING = TRUE (the D3D9 default). Near/far depth clipping is on, so the whole quad
//           is clipped away -- the RT keeps its clear color. This also proves the geometry really is
//           out of range.
//   Pass B: D3DRS_CLIPPING = FALSE. Clipping is disabled, which maps to depthClampEnable = VK_TRUE: the
//           quad is clamped (not clipped) and renders. The RT shows the quad color.
//
// Before the fix, create_graphics_pipeline never set depthClampEnable (stuck at VK_FALSE = clip on), so
// Pass B was wrongly clipped away too and the center pixel read back as the clear color. The two passes
// also build distinct pipelines only because D3DRS_CLIPPING is part of the pipeline cache key, so a
// passing Pass B validates both halves of the fix. Exit code 0 = pass, 1 = fail.

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
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Public D3DRENDERSTATETYPE value (d3d9types.h), in case the test's headers don't name it.
    constexpr D3DRENDERSTATETYPE kD3DRS_CLIPPING = static_cast<D3DRENDERSTATETYPE>(136);

    constexpr int kCenterX = 320;
    constexpr int kCenterY = 240;

    // X8R8G8B8 read back as BGRA bytes: pixel[0]=B, [1]=G, [2]=R.
    bool read_center(IDirect3DSurface9* rt, unsigned char& b, unsigned char& g, unsigned char& r)
    {
        D3DLOCKED_RECT lr{};
        if (FAILED(rt->LockRect(&lr, nullptr, D3DLOCK_READONLY)) || !lr.pBits)
        {
            return false;
        }
        const auto* px = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(kCenterY) * lr.Pitch +
                         static_cast<size_t>(kCenterX) * 4;
        b = px[0];
        g = px[1];
        r = px[2];
        rt->UnlockRect();
        return true;
    }
}

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-depthclip] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9depthclip";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "depthclip", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr,
                                wc.hInstance, nullptr);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-depthclip] FAIL: Direct3DCreate9 null\n");
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
        printf("[d3d9-depthclip] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    if (FAILED(dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt,
                                       nullptr)) ||
        !rt)
    {
        printf("[d3d9-depthclip] FAIL: CreateRenderTarget\n");
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    // Full-canvas RED quad, all four corners beyond the far plane: clip-space z = 1.5, rhw = 1 (z > w).
    IDirect3DVertexBuffer9* vb = nullptr;
    if (FAILED(dev->CreateVertexBuffer(4 * sizeof(FvfVertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr)) || !vb)
    {
        printf("[d3d9-depthclip] FAIL: CreateVertexBuffer\n");
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
            v[0] = {0.0f, 0.0f, 1.5f, 1.0f, red};
            v[1] = {static_cast<float>(kCanvasWidth), 0.0f, 1.5f, 1.0f, red};
            v[2] = {static_cast<float>(kCanvasWidth), static_cast<float>(kCanvasHeight), 1.5f, 1.0f, red};
            v[3] = {0.0f, static_cast<float>(kCanvasHeight), 1.5f, 1.0f, red};
            vb->Unlock();
        }
    }

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DIndexBuffer9* ib = nullptr;
    if (FAILED(dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr)) || !ib)
    {
        printf("[d3d9-depthclip] FAIL: CreateIndexBuffer\n");
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

    int rc = 0;

    // Pass A: clipping ON -> out-of-range quad clipped away -> center stays clear (B=FF,G=80,R=40).
    {
        dev->SetRenderState(kD3DRS_CLIPPING, TRUE);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();

        unsigned char b = 0, g = 0, r = 0;
        if (!read_center(rt, b, g, r))
        {
            printf("[d3d9-depthclip] FAIL: LockRect (pass A)\n");
            rc = 1;
        }
        else
        {
            printf("[d3d9-depthclip] pass A (clipping=ON)  center B=%02X G=%02X R=%02X (expect clear B=FF G=80 R=40)\n", b, g,
                   r);
            if (!(b == 0xFF && g == 0x80 && r == 0x40))
            {
                printf("[d3d9-depthclip] FAIL: pass A did not stay clear -- geometry not actually out of range\n");
                rc = 1;
            }
        }
    }

    // Pass B: clipping OFF -> depthClampEnable=VK_TRUE -> quad clamps and renders -> center is red.
    {
        dev->SetRenderState(kD3DRS_CLIPPING, FALSE);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();

        unsigned char b = 0, g = 0, r = 0;
        if (!read_center(rt, b, g, r))
        {
            printf("[d3d9-depthclip] FAIL: LockRect (pass B)\n");
            rc = 1;
        }
        else
        {
            printf("[d3d9-depthclip] pass B (clipping=OFF) center B=%02X G=%02X R=%02X (expect red B=00 G=00 R=FF)\n", b, g,
                   r);
            if (!(b == 0x00 && g == 0x00 && r == 0xFF))
            {
                printf("[d3d9-depthclip] FAIL: pass B was clipped -- depthClampEnable not honored for D3DRS_CLIPPING=FALSE\n");
                rc = 1;
            }
        }
    }

    printf("[d3d9-depthclip] %s\n", rc == 0 ? "PASS" : "FAIL");

    ib->Release();
    vb->Release();
    rt->Release();
    dev->Release();
    d3d->Release();
    return rc;
}
