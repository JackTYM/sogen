#pragma once

#ifdef __APPLE__

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <atomic>

#include "hvf_hypercalls.hpp"

namespace sogen::fex::hvf
{
    constexpr size_t guest_page_size = 0x1000;
    constexpr size_t vm_page_size = 0x4000; // hv_vm_map / Apple host page granularity

    enum class vm_create_result
    {
        hardware_tso, // VM up, EnTSO settable and reads back 1
        no_hardware_tso,
        unavailable, // hv_vm_create/hv_vcpu_create refused (entitlement, OS, nested VM)
    };

    // Process-wide Hypervisor.framework VM under the same-VA, compact-IPA model: every mapped host
    // VA range keeps its exact address inside the vCPU via 4KB-granule stage-1 tables whose output
    // IPAs come from a compact bump allocator backed by hv_vm_map of the same physical pages.
    class hvf_vm
    {
      public:
        static hvf_vm& instance();

        vm_create_result create();

        bool active() const
        {
            return this->active_;
        }

        // All addresses are host VAs; ranges are rounded outward to vm_page_size. map() establishes
        // or updates; protect() requires the pages to be mapped; unmap()/sync_page(PROT_NONE) drop
        // pages that are mapped and ignore ones that are not.
        void map(uint64_t va, size_t size, int prot);
        void unmap(uint64_t va, size_t size);
        void protect(uint64_t va, size_t size, int prot);

        // Single-page choke-point mirror: PROT_NONE unmaps, otherwise maps or reprotects.
        void sync_page(uint64_t va, int prot);

        // The physical pages under an already-mapped VA range were replaced in place (fresh
        // mmap(MAP_FIXED)); re-establish the stage-2 association for whatever subrange is mapped.
        void refresh_backing(uint64_t va, size_t size);

        bool is_mapped_page(uint64_t va) const;

        uint64_t stage1_root_ipa() const
        {
            return this->root_ipa_;
        }

        uint64_t stage1_generation() const
        {
            return this->stage1_generation_.load(std::memory_order_acquire);
        }

        const hvf_guest_runtime& runtime() const
        {
            return this->runtime_;
        }

        // Registers (or finds) the hypercall shim for a host function; returns the guest stub VA.
        uint64_t register_callback(uint64_t original, bool needs_fp);
        bool lookup_callback(uint16_t id, hvf_callback_slot& out) const;

        hvf_vm(const hvf_vm&) = delete;
        hvf_vm& operator=(const hvf_vm&) = delete;

      private:
        hvf_vm() = default;

        struct page_state
        {
            uint64_t ipa = 0;
            int prot = 0;
        };

        uint64_t alloc_ipa_locked(size_t size);
        uint64_t* stage1_walk_locked(uint64_t va);
        uint64_t* stage1_alloc_table_locked();
        uint64_t stage1_table_ipa(const uint64_t* table) const;
        void stage1_set_range_locked(uint64_t va, uint64_t ipa, size_t size);
        void stage1_clear_range_locked(uint64_t va, size_t size);
        void map_locked(uint64_t va, size_t size, int prot);
        void unmap_locked(uint64_t va, size_t size);
        void protect_locked(uint64_t va, size_t size, int prot);

        mutable std::mutex lock_;

        bool created_ = false;
        bool active_ = false;
        vm_create_result create_result_ = vm_create_result::unavailable;

        uint64_t next_ipa_ = 0x10000;
        std::map<uint64_t, page_state> pages_;
        std::atomic<uint64_t> stage1_generation_{1};

        uint8_t* table_pool_ = nullptr;
        size_t table_pool_size_ = 0;
        size_t table_pool_used_ = 0;
        uint64_t table_pool_ipa_ = 0;
        uint64_t* stage1_root_ = nullptr;
        uint64_t root_ipa_ = 0;

        hvf_guest_runtime runtime_{};

        std::vector<hvf_callback_slot> callbacks_;
    };
}

#endif
