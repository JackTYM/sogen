// D3D9-over-Vulkan partial-buffer Lock test (Task 6, .claude/plans/jazzy-giggling-cloud.md).
//
// Proves a real D3DLOCK_NOOVERWRITE-style partial lock on a growing dynamic vertex buffer only
// touches the sub-range the app actually requested, instead of silently getting whole-buffer
// semantics. Simulates the common "append to a growing dynamic buffer" pattern: fills chunk 0 with a
// D3DLOCK_DISCARD lock, then appends chunks 1 and 2 with D3DLOCK_NOOVERWRITE locks at increasing
// offsets, each writing a distinctive byte pattern. A final whole-buffer read-only Lock verifies every
// chunk still holds exactly its own pattern -- in particular that chunk 0 (the "untouched earlier
// region") was not disturbed by the later locks at nonzero offsets. This is a pure D3D9-API-level
// check (no host-side backdoor): with the DDI's real OffsetToLock always mapped to 0 (the pre-fix
// bug), the later locks would silently return a pointer based at the buffer's start instead of the
// requested offset, so the app's own writes would corrupt chunk 0 and this test would fail.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr UINT kChunkSize = 256;
    constexpr UINT kChunkCount = 3;
    constexpr UINT kTotalSize = kChunkSize * kChunkCount;
    constexpr unsigned char kChunkPattern[kChunkCount] = {0xAA, 0xBB, 0xCC};

    bool lock_and_fill(IDirect3DVertexBuffer9* vb, UINT offset, DWORD flags, unsigned char pattern, const char* label)
    {
        void* p = nullptr;
        HRESULT hr = vb->Lock(offset, kChunkSize, &p, flags);
        printf("[d3d9-partial-lock-test] Lock(%s) offset=%u size=%u hr=0x%08lx pData=%p\n", label, offset, kChunkSize,
               static_cast<unsigned long>(hr), p);
        if (FAILED(hr) || !p)
        {
            printf("[d3d9-partial-lock-test] FAIL: Lock(%s) failed\n", label);
            return false;
        }
        std::memset(p, pattern, kChunkSize);
        vb->Unlock();
        return true;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-partial-lock-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9partiallocktest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "partial-lock-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640,
                                 480, nullptr, nullptr, wc.hInstance, nullptr);
    printf("[d3d9-partial-lock-test] hwnd=%p\n", static_cast<void*>(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-partial-lock-test] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-partial-lock-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-partial-lock-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-partial-lock-test] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    // D3DUSAGE_DYNAMIC + D3DPOOL_DEFAULT is the same real-driver-routed pattern every other test in
    // this suite already uses to route through the DevCaps-gated driver path instead of the sysmem
    // fast path (see fill_d3d9caps's k_devcaps_driver_managed_pool).
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb =
        dev->CreateVertexBuffer(kTotalSize, D3DUSAGE_DYNAMIC, D3DFVF_XYZ, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-partial-lock-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (FAILED(hcvb) || !vb)
    {
        printf("[d3d9-partial-lock-test] FAIL: CreateVertexBuffer hr=0x%08lx\n", static_cast<unsigned long>(hcvb));
        dev->Release();
        d3d->Release();
        return 1;
    }

    int failures = 0;

    // Chunk 0: whole-buffer-style initial fill (offset 0, DISCARD) -- this is the "earlier region"
    // that must survive the later, higher-offset appends untouched.
    if (!lock_and_fill(vb, 0, D3DLOCK_DISCARD, kChunkPattern[0], "chunk0-discard"))
    {
        ++failures;
    }
    // Chunk 1 and 2: growing-buffer tail appends at increasing nonzero offsets, D3DLOCK_NOOVERWRITE --
    // the exact real-world pattern that silently got whole-buffer semantics before this fix.
    if (!lock_and_fill(vb, kChunkSize, D3DLOCK_NOOVERWRITE, kChunkPattern[1], "chunk1-noverwrite"))
    {
        ++failures;
    }
    if (!lock_and_fill(vb, 2 * kChunkSize, D3DLOCK_NOOVERWRITE, kChunkPattern[2], "chunk2-noverwrite"))
    {
        ++failures;
    }

    void* readback = nullptr;
    HRESULT hlr = vb->Lock(0, kTotalSize, &readback, D3DLOCK_READONLY);
    printf("[d3d9-partial-lock-test] final readback Lock hr=0x%08lx pData=%p\n", static_cast<unsigned long>(hlr), readback);
    if (FAILED(hlr) || !readback)
    {
        printf("[d3d9-partial-lock-test] FAIL: final readback Lock failed\n");
        ++failures;
    }
    else
    {
        const auto* bytes = static_cast<const unsigned char*>(readback);
        for (UINT chunk = 0; chunk < kChunkCount; ++chunk)
        {
            const unsigned char expected = kChunkPattern[chunk];
            const unsigned char* region = bytes + chunk * kChunkSize;
            bool all_match = true;
            UINT first_bad_index = 0;
            unsigned char first_bad_value = 0;
            for (UINT i = 0; i < kChunkSize; ++i)
            {
                if (region[i] != expected)
                {
                    all_match = false;
                    first_bad_index = i;
                    first_bad_value = region[i];
                    break;
                }
            }
            printf("[d3d9-partial-lock-test] chunk%u expected=0x%02X first_byte=0x%02X %s\n", chunk, expected,
                   region[0], all_match ? "MATCH" : "MISMATCH");
            if (!all_match)
            {
                printf("[d3d9-partial-lock-test] FAIL: chunk%u byte %u is 0x%02X, expected 0x%02X\n", chunk,
                       first_bad_index, first_bad_value, expected);
                ++failures;
            }
            else
            {
                printf("[d3d9-partial-lock-test] PASS: chunk%u intact\n", chunk);
            }
        }
        vb->Unlock();
    }

    vb->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-partial-lock-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
