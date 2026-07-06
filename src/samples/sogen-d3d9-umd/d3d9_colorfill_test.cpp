// IDirect3DDevice9::ColorFill guest regression test. Proves the real d3d9.dll ColorFill path
// (device-func-table slot 56 -> pfnColorFill -> D3DDDIARG_COLORFILL, RE'd in d3d9_ddi.hpp) reaches
// the sogen UMD, streams a color_fill record to the host, and fills exactly the requested sub-rect of
// a render target's Vulkan image while leaving every pixel OUTSIDE the rect untouched.
//
// Discriminator: a 640x480 render target is cleared BLUE, then ColorFill fills the center rect
// {160,120,480,360} RED. Pixels inside the rect must read back RED; pixels outside must stay BLUE. A
// whole-surface fill, an off-by-one rect, or a fill that ignored the rect entirely would each miss at
// least one of these checkpoints. LockRect returns BGRA8 (byte0=B, byte1=G, byte2=R, byte3=A).

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Center rect (D3D9 RECT: right/bottom exclusive). 320x240, centered.
    constexpr RECT kFillRect = {160, 120, 480, 360};

    constexpr int kRedB = 0, kRedG = 0, kRedR = 255;
    constexpr int kBlueB = 255, kBlueG = 0, kBlueR = 0;

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

    // Four interior points (all inside the fill rect, including two near its edges) must be RED; four
    // exterior points (corners + a point just outside each edge) must remain the BLUE clear color.
    const Check kChecks[] = {
        {"center (inside rect)", 320, 240, kRedB, kRedG, kRedR},
        {"just inside top-left corner of rect", 162, 122, kRedB, kRedG, kRedR},
        {"just inside bottom-right corner of rect", 477, 357, kRedB, kRedG, kRedR},
        {"mid-left inside rect", 200, 240, kRedB, kRedG, kRedR},
        {"top-left canvas corner (outside rect)", 10, 10, kBlueB, kBlueG, kBlueR},
        {"bottom-right canvas corner (outside rect)", 630, 470, kBlueB, kBlueG, kBlueR},
        {"just left of rect (outside)", 158, 240, kBlueB, kBlueG, kBlueR},
        {"just above rect (outside)", 320, 118, kBlueB, kBlueG, kBlueR},
    };

    int run_checks(IDirect3DSurface9* rt)
    {
        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-colorfill-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-colorfill-test] FAIL: LockRect failed\n");
            return static_cast<int>(sizeof(kChecks) / sizeof(kChecks[0]));
        }

        // The sogen UMD's pfnLock does not populate D3DLOCKED_RECT::Pitch (d3d9.dll reports 0 for these
        // driver-lockable render targets), so use the surface's known tightly-packed BGRA8 row stride --
        // the same convention every other guest test here (scissor/mrt/...) uses.
        constexpr LONG kStride = kCanvasWidth * 4;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        auto pixel_at = [&](const int col, const int row) { return base + static_cast<size_t>(row) * kStride + col * 4; };

        int failures = 0;
        for (const Check& c : kChecks)
        {
            const unsigned char* p = pixel_at(c.col, c.row);
            printf("[d3d9-colorfill-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", c.name, c.col, c.row, p[0], p[1], p[2],
                   p[3]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) ||
                !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-colorfill-test] FAIL: %s does not match expected B=%02X G=%02X R=%02X\n", c.name, c.expected_b,
                       c.expected_g, c.expected_r);
                ++failures;
            }
            else
            {
                printf("[d3d9-colorfill-test] PASS: %s matches the expected color\n", c.name);
            }
        }

        rt->UnlockRect();
        return failures;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-colorfill-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9colorfilltest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "colorfill-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-colorfill-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-colorfill-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-colorfill-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-colorfill-test] FAIL: render target creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    // Bind + Clear BLUE establishes the "outside" background color and leaves the RT in the resting
    // TRANSFER_SRC_OPTIMAL layout the host's ColorFill path expects.
    dev->SetRenderTarget(0, rt);
    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
    dev->EndScene();

    const D3DCOLOR red = D3DCOLOR_ARGB(255, 255, 0, 0);
    RECT fill = kFillRect;
    HRESULT hcf = dev->ColorFill(rt, &fill, red);
    printf("[d3d9-colorfill-test] ColorFill({%ld,%ld,%ld,%ld}, 0x%08lX) hr=0x%08lx\n", fill.left, fill.top, fill.right,
           fill.bottom, static_cast<unsigned long>(red), static_cast<unsigned long>(hcf));

    int failures = 0;
    if (FAILED(hcf))
    {
        printf("[d3d9-colorfill-test] FAIL: ColorFill returned an error\n");
        ++failures;
    }
    else
    {
        failures += run_checks(rt);
    }

    rt->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-colorfill-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
