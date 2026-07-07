// Cube/volume texture CREATION regression test for the g_formats FORMATOP cube/volume expansion and the
// umd_CreateResource Flags-based kind classification (sogen_d3d9_umd.cpp).
//
// Before that change the g_formats table applied FMT_OP_CUBETEXTURE/FMT_OP_VOLUMETEXTURE to zero rows, so
// real d3d9.dll rejected CreateCubeTexture/CreateVolumeTexture for EVERY format with D3DERR_INVALIDCALL
// (0x8876086c) -- it gates those entry points on the driver's per-format op-word. Adding the bits to the
// A8R8G8B8/X8R8G8B8 rows (cube+volume) and the DXT rows (cube only) flips creation to S_OK; this is a
// genuine BEFORE/AFTER discriminator.
//
// Scope is CREATION ONLY: cube/volume resources have no host-side GPU backing yet (the host
// create_resource/ensure_texture_uploaded paths only back plain texture_2d), so this test never Locks,
// StretchRects, samples, or draws with these resources -- it only asserts the creation HRESULT and releases.
// Rendering/sampling is a separate later task.
//
// Checks:
// 1. CreateCubeTexture(A8R8G8B8)   must SUCCEED (row carries FMT_OP_CUBETEXTURE).
// 2. CreateVolumeTexture(A8R8G8B8) must SUCCEED (row carries FMT_OP_VOLUMETEXTURE).
// 3. Negative control -- CreateCubeTexture(L8) and CreateVolumeTexture(L8) must still FAIL: L8 carries only
//    FMT_OP_TEXTURE, proving the capability is format-specific rather than "everything now works".
// 4. DXT cube/volume split -- CreateCubeTexture(DXT1) must SUCCEED but CreateVolumeTexture(DXT1) must FAIL,
//    proving the deliberate cube-yes/volume-no scope choice for the compressed rows.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>

namespace
{
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // A cube-texture create must succeed/fail as `expect_ok` says. Releases the resource on success.
    bool check_cube(IDirect3DDevice9* dev, D3DFORMAT format, bool expect_ok, const char* label)
    {
        IDirect3DCubeTexture9* tex = nullptr;
        HRESULT hr = dev->CreateCubeTexture(64, 1, 0, format, D3DPOOL_DEFAULT, &tex, nullptr);
        printf("[d3d9-cube-volume-test] CreateCubeTexture(%s) hr=0x%08lx tex=%p (expect %s)\n", label,
               static_cast<unsigned long>(hr), static_cast<void*>(tex), expect_ok ? "S_OK" : "FAIL");
        const bool ok = expect_ok ? (SUCCEEDED(hr) && tex != nullptr) : (FAILED(hr) && tex == nullptr);
        if (tex)
        {
            tex->Release();
        }
        if (!ok)
        {
            printf("[d3d9-cube-volume-test] FAIL: CreateCubeTexture(%s) did not match expectation\n", label);
        }
        return ok;
    }

    // A volume-texture create must succeed/fail as `expect_ok` says. Releases the resource on success.
    bool check_volume(IDirect3DDevice9* dev, D3DFORMAT format, bool expect_ok, const char* label)
    {
        IDirect3DVolumeTexture9* vol = nullptr;
        HRESULT hr = dev->CreateVolumeTexture(32, 32, 4, 1, 0, format, D3DPOOL_DEFAULT, &vol, nullptr);
        printf("[d3d9-cube-volume-test] CreateVolumeTexture(%s) hr=0x%08lx vol=%p (expect %s)\n", label,
               static_cast<unsigned long>(hr), static_cast<void*>(vol), expect_ok ? "S_OK" : "FAIL");
        const bool ok = expect_ok ? (SUCCEEDED(hr) && vol != nullptr) : (FAILED(hr) && vol == nullptr);
        if (vol)
        {
            vol->Release();
        }
        if (!ok)
        {
            printf("[d3d9-cube-volume-test] FAIL: CreateVolumeTexture(%s) did not match expectation\n", label);
        }
        return ok;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-cube-volume-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9cubevolumetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "cube-volume-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-cube-volume-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-cube-volume-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    int failures = 0;

    // 1 & 2: A8R8G8B8 cube + volume must now create.
    if (!check_cube(dev, D3DFMT_A8R8G8B8, true, "A8R8G8B8"))
    {
        ++failures;
    }
    if (!check_volume(dev, D3DFMT_A8R8G8B8, true, "A8R8G8B8"))
    {
        ++failures;
    }

    // 3: L8 was NOT granted cube/volume capability -- both must still fail (the discriminator).
    if (!check_cube(dev, D3DFMT_L8, false, "L8"))
    {
        ++failures;
    }
    if (!check_volume(dev, D3DFMT_L8, false, "L8"))
    {
        ++failures;
    }

    // 4: DXT1 got cube but deliberately NOT volume -- cube must succeed, volume must fail.
    if (!check_cube(dev, D3DFMT_DXT1, true, "DXT1"))
    {
        ++failures;
    }
    if (!check_volume(dev, D3DFMT_DXT1, false, "DXT1"))
    {
        ++failures;
    }

    dev->Release();
    d3d->Release();

    printf("[d3d9-cube-volume-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
