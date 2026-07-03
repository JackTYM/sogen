// D3D9-over-Vulkan de-risk slice (see .claude/plans/jazzy-giggling-cloud.md).
//
// Phase 1: force real backbuffer creation + a clear + a present, no draw yet -- this is the
// forcing function for RE'ing D3DDDIARG_CREATERESOURCE (render-target case) and
// D3DDDIARG_PRESENT's real layout, neither of which gate 3's d3d9_spike_test.cpp exercises.
// Phase 4 will extend this file with a vertex buffer and DrawPrimitive.

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
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "triangle", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr,
                                wc.hInstance, nullptr);
    printf("[d3d9-triangle] hwnd=%p\n", static_cast<void*>(hwnd));

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-triangle] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-triangle] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
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

    dev->Release();
    d3d->Release();
    return 0;
}
