// D3D9-over-Vulkan de-risk slice (see .claude/plans/jazzy-giggling-cloud.md).
//
// Draws a real triangle (via the fixed-function D3DFVF_XYZRHW|D3DFVF_DIFFUSE path) to an off-screen
// render target for analytic verification (LockRect readback), then draws the same triangle again to
// the real swap-chain backbuffer and Present()s it so it's actually visible on screen.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-triangle] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9triangle";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "triangle", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, nullptr,
                                nullptr, wc.hInstance, nullptr);
    printf("[d3d9-triangle] hwnd=%p\n", static_cast<void*>(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-triangle] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-triangle] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // required for LockRect on the backbuffer at all
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
                                   &dev);
    printf("[d3d9-triangle] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));

    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-triangle] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    printf("[d3d9-triangle] SUCCESS: IDirect3DDevice9 created\n");

    HRESULT hbs = dev->BeginScene();
    printf("[d3d9-triangle] BeginScene hr=0x%08lx\n", static_cast<unsigned long>(hbs));

    HRESULT hclr = dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
    printf("[d3d9-triangle] Clear hr=0x%08lx\n", static_cast<unsigned long>(hclr));

    HRESULT hes = dev->EndScene();
    printf("[d3d9-triangle] EndScene hr=0x%08lx\n", static_cast<unsigned long>(hes));

    HRESULT hp = dev->Present(nullptr, nullptr, nullptr, nullptr);
    printf("[d3d9-triangle] Present hr=0x%08lx\n", static_cast<unsigned long>(hp));

    if (SUCCEEDED(hp))
    {
        printf("[d3d9-triangle] SUCCESS: Present completed\n");
    }
    else
    {
        printf("[d3d9-triangle] FAIL: Present hr=0x%08lx\n", static_cast<unsigned long>(hp));
    }

    // Verify Clear reaches real GPU backing directly, via an explicit off-swapchain render target
    // (CreateRenderTarget -> SetRenderTarget -> Clear -> LockRect) -- avoids the implicit backbuffer's
    // swapchain/present-entangled Lock semantics entirely, so this is a clean, standard D3D9 pattern.
    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(640, 480, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-triangle] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));

    IDirect3DSurface9* original_rt = nullptr;
    dev->GetRenderTarget(0, &original_rt);

    if (SUCCEEDED(hcrt) && rt)
    {
        HRESULT hsrt = dev->SetRenderTarget(0, rt);
        printf("[d3d9-triangle] SetRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hsrt));

        // DrawPrimitive is only valid between BeginScene/EndScene -- the first pair (above) was already
        // closed before Present(); this is a fresh scene for the render-target draw.
        HRESULT hbs2 = dev->BeginScene();
        printf("[d3d9-triangle] BeginScene(rt) hr=0x%08lx\n", static_cast<unsigned long>(hbs2));

        HRESULT hclr2 = dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
        printf("[d3d9-triangle] Clear(rt) hr=0x%08lx\n", static_cast<unsigned long>(hclr2));

        // Part 3 pipeline-builder smoke test: a real triangle, D3DFVF_XYZRHW|D3DFVF_DIFFUSE (pre-
        // transformed screen-space position + per-vertex color, matching d3d9_host's hardcoded
        // fixed-function shader pair).
        struct FvfVertex
        {
            float x, y, z, rhw;
            DWORD color;
        };
        constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
        IDirect3DVertexBuffer9* vb = nullptr;
        HRESULT hcvb = dev->CreateVertexBuffer(3 * sizeof(FvfVertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
        printf("[d3d9-triangle] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb), static_cast<void*>(vb));

        if (SUCCEEDED(hcvb) && vb)
        {
            FvfVertex* verts = nullptr;
            HRESULT hvblock = vb->Lock(0, 3 * sizeof(FvfVertex), reinterpret_cast<void**>(&verts), 0);
            printf("[d3d9-triangle] VB Lock hr=0x%08lx data=%p\n", static_cast<unsigned long>(hvblock), static_cast<void*>(verts));
            if (SUCCEEDED(hvblock) && verts)
            {
                verts[0] = {320.0f, 120.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0)};
                verts[1] = {480.0f, 360.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0)};
                verts[2] = {160.0f, 360.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255)};
                vb->Unlock();
            }

            HRESULT hsfvf = dev->SetFVF(kFvf);
            printf("[d3d9-triangle] SetFVF hr=0x%08lx\n", static_cast<unsigned long>(hsfvf));
            HRESULT hsss = dev->SetStreamSource(0, vb, 0, sizeof(FvfVertex));
            printf("[d3d9-triangle] SetStreamSource hr=0x%08lx\n", static_cast<unsigned long>(hsss));
            HRESULT hdp = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
            printf("[d3d9-triangle] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdp));

            HRESULT hes2 = dev->EndScene();
            printf("[d3d9-triangle] EndScene(rt) hr=0x%08lx\n", static_cast<unsigned long>(hes2));

            // Draw the same triangle again, this time onto the real backbuffer and Present() it, so the
            // triangle actually shows up on screen (the earlier draw only ever reaches the off-screen
            // surface read back by LockRect below).
            if (original_rt)
            {
                HRESULT hunbind = dev->SetRenderTarget(0, original_rt);
                printf("[d3d9-triangle] SetRenderTarget(restore original) hr=0x%08lx\n", static_cast<unsigned long>(hunbind));

                HRESULT hbs3 = dev->BeginScene();
                printf("[d3d9-triangle] BeginScene(backbuffer) hr=0x%08lx\n", static_cast<unsigned long>(hbs3));
                dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
                dev->SetFVF(kFvf);
                dev->SetStreamSource(0, vb, 0, sizeof(FvfVertex));
                HRESULT hdp2 = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                printf("[d3d9-triangle] DrawPrimitive(backbuffer) hr=0x%08lx\n", static_cast<unsigned long>(hdp2));
                dev->EndScene();

                HRESULT hp2 = dev->Present(nullptr, nullptr, nullptr, nullptr);
                printf("[d3d9-triangle] Present(triangle) hr=0x%08lx\n", static_cast<unsigned long>(hp2));
            }

            vb->Release();
        }
        else
        {
            printf("[d3d9-triangle] FAIL: CreateVertexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcvb));
        }

        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-triangle] LockRect hr=0x%08lx pBits=%p Pitch=%ld\n", static_cast<unsigned long>(hlr), lr.pBits,
               static_cast<long>(lr.Pitch));

        if (SUCCEEDED(hlr) && lr.pBits)
        {
            const auto* pixel = static_cast<const unsigned char*>(lr.pBits);
            printf("[d3d9-triangle] pixel[0]=B=%02X G=%02X R=%02X A=%02X (expected B=FF G=80 R=40)\n", pixel[0], pixel[1],
                   pixel[2], pixel[3]);
            rt->UnlockRect();
        }
        else
        {
            printf("[d3d9-triangle] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        }
        rt->Release();
    }
    else
    {
        printf("[d3d9-triangle] FAIL: CreateRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
    }

    if (original_rt)
    {
        original_rt->Release();
    }

    dev->Release();
    d3d->Release();
    return 0;
}
