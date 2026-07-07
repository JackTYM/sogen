// Many-draws-per-scene performance + correctness test for the per-draw-overhead slice
// (commits 02b28ada, 0238dfd7, fcfccc00, 36b03142, 4b0bc778): a blocking fence-wait replacing the
// CPU-pinning busy-spin, plus per-pipeline descriptor-set pooling and per-device vertex/index/UBO
// buffer pooling in d3d9_host.cpp's execute_draw.
//
// The point of this test is twofold:
//
//   1. CORRECTNESS discriminator for the pooling. Within ONE BeginScene/EndScene it issues several
//      HUNDRED DrawIndexedPrimitive calls, ALL through the SAME cached programmable pipeline (same
//      vs_2_0/ps_2_0 pair, same 640x480 render-target shape, same 4-vertex/6-index unit-quad vertex
//      shape, same two constant UBOs) -- exactly the case the pooling optimizes: after the first draw
//      nothing is (re)allocated, the pooled VB/IB/descriptor-sets/UBOs are reused and only their
//      CONTENTS are rewritten per draw. Each draw fills a DISTINCT grid cell with a DISTINCT,
//      index-derived color, driven per-draw by a real, changing VS constant (c0 = cell NDC
//      offset+scale, so the pooled vertex data lands in a different place each draw) AND a real,
//      changing PS constant (c0 = cell color). If the pooling reused stale contents -- e.g. a later
//      draw saw an earlier draw's UBO bytes because the pool was rewritten before the GPU finished
//      reading it, or a buffer wasn't actually re-uploaded -- cells would show the WRONG color or land
//      in the WRONG place, and the final pixel readback (8 cells spread across the grid, each checked
//      against its own analytically-derived color) would fail. A pure timing number can't catch a
//      correctness regression; this can.
//
//   2. TIMING evidence for the mechanism. The whole draw loop is bracketed by QueryPerformanceCounter
//      and the elapsed wall-clock time is printed. Every draw is still FULLY SYNCHRONOUS (submit, then
//      block until the GPU completes, before the next draw starts) -- this slice did NOT add
//      multi-frame-in-flight pipelining, so the NUMBER of GPU round-trips per frame is unchanged. What
//      it removed is the per-round-trip COST: the busy-spin CPU pinning and the per-draw allocation
//      churn (a dozen-plus vkAllocateMemory/vkFreeMemory/buffer create+destroy pairs per draw, now
//      essentially zero after the first draw of a given shape). Printing the loop time before (pre-fix
//      host) and after (fixed host) gives a real, measured before/after for that per-round-trip cost.
//
// Vulkan-unflipped viewport convention (NDC y=-1 at the top row, y=+1 at the bottom -- see
// d3d9_texcoord_test.cpp / d3d9_texture_test.cpp) means readback row maps linearly to NDC y with no
// flip, so each cell's screen rect and its center-pixel readback location are computed the same way.
//
// Proven shape reuse: same D3DFVF_XYZ|D3DFVF_DIFFUSE vertex layout and PS-reads-c0 pattern as
// d3d9_const_test.cpp, driven through DrawIndexedPrimitive (4-vertex quad + 6-index buffer) so the
// pooled index buffer is exercised too, not just the vertex/constant pools.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; float4 color : COLOR0; };
struct VSOutput { float4 pos : POSITION; float4 color : COLOR0; };
float4 cellTransform : register(c0); // xy = NDC offset, zw = NDC scale
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos.x * cellTransform.z + cellTransform.x,
                        input.pos.y * cellTransform.w + cellTransform.y,
                        input.pos.z, 1.0);
    output.color = input.color;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 cellColor : register(c0);
float4 main(PSInput input) : COLOR0
{
    return cellColor;
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp), same as
    // the other programmable-pipeline tests.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // 32x24 = 768 distinct 20x20-pixel cells -> 768 DrawIndexedPrimitive calls in one scene. Both
    // dimensions divide the canvas exactly, so every cell is an integer screen rect.
    constexpr int kGridCols = 32;
    constexpr int kGridRows = 24;
    constexpr int kCellW = kCanvasWidth / kGridCols;  // 20
    constexpr int kCellH = kCanvasHeight / kGridRows; // 20
    constexpr int kDrawCount = kGridCols * kGridRows; // 768

    float to_ndc_x(const int screen_x) { return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f; }
    float to_ndc_y(const int screen_y) { return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f; }

    // Deterministic, index-derived color for cell (gx, gy). Adjacent cells differ by well more than the
    // +-2 channel tolerance in every channel (R steps ~8/col, G steps ~11/row, B steps by 13/col and
    // 7/row), so a stale-reuse bug that painted a cell with a neighbor's color is caught, not masked.
    void color_for(const int gx, const int gy, int& r, int& g, int& b)
    {
        r = gx * 255 / (kGridCols - 1);
        g = gy * 255 / (kGridRows - 1);
        b = (gx * 13 + gy * 7) % 200 + 40; // in [40, 239], distinct from the others
    }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    void release_all(IDirect3DIndexBuffer9* ib, IDirect3DVertexBuffer9* vb, IDirect3DSurface9* rt,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (ib)
        {
            ib->Release();
        }
        if (vb)
        {
            vb->Release();
        }
        if (rt)
        {
            rt->Release();
        }
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        if (dev)
        {
            dev->Release();
        }
        if (d3d)
        {
            d3d->Release();
        }
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-manydraws-test] start (%d draws in one scene)\n", kDrawCount);

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9manydrawstest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "manydraws-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-manydraws-test] FAIL: Direct3DCreate9 returned null\n");
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
    HRESULT hr =
        d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-manydraws-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-manydraws-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-manydraws-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-manydraws-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-manydraws-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-manydraws-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-manydraws-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Unit quad in cell-local space [0,1]x[0,1]; the VS maps it into a cell's NDC rect via c0.
    const Vertex kQuad[4] = {
        {0.0f, 0.0f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)},
        {1.0f, 0.0f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)},
        {1.0f, 1.0f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)},
        {0.0f, 1.0f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)},
    };
    const unsigned short kIndices[6] = {0, 1, 2, 0, 2, 3};

    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(sizeof(kQuad), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-manydraws-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (FAILED(hcvb) || !vb)
    {
        release_all(nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        void* dst = nullptr;
        vb->Lock(0, sizeof(kQuad), &dst, 0);
        if (dst)
        {
            std::memcpy(dst, kQuad, sizeof(kQuad));
            vb->Unlock();
        }
    }

    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hcib = dev->CreateIndexBuffer(sizeof(kIndices), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    printf("[d3d9-manydraws-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib),
           static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        release_all(nullptr, vb, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        void* dst = nullptr;
        ib->Lock(0, sizeof(kIndices), &dst, 0);
        if (dst)
        {
            std::memcpy(dst, kIndices, sizeof(kIndices));
            ib->Unlock();
        }
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE,
                                           &rt, nullptr);
    printf("[d3d9-manydraws-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        release_all(ib, vb, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    LARGE_INTEGER freq{};
    LARGE_INTEGER t0{};
    LARGE_INTEGER t1{};
    QueryPerformanceFrequency(&freq);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    QueryPerformanceCounter(&t0);
    for (int gy = 0; gy < kGridRows; ++gy)
    {
        for (int gx = 0; gx < kGridCols; ++gx)
        {
            const float x0 = to_ndc_x(gx * kCellW);
            const float x1 = to_ndc_x((gx + 1) * kCellW);
            const float y0 = to_ndc_y(gy * kCellH);
            const float y1 = to_ndc_y((gy + 1) * kCellH);
            const float cell_transform[4] = {x0, y0, x1 - x0, y1 - y0};
            dev->SetVertexShaderConstantF(0, cell_transform, 1);

            int r = 0;
            int g = 0;
            int b = 0;
            color_for(gx, gy, r, g, b);
            const float cell_color[4] = {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                                         static_cast<float>(b) / 255.0f, 1.0f};
            dev->SetPixelShaderConstantF(0, cell_color, 1);

            dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        }
    }
    QueryPerformanceCounter(&t1);

    dev->EndScene();

    const double elapsed_ms = 1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) / static_cast<double>(freq.QuadPart);
    printf("[d3d9-manydraws-test] TIMING: %d draws in %.3f ms (%.4f ms/draw)\n", kDrawCount, elapsed_ms,
           elapsed_ms / kDrawCount);

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-manydraws-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        // Check EVERY cell's center pixel, not a sample: a batching arena/descriptor-overlap bug corrupts
        // a CONTIGUOUS RUN of draws, and a sparse sample could miss a corrupted run entirely. Checking all
        // 768 guarantees detection regardless of which range, if any, is wrong. Only FAILs are printed
        // per-cell (768 PASS lines would drown the output); a final count reports full coverage.
        for (int gy = 0; gy < kGridRows; ++gy)
        {
            for (int gx = 0; gx < kGridCols; ++gx)
            {
                const int col = gx * kCellW + kCellW / 2;
                const int row = gy * kCellH + kCellH / 2;
                int r = 0;
                int g = 0;
                int b = 0;
                color_for(gx, gy, r, g, b);
                const unsigned char* p = pixel_at(col, row);
                if (!channel_close(p[0], b, 2) || !channel_close(p[1], g, 2) || !channel_close(p[2], r, 2))
                {
                    printf("[d3d9-manydraws-test] FAIL: cell(%d,%d) pixel(%d,%d)=B=%02X G=%02X R=%02X (expected "
                           "B=%02X G=%02X R=%02X) -- pooled per-draw content is stale or misplaced\n",
                           gx, gy, col, row, p[0], p[1], p[2], b, g, r);
                    ++failures;
                }
            }
        }
        printf("[d3d9-manydraws-test] checked all %d cell centers, %d passed, %d failed\n", kDrawCount,
               kDrawCount - failures, failures);
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-manydraws-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(ib, vb, rt, vs, ps, dev, d3d);

    printf("[d3d9-manydraws-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
