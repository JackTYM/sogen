// D3D9-over-Vulkan LockRect row-pitch test.
//
// Proves D3DLOCKED_RECT::Pitch reaches the app as the locked mip level's real row stride, and that an
// app filling the surface row-by-row at that stride -- the ordinary pattern every real texture loader
// uses -- lands every row where it belongs.
//
// Before the fix, umd_Lock never wrote D3DDDIARG_LOCK::Pitch. The runtime memsets that struct before
// each call, so the app was handed Pitch == 0 and wrote every row on top of row 0: an uncompressed
// texture ended up holding only its last row, at offset 0, with the rest still zero. Block-compressed
// uploads were unaffected (their loaders copy whole levels rather than stepping by Pitch), which is why
// nothing in this suite caught it -- every test here worked around the gap with a hardcoded stride.
//
// The check is pure D3D9-level: fill each level with a per-row colour ramp using the driver-reported
// Pitch, then read the level back and require row y to hold exactly colour(y). Reading back uses the
// known-tight stride rather than Pitch, so the readback cannot mask a wrong Pitch on the write side.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>

namespace
{
    constexpr UINT kTexWidth = 64;
    constexpr UINT kTexHeight = 32;
    constexpr UINT kMipLevels = 2;

    DWORD row_color(const UINT level, const UINT y)
    {
        return D3DCOLOR_ARGB(255, static_cast<BYTE>(0x40 + level * 0x20), static_cast<BYTE>(y * 4 + 1), static_cast<BYTE>(0xF0 - y * 4));
    }

    int check_level(IDirect3DTexture9* tex, const UINT level)
    {
        const UINT width = kTexWidth >> level;
        const UINT height = kTexHeight >> level;
        const UINT tight_stride = width * 4;
        int failures = 0;

        D3DLOCKED_RECT lr{};
        HRESULT hr = tex->LockRect(level, &lr, nullptr, 0);
        printf("[d3d9-lock-pitch-test] LockRect(level=%u) hr=0x%08lx pBits=%p Pitch=%ld (expected %u)\n", level,
               static_cast<unsigned long>(hr), lr.pBits, static_cast<long>(lr.Pitch), tight_stride);
        if (FAILED(hr) || !lr.pBits)
        {
            printf("[d3d9-lock-pitch-test] FAIL: LockRect(level=%u) failed\n", level);
            return 1;
        }
        if (static_cast<UINT>(lr.Pitch) != tight_stride)
        {
            printf("[d3d9-lock-pitch-test] FAIL: level %u Pitch=%ld, expected %u\n", level, static_cast<long>(lr.Pitch), tight_stride);
            ++failures;
        }

        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (UINT y = 0; y < height; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + static_cast<size_t>(y) * lr.Pitch);
            for (UINT x = 0; x < width; ++x)
            {
                row[x] = row_color(level, y);
            }
        }
        tex->UnlockRect(level);

        D3DLOCKED_RECT rr{};
        hr = tex->LockRect(level, &rr, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !rr.pBits)
        {
            printf("[d3d9-lock-pitch-test] FAIL: readback LockRect(level=%u) failed hr=0x%08lx\n", level, static_cast<unsigned long>(hr));
            return failures + 1;
        }
        const auto* read_base = static_cast<const unsigned char*>(rr.pBits);
        UINT bad_rows = 0;
        UINT first_bad_row = 0;
        DWORD first_bad_value = 0;
        for (UINT y = 0; y < height; ++y)
        {
            const auto* row = reinterpret_cast<const DWORD*>(read_base + static_cast<size_t>(y) * tight_stride);
            if (row[0] != row_color(level, y))
            {
                if (bad_rows == 0)
                {
                    first_bad_row = y;
                    first_bad_value = row[0];
                }
                ++bad_rows;
            }
        }
        tex->UnlockRect(level);

        if (bad_rows != 0)
        {
            printf("[d3d9-lock-pitch-test] FAIL: level %u has %u/%u wrong rows; first is row %u = 0x%08lX, expected 0x%08lX\n", level,
                   bad_rows, height, first_bad_row, static_cast<unsigned long>(first_bad_value),
                   static_cast<unsigned long>(row_color(level, first_bad_row)));
            ++failures;
        }
        else
        {
            printf("[d3d9-lock-pitch-test] PASS: level %u all %u rows landed at the right stride\n", level, height);
        }
        return failures;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-lock-pitch-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9lockpitchtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "lock-pitch-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, nullptr, nullptr,
                                wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-lock-pitch-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-lock-pitch-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DTexture9* tex = nullptr;
    HRESULT hct = dev->CreateTexture(kTexWidth, kTexHeight, kMipLevels, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    printf("[d3d9-lock-pitch-test] CreateTexture(%ux%u, %u levels) hr=0x%08lx tex=%p\n", kTexWidth, kTexHeight, kMipLevels,
           static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        printf("[d3d9-lock-pitch-test] FAIL: CreateTexture failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    int failures = 0;
    for (UINT level = 0; level < kMipLevels; ++level)
    {
        failures += check_level(tex, level);
    }

    tex->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-lock-pitch-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
