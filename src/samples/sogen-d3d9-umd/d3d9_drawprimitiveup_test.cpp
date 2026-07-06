// DrawPrimitiveUP / DrawIndexedPrimitiveUP regression test.
//
// Real d3d9.dll implements the "UP" (user-pointer) draws WITHOUT any dedicated UP DDI: it binds the
// app's user vertex array via pfnSetStreamSourceUm (device-func-table slot 7) and the user index
// array via pfnSetIndicesUm (slot 9), then reuses the ordinary pfnDrawPrimitive/pfnDrawIndexedPrimitive
// slot (RE-verified live on x64 AND x86). This test drives all three from the public D3D9 API with NO
// vertex buffer and NO index buffer created at all, then reads the rendered pixels back to prove the
// user-memory geometry actually reached the GPU.
//
// Fixed-function D3DFVF_XYZRHW|D3DFVF_DIFFUSE path only (screen-space pre-transformed geometry), same
// established pattern as d3d9_triangle_test.cpp / d3d9_scissor_test.cpp -- no shader compile involved.
//
// Sub-pass 1 (DrawPrimitiveUP): clear the 640x480 off-screen RT BLUE, then draw a RED right-triangle
//   from a 3-element user vertex array. Check a pixel inside the triangle is RED and two pixels outside
//   it stay BLUE.
// Sub-pass 2 (DrawIndexedPrimitiveUP, INDEX16): clear BLUE, then draw a GREEN quad from a 4-element
//   user vertex array + 6-element 16-bit user index array. Check inside=GREEN, corners=BLUE.
// Sub-pass 3 (DrawIndexedPrimitiveUP, INDEX32): identical geometry via a 32-bit user index array,
//   exercising the 4-byte index-element-size path.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    struct FvfVertex
    {
        float x, y, z, rhw;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp), same as
    // the other guest tests -- the render target is really 640x480 regardless of requested size.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

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

    constexpr int kRedB = 0, kRedG = 0, kRedR = 255;
    constexpr int kGreenB = 0, kGreenG = 255, kGreenR = 0;
    constexpr int kBlueB = 255, kBlueG = 0, kBlueR = 0;

    int run_checks(IDirect3DSurface9* rt, const char* pass_name, const Check* checks, const int check_count)
    {
        int failures = 0;
        D3DLOCKED_RECT lr{};
        HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-drawprimitiveup-test] %s LockRect hr=0x%08lx pBits=%p\n", pass_name, static_cast<unsigned long>(hlr),
               lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-drawprimitiveup-test] FAIL: %s LockRect failed\n", pass_name);
            return check_count;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        for (int i = 0; i < check_count; ++i)
        {
            const Check& c = checks[i];
            const unsigned char* p = pixel_at(c.col, c.row);
            printf("[d3d9-drawprimitiveup-test] %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name, c.name, c.col,
                   c.row, p[0], p[1], p[2], p[3]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) ||
                !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-drawprimitiveup-test] FAIL: %s %s does not match the expected color\n", pass_name, c.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-drawprimitiveup-test] PASS: %s %s matches the expected color\n", pass_name, c.name);
            }
        }

        rt->UnlockRect();
        return failures;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-drawprimitiveup-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9drawprimitiveuptest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "drawprimitiveup-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-drawprimitiveup-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-drawprimitiveup-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt =
        dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-drawprimitiveup-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-drawprimitiveup-test] FAIL: render target creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }
    dev->SetRenderTarget(0, rt);
    dev->SetFVF(kFvf);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    const DWORD red = D3DCOLOR_XRGB(255, 0, 0);
    const DWORD green = D3DCOLOR_XRGB(0, 255, 0);
    const DWORD blue = D3DCOLOR_XRGB(0, 0, 255);

    int failures = 0;

    // Sub-pass 1: DrawPrimitiveUP -- a RED right-triangle straight from a user vertex array, no VB.
    {
        FvfVertex tri[3] = {
            {100.0f, 100.0f, 0.5f, 1.0f, red},
            {300.0f, 100.0f, 0.5f, 1.0f, red},
            {100.0f, 300.0f, 0.5f, 1.0f, red},
        };

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, blue, 1.0f, 0);
        HRESULT hd = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(FvfVertex));
        printf("[d3d9-drawprimitiveup-test] DrawPrimitiveUP hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        // (150,150) is well inside the triangle; (250,250) is past the hypotenuse (x+y=400) and (10,10)
        // is left of the vertical edge -- both must remain the BLUE clear color.
        const Check checks[3] = {
            {"inside triangle", 150, 150, kRedB, kRedG, kRedR},
            {"outside past hypotenuse", 250, 250, kBlueB, kBlueG, kBlueR},
            {"outside left of edge", 10, 10, kBlueB, kBlueG, kBlueR},
        };
        failures += run_checks(rt, "DrawPrimitiveUP", checks, 3);
    }

    // Shared GREEN quad geometry for the two indexed sub-passes: a rectangle in the right-center region.
    FvfVertex quad[4] = {
        {350.0f, 100.0f, 0.5f, 1.0f, green},
        {550.0f, 100.0f, 0.5f, 1.0f, green},
        {550.0f, 300.0f, 0.5f, 1.0f, green},
        {350.0f, 300.0f, 0.5f, 1.0f, green},
    };
    const Check quad_checks[3] = {
        {"inside quad", 450, 200, kGreenB, kGreenG, kGreenR},
        {"corner (100,100)", 100, 100, kBlueB, kBlueG, kBlueR},
        {"corner (600,400)", 600, 400, kBlueB, kBlueG, kBlueR},
    };

    // Sub-pass 2: DrawIndexedPrimitiveUP with 16-bit user indices, no VB and no IB.
    {
        unsigned short idx16[6] = {0, 1, 2, 0, 2, 3};

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, blue, 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, idx16, D3DFMT_INDEX16, quad, sizeof(FvfVertex));
        printf("[d3d9-drawprimitiveup-test] DrawIndexedPrimitiveUP(INDEX16) hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += run_checks(rt, "DrawIndexedPrimitiveUP-INDEX16", quad_checks, 3);
    }

    // Sub-pass 3: DrawIndexedPrimitiveUP with 32-bit user indices (distinct index-element-size path).
    {
        unsigned int idx32[6] = {0, 1, 2, 0, 2, 3};

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, blue, 1.0f, 0);
        HRESULT hd = dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, idx32, D3DFMT_INDEX32, quad, sizeof(FvfVertex));
        printf("[d3d9-drawprimitiveup-test] DrawIndexedPrimitiveUP(INDEX32) hr=0x%08lx\n", static_cast<unsigned long>(hd));
        dev->EndScene();

        failures += run_checks(rt, "DrawIndexedPrimitiveUP-INDEX32", quad_checks, 3);
    }

    rt->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-drawprimitiveup-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
