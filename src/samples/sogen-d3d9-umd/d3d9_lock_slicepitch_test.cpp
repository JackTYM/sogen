// D3D9-over-Vulkan LockBox slice-pitch test.
//
// Proves D3DLOCKED_BOX::SlicePitch reaches the app as a D3DPOOL_DEFAULT volume level's real depth-slice
// stride, and that an app filling the box with the ordinary `pBits + z*SlicePitch + y*RowPitch` pattern
// lands every slice where it belongs.
//
// This is the volume analog of d3d9_lock_pitch_test: umd_Lock never wrote D3DDDIARG_LOCK::SlicePitch, so
// the memset-before-the-call left the app holding SlicePitch == 0 and every depth slice was written on
// top of slice 0. A D3DPOOL_SYSTEMMEM volume never showed it -- the runtime owns that allocation and
// computes both pitches itself, never asking the driver -- so only a D3DPOOL_DEFAULT volume discriminates.
//
// The check is pure D3D9-level: fill each level with a per-(slice, row) colour using the driver-reported
// pitches, then read the level back and require (slice, row) to hold exactly colour(slice, row). Readback
// uses the known-tight strides rather than the reported ones, so it cannot mask a wrong stride on the
// write side.
//
// x64-only on purpose. The x86 runtime copies the driver's SlicePitch out of D3DDDIARG_LOCK only when the
// locked surface carries a flag bit that no D3DPOOL_DEFAULT/MANAGED volume gets, so on that architecture
// the app is handed SlicePitch == 0 whatever the driver writes -- see d3d9_ddi.hpp's x86 SlicePitch note
// for the exact gate and the experiment that pinned it.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>

namespace
{
    constexpr UINT kWidth = 32;
    constexpr UINT kHeight = 16;
    constexpr UINT kDepth = 4;
    constexpr UINT kMipLevels = 2;

    DWORD slice_row_color(const UINT level, const UINT z, const UINT y)
    {
        return D3DCOLOR_ARGB(255, static_cast<BYTE>(0x11 + level * 0x40), static_cast<BYTE>(0x20 + z * 0x30),
                             static_cast<BYTE>(0x08 + y * 4));
    }

    int check_level(IDirect3DVolumeTexture9* vol, const UINT level)
    {
        const UINT width = kWidth >> level;
        const UINT height = kHeight >> level;
        const UINT depth = kDepth >> level;
        const UINT tight_row = width * 4;
        const UINT tight_slice = tight_row * height;
        int failures = 0;

        D3DLOCKED_BOX lb{};
        HRESULT hr = vol->LockBox(level, &lb, nullptr, 0);
        printf("[d3d9-lock-slicepitch-test] LockBox(level=%u) hr=0x%08lx pBits=%p RowPitch=%ld (expected %u) SlicePitch=%ld "
               "(expected %u)\n",
               level, static_cast<unsigned long>(hr), lb.pBits, static_cast<long>(lb.RowPitch), tight_row, static_cast<long>(lb.SlicePitch),
               tight_slice);
        if (FAILED(hr) || !lb.pBits)
        {
            printf("[d3d9-lock-slicepitch-test] FAIL: LockBox(level=%u) failed\n", level);
            return 1;
        }
        if (static_cast<UINT>(lb.RowPitch) != tight_row)
        {
            printf("[d3d9-lock-slicepitch-test] FAIL: level %u RowPitch=%ld, expected %u\n", level, static_cast<long>(lb.RowPitch),
                   tight_row);
            ++failures;
        }
        if (static_cast<UINT>(lb.SlicePitch) != tight_slice)
        {
            printf("[d3d9-lock-slicepitch-test] FAIL: level %u SlicePitch=%ld, expected %u\n", level, static_cast<long>(lb.SlicePitch),
                   tight_slice);
            ++failures;
        }

        auto* base = static_cast<unsigned char*>(lb.pBits);
        for (UINT z = 0; z < depth; ++z)
        {
            for (UINT y = 0; y < height; ++y)
            {
                auto* row = reinterpret_cast<DWORD*>(base + static_cast<size_t>(z) * lb.SlicePitch + static_cast<size_t>(y) * lb.RowPitch);
                for (UINT x = 0; x < width; ++x)
                {
                    row[x] = slice_row_color(level, z, y);
                }
            }
        }
        vol->UnlockBox(level);

        D3DLOCKED_BOX rb{};
        hr = vol->LockBox(level, &rb, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !rb.pBits)
        {
            printf("[d3d9-lock-slicepitch-test] FAIL: readback LockBox(level=%u) failed hr=0x%08lx\n", level,
                   static_cast<unsigned long>(hr));
            return failures + 1;
        }
        const auto* read_base = static_cast<const unsigned char*>(rb.pBits);
        for (UINT z = 0; z < depth; ++z)
        {
            UINT bad_rows = 0;
            UINT first_bad_row = 0;
            DWORD first_bad_value = 0;
            for (UINT y = 0; y < height; ++y)
            {
                const auto* row =
                    reinterpret_cast<const DWORD*>(read_base + static_cast<size_t>(z) * tight_slice + static_cast<size_t>(y) * tight_row);
                if (row[0] != slice_row_color(level, z, y))
                {
                    if (bad_rows == 0)
                    {
                        first_bad_row = y;
                        first_bad_value = row[0];
                    }
                    ++bad_rows;
                }
            }
            if (bad_rows != 0)
            {
                printf("[d3d9-lock-slicepitch-test] FAIL: level %u slice %u has %u/%u wrong rows; first is row %u = 0x%08lX, "
                       "expected 0x%08lX\n",
                       level, z, bad_rows, height, first_bad_row, static_cast<unsigned long>(first_bad_value),
                       static_cast<unsigned long>(slice_row_color(level, z, first_bad_row)));
                ++failures;
            }
            else
            {
                printf("[d3d9-lock-slicepitch-test] PASS: level %u slice %u all %u rows landed at the right strides\n", level, z, height);
            }
        }
        vol->UnlockBox(level);

        return failures;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-lock-slicepitch-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9lockslicepitchtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "lock-slicepitch-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, nullptr,
                                nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-lock-slicepitch-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-lock-slicepitch-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DVolumeTexture9* vol = nullptr;
    HRESULT hcv =
        dev->CreateVolumeTexture(kWidth, kHeight, kDepth, kMipLevels, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &vol, nullptr);
    printf("[d3d9-lock-slicepitch-test] CreateVolumeTexture(%ux%ux%u, %u levels) hr=0x%08lx vol=%p\n", kWidth, kHeight, kDepth, kMipLevels,
           static_cast<unsigned long>(hcv), static_cast<void*>(vol));
    if (FAILED(hcv) || !vol)
    {
        printf("[d3d9-lock-slicepitch-test] FAIL: CreateVolumeTexture failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    int failures = 0;
    for (UINT level = 0; level < kMipLevels; ++level)
    {
        failures += check_level(vol, level);
    }

    vol->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-lock-slicepitch-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
