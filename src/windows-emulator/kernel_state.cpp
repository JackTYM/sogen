#include "std_include.hpp"
#include "kernel_state.hpp"

#include <chrono>
#include <cstring>
#include <random>

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
#include <sched.h>
#include <sys/mman.h>
#endif

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
        std::atomic<uint32_t> table_lock;
        std::atomic<uint32_t> next_process_index;
    };

    struct kernel_arena::slot
    {
        std::atomic<uint32_t> reference_count;
        std::atomic<uint32_t> generation;
        std::array<std::atomic<uint64_t>, words_per_slot> words;
        std::array<uint64_t, 3> padding;
    };

    struct kernel_arena::named_entry
    {
        std::atomic<uint32_t> state; // 0 empty, 1 used, 2 removed
        uint32_t kind;
        uint32_t slot;
        uint32_t generation;
        uint32_t key_length;
        uint32_t reserved;
        uint64_t hash;
        std::array<char16_t, max_named_key_length> key;
    };

    static_assert(sizeof(kernel_arena::slot) == 64);
    static_assert(sizeof(kernel_arena::named_entry) % 8 == 0);
    static_assert(sizeof(kernel_arena::header) <= sizeof(kernel_arena::slot));
    static_assert(std::atomic<uint32_t>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);

    namespace
    {
        constexpr size_t named_table_offset()
        {
            return (kernel_arena::slot_count + 1) * sizeof(kernel_arena::slot);
        }

        constexpr size_t arena_size()
        {
            return named_table_offset() + kernel_arena::named_entry_count * sizeof(kernel_arena::named_entry);
        }

        uint64_t hash_key(const std::u16string_view key)
        {
            uint64_t hash = 1469598103934665603ULL;
            for (const auto character : key)
            {
                hash ^= character;
                hash *= 1099511628211ULL;
            }
            return hash;
        }
    }

    std::shared_ptr<kernel_arena> kernel_arena::create()
    {
        auto backing = shared_backing::create(arena_size());
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
        head->table_lock.store(0);
        head->next_process_index.store(first_child_process_index);

        return arena;
    }

    std::shared_ptr<kernel_arena> kernel_arena::adopt(std::shared_ptr<shared_backing> backing)
    {
        if (!backing || backing->size() < arena_size())
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

    uint32_t kernel_arena::allocate_process_index()
    {
        return this->get_header()->next_process_index.fetch_add(1);
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
            candidate->generation.fetch_add(1, std::memory_order_acq_rel);

            head->allocation_hint.store((index + 1) % slot_count, std::memory_order_relaxed);
            return index;
        }

        return std::nullopt;
    }

    void kernel_arena::add_reference(const uint32_t index)
    {
        this->get_slot(index)->reference_count.fetch_add(1, std::memory_order_acq_rel);
    }

    bool kernel_arena::try_add_reference(const uint32_t index)
    {
        auto& count = this->get_slot(index)->reference_count;
        auto current = count.load(std::memory_order_acquire);
        while (current != 0)
        {
            if (count.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel))
            {
                return true;
            }
        }

        return false;
    }

    bool kernel_arena::is_live(const uint32_t index, const uint32_t generation) const
    {
        const auto* const candidate = this->get_slot(index);
        return candidate->reference_count.load(std::memory_order_acquire) != 0 &&
               candidate->generation.load(std::memory_order_acquire) == generation;
    }

    namespace
    {
        constexpr uint64_t shm_backed_flag = 1ULL << 63;
        constexpr size_t shm_name_capacity = 3 * sizeof(uint64_t);
    }

    void kernel_arena::release(const uint32_t index)
    {
        auto* const released = this->get_slot(index);
        if (released->reference_count.fetch_sub(1, std::memory_order_acq_rel) != 1)
        {
            return;
        }

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
        if ((released->words[3].load(std::memory_order_acquire) & shm_backed_flag) != 0)
        {
            std::array<char, shm_name_capacity + 1> name{};
            for (size_t i = 0; i < 3; ++i)
            {
                const auto word = released->words[i].load(std::memory_order_acquire);
                std::memcpy(name.data() + i * sizeof(uint64_t), &word, sizeof(word));
            }
            ::shm_unlink(name.data());
        }
#endif
    }

    bool kernel_arena::set_shm_backing(const uint32_t index, const std::string_view shm_name, const uint64_t meta)
    {
        if (shm_name.size() >= shm_name_capacity || (meta & shm_backed_flag) != 0)
        {
            return false;
        }

        auto* const target = this->get_slot(index);
        std::array<char, shm_name_capacity> name{};
        std::memcpy(name.data(), shm_name.data(), shm_name.size());
        for (size_t i = 0; i < 3; ++i)
        {
            uint64_t word{};
            std::memcpy(&word, name.data() + i * sizeof(uint64_t), sizeof(word));
            target->words[i].store(word, std::memory_order_release);
        }
        target->words[3].store(meta | shm_backed_flag, std::memory_order_release);
        return true;
    }

    bool kernel_arena::get_shm_backing(const uint32_t index, std::string& shm_name, uint64_t& meta) const
    {
        const auto* const source = this->get_slot(index);
        const auto last = source->words[3].load(std::memory_order_acquire);
        if ((last & shm_backed_flag) == 0)
        {
            return false;
        }

        std::array<char, shm_name_capacity + 1> name{};
        for (size_t i = 0; i < 3; ++i)
        {
            const auto word = source->words[i].load(std::memory_order_acquire);
            std::memcpy(name.data() + i * sizeof(uint64_t), &word, sizeof(word));
        }

        shm_name = name.data();
        meta = last & ~shm_backed_flag;
        return true;
    }

    kernel_arena::named_entry* kernel_arena::get_named_entry(const size_t index) const
    {
        return reinterpret_cast<named_entry*>(this->backing_->data() + named_table_offset()) + index;
    }

    namespace
    {
        class table_lock_guard
        {
          public:
            explicit table_lock_guard(std::atomic<uint32_t>& lock)
                : lock_(lock)
            {
                // Held for a handful of instructions; a holder that died mid-operation would otherwise wedge
                // every process, so give up waiting eventually and proceed.
                for (uint32_t spins = 0; this->lock_.exchange(1, std::memory_order_acquire) != 0 && spins < 5'000'000; ++spins)
                {
#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
                    ::sched_yield();
#endif
                }
            }

            ~table_lock_guard()
            {
                this->lock_.store(0, std::memory_order_release);
            }

            table_lock_guard(const table_lock_guard&) = delete;
            table_lock_guard& operator=(const table_lock_guard&) = delete;

          private:
            std::atomic<uint32_t>& lock_;
        };
    }

    std::optional<kernel_arena::named_object> kernel_arena::find_named(const std::u16string_view key)
    {
        if (key.empty() || key.size() > max_named_key_length)
        {
            return std::nullopt;
        }

        const auto hash = hash_key(key);
        const table_lock_guard guard{this->get_header()->table_lock};

        for (size_t i = 0; i < named_entry_count; ++i)
        {
            auto* const entry = this->get_named_entry((hash + i) % named_entry_count);
            const auto state = entry->state.load(std::memory_order_acquire);
            if (state == 0)
            {
                break;
            }

            if (state != 1 || entry->hash != hash || entry->key_length != key.size() ||
                std::u16string_view(entry->key.data(), entry->key_length) != key)
            {
                continue;
            }

            if (!this->is_live(entry->slot, entry->generation))
            {
                entry->state.store(2, std::memory_order_release);
                continue;
            }

            if (!this->try_add_reference(entry->slot))
            {
                continue;
            }

            return named_object{static_cast<named_object_kind>(entry->kind), entry->slot};
        }

        return std::nullopt;
    }

    std::optional<kernel_arena::named_object> kernel_arena::register_named(const std::u16string_view key, const named_object_kind kind,
                                                                           const uint32_t slot)
    {
        if (key.empty() || key.size() > max_named_key_length)
        {
            return std::nullopt;
        }

        const auto hash = hash_key(key);
        const table_lock_guard guard{this->get_header()->table_lock};

        named_entry* free_entry = nullptr;
        for (size_t i = 0; i < named_entry_count; ++i)
        {
            auto* const entry = this->get_named_entry((hash + i) % named_entry_count);
            const auto state = entry->state.load(std::memory_order_acquire);
            if (state != 1)
            {
                if (!free_entry)
                {
                    free_entry = entry;
                }

                if (state == 0)
                {
                    break;
                }
                continue;
            }

            if (entry->hash != hash || entry->key_length != key.size() || std::u16string_view(entry->key.data(), entry->key_length) != key)
            {
                continue;
            }

            if (!this->is_live(entry->slot, entry->generation))
            {
                entry->state.store(2, std::memory_order_release);
                if (!free_entry)
                {
                    free_entry = entry;
                }
                continue;
            }

            if (this->try_add_reference(entry->slot))
            {
                return named_object{static_cast<named_object_kind>(entry->kind), entry->slot};
            }
        }

        if (free_entry)
        {
            free_entry->kind = static_cast<uint32_t>(kind);
            free_entry->slot = slot;
            free_entry->generation = this->get_slot(slot)->generation.load(std::memory_order_acquire);
            free_entry->key_length = static_cast<uint32_t>(key.size());
            free_entry->hash = hash;
            std::copy(key.begin(), key.end(), free_entry->key.begin());
            free_entry->state.store(1, std::memory_order_release);
        }

        return std::nullopt;
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
