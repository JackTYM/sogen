#pragma once
#include "std_include.hpp"

#include <arch_emulator.hpp>
#include <machine_type.hpp>
#include "process_manager.hpp"
#include "debug_port.hpp"

#include <stop_reason.hpp>
#include <utils/finally.hpp>
#include <utils/function.hpp>
#include <utils/container.hpp>

#include "syscall_dispatcher.hpp"
#include "process_context.hpp"
#include "kernel_lock.hpp"
#include "scheduler_clock.hpp"
#include "logger.hpp"
#include "file_system.hpp"
#include "frame_pacer.hpp"
#include "filesystem/mount_spec.hpp"
#include "memory_manager.hpp"
#include "module/module_manager.hpp"
#include "network/dns_lookup.hpp"
#include "network/socket_activity.hpp"
#include "network/socket_factory.hpp"
#include "version/windows_version_manager.hpp"
#include "pipe_ipc_channel.hpp"
#include "devices/named_pipe_end_transfer.hpp"
#include "process_control_channel.hpp"
#include <platform/audio_backend.hpp>
#include <platform/crypt_protect_backend.hpp>
#include <platform/console_backend.hpp>
#include <platform/ui_backend.hpp>

namespace sogen
{

    struct io_device;
    struct pointer_input_request;
    struct keyboard_input_request;
    class windows_emulator;
    struct application_settings;

    // Describes one PsAttributeHandleList device handle (always a named pipe in practice, see
    // handle_NtCreateUserProcess) the child must inherit, sent across the real process boundary so
    // the child can reconstruct an equivalent named_pipe device in its own process_context before
    // its guest code starts running. handle values are guest-facing numeric identities, meaningful
    // independently in both processes - no translation is needed, only the descriptive fields.
    struct inherited_pipe_handle
    {
        handle target_handle{};
        named_pipe_end_transfer transfer{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->target_handle);
            buffer.write(this->transfer);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->target_handle);
            buffer.read(this->transfer);
        }
    };

    // Describes one PsAttributeHandleList pagefile-backed section handle the child must inherit,
    // sent across the real process boundary so the child can reconstruct an equivalent section
    // object (with the same content, if the parent had already mapped and written it) in its own
    // process_context before its guest code starts running. Image- and file-backed sections aren't
    // covered here - they're re-derived from the file path instead, not meaningfully "inherited".
    struct inherited_section_handle
    {
        handle target_handle{};
        uint64_t maximum_size{};
        uint32_t section_page_protection{};
        uint32_t allocation_attributes{};
        ACCESS_MASK granted_access{};
        std::shared_ptr<shared_backing> backing{};
        // Set by deserialize when the sender's backing is a genuinely shared mapping: its descriptor
        // travels out of band, and the receiving transport adopts it into `backing`.
        bool wire_backing_is_shared{};
        uint64_t wire_backing_size{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            const auto shared = this->backing && this->backing->is_shared();

            buffer.write(this->target_handle);
            buffer.write(this->maximum_size);
            buffer.write(this->section_page_protection);
            buffer.write(this->allocation_attributes);
            buffer.write(this->granted_access);
            buffer.write(shared);
            buffer.write(static_cast<uint64_t>(this->backing ? this->backing->size() : 0));
            buffer.write_vector(shared || !this->backing ? std::vector<std::byte>{} : this->backing->content());
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            std::vector<std::byte> content{};

            buffer.read(this->target_handle);
            buffer.read(this->maximum_size);
            buffer.read(this->section_page_protection);
            buffer.read(this->allocation_attributes);
            buffer.read(this->granted_access);
            buffer.read(this->wire_backing_is_shared);
            buffer.read(this->wire_backing_size);
            buffer.read_vector(content);

            this->backing = this->wire_backing_is_shared ? nullptr : shared_backing::create_from_content(content);
        }
    };

    // Describes one PsAttributeHandleList event handle the child must inherit, sent across the real
    // process boundary so the child can reconstruct an equivalent event object (with the same
    // signaled state) in its own process_context before its guest code starts running. Only events
    // actually present in the parent's real handle_store need this - the well-known pseudo events
    // (WER_PORT_READY, DBWIN_DATA_READY, ...) are fixed constants that every process's syscall
    // handlers already recognize by bit pattern alone, so they resolve correctly in the child without
    // being forwarded at all (see handle_NtCreateUserProcess).
    struct inherited_event_handle
    {
        handle target_handle{};
        EVENT_TYPE type{};
        bool signaled{};
        std::u16string name{};
        // Slot in the kernel arena (see kernel_state.hpp) holding the event's state, so parent and child
        // share one event; -1 when the event state could not be placed in shared memory.
        int64_t arena_slot{-1};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->target_handle);
            buffer.write(this->type);
            buffer.write(this->signaled);
            buffer.write_string(std::u16string_view(this->name));
            buffer.write(this->arena_slot);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->target_handle);
            buffer.read(this->type);
            buffer.read(this->signaled);
            buffer.read_string(this->name);
            buffer.read(this->arena_slot);
        }
    };

    // Result of spawning a child process (NtCreateUserProcess) as a genuinely separate host OS
    // process. success reports whether the child got far enough through its own startup
    // (setup_process_if_necessary) to hand back real guest addresses for PS_CREATE_INFO -
    // peb_address/process_parameters_address are the child's own PEB/RTL_USER_PROCESS_PARAMETERS
    // guest virtual addresses, meaningless as host pointers since the child lives in its own
    // address space entirely. peb32_address/process_params32_address are the child's PEB32/
    // RTL_USER_PROCESS_PARAMETERS32 guest addresses (0 if the child isn't a WOW64 process) -
    // PS_CREATE_INFO's own SuccessState.PebAddressWow64/UserProcessParametersWow64.
    struct child_process_outcome
    {
        bool success{};
        std::string failure_detail{};
        uint64_t peb_address{};
        uint64_t process_parameters_address{};
        uint64_t peb32_address{};
        uint64_t process_params32_address{};

        // Still-open end of the same socketpair used for the bootstrap handshake, kept alive (rather
        // than closed once the handshake completes) so it can be registered as a pipe_ipc_channel
        // transport - see windows_emulator::register_pipe_ipc_peer. Only meaningful when success is
        // true; -1 if this platform doesn't support child process spawning at all.
        int ipc_fd{-1};

        // Parent's end of a second, separate socketpair dedicated to the synchronous
        // process_control_channel request/response protocol - kept apart from ipc_fd so a blocking
        // control round-trip can never stall or be reordered against the pipe_ipc_channel broadcast
        // traffic. Only meaningful when success is true; -1 if this platform doesn't support child
        // process spawning at all.
        int control_fd{-1};

        // Parent's end of a third socketpair on which the child sends control requests for processes it
        // has no channel to, for the parent to route (see windows_emulator::register_child_routing_channel).
        int router_fd{-1};

        // The real host OS process id backing this child (fork()'s return value in the parent) -
        // unrelated to child_process_record::pid, which is a synthetic guest pid. Lets a caller
        // force_kill() the real process even after its process_control_channel has poisoned itself
        // on a timeout. Only meaningful when success is true; -1 if this platform doesn't support
        // child process spawning at all.
        int host_pid{-1};
    };

    struct emulator_callbacks : module_manager::callbacks, process_context::callbacks
    {
        template <typename T>
        using opt_func = utils::optional_function<T>;

        using continuation = instruction_hook_continuation;

        opt_func<void()> on_exception{};

        // Spawns a child process (NtCreateUserProcess) as a genuinely separate host OS process
        // running its own copy of the analyzer, and blocks until that child has either completed
        // its own initial setup (module mapping, PEB/process-parameters construction) or failed -
        // only the embedder (e.g. the analyzer's main.cpp) knows how to re-invoke itself as a
        // child, so this can't happen inside windows_emulator itself. Unset means the embedder
        // doesn't support child processes; the handler reports STATUS_NOT_SUPPORTED in that case.
        // record_id is the caller's process_context::child_processes key for this child (also shared
        // by its minted process/thread pseudo handles), passed through so the embedder can register
        // the child's pipe_ipc_channel/process_control_channel under the same id.
        opt_func<child_process_outcome(uint32_t record_id, application_settings, std::vector<inherited_pipe_handle>,
                                       std::vector<inherited_section_handle>, std::vector<inherited_event_handle>,
                                       std::shared_ptr<shared_backing> kernel_arena_backing)>
            create_child_process{};

        opt_func<void(uint64_t address, uint64_t length, memory_permission)> on_memory_protect{};
        opt_func<void(uint64_t address, uint64_t length, memory_permission, bool commit)> on_memory_allocate{};
        opt_func<void(uint64_t address, uint64_t length, memory_operation, memory_violation_type type)> on_memory_violate{};

        opt_func<void()> on_rdtsc{};
        opt_func<void()> on_rdtscp{};
        opt_func<continuation(uint32_t syscall_id, std::string_view syscall_name)> on_syscall{};
        opt_func<void(std::string_view data)> on_stdout{};
        opt_func<void(std::string_view type, std::u16string_view name)> on_generic_access{};
        opt_func<void(std::string_view description)> on_generic_activity{};
        opt_func<void(std::string_view description)> on_suspicious_activity{};
        opt_func<void(network::socket_activity activity, const network::address& address)> on_socket_activity{};
        utils::callback_list<void(std::string_view message)> on_debug_string{};
        utils::callback_list<void(const mapped_module& mod, const mapped_section& section, uint64_t address)> on_section_first_execution{};
        opt_func<void(uint64_t address)> on_instruction{};
        opt_func<void()> on_event_pump{};
        opt_func<void(io_device& device, std::u16string_view device_name, ULONG code)> on_ioctrl{};
        opt_func<void(uint32_t fail_code)> on_fast_fail{};
    };

    struct application_settings
    {
        windows_path application{};
        std::u16string argument0{};
        windows_path working_directory{};
        std::vector<std::u16string> arguments{};
        std::optional<std::u16string> command_line{};
        utils::unordered_insensitive_u16string_map<std::u16string> environment{};
        // When set, used verbatim as the child's RTL_USER_PROCESS_PARAMETERS.ImagePathName instead of
        // application.u16string() - windows_path canonicalizes (lower-cases) every component for use
        // as a case-insensitive filesystem lookup key, but real Windows preserves the caller-supplied
        // casing in ImagePathName, which guest code can observe (e.g. via GetModuleFileName).
        std::optional<std::u16string> image_path{};

        // Set for a child spawned via NtCreateUserProcess with THREAD_CREATE_FLAGS_CREATE_SUSPENDED
        // (handle_NtCreateUserProcess) - the child's bootstrap (main.cpp) leaves its initial thread's
        // suspended count at 1 instead of running it immediately, so it executes no guest instructions
        // until the parent issues a resume_thread control request (see process_control_server.cpp).
        bool start_suspended{false};

        // Runs the process under the LocalSystem account (S-1-5-18) instead of the emulated user, as services
        // such as lsass do on Windows. Their own DACL checks (e.g. SYSTEM-only ACEs) depend on that identity.
        bool local_system{false};
        uint32_t process_id{default_guest_process_id};
        uint32_t thread_id{8};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->application);
            buffer.write(this->argument0);
            buffer.write(this->working_directory);
            buffer.write_vector(this->arguments);
            buffer.write_optional(this->command_line);
            buffer.write_map(this->environment);
            buffer.write_optional(this->image_path);
            buffer.write(this->start_suspended);
            buffer.write(this->local_system);
            buffer.write(this->process_id);
            buffer.write(this->thread_id);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->application);
            buffer.read(this->argument0);
            buffer.read(this->working_directory);
            buffer.read_vector(this->arguments);
            buffer.read_optional(this->command_line);
            buffer.read_map(this->environment);
            buffer.read_optional(this->image_path);
            buffer.read(this->start_suspended);
            buffer.read(this->local_system);
            buffer.read(this->process_id);
            buffer.read(this->thread_id);
        }
    };

    // Knobs for values the emulator exposes to the emulated process that don't
    // depend on the host environment. Samples (particularly anti-analysis
    // payloads) probe these to detect VM/sandbox; today they are hardcoded in
    // process_context.cpp (PEB.NumberOfProcessors = 4) and kusd_mmio.cpp
    // (KUSER_SHARED_DATA.NtProductType = NtProductWinNt). Defaults match the
    // legacy hardcoded values — behavior is unchanged when consumers leave
    // this field at its default.
    struct fake_environment_config
    {
        uint32_t number_of_processors{4};
        uint8_t nt_product_type{1}; // NtProductWinNt
        uint32_t locale_id{0x0407};
    };

    struct emulator_settings
    {
        bool disable_logging{false};
        bool use_relative_time{false};
        bool use_instruction_precision{true};

        // Timing fidelity vs. speed. With interception on (the default, used by the analyzer) rdtsc reads
        // and KUSER_SHARED_DATA accesses trap so the emulated clock stays exact and deterministic. Turning
        // them off (as the sandbox does) lets the guest read the host TSC natively and KUSD from a plain
        // mapped page, removing those VM exits. The cost is exact/reproducible timing, and KUSD's time
        // fields then only advance in steps of the interrupt thread's 20 ms period.
        bool intercept_rdtsc{true};
        bool intercept_kusd{true};

        std::filesystem::path emulation_root{};
        std::filesystem::path registry_directory{"./registry"};

        // When false, construct without loading the registry hives. Intended for headless harnesses
        // (e.g. fuzzing) that never run the guest and don't read the registry, so they can construct
        // with no emulation root / registry directory on disk. The registry ctor otherwise eagerly
        // parses the mandatory hives (SYSTEM/SOFTWARE/SAM/...).
        bool load_registry{true};

        std::unordered_map<uint16_t, uint16_t> port_mappings{};
        filesystem_mode root_mode{default_root_mode};
        std::filesystem::path root_layer{};
        std::vector<mount_spec> mounts{};

        fake_environment_config fake_env{};

        network::network_mode network{network::network_mode::offline};

        // Report Vulkan features DXVK requires to games even where the host GPU lacks them (Mali, older Adreno), so
        // DXVK accepts the adapter. A game that really uses one of them renders incorrectly there.
        bool spoof_gpu_features{false};

        // Gives the process an elevated administrator token. Windows then starts elevation requests such as
        // ShellExecute "runas" directly instead of going through the consent service, which is not emulated.
        bool elevated{false};
    };

    struct provider_mount
    {
        windows_path guest_path{};
        std::unique_ptr<mount_provider> provider{};
        filesystem_mode mode{default_mount_mode};
    };

    struct emulator_interfaces
    {
        std::unique_ptr<utils::clock> clock{};
        std::unique_ptr<network::dns_lookup> dns_lookup{};
        std::unique_ptr<network::socket_factory> socket_factory{};
        std::unique_ptr<ui_backend> ui{};
        std::unique_ptr<audio_backend> audio{};
        std::unique_ptr<crypt_protect_backend> crypt_protect{};
        process_manager* processes{};
        std::unique_ptr<console_backend> console{};
        // Mounted after emulator_settings::mounts, for sources that are not a host directory.
        std::vector<provider_mount> mounts{};
    };

    // Per-vCPU scheduler state: the guest thread a virtual CPU is currently executing
    // and its yield request. Replaces the previous global "active thread" notion
    // (docs/multi-vcpu-design.md, section 5.1).
    struct vcpu_context
    {
        guest_cpu& cpu;
        emulator_thread* active_thread{};
        std::atomic_bool switch_thread{false};
        std::atomic<std::thread::id> runner{};
        // Set while this vCPU waits in wait_for_thread_registers or runs a host call through run_without_kernel_lock.
        // Its active thread's registers are saved meanwhile, and it reloads them before it continues.
        bool active_thread_saved{false};

        void set_active_thread(emulator_thread* thread)
        {
            this->active_thread = thread;
            if (thread)
            {
                thread->last_vcpu = this;
            }
        }

        emulator_thread& thread() const
        {
            if (!this->active_thread)
            {
                throw std::runtime_error("No active thread!");
            }

            return *this->active_thread;
        }

        x86_64_cpu& x64_cpu() const
        {
            auto* x64 = dynamic_cast<x86_64_cpu*>(&this->cpu);
            if (!x64)
            {
                throw std::runtime_error("This operation is only implemented for x86-64 guest CPUs");
            }

            return *x64;
        }

        arm64_cpu& arm64_cpu_ref() const
        {
            auto* arm64 = dynamic_cast<arm64_cpu*>(&this->cpu);
            if (!arm64)
            {
                throw std::runtime_error("This operation is only implemented for ARM64 guest CPUs");
            }

            return *arm64;
        }
    };

    // The COFF machine of an application, resolved through its working directory and the settings' emulation
    // root and path mappings as the emulator will resolve it. Selects which guest CPU to create for the application.
    PEMachineType get_application_machine(const emulator_settings& settings, application_settings app_settings);

    // AMD64, ARM64EC and I386 (WoW64) images run on an x86-64 CPU, ARM64 images on an ARM64 CPU.
    machine_type get_machine_type(PEMachineType machine);

    class windows_emulator
    {
        uint64_t executed_instructions_{0};
        application_settings application_settings_{};

        std::unique_ptr<guest_machine> emu_{};
        bool use_native_timestamp_counter_{};
        std::unique_ptr<scheduler_clock> clock_{};
        network::network_mode network_mode_{};
        std::unique_ptr<network::dns_lookup> dns_lookup_{};
        std::unique_ptr<network::socket_factory> socket_factory_{};
        std::unique_ptr<ui_backend> ui_backend_{};
        frame_pacer frame_pacer_{};
        std::unique_ptr<audio_backend> audio_backend_{};
        std::unique_ptr<crypt_protect_backend> crypt_protect_backend_{};
        process_manager* process_manager_{};
        debug_port debugger_{};
        bool debugger_saw_process_{};
        std::unique_ptr<console_backend> console_backend_{};
        bool setup_completed_{false};

        // One entry per sibling OS process this process is directly connected to (its parent, if it was
        // itself spawned as a child, plus one per child it has spawned) - see child_process_spawn.hpp.
        // Named-pipe writes/connects are broadcast to every entry; pump_pipe_ipc() applies whatever comes
        // back the other way to this process's own named_pipe device instances.
        std::vector<std::unique_ptr<pipe_ipc_channel>> pipe_ipc_peers_{};

        window_surface_exporter surface_exporter_{};
        window_surface_importer surface_importer_{};

        // Live process_control_channel per child this process spawned, keyed by the same record_id
        // as process_context::child_processes - see register_child_control_channel. Deliberately not
        // serialized, same as pipe_ipc_peers_.
        std::map<uint32_t, std::unique_ptr<process_control_channel>> child_control_channels_{};

        // The incoming process_control_channel this process listens on for requests from its own
        // parent (see child_process_outcome::control_fd and --child-control-fd), serviced by
        // pump_process_control (perform_context_switch_work). Null for the root process and for any
        // child on a platform without spawning support - see set_process_control_server.
        std::unique_ptr<process_control_channel> process_control_server_{};

        // Outgoing channel to this process's parent for control requests addressed to a process this one
        // has no channel to (a sibling elsewhere in the spawn tree) - see set_process_control_router.
        std::unique_ptr<process_control_channel> process_control_router_{};

        // Incoming routed requests, one channel per spawned child - see register_child_routing_channel.
        std::vector<std::unique_ptr<process_control_channel>> child_routing_channels_{};

        // Short names (see devices/named_pipe.hpp's pipe_short_name) of every named pipe server
        // instance known to exist, whether created in this process (NtCreateNamedPipeFile) or
        // learned about from a sibling OS process (pipe_ipc_message_type::server_created). Backs
        // FSCTL_PIPE_WAIT (WaitNamedPipeW) -- see is_named_pipe_server_known.
        std::set<std::u16string, utils::insensitive_u16string_less> known_named_pipe_servers_{};

        // Events of threads currently parked in FSCTL_PIPE_WAIT (devices/named_pipe.hpp's wait()),
        // keyed by the short pipe name they're waiting for. Woken by register_named_pipe_server.
        std::multimap<std::u16string, handle, utils::insensitive_u16string_less> pending_pipe_waits_{};

        // Guest pids of every live process anywhere in the spawn tree that this process has learned
        // about via a process_alive broadcast (see broadcast_process_alive/pump_pipe_ipc), whether
        // spawned by this process, its parent, or a sibling several hops away. Lets handle_NtOpenProcess
        // resolve a sibling pid it never spawned itself (e.g. a crash-reporting broker registering every
        // Chromium child process by pid, not just its own children) - see is_known_sibling_pid.
        std::map<uint32_t, std::optional<bool>> known_sibling_pids_{};
        std::map<uint32_t, std::optional<uint64_t>> known_sibling_peb32_{};

        // Test tooling (SOGEN_INJECT_INPUT): left clicks, keys and typed text delivered in order through the same
        // path as host UI input, each a given time after the newest visible window of its class appeared (or the
        // previous step, whichever is later). A step may also wait until the guest writes a text to a file, and an
        // optional step is skipped once a later step's text is written first.
        struct scheduled_input
        {
            enum class kind : uint8_t
            {
                click,
                key,
                text,
            };

            kind type{};
            std::u16string window_class{};
            std::u16string ready_child_class{};
            std::string ready_written_text{};
            bool ready_text_written{};
            bool optional{};
            std::chrono::milliseconds delay{};
            int32_t x{};
            int32_t y{};
            uint16_t virtual_key{};
            std::u16string text{};
            std::optional<std::chrono::steady_clock::time_point> window_seen{};
            hwnd seen_window{};
            bool delivered{};
        };

        std::vector<scheduled_input> scheduled_inputs_{};

        void parse_scheduled_input(std::string_view specification);
        static void append_key_stroke(std::vector<keyboard_input_request>& events, uint16_t virtual_key, bool shift);
        void deliver_keyboard_input(hwnd window, std::span<const keyboard_input_request> events);
        void forward_keyboard_input(std::span<const keyboard_input_request> events);

        void deliver_ui_event(const ui_event& event);
        bool forward_pointer_input_to_foreign_children(hwnd parent, uint32_t message, uint64_t w_param, int32_t x, int32_t y);
        // Host threads owned by the process, such as audio streams, signal the guest through these, so they have to
        // outlive it.
        std::mutex idle_wakeup_mutex_{};
        std::condition_variable idle_wakeup_cv_{};
        uint32_t pending_idle_wakeups_{};
        std::atomic<uint32_t> idle_vcpus_{0};
        std::mutex pending_event_signals_mutex_{};
        std::map<uint64_t, uint32_t> pending_event_signals_{};

      public:
        const std::filesystem::path emulation_root{};
        const fake_environment_config fake_env{};
        emulator_callbacks callbacks{};
        logger log{};
        file_system file_sys;
        memory_manager memory;
        registry_manager registry{};
        windows_version_manager version{};
        module_manager mod_manager;
        process_context process;
        syscall_dispatcher dispatcher;

        windows_emulator(std::unique_ptr<guest_machine> emu, const emulator_settings& settings = {}, emulator_callbacks callbacks = {},
                         emulator_interfaces interfaces = {});
        windows_emulator(std::unique_ptr<guest_machine> emu, application_settings app_settings, const emulator_settings& settings = {},
                         emulator_callbacks callbacks = {}, emulator_interfaces interfaces = {});

        windows_emulator(windows_emulator&&) = delete;
        windows_emulator(const windows_emulator&) = delete;
        windows_emulator& operator=(windows_emulator&&) = delete;
        windows_emulator& operator=(const windows_emulator&) = delete;

        ~windows_emulator();

        guest_machine& emu()
        {
            return *this->emu_;
        }

        const guest_machine& emu() const
        {
            return *this->emu_;
        }

        x86_64_emulator& x64_emu() const
        {
            auto* x64 = dynamic_cast<x86_64_emulator*>(this->emu_.get());
            if (!x64)
            {
                throw std::runtime_error("This operation is only implemented for x86-64 guest machines");
            }

            return *x64;
        }

        const windows_arch_abi& abi() const
        {
            return this->process.abi();
        }

        utils::clock& clock()
        {
            return *this->clock_;
        }

        [[nodiscard]] scheduler_clock& scheduling_clock()
        {
            return *this->clock_;
        }

        const utils::clock& clock() const
        {
            return *this->clock_;
        }

        network::network_mode network_mode() const
        {
            return this->network_mode_;
        }

        network::dns_lookup& dns_lookup()
        {
            return *this->dns_lookup_;
        }

        const network::dns_lookup& dns_lookup() const
        {
            return *this->dns_lookup_;
        }

        network::socket_factory& socket_factory()
        {
            return *this->socket_factory_;
        }

        // Registers a sibling OS process's IPC transport (see child_process_outcome::ipc_fd and the
        // --child-ipc-fd bootstrap channel) as a named-pipe forwarding peer. The embedder (analyzer's
        // main.cpp) owns the actual fd/socketpair and constructs the concrete channel; windows_emulator
        // only ever talks to it through the pipe_ipc_channel interface.
        void register_pipe_ipc_peer(std::unique_ptr<pipe_ipc_channel> peer);

        // Registers a live process_control_channel for a child spawned via NtCreateUserProcess (see
        // child_process_outcome::control_fd), keyed by the same record_id that also indexes
        // process_context::child_processes and backs the process/thread pseudo handles minted for it
        // (see handle_NtCreateUserProcess) - resolve_child_target (cross_process.hpp) looks channels up
        // by that id. Deliberately not serialized, same as pipe_ipc_peers_: a snapshot restore has no
        // live host OS process on the other end of the socketpair to reconnect to.
        void register_child_control_channel(uint32_t record_id, std::unique_ptr<process_control_channel> channel);

        // The live control channel for record_id, or nullptr if none is registered (no spawning
        // support on this platform, or a snapshot-restored process whose children's channels were
        // never serialized).
        process_control_channel* find_child_control_channel(uint32_t record_id);

        // Drops the control channel for record_id, e.g. once its child has exited.
        void drop_child_control_channel(uint32_t record_id);

        // Broadcasts a named-pipe write/connect to every registered peer, so the other end of the connection
        // in a sibling OS process observes it exactly like a same-process one would (see
        // deliver_bytes_to_named_pipe/connect_named_pipe_client in devices/named_pipe.hpp).
        void broadcast_named_pipe_write(std::u16string_view name, uint64_t connection_id, bool to_server, std::string_view data);
        void broadcast_named_pipe_connect(std::u16string_view name, uint64_t connection_id,
                                          std::optional<uint32_t> client_process_id = std::nullopt);
        void broadcast_named_pipe_server_created(std::u16string_view name);

        // Hands an ALPC transport message to the rest of the process tree (see alpc::deliver).
        void broadcast_alpc(std::u16string_view port_name, uint64_t connection_id, bool to_server, std::string data);

        // Announces a newly-spawned guest pid (handle_NtCreateUserProcess) to every peer, which relays
        // it on to the rest of the tree exactly like broadcast_named_pipe_server_created's own
        // known-server announcement (see pump_pipe_ipc's generic relay loop). is_known_sibling_pid then
        // answers whether a given pid is a live process somewhere in the tree, for handle_NtOpenProcess's
        // sibling-pid fallback.
        void broadcast_process_alive(uint32_t pid, std::optional<bool> native_64bit, std::optional<uint64_t> peb32_address = std::nullopt);
        bool is_known_sibling_pid(uint32_t pid) const;
        std::optional<bool> known_sibling_native_64bit(uint32_t pid) const;
        std::optional<uint64_t> known_sibling_peb32(uint32_t pid) const;

        // Announces that `window`, whose hierarchy is attached to `parent_window` of another process, took
        // keyboard focus, so that process's input queue routes keyboard input to it (see pump_pipe_ipc).
        void broadcast_focus_change(hwnd window, hwnd parent_window);

        // Announces window creation/destruction to the whole tree, which mirrors the window into every process's
        // handle table (see user_handle_table::mirror_foreign_window).
        void broadcast_window_created(hwnd window, uint32_t thread_id);
        void broadcast_window_destroyed(hwnd window);

        // Shows a frame presented into `window`. A window embedded into a hierarchy of another process (e.g. a
        // GPU process's swap-chain window reparented into a browser window) has no on-screen window here, so its
        // frames travel to the process that owns the visible top-level (see relay_window_surface).
        void present_window_surface(hwnd window, const ui_surface_desc& surface);

        // Applies whatever named-pipe traffic has arrived from registered peers since the last call.
        // Called once per scheduler tick (perform_context_switch_work), the same cadence as io_device's
        // own work() pump.
        void pump_pipe_ipc();

        // Sets the incoming process_control_channel this process listens on for requests from its own
        // parent (see child_process_outcome::control_fd and --child-control-fd). Left null for the root
        // process and for any child on a platform without spawning support, in which case
        // pump_process_control_server is a no-op.
        void set_process_control_server(std::unique_ptr<process_control_channel> channel);

        // Drains every request currently queued on the process_control_server, if one is registered.
        // Called once per scheduler tick (perform_context_switch_work) right after pump_pipe_ipc, so it
        // shares that call's idle-loop guarantee: perform_thread_switch keeps invoking
        // perform_context_switch_work via switch_to_next_thread even while no thread is runnable, so a
        // child with zero runnable threads still services its parent's requests.
        void pump_process_control_server();

        // Sets the outgoing channel through which this process asks its parent to forward a control
        // request to a process it has no channel to (see child_process_outcome::router_fd and
        // --child-router-fd). Left null for the root process.
        void set_process_control_router(std::unique_ptr<process_control_channel> channel);
        process_control_channel* process_control_router() const;

        // Registers the channel a spawned child sends its routed control requests on (the parent end of
        // child_process_outcome::router_fd), serviced by pump_child_routing_channels.
        void register_child_routing_channel(std::unique_ptr<process_control_channel> channel);

        // Answers every routed request currently queued by a child: executed here when addressed to this
        // process, forwarded to the addressed child's control channel, or passed on to this process's own
        // parent. Called once per scheduler tick (perform_context_switch_work).
        void pump_child_routing_channels();

        // The control channel of the live child with guest pid `pid`, or nullptr if this process spawned
        // no such child.
        process_control_channel* find_child_control_channel_by_pid(uint32_t pid);

        // Called by main.cpp right before this process's own execution ends, for every exit path
        // (clean self-terminate, an uncaught exception, GDB detach, ...) - a no-op unless this
        // process was itself spawned as a child (process_control_server_ is set). Reports
        // exit_status to the parent over the same channel process_control_server_ listens on, so
        // its child_process_record stops reporting STATUS_PENDING forever - see
        // pump_child_exit_notifications and process_context::child_process_record.
        void notify_own_exit(std::optional<NTSTATUS> exit_status);

        // Drains any exit_notification a spawned child has sent since the last call (see
        // process_control_channel::notify_exit/notify_own_exit) and applies it to the matching
        // still-pending entry in process_context::child_processes, so observe_object_signal
        // (emulator_thread.cpp) sees the child's process handle as signaled - the same path a
        // remote NtTerminateProcess already updates. Called once per scheduler tick
        // (perform_context_switch_work), alongside pump_pipe_ipc/pump_process_control_server.
        void pump_child_exit_notifications();

        // Records short_name (see devices/named_pipe.hpp's pipe_short_name) as a known-existing named
        // pipe server instance and wakes every thread currently parked in FSCTL_PIPE_WAIT for it (see
        // devices/named_pipe.hpp's wait()). Called both for a local NtCreateNamedPipeFile and for a
        // server_created notification forwarded from a sibling OS process.
        void register_named_pipe_server(std::u16string_view short_name);
        void unregister_named_pipe_server(std::u16string_view short_name);

        // True once a named pipe server instance with this short name is known to exist, whether
        // created in this process or a sibling one (see register_named_pipe_server).
        bool is_named_pipe_server_known(std::u16string_view short_name);

        // Registers event as a wake target for register_named_pipe_server(short_name), for a thread
        // parked in FSCTL_PIPE_WAIT that found no known server yet.
        void register_pipe_wait(std::u16string_view short_name, handle event);

        const network::socket_factory& socket_factory() const
        {
            return *this->socket_factory_;
        }

        ui_backend& ui()
        {
            return *this->ui_backend_;
        }

        frame_pacer& frames()
        {
            return this->frame_pacer_;
        }

        const ui_backend& ui() const
        {
            return *this->ui_backend_;
        }

        audio_backend& audio()
        {
            return *this->audio_backend_;
        }

        const audio_backend& audio() const
        {
            return *this->audio_backend_;
        }

        crypt_protect_backend& crypt_protect()
        {
            return *this->crypt_protect_backend_;
        }

        const crypt_protect_backend& crypt_protect() const
        {
            return *this->crypt_protect_backend_;
        }

        process_manager* processes() const
        {
            return this->process_manager_;
        }

        debug_port& debugger()
        {
            return this->debugger_;
        }

        // Makes this process the debuggee of a debugger in the parent process.
        void attach_debugger();
        // Runs on the process manager connection thread.
        void detach_debugger();

        // Stops the current thread until the debugger continues the event; returns the continue status. Needs the
        // kernel lock, and does nothing without a debugger.
        uint32_t report_debug_event(process_debug_event event);

        // Brings the emulation loop around to its next scheduling point soon, where it runs the debugger's requests.
        void interrupt_for_debugger();

        console_backend& console()
        {
            return *this->console_backend_;
        }

        const console_backend& console() const
        {
            return *this->console_backend_;
        }

        void handle_ui_event(const ui_event& event);
        bool deliver_pointer_input_from_parent(hwnd window, const pointer_input_request& input);
        void deliver_keyboard_input_from_parent(hwnd window, std::span<const keyboard_input_request> events);
        void take_focus_from_parent(hwnd window, hwnd previous_focus);
        void release_focus_to_parent(hwnd window, hwnd new_focus);
        void inject_scheduled_input();
        void observe_file_write_for_scheduled_input(std::string_view data);
        void handle_ui_event_locked(const ui_event& event);
        void update_key_state(const ui_event& event);
        ui_event resolve_injected_event(const ui_event& event) const;
        void apply_cursor_capture();

        // The highest address the process's own code can reach: 32-bit code only reaches the low 2GB unless the
        // executable is large-address aware.
        uint64_t highest_user_address() const;
        void release_held_input();

        // Input held for low-level hooks: the next hook the given thread has to run, marked as running.
        std::optional<std::pair<process_context::low_level_input, user_low_level_hook>> begin_low_level_hook(emulator_thread& thread);
        void finish_low_level_hook(emulator_thread& thread, bool swallow);
        bool has_low_level_hook_work(uint32_t thread_id);
        emulator_thread* drain_low_level_input();
        void deliver_raw_input(const process_context::raw_input_payload& payload, hwnd explicit_target);
        void deliver_raw_mouse_input(int32_t dx, int32_t dy, uint16_t button_flags, uint16_t button_data = 0);
        void deliver_raw_keyboard_input(uint16_t vkey, uint16_t scan_code, uint32_t message, bool extended);

        // Observer convenience for external consumers (gdb stub, analyzer, python
        // bindings) and legacy paths that don't thread a vcpu_context through. Resolves
        // to the vCPU whose handler is currently running: all handler/observer code runs
        // under the kernel lock, so exactly one vCPU dispatches at a time and this is
        // race-free. Falls back to vCPU 0 when no handler is active (e.g. setup).
        emulator_thread& current_thread() const
        {
            return this->active_vcpu().thread();
        }

        // The CPU of the vCPU currently dispatching a handler on the calling host thread (see
        // scoped_dispatch). emu() is only the facade/vCPU 0, so handlers that inspect the faulting
        // register state must go through here to observe the right vCPU when vcpu_count > 1.
        guest_cpu& active_cpu() const
        {
            return this->active_vcpu().cpu;
        }

        x86_64_cpu& active_x64_cpu() const
        {
            return this->active_vcpu().x64_cpu();
        }

        // Run fn as a dispatched handler for the CPU that triggered a hook: takes the kernel lock and
        // marks that vCPU as the dispatching one, so active_cpu()/current_thread() resolve to it. Meant
        // for hooks installed outside setup_hooks (e.g. the analyzer's cpuid hook) which otherwise run
        // lock-free and would observe only the facade/vCPU 0 when vcpu_count > 1.
        template <typename Function>
        auto dispatch_on_cpu(cpu_interface& cpu, Function&& fn)
        {
            const std::scoped_lock lock(this->kernel_lock_);
            const scoped_dispatch dispatch(*this, this->vcpu(cpu.index()));
            return std::forward<Function>(fn)();
        }

        template <typename Function>
        auto synchronize(Function&& fn)
        {
            const std::scoped_lock lock(this->kernel_lock_);
            return std::forward<Function>(fn)();
        }

        vcpu_context& vcpu(const size_t index)
        {
            return *this->vcpus_.at(index);
        }

        // Marks vcpu as the one currently dispatching a handler on the calling host
        // thread, so current_thread() resolves correctly. RAII; must be held only while
        // the kernel lock is held.
        class scoped_dispatch
        {
          public:
            scoped_dispatch(windows_emulator& emu, vcpu_context& vcpu)
                : emu_(&emu),
                  previous_(emu.dispatch_vcpu_)
            {
                emu.dispatch_vcpu_ = &vcpu;
            }

            ~scoped_dispatch()
            {
                this->emu_->dispatch_vcpu_ = this->previous_;
            }

            scoped_dispatch(const scoped_dispatch&) = delete;
            scoped_dispatch& operator=(const scoped_dispatch&) = delete;
            scoped_dispatch(scoped_dispatch&&) = delete;
            scoped_dispatch& operator=(scoped_dispatch&&) = delete;

          private:
            windows_emulator* emu_{};
            vcpu_context* previous_{};
        };

        // Post-mortem exception capture for multi-vCPU debugging. Recorded under the
        // kernel lock (no I/O, no extra locking), dumped after the run ends, so it does
        // not perturb the timing-sensitive races it is meant to catch.
        struct exception_trace_entry
        {
            uint32_t status{};
            uint32_t tid{};
            uint32_t vcpu{};
            uint64_t rip{};
            uint64_t info{};
        };

        void record_exception_trace(const exception_trace_entry& entry)
        {
            this->exception_trace_[this->exception_trace_index_ % this->exception_trace_.size()] = entry;
            ++this->exception_trace_index_;
        }

        void dump_exception_trace();

        // Prints BEL contention stats when SOGEN_LOCK_PROFILE is set (see kernel_lock).
        void dump_lock_profile();

        // Signal a guest event from a host-owned thread (e.g. the audio render thread). The request is queued and
        // applied by the scheduler under the kernel lock, so it cannot race a concurrent close on an emulator
        // thread; a handle the guest has already closed is simply ignored. Taking the kernel lock here instead
        // would either stall the caller behind long kernel operations or drop the signal while it is busy.
        // Requests count as separate wakes: a new one is only delivered once the guest consumed the previous
        // one, so signals queued while the lock was busy don't coalesce in the event. At most maximum_pending
        // wakes are kept, for clients that stop waiting.
        void request_guest_event_signal(handle event_handle, uint32_t maximum_pending);
        void apply_pending_guest_event_signals();

        // For state restored from outside (a minidump): the process already exists, so it is not bootstrapped.
        void mark_process_setup_completed()
        {
            this->setup_completed_ = true;
        }

        void wake_idle_vcpu();

        uint64_t get_executed_instructions() const
        {
            return this->executed_instructions_;
        }

        bool runs_as_local_system() const
        {
            return this->application_settings_.local_system;
        }

        bool uses_instruction_precision() const
        {
            return this->instruction_precision_;
        }

        bool uses_relative_time() const
        {
            return this->use_relative_time_;
        }

        bool spoofs_gpu_features() const
        {
            return this->spoof_gpu_features_;
        }

        bool runs_elevated() const
        {
            return this->elevated_;
        }

        uint32_t vcpu_count() const
        {
            return this->vcpu_count_;
        }

        stop_reason last_stop_reason() const
        {
            return this->last_stop_reason_;
        }

        const std::string& last_stop_detail() const
        {
            return this->last_stop_detail_;
        }

        // Called by internal paths that force a stop due to an error (syscall
        // dispatcher unknown/unimplemented/exception). Public so future error
        // paths can record themselves without friending everyone.
        void record_stop(stop_reason r, std::string detail = {})
        {
            this->last_stop_reason_ = r;
            this->last_stop_detail_ = std::move(detail);
        }

        void setup_process_if_necessary();

        void start(size_t count = 0);
        void stop();

        // Stops the process for a snapshot: right away, or at the end of the next frame of a device that waits for one.
        void request_suspend();
        // Called by such a device at the end of a frame.
        bool take_suspend_request();
        void cancel_suspend_request();

        void prepare_serialization();
        void serialize(utils::buffer_serializer& buffer) const;
        void deserialize(utils::buffer_deserializer& buffer);

        void save_snapshot();
        void restore_snapshot();

        uint16_t get_host_port(const uint16_t emulator_port) const
        {
            const auto entry = this->port_mappings_.find(emulator_port);
            if (entry == this->port_mappings_.end())
            {
                return emulator_port;
            }

            return entry->second;
        }

        uint16_t get_emulator_port(const uint16_t host_port) const
        {
            for (const auto& mapping : this->port_mappings_)
            {
                if (mapping.second == host_port)
                {
                    return mapping.first;
                }
            }

            return host_port;
        }

        void map_port(const uint16_t emulator_port, const uint16_t host_port)
        {
            if (emulator_port != host_port)
            {
                this->port_mappings_[emulator_port] = host_port;
                return;
            }

            const auto entry = this->port_mappings_.find(emulator_port);
            if (entry != this->port_mappings_.end())
            {
                this->port_mappings_.erase(entry);
            }
        }

        void yield_thread(vcpu_context& vcpu, bool alertable = false);
        bool perform_thread_switch(vcpu_context& vcpu, std::unique_lock<kernel_lock>& lock);
        bool perform_thread_switch(vcpu_context& vcpu);
        bool activate_thread(vcpu_context& vcpu, uint32_t id);
        void try_warm_kernelbase_nls_cache_for_thread(vcpu_context& vcpu);

        // Runs f with the kernel lock released, for host calls that can block for a long time (e.g. a GPU present)
        // and would otherwise stall every other vCPU. f must not touch any emulator or guest state. Requires the
        // kernel lock, which is held again when this returns. Other vCPUs dispatch meanwhile, so the dispatch
        // marker is set aside for that time. The dispatching vCPU's thread counts as saved meanwhile, so that a
        // context access from another vCPU does not wait for f; its registers, including any change made to them,
        // are reloaded before this returns. If f throws, the vCPU keeps its own registers and such a change is lost.
        // Within a dispatch, only a syscall handler may call this, as an access treats the saved registers as the
        // syscall's hook frame.
        template <typename F>
        decltype(auto) run_without_kernel_lock(F&& f)
        {
            this->kernel_lock_.assert_held();
            auto* dispatch_vcpu = this->dispatch_vcpu_;
            const auto saved = this->save_dispatching_thread(dispatch_vcpu);
            this->dispatch_vcpu_ = nullptr;
            this->kernel_lock_.unlock();

            const auto run_unlocked = [&]() -> decltype(auto) {
                const auto _ = utils::finally([this, dispatch_vcpu, saved] {
                    this->kernel_lock_.lock();
                    this->dispatch_vcpu_ = dispatch_vcpu;
                    if (saved)
                    {
                        dispatch_vcpu->active_thread_saved = false;
                    }
                });
                return f();
            };

            if constexpr (std::is_void_v<decltype(run_unlocked())>)
            {
                run_unlocked();
                if (saved)
                {
                    this->reload_dispatching_thread(*dispatch_vcpu);
                }
            }
            else
            {
                auto result = run_unlocked();
                if (saved)
                {
                    this->reload_dispatching_thread(*dispatch_vcpu);
                }

                return result;
            }
        }

        // Swaps the registers of the outgoing thread on a vCPU for the incoming one. With more than one vCPU the
        // hypercalls run with the kernel lock released: they only touch this vCPU, but cost tens of microseconds and
        // would otherwise serialize every other vCPU behind each thread switch. Both threads are marked as in transit
        // meanwhile. Requires the kernel lock, and incoming must already be the vCPU's active thread.
        void transfer_thread_registers(vcpu_context& vcpu, emulator_thread* outgoing, emulator_thread& incoming);

        // Blocks until the thread's saved registers are current: no vCPU is transferring them and no other vCPU has the
        // thread loaded. A vCPU that has it loaded is asked to switch away, which saves them. The calling vCPU's active
        // thread must already be saved, so that two vCPUs accessing each other's thread do not wait for each other,
        // and the caller must reload it from the saved registers afterwards, since other vCPUs may read and write them
        // during the wait. Requires the kernel lock, which it releases while waiting. Returns false if the emulator
        // stops first, in which case the thread's registers must not be accessed.
        [[nodiscard]] bool wait_for_thread_registers(vcpu_context& vcpu, emulator_thread& thread);

      private:
        void send_window_surface(hwnd window, const window_surface_update& update, const std::string& inline_pixels);
        void relay_window_surface(const pipe_ipc_message& message);
        bool save_dispatching_thread(vcpu_context* vcpu);
        void reload_dispatching_thread(vcpu_context& vcpu) const;
        void set_foreground_window(hwnd window);
        void resize_window_from_host(hwnd window_handle, bool maximized, int32_t client_width, int32_t client_height);
        void move_window_from_host(hwnd window_handle, int32_t x, int32_t y);

        void report_debugger_process_start();
        bool reports_module_to_debugger(const mapped_module& mod) const;
        std::pair<int32_t, int32_t> cursor_after_pointer_event(const ui_event& event) const;
        std::pair<emulator_thread*, const user_low_level_hook*> find_low_level_hook(uint64_t handle);
        void wake_thread(emulator_thread& thread);

        bool use_relative_time_{false}; // TODO: Get rid of that
        bool instruction_precision_{true};
        bool intercept_rdtsc_{true};
        bool intercept_kusd_{true};
        bool spoof_gpu_features_{false};
        bool elevated_{false};
        uint32_t vcpu_count_{1};
        std::atomic_bool suspend_requested_{false};
        std::atomic_bool should_stop{false};

        // The emulator kernel lock: held by all code touching shared kernel state,
        // released while guest code executes (docs/multi-vcpu-design.md, section 7).
        kernel_lock kernel_lock_{};
        // Notified under the kernel lock whenever a thread's registers become saved or leave transit.
        std::condition_variable_any thread_registers_saved_{};

        void credit_idle_wakeup();
        bool wait_for_idle_wakeup(std::chrono::milliseconds timeout);

        // The vCPU currently running a handler under the kernel lock; drives
        // current_thread(). See scoped_dispatch.
        vcpu_context* dispatch_vcpu_{};

        std::array<exception_trace_entry, 32> exception_trace_{};
        size_t exception_trace_index_{0};

        // unique_ptr because vcpu_context contains an atomic and must stay address-stable.
        std::vector<std::unique_ptr<vcpu_context>> vcpus_{};

        vcpu_context& active_vcpu() const
        {
            return this->dispatch_vcpu_ ? *this->dispatch_vcpu_ : *this->vcpus_[0];
        }

        std::unordered_map<uint16_t, uint16_t> port_mappings_{};

        std::vector<std::byte> process_snapshot_{};
        // std::optional<process_context> process_snapshot_{};

        stop_reason last_stop_reason_{stop_reason::none};
        std::string last_stop_detail_{};

        std::map<uint64_t, std::vector<emulator_hook*>> section_first_execution_hooks_{};

        void setup_hooks();
        void setup_x64_hooks(x86_64_emulator& emu);
        void setup_arm64_hooks(arm64_emulator& emu);
        void handle_arm64_breakpoint(vcpu_context& vcpu);
        void setup_process();
        void vcpu_worker(vcpu_context& vcpu);

        // See vcpu_worker_thread_trampoline's doc comment (windows_emulator.cpp).
        struct vcpu_worker_thread_args
        {
            windows_emulator* self;
            uint32_t index;
            std::atomic<uint32_t>* active_workers;
        };

        static void* vcpu_worker_thread_trampoline(void* raw_args);
        void on_instruction_execution(vcpu_context& vcpu, uint64_t address);
        void on_basic_block_execution(vcpu_context& vcpu, const basic_block& block);
        void try_warm_kernelbase_nls_cache(vcpu_context& vcpu, uint64_t address);
        void arm_kernelbase_nls_cache_breakpoint(uint64_t address);
        void disarm_kernelbase_nls_cache_breakpoint();
        bool try_warm_kernelbase_nls_cache_breakpoint(vcpu_context& vcpu, uint64_t address);
        void begin_kernelbase_nls_cache_warmup(vcpu_context& vcpu);

        void arm_rtl_query_performance_counter_trap(uint64_t address);
        bool try_service_rtl_query_performance_counter(vcpu_context& vcpu, uint64_t address) const;

        bool uses_section_first_execution_hooks() const;
        void clear_section_first_execution_hooks();
        void install_section_first_execution_hook(const mapped_module& mod, size_t section_index);
        void install_section_first_execution_hooks();
        void track_section_first_execution(uint64_t address);

        void register_factories(utils::buffer_deserializer& buffer);
        void ensure_serialization_support() const;
    };

} // namespace sogen
