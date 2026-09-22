/*
Design notes:

1. emulator:               the machine interface (provides memory and hook interfaces).
2. typed_emulator<Traits>: a template that adapts to architecture/bitness via the Traits struct.
3. arch_emulator<Traits>:  a thin layer for architecture-specific logic, things that are shared by all x86 (32/64), or
                           all ARM (32/64), etc.
X. x86_emulator<Traits>:   x86_emulator<Traits> are specialisations for
                           x86 and ARM, parameterised by their respective traits (e.g., x86_64_traits) and stuff :)

Virtual CPUs are modelled separately (typed_cpu<Traits> -> x86_cpu<Traits>): a CPU owns register
and run state and delegates memory access to the machine. The machine exposes its CPUs via
get_cpu()/vcpu_count() and currently acts as its own single CPU (index 0) until backends grow
real per-vCPU objects (docs/multi-vcpu-design.md).

1. emulator (memory_interface, hook_interface)          typed_cpu<Traits> (cpu_interface)
2.  └── typed_emulator<address_t, register_t, ...>       └── x86_cpu<x86_64_traits>
3.         └── arch_emulator<arch_traits>                        │
              └── x86_emulator<x86_64_traits> ───────────────────┘ (implements its own CPU 0)
*/

#pragma once
#include "typed_emulator.hpp"
#include "typed_cpu.hpp"
#include "x86_register.hpp"

#include <stdexcept>

namespace sogen
{

    // --[Core]--------------------------------------------------------------------------

    template <typename Traits>
    struct arch_emulator : typed_emulator<Traits>
    {
    };

    template <typename Traits>
    struct x86_cpu : typed_cpu<Traits>
    {
        using register_type = Traits::register_type;
        using pointer_type = Traits::pointer_type;

        virtual void set_segment_base(register_type base, pointer_type value) = 0;
        virtual pointer_type get_segment_base(register_type base) = 0;
        virtual void load_gdt(pointer_type address, uint32_t limit) = 0;

        // Add new virtuals at the end of this class so the vtable slots of existing methods never
        // move; this keeps separately built backends (fex-emulator, kvm-emulator, ... can each be
        // built as their own shared library) ABI-compatible with existing callers.
        //
        // Called once before any module is mapped. Backends running on real x86-64 hardware ignore
        // this; the CPU switches to compatibility mode on the CS load alone. FEXCore compiles for a
        // fixed bitness and only stands up the 64-bit context, so it uses this to know, as early as
        // possible, whether it needs to stand up a second, 32-bit-mode context.
        virtual void notify_process_bitness(bool /*is_wow64_process*/)
        {
        }

        // Identifies which real WoW64 CPU-mode-switch mechanism lives at a registered gate-crossing
        // range, so a JIT backend knows which calling convention to decode when guest execution
        // reaches it (see register_gate_crossing).
        enum class gate_crossing_kind
        {
            // sogen's own synthetic heaven's-gate trampoline (wow64_heaven_gate.hpp, kCodeBase): a
            // 19-byte push/iretq sequence with the convention RAX=target RIP, RBX=target RSP,
            // RCX=target CS, RDX=target SS, current RFLAGS carried through. Used by
            // exception_dispatch.cpp to deliver a 64-bit exception to a thread currently running
            // 32-bit code.
            heaven_gate,
            // The real wow64cpu.dll turbo-thunk dispatcher (its TurboDispatchJumpAddressStart export:
            // `mov ecx,eax; shr ecx,0x10; jmp [r15+rcx*8]`). Its convention (EAX dispatch index, r15
            // jump-table base populated by BTCpuProcessInit) differs from the heaven's-gate one - see
            // perform_gate_crossing.
            wow64cpu_dispatch,
            // The real wow64cpu.dll forward (64->32) transition function RunSimulatedCode (RVA
            // 0x1650, called in a loop by BTCpuSimulate). Its `mov gs, cx` at RVA 0x16c7 is the exact
            // instruction FEXCore's fixed-bitness 64-bit JIT cannot compile, so this entry is
            // registered as a gate and intercepted before those bytes are ever compiled; the WoW64
            // CPU-area register block is decoded directly into the 32-bit Context instead - see
            // perform_gate_crossing.
            wow64_run_simulated_code,
            // wow64cpu.dll's own BTCpuProcessInit writes this into a dedicated, freshly-r-x'd page: a
            // bare `jmp far 0x33:<same page + a few bytes>` (opcode 0xEA, undefined in 64-bit long
            // mode). This is the real Wow64Transition entry point for this ntdll32/wow64cpu.dll build
            // combination - the 32-bit syscall stub's `call fs:[0xC0]` lands directly here with the
            // syscall number/args already live - so despite the different encoding, reaching it is
            // handled identically to wow64cpu_dispatch's thunk. See perform_gate_crossing.
            far_jmp_bitness_switch,
        };

        // Registers [address, address+size) as a WoW64 bitness gate crossing: a JIT backend
        // intercepts guest execution reaching it (its range is inherently non-executable to the JIT)
        // and marshals the CPU register file into its other-bitness Context and switches which one is
        // executing - the observable effect of the real hardware CS-segment mode switch that native
        // backends (KVM/Unicorn/WHP) perform transparently, hence the no-op default here.
        virtual void register_gate_crossing(pointer_type /*address*/, size_t /*size*/, gate_crossing_kind /*kind*/)
        {
        }

        // Tells a JIT backend wow64cpu.dll's real TurboDispatchJumpAddressEnd export address - the
        // generic 64-bit dispatch continuation a reverse (32->64) wow64cpu_dispatch gate crossing
        // resumes execution at. This can't be derived from a fixed RVA offset - it differs across
        // wow64cpu.dll builds/OS versions - so the caller resolves it from the real export table and
        // hands the address over directly. A no-op on native-execution backends.
        virtual void set_wow64_turbo_dispatch_end(pointer_type /*address*/)
        {
        }
    };

    template <typename Traits>
    struct x86_emulator : arch_emulator<Traits>, x86_cpu<Traits>
    {
        using registers = Traits::register_type;
        using register_type = Traits::register_type;
        using pointer_type = Traits::pointer_type;
        using hookable_instructions = Traits::hookable_instructions;

        // Both bases expose a memory surface (the machine's own and the CPU's
        // delegating one); they resolve to the same backend implementation.
        using arch_emulator<Traits>::read_memory;
        using arch_emulator<Traits>::try_read_memory;
        using arch_emulator<Traits>::write_memory;
        using arch_emulator<Traits>::try_write_memory;
        using arch_emulator<Traits>::move_memory;
        using arch_emulator<Traits>::set_memory;

        virtual size_t vcpu_count() const
        {
            return 1;
        }

        virtual x86_cpu<Traits>& get_cpu(const size_t index)
        {
            if (index >= this->vcpu_count())
            {
                throw std::out_of_range("Invalid vCPU index");
            }

            return *this;
        }

        size_t index() const override
        {
            return 0;
        }

        memory_interface& memory() override
        {
            return *this;
        }

        const memory_interface& memory() const override
        {
            return *this;
        }
    };

    template <typename Traits>
    struct arm_emulator : arch_emulator<Traits>
    {
    };

    enum class x86_hookable_instructions
    {
        invalid, // TODO: Get rid of that
        syscall,
        cpuid,
        rdtsc,
        rdtscp,
    };

    // --[x86_64]-------------------------------------------------------------------------

    struct x86_64_traits
    {
        using pointer_type = uint64_t;
        using register_type = x86_register;
        static constexpr register_type instruction_pointer = x86_register::rip;
        static constexpr register_type stack_pointer = x86_register::rsp;
        using hookable_instructions = x86_hookable_instructions;
    };

    using x86_64_cpu = x86_cpu<x86_64_traits>;
    using x86_64_emulator = x86_emulator<x86_64_traits>;

} // namespace sogen
