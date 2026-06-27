#include "std_include.hpp"
#include "exception_dispatch.hpp"
#include "process_context.hpp"
#include "cpu_context.hpp"
#include "windows_emulator.hpp"

#include "segment_utils.hpp"
#include "wow64_heaven_gate.hpp"

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

        void dispatch_exception_pointers(x86_64_emulator& emu, const uint64_t dispatcher,
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

            const auto cs_selector = emu.reg<uint16_t>(x86_register::cs);
            const auto bitness = segment_utils::get_segment_bitness(emu, cs_selector);

            if (!bitness || *bitness != segment_utils::segment_bitness::bit32)
            {
                emu.reg(x86_register::rsp, new_sp);
                emu.reg(x86_register::rip, dispatcher);
                return;
            }

            emu.reg(x86_register::rax, dispatcher);
            emu.reg(x86_register::rbx, new_sp);
            emu.reg(x86_register::rcx, static_cast<uint64_t>(wow64::heaven_gate::kUserCodeSelector));
            emu.reg(x86_register::rdx, static_cast<uint64_t>(wow64::heaven_gate::kUserStackSelector));
            emu.reg(x86_register::rsp, wow64::heaven_gate::kStackTop);
            emu.reg(x86_register::rip, wow64::heaven_gate::kCodeBase);
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

        void sync_wow64_cpu_reserved_context(windows_emulator& win_emu, CONTEXT64& ctx)
        {
            if (!win_emu.process.is_wow64_process)
            {
                return;
            }

            const auto bitness = segment_utils::get_segment_bitness(win_emu.emu(), ctx.SegCs);
            if (!bitness || *bitness != segment_utils::segment_bitness::bit32)
            {
                return;
            }

            // wow64cpu.dll reads CONTEXT64.R14 as the saved 32-bit ESP during exception
            // dispatch: BTCpuResetToConsistentState copies R14 → Rsp, and
            // Wow64PrepareForException uses R14 for stack relocation. In real WoW64,
            // wow64cpu.dll saves ESP→R14 on every 32→64 syscall transition. sogen
            // intercepts above that thunk and never sets R14, so we populate it here.
            ctx.R14 = ctx.Rsp;

            // Wow64PassExceptionToGuest rebuilds the 32-bit context from WOW64_CPURESERVED
            // (TEB64 TLS slot 1), not from the native exception ContextRecord below.
            win_emu.current_thread().wow64_cpu_reserved->access([&](WOW64_CPURESERVED& cpu) {
                cpu.Flags |= WOW64_CPURESERVED_FLAG_RESET_STATE;
                cpu.Context = make_wow64_context(ctx);
            });
        }

    }

    // Dispatch a 32-bit WoW64 exception directly to the 32-bit KiUserExceptionDispatcher32
    // in ntdll32.dll, bypassing the wow64 exception machinery entirely.
    //
    // The wow64 chain (64-bit KiUserExceptionDispatcher → Wow64PrepareForException →
    // BTCpuResetToConsistentState → wow64cpu stub) relies on a fully-initialized WoW64
    // thread environment (correct R13/R14/R15 pointing into wow64cpu.dll's per-thread
    // structures) that sogen does not provide. Going directly to the 32-bit dispatcher
    // avoids all of that.
    //
    // KiUserExceptionDispatcher32 entry (no return address on stack):
    //   [ESP+0] = CONTEXT*          (32-bit pointer to the WOW64_CONTEXT below)
    //   [ESP+4] = EXCEPTION_RECORD* (32-bit pointer to the 32-bit record below)
    void dispatch_exception_32bit(windows_emulator& win_emu, const CONTEXT64& ctx, const exception_record& record)
    {
        auto& emu = win_emu.emu();

        const WOW64_CONTEXT wow64_ctx = make_wow64_context(ctx);

        constexpr DWORD kMaxExceptionParameters = 15; // EXCEPTION_MAXIMUM_PARAMETERS

        struct EXCEPTION_RECORD32
        {
            DWORD ExceptionCode;
            DWORD ExceptionFlags;
            DWORD ExceptionRecord;
            DWORD ExceptionAddress;
            DWORD NumberParameters;
            DWORD ExceptionInformation[kMaxExceptionParameters];
        };

        EXCEPTION_RECORD32 record32{};
        record32.ExceptionCode = record.ExceptionCode;
        record32.ExceptionFlags = record.ExceptionFlags;
        record32.ExceptionRecord = static_cast<DWORD>(record.ExceptionRecord);
        record32.ExceptionAddress = static_cast<DWORD>(record.ExceptionAddress);
        record32.NumberParameters = record.NumberParameters;
        for (DWORD i = 0; i < record.NumberParameters && i < kMaxExceptionParameters; ++i)
        {
            record32.ExceptionInformation[i] = static_cast<DWORD>(record.ExceptionInformation[i]);
        }

        // Frame layout on the 32-bit stack (below current ESP):
        //   [new_esp+0]  : DWORD = ctx_ptr  (pointer to WOW64_CONTEXT)
        //   [new_esp+4]  : DWORD = rec_ptr  (pointer to EXCEPTION_RECORD32)
        //   [new_esp+8]  : WOW64_CONTEXT    (0x2CC bytes)
        //   [new_esp+8+sizeof(WOW64_CONTEXT)]: EXCEPTION_RECORD32
        constexpr auto ptr_pair_size = 2 * sizeof(DWORD);
        const auto frame_size = static_cast<uint32_t>(ptr_pair_size + sizeof(wow64_ctx) + sizeof(record32));
        const auto new_esp = align_down(static_cast<uint32_t>(ctx.Rsp) - frame_size, 4u);

        const DWORD ctx_ptr = new_esp + static_cast<DWORD>(ptr_pair_size);
        const DWORD rec_ptr = ctx_ptr + static_cast<DWORD>(sizeof(wow64_ctx));

        emu.write_memory(new_esp, &ctx_ptr, sizeof(ctx_ptr));
        emu.write_memory(new_esp + 4, &rec_ptr, sizeof(rec_ptr));
        emu.write_memory(ctx_ptr, &wow64_ctx, sizeof(wow64_ctx));
        emu.write_memory(rec_ptr, &record32, sizeof(record32));

        // CS is already 0x23 (we faulted in 32-bit mode); just redirect EIP and ESP.
        emu.reg(x86_register::rsp, static_cast<uint64_t>(new_esp));
        emu.reg(x86_register::rip, win_emu.process.ki_user_exception_dispatcher32);
    }

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

    void dispatch_exception(windows_emulator& win_emu, const DWORD status, const std::vector<EmulatorTraits<Emu64>::ULONG_PTR>& parameters)
    {
        CONTEXT64 ctx{};
        ctx.ContextFlags = CONTEXT64_ALL;
        cpu_context::save(win_emu.emu(), ctx);

        ctx.Rip = win_emu.uses_instruction_precision() //
                      ? win_emu.current_thread().current_ip
                      : win_emu.emu().read_instruction_pointer();

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

        // For 32-bit WoW64 exceptions, dispatch directly to the 32-bit
        // KiUserExceptionDispatcher32 rather than going through the wow64 exception
        // machinery (which requires a fully-initialized wow64cpu.dll thread state that
        // sogen does not provide).
        if (win_emu.process.ki_user_exception_dispatcher32)
        {
            const auto bitness = segment_utils::get_segment_bitness(win_emu.emu(), ctx.SegCs);
            if (bitness && *bitness == segment_utils::segment_bitness::bit32)
            {
                dispatch_exception_32bit(win_emu, ctx, record);
                return;
            }
        }

        sync_wow64_cpu_reserved_context(win_emu, ctx);

        EMU_EXCEPTION_POINTERS<EmulatorTraits<Emu64>> pointers{};
        pointers.ContextRecord = reinterpret_cast<EmulatorTraits<Emu64>::PVOID>(&ctx);
        pointers.ExceptionRecord = reinterpret_cast<EmulatorTraits<Emu64>::PVOID>(&record);
        dispatch_exception_pointers(win_emu.emu(), win_emu.process.ki_user_exception_dispatcher, pointers);
    }

    void dispatch_access_violation(windows_emulator& win_emu, const uint64_t address, const memory_operation operation)
    {
        dispatch_exception(win_emu, STATUS_ACCESS_VIOLATION,
                           {
                               map_violation_operation_to_parameter(operation),
                               address,
                           });
    }

    void dispatch_guard_page_violation(windows_emulator& win_emu, const uint64_t address, const memory_operation operation)
    {
        dispatch_exception(win_emu, STATUS_GUARD_PAGE_VIOLATION,
                           {
                               map_violation_operation_to_parameter(operation),
                               address,
                           });
    }

    void dispatch_illegal_instruction_violation(windows_emulator& win_emu)
    {
        dispatch_exception(win_emu, STATUS_ILLEGAL_INSTRUCTION, {});
    }

    void dispatch_integer_division_by_zero(windows_emulator& win_emu)
    {
        dispatch_exception(win_emu, STATUS_INTEGER_DIVIDE_BY_ZERO, {});
    }

    void dispatch_single_step(windows_emulator& win_emu)
    {
        dispatch_exception(win_emu, STATUS_SINGLE_STEP, {});
    }

    void dispatch_breakpoint(windows_emulator& win_emu)
    {
        dispatch_exception(win_emu, STATUS_BREAKPOINT, {});
    }

} // namespace sogen
