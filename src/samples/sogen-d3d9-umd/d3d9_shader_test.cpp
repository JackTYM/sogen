// D3D9-over-Vulkan de-risk slice: programmable-shader triangle (see
// docs/superpowers/plans/2026-07-03-vkd3d-shader-derisk.md). Real D3DCompile()-produced SM2 vertex +
// pixel shaders, position+color passthrough only (no constant registers, no textures) -- deliberately
// separate from d3d9_triangle_test.cpp so the fixed-function triangle stays a working regression
// baseline throughout this slice's development.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
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
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-shader-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9shadertest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "shader-triangle", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                                nullptr, nullptr, wc.hInstance, nullptr);
    printf("[d3d9-shader-test] hwnd=%p\n", static_cast<void*>(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-shader-test] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-shader-test] FAIL: Direct3DCreate9 returned null\n");
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
    printf("[d3d9-shader-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr), static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-shader-test] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-shader-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-shader-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-shader-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-shader-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-shader-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));

    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-shader-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));

    vs_blob->Release();
    ps_blob->Release();

    if (FAILED(hcvs) || FAILED(hcps))
    {
        if (vs)
        {
            vs->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(3 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-shader-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb), static_cast<void*>(vb));
    if (vb)
    {
        Vertex* verts = nullptr;
        vb->Lock(0, 3 * sizeof(Vertex), reinterpret_cast<void**>(&verts), 0);
        if (verts)
        {
            verts[0] = {0.0f, 0.5f, 0.5f, D3DCOLOR_XRGB(255, 0, 0)};
            verts[1] = {0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(0, 255, 0)};
            verts[2] = {-0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(0, 0, 255)};
            vb->Unlock();
        }

        dev->SetFVF(kFvf);
        dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
        dev->SetVertexShader(vs);
        dev->SetPixelShader(ps);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
        HRESULT hdp = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
        printf("[d3d9-shader-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdp));
        dev->EndScene();

        HRESULT hp = dev->Present(nullptr, nullptr, nullptr, nullptr);
        printf("[d3d9-shader-test] Present hr=0x%08lx\n", static_cast<unsigned long>(hp));

        vb->Release();
    }

    if (vs)
    {
        vs->Release();
    }
    if (ps)
    {
        ps->Release();
    }
    dev->Release();
    d3d->Release();
    printf("[d3d9-shader-test] done\n");
    return 0;
}
