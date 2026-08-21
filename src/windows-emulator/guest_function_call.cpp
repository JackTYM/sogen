#include "std_include.hpp"
#include "guest_function_call.hpp"
#include "windows_emulator.hpp"

namespace sogen
{
    void invoke_guest_function(emulator_thread& thread, x86_64_cpu& emu, const uint64_t sentinel_return_address,
                               const uint64_t function_address)
    {
        if (thread.pending_guest_call.has_value())
        {
            throw std::runtime_error("A guest function call is already pending on this thread");
        }

        if (sentinel_return_address == 0 || function_address == 0)
        {
            throw std::runtime_error("Cannot invoke guest function: sentinel or function address unresolved");
        }

        pending_guest_function_call pending{};
        pending.sentinel_address = sentinel_return_address;
        pending.saved_state.save_registers(emu);

        thread.pending_guest_call = std::move(pending);

        const auto call_rsp = align_down(emu.read_stack_pointer(), 0x10) - 0x8;
        emu.write_memory(call_rsp, &sentinel_return_address, sizeof(sentinel_return_address));
        emu.reg(x86_register::rsp, call_rsp);
        emu.reg(x86_register::rip, function_address);
    }

    bool try_complete_guest_function(vcpu_context& vcpu, const uint64_t address)
    {
        auto& thread = vcpu.thread();
        if (!thread.pending_guest_call.has_value() || thread.pending_guest_call->sentinel_address != address)
        {
            return false;
        }

        const auto pending = std::move(*thread.pending_guest_call);
        thread.pending_guest_call.reset();

        pending.saved_state.restore_registers(vcpu.cpu);

        return true;
    }
} // namespace sogen
