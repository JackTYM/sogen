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

        std::vector<std::string> build_child_argv(const child_process_spawn_config& config, const int ipc_fd)
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

            return argv;
        }
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
                                              std::vector<inherited_section_handle> inherited_sections)
    {
#if !defined(SOGEN_SUPPORTS_CHILD_PROCESS_SPAWNING)
        (void)config;
        (void)settings;
        (void)inherited_pipes;
        (void)inherited_sections;
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

        const auto argv_strings = build_child_argv(config, child_fd);
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
            return {.success = false, .failure_detail = error};
        }

        if (pid == 0)
        {
            ::close(parent_fd);
            ::execv(executable_path.c_str(), argv.data());
            _exit(127);
        }

        ::close(child_fd);

        utils::buffer_serializer bootstrap{};
        bootstrap.write(settings);
        bootstrap.write_vector(inherited_pipes);
        bootstrap.write_vector(inherited_sections);

        if (!send_framed(parent_fd, bootstrap.get_buffer()))
        {
            ::close(parent_fd);
            return {.success = false, .failure_detail = "Failed to send bootstrap data to child"};
        }

        constexpr int ready_timeout_ms = 30000;
        auto response = recv_framed(parent_fd, ready_timeout_ms);
        ::close(parent_fd);

        if (!response)
        {
            return {.success = false,
                    .failure_detail = "Child did not report readiness within " + std::to_string(ready_timeout_ms) +
                                      "ms (or exited/closed the IPC channel early)"};
        }

        return parse_response(*response);
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

} // namespace sogen
