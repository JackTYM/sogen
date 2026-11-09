#include "std_include.hpp"
#include "shared_backing.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
#define SOGEN_SHARED_BACKING_POSIX 1
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
// Windows hosts: pagefile-backed file mappings (utils::host_shm); native_fd() carries the mapping handle.
#define SOGEN_SHARED_BACKING_WIN32 1
#include <utils/host_shared_memory.hpp>
#endif

namespace sogen
{
    namespace
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        size_t host_page_size()
        {
            static const auto size = static_cast<size_t>(::sysconf(_SC_PAGESIZE));
            return size;
        }

        // Every shareable section keeps its descriptor open for as long as the backing lives (it may be
        // sent to another process at any later point), and macOS defaults to only 256 descriptors.
        void raise_descriptor_limit_once()
        {
            static std::once_flag once;
            std::call_once(once, [] {
                rlimit limit{};
                if (::getrlimit(RLIMIT_NOFILE, &limit) != 0)
                {
                    return;
                }

                rlim_t wanted = limit.rlim_max;
#if defined(__APPLE__)
                wanted = std::min<rlim_t>(wanted, 10240);
#endif
                if (wanted > limit.rlim_cur)
                {
                    limit.rlim_cur = wanted;
                    ::setrlimit(RLIMIT_NOFILE, &limit);
                }
            });
        }

        int create_anonymous_shm(const size_t size)
        {
            raise_descriptor_limit_once();

            static std::atomic<uint64_t> counter{0};
            for (int attempt = 0; attempt < 8; ++attempt)
            {
                // macOS limits POSIX shm names to 31 characters.
                std::array<char, 32> name{};
                std::snprintf(name.data(), name.size(), "/sg.%x.%llx", static_cast<unsigned>(::getpid()),
                              static_cast<unsigned long long>(counter.fetch_add(1)));

                const int fd = ::shm_open(name.data(), O_CREAT | O_EXCL | O_RDWR, 0600);
                if (fd < 0)
                {
                    if (errno == EEXIST)
                    {
                        continue;
                    }
                    return -1;
                }

                ::shm_unlink(name.data());
                ::fcntl(fd, F_SETFD, FD_CLOEXEC);

                // macOS only allows sizing a shm object once, so round up front.
                if (::ftruncate(fd, static_cast<off_t>(size)) != 0)
                {
                    ::close(fd);
                    return -1;
                }

                return fd;
            }

            return -1;
        }
#endif

#if defined(SOGEN_SHARED_BACKING_WIN32)
        constexpr size_t host_page_size = 0x1000;

        size_t round_to_page(const size_t size)
        {
            return (size + host_page_size - 1) & ~(host_page_size - 1);
        }
#endif
    }

    bool shared_backing::supports_sharing()
    {
#if defined(SOGEN_SHARED_BACKING_POSIX) || defined(SOGEN_SHARED_BACKING_WIN32)
        return true;
#else
        return false;
#endif
    }

    std::shared_ptr<shared_backing> shared_backing::create_heap(const size_t size)
    {
        std::shared_ptr<shared_backing> backing{new shared_backing{}};
        backing->size_ = size;
        backing->mapped_size_ = size;
        if (size != 0)
        {
            backing->heap_ = utils::shared_host_buffer{size};
            backing->data_ = reinterpret_cast<std::byte*>(backing->heap_.data());
            backing->mapped_size_ = backing->heap_.size();
        }
        return backing;
    }

    std::shared_ptr<shared_backing> shared_backing::create(const size_t size)
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        if (size != 0)
        {
            const auto mapped_size = (size + host_page_size() - 1) & ~(host_page_size() - 1);
            const int fd = create_anonymous_shm(mapped_size);
            if (fd >= 0)
            {
                void* const mapping = ::mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                if (mapping != MAP_FAILED)
                {
                    std::shared_ptr<shared_backing> backing{new shared_backing{}};
                    backing->data_ = static_cast<std::byte*>(mapping);
                    backing->size_ = size;
                    backing->mapped_size_ = mapped_size;
                    backing->fd_ = fd;
                    return backing;
                }

                ::close(fd);
            }
        }
#elif defined(SOGEN_SHARED_BACKING_WIN32)
        if (size != 0)
        {
            const int handle = utils::host_shm::create_anonymous(round_to_page(size));
            if (handle >= 0)
            {
                if (auto backing = adopt_fd(handle, size))
                {
                    return backing;
                }
            }
        }
#endif

        return create_heap(size);
    }

    std::shared_ptr<shared_backing> shared_backing::create_named(const size_t size, std::string& shm_name)
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        if (size == 0)
        {
            return nullptr;
        }

        static std::atomic<uint64_t> counter{0};
        const auto mapped_size = (size + host_page_size() - 1) & ~(host_page_size() - 1);

        for (int attempt = 0; attempt < 8; ++attempt)
        {
            std::array<char, 24> name{};
            std::snprintf(name.data(), name.size(), "/sgn.%x.%llx", static_cast<unsigned>(::getpid()),
                          static_cast<unsigned long long>(counter.fetch_add(1)));

            const int fd = ::shm_open(name.data(), O_CREAT | O_EXCL | O_RDWR, 0600);
            if (fd < 0)
            {
                if (errno == EEXIST)
                {
                    continue;
                }
                return nullptr;
            }

            ::fcntl(fd, F_SETFD, FD_CLOEXEC);
            if (::ftruncate(fd, static_cast<off_t>(mapped_size)) != 0)
            {
                ::close(fd);
                ::shm_unlink(name.data());
                return nullptr;
            }

            shm_name = name.data();
            auto backing = adopt_fd(fd, size);
            if (!backing)
            {
                ::shm_unlink(name.data());
            }
            return backing;
        }
#elif defined(SOGEN_SHARED_BACKING_WIN32)
        // A Windows mapping name lives exactly as long as some handle to the object is open (no unlink step), so
        // unlink_named has nothing to do; the creator's backing keeps the name alive like the POSIX object.
        const int handle = utils::host_shm::create_unique_named(round_to_page(size), shm_name);
        if (handle < 0)
        {
            return nullptr;
        }

        return adopt_fd(handle, size);
#else
        (void)size;
        (void)shm_name;
#endif
        return nullptr;
    }

    std::shared_ptr<shared_backing> shared_backing::open_named(const std::string& shm_name, const size_t size)
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        const int fd = ::shm_open(shm_name.c_str(), O_RDWR, 0600);
        if (fd < 0)
        {
            return nullptr;
        }

        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        return adopt_fd(fd, size);
#elif defined(SOGEN_SHARED_BACKING_WIN32)
        const int handle = utils::host_shm::open_named(shm_name);
        if (handle < 0)
        {
            return nullptr;
        }

        return adopt_fd(handle, size);
#else
        (void)shm_name;
        (void)size;
        return nullptr;
#endif
    }

    void shared_backing::unlink_named(const std::string& shm_name)
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        ::shm_unlink(shm_name.c_str());
#else
        (void)shm_name;
#endif
    }

    std::shared_ptr<shared_backing> shared_backing::adopt_fd(const int fd, const size_t size)
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        const auto mapped_size = (size + host_page_size() - 1) & ~(host_page_size() - 1);
        void* const mapping = size == 0 ? MAP_FAILED : ::mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapping == MAP_FAILED)
        {
            ::close(fd);
            return nullptr;
        }

        std::shared_ptr<shared_backing> backing{new shared_backing{}};
        backing->data_ = static_cast<std::byte*>(mapping);
        backing->size_ = size;
        backing->mapped_size_ = mapped_size;
        backing->fd_ = fd;
        return backing;
#elif defined(SOGEN_SHARED_BACKING_WIN32)
        const auto mapped_size = round_to_page(size);
        void* const mapping = utils::host_shm::map(fd, mapped_size);
        if (!mapping)
        {
            utils::host_shm::close(fd);
            return nullptr;
        }

        std::shared_ptr<shared_backing> backing{new shared_backing{}};
        backing->data_ = static_cast<std::byte*>(mapping);
        backing->size_ = size;
        backing->mapped_size_ = mapped_size;
        backing->fd_ = fd;
        return backing;
#else
        (void)fd;
        (void)size;
        return nullptr;
#endif
    }

    std::shared_ptr<shared_backing> shared_backing::create_from_content(const std::vector<std::byte>& content)
    {
        auto backing = create_heap(content.size());
        if (!content.empty())
        {
            std::memcpy(backing->data(), content.data(), content.size());
        }
        return backing;
    }

    shared_backing::~shared_backing()
    {
#if defined(SOGEN_SHARED_BACKING_POSIX)
        if (this->fd_ >= 0)
        {
            ::munmap(this->data_, this->mapped_size_);
            ::close(this->fd_);
        }
#elif defined(SOGEN_SHARED_BACKING_WIN32)
        if (this->fd_ >= 0)
        {
            utils::host_shm::unmap(this->data_);
            utils::host_shm::close(this->fd_);
        }
#endif
    }

    std::vector<std::byte> shared_backing::content() const
    {
        return {this->data_, this->data_ + this->size_};
    }
}
