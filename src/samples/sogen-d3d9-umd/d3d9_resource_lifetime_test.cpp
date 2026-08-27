// D3D9-over-Vulkan resource-lifetime (create/destroy churn) test.
//
// Exercises the pfnDestroyResource path the UMD used to leave unwired: hundreds of round trips of
// "create a texture + a dynamic vertex buffer, write to them, release them". With the DDI slot
// stubbed out the host never learned a resource was gone, so every iteration permanently added a
// host resource, a VkImage plus its device memory, and (for the dynamic buffer) a slice of the
// 32-bit guest address space aliasing its direct-mapped ring.
//
// The API-level assertion here is that a resource created AFTER hundreds of create/destroy cycles
// still works: it locks, keeps the bytes written into it, and reads them back. That covers the
// stale-lookup failure mode a destroy path can introduce (a released handle's guest-side entry left
// behind, or a later handle resolving onto a host resource that has already been freed).
//
// The leak itself is not observable through the D3D9 API at all, so it is measured out of band:
// run under EMULATOR_D3D9_RESLIFE_DIAG=1 and compare the live_resources / direct_va_bytes numbers
// at the end of the churn loop against the start. Fixed, they return to the pre-loop level; leaking,
// they climb once per iteration and never come back down.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr UINT kIterations = 200;
    constexpr UINT kTextureDim = 32;
    constexpr UINT kBufferSize = 1024;

    // One churn iteration: both resource kinds this UMD tracks differently -- a texture (registered in
    // the guest UMD's g_created_resource_ids) and a D3DUSAGE_DYNAMIC/D3DPOOL_DEFAULT vertex buffer (the
    // direct-mapped-ring kind, registered in g_resource_ids + g_direct_buffers). Each is locked and
    // written so it is fully realized host-side, not just declared, before being released.
    bool churn_once(IDirect3DDevice9* dev, UINT iteration)
    {
        IDirect3DTexture9* tex = nullptr;
        HRESULT hr = dev->CreateTexture(kTextureDim, kTextureDim, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
        if (FAILED(hr) || !tex)
        {
            printf("[d3d9-resource-lifetime-test] FAIL: CreateTexture failed at iteration %u hr=0x%08lx\n", iteration,
                   static_cast<unsigned long>(hr));
            return false;
        }

        D3DLOCKED_RECT rect{};
        hr = tex->LockRect(0, &rect, nullptr, 0);
        if (FAILED(hr) || !rect.pBits)
        {
            printf("[d3d9-resource-lifetime-test] FAIL: texture LockRect failed at iteration %u hr=0x%08lx\n", iteration,
                   static_cast<unsigned long>(hr));
            tex->Release();
            return false;
        }
        std::memset(rect.pBits, static_cast<int>(iteration & 0xFF), kTextureDim * kTextureDim * 4);
        tex->UnlockRect(0);

        IDirect3DVertexBuffer9* vb = nullptr;
        hr = dev->CreateVertexBuffer(kBufferSize, D3DUSAGE_DYNAMIC, D3DFVF_XYZ, D3DPOOL_DEFAULT, &vb, nullptr);
        if (FAILED(hr) || !vb)
        {
            printf("[d3d9-resource-lifetime-test] FAIL: CreateVertexBuffer failed at iteration %u hr=0x%08lx\n", iteration,
                   static_cast<unsigned long>(hr));
            tex->Release();
            return false;
        }

        void* data = nullptr;
        hr = vb->Lock(0, kBufferSize, &data, D3DLOCK_DISCARD);
        if (FAILED(hr) || !data)
        {
            printf("[d3d9-resource-lifetime-test] FAIL: vertex buffer Lock failed at iteration %u hr=0x%08lx\n", iteration,
                   static_cast<unsigned long>(hr));
            vb->Release();
            tex->Release();
            return false;
        }
        std::memset(data, static_cast<int>(iteration & 0xFF), kBufferSize);
        vb->Unlock();

        vb->Release();
        tex->Release();
        return true;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-resource-lifetime-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9resourcelifetimetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "resource-lifetime-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, nullptr,
                                nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-resource-lifetime-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-resource-lifetime-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-resource-lifetime-test] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    int failures = 0;

    printf("[d3d9-resource-lifetime-test] churn loop begin (%u iterations)\n", kIterations);
    UINT completed = 0;
    for (UINT i = 0; i < kIterations; ++i)
    {
        if (!churn_once(dev, i))
        {
            ++failures;
            break;
        }
        ++completed;
    }
    printf("[d3d9-resource-lifetime-test] churn loop end (%u/%u iterations completed)\n", completed, kIterations);
    if (completed == kIterations)
    {
        printf("[d3d9-resource-lifetime-test] PASS: all %u create/destroy iterations succeeded\n", kIterations);
    }
    else
    {
        printf("[d3d9-resource-lifetime-test] FAIL: churn stopped after %u of %u iterations\n", completed, kIterations);
    }

    // Post-churn usability: a brand new resource created after all those destroys must still round-trip
    // its own bytes. A stale guest-side handle entry, or a handle resolving onto an already-freed host
    // resource, shows up here as a readback mismatch rather than a crash.
    constexpr unsigned char kSentinel = 0x5A;
    IDirect3DVertexBuffer9* final_vb = nullptr;
    hr = dev->CreateVertexBuffer(kBufferSize, D3DUSAGE_DYNAMIC, D3DFVF_XYZ, D3DPOOL_DEFAULT, &final_vb, nullptr);
    if (FAILED(hr) || !final_vb)
    {
        printf("[d3d9-resource-lifetime-test] FAIL: post-churn CreateVertexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hr));
        ++failures;
    }
    else
    {
        void* data = nullptr;
        hr = final_vb->Lock(0, kBufferSize, &data, D3DLOCK_DISCARD);
        if (FAILED(hr) || !data)
        {
            printf("[d3d9-resource-lifetime-test] FAIL: post-churn Lock hr=0x%08lx\n", static_cast<unsigned long>(hr));
            ++failures;
        }
        else
        {
            std::memset(data, kSentinel, kBufferSize);
            final_vb->Unlock();

            void* readback = nullptr;
            hr = final_vb->Lock(0, kBufferSize, &readback, D3DLOCK_READONLY);
            if (FAILED(hr) || !readback)
            {
                printf("[d3d9-resource-lifetime-test] FAIL: post-churn readback Lock hr=0x%08lx\n", static_cast<unsigned long>(hr));
                ++failures;
            }
            else
            {
                const auto* bytes = static_cast<const unsigned char*>(readback);
                UINT bad = 0;
                for (UINT i = 0; i < kBufferSize; ++i)
                {
                    if (bytes[i] != kSentinel)
                    {
                        ++bad;
                    }
                }
                final_vb->Unlock();
                if (bad == 0)
                {
                    printf("[d3d9-resource-lifetime-test] PASS: post-churn buffer round-tripped all %u bytes\n", kBufferSize);
                }
                else
                {
                    printf("[d3d9-resource-lifetime-test] FAIL: post-churn buffer has %u/%u wrong bytes (first=0x%02X)\n", bad, kBufferSize,
                           bytes[0]);
                    ++failures;
                }
            }
        }
        final_vb->Release();
    }

    dev->Release();
    d3d->Release();

    printf("[d3d9-resource-lifetime-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
