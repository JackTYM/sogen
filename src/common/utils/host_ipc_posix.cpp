#include "host_ipc.hpp"

// POSIX transport for the analyzer's multi-process machinery. The bodies are the ones child_process_spawn.cpp
// used before the host layer was split out; behaviour is unchanged.

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>

#include <poll.h>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/uio.h>

namespace sogen::host_ipc
{
    bool supported()
    {
        return true;
    }

    bool write_all(const int fd, const void* data, size_t size)
    {
        const auto* p = static_cast<const std::byte*>(data);
        while (size > 0)
        {
            const auto n = ::write(fd, p, size);
            if (n < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }

            if (n == 0)
            {
                return false;
            }

            p += n;
            size -= static_cast<size_t>(n);
        }

        return true;
    }

    bool read_all(const int fd, void* data, size_t size)
    {
        auto* p = static_cast<std::byte*>(data);
        while (size > 0)
        {
            const auto n = ::read(fd, p, size);
            if (n < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }

            if (n == 0)
            {
                return false;
            }

            p += n;
            size -= static_cast<size_t>(n);
        }

        return true;
    }

    // A frame may carry file descriptors (shared-memory section backings) out of band: they ride
    // as SCM_RIGHTS ancillary data on the length prefix, which the receiver always reads with
    // recvmsg so a descriptor can never be silently dropped by a plain read().
    bool write_length_with_fds(const int fd, const uint64_t length, const std::span<const int> fds)
    {
        iovec iov{const_cast<uint64_t*>(&length), sizeof(length)};
        alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * max_fds_per_frame)> control{};

        msghdr message{};
        message.msg_iov = &iov;
        message.msg_iovlen = 1;
        message.msg_control = control.data();
        message.msg_controllen = CMSG_SPACE(sizeof(int) * fds.size());

        auto* const header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
        std::memcpy(CMSG_DATA(header), fds.data(), sizeof(int) * fds.size());

        while (true)
        {
            const auto sent = ::sendmsg(fd, &message, 0);
            if (sent < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }

            const auto remaining = sizeof(length) - static_cast<size_t>(sent);
            return remaining == 0 || write_all(fd, reinterpret_cast<const std::byte*>(&length) + sent, remaining);
        }
    }

    bool read_length_with_fds(const int fd, uint64_t& length, std::vector<int>& received_fds)
    {
        size_t received = 0;
        while (received < sizeof(length))
        {
            iovec iov{reinterpret_cast<std::byte*>(&length) + received, sizeof(length) - received};
            alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * max_fds_per_frame)> control{};

            msghdr message{};
            message.msg_iov = &iov;
            message.msg_iovlen = 1;
            message.msg_control = control.data();
            message.msg_controllen = control.size();

            const auto n = ::recvmsg(fd, &message, 0);
            if (n < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }

            if (n == 0)
            {
                return false;
            }

            for (auto* header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header))
            {
                if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS)
                {
                    continue;
                }

                const auto count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                for (size_t i = 0; i < count; ++i)
                {
                    int received_fd{};
                    std::memcpy(&received_fd, CMSG_DATA(header) + i * sizeof(int), sizeof(int));
                    ::fcntl(received_fd, F_SETFD, FD_CLOEXEC);
                    received_fds.push_back(received_fd);
                }
            }

            received += static_cast<size_t>(n);
        }

        return true;
    }

    void close_fd(const int fd)
    {
        ::close(fd);
    }

    bool wait_readable(const int fd, const int timeout_ms)
    {
        if (timeout_ms < 0)
        {
            return true;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (true)
        {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            pollfd pfd{fd, POLLIN, 0};
            const auto rc = ::poll(&pfd, 1, static_cast<int>(std::max<decltype(remaining)>(remaining, 0)));
            if (rc > 0)
            {
                return true;
            }

            if (rc < 0 && errno == EINTR)
            {
                continue;
            }

            return false;
        }
    }

    void set_nonblocking(const int fd)
    {
        const auto flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
        {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    io_result write_some(const int fd, const void* data, const size_t size)
    {
        while (true)
        {
            const auto n = ::write(fd, data, size);
            if (n > 0)
            {
                return {.status = io_status::done, .bytes = static_cast<size_t>(n), .raw = n};
            }

            if (n < 0 && errno == EINTR)
            {
                continue;
            }

            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                return {.status = io_status::would_block, .raw = n, .error_code = errno};
            }

            return {.status = io_status::error, .raw = n, .error_code = errno};
        }
    }

    io_result read_some(const int fd, void* data, const size_t size)
    {
        while (true)
        {
            const auto n = ::read(fd, data, size);
            if (n > 0)
            {
                return {.status = io_status::done, .bytes = static_cast<size_t>(n), .raw = n};
            }

            if (n < 0 && errno == EINTR)
            {
                continue;
            }

            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                return {.status = io_status::would_block, .raw = n, .error_code = errno};
            }

            return {.status = n == 0 ? io_status::eof : io_status::error, .raw = n, .error_code = errno};
        }
    }

    std::string describe_error(const int error_code)
    {
        return std::strerror(error_code);
    }

    void prepare_child_spawning()
    {
        static std::once_flag flag{};
        std::call_once(flag, [] {
            struct sigaction action
            {
            };
            action.sa_handler = [](int) {
                int status = 0;
                while (::waitpid(-1, &status, WNOHANG) > 0)
                {
                }
            };
            sigemptyset(&action.sa_mask);
            action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
            ::sigaction(SIGCHLD, &action, nullptr);
        });
    }

    std::optional<channel_pair> create_channel_pair(std::string& error)
    {
        int fds[2]{-1, -1};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        {
            error = std::string("socketpair() failed: ") + std::strerror(errno);
            return std::nullopt;
        }

        return channel_pair{.parent_end = fds[0], .child_end = fds[1]};
    }

    spawn_result spawn_process(const std::filesystem::path& executable, const std::vector<std::string>& argv_strings,
                               const std::span<const int> child_fds, const std::span<const int> parent_only_fds)
    {
        (void)child_fds; // socketpair descriptors are not close-on-exec, so execv keeps them

        std::vector<char*> argv{};
        argv.reserve(argv_strings.size() + 1);
        for (const auto& arg : argv_strings)
        {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        const auto executable_path = executable.string();

        const auto pid = ::fork();
        if (pid < 0)
        {
            return {.error = std::string("fork() failed: ") + std::strerror(errno)};
        }

        if (pid == 0)
        {
            for (const auto fd : parent_only_fds)
            {
                ::close(fd);
            }
            ::execv(executable_path.c_str(), argv.data());
            _exit(127);
        }

        return {.pid = static_cast<int>(pid)};
    }

    void bind_channel_peer(int, int)
    {
    }

    bool process_alive(const int pid)
    {
        return ::kill(pid, 0) == 0;
    }

    void kill_process(const int pid)
    {
        ::kill(pid, SIGKILL);
    }

    int current_process_id()
    {
        return static_cast<int>(::getpid());
    }
}

#elif defined(__EMSCRIPTEN__)

namespace sogen::host_ipc
{
    bool supported()
    {
        return false;
    }

    bool write_all(int, const void*, size_t)
    {
        return false;
    }

    bool read_all(int, void*, size_t)
    {
        return false;
    }

    bool write_length_with_fds(int, uint64_t, std::span<const int>)
    {
        return false;
    }

    bool read_length_with_fds(int, uint64_t&, std::vector<int>&)
    {
        return false;
    }

    void close_fd(int)
    {
    }

    bool wait_readable(int, int)
    {
        return true;
    }

    void set_nonblocking(int)
    {
    }

    io_result write_some(int, const void*, size_t)
    {
        return {};
    }

    io_result read_some(int, void*, size_t)
    {
        return {};
    }

    std::string describe_error(int)
    {
        return {};
    }

    void prepare_child_spawning()
    {
    }

    std::optional<channel_pair> create_channel_pair(std::string& error)
    {
        error = "Child process spawning is not supported on this platform";
        return std::nullopt;
    }

    spawn_result spawn_process(const std::filesystem::path&, const std::vector<std::string>&, std::span<const int>, std::span<const int>)
    {
        return {.error = "Child process spawning is not supported on this platform"};
    }

    void bind_channel_peer(int, int)
    {
    }

    bool process_alive(int)
    {
        return false;
    }

    void kill_process(int)
    {
    }

    int current_process_id()
    {
        return 0;
    }
}

#endif
