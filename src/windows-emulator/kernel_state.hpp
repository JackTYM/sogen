#pragma once

#include "shared_backing.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace sogen
{
    // A fixed-size table of small state slots living in one shared_backing, created by the root sogen
    // host process and inherited by every process it spawns. Synchronization objects (events, mutants,
    // semaphores) that must be observable across host processes keep their state in a slot, so a
    // signal, acquire or release performed by one process is seen - atomically - by all of them.
    class kernel_arena
    {
      public:
        static constexpr size_t slot_count = 16384;
        static constexpr size_t words_per_slot = 4;

        static std::shared_ptr<kernel_arena> create();
        // Wraps a backing received from another process. Returns nullptr if it isn't an arena.
        static std::shared_ptr<kernel_arena> adopt(std::shared_ptr<shared_backing> backing);

        uint64_t id() const;

        const std::shared_ptr<shared_backing>& backing() const
        {
            return this->backing_;
        }

        // Claims a free slot (zeroed, reference count 1) or returns nullopt when the arena is full.
        std::optional<uint32_t> allocate();
        void add_reference(uint32_t index);
        void release(uint32_t index);
        std::atomic<uint64_t>* words(uint32_t index) const;

        struct header;
        struct slot;

      private:
        explicit kernel_arena(std::shared_ptr<shared_backing> backing)
            : backing_(std::move(backing))
        {
        }

        header* get_header() const;
        slot* get_slot(uint32_t index) const;

        std::shared_ptr<shared_backing> backing_{};
    };

    // One process's reference to an arena slot; the slot is recycled once every process dropped theirs.
    class kernel_slot
    {
      public:
        kernel_slot(std::shared_ptr<kernel_arena> arena, uint32_t index)
            : arena_(std::move(arena)),
              index_(index)
        {
        }

        ~kernel_slot()
        {
            this->arena_->release(this->index_);
        }

        kernel_slot(const kernel_slot&) = delete;
        kernel_slot& operator=(const kernel_slot&) = delete;

        std::atomic<uint64_t>* words() const
        {
            return this->arena_->words(this->index_);
        }

        uint32_t index() const
        {
            return this->index_;
        }

        const std::shared_ptr<kernel_arena>& arena() const
        {
            return this->arena_;
        }

      private:
        std::shared_ptr<kernel_arena> arena_{};
        uint32_t index_{};
    };

    // Storage for the atomic words that make up a synchronization object's state. It starts out
    // process-local; promote() moves it into an arena slot the first time the object is shared with
    // another process, after which every access goes through the shared slot.
    class kernel_state
    {
      public:
        kernel_state() = default;

        kernel_state(const kernel_state& other)
        {
            this->assign(other);
        }

        kernel_state& operator=(const kernel_state& other)
        {
            if (this != &other)
            {
                this->assign(other);
            }
            return *this;
        }

        std::atomic<uint64_t>& word(const size_t index) const
        {
            return this->slot_ ? this->slot_->words()[index] : this->local_[index];
        }

        bool is_shared() const
        {
            return this->slot_ != nullptr;
        }

        std::shared_ptr<kernel_slot> slot() const
        {
            return this->slot_;
        }

        // Moves the current values into a fresh slot of `arena`. False if the arena is full.
        bool promote(const std::shared_ptr<kernel_arena>& arena);
        // Binds to an existing slot (taking a reference); the local values are discarded.
        bool adopt(const std::shared_ptr<kernel_arena>& arena, uint32_t index);

      private:
        void assign(const kernel_state& other);

        mutable std::array<std::atomic<uint64_t>, kernel_arena::words_per_slot> local_{};
        std::shared_ptr<kernel_slot> slot_{};
    };
}
