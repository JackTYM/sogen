// D3D9 hardware-instancing regression test. Proves a single DrawIndexedPrimitive draws N geometry
// instances (SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | N)) with a per-instance vertex stream
// advancing once per instance (SetStreamSourceFreq(1, D3DSTREAMSOURCE_INSTANCEDATA | 1)) -- the host
// path added in resolve_instancing()/vertex_shape_key()/execute_draw (d3d9_host.cpp). Modeled
// structurally on d3d9_multistream_test.cpp (real D3DVERTEXELEMENT9 declaration, COM cleanup via
// release_all, checkpoint-based analytic pixel verification).
//
// Real D3D9 instancing semantics (verified, not guessed): DrawIndexedPrimitive takes NO explicit
// instance count. Instancing is driven ENTIRELY by SetStreamSourceFreq -- the stream flagged
// D3DSTREAMSOURCE_INDEXEDDATA carries the instance count in its low 30 bits, every stream flagged
// D3DSTREAMSOURCE_INSTANCEDATA advances once per instance (low 30 bits = advance divider, only 1 is
// supported here). A single DrawIndexedPrimitive(..., PrimitiveCount) then draws that geometry N times,
// Vulkan fetching the INSTANCEDATA stream by instance index and the INDEXEDDATA stream by vertex index.
//
// Geometry: ONE small unit quad (4 vertices, 6 indices) on stream 0 (per-vertex, INDEXEDDATA | 4).
// Stream 1 (per-instance, INSTANCEDATA | 1) holds 4 entries: {float2 offset, D3DCOLOR color}. The VS
// adds the per-instance offset to the per-vertex local position, placing the quad into one of four
// screen quadrants; the PS outputs the per-instance color. One DrawIndexedPrimitive call therefore
// paints four disjoint, solid-colored quads (RED / GREEN / BLUE / YELLOW), one per quadrant.
//
// Double discriminator (must fail under EITHER wrong implementation):
//   (a) instance_count still 1 (INDEXEDDATA count ignored / execute_draw still hardcodes 1): only the
//       FIRST instance draws -- exactly one quadrant is painted, the other three stay the BLACK clear
//       color. Probing all four quadrant centers for four DISTINCT non-clear colors catches this.
//   (b) inputRate still VK_VERTEX_INPUT_RATE_VERTEX for the per-instance stream (mask never set): every
//       instance fetches stream 1 by VERTEX index instead of instance index, so the quad's four corners
//       each pick up a DIFFERENT instance's offset+color. Instead of four flat solid quads it renders a
//       single big quad stretched across the four offsets with the four corner colors bilinearly
//       interpolated -- the quadrant centers read muddy blends, not pure RED/GREEN/BLUE/YELLOW. The
//       purity check (each probe within tolerance of one of the four pure colors) plus the per-quadrant
//       solidity check (center and an inner-corner probe agree) catches this.
// Only a fully correct implementation (real instance_count AND real per-instance inputRate) paints four
// clean, solid, distinctly-colored quadrants.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput  { float3 pos : POSITION; float2 offset : TEXCOORD0; float4 color : COLOR0; };
struct VSOutput { float4 pos : POSITION; float4 color : COLOR0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos.xy + input.offset, input.pos.z, 1.0);
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

    struct InstanceData
    {
        float ox, oy;
        DWORD color;
    };

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) --
    // every resource this UMD creates is actually backed by a 640x480 surface, same as the other tests.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    constexpr int kInstanceCount = 4;

    // Four distinct, fully-saturated per-instance colors -- distinct from each other and from the BLACK
    // clear color, so any quadrant left at clear (missing instance) or blended (per-vertex fetch) is
    // unambiguously visible. Stored/read back as B,G,R,A.
    constexpr DWORD kColorRed = D3DCOLOR_XRGB(255, 0, 0);
    constexpr DWORD kColorGreen = D3DCOLOR_XRGB(0, 255, 0);
    constexpr DWORD kColorBlue = D3DCOLOR_XRGB(0, 0, 255);
    constexpr DWORD kColorYellow = D3DCOLOR_XRGB(255, 255, 0);

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    // Returns 0..3 for a pixel that matches one of the four pure per-instance colors within tolerance,
    // or -1 for anything else (clear color, or an interpolated blend from a per-vertex-rate bug).
    int classify_color(const unsigned char* p)
    {
        struct Pure
        {
            int b, g, r;
        };
        const Pure pures[kInstanceCount] = {
            {0, 0, 255},   // 0 RED
            {0, 255, 0},   // 1 GREEN
            {255, 0, 0},   // 2 BLUE
            {0, 255, 255}, // 3 YELLOW
        };
        for (int i = 0; i < kInstanceCount; ++i)
        {
            if (channel_close(p[0], pures[i].b, 2) && channel_close(p[1], pures[i].g, 2) &&
                channel_close(p[2], pures[i].r, 2))
            {
                return i;
            }
        }
        return -1;
    }

    void release_all(IDirect3DVertexDeclaration9* decl, IDirect3DVertexBuffer9* pos_vb,
                     IDirect3DVertexBuffer9* inst_vb, IDirect3DIndexBuffer9* ib, IDirect3DSurface9* rt,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (decl)
        {
            decl->Release();
        }
        if (pos_vb)
        {
            pos_vb->Release();
        }
        if (inst_vb)
        {
            inst_vb->Release();
        }
        if (ib)
        {
            ib->Release();
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
    printf("[d3d9-instancing-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9instancingtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "instancing-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-instancing-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-instancing-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-instancing-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-instancing-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-instancing-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-instancing-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-instancing-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-instancing-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Real D3DVERTEXELEMENT9 declaration in EXACTLY the VSInput field order (POSITION, TEXCOORD0, COLOR0)
    // -- declaration ordinal, not usage semantic, assigns the SPIR-V input Location (see
    // d3d9_multistream_test.cpp / parse_vertex_decl). POSITION on stream 0 (per-vertex geometry),
    // TEXCOORD0+COLOR0 on stream 1 (per-instance offset+color).
    const D3DVERTEXELEMENT9 kDecl[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        {1, 8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END(),
    };
    IDirect3DVertexDeclaration9* decl = nullptr;
    HRESULT hcd = dev->CreateVertexDeclaration(kDecl, &decl);
    printf("[d3d9-instancing-test] CreateVertexDeclaration hr=0x%08lx decl=%p\n", static_cast<unsigned long>(hcd),
           static_cast<void*>(decl));
    if (FAILED(hcd) || !decl)
    {
        printf("[d3d9-instancing-test] FAIL: CreateVertexDeclaration failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Stream 0: one small quad in local NDC space, centered on the origin (half-width 0.4). The VS shifts
    // it by the per-instance offset. 4 vertices, indexed as two triangles.
    constexpr float kHalf = 0.4f;
    const Position kPositions[4] = {
        {-kHalf, -kHalf, 0.5f},
        {+kHalf, -kHalf, 0.5f},
        {+kHalf, +kHalf, 0.5f},
        {-kHalf, +kHalf, 0.5f},
    };
    IDirect3DVertexBuffer9* pos_vb = nullptr;
    HRESULT hcpvb = dev->CreateVertexBuffer(sizeof(kPositions), 0, 0, D3DPOOL_DEFAULT, &pos_vb, nullptr);
    printf("[d3d9-instancing-test] CreateVertexBuffer(positions) hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcpvb),
           static_cast<void*>(pos_vb));
    if (FAILED(hcpvb) || !pos_vb)
    {
        printf("[d3d9-instancing-test] FAIL: position vertex buffer creation failed\n");
        release_all(decl, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        void* v = nullptr;
        pos_vb->Lock(0, sizeof(kPositions), &v, 0);
        if (v)
        {
            std::memcpy(v, kPositions, sizeof(kPositions));
            pos_vb->Unlock();
        }
    }

    // Stream 1: one entry per instance. The four offsets place the quad into the four screen quadrants
    // (centers at NDC (+-0.5, +-0.5)); the four colors are distinct and pure.
    const InstanceData kInstances[kInstanceCount] = {
        {-0.5f, +0.5f, kColorRed},
        {+0.5f, +0.5f, kColorGreen},
        {-0.5f, -0.5f, kColorBlue},
        {+0.5f, -0.5f, kColorYellow},
    };
    IDirect3DVertexBuffer9* inst_vb = nullptr;
    HRESULT hcivb = dev->CreateVertexBuffer(sizeof(kInstances), 0, 0, D3DPOOL_DEFAULT, &inst_vb, nullptr);
    printf("[d3d9-instancing-test] CreateVertexBuffer(instances) hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcivb),
           static_cast<void*>(inst_vb));
    if (FAILED(hcivb) || !inst_vb)
    {
        printf("[d3d9-instancing-test] FAIL: instance vertex buffer creation failed\n");
        release_all(decl, pos_vb, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        void* v = nullptr;
        inst_vb->Lock(0, sizeof(kInstances), &v, 0);
        if (v)
        {
            std::memcpy(v, kInstances, sizeof(kInstances));
            inst_vb->Unlock();
        }
    }

    const WORD kIndices[6] = {0, 1, 2, 0, 2, 3};
    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hcib = dev->CreateIndexBuffer(sizeof(kIndices), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    printf("[d3d9-instancing-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib),
           static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-instancing-test] FAIL: index buffer creation failed\n");
        release_all(decl, pos_vb, inst_vb, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        void* p = nullptr;
        ib->Lock(0, sizeof(kIndices), &p, 0);
        if (p)
        {
            std::memcpy(p, kIndices, sizeof(kIndices));
            ib->Unlock();
        }
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt,
                                           nullptr);
    printf("[d3d9-instancing-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-instancing-test] FAIL: render target creation failed\n");
        release_all(decl, pos_vb, inst_vb, ib, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    dev->SetVertexDeclaration(decl);
    dev->SetStreamSource(0, pos_vb, 0, sizeof(Position));
    dev->SetStreamSource(1, inst_vb, 0, sizeof(InstanceData));
    dev->SetIndices(ib);
    // The instancing setup: stream 0 is per-vertex indexed geometry drawn N=4 times; stream 1 advances
    // once per instance. This -- not any DrawIndexedPrimitive argument -- is what makes the single draw
    // below render four instances.
    dev->SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | kInstanceCount);
    dev->SetStreamSourceFreq(1, D3DSTREAMSOURCE_INSTANCEDATA | 1);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    HRESULT hd = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-instancing-test] DrawIndexedPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hd));
    dev->EndScene();

    // Reset the frequencies to their non-instanced defaults so nothing leaks into a later draw (defensive;
    // the device is torn down right after, but this mirrors real app hygiene).
    dev->SetStreamSourceFreq(0, 1);
    dev->SetStreamSourceFreq(1, 1);

    int failures = 0;
    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-instancing-test] LockRect hr=0x%08lx pBits=%p\n", static_cast<unsigned long>(hlr), lr.pBits);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        const LONG stride = lr.Pitch != 0 ? lr.Pitch : kCanvasWidth * 4;
        auto pixel_at = [&](const int col, const int row) { return base + row * stride + col * 4; };

        // The four screen-quadrant centers. NDC x is not flipped in this host (see d3d9_multistream_test:
        // NDC x=-1 reads back on the left), so NDC x=-0.5 -> col 160, +0.5 -> col 480; the two NDC
        // y=+-0.5 rows are 120 and 360. Which instance colors land where is asserted below as a bijection
        // (four distinct pure colors, none clear) so the check is independent of the Y-axis orientation
        // convention -- what matters for BOTH discriminators is that all four instances rendered as four
        // clean, distinctly-colored, solid quads.
        struct Quadrant
        {
            const char* name;
            int col, row;         // quadrant center
            int inner_col, inner_row; // a second probe pulled toward screen center (solidity check)
        };
        const Quadrant quads[kInstanceCount] = {
            {"top-left", kCanvasWidth / 4, kCanvasHeight / 4, kCanvasWidth / 4 + 40, kCanvasHeight / 4 + 30},
            {"top-right", 3 * kCanvasWidth / 4, kCanvasHeight / 4, 3 * kCanvasWidth / 4 - 40, kCanvasHeight / 4 + 30},
            {"bottom-left", kCanvasWidth / 4, 3 * kCanvasHeight / 4, kCanvasWidth / 4 + 40, 3 * kCanvasHeight / 4 - 30},
            {"bottom-right", 3 * kCanvasWidth / 4, 3 * kCanvasHeight / 4, 3 * kCanvasWidth / 4 - 40,
             3 * kCanvasHeight / 4 - 30},
        };

        int seen[kInstanceCount] = {0, 0, 0, 0};
        for (const auto& q : quads)
        {
            const unsigned char* c = pixel_at(q.col, q.row);
            const unsigned char* inner = pixel_at(q.inner_col, q.inner_row);
            const int cls = classify_color(c);
            const int inner_cls = classify_color(inner);
            printf("[d3d9-instancing-test] %s center(%d,%d)=B=%02X G=%02X R=%02X A=%02X class=%d inner class=%d\n",
                   q.name, q.col, q.row, c[0], c[1], c[2], c[3], cls, inner_cls);
            if (cls < 0)
            {
                // Discriminator (a): a clear/black or blended center means this instance never drew (still
                // instance_count==1) or the stream was fetched per-vertex and interpolated (bug b).
                printf("[d3d9-instancing-test] FAIL: %s is not a pure per-instance color (missing instance "
                       "or per-vertex-rate interpolation)\n",
                       q.name);
                ++failures;
                continue;
            }
            if (inner_cls != cls)
            {
                // Discriminator (b): a solid per-instance quad reads the SAME pure color at its center and
                // an interior point; a per-vertex-rate quad interpolates across its corners, so the two
                // probes disagree.
                printf("[d3d9-instancing-test] FAIL: %s is not a solid single color (center class %d != "
                       "inner class %d -- per-vertex-rate interpolation)\n",
                       q.name, cls, inner_cls);
                ++failures;
                continue;
            }
            ++seen[cls];
            printf("[d3d9-instancing-test] PASS: %s is solid pure color class %d\n", q.name, cls);
        }

        // Bijection: each of the four instance colors must appear in exactly one quadrant. A repeat (or a
        // gap) means fewer than four distinct instances actually rendered.
        for (int i = 0; i < kInstanceCount; ++i)
        {
            if (seen[i] != 1)
            {
                printf("[d3d9-instancing-test] FAIL: instance color class %d appeared in %d quadrants "
                       "(expected exactly 1 -- instancing did not produce 4 distinct instances)\n",
                       i, seen[i]);
                ++failures;
            }
        }
        if (failures == 0)
        {
            printf("[d3d9-instancing-test] PASS: four distinct instances painted four solid quadrants\n");
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-instancing-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(decl, pos_vb, inst_vb, ib, rt, vs, ps, dev, d3d);

    printf("[d3d9-instancing-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
