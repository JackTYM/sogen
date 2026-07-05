// Multi-stream vertex source binding regression test (Task 9 of .claude/plans/jazzy-giggling-cloud.md;
// see Task 6's commit 37685830 for stream_offsets storage, Task 7's commits 76a0913b/f32d0e12 for
// parse_vertex_decl, and Task 8's commits 1a6461ff/797caf7d for wiring both into ensure_programmable_
// pipeline/execute_draw). This is the empirical gate for the whole multi-stream feature: before this
// test, only d3d9_vertex_decl_test.cpp (a host-side gtest) exercised parse_vertex_decl, in isolation,
// with no guest ever driving a real CreateVertexDeclaration/SetStreamSource(stream>0) draw end to end.
//
// THREE GENUINE BUGS FOUND AND FIXED WHILE BUILDING THIS TEST (not silently worked around -- all in the
// guest UMD, sogen_d3d9_umd.cpp/d3d9_ddi.hpp; Tasks 6-8 never touched those files, only d3d9_host.cpp/
// .hpp, and nothing before this task had ever called IDirect3DDevice9::CreateVertexDeclaration from a
// guest, so none of these three were previously reachable or visible):
//
//   1. pfnCreateVertexShaderDecl (D3DDDI_DEVICEFUNCS slot 45) was still an unwired device_stub --
//      CreateVertexDeclaration() would appear to succeed (real d3d9.dll's own client-side validation
//      passed) but the DDI call never reached the host at all, so D3DDDIARG_CREATEVERTEXSHADERDECL::
//      ShaderHandle was never populated with a real host-side decl id. Fixed by adding
//      umd_CreateVertexShaderDecl (mirrors umd_CreateVertexShaderFunc/create_shader_common's already-
//      verified struct-pointer-plus-trailing-array convention) and wiring slot 45 to it. The host-side
//      dispatch (gpu_bridge.cpp's handle_d3d9_create_vertex_decl) and wire-protocol structs already
//      existed and needed no changes.
//
//   2. D3DDDIARG_CREATEVERTEXSHADERDECL's field order was guessed backwards ({HANDLE ShaderHandle; UINT
//      NumVertexElements;}), by analogy with D3DDDIARG_CREATESHADERFUNC -- but a live diagnostic byte
//      dump of the real pArgs this DDI call receives (once bug 1 above was fixed enough to reach it)
//      showed NumVertexElements actually comes FIRST: bytes 0-3 read back the real element count (2,
//      exactly matching this test's declaration), and the 8-byte HANDLE lives at offset 8 (with 4 bytes
//      of ordinary x64 alignment padding at offset 4-7 in between -- not a real field). The original
//      guess instead decoded that padding as a garbage 64-bit ShaderHandle and NumVertexElements as 0.
//      Fixed by swapping the struct's field order to {UINT NumVertexElements; HANDLE ShaderHandle;}.
//
//   3. pfnSetVertexShaderDecl (slot 47) was already wired, but as a struct-pointer call
//      (CONST D3DDDIARG_SETVERTEXSHADERDECL* pArgs) -- a live diagnostic dump showed the "pArgs"
//      parameter slot itself receiving the raw, small decl-id value (e.g. 0x10007) rather than a
//      genuine pointer (every real pointer elsewhere in this driver's traces looks like a ~40-bit heap/
//      stack address, not a 5-digit number), and dereferencing that bogus "pointer" silently read back
//      0. pfnSetVertexShaderDecl is actually a DIRECT-VALUE HANDLE call, same convention as
//      umd_SetVertexShaderFunc/umd_SetPixelShader. Before this fix, every real SetVertexDeclaration()
//      call silently forwarded decl=0 ("no declaration") to the host regardless of bug 1/2's fixes,
//      permanently defeating Tasks 6-8's multi-stream draw path from a third, independent angle. Fixed
//      by changing umd_SetVertexShaderDecl's signature to take a plain HANDLE parameter directly.
//
// All three had to be found and fixed together before this test could produce anything but a black
// (unrendered) result -- each one alone was sufficient to make CreateVertexDeclaration/
// SetVertexDeclaration a complete, silent no-op end to end.
//
// Design (double discriminator -- must fail under EITHER of these wrong implementations):
//   - Stream 0: FLOAT3 positions only, 12 vertices (two flat-shaded, non-indexed D3DPT_TRIANGLELIST
//     rectangles: a RED left half and a GREEN right half of the viewport, each built from 2 triangles
//     with no shared vertices between them, so there is no gradient/interpolation seam to worry about).
//   - Stream 1: D3DCOLOR per-vertex color, in a buffer whose real per-vertex data does NOT start at
//     byte 0 -- kColorPadBytes (20) bytes of a deliberately wrong, distinctive pad color come first,
//     and SetStreamSource(1, colorVb, kColorPadBytes, sizeof(DWORD)) supplies that nonzero offset.
//   - A real D3DVERTEXELEMENT9 array with exactly 2 elements, POSITION (stream 0) then COLOR (stream 1)
//     -- matching the VS's HLSL input-struct order exactly, per Task 7's documented, empirically-proven
//     "location = ordinal position in the declaration, not usage semantic" finding (see
//     d3d9_host.cpp's parse_vertex_decl comment).
//   - Discriminator 1 (multi-stream binding broken): if execute_draw fell back to the old single-stream
//     heuristic (stream 0 only, position+color packed in ONE 12-byte-stride buffer), the "color"
//     attribute would be read 12 bytes into stream 0's OWN buffer -- i.e. out of the position-only
//     buffer's actual per-vertex layout entirely (reading the next vertex's position.x as if it were
//     color) -- producing neither RED nor GREEN.
//   - Discriminator 2 (per-stream offsets ignored): if stream 1's real offset were dropped/treated as
//     0, every vertex's color would be read from the WRONG place -- the pad zone at the head of
//     stream 1's buffer -- again producing neither RED nor GREEN (the pad color, or an offset-shifted
//     mix of pad and real data depending on vertex index).
// Only a fully correct implementation (real multi-stream binding AND real per-stream offsets) reads
// RED on the left half and GREEN on the right half, proving per-vertex fetch through the second stream
// binding too (not some kind of accidental constant color).

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

    struct Position
    {
        float x, y, z;
    };

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) --
    // every resource this UMD creates is actually backed by a 640x480 surface regardless of the size
    // the app requests, same approach as d3d9_scissor_test.cpp/d3d9_mrt_test.cpp.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    // Deliberately wrong, distinctive pad color living at stream 1's real byte offset 0 -- distinct
    // from both real colors (RED/GREEN) and the clear color (BLACK), so any read that lands here
    // instead of the real per-vertex data is unambiguously visible.
    constexpr DWORD kPadColor = D3DCOLOR_XRGB(255, 0, 255); // magenta
    constexpr int kPadDwordCount = 5;
    constexpr DWORD kColorPadBytes = kPadDwordCount * sizeof(DWORD); // 20 bytes, nonzero SetStreamSource offset

    float to_ndc_x(const int screen_x) { return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f; }
    float to_ndc_y(const int screen_y) { return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f; }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    void release_all(IDirect3DVertexDeclaration9* decl, IDirect3DVertexBuffer9* pos_vb, IDirect3DVertexBuffer9* color_vb,
                     IDirect3DSurface9* rt, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev,
                     IDirect3D9* d3d)
    {
        if (decl)
        {
            decl->Release();
        }
        if (pos_vb)
        {
            pos_vb->Release();
        }
        if (color_vb)
        {
            color_vb->Release();
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
    printf("[d3d9-multistream-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9multistreamtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "multistream-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-multistream-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-multistream-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-multistream-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-multistream-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-multistream-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-multistream-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-multistream-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-multistream-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Real D3DVERTEXELEMENT9 declaration -- POSITION (stream 0) then COLOR (stream 1), in EXACTLY the
    // same order as k_vertex_shader_hlsl's VSInput struct fields (see this file's header comment on
    // why declaration order, not usage semantic, is what actually assigns the SPIR-V input Location).
    const D3DVERTEXELEMENT9 kDecl[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END(),
    };
    IDirect3DVertexDeclaration9* decl = nullptr;
    HRESULT hcd = dev->CreateVertexDeclaration(kDecl, &decl);
    printf("[d3d9-multistream-test] CreateVertexDeclaration hr=0x%08lx decl=%p\n", static_cast<unsigned long>(hcd),
           static_cast<void*>(decl));
    if (FAILED(hcd) || !decl)
    {
        printf("[d3d9-multistream-test] FAIL: CreateVertexDeclaration failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Stream 0: 12 FLOAT3 positions -- two flat-shaded, non-indexed triangles making up the left half
    // of the viewport (x in [-1,0]) and two more for the right half (x in [0,1]), each covering full
    // height (y in [-1,1]) so no Y-flip convention ambiguity affects which half a check point lands in.
    constexpr int kVertexCount = 12;
    const Position kPositions[kVertexCount] = {
        // Left half, triangle 1
        {to_ndc_x(0), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(kCanvasHeight), 0.5f},
        // Left half, triangle 2
        {to_ndc_x(0), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(kCanvasHeight), 0.5f},
        {to_ndc_x(0), to_ndc_y(kCanvasHeight), 0.5f},
        // Right half, triangle 1
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth), to_ndc_y(kCanvasHeight), 0.5f},
        // Right half, triangle 2
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth), to_ndc_y(kCanvasHeight), 0.5f},
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(kCanvasHeight), 0.5f},
    };

    IDirect3DVertexBuffer9* pos_vb = nullptr;
    HRESULT hcpvb = dev->CreateVertexBuffer(sizeof(kPositions), 0, 0, D3DPOOL_DEFAULT, &pos_vb, nullptr);
    printf("[d3d9-multistream-test] CreateVertexBuffer(positions) hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcpvb),
           static_cast<void*>(pos_vb));
    if (FAILED(hcpvb) || !pos_vb)
    {
        printf("[d3d9-multistream-test] FAIL: position vertex buffer creation failed\n");
        release_all(decl, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        Position* v = nullptr;
        pos_vb->Lock(0, sizeof(kPositions), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            std::memcpy(v, kPositions, sizeof(kPositions));
            pos_vb->Unlock();
        }
    }

    // Stream 1: kColorPadBytes of a deliberately WRONG color pattern, then the 12 real per-vertex
    // D3DCOLOR entries (RED for the first 6 vertices/left half, GREEN for the last 6/right half).
    // SetStreamSource's offset parameter below points past the pad, at the real data -- if that offset
    // were ignored, every fetch would land in the pad zone instead (Discriminator 2).
    constexpr DWORD kColorRed = D3DCOLOR_XRGB(255, 0, 0);
    constexpr DWORD kColorGreen = D3DCOLOR_XRGB(0, 255, 0);
    const DWORD kColorBufferSize = kColorPadBytes + kVertexCount * sizeof(DWORD);

    IDirect3DVertexBuffer9* color_vb = nullptr;
    HRESULT hccvb = dev->CreateVertexBuffer(kColorBufferSize, 0, 0, D3DPOOL_DEFAULT, &color_vb, nullptr);
    printf("[d3d9-multistream-test] CreateVertexBuffer(colors) hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hccvb),
           static_cast<void*>(color_vb));
    if (FAILED(hccvb) || !color_vb)
    {
        printf("[d3d9-multistream-test] FAIL: color vertex buffer creation failed\n");
        release_all(decl, pos_vb, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        unsigned char* base = nullptr;
        color_vb->Lock(0, kColorBufferSize, reinterpret_cast<void**>(&base), 0);
        if (base)
        {
            auto* pad = reinterpret_cast<DWORD*>(base);
            for (int i = 0; i < kPadDwordCount; ++i)
            {
                pad[i] = kPadColor;
            }
            auto* colors = reinterpret_cast<DWORD*>(base + kColorPadBytes);
            for (int i = 0; i < kVertexCount; ++i)
            {
                colors[i] = i < 6 ? kColorRed : kColorGreen;
            }
            color_vb->Unlock();
        }
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt,
                                           nullptr);
    printf("[d3d9-multistream-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-multistream-test] FAIL: render target creation failed\n");
        release_all(decl, pos_vb, color_vb, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    dev->SetVertexDeclaration(decl);
    dev->SetStreamSource(0, pos_vb, 0, sizeof(Position));
    dev->SetStreamSource(1, color_vb, kColorPadBytes, sizeof(DWORD));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    HRESULT hd = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, kVertexCount / 3);
    printf("[d3d9-multistream-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
    dev->EndScene();

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-multistream-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * kStride + col * 4; };

        struct Check
        {
            const char* name;
            int col, row;
            int expected_b, expected_g, expected_r;
        };
        const Check checks[2] = {
            {"left half (RED, stream 1)", kCanvasWidth / 4, kCanvasHeight / 2, 0, 0, 255},
            {"right half (GREEN, stream 1)", 3 * kCanvasWidth / 4, kCanvasHeight / 2, 0, 255, 0},
        };
        for (const auto& c : checks)
        {
            const unsigned char* p = pixel_at(c.col, c.row);
            printf("[d3d9-multistream-test] %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", c.name, c.col, c.row, p[0],
                   p[1], p[2], p[3]);
            if (!channel_close(p[0], c.expected_b, 2) || !channel_close(p[1], c.expected_g, 2) ||
                !channel_close(p[2], c.expected_r, 2))
            {
                printf("[d3d9-multistream-test] FAIL: %s does not match the expected per-vertex stream-1 color\n",
                       c.name);
                ++failures;
            }
            else
            {
                printf("[d3d9-multistream-test] PASS: %s matches the expected per-vertex stream-1 color\n", c.name);
            }
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-multistream-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(decl, pos_vb, color_vb, rt, vs, ps, dev, d3d);

    printf("[d3d9-multistream-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
