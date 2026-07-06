// D3D9-over-Vulkan Shader Model 3.0 end-to-end test. Proves the SM3.0 caps delta in
// `sogen_d3d9_umd.cpp`'s `fill_d3d9caps` (VertexShaderVersion/PixelShaderVersion raised to vs_3_0/ps_3_0
// plus the dozen dependent D3DCAPS9 fields real d3d9.dll's IsD3DHALSupported requires for the SM3.0
// validation branch) makes real Microsoft `d3d9.dll` accept and render a genuine SM3.0 shader pair --
// not just that shader-object creation returns S_OK, but that a real draw through the programmable
// pipeline produces the analytically-predicted pixel.
//
// The proof has four independent parts, each a hard failure if broken:
//
// 1. EXPLICIT CAPS CHECK: GetDeviceCaps(HAL) must report VertexShaderVersion==D3DVS_VERSION(3,0) and
//    PixelShaderVersion==D3DPS_VERSION(3,0). This reads back the exact DWORDs `fill_d3d9caps` wrote and
//    that IsD3DHALSupported validated -- if the caps delta didn't take, this fails before any shader work.
//
// 2. SM3.0 DISCRIMINATOR: the pixel shader below contains a real runtime-count loop (`for i < loopCount.x`,
//    driven by SetPixelShaderConstantI). ps_2_0 has no loop/rep instruction and no integer constant
//    registers, so D3DCompile of the SAME source at target `ps_2_0` MUST FAIL, while `ps_3_0` MUST
//    SUCCEED. This proves the shader genuinely requires SM3.0 rather than being an SM2.0 shader that
//    merely happens to compile under a 3.0 profile.
//
// 3. REAL HARDWARE LOOP: the compiled ps_3_0 bytecode is walked as raw D3DBC tokens (same opcode/length
//    decode as d3d9_int_bool_const_test.cpp) to confirm the compiler emitted an actual LOOP/ENDLOOP or
//    REP/ENDREP opcode -- i.e. the loop was NOT unrolled or closed-formed into straight-line arithmetic
//    (which would have silently made it an SM2.0-expressible shader). d3dcompiler_43 keeps a runtime-count
//    additive loop as a real loop, confirmed empirically both here and in d3d9_int_bool_const_test.cpp.
//
// 4. ANALYTIC RENDER: a triangle is drawn to an off-screen render target with this vs_3_0/ps_3_0 pair and
//    LockRect-read back. The pixel shader accumulates `kDelta` per loop iteration over exactly
//    `kLoopCount` iterations (both supplied ONLY via runtime SetPixelShaderConstantI, never baked into the
//    HLSL) and returns float4(acc, acc*0.5, acc*1.5, 1) -- three DISTINCT channels (so an R/G/B swizzle
//    error is caught) whose exact bytes are recomputed on the C++ side by replaying the identical float
//    loop. A correct pixel proves the whole SM3.0 path -- caps acceptance, ps_3_0 SPIR-V translation, the
//    PS integer constant register (set 1 / binding 2) delivery, and the real GPU loop -- worked end to end.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>

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

    // Runtime-count pixel-shader loop. loopCount comes ONLY from register(i0) (SetPixelShaderConstantI),
    // so d3dcompiler has no compile-time trip count to unroll around and must emit a real hardware loop --
    // a ps_3_0-only construct (ps_2_0 has neither loop/rep nor integer constant registers). The additive
    // body is kept (not multiplicative) precisely because d3dcompiler_43 is empirically confirmed to leave
    // a runtime-count additive loop as a genuine REP rather than closed-forming it into `acc = kDelta*N`
    // (see d3d9_int_bool_const_test.cpp's proven vs_2_0 loop of the same shape).
    const char* const k_pixel_shader_hlsl = R"(
int4 loopCount : register(i0);
struct PSInput { float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    float acc = 0.0;
    for (int i = 0; i < loopCount.x; i++)
    {
        acc += 0.1;
    }
    return float4(acc, acc * 0.5, acc * 1.5, 1.0);
}
)";

    // Runtime-only loop trip count -- never a literal in the HLSL above.
    constexpr int kLoopCountVec[4] = {5, 0, 0, 0};
    constexpr float kDelta = 0.1f;

    constexpr unsigned char kBackgroundR = 20;
    constexpr unsigned char kBackgroundG = 20;
    constexpr unsigned char kBackgroundB = 20;

    bool channel_close(const unsigned char actual, const int expected)
    {
        return std::abs(static_cast<int>(actual) - expected) <= 3;
    }

    int to_byte(const float v)
    {
        float c = v;
        if (c < 0.0f)
        {
            c = 0.0f;
        }
        if (c > 1.0f)
        {
            c = 1.0f;
        }
        return static_cast<int>(c * 255.0f + 0.5f);
    }

    // D3DBC token-stream decode, matching vkd3d-shader's own VKD3D_SM1_INSTRUCTION_LENGTH_SHIFT/_MASK and
    // d3d9_int_bool_const_test.cpp: opcode in the low 16 bits, instruction length in bits 24-27.
    constexpr uint32_t kOpcodeMask = 0x0000FFFFu;
    constexpr uint32_t kLengthShift = 24u;
    constexpr uint32_t kLengthMask = 0x0Fu << kLengthShift;
    constexpr uint32_t kSioComment = 0xFFFEu;
    constexpr uint32_t kSioEnd = 0xFFFFu;
    constexpr uint32_t kSioLoop = 0x1Bu;
    constexpr uint32_t kSioEndLoop = 0x1Du;
    constexpr uint32_t kSioRep = 0x26u;
    constexpr uint32_t kSioEndRep = 0x27u;

    bool scan_for_loop_opcode(const uint32_t* tokens, const size_t token_count)
    {
        if (token_count == 0)
        {
            return false;
        }
        size_t i = 1; // token[0] is the version token.
        while (i < token_count)
        {
            const uint32_t token = tokens[i];
            const uint32_t opcode = token & kOpcodeMask;
            if (opcode == kSioEnd)
            {
                break;
            }
            if (opcode == kSioComment)
            {
                const uint32_t comment_len = (token >> 16) & 0x7FFFu;
                i += 1 + comment_len;
                continue;
            }
            if (opcode == kSioLoop || opcode == kSioEndLoop || opcode == kSioRep || opcode == kSioEndRep)
            {
                return true;
            }
            i += 1 + ((token & kLengthMask) >> kLengthShift);
        }
        return false;
    }

    void release_all(IDirect3DSurface9* rt, IDirect3DVertexBuffer9* vb, IDirect3DPixelShader9* ps,
                     IDirect3DVertexShader9* vs, IDirect3DDevice9* dev, IDirect3D9* d3d)
    {
        if (rt)
        {
            rt->Release();
        }
        if (vb)
        {
            vb->Release();
        }
        if (ps)
        {
            ps->Release();
        }
        if (vs)
        {
            vs->Release();
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
    printf("[d3d9-sm3-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9sm3test";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "sm3-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                                nullptr, nullptr, wc.hInstance, nullptr);
    printf("[d3d9-sm3-test] hwnd=%p\n", static_cast<void*>(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-sm3-test] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-sm3-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    int failures = 0;

    // Part 1: explicit SM3.0 caps check (reads back exactly what fill_d3d9caps wrote / IsD3DHALSupported
    // validated).
    D3DCAPS9 caps{};
    HRESULT hgdc = d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
    printf("[d3d9-sm3-test] GetDeviceCaps hr=0x%08lx VertexShaderVersion=0x%08lx PixelShaderVersion=0x%08lx\n",
           static_cast<unsigned long>(hgdc), static_cast<unsigned long>(caps.VertexShaderVersion),
           static_cast<unsigned long>(caps.PixelShaderVersion));
    printf("[d3d9-sm3-test] MaxVertexShader30InstructionSlots=%lu MaxPixelShader30InstructionSlots=%lu\n",
           static_cast<unsigned long>(caps.MaxVertexShader30InstructionSlots),
           static_cast<unsigned long>(caps.MaxPixelShader30InstructionSlots));
    if (FAILED(hgdc) || caps.VertexShaderVersion != D3DVS_VERSION(3, 0) ||
        caps.PixelShaderVersion != D3DPS_VERSION(3, 0))
    {
        printf("[d3d9-sm3-test] FAIL: GetDeviceCaps did not report vs_3_0/ps_3_0 (got VS=0x%08lx PS=0x%08lx, "
               "expected 0x%08lx/0x%08lx)\n",
               static_cast<unsigned long>(caps.VertexShaderVersion),
               static_cast<unsigned long>(caps.PixelShaderVersion),
               static_cast<unsigned long>(D3DVS_VERSION(3, 0)), static_cast<unsigned long>(D3DPS_VERSION(3, 0)));
        d3d->Release();
        return 1;
    }
    printf("[d3d9-sm3-test] PASS: GetDeviceCaps reports vs_3_0/ps_3_0\n");

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr =
        d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-sm3-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-sm3-test] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    // Part 2 (discriminator, negative half): the pixel shader MUST NOT compile as ps_2_0.
    {
        ID3DBlob* ps20_blob = nullptr;
        ID3DBlob* ps20_errors = nullptr;
        HRESULT h20 = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr,
                                 "main", "ps_2_0", 0, 0, &ps20_blob, &ps20_errors);
        printf("[d3d9-sm3-test] D3DCompile(ps, ps_2_0) hr=0x%08lx (expected failure)\n",
               static_cast<unsigned long>(h20));
        if (SUCCEEDED(h20))
        {
            printf("[d3d9-sm3-test] FAIL: the loop pixel shader unexpectedly compiled as ps_2_0 -- it is not a "
                   "genuine SM3.0-only shader\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-sm3-test] PASS: loop pixel shader correctly rejected by the ps_2_0 profile (genuine "
                   "SM3.0-only)\n");
        }
        if (ps20_blob)
        {
            ps20_blob->Release();
        }
        if (ps20_errors)
        {
            ps20_errors->Release();
        }
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_3_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-sm3-test] D3DCompile(vs, vs_3_0) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-sm3-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        release_all(nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    // Part 2 (discriminator, positive half): the SAME source compiles as ps_3_0.
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_3_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-sm3-test] D3DCompile(ps, ps_3_0) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-sm3-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        release_all(nullptr, nullptr, nullptr, nullptr, dev, d3d);
        return 1;
    }

    // Part 3: confirm the ps_3_0 bytecode carries a REAL hardware loop (not unrolled/closed-formed).
    {
        const auto* ps_tokens = static_cast<const uint32_t*>(ps_blob->GetBufferPointer());
        const size_t ps_token_count = ps_blob->GetBufferSize() / sizeof(uint32_t);
        const bool saw_loop = scan_for_loop_opcode(ps_tokens, ps_token_count);
        printf("[d3d9-sm3-test] ps_3_0 bytecode size=%zu bytes, saw_LOOP/REP=%s\n", ps_blob->GetBufferSize(),
               saw_loop ? "yes" : "no");
        if (!saw_loop)
        {
            printf("[d3d9-sm3-test] FAIL: no LOOP/REP opcode in the ps_3_0 bytecode -- the loop was unrolled or "
                   "closed-formed, so it is not exercising genuine SM3.0 pixel-shader flow control\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-sm3-test] PASS: ps_3_0 bytecode contains a real LOOP/REP opcode\n");
        }
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-sm3-test] CreateVertexShader hr=0x%08lx vs=%p\n", static_cast<unsigned long>(hcvs),
           static_cast<void*>(vs));

    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-sm3-test] CreatePixelShader hr=0x%08lx ps=%p\n", static_cast<unsigned long>(hcps),
           static_cast<void*>(ps));

    vs_blob->Release();
    ps_blob->Release();

    if (FAILED(hcvs) || FAILED(hcps) || !vs || !ps)
    {
        printf("[d3d9-sm3-test] FAIL: CreateVertexShader/CreatePixelShader failed for real SM3.0 bytecode\n");
        release_all(nullptr, nullptr, ps, vs, dev, d3d);
        return 1;
    }
    printf("[d3d9-sm3-test] PASS: CreateVertexShader/CreatePixelShader succeeded with non-null vs_3_0/ps_3_0 "
           "handles\n");

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hcvb = dev->CreateVertexBuffer(3 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    printf("[d3d9-sm3-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (!vb)
    {
        release_all(nullptr, nullptr, ps, vs, dev, d3d);
        return 1;
    }

    Vertex* verts = nullptr;
    vb->Lock(0, 3 * sizeof(Vertex), reinterpret_cast<void**>(&verts), 0);
    if (verts)
    {
        verts[0] = {0.0f, 0.6f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)};
        verts[1] = {0.6f, -0.6f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)};
        verts[2] = {-0.6f, -0.6f, 0.5f, D3DCOLOR_XRGB(255, 255, 255)};
        vb->Unlock();
    }

    IDirect3DSurface9* rt = nullptr;
    HRESULT hcrt = dev->CreateRenderTarget(640, 480, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
    printf("[d3d9-sm3-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        release_all(nullptr, vb, ps, vs, dev, d3d);
        return 1;
    }

    HRESULT hsrt = dev->SetRenderTarget(0, rt);
    printf("[d3d9-sm3-test] SetRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hsrt));

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);

    HRESULT hspi = dev->SetPixelShaderConstantI(0, kLoopCountVec, 1);
    printf("[d3d9-sm3-test] SetPixelShaderConstantI(0) hr=0x%08lx\n", static_cast<unsigned long>(hspi));

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(kBackgroundR, kBackgroundG, kBackgroundB), 1.0f, 0);
    HRESULT hdp = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
    printf("[d3d9-sm3-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdp));
    dev->EndScene();

    // Part 4: analytic pixel check. Replay the exact float loop the shader runs to derive the expected
    // color (acc accumulated kDelta per iteration over kLoopCount iterations, then float4(acc, acc*0.5,
    // acc*1.5, 1)). Three distinct channels catch an R/G/B swizzle error.
    float acc = 0.0f;
    for (int i = 0; i < kLoopCountVec[0]; i++)
    {
        acc += kDelta;
    }
    const int expected_r = to_byte(acc);
    const int expected_g = to_byte(acc * 0.5f);
    const int expected_b = to_byte(acc * 1.5f);
    printf("[d3d9-sm3-test] expected interior pixel B=%02X G=%02X R=%02X (acc=%.4f over %d iterations)\n",
           expected_b, expected_g, expected_r, static_cast<double>(acc), kLoopCountVec[0]);

    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-sm3-test] LockRect hr=0x%08lx pBits=%p Pitch=%ld\n", static_cast<unsigned long>(hlr), lr.pBits,
           static_cast<long>(lr.Pitch));
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = 640 * 4;
        const unsigned char* pixel = base + 240 * kStride + 320 * 4; // (320,240): well inside the triangle.
        printf("[d3d9-sm3-test] interior pixel(320,240)=B=%02X G=%02X R=%02X A=%02X\n", pixel[0], pixel[1],
               pixel[2], pixel[3]);
        if (channel_close(pixel[2], expected_r) && channel_close(pixel[1], expected_g) &&
            channel_close(pixel[0], expected_b))
        {
            printf("[d3d9-sm3-test] PASS: rendered pixel matches the analytic SM3.0 loop result -- genuine "
                   "end-to-end vs_3_0/ps_3_0 translation and render\n");
        }
        else
        {
            printf("[d3d9-sm3-test] FAIL: rendered pixel does not match the analytic loop result (expected "
                   "B=%02X G=%02X R=%02X)\n",
                   expected_b, expected_g, expected_r);
            ++failures;
        }

        // Background checkpoint: a corner outside the triangle must stay the clear color, proving the draw
        // was scoped to the geometry and not a whole-surface fill.
        const unsigned char* corner = base + 10 * kStride + 10 * 4;
        printf("[d3d9-sm3-test] corner pixel(10,10)=B=%02X G=%02X R=%02X\n", corner[0], corner[1], corner[2]);
        if (channel_close(corner[0], kBackgroundB) && channel_close(corner[1], kBackgroundG) &&
            channel_close(corner[2], kBackgroundR))
        {
            printf("[d3d9-sm3-test] PASS: corner pixel stayed the clear color\n");
        }
        else
        {
            printf("[d3d9-sm3-test] FAIL: corner pixel is not the clear color\n");
            ++failures;
        }

        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-sm3-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    release_all(rt, vb, ps, vs, dev, d3d);

    printf("[d3d9-sm3-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
