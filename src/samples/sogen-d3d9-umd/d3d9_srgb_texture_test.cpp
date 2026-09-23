// D3D9-over-Vulkan D3DSAMP_SRGBTEXTURE test: proves a sampler with the state set decodes sRGB->linear
// on every fetch, and that the same texture sampled with it clear is returned untouched.
//
// D3D9 defines an sRGB pair: D3DSAMP_SRGBTEXTURE decodes on the read, D3DRS_SRGBWRITEENABLE encodes on
// the write. sogen implemented only the write half, so a post-process chain that sets both -- the
// ordinary shape of a bloom chain, which ping-pongs the same surfaces through several passes -- had its
// encode applied at every pass with no decode to undo it, driving anything above black towards white.
// Nothing errors: the sampler returns a plausible value, just far too bright.
//
// Design: a 16x16 source texture covering all 256 byte values, drawn 1:1 over a 16x16 render target
// through the standard D3D9 half-pixel quad, sampled POINT (so each destination texel is exactly one
// source texel and no filtering can blur the comparison), with the render target written linearly. The
// same draw runs twice, once per D3DSAMP_SRGBTEXTURE value, and each pass is compared against its own
// expectation: the stored byte with the state clear, the sRGB-decoded byte with it set.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOutput { float4 pos : POSITION; float2 uv : TEXCOORD0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.uv = input.uv;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
sampler2D s0 : register(s0);
struct PSInput { float4 pos : POSITION; float2 uv : TEXCOORD0; };
float4 main(PSInput input) : COLOR0
{
    return tex2D(s0, input.uv);
}
)";

    struct Vertex
    {
        float x, y, z;
        float u, v;
    };

    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_TEX1;
    constexpr int kSize = 16;
    constexpr BYTE kClearB = 0x7B;
    constexpr BYTE kClearG = 0x2D;
    constexpr BYTE kClearR = 0xC5;

    BYTE source_byte(const int x, const int y)
    {
        return static_cast<BYTE>(y * kSize + x);
    }

    // The sRGB transfer function Vulkan's _SRGB formats apply on a sample (Vulkan spec, "sRGB EOTF").
    BYTE srgb_to_linear_byte(const BYTE stored)
    {
        const double c = stored / 255.0;
        const double linear = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
        return static_cast<BYTE>(std::lround(linear * 255.0));
    }

    float to_ndc_x(const float screen_x)
    {
        return 2.0f * (screen_x - 0.5f) / kSize - 1.0f;
    }

    float to_ndc_y(const float screen_y)
    {
        return 1.0f - 2.0f * (screen_y - 0.5f) / kSize;
    }

    // Draws the source texture 1:1 into the bound render target with the given D3DSAMP_SRGBTEXTURE value
    // and checks every destination texel against `expected`. `decoding` only selects the wording and the
    // extra "did anything actually change" guard, so a pass that silently returns the stored bytes when
    // the decode was asked for cannot be reported as success.
    int check_pass(IDirect3DDevice9* dev, IDirect3DSurface9* rt, const DWORD srgb_texture, BYTE (*expected)(int, int), const bool decoding)
    {
        dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, srgb_texture);
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(kClearR, kClearG, kClearB), 1.0f, 0);
        const HRESULT hdraw = dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        dev->EndScene();

        D3DLOCKED_RECT lr{};
        const HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
        printf("[d3d9-srgb-texture-test] SRGBTEXTURE=%lu draw hr=0x%08lx LockRect hr=0x%08lx pitch=%ld\n",
               static_cast<unsigned long>(srgb_texture), static_cast<unsigned long>(hdraw), static_cast<unsigned long>(hlr),
               static_cast<long>(lr.Pitch));
        if (FAILED(hlr) || !lr.pBits)
        {
            printf("[d3d9-srgb-texture-test] FAIL: render-target LockRect failed\n");
            return 1;
        }

        const LONG pitch = lr.Pitch > 0 ? lr.Pitch : kSize * 4;
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        int mismatched = 0;
        int changed = 0;
        int first_bad_x = -1;
        for (int y = 0; y < kSize; ++y)
        {
            const auto* row = base + static_cast<size_t>(y) * pitch;
            for (int x = 0; x < kSize; ++x)
            {
                const unsigned char got = row[x * 4 + 2];
                const unsigned char want = expected(x, y);
                if (got != source_byte(x, y))
                {
                    ++changed;
                }
                if (std::abs(static_cast<int>(got) - static_cast<int>(want)) > 2)
                {
                    if (first_bad_x < 0)
                    {
                        first_bad_x = x;
                        printf("[d3d9-srgb-texture-test] first mismatch at (%d,%d): stored=%u got=%u expected=%u\n", x, y,
                               source_byte(x, y), got, want);
                    }
                    ++mismatched;
                }
            }
        }
        rt->UnlockRect();

        int failures = 0;
        if (mismatched != 0)
        {
            printf("[d3d9-srgb-texture-test] FAIL: SRGBTEXTURE=%lu left %d/%d texels wrong\n", static_cast<unsigned long>(srgb_texture),
                   mismatched, kSize * kSize);
            ++failures;
        }
        else
        {
            printf("[d3d9-srgb-texture-test] PASS: SRGBTEXTURE=%lu sampled every texel as %s\n", static_cast<unsigned long>(srgb_texture),
                   decoding ? "its sRGB-decoded value" : "the stored value");
        }
        if (decoding && changed == 0)
        {
            printf("[d3d9-srgb-texture-test] FAIL: the decode changed nothing -- the sampler returned the stored bytes\n");
            ++failures;
        }
        return failures;
    }

    BYTE expected_stored(const int x, const int y)
    {
        return source_byte(x, y);
    }

    BYTE expected_decoded(const int x, const int y)
    {
        return srgb_to_linear_byte(source_byte(x, y));
    }

    void release_all(IDirect3DTexture9* tex, IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb, IDirect3DIndexBuffer9* ib,
                     IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (tex)
        {
            tex->Release();
        }
        if (rt)
        {
            rt->Release();
        }
        if (vb)
        {
            vb->Release();
        }
        if (ib)
        {
            ib->Release();
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
    printf("[d3d9-srgb-texture-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9srgbtexturetest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "srgb-texture-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 240, nullptr,
                                nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-srgb-texture-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    const HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-srgb-texture-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
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
    printf("[d3d9-srgb-texture-test] D3DCompile vs hr=0x%08lx ps hr=0x%08lx\n", static_cast<unsigned long>(hvsc),
           static_cast<unsigned long>(hpsc));
    if (FAILED(hvsc) || FAILED(hpsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-srgb-texture-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
        }
        if (ps_errors)
        {
            printf("[d3d9-srgb-texture-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
        }
        release_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, dev, d3d);
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
        printf("[d3d9-srgb-texture-test] FAIL: shader creation failed\n");
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }

    IDirect3DTexture9* tex = nullptr;
    const HRESULT hct = dev->CreateTexture(kSize, kSize, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    printf("[d3d9-srgb-texture-test] CreateTexture hr=0x%08lx tex=%p\n", static_cast<unsigned long>(hct), static_cast<void*>(tex));
    if (FAILED(hct) || !tex)
    {
        release_all(nullptr, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    D3DLOCKED_RECT src_lr{};
    if (FAILED(tex->LockRect(0, &src_lr, nullptr, 0)) || !src_lr.pBits)
    {
        printf("[d3d9-srgb-texture-test] FAIL: source LockRect failed\n");
        release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    for (int y = 0; y < kSize; ++y)
    {
        auto* row = static_cast<unsigned char*>(src_lr.pBits) + static_cast<size_t>(y) * src_lr.Pitch;
        for (int x = 0; x < kSize; ++x)
        {
            const BYTE value = source_byte(x, y);
            row[x * 4 + 0] = value;
            row[x * 4 + 1] = value;
            row[x * 4 + 2] = value;
            row[x * 4 + 3] = 0xFF;
        }
    }
    tex->UnlockRect(0);

    IDirect3DSurface9* rt = nullptr;
    const HRESULT hcrt = dev->CreateRenderTarget(kSize, kSize, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-srgb-texture-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt), static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        release_all(tex, nullptr, nullptr, nullptr, vs, ps, dev, d3d);
        return 1;
    }
    dev->SetRenderTarget(0, rt);

    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(4 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* v = nullptr;
        vb->Lock(0, 4 * sizeof(Vertex), reinterpret_cast<void**>(&v), 0);
        if (v)
        {
            constexpr float kFar = static_cast<float>(kSize);
            v[0] = {to_ndc_x(0.0f), to_ndc_y(0.0f), 0.5f, 0.0f, 0.0f};
            v[1] = {to_ndc_x(kFar), to_ndc_y(0.0f), 0.5f, 1.0f, 0.0f};
            v[2] = {to_ndc_x(kFar), to_ndc_y(kFar), 0.5f, 1.0f, 1.0f};
            v[3] = {to_ndc_x(0.0f), to_ndc_y(kFar), 0.5f, 0.0f, 1.0f};
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
        printf("[d3d9-srgb-texture-test] FAIL: buffer creation failed\n");
        release_all(tex, rt, vb, ib, vs, ps, dev, d3d);
        return 1;
    }

    dev->SetFVF(kFvf);
    dev->SetIndices(ib);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTexture(0, tex);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

    int failures = 0;
    failures += check_pass(dev, rt, FALSE, expected_stored, false);
    failures += check_pass(dev, rt, TRUE, expected_decoded, true);

    release_all(tex, rt, vb, ib, vs, ps, dev, d3d);

    printf("[d3d9-srgb-texture-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
