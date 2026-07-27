#pragma once

#ifdef __APPLE__

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace sogen::fex::hvf
{
    // Minimal fixed-encoding AArch64 assembler. Only the forms the guest-resident x87 fast paths
    // need are provided; every helper encodes exactly one instruction so the emitted stream can be
    // checked instruction-for-instruction against a disassembler.
    class arm64_emitter
    {
      public:
        using label = size_t;

        explicit arm64_emitter(uint32_t* code, const size_t capacity_words)
            : code_(code),
              capacity_(capacity_words)
        {
        }

        size_t offset() const
        {
            return this->count_ * 4;
        }

        label here() const
        {
            return this->count_;
        }

        label new_label()
        {
            this->labels_.push_back(SIZE_MAX);
            return this->labels_.size() - 1;
        }

        void bind(const label id)
        {
            this->labels_[id] = this->count_;
        }

        // b.<cond> <label>
        void b_cond(const uint32_t cond, const label id)
        {
            this->fixups_.push_back({this->count_, id, fixup_kind::cond_branch});
            this->emit(0x54000000u | (cond & 0xF));
        }

        // ldr <Xt>, <label>  (PC-relative literal)
        void ldr_literal(const uint32_t rt, const label id)
        {
            this->fixups_.push_back({this->count_, id, fixup_kind::ldr_literal});
            this->emit(0x58000000u | rt);
        }

        void dcd(const uint32_t word)
        {
            this->emit(word);
        }

        void br(const uint32_t rn)
        {
            this->emit(0xD61F0000u | (rn << 5));
        }

        void ret()
        {
            this->emit(0xD65F03C0u);
        }

        // umov <Xd>, <Vn>.D[index]
        void umov_d(const uint32_t rd, const uint32_t vn, const uint32_t index)
        {
            const uint32_t imm5 = 0x8u | (index << 4);
            this->emit(0x4E003C00u | (imm5 << 16) | (vn << 5) | rd);
        }

        // umov <Wd>, <Vn>.H[index]
        void umov_h(const uint32_t rd, const uint32_t vn, const uint32_t index)
        {
            const uint32_t imm5 = 0x2u | (index << 2);
            this->emit(0x0E003C00u | (imm5 << 16) | (vn << 5) | rd);
        }

        // fmov <Sd>, <Wn>
        void fmov_s_w(const uint32_t vd, const uint32_t rn)
        {
            this->emit(0x1E270000u | (rn << 5) | vd);
        }

        // fmov <Dd>, <Xn>
        void fmov_d_x(const uint32_t vd, const uint32_t rn)
        {
            this->emit(0x9E670000u | (rn << 5) | vd);
        }

        // ins <Vd>.D[index], <Xn>
        void ins_d(const uint32_t vd, const uint32_t index, const uint32_t rn)
        {
            const uint32_t imm5 = 0x8u | (index << 4);
            this->emit(0x4E001C00u | (imm5 << 16) | (rn << 5) | vd);
        }

        // movi <Vd>.2D, #0
        void movi_2d_zero(const uint32_t vd)
        {
            this->emit(0x6E005400u | vd);
        }

        // 32-bit AND (immediate) against a mask of `ones` low set bits. The N:immr:imms encoding of
        // such a mask is N=0, immr=0, imms=ones-1 (the 0xxxxx imms form selects a 32-bit element).
        void and_low_mask(const uint32_t rd, const uint32_t rn, const uint32_t ones)
        {
            if (ones == 0 || ones > 31)
            {
                throw std::runtime_error("arm64_emitter: unencodable low mask");
            }
            this->emit(0x12000000u | ((ones - 1) << 10) | (rn << 5) | rd);
        }

        void orr_shifted(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x2A000000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
        }

        void and_shifted(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x0A000000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
        }

        void bic_shifted(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x0A200000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
        }

        void eor_shifted(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x4A000000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
        }

        void add_imm(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t imm12, const bool shift12 = false)
        {
            this->emit(0x11000000u | (is64 ? 0x80000000u : 0u) | (shift12 ? (1u << 22) : 0u) | (imm12 << 10) | (rn << 5) | rd);
        }

        void sub_imm(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t imm12, const bool shift12 = false)
        {
            this->emit(0x51000000u | (is64 ? 0x80000000u : 0u) | (shift12 ? (1u << 22) : 0u) | (imm12 << 10) | (rn << 5) | rd);
        }

        void add_shifted(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x0B000000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
        }

        void sub_shifted(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x4B000000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
        }

        void cmp_imm(const bool is64, const uint32_t rn, const uint32_t imm12, const bool shift12 = false)
        {
            this->emit(0x7100001Fu | (is64 ? 0x80000000u : 0u) | (shift12 ? (1u << 22) : 0u) | (imm12 << 10) | (rn << 5));
        }

        void cmp_shifted(const bool is64, const uint32_t rn, const uint32_t rm, const uint32_t shift = 0)
        {
            this->emit(0x6B00001Fu | (is64 ? 0x80000000u : 0u) | (rm << 16) | (shift << 10) | (rn << 5));
        }

        void lsl_imm(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t shift)
        {
            const uint32_t width = is64 ? 64 : 32;
            this->ubfm(is64, rd, rn, (width - shift) % width, width - 1 - shift);
        }

        void lsr_imm(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t shift)
        {
            this->ubfm(is64, rd, rn, shift, is64 ? 63 : 31);
        }

        void ubfx(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t lsb, const uint32_t width)
        {
            this->ubfm(is64, rd, rn, lsb, lsb + width - 1);
        }

        void ubfm(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t immr, const uint32_t imms)
        {
            this->emit(0x53000000u | (is64 ? 0x80400000u : 0u) | (immr << 16) | (imms << 10) | (rn << 5) | rd);
        }

        void movz(const bool is64, const uint32_t rd, const uint32_t imm16, const uint32_t shift = 0)
        {
            this->emit(0x52800000u | (is64 ? 0x80000000u : 0u) | ((shift / 16) << 21) | (imm16 << 5) | rd);
        }

        void csel(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm, const uint32_t cond)
        {
            this->emit(0x1A800000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (cond << 12) | (rn << 5) | rd);
        }

        void cset(const bool is64, const uint32_t rd, const uint32_t cond)
        {
            this->emit(0x1A9F07E0u | (is64 ? 0x80000000u : 0u) | ((cond ^ 1u) << 12) | rd);
        }

        void mul(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm)
        {
            this->emit(0x1B007C00u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (rn << 5) | rd);
        }

        void umulh(const uint32_t rd, const uint32_t rn, const uint32_t rm)
        {
            this->emit(0x9BC07C00u | (rm << 16) | (rn << 5) | rd);
        }

        void clz(const bool is64, const uint32_t rd, const uint32_t rn)
        {
            this->emit(0x5AC01000u | (is64 ? 0x80000000u : 0u) | (rn << 5) | rd);
        }

        void lslv(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm)
        {
            this->emit(0x1AC02000u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (rn << 5) | rd);
        }

        void lsrv(const bool is64, const uint32_t rd, const uint32_t rn, const uint32_t rm)
        {
            this->emit(0x1AC02400u | (is64 ? 0x80000000u : 0u) | (rm << 16) | (rn << 5) | rd);
        }

        void resolve()
        {
            for (const auto& fixup : this->fixups_)
            {
                const size_t target = this->labels_.at(fixup.target);
                if (target == SIZE_MAX)
                {
                    throw std::runtime_error("arm64_emitter: unbound label");
                }

                const auto delta = static_cast<int64_t>(target) - static_cast<int64_t>(fixup.at);
                if (delta < -(1 << 18) || delta >= (1 << 18))
                {
                    throw std::runtime_error("arm64_emitter: branch out of range");
                }

                this->code_[fixup.at] |= (static_cast<uint32_t>(delta) & 0x7FFFFu) << 5;
            }
            this->fixups_.clear();
        }

      private:
        enum class fixup_kind
        {
            cond_branch,
            ldr_literal,
        };

        struct fixup
        {
            size_t at;
            label target;
            fixup_kind kind;
        };

        void emit(const uint32_t insn)
        {
            if (this->count_ >= this->capacity_)
            {
                throw std::runtime_error("arm64_emitter: code buffer overflow");
            }
            this->code_[this->count_++] = insn;
        }

        uint32_t* code_;
        size_t capacity_;
        size_t count_ = 0;
        std::vector<size_t> labels_;
        std::vector<fixup> fixups_;
    };
}

#endif
