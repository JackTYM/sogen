#include "std_include.hpp"
#include "child_process_spawn.hpp"

#include <cerrno>
#include <cstring>

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
#define SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING 1
#include <poll.h>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>

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

        bool send_framed(const int fd, const std::vector<std::byte>& payload)
        {
            const uint64_t length = payload.size();
            if (!write_all(fd, &length, sizeof(length)))
            {
                return false;
            }

            if (length == 0)
            {
                return true;
            }

            return write_all(fd, payload.data(), payload.size());
        }

        std::optional<std::vector<std::byte>> recv_framed(const int fd, const int timeout_ms)
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

            uint64_t length = 0;
            if (!read_all(fd, &length, sizeof(length)))
            {
                return std::nullopt;
            }

            constexpr uint64_t max_reasonable_length = 64ull << 20;
            if (length > max_reasonable_length)
            {
                return std::nullopt;
            }

            std::vector<std::byte> buffer(length);
            if (length > 0 && !read_all(fd, buffer.data(), buffer.size()))
            {
                return std::nullopt;
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

        std::vector<std::string> build_child_argv(const child_process_spawn_config& config, const int ipc_fd, const int control_fd)
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

            argv.emplace_back("--child-ipc-fd");
            argv.push_back(std::to_string(ipc_fd));

            argv.emplace_back("--child-control-fd");
            argv.push_back(std::to_string(control_fd));

            return argv;
        }
#endif

#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        class fd_pipe_ipc_channel final : public pipe_ipc_channel
        {
          public:
            explicit fd_pipe_ipc_channel(const int fd)
                : fd_(fd)
            {
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
                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(message.type));
                buffer.write(message.pipe_name);
                buffer.write(message.data);
                send_framed(this->fd_, buffer.get_buffer());
            }

            std::optional<pipe_ipc_message> try_receive() override
            {
                auto raw = recv_framed(this->fd_, 0);
                if (!raw)
                {
                    return std::nullopt;
                }

                utils::buffer_deserializer deserializer{*raw};

                pipe_ipc_message message{};
                uint8_t type{};
                deserializer.read(type);
                message.type = static_cast<pipe_ipc_message_type>(type);
                deserializer.read(message.pipe_name);
                deserializer.read(message.data);

                return message;
            }

          private:
            int fd_{-1};
        };
#endif

        enum class process_control_frame_kind : uint8_t
        {
            request = 0,
            response = 1,
        };

        void write_process_control_request_body(utils::buffer_serializer& buffer, const process_control_request& request)
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
        }

        void read_process_control_request_body(utils::buffer_deserializer& buffer, process_control_request& request)
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
        }

        void write_process_control_response_body(utils::buffer_serializer& buffer, const process_control_response& response)
        {
            buffer.write(response.status);
            buffer.write(response.bytes_written);
            buffer.write(response.base_address);
            buffer.write(response.region_size);
            buffer.write(response.old_protection);
            buffer.write(response.previous_suspend_count);
            buffer.write(response.minted_handle_bits);
            buffer.write_vector(response.payload);
        }

        void read_process_control_response_body(utils::buffer_deserializer& buffer, process_control_response& response)
        {
            buffer.read(response.status);
            buffer.read(response.bytes_written);
            buffer.read(response.base_address);
            buffer.read(response.region_size);
            buffer.read(response.old_protection);
            buffer.read(response.previous_suspend_count);
            buffer.read(response.minted_handle_bits);
            buffer.read_vector(response.payload);
        }

#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        class fd_process_control_channel final : public process_control_channel
        {
          public:
            explicit fd_process_control_channel(const int fd)
                : fd_(fd)
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
                write_process_control_request_body(buffer, request);

                if (!send_framed(this->fd_, buffer.get_buffer()))
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                auto raw = recv_framed(this->fd_, timeout_ms);
                if (!raw)
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                utils::buffer_deserializer deserializer{*raw};

                uint8_t kind{};
                deserializer.read(kind);

                uint64_t response_id{};
                deserializer.read(response_id);

                if (static_cast<process_control_frame_kind>(kind) != process_control_frame_kind::response || response_id != request_id)
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                process_control_response response{};
                response.request_id = response_id;
                read_process_control_response_body(deserializer, response);

                return response;
            }

            std::optional<process_control_request> try_receive() override
            {
                auto raw = recv_framed(this->fd_, 0);
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
                read_process_control_request_body(deserializer, request);

                return request;
            }

            void respond(const process_control_response& response) override
            {
                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(process_control_frame_kind::response));
                buffer.write(response.request_id);
                write_process_control_response_body(buffer, response);

                send_framed(this->fd_, buffer.get_buffer());
            }

          private:
            int fd_{-1};
            uint64_t next_request_id_{1};
            bool dead_{false};
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

        const auto argv_strings = build_child_argv(config, child_fd, child_control_fd);
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

        utils::buffer_serializer bootstrap{};
        bootstrap.write(settings);
        bootstrap.write_vector(inherited_pipes);
        bootstrap.write_vector(inherited_sections);
        bootstrap.write_vector(inherited_events);

        if (!send_framed(parent_fd, bootstrap.get_buffer()))
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
        }
        else
        {
            ::close(parent_fd);
            ::close(parent_control_fd);
        }

        return outcome;
#endif
    }

    std::optional<child_bootstrap_data> receive_child_bootstrap_data(const int fd)
    {
        auto raw = recv_framed(fd, -1);
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

        return data;
    }

    void send_child_ready(const int fd, const uint64_t peb_address, const uint64_t process_parameters_address)
    {
        utils::buffer_serializer buffer{};
        buffer.write(true);
        buffer.write(peb_address);
        buffer.write(process_parameters_address);
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

    std::unique_ptr<process_control_channel> create_fd_process_control_channel(const int fd)
    {
#if defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        return std::make_unique<fd_process_control_channel>(fd);
#else
        (void)fd;
        return nullptr;
#endif
    }

} // namespace sogen
