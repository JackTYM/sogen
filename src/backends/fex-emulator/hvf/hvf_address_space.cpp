#include "hvf_address_space.hpp"

#include <array>
#include <cstdio>
#include <iterator>
#include <stdexcept>

#include <sys/mman.h>

namespace sogen::fex::hvf
{
    namespace
    {
        constexpr size_t table_pool_size = 128ULL << 20;
        constexpr uint64_t first_ipa = 0x10000;
        constexpr uint64_t stage1_address_limit = 1ULL << 48;
        constexpr size_t table_entry_count = stage1_page_size / sizeof(uint64_t);

        constexpr uint64_t descriptor_valid = 0b11;
        constexpr uint64_t descriptor_inner_shareable = 3ULL << 8;
        constexpr uint64_t descriptor_access_flag = 1ULL << 10;
        constexpr uint64_t descriptor_unprivileged_execute_never = 1ULL << 54;
        constexpr uint64_t descriptor_address_mask = 0x0000FFFFFFFFF000ULL;

        constexpr uint64_t page_descriptor_attributes =
            descriptor_valid | descriptor_inner_shareable | descriptor_access_flag | descriptor_unprivileged_execute_never;

        uint64_t align_down(const uint64_t value)
        {
            return value & ~(static_cast<uint64_t>(stage2_page_size) - 1);
        }

        uint64_t align_up(const uint64_t value)
        {
            return align_down(value + stage2_page_size - 1);
        }

        [[noreturn]] void throw_unmapped_page(const uint64_t address)
        {
            std::array<char, 96> message{};
            snprintf(message.data(), message.size(), "HVF: cannot protect unmapped page 0x%llx", static_cast<unsigned long long>(address));
            throw std::runtime_error(message.data());
        }
    }

    address_space::address_space(stage2_mapper& stage2, const uint64_t ipa_limit)
        : stage2_(stage2),
          ipa_limit_(ipa_limit),
          next_ipa_(first_ipa)
    {
        void* pool = ::mmap(nullptr, table_pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pool == MAP_FAILED)
        {
            throw std::runtime_error("HVF: failed to allocate the stage-1 table pool");
        }

        this->table_pool_ = static_cast<uint8_t*>(pool);

        try
        {
            this->table_pool_ipa_ = this->allocate_ipa(table_pool_size);
            this->map_stage2(reinterpret_cast<uint64_t>(pool), this->table_pool_ipa_, table_pool_size, PROT_READ | PROT_WRITE);
        }
        catch (...)
        {
            ::munmap(pool, table_pool_size);
            throw;
        }

        this->stage1_root_ = this->allocate_table();
        this->stage1_root_ipa_ = this->table_ipa(this->stage1_root_);
    }

    address_space::~address_space()
    {
        ::munmap(this->table_pool_, table_pool_size);
    }

    void address_space::map(const uint64_t address, const size_t size, const int prot)
    {
        const uint64_t end = align_up(address + size);
        uint64_t cursor = align_down(address);

        while (cursor < end)
        {
            if (this->pages_.contains(cursor))
            {
                const uint64_t mapped_end = this->run_end(cursor, end, [](const page_state&) { return true; });
                this->protect(cursor, mapped_end - cursor, prot);
                cursor = mapped_end;
                continue;
            }

            uint64_t unmapped_end = cursor + stage2_page_size;
            while (unmapped_end < end && !this->pages_.contains(unmapped_end))
            {
                unmapped_end += stage2_page_size;
            }

            const size_t run_size = unmapped_end - cursor;
            const uint64_t ipa = this->allocate_ipa(run_size);
            try
            {
                this->map_stage2(cursor, ipa, run_size, prot);
            }
            catch (...)
            {
                this->release_ipa(ipa, run_size);
                throw;
            }

            this->set_stage1_range(cursor, ipa, run_size);
            for (uint64_t page = cursor; page < unmapped_end; page += stage2_page_size)
            {
                this->pages_[page] = page_state{.ipa = ipa + (page - cursor), .prot = prot};
            }

            cursor = unmapped_end;
        }
    }

    void address_space::unmap(const uint64_t address, const size_t size)
    {
        const uint64_t end = align_up(address + size);
        uint64_t cursor = align_down(address);

        while (cursor < end)
        {
            const auto page = this->pages_.find(cursor);
            if (page == this->pages_.end())
            {
                cursor += stage2_page_size;
                continue;
            }

            const uint64_t ipa = page->second.ipa;
            const uint64_t mapped_end = this->run_end(cursor, end, [](const page_state&) { return true; });
            const size_t run_size = mapped_end - cursor;

            this->stage2_.unmap(ipa, run_size);
            this->clear_stage1_range(cursor, run_size);
            this->pages_.erase(page, this->pages_.lower_bound(mapped_end));
            this->release_ipa(ipa, run_size);

            cursor = mapped_end;
        }
    }

    void address_space::protect(const uint64_t address, const size_t size, const int prot)
    {
        const uint64_t end = align_up(address + size);
        uint64_t cursor = align_down(address);

        for (uint64_t page = cursor; page < end; page += stage2_page_size)
        {
            if (!this->pages_.contains(page))
            {
                throw_unmapped_page(page);
            }
        }

        while (cursor < end)
        {
            const auto& state = this->pages_.at(cursor);
            if (state.prot == prot)
            {
                cursor += stage2_page_size;
                continue;
            }

            const uint64_t changed_end = this->run_end(cursor, end, [prot](const page_state& next) { return next.prot != prot; });
            this->stage2_.protect(state.ipa, changed_end - cursor, prot);
            for (uint64_t page = cursor; page < changed_end; page += stage2_page_size)
            {
                this->pages_.at(page).prot = prot;
            }

            cursor = changed_end;
        }
    }

    void address_space::refresh_backing(const uint64_t address, const size_t size)
    {
        const uint64_t end = align_up(address + size);
        uint64_t cursor = align_down(address);

        while (cursor < end)
        {
            const auto page = this->pages_.find(cursor);
            if (page == this->pages_.end())
            {
                cursor += stage2_page_size;
                continue;
            }

            const auto state = page->second;
            const uint64_t refreshed_end = this->run_end(cursor, end, [&state](const page_state& next) { return next.prot == state.prot; });
            const size_t run_size = refreshed_end - cursor;

            this->stage2_.unmap(state.ipa, run_size);
            this->map_stage2(cursor, state.ipa, run_size, state.prot);

            cursor = refreshed_end;
        }
    }

    // Apple Silicon's SPTM halts the whole machine, rather than failing the call, when a frame the host has
    // mapped executable ends up in a guest's stage-2 tables, and it does so lazily, on the guest's first
    // access. Refusing such a range here turns a missed host protection change into an exception.
    void address_space::map_stage2(const uint64_t address, const uint64_t ipa, const size_t size, const int prot)
    {
        if (this->stage2_.is_host_executable(address, size))
        {
            std::array<char, 96> message{};
            snprintf(message.data(), message.size(), "HVF: refusing to map host-executable memory at 0x%llx into the VM",
                     static_cast<unsigned long long>(address));
            throw std::runtime_error(message.data());
        }

        this->stage2_.map(address, ipa, size, prot);
    }

    uint64_t address_space::allocate_ipa(const size_t size)
    {
        const uint64_t needed = align_up(size);

        const auto fit = this->free_ipas_by_size_.lower_bound({needed, 0});
        if (fit != this->free_ipas_by_size_.end())
        {
            const auto [block_size, ipa] = *fit;
            this->free_ipas_by_size_.erase(fit);
            this->free_ipas_.erase(ipa);

            if (block_size > needed)
            {
                this->free_ipas_.emplace(ipa + needed, block_size - needed);
                this->free_ipas_by_size_.emplace(block_size - needed, ipa + needed);
            }

            return ipa;
        }

        if (needed > this->ipa_limit_ || this->next_ipa_ > this->ipa_limit_ - needed)
        {
            throw std::runtime_error("HVF: guest-physical address space exhausted");
        }

        const uint64_t ipa = this->next_ipa_;
        this->next_ipa_ += needed;
        return ipa;
    }

    void address_space::release_ipa(uint64_t ipa, uint64_t size)
    {
        auto next = this->free_ipas_.lower_bound(ipa);
        if (next != this->free_ipas_.begin())
        {
            const auto previous = std::prev(next);
            if (previous->first + previous->second == ipa)
            {
                ipa = previous->first;
                size += previous->second;
                this->free_ipas_by_size_.erase({previous->second, previous->first});
                next = this->free_ipas_.erase(previous);
            }
        }

        if (next != this->free_ipas_.end() && next->first == ipa + size)
        {
            size += next->second;
            this->free_ipas_by_size_.erase({next->second, next->first});
            this->free_ipas_.erase(next);
        }

        if (ipa + size == this->next_ipa_)
        {
            this->next_ipa_ = ipa;
            return;
        }

        this->free_ipas_.emplace(ipa, size);
        this->free_ipas_by_size_.emplace(size, ipa);
    }

    uint64_t* address_space::allocate_table()
    {
        if (this->table_pool_used_ + stage1_page_size > table_pool_size)
        {
            throw std::runtime_error("HVF: stage-1 table pool exhausted");
        }

        auto* table = reinterpret_cast<uint64_t*>(this->table_pool_ + this->table_pool_used_);
        this->table_pool_used_ += stage1_page_size;
        return table;
    }

    uint64_t address_space::table_ipa(const uint64_t* table) const
    {
        return this->table_pool_ipa_ + (reinterpret_cast<uint64_t>(table) - reinterpret_cast<uint64_t>(this->table_pool_));
    }

    uint64_t* address_space::stage1_entry(const uint64_t address)
    {
        if (address >= stage1_address_limit)
        {
            throw std::runtime_error("HVF: address outside the 48-bit stage-1 range");
        }

        uint64_t* table = this->stage1_root_;
        for (int shift = 39; shift > 12; shift -= 9)
        {
            auto& entry = table[(address >> shift) & (table_entry_count - 1)];
            if (entry == 0)
            {
                entry = this->table_ipa(this->allocate_table()) | descriptor_valid;
            }

            table = reinterpret_cast<uint64_t*>(this->table_pool_ + ((entry & descriptor_address_mask) - this->table_pool_ipa_));
        }

        return &table[(address >> 12) & (table_entry_count - 1)];
    }

    void address_space::set_stage1_range(const uint64_t address, const uint64_t ipa, const size_t size)
    {
        for (size_t offset = 0; offset < size; offset += stage1_page_size)
        {
            *this->stage1_entry(address + offset) = (ipa + offset) | page_descriptor_attributes;
        }
    }

    void address_space::clear_stage1_range(const uint64_t address, const size_t size)
    {
        for (size_t offset = 0; offset < size; offset += stage1_page_size)
        {
            *this->stage1_entry(address + offset) = 0;
        }

        this->stage1_generation_.fetch_add(1, std::memory_order_release);
    }
}
