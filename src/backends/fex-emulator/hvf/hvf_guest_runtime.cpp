#include "hvf_guest_runtime.hpp"

#include <cstddef>
#include <stdexcept>

#include <libkern/OSCacheControl.h>
#include <sys/mman.h>

namespace sogen::fex::hvf
{
    namespace
    {
        constexpr uint32_t insn_ret = 0xD65F03C0;
        constexpr uint32_t insn_branch_to_self = 0x14000000;
        constexpr uint32_t insn_tlbi_vmalle1is = 0xD508831F;
        constexpr uint32_t insn_dsb_ish = 0xD5033B9F;
        constexpr uint32_t insn_isb = 0xD5033FDF;

        constexpr size_t exit_stub_offset = vector_count * guest_runtime::vector_stride;
        constexpr size_t tlb_flush_stub_offset = exit_stub_offset + 0x40;
        constexpr size_t host_call_stubs_offset = tlb_flush_stub_offset + 0x40;
        constexpr size_t host_call_stub_size = 8;

        static_assert(host_call_stubs_offset + max_host_calls * host_call_stub_size <= guest_runtime::size);
        static_assert(offsetof(host_call_registers, q) == 80);
    }

    // NOLINTNEXTLINE(hicpp-no-assembler): the trampoline has to load and save exact argument registers
    __asm__(".text\n"
            ".p2align 2\n"
            ".globl _sogen_fex_hvf_call_host\n"
            ".private_extern _sogen_fex_hvf_call_host\n"
            "_sogen_fex_hvf_call_host:\n"
            "  stp x19, x30, [sp, #-16]!\n"
            "  mov x19, x0\n"
            "  mov x16, x1\n"
            "  ldp q0, q1, [x19, #80]\n"
            "  ldp q2, q3, [x19, #112]\n"
            "  ldp q4, q5, [x19, #144]\n"
            "  ldp q6, q7, [x19, #176]\n"
            "  ldp x0, x1, [x19]\n"
            "  ldp x2, x3, [x19, #16]\n"
            "  ldp x4, x5, [x19, #32]\n"
            "  ldp x6, x7, [x19, #48]\n"
            "  ldr x8, [x19, #64]\n"
            "  blr x16\n"
            "  stp x0, x1, [x19]\n"
            "  stp q0, q1, [x19, #80]\n"
            "  ldp x19, x30, [sp], #16\n"
            "  ret\n");

    guest_runtime::guest_runtime()
    {
        void* page = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
        {
            throw std::runtime_error("HVF: failed to allocate the guest runtime page");
        }

        this->page_ = static_cast<uint32_t*>(page);
        const auto at = [this](const size_t offset) { return this->page_ + (offset / sizeof(uint32_t)); };

        for (size_t entry = 0; entry < vector_count; ++entry)
        {
            auto* code = at(entry * vector_stride);
            code[0] = encode_hvc(static_cast<uint16_t>(first_vector_call + entry));
            code[1] = insn_branch_to_self;
        }

        auto* exit = at(exit_stub_offset);
        exit[0] = encode_hvc(exit_call);
        exit[1] = insn_branch_to_self;

        auto* flush = at(tlb_flush_stub_offset);
        flush[0] = insn_tlbi_vmalle1is;
        flush[1] = insn_dsb_ish;
        flush[2] = insn_isb;
        flush[3] = encode_hvc(tlb_flush_done_call);
        flush[4] = insn_branch_to_self;

        for (size_t index = 0; index < max_host_calls; ++index)
        {
            auto* stub = at(host_call_stubs_offset + (index * host_call_stub_size));
            stub[0] = encode_hvc(static_cast<uint16_t>(index));
            stub[1] = insn_ret;
        }

        ::sys_icache_invalidate(this->page_, size);
    }

    guest_runtime::~guest_runtime()
    {
        ::munmap(this->page_, size);
    }

    uint64_t guest_runtime::exit_stub() const
    {
        return this->address() + exit_stub_offset;
    }

    uint64_t guest_runtime::tlb_flush_stub() const
    {
        return this->address() + tlb_flush_stub_offset;
    }

    uint64_t guest_runtime::host_call_stub(const size_t index) const
    {
        return this->address() + host_call_stubs_offset + (index * host_call_stub_size);
    }

    size_t host_call_table::add(const uint64_t function, const bool uses_vector_registers)
    {
        const std::scoped_lock lock(this->mutex_);

        for (size_t index = 0; index < this->calls_.size(); ++index)
        {
            const auto& call = this->calls_[index];
            if (call.function == function && call.uses_vector_registers == uses_vector_registers)
            {
                return index;
            }
        }

        if (this->calls_.size() >= max_host_calls)
        {
            throw std::runtime_error("HVF: out of host call stubs");
        }

        this->calls_.push_back({.function = function, .uses_vector_registers = uses_vector_registers});
        return this->calls_.size() - 1;
    }

    std::optional<host_call> host_call_table::get(const size_t index) const
    {
        const std::scoped_lock lock(this->mutex_);
        if (index >= this->calls_.size())
        {
            return std::nullopt;
        }

        return this->calls_[index];
    }
}
