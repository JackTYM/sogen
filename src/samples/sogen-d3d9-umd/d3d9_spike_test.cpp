// Spike-B guest: drive the official d3d9.dll to CreateDevice so it loads the sogen D3D9 UMD.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-spike] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9spike";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "spike", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr,
                                wc.hInstance, nullptr);
    printf("[d3d9-spike] hwnd=%p\n", static_cast<void*>(hwnd));

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-spike] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-spike] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    UINT adapters = d3d->GetAdapterCount();
    printf("[d3d9-spike] GetAdapterCount=%u\n", adapters);

    D3DCAPS9 caps{};
    HRESULT hc = d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
    printf("[d3d9-spike] GetDeviceCaps hr=0x%08lx VS=0x%lx PS=0x%lx\n", static_cast<unsigned long>(hc),
           static_cast<unsigned long>(caps.VertexShaderVersion), static_cast<unsigned long>(caps.PixelShaderVersion));

    // Exercise the same public checks CreateDevice itself performs, for standalone diagnostics.
    D3DDISPLAYMODE dm{};
    HRESULT hdm = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
    printf("[d3d9-spike] GetAdapterDisplayMode hr=0x%08lx Format=%u %ux%u\n", static_cast<unsigned long>(hdm), dm.Format, dm.Width,
           dm.Height);

    HRESULT hcdt = d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
    printf("[d3d9-spike] CheckDeviceType(HAL, X8R8G8B8, X8R8G8B8, windowed) hr=0x%08lx\n", static_cast<unsigned long>(hcdt));

    HRESULT hcdf = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET,
                                          D3DRTYPE_SURFACE, D3DFMT_X8R8G8B8);
    printf("[d3d9-spike] CheckDeviceFormat(HAL, X8R8G8B8 adapter, RENDERTARGET, X8R8G8B8) hr=0x%08lx\n",
           static_cast<unsigned long>(hcdf));

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
    printf("[d3d9-spike] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));

    if (SUCCEEDED(hr) && dev)
    {
        printf("[d3d9-spike] SUCCESS: IDirect3DDevice9 created\n");
        dev->Release();
    }
    else
    {
        printf("[d3d9-spike] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
    }

    d3d->Release();
    return 0;
}
