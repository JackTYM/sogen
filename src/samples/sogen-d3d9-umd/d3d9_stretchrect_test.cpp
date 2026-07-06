// IDirect3DDevice9::StretchRect guest regression test. Proves the real d3d9.dll StretchRect path
// (device-func-table slot 55 -> pfnBlt -> D3DDDIARG_BLT, RE'd in d3d9_ddi.hpp) reaches the sogen UMD,
// streams a blt record to the host, and blits a source render target into a destination render target
// via vkCmdBlitImage -- both as a same-size 1:1 copy and (caps permitting) a genuinely scaled stretch.
//
// Source content is produced by a REAL draw (fixed-function D3DFVF_XYZRHW|D3DFVF_DIFFUSE, no shader),
// so this test does not depend on ColorFill: a 640x480 src RT is cleared BLUE and a RED quad is drawn
// covering only the LEFT HALF (x in [0,320]). That gives a spatially distinctive src: RED left,
// BLUE right.
//
// Sub-pass A (same-size 1:1 copy): dst cleared GREEN, StretchRect(src whole -> dst whole, POINT). dst
// must become RED on the left, BLUE on the right -- exactly mirroring src. A flipped/offset/whole-image
// wrong copy would miss a checkpoint. (480,240) reading BLUE here is the key vs sub-pass B.
//
// Sub-pass B (2x horizontal scale): dst cleared GREEN, StretchRect(src {0,0,320,480} (the RED half) ->
// dst {0,0,640,480} (whole), POINT). The 320px-wide RED region is stretched to fill the whole 640px
// dst, so dst must be RED everywhere -- crucially (480,240) is now RED, the discriminator proving a
// genuine horizontal magnification occurred (not a plain copy). Only exercised if the scaled
// StretchRect is accepted by the runtime (see StretchRectFilterCaps in sogen_d3d9_umd.cpp); the result
// HRESULT is reported either way.
//
// LockRect returns BGRA8 (byte0=B, byte1=G, byte2=R, byte3=A).

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

    int run_checks(IDirect3DSurface9* rt, const char* pass_name, const Check* checks, const int check_count)
    {
        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-stretchrect-test] %s LockRect hr=0x%08lx pBits=%p\n", pass_name, static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-stretchrect-test] FAIL: %s LockRect failed\n", pass_name);
            return check_count;
        }

        // The sogen UMD's pfnLock does not populate D3DLOCKED_RECT::Pitch, so use the surface's known
        // tightly-packed BGRA8 row stride -- the same convention every other guest test here uses.
        constexpr LONG kStride = kCanvasWidth * 4;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        auto pixel_at = [&](const int col, const int row) { return base + static_cast<size_t>(row) * kStride + col * 4; };

        int failures = 0;
        for (int i = 0; i < check_count; ++i)
        {
            const Check& c = checks[i];
            const unsigned char* p = pixel_at(c.col, c.row);
            printf("[d3d9-stretchrect-test] %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name, c.name, c.col, c.row,
                   p[0], p[1], p[2], p[3]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) ||
                !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-stretchrect-test] FAIL: %s %s does not match expected B=%02X G=%02X R=%02X\n", pass_name, c.name,
                       c.expected_b, c.expected_g, c.expected_r);
                ++failures;
            }
            else
            {
                printf("[d3d9-stretchrect-test] PASS: %s %s matches the expected color\n", pass_name, c.name);
            }
        }

        rt->UnlockRect();
        return failures;
    }

    // Same-size copy: dst mirrors src (RED left, BLUE right).
    const Check kCopyChecks[] = {
        {"left half (RED from src)", 160, 240, kRedB, kRedG, kRedR},
        {"right half (BLUE from src)", 480, 240, kBlueB, kBlueG, kBlueR},
        {"top-left corner (RED)", 10, 10, kRedB, kRedG, kRedR},
        {"bottom-right corner (BLUE)", 630, 470, kBlueB, kBlueG, kBlueR},
    };

    // 2x horizontal stretch of the RED left half: dst is RED everywhere. (480,240) RED is the
    // discriminator against the same-size copy above (where it was BLUE).
    const Check kScaledChecks[] = {
        {"left (RED)", 160, 240, kRedB, kRedG, kRedR},
        {"right, was BLUE in 1:1 copy, now RED (proves scaling)", 480, 240, kRedB, kRedG, kRedR},
        {"far-right corner (RED)", 630, 470, kRedB, kRedG, kRedR},
    };
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-stretchrect-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9stretchrecttest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "stretchrect-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-stretchrect-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-stretchrect-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* src = nullptr;
    IDirect3DSurface9* dst = nullptr;
    HRESULT hcs =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &src, nullptr);
    HRESULT hcd =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &dst, nullptr);
    printf("[d3d9-stretchrect-test] CreateRenderTarget src hr=0x%08lx dst hr=0x%08lx\n", static_cast<unsigned long>(hcs),
           static_cast<unsigned long>(hcd));
    if (FAILED(hcs) || FAILED(hcd) || !src || !dst)
    {
        printf("[d3d9-stretchrect-test] FAIL: render target creation failed\n");
        if (src) src->Release();
        if (dst) dst->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    // RED quad covering the LEFT HALF only (x in [0,320]).
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(FvfVertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        FvfVertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(FvfVertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            const DWORD red = D3DCOLOR_XRGB(255, 0, 0);
            const float half = static_cast<float>(kCanvasWidth) / 2.0f;
            v[0] = {0.0f, 0.0f, 0.5f, 1.0f, red};
            v[1] = {half, 0.0f, 0.5f, 1.0f, red};
            v[2] = {half, static_cast<float>(kCanvasHeight), 0.5f, 1.0f, red};
            v[3] = {0.0f, static_cast<float>(kCanvasHeight), 0.5f, 1.0f, red};
            vb->Unlock();
        }
    }

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    if (ib)
    {
        WORD* idx = nullptr;
        ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0);
        if (idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
    }

    if (!vb || !ib)
    {
        printf("[d3d9-stretchrect-test] FAIL: buffer creation failed\n");
        if (ib) ib->Release();
        if (vb) vb->Release();
        src->Release();
        dst->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(FvfVertex));
    dev->SetIndices(ib);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    // Render distinctive content into src: BLUE clear + RED left-half quad.
    dev->SetRenderTarget(0, src);
    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
    HRESULT hdraw = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    dev->EndScene();
    printf("[d3d9-stretchrect-test] source DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdraw));

    int failures = 0;

    // Sub-pass A: same-size 1:1 copy (whole surface, NULL rects).
    {
        dev->SetRenderTarget(0, dst);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 255, 0), 1.0f, 0);
        dev->EndScene();

        HRESULT hb = dev->StretchRect(src, nullptr, dst, nullptr, D3DTEXF_POINT);
        printf("[d3d9-stretchrect-test] StretchRect(same-size, whole->whole) hr=0x%08lx\n", static_cast<unsigned long>(hb));
        if (FAILED(hb))
        {
            printf("[d3d9-stretchrect-test] FAIL: same-size StretchRect returned an error\n");
            ++failures;
        }
        else
        {
            failures += run_checks(dst, "same-size-copy", kCopyChecks, static_cast<int>(sizeof(kCopyChecks) / sizeof(kCopyChecks[0])));
        }
    }

    // Sub-pass B: 2x horizontal stretch of the RED left half into the whole dst.
    {
        dev->SetRenderTarget(0, dst);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 255, 0), 1.0f, 0);
        dev->EndScene();

        RECT src_rect{0, 0, kCanvasWidth / 2, kCanvasHeight};
        RECT dst_rect{0, 0, kCanvasWidth, kCanvasHeight};
        HRESULT hb = dev->StretchRect(src, &src_rect, dst, &dst_rect, D3DTEXF_POINT);
        printf("[d3d9-stretchrect-test] StretchRect(2x-horizontal-scale, {0,0,320,480}->{0,0,640,480}) hr=0x%08lx\n",
               static_cast<unsigned long>(hb));
        if (FAILED(hb))
        {
            // Scaled StretchRect not accepted by the runtime -- report but do not fail the suite; the
            // same-size copy above is the core proof, scaling is the caps-gated extension.
            printf("[d3d9-stretchrect-test] NOTE: scaled StretchRect returned 0x%08lx (not exercised); "
                   "same-size copy remains the primary result\n",
                   static_cast<unsigned long>(hb));
        }
        else
        {
            failures += run_checks(dst, "scaled-copy", kScaledChecks,
                                   static_cast<int>(sizeof(kScaledChecks) / sizeof(kScaledChecks[0])));
        }
    }

    ib->Release();
    vb->Release();
    src->Release();
    dst->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-stretchrect-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
