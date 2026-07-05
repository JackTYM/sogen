// D3D9-over-Vulkan int (i#) / bool (b#) shader constant register end-to-end test (see
// .claude/plans/jazzy-giggling-cloud.md, Task 4). Mirrors d3d9_const_test.cpp's proven float-constant
// (c#) structure, but exercises REAL vs_2_0 flow control instead of a straight-line shader: a static
// bool branch selects between two unambiguous branch colors, and a register-bound for-loop
// (register(i0)) accumulates a small per-iteration delta into the blue channel.
//
// Two non-obvious d3dcompiler_43/HLSL findings drove this shader's exact shape (both confirmed
// empirically by compiling isolated variants and inspecting the resulting D3DBC bytecode):
// 1. `bool x : register(b0);` is REJECTED by d3dcompiler_43 for vs_2_0/vs_2_a with error X4509
//    ("invalid register semantic 'b0', ... c register binding required"). A scalar bool used in a
//    runtime "if" must be declared with NO explicit register annotation; the compiler auto-allocates
//    it (confirmed to land at b0, the only bool declared, matching SetVertexShaderConstantB(0, ...)).
// 2. Far more important: an `if (b) { X } else { Y }` shape where both branches merge into one
//    trailing assignment gets FLATTENED by d3dcompiler_43 into select-style arithmetic (SGE/MAD) backed
//    by an auto-allocated FLOAT (c#) register -- NOT the b0 CONSTBOOL register bank at all. Confirmed by
//    disassembling the resulting bytecode: zero CONSTBOOL (register type 0x0E) operands anywhere, and
//    the branch silently always read the default/never-set c-register (this was caught red-handed: an
//    earlier version of this shader used exactly this if/else shape, compiled and ran successfully, but
//    always rendered the "false" branch regardless of SetVertexShaderConstantB(0, TRUE, 1) -- a false
//    negative that would have gone unnoticed without disassembling the bytecode). The fix: give each
//    branch an early `return` instead of a shared trailing write. This is NOT flattenable (the compiler
//    can't reduce an early exit to arithmetic) and empirically forces a real D3DBC IF instruction whose
//    operand IS the genuine CONSTBOOL b0 register (confirmed: opcode 0x28, operand register type=0x0E,
//    number=0).
//
// Both the bool and the int4 loop trip count are supplied ONLY via runtime SetVertexShaderConstantB/
// SetVertexShaderConstantI calls -- never as HLSL-side literals/defb/defi -- so vkd3d-shader's D3DBC
// frontend cannot const-fold either register out of the CBV (see vsir_program_normalise_flat_constants
// in deps/vkd3d/libs/vkd3d-shader/ir.c).
//
// Loop-vs-unroll verification: the VS bytecode is dumped and walked as real D3DBC tokens (opcode in
// the low 16 bits, instruction length in bits 24-27, per vkd3d-shader's own
// VKD3D_SM1_INSTRUCTION_LENGTH_SHIFT/_MASK in deps/vkd3d/libs/vkd3d-shader/d3dbc.c) looking for a
// LOOP/ENDLOOP or REP/ENDREP opcode pair (0x1B/0x1D or 0x26/0x27), and for the IF opcode (0x28)
// confirming its operand is the real b0 CONSTBOOL register (type 0x0E). Since loopTripCount.x is a
// genuine runtime register read with no compile-time literal anywhere in the HLSL source,
// d3dcompiler_43 has no fixed trip count to unroll around in the first place -- the dump confirms which
// real loop opcode it chose, not just that unrolling didn't happen.
//
// Renders one triangle to an off-screen render target and LockRect-reads back a single point (the
// output color is constant across the whole triangle, so any interior point works): the R/G channels
// prove the bool branch was taken from the real runtime b0 value, and the B channel proves the loop
// ran exactly N iterations driven by the real runtime i0 value.

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
bool useAltColor;
int4 loopTripCount : register(i0);
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);

    float accum = 0.0;
    for (int k = 0; k < loopTripCount.x; k++)
    {
        accum += 0.05;
    }

    // The early-return shape (as opposed to if/else assigning a shared trailing variable) is
    // required to force a genuine D3DBC IF instruction reading the b0 CONSTBOOL register -- see this
    // file's header comment. An if/else merging into one trailing output.color write gets flattened by
    // d3dcompiler_43 into select-style arithmetic backed by an auto-allocated FLOAT (c#) register that
    // SetVertexShaderConstantB never populates, silently defeating the whole point of this test
    // (confirmed empirically; see the header comment's "genuine host/compiler finding" note).
    if (useAltColor)
    {
        output.color = float4(1.0, 0.0, accum, 1.0);
        return output;
    }
    output.color = float4(0.0, 1.0, accum, 1.0);
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

    // Runtime-only inputs -- never baked into the HLSL above.
    constexpr BOOL kUseAltColor = TRUE;
    constexpr int kLoopTripCount[4] = {3, 0, 0, 0};
    constexpr float kAccumDelta = 0.05f;

    constexpr unsigned char kBackgroundB = 30;
    constexpr unsigned char kBackgroundG = 30;
    constexpr unsigned char kBackgroundR = 30;

    bool channel_close(const unsigned char actual, const int expected)
    {
        return std::abs(static_cast<int>(actual) - expected) <= 2;
    }

    constexpr uint32_t kD3dsioOpcodeMask = 0x0000FFFFu;
    constexpr uint32_t kD3dsioLengthShift = 24u;
    constexpr uint32_t kD3dsioLengthMask = 0x0Fu << kD3dsioLengthShift;
    constexpr uint32_t kD3dsioComment = 0xFFFEu;
    constexpr uint32_t kD3dsioEnd = 0xFFFFu;
    constexpr uint32_t kD3dsioLoop = 0x1Bu;
    constexpr uint32_t kD3dsioEndLoop = 0x1Du;
    constexpr uint32_t kD3dsioRep = 0x26u;
    constexpr uint32_t kD3dsioEndRep = 0x27u;
    constexpr uint32_t kD3dsioIf = 0x28u;
    constexpr uint32_t kD3dsioIfc = 0x29u;
    constexpr uint32_t kVkd3dSm1RegConstBool = 0x0Eu;

    // Register-token layout per vkd3d-shader's own d3dbc.c (VKD3D_SM1_REGISTER_NUMBER_MASK/
    // _TYPE_SHIFT/_TYPE_SHIFT2): register number in the low 11 bits, register type split across bits
    // 28-30 and bits 8-12 (5 bits total). VKD3D_SM1_REG_CONSTINT=0x07, VKD3D_SM1_REG_CONSTBOOL=0x0e.
    uint32_t register_type_of(const uint32_t reg_token)
    {
        return ((reg_token >> 28) & 0x7u) | ((reg_token >> 8) & 0x18u);
    }

    uint32_t register_number_of(const uint32_t reg_token)
    {
        return reg_token & 0x7FFu;
    }

    // Walks a D3DBC (SM1-3) token stream as real instructions (opcode in the low 16 bits, instruction
    // length in bits 24-27, matching vkd3d-shader's own VKD3D_SM1_INSTRUCTION_LENGTH_SHIFT/_MASK), and
    // reports whether a genuine LOOP/ENDLOOP or REP/ENDREP opcode pair is present, and whether a genuine
    // IF instruction reads the b0 CONSTBOOL register (as opposed to being flattened away -- see this
    // file's header comment). Also prints the register type/number of IF's and REP's source operand, to
    // independently confirm which physical b#/i# register the compiler actually bound
    // useAltColor/loopTripCount to.
    void scan_for_loop_opcode(const uint32_t* tokens, const size_t token_count, bool& out_saw_loop,
                               bool& out_saw_rep, bool& out_saw_if_reading_bool0)
    {
        out_saw_loop = false;
        out_saw_rep = false;
        out_saw_if_reading_bool0 = false;
        if (token_count == 0)
        {
            return;
        }

        size_t i = 1; // token[0] is the version token.
        while (i < token_count)
        {
            const uint32_t token = tokens[i];
            const uint32_t opcode = token & kD3dsioOpcodeMask;
            if (opcode == kD3dsioEnd)
            {
                break;
            }
            if (opcode == kD3dsioComment)
            {
                const uint32_t comment_len = (token >> 16) & 0x7FFFu;
                i += 1 + comment_len;
                continue;
            }

            const uint32_t length = (token & kD3dsioLengthMask) >> kD3dsioLengthShift;

            if (opcode == kD3dsioLoop || opcode == kD3dsioEndLoop)
            {
                out_saw_loop = true;
            }
            if (opcode == kD3dsioRep || opcode == kD3dsioEndRep)
            {
                out_saw_rep = true;
            }
            if ((opcode == kD3dsioRep || opcode == kD3dsioIf) && length >= 1 && i + 1 < token_count)
            {
                const uint32_t reg_token = tokens[i + 1];
                const uint32_t reg_type = register_type_of(reg_token);
                const uint32_t reg_number = register_number_of(reg_token);
                printf("[d3d9-int-bool-const-test]   opcode=0x%02X operand register type=0x%02X number=%u\n", opcode,
                       reg_type, reg_number);
                if (opcode == kD3dsioIf && reg_type == kVkd3dSm1RegConstBool && reg_number == 0)
                {
                    out_saw_if_reading_bool0 = true;
                }
            }
            if (opcode == kD3dsioIfc && length >= 2 && i + 2 < token_count)
            {
                const uint32_t reg_token_a = tokens[i + 1];
                const uint32_t reg_token_b = tokens[i + 2];
                printf("[d3d9-int-bool-const-test]   opcode=0x%02X (IFC) operandA type=0x%02X number=%u operandB "
                       "type=0x%02X number=%u\n",
                       opcode, register_type_of(reg_token_a), register_number_of(reg_token_a),
                       register_type_of(reg_token_b), register_number_of(reg_token_b));
            }

            i += 1 + length;
        }
    }
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-int-bool-const-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9intboolconsttest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "int-bool-const-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0,
                                 640, 480, nullptr, nullptr, wc.hInstance, nullptr);
    printf("[d3d9-int-bool-const-test] hwnd=%p\n", static_cast<void*>(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    printf("[d3d9-int-bool-const-test] Direct3DCreate9=%p\n", static_cast<void*>(d3d));
    if (!d3d)
    {
        printf("[d3d9-int-bool-const-test] FAIL: Direct3DCreate9 returned null\n");
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
    HRESULT hr =
        d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-int-bool-const-test] CreateDevice hr=0x%08lx dev=%p\n", static_cast<unsigned long>(hr),
           static_cast<void*>(dev));
    if (FAILED(hr) || !dev)
    {
        printf("[d3d9-int-bool-const-test] FAIL: CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-int-bool-const-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-int-bool-const-test] VS compile errors: %s\n",
                   static_cast<const char*>(vs_errors->GetBufferPointer()));
            vs_errors->Release();
        }
        dev->Release();
        d3d->Release();
        return 1;
    }

    {
        const auto* vs_tokens = static_cast<const uint32_t*>(vs_blob->GetBufferPointer());
        const size_t vs_token_count = vs_blob->GetBufferSize() / sizeof(uint32_t);
        printf("[d3d9-int-bool-const-test] VS bytecode size=%zu bytes (%zu tokens)\n", vs_blob->GetBufferSize(),
               vs_token_count);
        bool saw_loop = false;
        bool saw_rep = false;
        bool saw_if_bool0 = false;
        scan_for_loop_opcode(vs_tokens, vs_token_count, saw_loop, saw_rep, saw_if_bool0);
        printf("[d3d9-int-bool-const-test] VS bytecode opcode scan: saw_LOOP/ENDLOOP=%s saw_REP/ENDREP=%s "
               "saw_IF(b0)=%s\n",
               saw_loop ? "yes" : "no", saw_rep ? "yes" : "no", saw_if_bool0 ? "yes" : "no");
        if (!saw_loop && !saw_rep)
        {
            printf("[d3d9-int-bool-const-test] WARNING: neither a LOOP nor a REP opcode was found in the VS "
                   "bytecode -- the compiler may not have emitted a real hardware loop\n");
        }
        if (!saw_if_bool0)
        {
            printf("[d3d9-int-bool-const-test] WARNING: no IF instruction reading the b0 CONSTBOOL register was "
                   "found -- the bool branch may have been flattened into arithmetic that never reads b0\n");
        }
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-int-bool-const-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-int-bool-const-test] PS compile errors: %s\n",
                   static_cast<const char*>(ps_errors->GetBufferPointer()));
            ps_errors->Release();
        }
        vs_blob->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-int-bool-const-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));

    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-int-bool-const-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));

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
    printf("[d3d9-int-bool-const-test] CreateVertexBuffer hr=0x%08lx vb=%p\n", static_cast<unsigned long>(hcvb),
           static_cast<void*>(vb));
    if (!vb)
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
    printf("[d3d9-int-bool-const-test] CreateRenderTarget hr=0x%08lx surf=%p\n", static_cast<unsigned long>(hcrt),
           static_cast<void*>(rt));
    if (FAILED(hcrt) || !rt)
    {
        printf("[d3d9-int-bool-const-test] FAIL: CreateRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hcrt));
        vb->Release();
        vs->Release();
        ps->Release();
        dev->Release();
        d3d->Release();
        return 1;
    }

    HRESULT hsrt = dev->SetRenderTarget(0, rt);
    printf("[d3d9-int-bool-const-test] SetRenderTarget hr=0x%08lx\n", static_cast<unsigned long>(hsrt));

    dev->SetFVF(kFvf);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);

    HRESULT hsvb = dev->SetVertexShaderConstantB(0, &kUseAltColor, 1);
    printf("[d3d9-int-bool-const-test] SetVertexShaderConstantB hr=0x%08lx\n", static_cast<unsigned long>(hsvb));
    HRESULT hsvi = dev->SetVertexShaderConstantI(0, kLoopTripCount, 1);
    printf("[d3d9-int-bool-const-test] SetVertexShaderConstantI hr=0x%08lx\n", static_cast<unsigned long>(hsvi));

    dev->BeginScene();
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(kBackgroundR, kBackgroundG, kBackgroundB), 1.0f, 0);
    HRESULT hdp = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
    printf("[d3d9-int-bool-const-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdp));
    dev->EndScene();

    int failures = 0;

    D3DLOCKED_RECT lr{};
    HRESULT hlr = rt->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    printf("[d3d9-int-bool-const-test] LockRect hr=0x%08lx pBits=%p Pitch=%ld\n", static_cast<unsigned long>(hlr),
           lr.pBits, static_cast<long>(lr.Pitch));
    if (SUCCEEDED(hlr) && lr.pBits)
    {
        const auto* base = static_cast<const unsigned char*>(lr.pBits);
        constexpr LONG kStride = 640 * 4;

        // (col=320, row=240) -> NDC (~0.0, ~0.0), well inside the triangle (the output color is
        // uniform across the whole triangle, so any interior point works equally for both checks).
        const unsigned char* pixel = base + 240 * kStride + 320 * 4;
        printf("[d3d9-int-bool-const-test] pixel(320,240)=B=%02X G=%02X R=%02X A=%02X\n", pixel[0], pixel[1],
               pixel[2], pixel[3]);

        // Bool check: b0=TRUE must select the "alt" branch (R=255,G=0), not the default (R=0,G=255).
        constexpr int expected_r_alt = 255;
        constexpr int expected_g_alt = 0;
        if (!channel_close(pixel[2], expected_r_alt) || !channel_close(pixel[1], expected_g_alt))
        {
            printf("[d3d9-int-bool-const-test] FAIL: pixel R/G does not match the alt-branch color -- bool constant "
                   "register b0 did not select the alt branch\n");
            ++failures;
        }
        else
        {
            printf("[d3d9-int-bool-const-test] PASS: pixel R/G matches the alt-branch color, confirming the "
                   "runtime bool constant drove the VS branch\n");
        }

        // Int check: i0.x=3 loop iterations, each accumulating kAccumDelta into the blue channel.
        const int expected_b =
            static_cast<int>(static_cast<float>(kLoopTripCount[0]) * kAccumDelta * 255.0f + 0.5f);
        if (!channel_close(pixel[0], expected_b))
        {
            printf("[d3d9-int-bool-const-test] FAIL: pixel B=%02X does not match expected %02X -- int constant "
                   "register i0 did not drive the loop trip count\n",
                   pixel[0], expected_b);
            ++failures;
        }
        else
        {
            printf("[d3d9-int-bool-const-test] PASS: pixel B=%02X matches expected %02X, confirming the runtime "
                   "int constant drove exactly %d loop iterations\n",
                   pixel[0], expected_b, kLoopTripCount[0]);
        }

        rt->UnlockRect();
    }
    else
    {
        printf("[d3d9-int-bool-const-test] FAIL: LockRect hr=0x%08lx\n", static_cast<unsigned long>(hlr));
        ++failures;
    }

    rt->Release();
    vb->Release();
    vs->Release();
    ps->Release();
    dev->Release();
    d3d->Release();

    printf("[d3d9-int-bool-const-test] %s\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
