#include "std_include.hpp"
#include "child_process_spawn.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
#define SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING 1
#include <poll.h>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/uio.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace sogen
{
    namespace
    {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
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
#else
        bool write_all(int, const void*, size_t)
        {
            return false;
        }

        bool read_all(int, void*, size_t)
        {
            return false;
        }
#endif

        // A frame may carry file descriptors (shared-memory section backings) out of band: they ride
        // as SCM_RIGHTS ancillary data on the length prefix, which the receiver always reads with
        // recvmsg so a descriptor can never be silently dropped by a plain read().
        constexpr size_t max_fds_per_frame = 128;

#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
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
#else
        bool write_length_with_fds(int, uint64_t, std::span<const int>)
        {
            return false;
        }

        bool read_length_with_fds(int, uint64_t&, std::vector<int>&)
        {
            return false;
        }
#endif

        void close_fds(const std::vector<int>& fds)
        {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
            for (const auto fd : fds)
            {
                ::close(fd);
            }
#else
            (void)fds;
#endif
        }

        bool send_framed(const int fd, const std::vector<std::byte>& payload, const std::span<const int> fds = {})
        {
            const uint64_t length = payload.size();
            if (fds.empty() ? !write_all(fd, &length, sizeof(length)) : !write_length_with_fds(fd, length, fds))
            {
                return false;
            }

            if (length == 0)
            {
                return true;
            }

            return write_all(fd, payload.data(), payload.size());
        }

        std::optional<std::vector<std::byte>> recv_framed(const int fd, const int timeout_ms,
                                                          std::vector<int>* const received_fds = nullptr)
        {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
            if (timeout_ms >= 0)
            {
                pollfd pfd{fd, POLLIN, 0};
                const auto rc = ::poll(&pfd, 1, timeout_ms);
                if (rc <= 0)
                {
                    return std::nullopt;
                }
            }
#else
            (void)fd;
            (void)timeout_ms;
#endif

            std::vector<int> fds{};
            uint64_t length = 0;
            if (!read_length_with_fds(fd, length, fds))
            {
                close_fds(fds);
                return std::nullopt;
            }

            constexpr uint64_t max_reasonable_length = 64ull << 20;
            std::vector<std::byte> buffer{};
            if (length <= max_reasonable_length)
            {
                buffer.resize(length);
            }

            if (length > max_reasonable_length || (length > 0 && !read_all(fd, buffer.data(), buffer.size())))
            {
                close_fds(fds);
                return std::nullopt;
            }

            if (received_fds)
            {
                received_fds->insert(received_fds->end(), fds.begin(), fds.end());
            }
            else
            {
                close_fds(fds);
            }

            return buffer;
        }

#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        void install_sigchld_reaper()
        {
            static std::once_flag flag{};
            std::call_once(flag, [] {
                struct sigaction action{};
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

        std::string to_lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) { return std::tolower(c); });
            return value;
        }

        std::string build_child_match_string(const application_settings& settings)
        {
            std::string result = settings.application.string();

            if (settings.command_line)
            {
                result += ' ';
                result += u16_to_u8(*settings.command_line);
            }
            else
            {
                for (const auto& argument : settings.arguments)
                {
                    result += ' ';
                    result += u16_to_u8(argument);
                }
            }

            return to_lower(result);
        }

        std::vector<std::string> build_child_argv(const child_process_spawn_config& config, const application_settings& settings,
                                                  const int ipc_fd, const int control_fd)
        {
            std::vector<std::string> argv{};
            argv.push_back(config.executable_path.string());

            if (!config.emulation_root.empty())
            {
                argv.emplace_back("-e");
                argv.push_back(config.emulation_root.string());
            }

            if (!config.registry_path.empty())
            {
                argv.emplace_back("-r");
                argv.push_back(config.registry_path.string());
            }

            for (const auto& [source, target] : config.path_mappings)
            {
                argv.emplace_back("-p");
                argv.push_back(source.string());
                argv.push_back(target.string());
            }

            argv.emplace_back("--vcpus");
            argv.push_back(std::to_string(config.vcpu_count));

            if (config.backend)
            {
                static const std::map<backend_type, std::string> backend_names{
                    {backend_type::unicorn, "unicorn"}, {backend_type::icicle, "icicle"}, {backend_type::whp, "whp"},
                    {backend_type::kvm, "kvm"},         {backend_type::fex, "fex"},
                };
                argv.emplace_back("--backend");
                argv.push_back(backend_names.at(*config.backend));
            }

            for (const auto& [title, text] : config.click_dialog_rules)
            {
                argv.emplace_back("--click-dialog-button");
                argv.push_back(title);
                argv.push_back(text);
            }

            if (config.silent)
            {
                argv.emplace_back("-s");
            }
            if (config.verbose_logging)
            {
                argv.emplace_back("-v");
            }
            if (config.buffer_stdout)
            {
                argv.emplace_back("-b");
            }
            if (config.concise_logging)
            {
                argv.emplace_back("-c");
            }
            if (config.skip_syscalls)
            {
                argv.emplace_back("--skip-syscalls");
            }
            if (config.reproducible)
            {
                argv.emplace_back("--reproducible");
            }
            if (config.disable_instruction_precision)
            {
                argv.emplace_back("--no-inst-precision");
            }

            argv.emplace_back("--whp-exec-hook");
            argv.push_back(config.whp_execution_hook_mode);

            if (!config.debug_child_pattern.empty())
            {
                argv.emplace_back("--debug-child");
                argv.push_back(config.debug_child_pattern);
                argv.emplace_back("--bind");
                argv.push_back(config.debug_host);
                argv.emplace_back("--port");
                argv.push_back(std::to_string(config.debug_port));

                if (build_child_match_string(settings).find(to_lower(config.debug_child_pattern)) != std::string::npos)
                {
                    argv.emplace_back("-d");
                }
            }

            argv.emplace_back("--child-ipc-fd");
            argv.push_back(std::to_string(ipc_fd));

            argv.emplace_back("--child-control-fd");
            argv.push_back(std::to_string(control_fd));

            return argv;
        }
#endif

#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        std::vector<std::byte> frame_message(const std::vector<std::byte>& payload)
        {
            const uint64_t length = payload.size();
            std::vector<std::byte> framed(sizeof(length) + payload.size());
            std::memcpy(framed.data(), &length, sizeof(length));
            if (!payload.empty())
            {
                std::memcpy(framed.data() + sizeof(length), payload.data(), payload.size());
            }

            return framed;
        }

        // windows_emulator::pump_pipe_ipc relays every message it receives from one peer straight
        // to every other peer, inline, on the same thread that's servicing the emulator's own
        // scheduling loop (see its own doc comment). A peer that hasn't drained its socket in a
        // while - because it's busy running guest code, or itself blocked writing back to us - can
        // leave that socket's kernel buffer full; a plain blocking write() into it then freezes
        // this whole host process (and with it, its vCPU) until the peer gets around to reading,
        // which, if both sides end up blocked on each other this way, is never. Keep the fd
        // non-blocking and queue whatever a write can't take right now, retried from here and from
        // try_receive - both are already polled continuously by the scheduler - instead of ever
        // blocking on the socket directly.
        class fd_pipe_ipc_channel final : public pipe_ipc_channel
        {
          public:
            explicit fd_pipe_ipc_channel(const int fd)
                : fd_(fd)
            {
                const auto flags = ::fcntl(this->fd_, F_GETFL, 0);
                if (flags >= 0)
                {
                    ::fcntl(this->fd_, F_SETFL, flags | O_NONBLOCK);
                }
            }

            fd_pipe_ipc_channel(const fd_pipe_ipc_channel&) = delete;
            fd_pipe_ipc_channel& operator=(const fd_pipe_ipc_channel&) = delete;

            ~fd_pipe_ipc_channel() override
            {
                if (this->fd_ >= 0)
                {
                    ::close(this->fd_);
                }
            }

            void send(const pipe_ipc_message& message) override
            {
                if (this->dead_)
                {
                    return;
                }

                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(message.type));
                buffer.write(message.pipe_name);
                buffer.write(message.data);
                buffer.write_optional(message.client_process_id);

                const auto framed = frame_message(buffer.get_buffer());
                this->pending_send_.insert(this->pending_send_.end(), framed.begin(), framed.end());
                this->flush_pending_send();
            }

            std::optional<pipe_ipc_message> try_receive() override
            {
                if (!this->dead_)
                {
                    this->flush_pending_send();
                    this->fill_recv_buffer();
                }

                constexpr size_t header_size = sizeof(uint64_t);
                if (this->recv_buffer_.size() < header_size)
                {
                    return std::nullopt;
                }

                uint64_t length = 0;
                std::memcpy(&length, this->recv_buffer_.data(), header_size);

                constexpr uint64_t max_reasonable_length = 64ull << 20;
                if (length > max_reasonable_length)
                {
                    this->recv_buffer_.clear();
                    return std::nullopt;
                }

                const auto frame_size = header_size + length;
                if (this->recv_buffer_.size() < frame_size)
                {
                    return std::nullopt;
                }

                const std::vector<std::byte> payload(this->recv_buffer_.begin() + static_cast<ptrdiff_t>(header_size),
                                                     this->recv_buffer_.begin() + static_cast<ptrdiff_t>(frame_size));
                this->recv_buffer_.erase(this->recv_buffer_.begin(), this->recv_buffer_.begin() + static_cast<ptrdiff_t>(frame_size));

                utils::buffer_deserializer deserializer{payload};

                pipe_ipc_message message{};
                uint8_t type{};
                deserializer.read(type);
                message.type = static_cast<pipe_ipc_message_type>(type);
                deserializer.read(message.pipe_name);
                deserializer.read(message.data);
                deserializer.read_optional(message.client_process_id);

                return message;
            }

          private:
            int fd_{-1};
            std::vector<std::byte> pending_send_{};
            std::vector<std::byte> recv_buffer_{};

            // Set once fill_recv_buffer/flush_pending_send observes the peer's end of the socketpair
            // is gone (EOF or a hard I/O error, as opposed to EAGAIN/EWOULDBLOCK, which just means "no
            // data/backlog right now"). A dead channel is never removed from pipe_ipc_peers_ (see that
            // member's own doc comment on the sibling-relay gap this doesn't attempt to close), but
            // send()/try_receive() short-circuit on it instead of re-issuing read()/write() against an
            // fd that can only ever report the same EOF/error again - without this, a channel whose
            // peer process already exited gets polled as fast as the scheduler's own loop runs,
            // forever, for the remaining lifetime of this process.
            bool dead_{false};
            bool send_error_logged_{false};
            bool recv_eof_or_error_logged_{false};

            void flush_pending_send()
            {
                while (!this->pending_send_.empty())
                {
                    const auto n = ::write(this->fd_, this->pending_send_.data(), this->pending_send_.size());
                    if (n > 0)
                    {
                        this->pending_send_.erase(this->pending_send_.begin(), this->pending_send_.begin() + n);
                        continue;
                    }

                    if (n < 0 && errno == EINTR)
                    {
                        continue;
                    }

                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        return;
                    }

                    if (!this->send_error_logged_ && std::getenv("SOGEN_TRACE_PIPE_RELAY_HEALTH"))
                    {
                        this->send_error_logged_ = true;
                        std::fprintf(stderr,
                                     "[pipe-relay-health-trace] pid=%d fd=%d flush_pending_send DROPPING %zu queued bytes (first "
                                     "occurrence for this channel): write() returned %zd errno=%d (%s)\n",
                                     ::getpid(), this->fd_, this->pending_send_.size(), n, errno, std::strerror(errno));
                    }

                    this->dead_ = true;
                    this->pending_send_.clear();
                    return;
                }
            }

            void fill_recv_buffer()
            {
                std::vector<std::byte> chunk(65536);
                while (true)
                {
                    const auto n = ::read(this->fd_, chunk.data(), chunk.size());
                    if (n > 0)
                    {
                        this->recv_buffer_.insert(this->recv_buffer_.end(), chunk.data(), chunk.data() + n);
                        if (static_cast<size_t>(n) == chunk.size())
                        {
                            continue;
                        }
                        return;
                    }

                    if (n < 0 && errno == EINTR)
                    {
                        continue;
                    }

                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        return;
                    }

                    this->dead_ = true;

                    if (!this->recv_eof_or_error_logged_ && std::getenv("SOGEN_TRACE_PIPE_RELAY_HEALTH"))
                    {
                        if (n == 0)
                        {
                            this->recv_eof_or_error_logged_ = true;
                            std::fprintf(stderr,
                                         "[pipe-relay-health-trace] pid=%d fd=%d fill_recv_buffer EOF (first occurrence for this "
                                         "channel) - peer closed its write end, %zu bytes still buffered unparsed\n",
                                         ::getpid(), this->fd_, this->recv_buffer_.size());
                        }
                        else
                        {
                            this->recv_eof_or_error_logged_ = true;
                            std::fprintf(stderr,
                                         "[pipe-relay-health-trace] pid=%d fd=%d fill_recv_buffer read() error (first occurrence for "
                                         "this channel) errno=%d (%s)\n",
                                         ::getpid(), this->fd_, errno, std::strerror(errno));
                        }
                    }

                    return;
                }
            }
        };
#endif

        enum class process_control_frame_kind : uint8_t
        {
            request = 0,
            response = 1,
            exit_notification = 2,
        };

        // The section backing is the one part of a control message that cannot be serialized: a shared
        // backing is represented by a flag plus its size in the body, with its descriptor appended to
        // `out_fds` for the transport to pass out of band.
        void write_backing_descriptor(utils::buffer_serializer& buffer, const std::shared_ptr<shared_backing>& backing,
                                      std::vector<int>& out_fds)
        {
            const auto shared = backing && backing->is_shared();
            buffer.write(shared);
            buffer.write(static_cast<uint64_t>(backing ? backing->size() : 0));
            if (shared)
            {
                out_fds.push_back(backing->native_fd());
            }
        }

        std::shared_ptr<shared_backing> read_backing_descriptor(utils::buffer_deserializer& buffer, std::vector<int>& in_fds)
        {
            bool shared{};
            uint64_t size{};
            buffer.read(shared);
            buffer.read(size);
            if (!shared || in_fds.empty())
            {
                return nullptr;
            }

            const auto fd = in_fds.front();
            in_fds.erase(in_fds.begin());
            return shared_backing::adopt_fd(fd, static_cast<size_t>(size));
        }

        void write_process_control_request_body(utils::buffer_serializer& buffer, const process_control_request& request,
                                                std::vector<int>& out_fds)
        {
            buffer.write(static_cast<uint8_t>(request.op));
            buffer.write(request.address);
            buffer.write(request.size);
            buffer.write(request.allocation_type);
            buffer.write(request.protection);
            buffer.write(request.free_type);
            buffer.write(request.info_class);
            buffer.write(request.exit_status);
            buffer.write(request.maximum_size);
            buffer.write(request.page_protection);
            buffer.write(request.allocation_attributes);
            buffer.write(request.granted_access);
            buffer.write_vector(request.payload);
            write_backing_descriptor(buffer, request.backing, out_fds);
        }

        void read_process_control_request_body(utils::buffer_deserializer& buffer, process_control_request& request,
                                               std::vector<int>& in_fds)
        {
            uint8_t op{};
            buffer.read(op);
            request.op = static_cast<process_control_op>(op);
            buffer.read(request.address);
            buffer.read(request.size);
            buffer.read(request.allocation_type);
            buffer.read(request.protection);
            buffer.read(request.free_type);
            buffer.read(request.info_class);
            buffer.read(request.exit_status);
            buffer.read(request.maximum_size);
            buffer.read(request.page_protection);
            buffer.read(request.allocation_attributes);
            buffer.read(request.granted_access);
            buffer.read_vector(request.payload);
            request.backing = read_backing_descriptor(buffer, in_fds);
        }

        void write_process_control_response_body(utils::buffer_serializer& buffer, const process_control_response& response,
                                                 std::vector<int>& out_fds)
        {
            buffer.write(response.status);
            buffer.write(response.bytes_written);
            buffer.write(response.base_address);
            buffer.write(response.region_size);
            buffer.write(response.old_protection);
            buffer.write(response.previous_suspend_count);
            buffer.write(response.minted_handle_bits);
            buffer.write(response.exported_object_type);
            buffer.write(response.allocation_type);
            buffer.write(response.size);
            buffer.write(response.maximum_size);
            buffer.write(response.page_protection);
            buffer.write(response.allocation_attributes);
            buffer.write(response.granted_access);
            buffer.write_vector(response.payload);
            write_backing_descriptor(buffer, response.backing, out_fds);
        }

        void read_process_control_response_body(utils::buffer_deserializer& buffer, process_control_response& response,
                                                std::vector<int>& in_fds)
        {
            buffer.read(response.status);
            buffer.read(response.bytes_written);
            buffer.read(response.base_address);
            buffer.read(response.region_size);
            buffer.read(response.old_protection);
            buffer.read(response.previous_suspend_count);
            buffer.read(response.minted_handle_bits);
            buffer.read(response.exported_object_type);
            buffer.read(response.allocation_type);
            buffer.read(response.size);
            buffer.read(response.maximum_size);
            buffer.read(response.page_protection);
            buffer.read(response.allocation_attributes);
            buffer.read(response.granted_access);
            buffer.read_vector(response.payload);
            response.backing = read_backing_descriptor(buffer, in_fds);
        }

#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        class fd_process_control_channel final : public process_control_channel
        {
          public:
            explicit fd_process_control_channel(const int fd, const int host_pid = -1)
                : fd_(fd),
                  host_pid_(host_pid)
            {
            }

            fd_process_control_channel(const fd_process_control_channel&) = delete;
            fd_process_control_channel& operator=(const fd_process_control_channel&) = delete;

            ~fd_process_control_channel() override
            {
                if (this->fd_ >= 0)
                {
                    ::close(this->fd_);
                }
            }

            std::optional<process_control_response> request(const process_control_request& request, const int timeout_ms) override
            {
                if (this->dead_)
                {
                    return std::nullopt;
                }

                const auto request_id = this->next_request_id_++;

                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(process_control_frame_kind::request));
                buffer.write(request_id);
                std::vector<int> out_fds{};
                write_process_control_request_body(buffer, request, out_fds);

                if (!send_framed(this->fd_, buffer.get_buffer(), out_fds))
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                std::vector<int> in_fds{};
                auto raw = recv_framed(this->fd_, timeout_ms, &in_fds);
                if (!raw)
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                utils::buffer_deserializer deserializer{*raw};

                uint8_t kind{};
                deserializer.read(kind);

                if (static_cast<process_control_frame_kind>(kind) == process_control_frame_kind::exit_notification)
                {
                    int32_t exit_status{};
                    deserializer.read(exit_status);
                    this->pending_exit_status_ = exit_status;
                    this->dead_ = true;
                    return std::nullopt;
                }

                uint64_t response_id{};
                deserializer.read(response_id);

                if (static_cast<process_control_frame_kind>(kind) != process_control_frame_kind::response || response_id != request_id)
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                process_control_response response{};
                response.request_id = response_id;
                read_process_control_response_body(deserializer, response, in_fds);
                close_fds(in_fds);

                return response;
            }

            std::optional<process_control_request> try_receive() override
            {
                std::vector<int> in_fds{};
                auto raw = recv_framed(this->fd_, 0, &in_fds);
                if (!raw)
                {
                    return std::nullopt;
                }

                utils::buffer_deserializer deserializer{*raw};

                uint8_t kind{};
                deserializer.read(kind);
                (void)kind;

                process_control_request request{};
                deserializer.read(request.request_id);
                read_process_control_request_body(deserializer, request, in_fds);
                close_fds(in_fds);

                return request;
            }

            void respond(const process_control_response& response) override
            {
                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(process_control_frame_kind::response));
                buffer.write(response.request_id);
                std::vector<int> out_fds{};
                write_process_control_response_body(buffer, response, out_fds);

                send_framed(this->fd_, buffer.get_buffer(), out_fds);
            }

            void notify_exit(const int32_t exit_status) override
            {
                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(process_control_frame_kind::exit_notification));
                buffer.write(exit_status);

                send_framed(this->fd_, buffer.get_buffer());
            }

            std::optional<int32_t> try_receive_exit_notification() override
            {
                if (this->pending_exit_status_.has_value())
                {
                    const auto exit_status = *this->pending_exit_status_;
                    this->pending_exit_status_.reset();
                    return exit_status;
                }

                if (this->dead_)
                {
                    return std::nullopt;
                }

                auto raw = recv_framed(this->fd_, 0);
                if (!raw)
                {
                    return std::nullopt;
                }

                utils::buffer_deserializer deserializer{*raw};

                uint8_t kind{};
                deserializer.read(kind);

                if (static_cast<process_control_frame_kind>(kind) != process_control_frame_kind::exit_notification)
                {
                    return std::nullopt;
                }

                int32_t exit_status{};
                deserializer.read(exit_status);
                return exit_status;
            }

            void force_kill() override
            {
                if (this->host_pid_ >= 0)
                {
                    ::kill(this->host_pid_, SIGKILL);
                }
            }

          private:
            int fd_{-1};
            int host_pid_{-1};
            uint64_t next_request_id_{1};
            bool dead_{false};
            std::optional<int32_t> pending_exit_status_{};
        };
#endif

        child_process_outcome parse_response(const std::vector<std::byte>& raw)
        {
            utils::buffer_deserializer buffer{raw};

            child_process_outcome outcome{};
            bool success{};
            buffer.read(success);
            outcome.success = success;

            if (success)
            {
                buffer.read(outcome.peb_address);
                buffer.read(outcome.process_parameters_address);
                buffer.read(outcome.peb32_address);
                buffer.read(outcome.process_params32_address);
            }
            else
            {
                outcome.failure_detail = buffer.read_string<char>();
            }

            return outcome;
        }
    }

    bool supports_child_process_spawning()
    {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        return true;
#else
        return false;
#endif
    }

    std::filesystem::path resolve_own_executable_path()
    {
#if defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);

        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            throw std::runtime_error("Failed to resolve own executable path");
        }

        while (!buffer.empty() && buffer.back() == '\0')
        {
            buffer.pop_back();
        }

        return std::filesystem::canonical(buffer);
#elif defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        return std::filesystem::canonical("/proc/self/exe");
#else
        throw std::runtime_error("Child process spawning is not supported on this platform");
#endif
    }

    child_process_outcome spawn_child_process(const child_process_spawn_config& config, application_settings settings,
                                              std::vector<inherited_pipe_handle> inherited_pipes,
                                              std::vector<inherited_section_handle> inherited_sections,
                                              std::vector<inherited_event_handle> inherited_events)
    {
#if !defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        (void)config;
        (void)settings;
        (void)inherited_pipes;
        (void)inherited_sections;
        (void)inherited_events;
        return {.success = false, .failure_detail = "Child process spawning is not supported on this platform"};
#else
        install_sigchld_reaper();

        int fds[2]{-1, -1};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        {
            return {.success = false, .failure_detail = std::string("socketpair() failed: ") + std::strerror(errno)};
        }

        const auto parent_fd = fds[0];
        const auto child_fd = fds[1];

        int control_fds[2]{-1, -1};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, control_fds) != 0)
        {
            const auto error = std::string("socketpair() failed: ") + std::strerror(errno);
            ::close(parent_fd);
            ::close(child_fd);
            return {.success = false, .failure_detail = error};
        }

        const auto parent_control_fd = control_fds[0];
        const auto child_control_fd = control_fds[1];

        const auto argv_strings = build_child_argv(config, settings, child_fd, child_control_fd);
        std::vector<char*> argv{};
        argv.reserve(argv_strings.size() + 1);
        for (const auto& arg : argv_strings)
        {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        const auto executable_path = config.executable_path.string();

        const auto pid = ::fork();
        if (pid < 0)
        {
            const auto error = std::string("fork() failed: ") + std::strerror(errno);
            ::close(parent_fd);
            ::close(child_fd);
            ::close(parent_control_fd);
            ::close(child_control_fd);
            return {.success = false, .failure_detail = error};
        }

        if (pid == 0)
        {
            ::close(parent_fd);
            ::close(parent_control_fd);
            ::execv(executable_path.c_str(), argv.data());
            _exit(127);
        }

        ::close(child_fd);
        ::close(child_control_fd);

        if (!send_child_bootstrap_data(parent_fd, settings, inherited_pipes, std::move(inherited_sections), inherited_events))
        {
            ::close(parent_fd);
            ::close(parent_control_fd);
            return {.success = false, .failure_detail = "Failed to send bootstrap data to child"};
        }

        constexpr int ready_timeout_ms = 30000;
        auto response = recv_framed(parent_fd, ready_timeout_ms);

        if (!response)
        {
            ::close(parent_fd);
            ::close(parent_control_fd);
            return {.success = false,
                    .failure_detail = "Child did not report readiness within " + std::to_string(ready_timeout_ms) +
                                      "ms (or exited/closed the IPC channel early)"};
        }

        auto outcome = parse_response(*response);
        if (outcome.success)
        {
            outcome.ipc_fd = parent_fd;
            outcome.control_fd = parent_control_fd;
            outcome.host_pid = static_cast<int>(pid);
        }
        else
        {
            ::close(parent_fd);
            ::close(parent_control_fd);
        }

        return outcome;
#endif
    }

    bool send_child_bootstrap_data(const int fd, const application_settings& settings,
                                   const std::vector<inherited_pipe_handle>& inherited_pipes,
                                   std::vector<inherited_section_handle> inherited_sections,
                                   const std::vector<inherited_event_handle>& inherited_events)
    {
        std::vector<int> section_fds{};
        for (auto& section : inherited_sections)
        {
            if (!section.backing || !section.backing->is_shared())
            {
                continue;
            }

            if (section_fds.size() >= max_fds_per_frame)
            {
                section.backing = shared_backing::create_from_content(section.backing->content());
                continue;
            }

            section_fds.push_back(section.backing->native_fd());
        }

        utils::buffer_serializer bootstrap{};
        bootstrap.write(settings);
        bootstrap.write_vector(inherited_pipes);
        bootstrap.write_vector(inherited_sections);
        bootstrap.write_vector(inherited_events);

        return send_framed(fd, bootstrap.get_buffer(), section_fds);
    }

    std::optional<child_bootstrap_data> receive_child_bootstrap_data(const int fd)
    {
        std::vector<int> section_fds{};
        auto raw = recv_framed(fd, -1, &section_fds);
        if (!raw)
        {
            return std::nullopt;
        }

        utils::buffer_deserializer buffer{*raw};

        child_bootstrap_data data{};
        buffer.read(data.settings);
        buffer.read_vector(data.inherited_pipes);
        buffer.read_vector(data.inherited_sections);
        buffer.read_vector(data.inherited_events);

        size_t next_fd = 0;
        for (auto& section : data.inherited_sections)
        {
            if (section.wire_backing_is_shared && next_fd < section_fds.size())
            {
                section.backing = shared_backing::adopt_fd(section_fds[next_fd++], static_cast<size_t>(section.wire_backing_size));
            }
        }
        close_fds(std::vector<int>(section_fds.begin() + static_cast<std::ptrdiff_t>(next_fd), section_fds.end()));

        return data;
    }

    void send_child_ready(const int fd, const uint64_t peb_address, const uint64_t process_parameters_address, const uint64_t peb32_address,
                          const uint64_t process_params32_address)
    {
        utils::buffer_serializer buffer{};
        buffer.write(true);
        buffer.write(peb_address);
        buffer.write(process_parameters_address);
        buffer.write(peb32_address);
        buffer.write(process_params32_address);
        send_framed(fd, buffer.get_buffer());
    }

    void send_child_failed(const int fd, const std::string& detail)
    {
        utils::buffer_serializer buffer{};
        buffer.write(false);
        buffer.write_string(std::string_view(detail));
        send_framed(fd, buffer.get_buffer());
    }

    std::unique_ptr<pipe_ipc_channel> create_fd_pipe_ipc_channel(const int fd)
    {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        return std::make_unique<fd_pipe_ipc_channel>(fd);
#else
        (void)fd;
        return nullptr;
#endif
    }

    std::unique_ptr<process_control_channel> create_fd_process_control_channel(const int fd, const int host_pid)
    {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        return std::make_unique<fd_process_control_channel>(fd, host_pid);
#else
        (void)fd;
        (void)host_pid;
        return nullptr;
#endif
    }

} // namespace sogen
