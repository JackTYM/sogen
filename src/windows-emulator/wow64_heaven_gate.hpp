#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sogen
{

    namespace wow64::heaven_gate
    {
        inline constexpr uint64_t kCodeBase = 0xFF300000ULL;
        inline constexpr std::size_t kCodeSize = 0x1000ULL;

        inline constexpr uint64_t kStackBase = 0xFF400000ULL;
        inline constexpr std::size_t kStackSize = 0x10000ULL;
        inline constexpr uint64_t kStackTop = kStackBase + kStackSize;

        inline constexpr uint16_t kUserCodeSelector = 0x33;
        inline constexpr uint16_t kUserStackSelector = 0x2B;

        inline constexpr std::array<uint8_t, 19> kTrampolineBytes{0x6A, 0x33, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x83, 0x04, 0x24,
                                                                  0x05, 0xCB, 0x52, 0x53, 0x9C, 0x51, 0x50, 0x48, 0xCF};

        // A second, unrelated trampoline in its OWN dedicated page (deliberately NOT sharing
        // kCodeBase/kCodeSize - see below): real ntdll's RtlUserThreadStart top-level SEH filter
        // unconditionally decodes and calls whatever ntdll-internal `_RtlpUnhandledExceptionFilter`
        // variable holds - normally populated by LdrpInitializeProcess calling
        // RtlSetUnhandledExceptionFilter(NULL) during the real loader's process init, a step
        // sogen's own process setup skips. Without it, the variable stays zero, decodes to a
        // garbage address, and guest code jumps into it. Redirecting the wow64 thread's initial EIP
        // here (instead of straight to RtlUserThreadStart32) lets real ntdll32 code perform that one
        // call itself before falling through to the real entry.
        //
        // This is ordinary same-bitness x86 control flow (push/call/jmp, no CS/SS switch) and needs
        // no gate-crossing handling of its own - but it must NOT be placed inside kCodeBase's page:
        // that whole page is registered with register_gate_crossing(kCodeBase, kCodeSize, ...), so
        // FEXCore intercepts ANY execution reaching ANY address within it (not just kCodeBase
        // itself) and runs its own heaven-gate mode-switch synthesis instead of just JIT-executing
        // the bytes there - confirmed as a real regression (wow64-test-sample.exe under
        // EMULATOR_FEX=1 crashed deep in thread-pool code with corrupted register state once this
        // trampoline was added at kCodeBase+offset). A separate page sidesteps that entirely.
        inline constexpr uint64_t kFilterTrampolineBase = 0xFF302000ULL;
        inline constexpr std::size_t kFilterTrampolineSize = 0x1000ULL;
    }

} // namespace sogen
