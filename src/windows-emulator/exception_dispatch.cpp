#include "std_include.hpp"
#include "exception_dispatch.hpp"
#include "process_context.hpp"
#include "cpu_context.hpp"
#include "windows_emulator.hpp"

#include "segment_utils.hpp"

namespace sogen
{

    namespace
    {
        using exception_record = EMU_EXCEPTION_RECORD<EmulatorTraits<Emu64>>;
        using exception_record_map = std::unordered_map<const exception_record*, emulator_object<exception_record>>;

        emulator_object<exception_record> save_exception_record(emulator_allocator& allocator, const exception_record& record,
                                                                exception_record_map& record_mapping)
        {
            const auto record_obj = allocator.reserve<exception_record>();
            record_obj.write(record);

            if (record.ExceptionRecord)
            {
                record_mapping.emplace(&record, record_obj);

                emulator_object<exception_record> nested_record_obj{allocator.get_memory()};
                const auto nested_record = record_mapping.find(reinterpret_cast<exception_record*>(record.ExceptionRecord));

                if (nested_record != record_mapping.end())
                {
                    nested_record_obj = nested_record->second;
                }
                else
                {
                    nested_record_obj =
                        save_exception_record(allocator, *reinterpret_cast<exception_record*>(record.ExceptionRecord), record_mapping);
                }

                record_obj.access([&](exception_record& r) {
                    r.ExceptionRecord = nested_record_obj.value(); //
                });
            }

            return record_obj;
        }

        emulator_object<exception_record> save_exception_record(emulator_allocator& allocator, const exception_record& record)
        {
            exception_record_map record_mapping{};
            return save_exception_record(allocator, record, record_mapping);
        }

        uint32_t map_violation_operation_to_parameter(const memory_operation operation)
        {
            switch (operation)
            {
            default:
            case memory_operation::read:
                return 0;
            case memory_operation::write:
            case memory_operation::exec:
                return 1;
            }
        }

        size_t calculate_exception_record_size(const exception_record& record)
        {
            std::unordered_set<const exception_record*> records{};
            size_t total_size = 0;

            const exception_record* current_record = &record;
            while (current_record)
            {
                if (!records.insert(current_record).second)
                {
                    break;
                }

                total_size += sizeof(*current_record);
                current_record = reinterpret_cast<exception_record*>(record.ExceptionRecord);
            }

            return total_size;
        }

        struct machine_frame
        {
            uint64_t rip;
            uint64_t cs;
            uint64_t eflags;
            uint64_t rsp;
            uint64_t ss;
        };

        void dispatch_exception_pointers(x86_64_cpu& emu, const uint64_t dispatcher,
                                         const EMU_EXCEPTION_POINTERS<EmulatorTraits<Emu64>> pointers)
        {
            constexpr auto mach_frame_size = 0x40;
            constexpr auto context_record_size = 0x4F0;
            const auto exception_record_size =
                calculate_exception_record_size(*reinterpret_cast<exception_record*>(pointers.ExceptionRecord));
            const auto combined_size = align_up(exception_record_size + context_record_size, 0x10);

            assert(combined_size == 0x590);

            const auto allocation_size = combined_size + mach_frame_size;

            const auto initial_sp = emu.reg(x86_register::rsp);
            const auto new_sp = align_down(initial_sp - allocation_size, 0x100);

            const auto total_size = initial_sp - new_sp;
            assert(total_size >= allocation_size);

            std::vector<uint8_t> zero_memory{};
            zero_memory.resize(static_cast<size_t>(total_size), 0);

            emu.write_memory(new_sp, zero_memory.data(), zero_memory.size());

            const emulator_object<CONTEXT64> context_record_obj{emu, new_sp};
            context_record_obj.write(*reinterpret_cast<CONTEXT64*>(pointers.ContextRecord));

            emulator_allocator allocator{emu, new_sp + context_record_size, exception_record_size};
            const auto exception_record_obj =
                save_exception_record(allocator, *reinterpret_cast<exception_record*>(pointers.ExceptionRecord));

            if (exception_record_obj.value() != allocator.get_base())
            {
                throw std::runtime_error("Bad exception record position on stack");
            }

            const emulator_object<machine_frame> machine_frame_obj{emu, new_sp + combined_size};
            machine_frame_obj.access([&](machine_frame& frame) {
                const auto& record = *reinterpret_cast<CONTEXT64*>(pointers.ContextRecord);
                frame.rip = record.Rip;
                frame.rsp = record.Rsp;
                frame.ss = record.SegSs;
                frame.cs = record.SegCs;
                frame.eflags = record.EFlags;
            });

            emu.reg(x86_register::rsp, new_sp);
            emu.reg(x86_register::rip, dispatcher);
        }

        WOW64_CONTEXT make_wow64_context(const CONTEXT64& ctx)
        {
            WOW64_CONTEXT result{};
            result.ContextFlags = CONTEXT32_ALL;
            result.Dr0 = static_cast<DWORD>(ctx.Dr0);
            result.Dr1 = static_cast<DWORD>(ctx.Dr1);
            result.Dr2 = static_cast<DWORD>(ctx.Dr2);
            result.Dr3 = static_cast<DWORD>(ctx.Dr3);
            result.Dr6 = static_cast<DWORD>(ctx.Dr6);
            result.Dr7 = static_cast<DWORD>(ctx.Dr7);
            result.SegGs = ctx.SegGs;
            result.SegFs = ctx.SegFs;
            result.SegEs = ctx.SegEs;
            result.SegDs = ctx.SegDs;
            result.Edi = static_cast<DWORD>(ctx.Rdi);
            result.Esi = static_cast<DWORD>(ctx.Rsi);
            result.Ebx = static_cast<DWORD>(ctx.Rbx);
            result.Edx = static_cast<DWORD>(ctx.Rdx);
            result.Ecx = static_cast<DWORD>(ctx.Rcx);
            result.Eax = static_cast<DWORD>(ctx.Rax);
            result.Ebp = static_cast<DWORD>(ctx.Rbp);
            result.Eip = static_cast<DWORD>(ctx.Rip);
            result.SegCs = ctx.SegCs;
            result.EFlags = ctx.EFlags;
            result.Esp = static_cast<DWORD>(ctx.Rsp);
            result.SegSs = ctx.SegSs;
            result.FloatSave.ControlWord = 0x037F;
            result.FloatSave.TagWord = 0xFFFF;

            XMM_SAVE_AREA32 xmm_state{};
            xmm_state.ControlWord = 0x037F;
            // FXSAVE abridged x87 tag: 0x00 = empty stack (fresh FPU). 0xFF (all valid) makes the next x87
            // FLD overflow the stack and yield the x87 indefinite (NaN).
            xmm_state.TagWord = 0x00;
            xmm_state.MxCsr = 0x1F80;
            xmm_state.MxCsr_Mask = 0xFFFFFFFF;
            static_assert(sizeof(xmm_state) <= sizeof(result.ExtendedRegisters));
            memcpy(result.ExtendedRegisters, &xmm_state, sizeof(xmm_state));
            return result;
        }

        EMU_EXCEPTION_RECORD<EmulatorTraits<Emu32>> make_wow64_exception_record(const exception_record& record)
        {
            EMU_EXCEPTION_RECORD<EmulatorTraits<Emu32>> result{};
            result.ExceptionCode = record.ExceptionCode;
            result.ExceptionFlags = record.ExceptionFlags;
            result.ExceptionRecord = 0;
            result.ExceptionAddress = static_cast<uint32_t>(record.ExceptionAddress);
            result.NumberParameters = record.NumberParameters;
            for (size_t i = 0; i < std::size(result.ExceptionInformation); ++i)
            {
                result.ExceptionInformation[i] = static_cast<uint32_t>(record.ExceptionInformation[i]);
            }
            return result;
        }

        // Real x86 ntdll.dll!KiUserExceptionDispatcher(PEXCEPTION_RECORD, PCONTEXT) is entered directly
        // by the kernel (no `call`, no return address on the stack): confirmed via disassembly of the
        // real 32-bit ntdll.dll that it reads `[esp+0]` as PEXCEPTION_RECORD and `[esp+4]` as PCONTEXT,
        // then tail-calls the real RtlDispatchException with them. Since a wow64 thread's 32-bit
        // ntdll.dll is genuine Windows code already mapped in guest memory, dispatch only has to hand
        // control to it correctly - RtlDispatchException performs the real FS:[0] SEH chain walk and
        // invokes the guest's actual handler(s) itself, with no need for sogen to reimplement any of
        // that. A wow64 thread that faults while running 32-bit code never has to leave 32-bit mode for
        // exception delivery at all (unlike the heaven's-gate-crossing dispatch_exception_pointers uses
        // for genuinely 64-bit-mode faults) - this matches how the real kernel picks the dispatch entry
        // point from the trap frame's Cs at fault time (Wow64SharedInformation, populated in
        // module_manager.cpp, exists precisely so the kernel/wow64 layer knows this 32-bit address).
        //
        // The PCONTEXT passed to the dispatcher must be thread.wow64_cpu_reserved's Context field, not
        // some arbitrary scratch copy: real wow64's own machinery (e.g. the translation NtContinue's
        // wow64 syscall thunk performs when the guest's SEH filter resumes execution) reads/writes the
        // 32-bit context from that well-known, TEB-referenced location - confirmed by sync_wow64_cpu_-
        // reserved_context's own doc comment below ("Wow64PassExceptionToGuest rebuilds the 32-bit
        // context from WOW64_CPURESERVED"). Handing it a disconnected copy left later stages (observed:
        // a wow64cpu.dll-internal single-step immediately after a handled STATUS_BREAKPOINT) restoring
        // CPU state from stale/uninitialized data instead of this dispatch's actual context.
        void dispatch_exception_pointers_wow64(x86_64_cpu& emu, const uint64_t dispatcher32, const uint64_t wow64_context_addr,
                                               const EMU_EXCEPTION_POINTERS<EmulatorTraits<Emu64>> pointers)
        {
            const auto& ctx = *reinterpret_cast<CONTEXT64*>(pointers.ContextRecord);
            const auto& record = *reinterpret_cast<exception_record*>(pointers.ExceptionRecord);

            const auto wow64_record = make_wow64_exception_record(record);

            constexpr uint64_t args_frame_size = 8;
            const uint64_t total_size = align_up(sizeof(wow64_record) + args_frame_size, 0x10);
            const auto current_esp = ctx.Rsp & 0xFFFFFFFFu;
            const auto new_esp = align_down(current_esp - total_size, 0x10);

            const auto record_addr = new_esp;
            const auto args_frame_addr = record_addr + sizeof(wow64_record);

            const emulator_object<EMU_EXCEPTION_RECORD<EmulatorTraits<Emu32>>> record_obj{emu, record_addr};
            record_obj.write(wow64_record);

            // NONCONTINUABLEDIAG: RtlDispatchException is confirmed (via disassembly) to refuse
            // continuation and raise STATUS_NONCONTINUABLE_EXCEPTION whenever the original
            // EXCEPTION_RECORD's ExceptionFlags bit 0 (EXCEPTION_NONCONTINUABLE) is set - this exactly
            // matches the observed infinite loop (UnhandledExceptionFilter's callback returns -1, but
            // resumption is refused, so a fresh STATUS_NONCONTINUABLE_EXCEPTION gets raised and
            // re-dispatched forever). record.ExceptionFlags is set to 0 in dispatch_exception, but
            // verify the actual value written here and read back from guest memory to rule out any
            // corruption/miscomputation before assuming the C++ source is what's actually landing.
            {
                const auto readback = record_obj.try_read();
                fprintf(stderr,
                        "[NONCONTINUABLEDIAG] wow64_record.ExceptionFlags=0x%x wow64_record.ExceptionCode=0x%x record_addr=0x%llx "
                        "readback_ok=%d readback.ExceptionFlags=0x%x readback.ExceptionCode=0x%x\n",
                        static_cast<unsigned int>(wow64_record.ExceptionFlags), static_cast<unsigned int>(wow64_record.ExceptionCode),
                        static_cast<unsigned long long>(record_addr), readback.has_value() ? 1 : 0,
                        static_cast<unsigned int>(readback ? readback->ExceptionFlags : 0),
                        static_cast<unsigned int>(readback ? readback->ExceptionCode : 0));
                fflush(stderr);
            }

            const std::array<uint32_t, 2> args_frame = {static_cast<uint32_t>(record_addr), static_cast<uint32_t>(wow64_context_addr)};
            emu.write_memory(args_frame_addr, args_frame.data(), sizeof(args_frame));

            // SEHCHAINDIAG: a Smoke Test Windows x86 run confirmed the crash from the stale-context bug
            // is gone, but the run then hung indefinitely - _seh_filter_exe/anti-debug-check/
            // NtSetInformationThread repeating thousands of times with no new dispatch_exception call
            // in between, meaning real ntdll's own FS:[0] walk (or _seh_filter_exe's own logic) is
            // looping entirely in already-dispatched guest code. Dump the chain right before handing
            // off to the real dispatcher, with cycle detection, to see whether it's already circular/
            // malformed at this exact point (before real ntdll ever touches it) or looks fine here.
            {
                const auto fs_base = emu.get_segment_base(x86_register::fs);
                uint32_t chain_addr = 0;
                emu.try_read_memory(fs_base, &chain_addr, sizeof(chain_addr));
                fprintf(stderr, "[SEHCHAINDIAG] dispatch fs_base=0x%llx head=0x%x wow64_context_addr=0x%llx record_addr=0x%llx\n",
                        static_cast<unsigned long long>(fs_base), chain_addr, static_cast<unsigned long long>(wow64_context_addr),
                        static_cast<unsigned long long>(record_addr));

                std::unordered_set<uint32_t> seen{};
                for (int i = 0; i < 32 && chain_addr != 0xFFFFFFFFu && chain_addr != 0; ++i)
                {
                    if (!seen.insert(chain_addr).second)
                    {
                        fprintf(stderr, "[SEHCHAINDIAG] CYCLE detected re-visiting 0x%x after %d entries\n", chain_addr, i);
                        break;
                    }

                    struct
                    {
                        uint32_t next;
                        uint32_t handler;
                    } rec{};

                    if (!emu.try_read_memory(chain_addr, &rec, sizeof(rec)))
                    {
                        fprintf(stderr, "[SEHCHAINDIAG] chain[%d] @0x%x unreadable\n", i, chain_addr);
                        break;
                    }

                    fprintf(stderr, "[SEHCHAINDIAG] chain[%d] @0x%x next=0x%x handler=0x%x\n", i, chain_addr, rec.next, rec.handler);
                    chain_addr = rec.next;
                }
                fflush(stderr);
            }

            emu.reg(x86_register::esp, args_frame_addr);
            emu.reg(x86_register::eip, dispatcher32);
        }

        void sync_wow64_cpu_reserved_context(windows_emulator& win_emu, x86_64_cpu& emu, emulator_thread& thread, const CONTEXT64& ctx)
        {
            if (!win_emu.process.is_wow64_process)
            {
                return;
            }

            const auto bitness = segment_utils::get_segment_bitness(emu, ctx.SegCs);
            if (!bitness || *bitness != segment_utils::segment_bitness::bit32)
            {
                return;
            }

            // Wow64PassExceptionToGuest rebuilds the 32-bit context from WOW64_CPURESERVED
            // (TEB64 TLS slot 1), not from the native exception ContextRecord below.
            thread.wow64_cpu_reserved->access([&](WOW64_CPURESERVED& cpu) {
                cpu.Flags |= WOW64_CPURESERVED_FLAG_RESET_STATE;
                cpu.Context = make_wow64_context(ctx);
            });
        }

    }

    // WOW64LOOPDIAG: set once the breakpoint dispatch preceding the observed post-breakpoint hang
    // happens, so thread.cpp/memory.cpp's gated diagnostics capture calls from inside the actual loop
    // instead of being exhausted by unrelated startup-time calls to the same syscalls.
    std::atomic<bool> g_wow64_post_breakpoint_diag{false};

    bool dispatch_debug_exception(windows_emulator& win_emu, CONTEXT64& ctx, EMU_EXCEPTION_RECORD<EmulatorTraits<Emu64>>& record)
    {
        std::array<uint8_t, 2> ins = {0};

        // CD 2D int 2dh
        if (win_emu.memory.try_read_memory(ctx.Rip, &ins, sizeof(ins)) && ins[0] == 0xCD && ins[1] == 0x2D)
        {
            // skip 2 bytes int 2dh
            ctx.Rip += 2;

            record.NumberParameters = 3;

            record.ExceptionInformation[0] = ctx.Rax;
            record.ExceptionInformation[1] = ctx.Rcx;
            record.ExceptionInformation[2] = ctx.Rdx;

            return true;
        }

        return false;
    }

    void dispatch_exception(windows_emulator& win_emu, vcpu_context& vcpu, const DWORD status,
                            const std::vector<EmulatorTraits<Emu64>::ULONG_PTR>& parameters)
    {
        auto& thread = vcpu.thread();

        win_emu.record_exception_trace({
            .status = static_cast<uint32_t>(status),
            .tid = thread.id,
            .vcpu = static_cast<uint32_t>(vcpu.cpu.index()),
            .rip = vcpu.cpu.read_instruction_pointer(),
            .info = parameters.size() > 1 ? static_cast<uint64_t>(parameters[1]) : 0,
        });

        CONTEXT64 ctx{};
        ctx.ContextFlags = CONTEXT64_ALL;
        cpu_context::save(vcpu.cpu, ctx);
        ctx.Rip = win_emu.uses_instruction_precision() //
                      ? thread.current_ip
                      : vcpu.cpu.read_instruction_pointer();

        fprintf(stderr,
                "[EXCDIAG5] status=0x%x ctx.Rsp=0x%llx ctx.Rbp=0x%llx ctx.SegCs=0x%llx ctx.SegSs=0x%llx cs_reg=0x%x rip=0x%llx "
                "ctx.R12=0x%llx ctx.R13=0x%llx ctx.R14=0x%llx ctx.R15=0x%llx\n",
                static_cast<unsigned int>(status), static_cast<unsigned long long>(ctx.Rsp), static_cast<unsigned long long>(ctx.Rbp),
                static_cast<unsigned long long>(ctx.SegCs), static_cast<unsigned long long>(ctx.SegSs),
                vcpu.cpu.reg<uint16_t>(x86_register::cs), static_cast<unsigned long long>(ctx.Rip),
                static_cast<unsigned long long>(ctx.R12), static_cast<unsigned long long>(ctx.R13),
                static_cast<unsigned long long>(ctx.R14), static_cast<unsigned long long>(ctx.R15));
        fflush(stderr);

        // FEXCore's JIT translation of INT3 reports RIP already advanced past the trapping 0xCC
        // (see reports_breakpoint_rip_past_instruction's doc comment) - real hardware/NT
        // (KiBreakpointTrap) always reports #BP at the INT3 itself, which is what guest SEH/VEH
        // handlers and ContextRecord->Rip adjustments (e.g. `Rip += 1` to step past it) expect.
        // KVM/WHP already report the pre-advance address like real hardware, so this is scoped to
        // the one backend that actually needs it - applying it more broadly landed one byte short
        // of where KVM/WHP's own guest-visible breakpoints (e.g. anti-debug INT3 padding probes)
        // expect to resume, causing an infinite re-fault loop there.
        if (status == STATUS_BREAKPOINT && vcpu.cpu.reports_breakpoint_rip_past_instruction())
        {
            ctx.Rip -= 1;
        }

        exception_record record{};
        memset(&record, 0, sizeof(record));
        record.ExceptionCode = status;
        record.ExceptionFlags = 0;
        record.ExceptionRecord = 0;
        record.NumberParameters = 0;

        bool is_debug_exception = false;
        if (status == STATUS_BREAKPOINT)
        {
            is_debug_exception = dispatch_debug_exception(win_emu, ctx, record);
        }

        // ContextRecord->Eip must reach the guest's first-chance handler UNADJUSTED (the int3's own
        // address), matching the general real-hardware/NT contract documented on
        // reports_breakpoint_rip_past_instruction() above and on every other backend (KVM/WHP/
        // Unicorn already report the unadjusted address; only FEX needed the -1 correction just
        // above). A previous version of this code added a wow64-32-bit-only +1 pre-advance here,
        // reasoning that ContextRecord->Eip arrives "already one past" the int3 for wow64 - this
        // directly contradicted the general contract two paragraphs up and was disproven live: with
        // it in place, test-sample.exe's own test_unhandled_exception() (its top-level filter does
        // `ContextRecord->Eip += 1` itself, exactly the self-adjustment the general contract expects
        // callers to make) ends up double-advanced by 2 bytes total, landing execution mid-instruction
        // one byte into whatever follows the int3 - confirmed via a live GDB-stub session (breakpoints
        // on RtlDispatchException/RtlpExecuteHandlerForException/__except_handler4 in the real syswow64
        // ntdll.dll) that this is exactly what produces the observed STATUS_NONCONTINUABLE_EXCEPTION
        // (0xc0000025) infinite recursion: the corrupted resume raises a new exception whose handler
        // (ntdll's own __except_handler4) returns ExceptionContinueExecution for it, which
        // RtlDispatchException correctly refuses (since EXCEPTION_NONCONTINUABLE is set) by raising a
        // fresh STATUS_NONCONTINUABLE_EXCEPTION - recursing until the stack is exhausted. Excludes the
        // dispatch_debug_exception (int 2dh) case above, which already advances ctx.Rip past its own,
        // differently-sized instruction.
        const auto cs_selector = vcpu.cpu.reg<uint16_t>(x86_register::cs);
        const auto bitness = segment_utils::get_segment_bitness(vcpu.cpu, cs_selector);
        const auto is_bit32 = bitness && *bitness == segment_utils::segment_bitness::bit32;

        if (!is_debug_exception)
        {
            record.NumberParameters = static_cast<DWORD>(parameters.size());

            if (parameters.size() > 15)
            {
                throw std::runtime_error("Too many exception parameters");
            }

            for (size_t i = 0; i < parameters.size(); ++i)
            {
                record.ExceptionInformation[i] = parameters[i];
            }
        }

        record.ExceptionAddress = ctx.Rip;

        sync_wow64_cpu_reserved_context(win_emu, vcpu.cpu, thread, ctx);

        EMU_EXCEPTION_POINTERS<EmulatorTraits<Emu64>> pointers{};
        pointers.ContextRecord = reinterpret_cast<EmulatorTraits<Emu64>::PVOID>(&ctx);
        pointers.ExceptionRecord = reinterpret_cast<EmulatorTraits<Emu64>::PVOID>(&record);

        if (is_bit32 && win_emu.process.ki_user_exception_dispatcher32 && thread.wow64_cpu_reserved)
        {
            if (status == STATUS_BREAKPOINT)
            {
                // WOW64LOOPDIAG: flips on once the breakpoint dispatch that precedes the observed
                // post-breakpoint hang happens, so thread.cpp/memory.cpp's diagnostics (gated on this
                // flag) capture calls from inside the actual loop instead of being exhausted by
                // unrelated startup-time calls to the same syscalls long before this point.
                g_wow64_post_breakpoint_diag.store(true);
            }

            const auto wow64_context_addr = thread.wow64_cpu_reserved->value() + offsetof(WOW64_CPURESERVED, Context);
            dispatch_exception_pointers_wow64(vcpu.cpu, *win_emu.process.ki_user_exception_dispatcher32, wow64_context_addr, pointers);
            return;
        }

        dispatch_exception_pointers(vcpu.cpu, win_emu.process.ki_user_exception_dispatcher, pointers);
    }

    void dispatch_access_violation(windows_emulator& win_emu, vcpu_context& vcpu, const uint64_t address, const memory_operation operation)
    {
        dispatch_exception(win_emu, vcpu, STATUS_ACCESS_VIOLATION,
                           {
                               map_violation_operation_to_parameter(operation),
                               address,
                           });
    }

    void dispatch_guard_page_violation(windows_emulator& win_emu, vcpu_context& vcpu, const uint64_t address,
                                       const memory_operation operation)
    {
        dispatch_exception(win_emu, vcpu, STATUS_GUARD_PAGE_VIOLATION,
                           {
                               map_violation_operation_to_parameter(operation),
                               address,
                           });
    }

    void dispatch_illegal_instruction_violation(windows_emulator& win_emu, vcpu_context& vcpu)
    {
        dispatch_exception(win_emu, vcpu, STATUS_ILLEGAL_INSTRUCTION, {});
    }

    void dispatch_integer_division_by_zero(windows_emulator& win_emu, vcpu_context& vcpu)
    {
        dispatch_exception(win_emu, vcpu, STATUS_INTEGER_DIVIDE_BY_ZERO, {});
    }

    void dispatch_single_step(windows_emulator& win_emu, vcpu_context& vcpu)
    {
        dispatch_exception(win_emu, vcpu, STATUS_SINGLE_STEP, {});
    }

    void dispatch_breakpoint(windows_emulator& win_emu, vcpu_context& vcpu)
    {
        dispatch_exception(win_emu, vcpu, STATUS_BREAKPOINT, {});
    }

} // namespace sogen
