#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sogen
{
    // Host-owned memory backing a guest-visible kernel object (pagefile-backed section, shared
    // synchronization state, ...). On POSIX hosts it is a real MAP_SHARED mapping of an anonymous
    // shared-memory object, whose file descriptor can be passed to another sogen host process
    // (SCM_RIGHTS) so both processes map the very same physical pages - matching real Windows section
    // semantics. Where that is unavailable (Windows/Emscripten hosts, or shm_open failing) it falls
    // back to a plain private heap buffer, which callers can still ship across a process boundary as a
    // one-time content snapshot (content()/create_from_content()).
    class shared_backing
    {
      public:
        // Prefers a real shared mapping, falls back to heap. The bytes start zeroed.
        static std::shared_ptr<shared_backing> create(size_t size);
        static std::shared_ptr<shared_backing> create_heap(size_t size);
        // Maps a shared-memory file descriptor received from another process. Takes ownership of fd
        // (closed on failure and on destruction). Returns nullptr if the mapping fails.
        static std::shared_ptr<shared_backing> adopt_fd(int fd, size_t size);
        // Heap backing initialised from a snapshot (the cross-process fallback path).
        static std::shared_ptr<shared_backing> create_from_content(const std::vector<std::byte>& content);

        // A backing addressable by name instead of by passed descriptor: `shm_name` receives a POSIX shm name
        // any process can open_named(). The object is not unlinked here; whoever owns the name is
        // responsible for shm_unlink (see kernel_arena::set_shm_backing). Returns nullptr without shared
        // memory support.
        static std::shared_ptr<shared_backing> create_named(size_t size, std::string& shm_name);
        static std::shared_ptr<shared_backing> open_named(const std::string& shm_name, size_t size);

        static bool supports_sharing();

        ~shared_backing();

        shared_backing(const shared_backing&) = delete;
        shared_backing& operator=(const shared_backing&) = delete;

        std::byte* data() const
        {
            return this->data_;
        }

        // Logical size requested by the creator. The underlying mapping is rounded up to the host page
        // size, so data()[size() .. mapped_size()) is valid, zeroed padding.
        size_t size() const
        {
            return this->size_;
        }

        // File descriptor to hand to another process, or -1 for a heap backing (which has no way to be
        // shared and must be snapshotted instead).
        int native_fd() const
        {
            return this->fd_;
        }

        bool is_shared() const
        {
            return this->fd_ >= 0;
        }

        std::vector<std::byte> content() const;

      private:
        shared_backing() = default;

        std::byte* data_{};
        size_t size_{};
        size_t mapped_size_{};
        int fd_{-1};
        std::vector<std::byte> heap_{};
    };
}
