#pragma once

#ifdef __APPLE__

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <sys/mman.h>
#include <libkern/OSCacheControl.h>

namespace sogen::fex::hvf
{
    // Hypercall ID space carried in the hvc immediate (ESR ISS[15:0]).
    constexpr uint16_t hc_callback_base = 0x000; // + callback slot index
    constexpr uint16_t hc_vector_base = 0x100;   // + EL1 vector entry index (offset / 0x80)
    constexpr uint16_t hc_tlbi_done = 0x1F1;
    constexpr uint16_t hc_done = 0x1FF;

    constexpr uint32_t insn_ret = 0xD65F03C0;
    constexpr uint32_t insn_b_self = 0x14000000;
    constexpr uint32_t insn_tlbi_vmalle1is = 0xD508831F;
    constexpr uint32_t insn_dsb_ish = 0xD5033B9F;
    constexpr uint32_t insn_isb = 0xD5033FDF;

    constexpr uint32_t insn_hvc(const uint16_t imm)
    {
        return 0xD4000002u | (static_cast<uint32_t>(imm) << 5);
    }

    struct hvf_callback_slot
    {
        uint64_t original = 0;
        bool needs_fp = false;
    };

    // One read-execute page inside the VM holding all guest-resident runtime code: the EL1 vector
    // table (every entry immediately exits via a distinguishing hvc), the hypercall stubs the
    // rewritten JITPointers slots point at, the exit-done stub used as the dispatcher's return
    // address, and the stage-1 TLB flush stub.
    struct hvf_guest_runtime
    {
        static constexpr size_t page_bytes = 0x4000;
        static constexpr size_t max_stubs = 128;

        uint8_t* page = nullptr;
        uint64_t vector_table_va = 0;
        uint64_t done_stub_va = 0;
        uint64_t tlbi_stub_va = 0;
        uint64_t stub_base_va = 0;

        void build()
        {
            this->page =
                static_cast<uint8_t*>(::mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
            if (this->page == MAP_FAILED)
            {
                throw std::runtime_error("HVF: failed to allocate the guest runtime page");
            }

            auto* insns = reinterpret_cast<uint32_t*>(this->page);
            for (int entry = 0; entry < 16; ++entry)
            {
                insns[entry * 0x80 / 4] = insn_hvc(hc_vector_base + entry);
                insns[entry * 0x80 / 4 + 1] = insn_b_self;
            }
            this->vector_table_va = reinterpret_cast<uint64_t>(this->page);

            auto* done = reinterpret_cast<uint32_t*>(this->page + 0x800);
            done[0] = insn_hvc(hc_done);
            done[1] = insn_b_self;
            this->done_stub_va = reinterpret_cast<uint64_t>(done);

            auto* tlbi = reinterpret_cast<uint32_t*>(this->page + 0x840);
            tlbi[0] = insn_tlbi_vmalle1is;
            tlbi[1] = insn_dsb_ish;
            tlbi[2] = insn_isb;
            tlbi[3] = insn_hvc(hc_tlbi_done);
            tlbi[4] = insn_b_self;
            this->tlbi_stub_va = reinterpret_cast<uint64_t>(tlbi);

            auto* stubs = reinterpret_cast<uint32_t*>(this->page + 0x900);
            for (size_t i = 0; i < max_stubs; ++i)
            {
                stubs[i * 2] = insn_hvc(static_cast<uint16_t>(hc_callback_base + i));
                stubs[i * 2 + 1] = insn_ret;
            }
            this->stub_base_va = reinterpret_cast<uint64_t>(stubs);

            ::sys_icache_invalidate(this->page, page_bytes);
        }

        uint64_t stub_va(const size_t index) const
        {
            return this->stub_base_va + index * 8;
        }

        bool contains_stub(const uint64_t va) const
        {
            return va >= this->stub_base_va && va < this->stub_base_va + max_stubs * 8;
        }
    };
}

#endif
