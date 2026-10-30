#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>

namespace sogen::fex::hvf
{
    constexpr size_t stage1_page_size = 0x1000;
    constexpr size_t stage2_page_size = 0x4000;

    class stage2_mapper
    {
      public:
        virtual bool is_host_executable(uint64_t host_address, size_t size) const = 0;
        virtual void map(uint64_t host_address, uint64_t ipa, size_t size, int prot) = 0;
        virtual void unmap(uint64_t ipa, size_t size) = 0;
        virtual void protect(uint64_t ipa, size_t size, int prot) = 0;

      protected:
        ~stage2_mapper() = default;
    };

    // Makes host memory visible inside the VM at its own host address. Stage 1 translates each address to a
    // guest-physical address (IPA) and grants everything, stage 2 maps that IPA onto the host page and holds the
    // protection, so every permission fault leaves the VM. IPAs are a small resource, so freed runs are reused.
    class address_space
    {
      public:
        address_space(stage2_mapper& stage2, uint64_t ipa_limit);
        ~address_space();

        address_space(const address_space&) = delete;
        address_space& operator=(const address_space&) = delete;
        address_space(address_space&&) = delete;
        address_space& operator=(address_space&&) = delete;

        // Ranges are widened to whole stage-2 pages. map() also reprotects pages that are already mapped,
        // protect() requires every page to be mapped, and unmap() skips pages that are not.
        void map(uint64_t address, size_t size, int prot);
        void unmap(uint64_t address, size_t size);
        void protect(uint64_t address, size_t size, int prot);

        // The host replaced the pages behind a mapped range, which stage 2 would otherwise keep pointing at.
        void refresh_backing(uint64_t address, size_t size);

        uint64_t stage1_root() const
        {
            return this->stage1_root_ipa_;
        }

        // Changes whenever a mapping is removed, which makes translations the vCPU has cached stale.
        uint64_t stage1_generation() const
        {
            return this->stage1_generation_.load(std::memory_order_acquire);
        }

      private:
        struct page_state
        {
            uint64_t ipa{};
            int prot{};
        };

        stage2_mapper& stage2_;
        uint64_t ipa_limit_{};
        uint64_t next_ipa_{};
        std::map<uint64_t, page_state> pages_;

        std::map<uint64_t, uint64_t> free_ipas_;
        std::set<std::pair<uint64_t, uint64_t>> free_ipas_by_size_;

        uint8_t* table_pool_{};
        size_t table_pool_used_{};
        uint64_t table_pool_ipa_{};
        uint64_t* stage1_root_{};
        uint64_t stage1_root_ipa_{};
        std::atomic<uint64_t> stage1_generation_{0};

        uint64_t allocate_ipa(size_t size);
        void release_ipa(uint64_t ipa, uint64_t size);
        void map_stage2(uint64_t address, uint64_t ipa, size_t size, int prot);

        uint64_t* allocate_table();
        uint64_t table_ipa(const uint64_t* table) const;
        uint64_t* stage1_entry(uint64_t address);
        void set_stage1_range(uint64_t address, uint64_t ipa, size_t size);
        void clear_stage1_range(uint64_t address, size_t size);

        // The end of the run of mapped pages from start whose IPAs are contiguous and that extends() accepts.
        template <typename Predicate>
        uint64_t run_end(const uint64_t start, const uint64_t end, Predicate&& extends) const
        {
            uint64_t expected_ipa = this->pages_.at(start).ipa + stage2_page_size;
            uint64_t cursor = start + stage2_page_size;
            for (; cursor < end; cursor += stage2_page_size, expected_ipa += stage2_page_size)
            {
                const auto page = this->pages_.find(cursor);
                if (page == this->pages_.end() || page->second.ipa != expected_ipa || !extends(page->second))
                {
                    break;
                }
            }

            return cursor;
        }
    };
}
