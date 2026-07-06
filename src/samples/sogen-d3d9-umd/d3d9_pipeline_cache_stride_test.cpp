// Pipeline-cache-key GATE test for VERTEX-BINDING STRIDE with a REAL vertex declaration bound (not RT
// shape -- d3d9_pipeline_cache_test.cpp's job -- and not render state -- d3d9_pipeline_cache_rs_test.cpp's
// job). `vertex_shape_key()` (d3d9_host.cpp) is the piece of `pipeline_cache_key` (d3d9_host.hpp) that is
// supposed to fingerprint "what vertex-input shape will this draw's pipeline get built with". When a real
// declaration is bound (`state_.vertex_decl != 0`), it returns ONLY the raw declaration HANDLE:
//
//     if (real_decl != nullptr) { return this->state_.vertex_decl; }
//
// its own comment reasoning that `vertex_decl_entry::parsed` is populated once, eagerly, at
// CreateVertexDeclaration time and never mutated after, so the same handle always implies the same
// element types/offsets/usages. That reasoning is correct for the DECLARATION's own shape -- but it is
// NOT the only thing that determines the built `VkVertexInputBindingDescription`s. `ensure_programmable_
// pipeline`'s real-decl branch (d3d9_host.cpp) builds one binding per used stream with
// `.stride = this->state_.stream_strides[stream]` -- and `state_.stream_strides` is populated by
// `SetStreamSource(stream, buffer, offsetInBytes, stride)`, which the app can call again, with a
// DIFFERENT stride, WITHOUT ever touching the declaration handle. Since `vertex_shape_key()` folds in
// no stride/stream-strides fingerprint at all in the real-decl branch, two draws using the same
// declaration handle but genuinely different per-stream strides collapse to the identical
// `pipeline_cache_key` -- the second draw's `ensure_programmable_pipeline` call hits the cache and reuses
// the FIRST draw's `VkPipeline`, whose binding-0 stride is permanently baked to the FIRST stride. Vulkan
// then fetches every subsequent vertex's attributes at byte offsets computed from the STALE baked stride,
// not the real, current one -- silently wrong per-vertex data, no error, no HRESULT failure anywhere.
//
// Discriminator (must FAIL under the current, unfixed cache key; expected to PASS once vertex_shape_key()
// also folds in the bound streams' real strides): compile ONE vs_2_0/ps_2_0 pair -- a trivial position-
// only passthrough VS (single POSITION FLOAT3 input, matching this test's minimal one-element real
// declaration) and a PS that always outputs a constant, distinctive solid RED -- and ONE real
// `D3DVERTEXELEMENT9` declaration with exactly one element, POSITION on stream 0, never recreated.
//
//   Sub-pass 1: bind vertex buffer A -- 4 vertices, tightly packed 12-byte FLOAT3 stride (strideA),
//   forming a quad covering the LEFT half of the viewport (NDC x in [-1,0], y in [-1,1]) --
//   via `SetStreamSource(0, bufA, 0, 12)`. Clear the RT BLACK, draw the quad
//   (`DrawIndexedPrimitive`, indices {0,1,2,0,2,3}). This builds/caches a `VkPipeline` whose binding-0
//   `VkVertexInputBindingDescription::stride` is baked to 12. The left-half checkpoint
//   (`kCanvasWidth/4, kCanvasHeight/2`) must read back RED -- proving the first draw (and the pipeline it
//   builds/caches) is correct.
//
//   Sub-pass 2 (the actual discriminator): WITHOUT recreating the declaration/VS/PS, re-clear the SAME
//   RT BLACK, then bind vertex buffer B -- 4 vertices, a DIFFERENT, DISTINCTIVELY larger 32-byte stride
//   (strideB = strideA + 20 bytes of zeroed padding after each 12-byte position) -- via
//   `SetStreamSource(0, bufB, 0, 32)`. Buffer B's real per-vertex positions form a quad covering the
//   RIGHT half of the viewport (NDC x in [0,1], y in [-1,1]) -- a screen region that shares NO pixels
//   with sub-pass 1's left-half quad. Same declaration handle, same VS/PS pair, same RT-shape as
//   sub-pass 1, so `pipeline_cache_key` is bit-for-bit identical under the current, unfixed
//   `vertex_shape_key()` -- `ensure_programmable_pipeline` hits the cache and returns sub-pass 1's stale
//   stride-12 pipeline instead of building a genuinely new stride-32 one. Draw the same quad shape again
//   and read back the right-half checkpoint (`3*kCanvasWidth/4, kCanvasHeight/2`).
//
// What does the stale stride-12 pipeline actually fetch from buffer B? `VkVertexInputAttributeDescription
// ::offset` is 0 (the declaration's only element), so vertex index i's position is fetched from byte
// range `[i*12, i*12+12)` of buffer B, NOT `[i*32, i*32+12)` as the real 32-byte layout requires. Buffer
// B's real per-vertex records (each 32 bytes: 12 real position bytes then 20 zeroed pad bytes) are:
//   record0 bytes [ 0: 32) = pos(0,-1,0.5) + 20 zero bytes
//   record1 bytes [32: 64) = pos(1,-1,0.5) + 20 zero bytes
//   record2 bytes [64: 96) = pos(1, 1,0.5) + 20 zero bytes
//   record3 bytes [96:128) = pos(0, 1,0.5) + 20 zero bytes
// Reading with the stale 12-byte stride instead, index i's fetched-as-FLOAT3 bytes are:
//   i=0: bytes[ 0:12) = (0,-1,0.5)                        -- record0's own real position (correct by
//                                                              coincidence -- index 0 always aligns).
//   i=1: bytes[12:24) = record0's zero padding             -- (0, 0, 0)
//   i=2: bytes[24:36) = record0's last 8 zero pad bytes +
//                       record1's first 4 bytes (pos.x=1.0) reinterpreted as (x,y,z) = (0, 0, 1.0)
//   i=3: bytes[36:48) = record1's pos.y/pos.z (-1.0, 0.5) + record1's first pad float (0.0)
//                       reinterpreted as (x,y,z) = (-1.0, 0.5, 0.0)
// So under the bug, the two triangles actually rasterized are {(0,-1),(0,0),(0,0)} (degenerate, zero
// area -- not rasterized at all) and {(0,-1),(0,0),(-1,0.5)} (real area, but EVERY vertex has x <= 0).
// Both triangles' entire convex hull is confined to NDC x <= 0 -- the LEFT half of the viewport -- so
// neither one can ever rasterize sub-pass 2's right-half checkpoint at NDC x = 0.5 (screen column
// `3*kCanvasWidth/4`). That checkpoint therefore stays exactly BLACK (its own just-applied Clear color,
// untouched by either garbage triangle) under the bug -- not merely "some wrong color", a specific,
// analytically-predicted, deterministic wrong answer, with no dependency on NaN handling, GPU-specific
// degenerate-triangle behavior, or any other non-portable rasterizer quirk (the one degenerate triangle is
// simply never rasterized regardless of how a given rasterizer treats zero-area primitives; the case that
// matters is the non-degenerate one, whose vertices are computed above from plain, exact IEEE-754 zero and
// small-integer bit patterns). Under a fixed `vertex_shape_key()` that also fingerprints the bound
// streams' real strides, sub-pass 2 needs (and gets) a genuinely fresh, correctly-shaped stride-32
// pipeline, the real position data is fetched correctly, and the right-half checkpoint reads back RED.
//
// This test is expected to FAIL (`[d3d9-pipeline-cache-stride-test] FAILED`, exit 1) against the current,
// unmodified host code -- sub-pass 1 passes (RED), sub-pass 2's checkpoint reads BLACK instead of RED. It
// should start passing once `vertex_shape_key()`'s real-declaration branch is extended to also fold in
// the real, current per-stream strides of every stream the declaration references, with no changes to
// this test itself.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; };
struct VSOutput { float4 pos : POSITION; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    return output;
}
)";

    // Constant, distinctive solid RED -- hardcoded directly, no shader constants needed: both sub-passes
    // expect the exact same color, only the vertex-fetch geometry differs between them.
    const char* const k_pixel_shader_hlsl = R"(
float4 main() : COLOR0
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
)";

    // Matches pfnCreateResource's hardcoded 640x480 KNOWN LIMITATION (see sogen_d3d9_umd.cpp) -- every
    // resource this UMD creates is actually backed by a 640x480 surface regardless of the size the app
    // requests, same approach as every other guest test in this directory.
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;

    constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};

    // Buffer A: tightly-packed FLOAT3 positions, stride A = 12 bytes -- the stride sub-pass 1's pipeline
    // gets baked with. Quad covers the LEFT half of the viewport.
    struct PositionOnlyVertex
    {
        float x, y, z;
    };
    constexpr DWORD kStrideA = sizeof(PositionOnlyVertex);
    static_assert(kStrideA == 12, "strideA must be the tightly-packed FLOAT3 size");

    // Buffer B: 32-byte records -- 12 real position bytes then 20 zeroed pad bytes -- stride B = 32,
    // deliberately not equal to strideA. Quad covers the RIGHT half of the viewport.
    struct PaddedVertex
    {
        float x, y, z;
        unsigned char pad[20];
    };
    constexpr DWORD kStrideB = sizeof(PaddedVertex);
    static_assert(kStrideB == 32, "strideB must be a distinctively different, larger stride than strideA");

    float to_ndc_x(const int screen_x) { return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f; }
    float to_ndc_y(const int screen_y) { return static_cast<float>(screen_y) / (kCanvasHeight / 2) - 1.0f; }

    bool channel_close(const unsigned char actual, const int expected, const int tolerance)
    {
        return std::abs(static_cast<int>(actual) - expected) <= tolerance;
    }

    int check_pixel(IDirect3DSurface9* surf, const char* pass_name, const char* point_name, const int col,
                     const int row, const int expected_b, const int expected_g, const int expected_r)
    {
        D3DLOCKED_RECT lr{};
        HRESULT hlr = surf->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-pipeline-cache-stride-test] %s LockRect hr=0x%08lx pBits=%p\n", pass_name,
               static_cast<unsigned long>(hlr), lr.pBits);
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-pipeline-cache-stride-test] FAIL: %s LockRect failed\n", pass_name);
            return 1;
        }

        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = kCanvasWidth * 4;
        const unsigned char* p = base + row * kStride + col * 4;
        printf("[d3d9-pipeline-cache-stride-test] %s %s pixel(%d,%d)=B=%02X G=%02X R=%02X A=%02X\n", pass_name,
               point_name, col, row, p[0], p[1], p[2], p[3]);

        int failures = 0;
        if (!channel_close(p[0], expected_b, 2) || !channel_close(p[1], expected_g, 2) ||
            !channel_close(p[2], expected_r, 2))
        {
            printf("[d3d9-pipeline-cache-stride-test] FAIL: %s %s does not match the expected color\n", pass_name,
                   point_name);
            ++failures;
        }
        else
        {
            printf("[d3d9-pipeline-cache-stride-test] PASS: %s %s matches the expected color\n", pass_name,
                   point_name);
        }

        surf->UnlockRect();
        return failures;
    }

    void release_all(IDirect3DVertexDeclaration9* decl, IDirect3DVertexBuffer9* buf_a, IDirect3DVertexBuffer9* buf_b,
                     IDirect3DIndexBuffer9* ib, IDirect3DSurface9* rt, IDirect3DVertexShader9* vs,
                     IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (decl)
        {
            decl->Release();
        }
        if (ib)
        {
            ib->Release();
        }
        if (buf_a)
        {
            buf_a->Release();
        }
        if (buf_b)
        {
            buf_b->Release();
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
    printf("[d3d9-pipeline-cache-stride-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9pipelinecachestridetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "pipeline-cache-stride-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0,
                                0, kCanvasWidth, kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-pipeline-cache-stride-test] FAIL: Direct3DCreate9 returned null\n");
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
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
                                   &dev);
    printf("[d3d9-pipeline-cache-stride-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
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
    printf("[d3d9-pipeline-cache-stride-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-pipeline-cache-stride-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-pipeline-cache-stride-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-pipeline-cache-stride-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-pipeline-cache-stride-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));
    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-pipeline-cache-stride-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hcvs) || FAILED(hcps))
    {
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Real D3DVERTEXELEMENT9 declaration -- exactly one element, POSITION on stream 0 -- matching
    // k_vertex_shader_hlsl's single-field VSInput struct (declaration order, not usage semantic, assigns
    // the SPIR-V input Location -- see d3d9_multistream_test.cpp's header comment for the full account).
    const D3DVERTEXELEMENT9 kDecl[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    IDirect3DVertexDeclaration9* decl = nullptr;
    HRESULT hcd = dev->CreateVertexDeclaration(kDecl, &decl);
    printf("[d3d9-pipeline-cache-stride-test] CreateVertexDeclaration hr=0x%08lx decl=%p\n",
           static_cast<unsigned long>(hcd), static_cast<void*>(decl));
    if (FAILED(hcd) || !decl)
    {
        printf("[d3d9-pipeline-cache-stride-test] FAIL: CreateVertexDeclaration failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    // Buffer A (strideA=12): quad covering the LEFT half of the viewport (NDC x in [-1,0], y in [-1,1]).
    const PositionOnlyVertex kQuadA[4] = {
        {to_ndc_x(0), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(0), 0.5f},
        {to_ndc_x(kCanvasWidth / 2), to_ndc_y(kCanvasHeight), 0.5f},
        {to_ndc_x(0), to_ndc_y(kCanvasHeight), 0.5f},
    };
    IDirect3DVertexBuffer9* buf_a = nullptr;
    HRESULT hcba = dev->CreateVertexBuffer(sizeof(kQuadA), 0, 0, D3DPOOL_DEFAULT, &buf_a, nullptr);
    printf("[d3d9-pipeline-cache-stride-test] CreateVertexBuffer(A, stride=%lu) hr=0x%08lx vb=%p\n",
           static_cast<unsigned long>(kStrideA), static_cast<unsigned long>(hcba), static_cast<void*>(buf_a));
    if (FAILED(hcba) || !buf_a)
    {
        printf("[d3d9-pipeline-cache-stride-test] FAIL: buffer A creation failed\n");
        release_all(decl, nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        PositionOnlyVertex* v = nullptr;
        buf_a->Lock(0, sizeof(kQuadA), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            std::memcpy(v, kQuadA, sizeof(kQuadA));
            buf_a->Unlock();
        }
    }

    // Buffer B (strideB=32): quad covering the RIGHT half of the viewport (NDC x in [0,1], y in [-1,1]),
    // each 32-byte record holding the real 12-byte position followed by 20 zeroed pad bytes -- see this
    // file's header comment for the exact garbage-fetch computation this padding drives under the bug.
    PaddedVertex kQuadB[4]{};
    kQuadB[0] = {to_ndc_x(kCanvasWidth / 2), to_ndc_y(0), 0.5f, {}};
    kQuadB[1] = {to_ndc_x(kCanvasWidth), to_ndc_y(0), 0.5f, {}};
    kQuadB[2] = {to_ndc_x(kCanvasWidth), to_ndc_y(kCanvasHeight), 0.5f, {}};
    kQuadB[3] = {to_ndc_x(kCanvasWidth / 2), to_ndc_y(kCanvasHeight), 0.5f, {}};
    for (auto& vert : kQuadB)
    {
        std::memset(vert.pad, 0, sizeof(vert.pad));
    }
    IDirect3DVertexBuffer9* buf_b = nullptr;
    HRESULT hcbb = dev->CreateVertexBuffer(sizeof(kQuadB), 0, 0, D3DPOOL_DEFAULT, &buf_b, nullptr);
    printf("[d3d9-pipeline-cache-stride-test] CreateVertexBuffer(B, stride=%lu) hr=0x%08lx vb=%p\n",
           static_cast<unsigned long>(kStrideB), static_cast<unsigned long>(hcbb), static_cast<void*>(buf_b));
    if (FAILED(hcbb) || !buf_b)
    {
        printf("[d3d9-pipeline-cache-stride-test] FAIL: buffer B creation failed\n");
        release_all(decl, buf_a, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        PaddedVertex* v = nullptr;
        buf_b->Lock(0, sizeof(kQuadB), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            std::memcpy(v, kQuadB, sizeof(kQuadB));
            buf_b->Unlock();
        }
    }

    IDirect3DIndexBuffer9* ib = nullptr;
    HRESULT hcib = dev->CreateIndexBuffer(sizeof(kQuadIndices), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    printf("[d3d9-pipeline-cache-stride-test] CreateIndexBuffer hr=0x%08lx ib=%p\n", static_cast<unsigned long>(hcib),
           static_cast<void*>(ib));
    if (FAILED(hcib) || !ib)
    {
        printf("[d3d9-pipeline-cache-stride-test] FAIL: index buffer creation failed\n");
        release_all(decl, buf_a, buf_b, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    {
        WORD* idx = nullptr;
        ib->Lock(0, sizeof(kQuadIndices), reinterpret_cast<void**>(&idx), 0);
        if (idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE,
                                           &rt, nullptr);
    printf("[d3d9-pipeline-cache-stride-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-pipeline-cache-stride-test] FAIL: render target creation failed\n");
        release_all(decl, buf_a, buf_b, ib, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    dev->SetVertexDeclaration(decl);
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    int failures = 0;

    // Sub-pass 1: bind buffer A (strideA=12), draw the left-half quad. Builds/caches a VkPipeline whose
    // binding-0 stride is baked to 12.
    dev->SetStreamSource(0, buf_a, 0, kStrideA);
    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    HRESULT hd1 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-pipeline-cache-stride-test] sub-pass 1 DrawIndexedPrimitive hr=0x%08lx\n",
           static_cast<unsigned long>(hd1));
    dev->EndScene();

    failures += check_pixel(rt, "sub-pass-1 (strideA=12)", "left-half checkpoint", kCanvasWidth / 4, kCanvasHeight / 2,
                            0, 0, 255);

    // Sub-pass 2 (the discriminator): SAME declaration/VS/PS (never recreated), re-clear the SAME RT, bind
    // buffer B (strideB=32, SAME stream 0) covering the RIGHT half instead. pipeline_cache_key's
    // vertex_shape currently collapses to the same declaration-handle value as sub-pass 1.
    dev->SetStreamSource(0, buf_b, 0, kStrideB);
    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    HRESULT hd2 = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
    printf("[d3d9-pipeline-cache-stride-test] sub-pass 2 DrawIndexedPrimitive hr=0x%08lx\n",
           static_cast<unsigned long>(hd2));
    dev->EndScene();

    // Expected RED under a fixed vertex_shape_key(); this is the check predicted to FAIL (read back
    // BLACK instead) against the current, unmodified host code -- see this file's header comment for the
    // exact garbage-fetch computation proving the buggy geometry can never cover this checkpoint.
    failures += check_pixel(rt, "sub-pass-2 (strideB=32)", "right-half checkpoint", 3 * kCanvasWidth / 4,
                            kCanvasHeight / 2, 0, 0, 255);

    release_all(decl, buf_a, buf_b, ib, rt, vs, ps, dev, d3d);

    printf("[d3d9-pipeline-cache-stride-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
