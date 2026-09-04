// Batched sampled-texture staging uploads: d3d9_host::ensure_texture_uploaded records its staging
// buffer->image copy into the open draw batch instead of submitting a private command buffer and blocking
// on its fence, so an arbitrary number of textures that first become visible in the same frame share one
// submit and one fence. Two properties of that batching are what this test discriminates, both of which a
// single-texture test cannot see:
//
// Phase 1 -- eight textures, each a different solid colour, are each sampled by their own draw inside one
// BeginScene/EndScene against one render target, i.e. eight staging copies recorded into one command
// buffer whose staging buffers must all stay alive until that batch's fence signals. Freeing any of them
// at the end of its own ensure_texture_uploaded call (the natural mistake when converting the old
// submit-and-wait code) leaves the GPU copying from freed memory, which shows up as wrong or black quads.
//
// Phase 2 -- one further texture is drawn, then rewritten through Lock/Unlock, then drawn again, all
// inside one scene, so the second draw samples an image whose first staging copy is still recorded but not
// yet executed. The second upload must therefore both be recorded (the "a copy is already in flight" fast
// path has to be invalidated by the write) and be ordered after the first draw's sampling of the same
// image. A driver that misses either reads the first colour twice.
//
// Phase 3 -- four textures of four DIFFERENT sizes are uploaded in one batch, then rewritten and
// uploaded again in a second batch, by which point phases 1 and 2 have already retired staging buffers of
// a fifth size. The host recycles a retired staging buffer rather than allocating one per upload, so this
// is what discriminates a recycler that hands a buffer back before its slot's fence has signalled (the
// second round samples the first round's colours), or one that hands out a buffer too small for the
// upload, or one whose leftover bytes from a larger previous tenant bleed into a smaller texture.
//
// Every colour is checked analytically against the exact value written into the texture, so a batch that
// drops, reorders or corrupts any individual texture's content is a hard failure rather than a subtle
// shade difference.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    // UV rides in COLOR0 (D3DFVF_DIFFUSE), the convention the rest of this suite uses to sidestep its
    // documented TEXCOORD0-interpolation quirk.
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
sampler2D s0 : register(s0);
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return float4(tex2D(s0, input.color.rg).rgb, 1.0);
}
)";

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };

    struct Rgb
    {
        int r, g, b;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    constexpr int kCanvasWidth = 640;
    constexpr int kCanvasHeight = 480;
    constexpr int kTexSize = 32;
    constexpr int kBatchTextures = 8;

    // Eight well-separated colours: every pair differs by at least 128 in some channel, so a quad showing
    // the wrong texture's content can never be mistaken for a tolerance miss.
    constexpr Rgb k_batch_colors[kBatchTextures] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {255, 0, 255}, {0, 255, 255}, {255, 128, 0}, {128, 0, 255},
    };
    // Phase 3's four textures, deliberately four different sizes so the host's staging recycling has to
    // match a request against differently-sized retired buffers rather than always finding an exact fit.
    constexpr int kMixedTextures = 4;
    constexpr int kMixedSizes[kMixedTextures] = {16, 128, 64, 32};
    constexpr Rgb k_mixed_first[kMixedTextures] = {{240, 32, 16}, {32, 240, 16}, {16, 32, 240}, {240, 240, 32}};
    constexpr Rgb k_mixed_second[kMixedTextures] = {{16, 128, 240}, {240, 16, 128}, {128, 240, 16}, {32, 32, 240}};

    struct QuadRect
    {
        int left, top, right, bottom;
    };

    constexpr int kLeftQuad = kBatchTextures;
    constexpr int kRightQuad = kBatchTextures + 1;
    constexpr int kMixedQuad = kBatchTextures + 2;
    constexpr int kQuadCount = kMixedQuad + kMixedTextures;

    constexpr Rgb k_rewrite_before{16, 224, 128};
    constexpr Rgb k_rewrite_after{224, 16, 96};

    float to_ndc_x(const int screen_x)
    {
        return static_cast<float>(screen_x) / (kCanvasWidth / 2) - 1.0f;
    }

    float to_ndc_y(const int screen_y)
    {
        return 1.0f - static_cast<float>(screen_y) / (kCanvasHeight / 2);
    }

    void fill_quad(Vertex* out, const int left, const int top, const int right, const int bottom)
    {
        const auto uv_color = [](const float u, const float v) {
            return D3DCOLOR_ARGB(255, static_cast<BYTE>(u * 255.0f + 0.5f), static_cast<BYTE>(v * 255.0f + 0.5f), 0);
        };
        out[0] = {to_ndc_x(left), to_ndc_y(top), 0.5f, uv_color(0.0f, 0.0f)};
        out[1] = {to_ndc_x(right), to_ndc_y(top), 0.5f, uv_color(1.0f, 0.0f)};
        out[2] = {to_ndc_x(right), to_ndc_y(bottom), 0.5f, uv_color(1.0f, 1.0f)};
        out[3] = {to_ndc_x(left), to_ndc_y(bottom), 0.5f, uv_color(0.0f, 1.0f)};
    }

    bool fill_texture(IDirect3DTexture9* tex, const Rgb color, const char* label, const int size)
    {
        D3DLOCKED_RECT lr{};
        const HRESULT hr = tex->LockRect(0, &lr, nullptr, 0);
        if (FAILED(hr) || !lr.pBits)
        {
            printf("[d3d9-texture-upload-batch-test] FAIL: LockRect(%s) hr=0x%08lx pBits=%p\n", label, static_cast<unsigned long>(hr),
                   lr.pBits);
            return false;
        }
        const LONG pitch = lr.Pitch != 0 ? lr.Pitch : static_cast<LONG>(size * 4);
        const DWORD argb = D3DCOLOR_ARGB(255, color.r, color.g, color.b);
        auto* base = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < size; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(base + static_cast<size_t>(y) * pitch);
            for (int x = 0; x < size; ++x)
            {
                row[x] = argb;
            }
        }
        tex->UnlockRect(0);
        return true;
    }

    IDirect3DTexture9* make_texture(IDirect3DDevice9* dev, const Rgb color, const char* label, const int size)
    {
        IDirect3DTexture9* tex = nullptr;
        const HRESULT hr = dev->CreateTexture(size, size, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
        if (FAILED(hr) || !tex)
        {
            printf("[d3d9-texture-upload-batch-test] FAIL: CreateTexture(%s) hr=0x%08lx\n", label, static_cast<unsigned long>(hr));
            return nullptr;
        }
        if (!fill_texture(tex, color, label, size))
        {
            tex->Release();
            return nullptr;
        }
        return tex;
    }

    // Every quad's vertices are written once, before any scene, and selected by BaseVertexIndex -- so no
    // buffer is ever rewritten between draws and nothing but the texture path is under test.
    void draw_quad(IDirect3DDevice9* dev, IDirect3DTexture9* tex, const int quad_index)
    {
        dev->SetTexture(0, tex);
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, quad_index * 4, 0, 4, 0, 2);
    }

    // Reads the centre of the given rect out of an already-locked render target and compares it against
    // the colour the sampled texture was filled with.
    bool check_center(const D3DLOCKED_RECT& lr, const QuadRect& quad, const Rgb expected, const char* label)
    {
        const LONG pitch = lr.Pitch != 0 ? lr.Pitch : static_cast<LONG>(kCanvasWidth * 4);
        const int cx = (quad.left + quad.right) / 2;
        const int cy = (quad.top + quad.bottom) / 2;
        const auto* p = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(cy) * pitch + static_cast<size_t>(cx) * 4;
        const int b = p[0];
        const int g = p[1];
        const int r = p[2];
        const bool ok = std::abs(r - expected.r) <= 2 && std::abs(g - expected.g) <= 2 && std::abs(b - expected.b) <= 2;
        printf("[d3d9-texture-upload-batch-test] %s at (%d,%d) R=%02X G=%02X B=%02X expected R=%02X G=%02X B=%02X -> %s\n", label, cx, cy, r,
               g, b, expected.r, expected.g, expected.b, ok ? "OK" : "MISMATCH");
        return ok;
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-texture-upload-batch-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9textureuploadbatchtest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "texture-upload-batch-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, kCanvasWidth,
                                kCanvasHeight, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-texture-upload-batch-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-texture-upload-batch-test] CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    const HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0,
                                    &vs_blob, nullptr);
    const HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0,
                                    &ps_blob, nullptr);
    printf("[d3d9-texture-upload-batch-test] D3DCompile vs=0x%08lx ps=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    vs_blob->Release();
    ps_blob->Release();
    if (!vs || !ps)
    {
        printf("[d3d9-texture-upload-batch-test] FAIL: shader creation failed\n");
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DTexture9* textures[kBatchTextures]{};
    bool textures_ok = true;
    for (int i = 0; i < kBatchTextures; ++i)
    {
        char label[32];
        snprintf(label, sizeof(label), "batch%d", i);
        textures[i] = make_texture(dev, k_batch_colors[i], label, kTexSize);
        textures_ok = textures_ok && textures[i] != nullptr;
    }
    IDirect3DTexture9* rewrite_tex = make_texture(dev, k_rewrite_before, "rewrite", kTexSize);
    textures_ok = textures_ok && rewrite_tex != nullptr;

    IDirect3DTexture9* mixed[kMixedTextures]{};
    for (int i = 0; i < kMixedTextures; ++i)
    {
        char label[32];
        snprintf(label, sizeof(label), "mixed%d", i);
        mixed[i] = make_texture(dev, k_mixed_first[i], label, kMixedSizes[i]);
        textures_ok = textures_ok && mixed[i] != nullptr;
    }

    // Quad 0..7 are phase 1's grid; quad 8/9 are phase 2's left/right pair.
    QuadRect quads[kQuadCount]{};
    for (int i = 0; i < kBatchTextures; ++i)
    {
        const int col = i % 4;
        const int row = i / 4;
        quads[i] = {col * 160 + 20, row * 240 + 20, col * 160 + 140, row * 240 + 220};
    }
    quads[kLeftQuad] = {40, 40, 280, 440};
    quads[kRightQuad] = {360, 40, 600, 440};
    for (int i = 0; i < kMixedTextures; ++i)
    {
        quads[kMixedQuad + i] = {i * 160 + 20, 120, i * 160 + 140, 360};
    }

    IDirect3DSurface9* rt = nullptr;
    const HRESULT hcrt = dev->CreateRenderTarget(kCanvasWidth, kCanvasHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(kQuadCount * 4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, kQuadCount * 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            for (int i = 0; i < kQuadCount; ++i)
            {
                fill_quad(v + i * 4, quads[i].left, quads[i].top, quads[i].right, quads[i].bottom);
            }
            vb->Unlock();
        }
    }
    IDirect3DIndexBuffer9* ib = nullptr;
    dev->CreateIndexBuffer(6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, nullptr);
    if (ib)
    {
        constexpr WORD kQuadIndices[6] = {0, 1, 2, 0, 2, 3};
        WORD* idx = nullptr;
        ib->Lock(0, 6 * sizeof(WORD), reinterpret_cast<void**>(&idx), 0);
        if (idx)
        {
            std::memcpy(idx, kQuadIndices, sizeof(kQuadIndices));
            ib->Unlock();
        }
    }
    if (!textures_ok || FAILED(hcrt) || !rt || !vb || !ib)
    {
        printf("[d3d9-texture-upload-batch-test] FAIL: resource creation failed (rt=%p vb=%p ib=%p)\n", static_cast<void*>(rt),
               static_cast<void*>(vb), static_cast<void*>(ib));
        return 1;
    }

    dev->SetRenderTarget(0, rt);
    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetIndices(ib);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    int failures = 0;

    // Phase 1: every texture's first upload, all inside one batch.
    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    for (int i = 0; i < kBatchTextures; ++i)
    {
        draw_quad(dev, textures[i], i);
    }
    dev->EndScene();

    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        for (int i = 0; i < kBatchTextures; ++i)
        {
            char label[48];
            snprintf(label, sizeof(label), "phase1 quad %d", i);
            failures += check_center(lr, quads[i], k_batch_colors[i], label) ? 0 : 1;
        }
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-texture-upload-batch-test] FAIL: phase1 rt LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    // Phase 2: first upload, rewrite, second upload -- all recorded into one batch, so the second draw
    // samples an image whose first copy has not executed yet.
    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    draw_quad(dev, rewrite_tex, kLeftQuad);
    if (!fill_texture(rewrite_tex, k_rewrite_after, "rewrite/after", kTexSize))
    {
        ++failures;
    }
    draw_quad(dev, rewrite_tex, kRightQuad);
    dev->EndScene();

    lr = {};
    hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        failures += check_center(lr, quads[kLeftQuad], k_rewrite_before, "phase2 before-rewrite") ? 0 : 1;
        failures += check_center(lr, quads[kRightQuad], k_rewrite_after, "phase2 after-rewrite") ? 0 : 1;
        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-texture-upload-batch-test] FAIL: phase2 rt LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    // Phase 3: four sizes in one batch, then the same four rewritten and re-uploaded in a second batch,
    // whose staging buffers can only come from the ones phases 1-2 retired.
    for (int round = 0; round < 2; ++round)
    {
        const Rgb* expected = round == 0 ? k_mixed_first : k_mixed_second;
        if (round == 1)
        {
            for (int i = 0; i < kMixedTextures; ++i)
            {
                char label[32];
                snprintf(label, sizeof(label), "mixed%d/round1", i);
                if (!fill_texture(mixed[i], k_mixed_second[i], label, kMixedSizes[i]))
                {
                    ++failures;
                }
            }
        }
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        for (int i = 0; i < kMixedTextures; ++i)
        {
            draw_quad(dev, mixed[i], kMixedQuad + i);
        }
        dev->EndScene();

        lr = {};
        hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        if (SUCCEEDED(hlr) && lr.pBits)
        {
            for (int i = 0; i < kMixedTextures; ++i)
            {
                char label[64];
                snprintf(label, sizeof(label), "phase3 round %d quad %d (%dx%d)", round, i, kMixedSizes[i], kMixedSizes[i]);
                failures += check_center(lr, quads[kMixedQuad + i], expected[i], label) ? 0 : 1;
            }
            rt->UnlockRect();
        }
        else
        {
            printf("[d3d9-texture-upload-batch-test] FAIL: phase3 round %d rt LockRect hr=0x%08lx\n", round,
                   static_cast<unsigned long>(hlr));
            ++failures;
        }
    }

    for (auto* tex : textures)
    {
        if (tex)
        {
            tex->Release();
        }
    }
    for (auto* tex : mixed)
    {
        if (tex)
        {
            tex->Release();
        }
    }
    rewrite_tex->Release();
    rt->Release();
    vb->Release();
    ib->Release();
    vs->Release();
    ps->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-texture-upload-batch-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
