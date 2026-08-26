#pragma once

#include <windows_emulator.hpp>
#include <backend_selection.hpp>
#include <process_control_channel.hpp>

namespace sogen
{
    // Deliberately narrower than analysis_options (which is private to main.cpp) - only the fields
    // that affect a spawned child's own emulation correctness or this investigation's own
    // dialog-automation harness need to survive the real process boundary as reconstructed CLI
    // flags. Everything else (application/command line/working directory/environment) travels over
    // the IPC channel as a real application_settings instead.
    struct child_process_spawn_config
    {
        std::filesystem::path executable_path{};
        std::filesystem::path emulation_root{};
        std::filesystem::path registry_path{};
        std::vector<std::pair<windows_path, std::filesystem::path>> path_mappings{};
        uint32_t vcpu_count{1};
        std::optional<backend_type> backend{};
        std::vector<std::pair<std::string, std::string>> click_dialog_rules{};
        bool silent{};
        bool verbose_logging{};
        bool buffer_stdout{};
        bool concise_logging{};
        bool skip_syscalls{};
        bool reproducible{};
        bool disable_instruction_precision{};
    };

    struct child_bootstrap_data
    {
        application_settings settings{};
        std::vector<inherited_pipe_handle> inherited_pipes{};
        std::vector<inherited_section_handle> inherited_sections{};
        std::vector<inherited_event_handle> inherited_events{};
    };

    // True on platforms where a child can genuinely be spawned as a separate host OS process
    // running its own copy of this same binary (POSIX hosts only - not Windows, not Emscripten).
    bool supports_child_process_spawning();

    std::filesystem::path resolve_own_executable_path();

    // Forks+execs config.executable_path as a child running in --child-ipc-fd mode, hands it
    // settings/inherited_pipes/inherited_sections over a real socketpair, and blocks (bounded by a
    // timeout) until the child reports it has completed its own initial setup or failed. Does NOT
    // wait for the child to exit - once it answers, it keeps running independently.
    child_process_outcome spawn_child_process(const child_process_spawn_config& config, application_settings settings,
                                              std::vector<inherited_pipe_handle> inherited_pipes,
                                              std::vector<inherited_section_handle> inherited_sections,
                                              std::vector<inherited_event_handle> inherited_events);

    // Used by a process running in --child-ipc-fd mode: reads the settings/inherited_pipes/
    // inherited_sections its parent sent right after spawning it.
    std::optional<child_bootstrap_data> receive_child_bootstrap_data(int fd);

    // Used by a process running in --child-ipc-fd mode, once it has completed its own initial
    // setup (windows_emulator::setup_process_if_necessary), to answer its parent's blocked
    // spawn_child_process call.
    void send_child_ready(int fd, uint64_t peb_address, uint64_t process_parameters_address, uint64_t peb32_address,
                          uint64_t process_params32_address);
    void send_child_failed(int fd, const std::string& detail);

    // Wraps an already-connected fd (child_process_outcome::ipc_fd on the parent side, or
    // options.child_ipc_fd on the child side, both ends of the same socketpair the bootstrap
    // handshake used) as a pipe_ipc_channel, for windows_emulator::register_pipe_ipc_peer. Takes
    // ownership of fd - it is closed when the returned channel is destroyed.
    std::unique_ptr<pipe_ipc_channel> create_fd_pipe_ipc_channel(int fd);

    // Wraps an already-connected fd (child_process_outcome::control_fd on the parent side, or
    // options.child_control_fd on the child side, both ends of the second socketpair
    // spawn_child_process opens for the process_control_channel protocol) as a
    // process_control_channel. Takes ownership of fd - it is closed when the returned channel is
    // destroyed.
    std::unique_ptr<process_control_channel> create_fd_process_control_channel(int fd);

} // namespace sogen
