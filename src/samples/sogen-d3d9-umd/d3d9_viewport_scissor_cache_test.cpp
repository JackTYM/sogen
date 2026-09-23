// Viewport/scissor dynamic-state caching regression test. execute_draw skips vkCmdSetViewport /
// vkCmdSetScissor when the rect it would issue matches what the currently-recording batch already
// has; this test drives the two ways that skip can be wrong.
//
// Sub-pass 1 (two viewports, one batch): two draws into the same render target with nothing between
// them that closes the batch, each with its own viewport. A cache that never re-issues within a
// batch paints the second draw through the first draw's viewport.
//
// Sub-pass 2 (two scissor rects, one batch): the same shape for D3DRS_SCISSORTESTENABLE +
// SetScissorRect.
//
// Sub-pass 3 (batch-slot rotation): three draws with the IDENTICAL viewport and scissor rect,
// alternating between two render targets. Each render-target change closes the open batch and
// rotates to the other slot's command buffer, and a command buffer starts with its dynamic state
// undefined -- so a rect unchanged since the previous batch still has to be re-issued. A cache keyed
// only on the value, without invalidating on batch open, skips all but the first draw's setup and
// renders the last two through whatever the driver defaults an unset viewport/scissor to.
//
// Programmable vs_2_0/ps_2_0 emitting clip-space positions directly (not D3DFVF_XYZRHW), so the
// viewport rect is the only thing deciding where the quad lands.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; float4 color : COLOR0; };
struct VSOutput { float4 pos : POSITION; float4 color : COLOR0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.color = input.color;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return input.color;
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp).
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Public D3DRENDERSTATETYPE value (d3d9types.h) -- not otherwise declared by this test's headers.
    constexpr D3DRENDERSTATETYPE kD3DRS_SCISSORTESTENABLE = static_cast<D3DRENDERSTATETYPE>(174);

    constexpr int kRedB = 0, kRedG = 0, kRedR = 255;
    constexpr int kGreenB = 0, kGreenG = 255, kGreenR = 0;
    constexpr int kWhiteB = 255, kWhiteG = 255, kWhiteR = 255;
    constexpr int kBlueB = 255, kBlueG = 0, kBlueR = 0;

    struct Check
    {
        const char* name;
        int col, row;
        int expected_b, expected_g, expected_r;
    };

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    int run_checks(IDirect3DSurface9* rt, const char* pass_name, const Check* checks, const int check_count)
    {
        int failures = 0;
        D3DLOCKED_RECT lr{};
        const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-viewport-scissor-cache-test] %s LockRect hr=0x%08lx pBits=%p\n", pass_name, static_cast<unsigned long>(hlr),
               lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-viewport-scissor-cache-test] FAIL: %s LockRect failed\n", pass_name);
            return check_count;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        for (int i = 0; i < check_count; ++i)
        {
            const Check& c = checks[i];
            const unsigned char* p = base + c.row * kStride + c.col * 4;
            printf("[d3d9-viewport-scissor-cache-test] %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X\n", pass_name, c.name, c.col, c.row, p[0],
                   p[1], p[2]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) || !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-viewport-scissor-cache-test] FAIL: %s %s does not match the expected color\n", pass_name, c.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-viewport-scissor-cache-test] PASS: %s %s matches the expected color\n", pass_name, c.name);
            }
        }

        rt->UnlockRect();
        return failures;
    }

    IDirect3DVertexBuffer9* make_quad(IDirect3DDevice9* dev, const DWORD color)
    {
        IDirect3DVertexBuffer9* vb = nullptr;
        if (FAILED(dev->CreateVertexBuffer(6 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr)) || !vb)
        {
            return nullptr;
        }
        Vertex* v = nullptr;
        if (SUCCEEDED(vb->Lock(0, 6 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0)) && v)
        {
            // Full clip-space quad: the viewport transform alone decides which pixels it covers.
            v[0] = {-1.0f, -1.0f, 0.5f, color};
            v[1] = {-1.0f, 1.0f, 0.5f, color};
            v[2] = {1.0f, 1.0f, 0.5f, color};
            v[3] = {-1.0f, -1.0f, 0.5f, color};
            v[4] = {1.0f, 1.0f, 0.5f, color};
            v[5] = {1.0f, -1.0f, 0.5f, color};
            vb->Unlock();
        }
        return vb;
    }

    void set_viewport(IDirect3DDevice9* dev, const DWORD x, const DWORD y, const DWORD w, const DWORD h)
    {
        D3DVIEWPORT9 vp{};
        vp.X = x;
        vp.Y = y;
        vp.Width = w;
        vp.Height = h;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        dev->SetViewport(&vp);
    }

    // Sub-pass 1/2 paint the top-left quadrant red and the bottom-right quadrant green over a blue
    // clear; the other two quadrants must stay blue.
    const Check kQuadrantChecks[4] = {
        {"top-left quadrant", 80, 60, kRedB, kRedG, kRedR},
        {"bottom-right quadrant", 560, 420, kGreenB, kGreenG, kGreenR},
        {"top-right quadrant (untouched)", 560, 60, kBlueB, kBlueG, kBlueR},
        {"bottom-left quadrant (untouched)", 80, 420, kBlueB, kBlueG, kBlueR},
    };

    // Sub-pass 3 draws through viewport {0,0,320,240} intersected with scissor {0,0,160,240}, so only
    // the left half of the top-left quadrant is painted.
    const Check kRotationChecksA[4] = {
        {"inside viewport and scissor", 80, 120, kWhiteB, kWhiteG, kWhiteR},
        {"inside viewport, outside scissor", 240, 120, kBlueB, kBlueG, kBlueR},
        {"outside viewport (below)", 80, 360, kBlueB, kBlueG, kBlueR},
        {"outside viewport (right)", 560, 60, kBlueB, kBlueG, kBlueR},
    };

    const Check kRotationChecksB[4] = {
        {"inside viewport and scissor", 80, 120, kGreenB, kGreenG, kGreenR},
        {"inside viewport, outside scissor", 240, 120, kBlueB, kBlueG, kBlueR},
        {"outside viewport (below)", 80, 360, kBlueB, kBlueG, kBlueR},
        {"outside viewport (right)", 560, 60, kBlueB, kBlueG, kBlueR},
    };
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-viewport-scissor-cache-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9vpscissorcachetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "viewport-scissor-cache-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-viewport-scissor-cache-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-viewport-scissor-cache-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    const HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                                    &vs_blob, &vs_errors);
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    const HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0,
                                    &ps_blob, &ps_errors);
    printf("[d3d9-viewport-scissor-cache-test] D3DCompile vs=0x%08lx ps=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-viewport-scissor-cache-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
        }
        if (ps_errors)
        {
            printf("[d3d9-viewport-scissor-cache-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    const HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    const HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    printf("[d3d9-viewport-scissor-cache-test] CreateVertexShader hr=0x%08lx CreatePixelShader hr=0x%08lx\n",
           static_cast<unsigned long>(hcvs), static_cast<unsigned long>(hcps));
    if (FAILED(hcvs) || FAILED(hcps) || !vs || !ps)
    {
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DSurface9* rt_a = nullptr;
    IDirect3DSurface9* rt_b = nullptr;
    const HRESULT hca = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt_a, nullptr);
    const HRESULT hcb = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt_b, nullptr);
    printf("[d3d9-viewport-scissor-cache-test] CreateRenderTarget a=0x%08lx b=0x%08lx\n", static_cast<unsigned long>(hca),
           static_cast<unsigned long>(hcb));
    if (FAILED(hca) || FAILED(hcb) || !rt_a || !rt_b)
    {
        printf("[d3d9-viewport-scissor-cache-test] FAIL: render target creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexBuffer9* vb_red = make_quad(dev, D3DCOLOR_XRGB(255, 0, 0));
    IDirect3DVertexBuffer9* vb_green = make_quad(dev, D3DCOLOR_XRGB(0, 255, 0));
    IDirect3DVertexBuffer9* vb_white = make_quad(dev, D3DCOLOR_XRGB(255, 255, 255));
    if (!vb_red || !vb_green || !vb_white)
    {
        printf("[d3d9-viewport-scissor-cache-test] FAIL: vertex buffer creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    const DWORD kBlue = D3DCOLOR_XRGB(0, 0, 255);
    int failures = 0;

    // The host's per-batch vertex/uniform arena grows by doubling, and its first growth closes and
    // reopens the batch between the two draws that trigger it -- which would hand the sub-passes below
    // a fresh command buffer mid-scene and mask an in-batch caching bug. One two-draw scene of the same
    // shape settles the arena (and the shader/pipeline caches) before anything is checked.
    {
        dev->SetRenderTarget(0, rt_a);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, kBlue, 1.0f, 0);
        dev->SetStreamSource(0, vb_red, 0, sizeof(Vertex));
        dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        dev->SetStreamSource(0, vb_green, 0, sizeof(Vertex));
        dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        dev->EndScene();
        D3DLOCKED_RECT warmup_lr{};
        if (SUCCEEDED(rt_a->LockRect(&warmup_lr, nullptr, D3DLOCK_READONLY)))
        {
            rt_a->UnlockRect();
        }
    }

    {
        dev->SetRenderTarget(0, rt_a);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, kBlue, 1.0f, 0);
        set_viewport(dev, 0, 0, kCanvasWidth / 2, kCanvasHeight / 2);
        dev->SetStreamSource(0, vb_red, 0, sizeof(Vertex));
        dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        set_viewport(dev, kCanvasWidth / 2, kCanvasHeight / 2, kCanvasWidth / 2, kCanvasHeight / 2);
        dev->SetStreamSource(0, vb_green, 0, sizeof(Vertex));
        dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        dev->EndScene();
        failures += run_checks(rt_a, "two-viewports-one-batch", kQuadrantChecks, 4);
    }

    {
        dev->SetRenderTarget(0, rt_a);
        set_viewport(dev, 0, 0, kCanvasWidth, kCanvasHeight);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, kBlue, 1.0f, 0);
        dev->SetRenderState(kD3DRS_SCISSORTESTENABLE, TRUE);
        RECT top_left{0, 0, kCanvasWidth / 2, kCanvasHeight / 2};
        dev->SetScissorRect(&top_left);
        dev->SetStreamSource(0, vb_red, 0, sizeof(Vertex));
        dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        RECT bottom_right{kCanvasWidth / 2, kCanvasHeight / 2, kCanvasWidth, kCanvasHeight};
        dev->SetScissorRect(&bottom_right);
        dev->SetStreamSource(0, vb_green, 0, sizeof(Vertex));
        dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        dev->SetRenderState(kD3DRS_SCISSORTESTENABLE, FALSE);
        dev->EndScene();
        failures += run_checks(rt_a, "two-scissors-one-batch", kQuadrantChecks, 4);
    }

    {
        // SetRenderTarget resets both the viewport and the scissor rect to the new target's full
        // extent, so every draw below re-states the identical rects it wants -- which is exactly the
        // input a value-only cache would wrongly treat as already bound.
        RECT left_half_of_quadrant{0, 0, kCanvasWidth / 4, kCanvasHeight / 2};
        auto draw_through_rects = [&](IDirect3DVertexBuffer9* vb) {
            set_viewport(dev, 0, 0, kCanvasWidth / 2, kCanvasHeight / 2);
            dev->SetScissorRect(&left_half_of_quadrant);
            dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
            dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        };

        dev->BeginScene();
        dev->SetRenderTarget(0, rt_a);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, kBlue, 1.0f, 0);
        dev->SetRenderTarget(0, rt_b);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, kBlue, 1.0f, 0);
        dev->SetRenderState(kD3DRS_SCISSORTESTENABLE, TRUE);

        dev->SetRenderTarget(0, rt_a);
        draw_through_rects(vb_red);
        dev->SetRenderTarget(0, rt_b);
        draw_through_rects(vb_green);
        dev->SetRenderTarget(0, rt_a);
        draw_through_rects(vb_white);

        dev->SetRenderState(kD3DRS_SCISSORTESTENABLE, FALSE);
        dev->EndScene();

        failures += run_checks(rt_a, "batch-rotation-target-a", kRotationChecksA, 4);
        failures += run_checks(rt_b, "batch-rotation-target-b", kRotationChecksB, 4);
    }

    vb_white->Release();
    vb_green->Release();
    vb_red->Release();
    rt_b->Release();
    rt_a->Release();
    ps->Release();
    vs->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-viewport-scissor-cache-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
