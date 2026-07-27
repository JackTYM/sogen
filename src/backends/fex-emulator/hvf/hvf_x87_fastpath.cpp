#ifdef __APPLE__

#include "hvf_x87_fastpath.hpp"

#include "hvf_arm64_emitter.hpp"

#include <cstring>
#include <stdexcept>

#include <sys/mman.h>
#include <libkern/OSCacheControl.h>

namespace sogen::fex::hvf
{
    namespace
    {
        constexpr uint32_t cond_eq = 0;
        constexpr uint32_t cond_ne = 1;
        constexpr uint32_t cond_hi = 8;

        constexpr uint32_t wzr = 31;

        // FEX calls these through Arm64Emitter::SpillForABICall, which on a host with the clang
        // `preserve_all` ABI spills only X0-X8, X16-X18, X30 and v0-v7 and requires the callee to
        // preserve X9-X15, X19-X29 and the low 128 bits of v8-v31. X18 is the reserved platform
        // register on Apple targets, so the usable scratch set is X0-X8, X16 and X17 plus v0-v7.
        // NZCV is spilled by the caller and may be clobbered.
        //
        // x0 arrives holding FCW and x1 the CpuStateFrame; both become scratch once FCW's rounding
        // control has been extracted. The fast paths never touch the frame - every case that would
        // raise an x87 exception flag defers instead.
        constexpr uint32_t reg_scratch = 1;
        constexpr uint32_t reg_sig = 2;      // later: rounded significand
        constexpr uint32_t reg_signexp = 3;  // later: round bits
        constexpr uint32_t reg_sign = 4;
        constexpr uint32_t reg_exp = 5;
        constexpr uint32_t reg_tmp = 6;
        constexpr uint32_t reg_val = 7;
        constexpr uint32_t reg_rc = 8;
        constexpr uint32_t reg_pack = 16; // earlier: round increment
        constexpr uint32_t reg_tmp2 = 17;

        // roundIncrement per SoftFloat's roundPack helpers: `near` under round-to-nearest-even and
        // `away` when the x87 rounding-control field selects the direction that rounds this sign
        // away from zero, 0 otherwise. FCW.RC is bits 11:10 and maps 0 -> nearest even, 1 -> -inf,
        // 2 -> +inf, 3 -> truncate, so the away case is exactly RC == 2 - sign.
        void emit_round_increment(arm64_emitter& em, const bool is64, const uint32_t near, const uint32_t away)
        {
            em.ubfx(false, reg_rc, 0, 10, 2);
            em.movz(false, reg_tmp2, 2);
            em.sub_shifted(false, reg_tmp2, reg_tmp2, reg_sign);
            em.cmp_shifted(false, reg_rc, reg_tmp2);
            em.movz(false, reg_scratch, away);
            em.csel(is64, reg_pack, reg_scratch, wzr, cond_eq);
            em.cmp_imm(false, reg_rc, 0);
            em.movz(false, reg_scratch, near);
            em.csel(is64, reg_pack, reg_scratch, reg_pack, cond_eq);
        }

        // The tail shared by roundPackToF32/F64 once the overflow and underflow branches are known
        // not to be taken: add the increment, drop the round bits, clear the ULP on an exact
        // halfway tie under round-to-nearest-even, and force a zero significand to a zero exponent.
        void emit_round_tail(arm64_emitter& em, const bool is64, const uint32_t round_bit_count, const uint32_t half)
        {
            em.and_low_mask(reg_signexp, reg_val, round_bit_count);
            em.add_shifted(is64, reg_sig, reg_val, reg_pack);
            em.lsr_imm(is64, reg_sig, reg_sig, round_bit_count);
            em.cmp_imm(false, reg_signexp, half);
            em.cset(false, reg_tmp2, cond_eq);
            em.cmp_imm(false, reg_rc, 0);
            em.cset(false, reg_scratch, cond_eq);
            em.and_shifted(false, reg_tmp2, reg_tmp2, reg_scratch);
            em.bic_shifted(is64, reg_sig, reg_sig, reg_tmp2);
            em.cmp_imm(is64, reg_sig, 0);
            em.csel(false, reg_exp, wzr, reg_exp, cond_eq);
        }

        // extF80_to_f32. Handles finite operands whose result lands strictly inside the normal
        // float range; NaN, infinity, zero, subnormal and overflowing results defer. The exponent
        // window check subsumes softfloat's zero test, since a zero operand has exp 0 and biases to
        // a negative value.
        void emit_f80_cvt_f32(arm64_emitter& em, const arm64_emitter::label slow)
        {
            em.umov_d(reg_sig, 0, 0);
            em.umov_h(reg_signexp, 0, 4);
            em.and_low_mask(reg_exp, reg_signexp, 15);
            em.movz(false, reg_tmp2, 0x7FFF);
            em.cmp_shifted(false, reg_exp, reg_tmp2);
            em.b_cond(cond_eq, slow);

            em.lsr_imm(false, reg_sign, reg_signexp, 15);

            // sig32 = softfloat_shortShiftRightJam64(sig, 33)
            em.lsl_imm(true, reg_tmp, reg_sig, 31);
            em.cmp_imm(true, reg_tmp, 0);
            em.cset(false, reg_tmp, cond_ne);
            em.lsr_imm(true, reg_val, reg_sig, 33);
            em.orr_shifted(false, reg_val, reg_val, reg_tmp);

            em.sub_imm(false, reg_exp, reg_exp, 3, true);
            em.sub_imm(false, reg_exp, reg_exp, 0xF81);
            em.cmp_imm(false, reg_exp, 0xFC);
            em.b_cond(cond_hi, slow);

            emit_round_increment(em, false, 0x40, 0x7F);
            emit_round_tail(em, false, 7, 0x40);

            em.lsl_imm(false, reg_pack, reg_sign, 31);
            em.add_shifted(false, reg_pack, reg_pack, reg_exp, 23);
            em.add_shifted(false, reg_pack, reg_pack, reg_sig);
            em.fmov_s_w(0, reg_pack);
            em.ret();
        }

        // extF80_to_f64, structurally identical with 64-bit arithmetic.
        void emit_f80_cvt_f64(arm64_emitter& em, const arm64_emitter::label slow)
        {
            em.umov_d(reg_sig, 0, 0);
            em.umov_h(reg_signexp, 0, 4);
            em.and_low_mask(reg_exp, reg_signexp, 15);
            em.movz(false, reg_tmp2, 0x7FFF);
            em.cmp_shifted(false, reg_exp, reg_tmp2);
            em.b_cond(cond_eq, slow);

            em.lsr_imm(false, reg_sign, reg_signexp, 15);

            // sig = softfloat_shortShiftRightJam64(sig, 1)
            em.lsr_imm(true, reg_val, reg_sig, 1);
            em.and_low_mask(reg_tmp, reg_sig, 1);
            em.orr_shifted(true, reg_val, reg_val, reg_tmp);

            em.sub_imm(false, reg_exp, reg_exp, 3, true);
            em.sub_imm(false, reg_exp, reg_exp, 0xC01);
            em.cmp_imm(false, reg_exp, 0x7FC);
            em.b_cond(cond_hi, slow);

            emit_round_increment(em, true, 0x200, 0x3FF);
            emit_round_tail(em, true, 10, 0x200);

            em.lsl_imm(true, reg_pack, reg_sign, 63);
            em.add_shifted(true, reg_pack, reg_pack, reg_exp, 52);
            em.add_shifted(true, reg_pack, reg_pack, reg_sig);
            em.fmov_d_x(0, reg_pack);
            em.ret();
        }

        using op_emitter = void (*)(arm64_emitter&, arm64_emitter::label);

        op_emitter emitter_for(const hvf_x87_fastpath::op which)
        {
            switch (which)
            {
            case hvf_x87_fastpath::op::f80_cvt_f32:
                return emit_f80_cvt_f32;
            case hvf_x87_fastpath::op::f80_cvt_f64:
                return emit_f80_cvt_f64;
            default:
                return nullptr;
            }
        }
    }

    void hvf_x87_fastpath::build()
    {
        this->page_ = static_cast<uint8_t*>(::mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (this->page_ == MAP_FAILED)
        {
            this->page_ = nullptr;
            throw std::runtime_error("HVF: failed to allocate the x87 fast-path page");
        }

        std::memset(this->page_, 0, page_bytes);

        arm64_emitter em(reinterpret_cast<uint32_t*>(this->page_), page_bytes / 4);

        std::array<arm64_emitter::label, op_count> slow_labels{};
        for (size_t i = 0; i < op_count; ++i)
        {
            slow_labels[i] = em.new_label();
            em.bind(slow_labels[i]);
            this->slow_slot_offset_[i] = em.offset();
            em.dcd(0);
            em.dcd(0);
        }

        for (size_t i = 0; i < op_count; ++i)
        {
            const auto emitter = emitter_for(static_cast<op>(i));
            if (emitter == nullptr)
            {
                this->entry_offset_[i] = SIZE_MAX;
                continue;
            }

            this->entry_offset_[i] = em.offset();

            const auto slow = em.new_label();
            emitter(em, slow);

            em.bind(slow);
            em.ldr_literal(reg_pack, slow_labels[i]);
            em.br(reg_pack);
        }

        em.resolve();
        this->code_bytes_ = em.offset();

        ::sys_icache_invalidate(this->page_, page_bytes);
    }

    uint64_t hvf_x87_fastpath::bind(const op which, const uint64_t stub_va)
    {
        const auto index = static_cast<size_t>(which);
        if (this->page_ == nullptr || this->entry_offset_[index] == SIZE_MAX)
        {
            return stub_va;
        }

        auto* const slot = reinterpret_cast<uint64_t*>(this->page_ + this->slow_slot_offset_[index]);
        if (*slot != 0 && *slot != stub_va)
        {
            throw std::runtime_error("HVF: x87 fast-path deferral target changed");
        }
        *slot = stub_va;

        return reinterpret_cast<uint64_t>(this->page_) + this->entry_offset_[index];
    }

    bool hvf_x87_fastpath::contains_entry(const uint64_t va) const
    {
        if (this->page_ == nullptr)
        {
            return false;
        }

        const auto base = reinterpret_cast<uint64_t>(this->page_);
        return va >= base && va < base + page_bytes;
    }
}

#endif
