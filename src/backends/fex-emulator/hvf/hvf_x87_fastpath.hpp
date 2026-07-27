#pragma once

#ifdef __APPLE__

#include <array>
#include <cstddef>
#include <cstdint>

namespace sogen::fex::hvf
{
    // Guest-resident ARM64 implementations of the hottest x87 80-bit fallbacks.
    //
    // Under HVF every FallbackHandlerPointers[].Func is rewritten to a hypercall stub, so each x87
    // fallback the JIT emits costs a full VM exit. The ops below are re-implemented as native code
    // living in the guest so the dispatcher's ABI handler calls straight into them with no exit.
    //
    // Each entry only handles the operand shapes where the emitted arithmetic is provably identical
    // to SoftFloat-3e's, and branches to the op's original hypercall stub for everything else
    // (NaN/infinity operands, overflow, underflow, denormal results, and any case that would raise
    // an x87 invalid-operation flag). Deferring is always bit-exact by construction, so the fast
    // path can never disagree with the softfloat implementation - it can only decline to run.
    //
    // The page is allocated as plain anonymous memory owned by this backend and is never host
    // executable; hvf_vm maps it PROT_READ | PROT_EXEC into the guest only.
    class hvf_x87_fastpath
    {
      public:
        enum class op : size_t
        {
            f80_cvt_f32,
            f80_cvt_f64,
            f80_cmp,
            f80_cvtint_i64,
            f80_mul,
            f80_add,
            f80_sub,
            count,
        };

        static constexpr size_t page_bytes = 0x4000;
        static constexpr size_t op_count = static_cast<size_t>(op::count);

        void build();

        uint8_t* page() const
        {
            return this->page_;
        }

        // Points `which`'s deferral path at `stub_va` and returns the guest entry VA to install in
        // FallbackHandlerPointers[].Func. Idempotent for a given (op, stub_va) pair.
        uint64_t bind(op which, uint64_t stub_va);

        bool contains_entry(uint64_t va) const;

      private:
        uint8_t* page_ = nullptr;
        std::array<size_t, op_count> entry_offset_{};
        std::array<size_t, op_count> slow_slot_offset_{};
        size_t code_bytes_ = 0;
    };
}

#endif
