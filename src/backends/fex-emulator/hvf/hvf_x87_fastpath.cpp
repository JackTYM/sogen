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
        constexpr uint32_t cond_hs = 2;
        constexpr uint32_t cond_mi = 4;
        constexpr uint32_t cond_pl = 5;
        constexpr uint32_t cond_hi = 8;
        constexpr uint32_t cond_al = 14;

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
        constexpr uint32_t reg_sig = 2;     // later: rounded significand
        constexpr uint32_t reg_signexp = 3; // later: round bits
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

        // softfloat_roundPackToExtF80 with x6 = expZ, x7 = signZ, x16 = sig, x17 = sigExtra and x0
        // still holding FCW. Only the plain rounding path is emitted: an exponent outside
        // [1, 0x7FFD] is exactly softfloat's `0x7FFD <= (uint32_t)(exp - 1)` test, so the underflow
        // and overflow arms - the only ones that raise an x87 exception flag - defer instead. All
        // three x87 precision-control settings are implemented; the reserved PC encoding defers.
        //
        // Both deferrals are emitted before FCW is consumed, because the stub they branch to is a
        // tail call that reads the original arguments back out of x0 and x1.
        void emit_round_pack_ext_f80(arm64_emitter& em, const arm64_emitter::label slow)
        {
            const auto prec80 = em.new_label();
            const auto no_carry = em.new_label();
            const auto no_wrap = em.new_label();
            const auto pack = em.new_label();

            em.sub_imm(false, 2, 6, 1);
            em.movz(false, 4, 0x7FFC);
            em.cmp_shifted(false, 2, 4);
            em.b_cond(cond_hi, slow);

            em.ubfx(false, 3, 0, 8, 2); // PC
            em.cmp_imm(false, 3, 1);
            em.b_cond(cond_eq, slow); // reserved precision-control encoding

            em.ubfx(false, 8, 0, 10, 2); // RC
            em.movz(false, 2, 2);
            em.sub_shifted(false, 2, 2, 7);
            em.cmp_shifted(false, 8, 2);
            em.cset(false, 2, cond_eq); // rounds away from zero for this sign
            em.cmp_imm(false, 8, 0);
            em.cset(false, 4, cond_eq); // round to nearest even
            em.cmp_imm(false, 3, 3);
            em.b_cond(cond_eq, prec80);

            // 32- and 64-bit precision share softfloat's roundMask formulation.
            em.movz(false, 5, 11);
            em.movz(false, 0, 40);
            em.cmp_imm(false, 3, 0);
            em.csel(false, 3, 0, 5, cond_eq);
            em.movz(true, 5, 1);
            em.lslv(true, 5, 5, 3);
            em.sub_imm(true, 5, 5, 1); // roundMask
            em.add_imm(true, 0, 5, 1);
            em.lsr_imm(true, 0, 0, 1); // round-to-nearest increment

            em.cmp_imm(true, 17, 0);
            em.cset(true, 3, cond_ne);
            em.orr_shifted(true, 16, 16, 3); // sig |= (sigExtra != 0)

            em.cmp_imm(false, 2, 0);
            em.csel(true, 3, 5, wzr, cond_ne);
            em.cmp_imm(false, 4, 0);
            em.csel(true, 3, 0, 3, cond_ne); // roundIncrement

            em.and_shifted(true, 0, 16, 5); // roundBits
            em.add_shifted(true, 16, 16, 3);
            em.cmp_shifted(true, 16, 3);
            em.b_cond(cond_hs, no_carry);
            em.add_imm(false, 6, 6, 1);
            em.movz(true, 16, 1);
            em.lsl_imm(true, 16, 16, 63);
            em.bind(no_carry);

            em.add_imm(true, 3, 5, 1);
            em.lsl_imm(true, 1, 0, 1);
            em.cmp_shifted(true, 1, 3);
            em.cset(false, 1, cond_eq);
            em.and_shifted(false, 1, 1, 4);
            em.orr_shifted(true, 8, 5, 3);
            em.cmp_imm(false, 1, 0);
            em.csel(true, 5, 8, 5, cond_ne);
            em.bic_shifted(true, 16, 16, 5);
            em.b_cond(cond_al, pack);

            em.bind(prec80);
            em.lsr_imm(true, 3, 17, 63);
            em.cmp_imm(true, 17, 0);
            em.cset(false, 0, cond_ne);
            em.and_shifted(false, 0, 0, 2);
            em.cmp_imm(false, 4, 0);
            em.csel(false, 3, 3, 0, cond_ne); // doIncrement
            em.cmp_imm(false, 3, 0);
            em.b_cond(cond_eq, pack);
            em.add_imm(true, 16, 16, 1);
            em.cmp_imm(true, 16, 0);
            em.b_cond(cond_ne, no_wrap);
            em.add_imm(false, 6, 6, 1);
            em.movz(true, 16, 1);
            em.lsl_imm(true, 16, 16, 63);
            em.b_cond(cond_al, pack);
            em.bind(no_wrap);
            em.lsl_imm(true, 0, 17, 1);
            em.cmp_imm(true, 0, 0);
            em.cset(false, 0, cond_eq);
            em.and_shifted(false, 0, 0, 4);
            em.bic_shifted(true, 16, 16, 0);

            em.bind(pack);
            em.lsl_imm(false, 0, 7, 15);
            em.orr_shifted(false, 0, 0, 6);
            em.fmov_d_x(0, 16);
            em.ins_d(0, 1, 0);
            em.ret();
        }

        // extF80_mul. Handles two normalized finite operands (explicit integer bit set, exponent in
        // [1, 0x7FFE]) whose product exponent stays in [1, 0x7FFD], which is exactly the set of
        // inputs for which softfloat_roundPackToExtF80 takes its plain rounding path - no NaN or
        // infinity propagation, no subnormal normalization, no underflow or overflow.
        void emit_f80_mul(arm64_emitter& em, const arm64_emitter::label slow)
        {
            const auto no_shift = em.new_label();

            em.umov_d(2, 0, 0); // sigA
            em.umov_h(3, 0, 4); // signExpA
            em.umov_d(4, 1, 0); // sigB
            em.umov_h(5, 1, 4); // signExpB

            em.and_low_mask(6, 3, 15); // expA
            em.and_low_mask(7, 5, 15); // expB
            em.movz(false, 17, 0x7FFD);
            em.sub_imm(false, 16, 6, 1);
            em.cmp_shifted(false, 16, 17);
            em.b_cond(cond_hi, slow);
            em.sub_imm(false, 16, 7, 1);
            em.cmp_shifted(false, 16, 17);
            em.b_cond(cond_hi, slow);

            em.and_shifted(true, 16, 2, 4);
            em.cmp_imm(true, 16, 0);
            em.b_cond(cond_pl, slow); // an operand is unnormal, subnormal or zero

            em.add_shifted(false, 6, 6, 7);
            em.sub_imm(false, 6, 6, 3, true);
            em.sub_imm(false, 6, 6, 0xFFE); // expZ = expA + expB - 0x3FFE

            em.eor_shifted(false, 7, 3, 5);
            em.lsr_imm(false, 7, 7, 15);
            em.and_low_mask(7, 7, 1); // signZ

            em.umulh(16, 2, 4);
            em.mul(true, 17, 2, 4);

            em.cmp_imm(true, 16, 0);
            em.b_cond(cond_mi, no_shift);
            em.sub_imm(false, 6, 6, 1);
            em.lsr_imm(true, 2, 17, 63);
            em.lsl_imm(true, 16, 16, 1);
            em.orr_shifted(true, 16, 16, 2);
            em.lsl_imm(true, 17, 17, 1);
            em.bind(no_shift);

            emit_round_pack_ext_f80(em, slow);
        }

        // extF80_add, and extF80_sub via `negate_b` - softfloat implements the two as one routine
        // selected by whether the operand signs agree, so flipping B's sign bit turns either into
        // the other. Both operands must be normalized finite (explicit integer bit set, exponent in
        // [1, 0x7FFE]), which removes softfloat's NaN, infinity, zero and subnormal arms from both
        // addMagsExtF80 and subMagsExtF80 - including the only one that raises a flag, the
        // infinity-minus-infinity invalid case.
        //
        // The exponent difference is additionally capped at 63 so the operand alignment collapses
        // to the `dist < 64` arm of softfloat_shiftRightJam64Extra and softfloat_shiftRightJam128,
        // which agree there and differ above it.
        //
        // This is the one op whose live set does not fit in the ABI's scratch registers, so the
        // frame pointer is parked in v2 and restored on the deferral path, which tail-calls a stub
        // that expects the original arguments still in x0 and x1.
        void emit_f80_addsub(arm64_emitter& em, const arm64_emitter::label slow, const bool negate_b)
        {
            constexpr uint32_t sign_z = 1;
            constexpr uint32_t sig_a = 2;
            constexpr uint32_t signexp_a = 3;
            constexpr uint32_t sig_b = 4;
            constexpr uint32_t signexp_b = 5;
            constexpr uint32_t exp_z = 6;
            constexpr uint32_t exp_b = 7;
            constexpr uint32_t exp_diff = 8;
            constexpr uint32_t sig_z = 16;
            constexpr uint32_t sig_extra = 17;
            constexpr uint32_t tmp = signexp_a;
            constexpr uint32_t tmp2 = signexp_b;

            const auto sub_mags = em.new_label();
            const auto add_b_bigger = em.new_label();
            const auto add_same_exp = em.new_label();
            const auto add_aligned = em.new_label();
            const auto add_shift_right1 = em.new_label();
            const auto sub_b_bigger = em.new_label();
            const auto sub_same_exp = em.new_label();
            const auto sub_same_exp_a = em.new_label();
            const auto sub_same_exp_b = em.new_label();
            const auto sub_norm = em.new_label();
            const auto sub_norm_shift = em.new_label();
            const auto round = em.new_label();
            const auto defer = em.new_label();

            em.fmov_d_x(2, 1);

            em.umov_d(sig_a, 0, 0);
            em.umov_h(signexp_a, 0, 4);
            em.umov_d(sig_b, 1, 0);
            em.umov_h(signexp_b, 1, 4);

            if (negate_b)
            {
                em.movz(false, exp_diff, 0x8000);
                em.eor_shifted(false, signexp_b, signexp_b, exp_diff);
            }

            em.and_low_mask(exp_z, signexp_a, 15);
            em.and_low_mask(exp_b, signexp_b, 15);
            em.movz(false, sig_extra, 0x7FFD);
            em.sub_imm(false, sig_z, exp_z, 1);
            em.cmp_shifted(false, sig_z, sig_extra);
            em.b_cond(cond_hi, defer);
            em.sub_imm(false, sig_z, exp_b, 1);
            em.cmp_shifted(false, sig_z, sig_extra);
            em.b_cond(cond_hi, defer);

            em.and_shifted(true, sig_z, sig_a, sig_b);
            em.cmp_imm(true, sig_z, 0);
            em.b_cond(cond_pl, defer); // an operand is unnormal, subnormal or zero

            em.sub_shifted(false, exp_diff, exp_z, exp_b);
            em.sub_shifted(false, sig_z, wzr, exp_diff);
            em.cmp_imm(false, exp_diff, 0);
            em.csel(false, sig_z, sig_z, exp_diff, cond_mi);
            em.cmp_imm(false, sig_z, 63);
            em.b_cond(cond_hi, defer);

            em.eor_shifted(false, sig_z, signexp_a, signexp_b);
            em.lsr_imm(false, sig_z, sig_z, 15);
            em.lsr_imm(false, sign_z, signexp_a, 15);
            em.cmp_imm(false, sig_z, 0);
            em.b_cond(cond_ne, sub_mags);

            em.cmp_imm(false, exp_diff, 0);
            em.b_cond(cond_eq, add_same_exp);
            em.b_cond(cond_mi, add_b_bigger);

            em.lsrv(true, tmp, sig_b, exp_diff);
            em.sub_shifted(false, tmp2, wzr, exp_diff);
            em.lslv(true, sig_extra, sig_b, tmp2);
            em.add_shifted(true, sig_z, sig_a, tmp);
            em.b_cond(cond_al, add_aligned);

            em.bind(add_b_bigger);
            em.orr_shifted(false, exp_z, wzr, exp_b);
            em.sub_shifted(false, exp_diff, wzr, exp_diff);
            em.lsrv(true, tmp, sig_a, exp_diff);
            em.sub_shifted(false, tmp2, wzr, exp_diff);
            em.lslv(true, sig_extra, sig_a, tmp2);
            em.add_shifted(true, sig_z, tmp, sig_b);
            em.b_cond(cond_al, add_aligned);

            em.bind(add_same_exp);
            em.add_shifted(true, sig_z, sig_a, sig_b);
            em.movz(true, sig_extra, 0);
            em.b_cond(cond_al, add_shift_right1);

            em.bind(add_aligned);
            em.cmp_imm(true, sig_z, 0);
            em.b_cond(cond_mi, round);

            em.bind(add_shift_right1);
            em.cmp_imm(true, sig_extra, 0);
            em.cset(false, tmp, cond_ne);
            em.lsl_imm(true, tmp2, sig_z, 63);
            em.orr_shifted(true, sig_extra, tmp2, tmp);
            em.lsr_imm(true, sig_z, sig_z, 1);
            em.movz(true, tmp, 1);
            em.lsl_imm(true, tmp, tmp, 63);
            em.orr_shifted(true, sig_z, sig_z, tmp);
            em.add_imm(false, exp_z, exp_z, 1);
            em.b_cond(cond_al, round);

            em.bind(sub_mags);
            em.cmp_imm(false, exp_diff, 0);
            em.b_cond(cond_eq, sub_same_exp);
            em.b_cond(cond_mi, sub_b_bigger);

            em.lsrv(true, tmp, sig_b, exp_diff);
            em.sub_shifted(false, tmp2, wzr, exp_diff);
            em.lslv(true, tmp2, sig_b, tmp2);
            em.sub_shifted(true, sig_extra, wzr, tmp2);
            em.cmp_imm(true, tmp2, 0);
            em.cset(true, exp_diff, cond_ne);
            em.sub_shifted(true, sig_z, sig_a, tmp);
            em.sub_shifted(true, sig_z, sig_z, exp_diff);
            em.b_cond(cond_al, sub_norm);

            em.bind(sub_b_bigger);
            em.orr_shifted(false, exp_z, wzr, exp_b);
            em.sub_shifted(false, exp_diff, wzr, exp_diff);
            em.lsrv(true, tmp, sig_a, exp_diff);
            em.sub_shifted(false, tmp2, wzr, exp_diff);
            em.lslv(true, tmp2, sig_a, tmp2);
            em.movz(false, exp_b, 1);
            em.eor_shifted(false, sign_z, sign_z, exp_b);
            em.sub_shifted(true, sig_extra, wzr, tmp2);
            em.cmp_imm(true, tmp2, 0);
            em.cset(true, exp_diff, cond_ne);
            em.sub_shifted(true, sig_z, sig_b, tmp);
            em.sub_shifted(true, sig_z, sig_z, exp_diff);
            em.b_cond(cond_al, sub_norm);

            em.bind(sub_same_exp);
            em.cmp_shifted(true, sig_a, sig_b);
            em.b_cond(cond_hi, sub_same_exp_a);
            em.b_cond(cond_ne, sub_same_exp_b);

            // Exact cancellation. softfloat answers -0 under round-toward-negative and +0 for every
            // other x87 rounding mode, and records no flag.
            em.ubfx(false, sig_z, 0, 10, 2);
            em.cmp_imm(false, sig_z, 1);
            em.cset(false, sig_z, cond_eq);
            em.lsl_imm(false, sig_z, sig_z, 15);
            em.movi_2d_zero(0);
            em.ins_d(0, 1, sig_z);
            em.ret();

            em.bind(sub_same_exp_a);
            em.sub_shifted(true, sig_z, sig_a, sig_b);
            em.movz(true, sig_extra, 0);
            em.b_cond(cond_al, sub_norm);

            em.bind(sub_same_exp_b);
            em.movz(false, exp_b, 1);
            em.eor_shifted(false, sign_z, sign_z, exp_b);
            em.sub_shifted(true, sig_z, sig_b, sig_a);
            em.movz(true, sig_extra, 0);

            // softfloat_normRoundPackToExtF80. The 128-bit difference is never zero here - exact
            // cancellation only happens with equal exponents and equal significands, which returned
            // above - so the leading-zero count is always well defined.
            em.bind(sub_norm);
            em.cmp_imm(true, sig_z, 0);
            em.b_cond(cond_ne, sub_norm_shift);
            em.sub_imm(false, exp_z, exp_z, 64);
            em.orr_shifted(true, sig_z, wzr, sig_extra);
            em.movz(true, sig_extra, 0);

            em.bind(sub_norm_shift);
            em.clz(true, tmp, sig_z);
            em.sub_shifted(false, exp_z, exp_z, tmp);
            em.cmp_imm(false, tmp, 0);
            em.b_cond(cond_eq, round);
            em.sub_shifted(false, tmp2, wzr, tmp);
            em.lsrv(true, tmp2, sig_extra, tmp2);
            em.lslv(true, sig_z, sig_z, tmp);
            em.orr_shifted(true, sig_z, sig_z, tmp2);
            em.lslv(true, sig_extra, sig_extra, tmp);

            em.bind(round);
            em.orr_shifted(false, exp_b, wzr, sign_z);
            emit_round_pack_ext_f80(em, defer);

            em.bind(defer);
            em.umov_d(1, 2, 0);
            em.b_cond(cond_al, slow);
        }

        void emit_f80_add(arm64_emitter& em, const arm64_emitter::label slow)
        {
            emit_f80_addsub(em, slow, false);
        }

        void emit_f80_sub(arm64_emitter& em, const arm64_emitter::label slow)
        {
            emit_f80_addsub(em, slow, true);
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
            case hvf_x87_fastpath::op::f80_mul:
                return emit_f80_mul;
            case hvf_x87_fastpath::op::f80_add:
                return emit_f80_add;
            case hvf_x87_fastpath::op::f80_sub:
                return emit_f80_sub;
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
