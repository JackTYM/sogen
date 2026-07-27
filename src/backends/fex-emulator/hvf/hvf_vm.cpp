#ifdef __APPLE__

#include "hvf_vm.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "hvf_vcpu_executor.hpp"

#include <sys/mman.h>
#include <Hypervisor/Hypervisor.h>

namespace sogen::fex::hvf
{
    namespace
    {
        // HV_SYS_REG_ACTLR_EL1 is declared macOS-15+ in the SDK while the project's deployment
        // target is older; the raw encoding avoids the availability-guarded enumerator.
        constexpr auto sys_reg_actlr_el1 = static_cast<hv_sys_reg_t>(0xc081);

        constexpr size_t table_pool_bytes = 128ull << 20;

        constexpr uint64_t attr_valid_page = 0b11ull;
        constexpr uint64_t attr_valid_table = 0b11ull;
        constexpr uint64_t attr_af = 1ull << 10;
        constexpr uint64_t attr_sh_inner = 3ull << 8;
        constexpr uint64_t attr_uxn = 1ull << 54;

        const char* hv_err_str(const hv_return_t r)
        {
            switch (static_cast<uint32_t>(r))
            {
            case 0:
                return "HV_SUCCESS";
            case 0xfae94001:
                return "HV_ERROR";
            case 0xfae94002:
                return "HV_BUSY";
            case 0xfae94003:
                return "HV_BAD_ARGUMENT";
            case 0xfae94004:
                return "HV_ILLEGAL_GUEST_STATE";
            case 0xfae94005:
                return "HV_NO_RESOURCES";
            case 0xfae94006:
                return "HV_NO_DEVICE";
            case 0xfae94007:
                return "HV_DENIED";
            case 0xfae9400f:
                return "HV_UNSUPPORTED";
            default:
                return "HV_<unknown>";
            }
        }

        [[noreturn]] void throw_hv_error(const char* what, const hv_return_t r)
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "HVF: %s failed: 0x%x (%s)", what, static_cast<uint32_t>(r), hv_err_str(r));
            throw std::runtime_error(buf);
        }

        void check_hv(const char* what, const hv_return_t r)
        {
            if (r != HV_SUCCESS)
            {
                throw_hv_error(what, r);
            }
        }

        hv_memory_flags_t to_hv_flags(const int prot)
        {
            hv_memory_flags_t flags = 0;
            if (prot & PROT_READ)
            {
                flags |= HV_MEMORY_READ;
            }
            if (prot & PROT_WRITE)
            {
                flags |= HV_MEMORY_WRITE;
            }
            if (prot & PROT_EXEC)
            {
                flags |= HV_MEMORY_EXEC;
            }
            return flags;
        }

        // Stage-1 entries are always fully permissive: permissions are enforced exclusively by
        // stage-2 (hv_vm_map/hv_vm_protect). This keeps every permission fault a stage-2 abort -
        // i.e. a VM exit the host handles directly (the InterruptFaultPage stop protocol depends on
        // this) - and means protect() never has to touch stage-1 entries or flush the guest TLB.
        uint64_t stage1_page_attrs()
        {
            return attr_valid_page | attr_af | attr_sh_inner | attr_uxn;
        }

        uint64_t page_align_down(const uint64_t value)
        {
            return value & ~(static_cast<uint64_t>(vm_page_size) - 1);
        }

        uint64_t page_align_up(const uint64_t value)
        {
            return (value + vm_page_size - 1) & ~(static_cast<uint64_t>(vm_page_size) - 1);
        }
    }

    hvf_vm& hvf_vm::instance()
    {
        static hvf_vm vm;
        return vm;
    }

    vm_create_result hvf_vm::create()
    {
        const std::lock_guard guard(this->lock_);

        if (this->created_)
        {
            return this->create_result_;
        }
        this->created_ = true;

        hv_return_t r = hv_vm_create(nullptr);
        if (r != HV_SUCCESS)
        {
            fprintf(stderr, "[FEX backend] HVF: hv_vm_create -> %s\n", hv_err_str(r));
            this->create_result_ = vm_create_result::unavailable;
            return this->create_result_;
        }

        // Probe hardware TSO on a throwaway vCPU: the setting is per-vCPU, so what matters is
        // whether ACTLR_EL1.EnTSO can be set and read back on this chip/OS at all.
        {
            hv_vcpu_t probe_vcpu = 0;
            hv_vcpu_exit_t* probe_exit = nullptr;
            r = hv_vcpu_create(&probe_vcpu, &probe_exit, nullptr);
            if (r != HV_SUCCESS)
            {
                fprintf(stderr, "[FEX backend] HVF: hv_vcpu_create (probe) -> %s\n", hv_err_str(r));
                hv_vm_destroy();
                this->create_result_ = vm_create_result::unavailable;
                return this->create_result_;
            }

            uint64_t actlr = 0;
            bool tso_ok = hv_vcpu_get_sys_reg(probe_vcpu, sys_reg_actlr_el1, &actlr) == HV_SUCCESS;
            tso_ok = tso_ok && hv_vcpu_set_sys_reg(probe_vcpu, sys_reg_actlr_el1, actlr | (1ull << 1)) == HV_SUCCESS;
            uint64_t readback = 0;
            tso_ok = tso_ok && hv_vcpu_get_sys_reg(probe_vcpu, sys_reg_actlr_el1, &readback) == HV_SUCCESS && ((readback >> 1) & 1) == 1;
            hv_vcpu_destroy(probe_vcpu);

            if (!tso_ok)
            {
                fprintf(stderr, "[FEX backend] HVF: ACTLR_EL1.EnTSO not available on this machine\n");
                hv_vm_destroy();
                this->create_result_ = vm_create_result::no_hardware_tso;
                return this->create_result_;
            }
        }

        this->table_pool_size_ = table_pool_bytes;
        void* pool = ::mmap(nullptr, this->table_pool_size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pool == MAP_FAILED)
        {
            hv_vm_destroy();
            throw std::runtime_error("HVF: failed to allocate the stage-1 table pool");
        }
        this->table_pool_ = static_cast<uint8_t*>(pool);
        this->table_pool_ipa_ = this->alloc_ipa_locked(this->table_pool_size_);
        check_hv("hv_vm_map(table pool)", hv_vm_map(pool, this->table_pool_ipa_, this->table_pool_size_, HV_MEMORY_READ | HV_MEMORY_WRITE));
        this->stage1_root_ = this->stage1_alloc_table_locked();
        this->root_ipa_ = this->stage1_table_ipa(this->stage1_root_);

        this->runtime_.build();
        this->map_locked(reinterpret_cast<uint64_t>(this->runtime_.page), hvf_guest_runtime::page_bytes, PROT_READ | PROT_EXEC);

        this->x87_fastpath_.build();
        this->map_locked(reinterpret_cast<uint64_t>(this->x87_fastpath_.page()), hvf_x87_fastpath::page_bytes, PROT_READ | PROT_EXEC);

        this->active_ = true;
        this->create_result_ = vm_create_result::hardware_tso;
        return this->create_result_;
    }

    uint32_t hvf_vm::max_vcpu_count() const
    {
        uint32_t count = 0;
        if (hv_vm_get_max_vcpu_count(&count) != HV_SUCCESS)
        {
            return 0;
        }
        return count;
    }

    void hvf_vm::register_vcpu(hvf_vcpu_executor& vcpu)
    {
        const std::lock_guard guard(this->lock_);
        this->vcpus_.push_back(&vcpu);
    }

    void hvf_vm::unregister_vcpu(hvf_vcpu_executor& vcpu)
    {
        const std::lock_guard guard(this->lock_);
        this->vcpus_.erase(std::remove(this->vcpus_.begin(), this->vcpus_.end(), &vcpu), this->vcpus_.end());
    }

    // A vCPU only re-walks the stage-1 tables when its run loop notices the generation moved, and it
    // can only notice that between hv_vcpu_run calls. Any other vCPU currently inside hv_vcpu_run
    // would keep using cached translations for entries this edit just invalidated, so force it out:
    // HV_EXIT_REASON_CANCELED lands back at the top of the run loop, which flushes before re-entering.
    void hvf_vm::bump_stage1_generation_locked()
    {
        this->stage1_generation_.fetch_add(1, std::memory_order_release);
        for (auto* const vcpu : this->vcpus_)
        {
            vcpu->kick_for_stage1_invalidation();
        }
    }

    uint64_t hvf_vm::alloc_ipa_locked(const size_t size)
    {
        const uint64_t ipa = this->next_ipa_;
        this->next_ipa_ += page_align_up(size);
        return ipa;
    }

    uint64_t* hvf_vm::stage1_alloc_table_locked()
    {
        if (this->table_pool_used_ + guest_page_size > this->table_pool_size_)
        {
            throw std::runtime_error("HVF: stage-1 table pool exhausted");
        }
        auto* table = reinterpret_cast<uint64_t*>(this->table_pool_ + this->table_pool_used_);
        this->table_pool_used_ += guest_page_size;
        return table;
    }

    uint64_t hvf_vm::stage1_table_ipa(const uint64_t* table) const
    {
        return this->table_pool_ipa_ + (reinterpret_cast<uintptr_t>(table) - reinterpret_cast<uintptr_t>(this->table_pool_));
    }

    uint64_t* hvf_vm::stage1_walk_locked(const uint64_t va)
    {
        if ((va >> 48) != 0)
        {
            throw std::runtime_error("HVF: VA outside the 48-bit stage-1 range");
        }
        uint64_t* table = this->stage1_root_;
        for (int shift = 39; shift > 12; shift -= 9)
        {
            const size_t index = (va >> shift) & 0x1FF;
            if (table[index] == 0)
            {
                uint64_t* next = this->stage1_alloc_table_locked();
                table[index] = this->stage1_table_ipa(next) | attr_valid_table;
            }
            table = reinterpret_cast<uint64_t*>(this->table_pool_ + ((table[index] & ~0xFFFull) - this->table_pool_ipa_));
        }
        return &table[(va >> 12) & 0x1FF];
    }

    void hvf_vm::stage1_set_range_locked(const uint64_t va, const uint64_t ipa, const size_t size)
    {
        const uint64_t attrs = stage1_page_attrs();
        bool replaced_valid = false;
        for (size_t off = 0; off < size; off += guest_page_size)
        {
            uint64_t* entry = this->stage1_walk_locked(va + off);
            replaced_valid = replaced_valid || *entry != 0;
            *entry = (ipa + off) | attrs;
        }
        if (replaced_valid)
        {
            this->bump_stage1_generation_locked();
        }
    }

    void hvf_vm::stage1_clear_range_locked(const uint64_t va, const size_t size)
    {
        for (size_t off = 0; off < size; off += guest_page_size)
        {
            *this->stage1_walk_locked(va + off) = 0;
        }
        this->bump_stage1_generation_locked();
    }

    void hvf_vm::map_locked(const uint64_t va, const size_t size, const int prot)
    {
        uint64_t cursor = page_align_down(va);
        const uint64_t end = page_align_up(va + size);

        while (cursor < end)
        {
            const auto it = this->pages_.find(cursor);
            if (it != this->pages_.end())
            {
                if (it->second.prot != prot)
                {
                    this->protect_locked(cursor, vm_page_size, prot);
                }
                cursor += vm_page_size;
                continue;
            }

            uint64_t run_end = cursor + vm_page_size;
            while (run_end < end && !this->pages_.contains(run_end))
            {
                run_end += vm_page_size;
            }

            const size_t run_size = run_end - cursor;
            const uint64_t ipa = this->alloc_ipa_locked(run_size);
            const hv_return_t r = hv_vm_map(reinterpret_cast<void*>(cursor), ipa, run_size, to_hv_flags(prot));
            if (r != HV_SUCCESS)
            {
                char buf[192];
                snprintf(buf, sizeof(buf), "hv_vm_map(host=0x%llx, ipa=0x%llx, size=0x%zx, prot=%d)",
                         static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(ipa), run_size, prot);
                throw_hv_error(buf, r);
            }
            this->stage1_set_range_locked(cursor, ipa, run_size);
            for (uint64_t page = cursor; page < run_end; page += vm_page_size)
            {
                this->pages_[page] = page_state{.ipa = ipa + (page - cursor), .prot = prot};
            }
            cursor = run_end;
        }
    }

    void hvf_vm::unmap_locked(const uint64_t va, const size_t size)
    {
        uint64_t cursor = page_align_down(va);
        const uint64_t end = page_align_up(va + size);

        while (cursor < end)
        {
            const auto it = this->pages_.find(cursor);
            if (it == this->pages_.end())
            {
                cursor += vm_page_size;
                continue;
            }

            uint64_t run_end = cursor + vm_page_size;
            uint64_t expected_ipa = it->second.ipa + vm_page_size;
            while (run_end < end)
            {
                const auto next = this->pages_.find(run_end);
                if (next == this->pages_.end() || next->second.ipa != expected_ipa)
                {
                    break;
                }
                run_end += vm_page_size;
                expected_ipa += vm_page_size;
            }

            const size_t run_size = run_end - cursor;
            check_hv("hv_vm_unmap", hv_vm_unmap(it->second.ipa, run_size));
            this->stage1_clear_range_locked(cursor, run_size);
            for (uint64_t page = cursor; page < run_end; page += vm_page_size)
            {
                this->pages_.erase(page);
            }
            cursor = run_end;
        }
    }

    void hvf_vm::protect_locked(const uint64_t va, const size_t size, const int prot)
    {
        uint64_t cursor = page_align_down(va);
        const uint64_t end = page_align_up(va + size);

        while (cursor < end)
        {
            const auto it = this->pages_.find(cursor);
            if (it == this->pages_.end())
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "HVF: protect of unmapped page 0x%llx", static_cast<unsigned long long>(cursor));
                throw std::runtime_error(buf);
            }
            if (it->second.prot != prot)
            {
                check_hv("hv_vm_protect", hv_vm_protect(it->second.ipa, vm_page_size, to_hv_flags(prot)));
                it->second.prot = prot;
            }
            cursor += vm_page_size;
        }
    }

    void hvf_vm::map(const uint64_t va, const size_t size, const int prot)
    {
        const std::lock_guard guard(this->lock_);
        this->map_locked(va, size, prot);
    }

    void hvf_vm::unmap(const uint64_t va, const size_t size)
    {
        const std::lock_guard guard(this->lock_);
        this->unmap_locked(va, size);
    }

    void hvf_vm::protect(const uint64_t va, const size_t size, const int prot)
    {
        const std::lock_guard guard(this->lock_);
        this->protect_locked(va, size, prot);
    }

    void hvf_vm::sync_page(const uint64_t va, const int prot)
    {
        const std::lock_guard guard(this->lock_);
        if (prot == PROT_NONE)
        {
            this->unmap_locked(va, vm_page_size);
            return;
        }
        this->map_locked(va, vm_page_size, prot);
    }

    void hvf_vm::refresh_backing(const uint64_t va, const size_t size)
    {
        const std::lock_guard guard(this->lock_);

        uint64_t cursor = page_align_down(va);
        const uint64_t end = page_align_up(va + size);
        while (cursor < end)
        {
            const auto it = this->pages_.find(cursor);
            if (it != this->pages_.end())
            {
                check_hv("hv_vm_unmap(refresh)", hv_vm_unmap(it->second.ipa, vm_page_size));
                check_hv("hv_vm_map(refresh)",
                         hv_vm_map(reinterpret_cast<void*>(cursor), it->second.ipa, vm_page_size, to_hv_flags(it->second.prot)));
            }
            cursor += vm_page_size;
        }
    }

    bool hvf_vm::is_mapped_page(const uint64_t va) const
    {
        const std::lock_guard guard(this->lock_);
        return this->pages_.contains(page_align_down(va));
    }

    uint64_t hvf_vm::register_callback(const uint64_t original, const bool needs_fp)
    {
        const std::lock_guard guard(this->callbacks_mutex_);

        for (size_t i = 0; i < this->callbacks_.size(); ++i)
        {
            if (this->callbacks_[i].original == original && this->callbacks_[i].needs_fp == needs_fp)
            {
                return this->runtime_.stub_va(i);
            }
        }

        if (this->callbacks_.size() >= hvf_guest_runtime::max_stubs)
        {
            throw std::runtime_error("HVF: hypercall stub table exhausted");
        }

        this->callbacks_.push_back(hvf_callback_slot{.original = original, .needs_fp = needs_fp});
        return this->runtime_.stub_va(this->callbacks_.size() - 1);
    }

    bool hvf_vm::lookup_callback(const uint16_t id, hvf_callback_slot& out) const
    {
        const std::lock_guard guard(this->callbacks_mutex_);
        if (id >= this->callbacks_.size())
        {
            return false;
        }
        out = this->callbacks_[id];
        return true;
    }
}

#endif
