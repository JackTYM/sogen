#include "std_include.hpp"
#include "kernel_state.hpp"

#include <chrono>
#include <cstring>
#include <random>

namespace sogen
{
    namespace
    {
        constexpr uint64_t arena_magic = 0x534f47454e4b4152ULL; // "SOGENKAR"
        constexpr uint32_t arena_version = 1;
    }

    struct kernel_arena::header
    {
        uint64_t magic;
        uint32_t version;
        uint32_t slot_count;
        uint64_t arena_id;
        std::atomic<uint32_t> allocation_hint;
    };

    struct kernel_arena::slot
    {
        std::atomic<uint32_t> reference_count;
        uint32_t reserved;
        std::array<std::atomic<uint64_t>, words_per_slot> words;
        std::array<uint64_t, 3> padding;
    };

    static_assert(sizeof(kernel_arena::slot) == 64);
    static_assert(sizeof(kernel_arena::header) <= sizeof(kernel_arena::slot));
    static_assert(std::atomic<uint32_t>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);

    std::shared_ptr<kernel_arena> kernel_arena::create()
    {
        auto backing = shared_backing::create((slot_count + 1) * sizeof(slot));
        if (!backing->is_shared())
        {
            return nullptr;
        }

        std::shared_ptr<kernel_arena> arena{new kernel_arena{std::move(backing)}};

        auto* const head = arena->get_header();
        head->magic = arena_magic;
        head->version = arena_version;
        head->slot_count = slot_count;
        head->arena_id = (static_cast<uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}() ^
                         static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        head->allocation_hint.store(0);

        return arena;
    }

    std::shared_ptr<kernel_arena> kernel_arena::adopt(std::shared_ptr<shared_backing> backing)
    {
        if (!backing || backing->size() < (slot_count + 1) * sizeof(slot))
        {
            return nullptr;
        }

        std::shared_ptr<kernel_arena> arena{new kernel_arena{std::move(backing)}};
        const auto* const head = arena->get_header();
        if (head->magic != arena_magic || head->version != arena_version || head->slot_count != slot_count)
        {
            return nullptr;
        }

        return arena;
    }

    uint64_t kernel_arena::id() const
    {
        return this->get_header()->arena_id;
    }

    kernel_arena::header* kernel_arena::get_header() const
    {
        return reinterpret_cast<header*>(this->backing_->data());
    }

    kernel_arena::slot* kernel_arena::get_slot(const uint32_t index) const
    {
        return reinterpret_cast<slot*>(this->backing_->data()) + 1 + index;
    }

    std::optional<uint32_t> kernel_arena::allocate()
    {
        auto* const head = this->get_header();
        const auto start = head->allocation_hint.load(std::memory_order_relaxed);

        for (uint32_t i = 0; i < slot_count; ++i)
        {
            const auto index = (start + i) % slot_count;
            auto* const candidate = this->get_slot(index);

            uint32_t expected = 0;
            if (!candidate->reference_count.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
            {
                continue;
            }

            for (auto& word : candidate->words)
            {
                word.store(0, std::memory_order_relaxed);
            }

            head->allocation_hint.store((index + 1) % slot_count, std::memory_order_relaxed);
            return index;
        }

        return std::nullopt;
    }

    void kernel_arena::add_reference(const uint32_t index)
    {
        this->get_slot(index)->reference_count.fetch_add(1, std::memory_order_acq_rel);
    }

    void kernel_arena::release(const uint32_t index)
    {
        this->get_slot(index)->reference_count.fetch_sub(1, std::memory_order_acq_rel);
    }

    std::atomic<uint64_t>* kernel_arena::words(const uint32_t index) const
    {
        return this->get_slot(index)->words.data();
    }

    bool kernel_state::promote(const std::shared_ptr<kernel_arena>& arena)
    {
        if (this->slot_)
        {
            return true;
        }

        const auto index = arena->allocate();
        if (!index)
        {
            return false;
        }

        auto slot = std::make_shared<kernel_slot>(arena, *index);
        for (size_t i = 0; i < kernel_arena::words_per_slot; ++i)
        {
            slot->words()[i].store(this->local_[i].load(std::memory_order_acquire), std::memory_order_release);
        }

        this->slot_ = std::move(slot);
        return true;
    }

    bool kernel_state::adopt(const std::shared_ptr<kernel_arena>& arena, const uint32_t index)
    {
        if (!arena || index >= kernel_arena::slot_count)
        {
            return false;
        }

        arena->add_reference(index);
        this->slot_ = std::make_shared<kernel_slot>(arena, index);
        return true;
    }

    void kernel_state::assign(const kernel_state& other)
    {
        if (other.slot_)
        {
            this->slot_ = other.slot_;
            return;
        }

        this->slot_.reset();
        for (size_t i = 0; i < kernel_arena::words_per_slot; ++i)
        {
            this->local_[i].store(other.local_[i].load(std::memory_order_acquire), std::memory_order_release);
        }
    }
}
