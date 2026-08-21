#pragma once

#include "windows_emulator.hpp"

namespace sogen
{

    // Narrow, single-purpose mechanism for transparently inserting a real, argument-less,
    // `ret`-terminated guest function call into a thread's natural instruction stream from
    // host-side code - e.g. running a DLL's own real exported routine once, at a point host code
    // has determined it is now safe to call.
    //
    // Unlike user_callback_dispatch.hpp's mechanism (built for KiUserCallbackDispatcher /
    // NtCallbackReturn round trips, which require the guest's own cooperation via a syscall to
    // signal completion), this detects completion by giving the call a reserved sentinel return
    // address and watching ordinary instruction-precision execution reach it - appropriate for a
    // plain exported function that just runs and `ret`s, with no callback protocol of its own.
    // Only works while windows_emulator::uses_instruction_precision() is true (see
    // try_complete_guest_function, wired into windows_emulator::on_instruction_execution, which is
    // itself only ever invoked in that mode) - callers must check that themselves before calling
    // this.
    //
    // Must be called from within an instruction-execution hook, right before the thread's current
    // instruction would otherwise run (i.e. with the CPU's registers exactly as the interrupted
    // instruction would have seen them) - once the call completes, that original instruction runs
    // normally, as if it had never been interrupted.
    void invoke_guest_function(emulator_thread& thread, x86_64_cpu& emu, uint64_t sentinel_return_address, uint64_t function_address);

    // Completes a pending invoke_guest_function call if `address` is the sentinel armed on this
    // vCPU's active thread. Returns true if it did (the caller should not treat `address` as
    // having actually executed - the interrupted instruction will run normally on the next step).
    bool try_complete_guest_function(vcpu_context& vcpu, uint64_t address);

} // namespace sogen
