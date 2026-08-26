#include "emulation_test_utils.hpp"

#include <syscall_utils.hpp>
#include <process_control_server.hpp>

namespace sogen::syscalls
{
    NTSTATUS handle_NtTerminateProcess(const syscall_context& c, handle process_handle, NTSTATUS exit_status);
    NTSTATUS handle_NtQueryInformationProcess(const syscall_context& c, handle process_handle, uint32_t info_class,
                                              uint64_t process_information, uint32_t process_information_length,
                                              emulator_object<uint32_t> return_length);
    NTSTATUS handle_NtDuplicateObject(const syscall_context& c, handle source_process_handle, handle source_handle,
                                      handle target_process_handle, emulator_object<handle> target_handle, ACCESS_MASK desired_access,
                                      ULONG handle_attributes, ULONG options);
}

namespace sogen::test
{
    namespace
    {
        class fake_control_channel final : public process_control_channel
        {
          public:
            std::optional<process_control_response> request(const process_control_request&, int /*timeout_ms*/) override
            {
                return std::nullopt;
            }

            std::optional<process_control_request> try_receive() override
            {
                return std::nullopt;
            }

            void respond(const process_control_response&) override
            {
            }
        };

        constexpr ACCESS_MASK PROCESS_TERMINATE = 0x0001;
        constexpr ACCESS_MASK PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;
        constexpr ACCESS_MASK PROCESS_ALL_ACCESS = 0x001FFFFF;

        syscall_context make_context(windows_emulator& emu)
        {
            return syscall_context{emu, emu.vcpu(0).cpu, emu.vcpu(0), emu.process};
        }

        // A real, reusable test double for process_control_channel: request() plays both transport
        // roles synchronously in-process by invoking execute_process_control_request directly against
        // peer, rather than mocking the executor itself. Mirrors the real fd_process_control_channel's
        // own contract (child_process_spawn.cpp): it assigns its own monotonic request_id per call and
        // poisons itself (permanently returning nullopt) the moment a response's request_id doesn't
        // match what was just sent.
        class loopback_process_control_channel final : public process_control_channel
        {
          public:
            explicit loopback_process_control_channel(windows_emulator& peer)
                : peer_(&peer)
            {
            }

            std::optional<process_control_response> request(const process_control_request& request, int /*timeout_ms*/) override
            {
                if (this->dead_)
                {
                    return std::nullopt;
                }

                process_control_request sent = request;
                sent.request_id = this->next_request_id_++;

                process_control_response response{};
                execute_process_control_request(*this->peer_, sent, response);

                if (response.request_id != sent.request_id)
                {
                    this->dead_ = true;
                    return std::nullopt;
                }

                return response;
            }

            std::optional<process_control_request> try_receive() override
            {
                return std::nullopt;
            }

            void respond(const process_control_response&) override
            {
            }

          private:
            windows_emulator* peer_{};
            uint64_t next_request_id_{1};
            bool dead_{false};
        };
    }

    // Verifies the child's own, genuinely separate windows_emulator instance actually stops - the same
    // proof standard Task 3's TerminateStopsTarget already established for execute_terminate itself, now
    // applied at the syscall-handler entry point. This codebase has no test tooling that spawns and
    // PID-checks a real host subprocess (that only happens through the analyzer's own
    // spawn_child_process); the loopback two-instance pattern is the established substitute for that
    // proof throughout Tasks 6-7 and is used identically here.
    TEST(CrossProcessTest, NtTerminateProcessSyscallResolvesChildAndTerminates)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.pid = 0x1234;
        record.exit_status = STATUS_PENDING;
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtTerminateProcess(c, h, 0x1234);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_TERMINATED_WITH_STATUS(child, 0x1234);
        ASSERT_EQ(parent.process.child_processes.at(7).exit_status, 0x1234);
        ASSERT_EQ(parent.find_child_control_channel(7), nullptr);
    }

    TEST(CrossProcessTest, NtTerminateProcessSyscallDeniesAccessWithoutTerminateRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_QUERY_LIMITED_INFORMATION; // deliberately lacks PROCESS_TERMINATE (0x0001)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtTerminateProcess(c, h, 0);

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
        ASSERT_NOT_TERMINATED(child);
    }

    // fake_control_channel's request() always returns nullopt, simulating a dead/unresponsive channel.
    // The parent's own record must be left untouched (still STATUS_PENDING) since nothing confirms the
    // requested exit code was ever actually observed by the target.
    TEST(CrossProcessTest, NtTerminateProcessSyscallReportsProcessIsTerminatingOnDeadChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        record.exit_status = STATUS_PENDING;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtTerminateProcess(c, h, 0x1234);

        ASSERT_EQ(status, STATUS_PROCESS_IS_TERMINATING);
        ASSERT_EQ(parent.process.child_processes.at(7).exit_status, STATUS_PENDING);
    }

    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReturnsBasicInformationForLiveChild)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.pid = 0x4242;
        record.exit_status = STATUS_PENDING;
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_out, 0u);
        const emulator_object<PROCESS_BASIC_INFORMATION64> info{parent.memory, info_out};

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessBasicInformation, info_out, sizeof(PROCESS_BASIC_INFORMATION64), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);
        const auto value = info.read();
        ASSERT_EQ(value.UniqueProcessId, 0x4242u);
        ASSERT_EQ(value.ExitStatus, STATUS_PENDING);
    }

    // The critical case this task exists for: real Windows answers ProcessBasicInformation
    // (GetExitCodeProcess's underlying primitive) even after a child has exited, and this must too - no
    // control channel is registered at all here, mirroring what handle_NtTerminateProcess leaves behind
    // via drop_child_control_channel. Must NOT return STATUS_NOT_SUPPORTED.
    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReturnsBasicInformationForTerminatedChildWithoutLiveChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.pid = 0x4242;
        record.exit_status = 0x1234;
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        ASSERT_EQ(parent.find_child_control_channel(7), nullptr);

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_out, 0u);
        const emulator_object<PROCESS_BASIC_INFORMATION64> info{parent.memory, info_out};

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessBasicInformation, info_out, sizeof(PROCESS_BASIC_INFORMATION64), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);
        const auto value = info.read();
        ASSERT_EQ(value.UniqueProcessId, 0x4242u);
        ASSERT_EQ(value.ExitStatus, 0x1234);
    }

    TEST(CrossProcessTest, NtQueryInformationProcessSyscallSupportsExtendedBasicInformationForTerminatedChild)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.pid = 0x55;
        record.exit_status = 0xC0000005; // STATUS_ACCESS_VIOLATION, as an arbitrary crash exit code
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_out, 0u);
        const emulator_object<PROCESS_EXTENDED_BASIC_INFORMATION> info{parent.memory, info_out};

        const auto status =
            syscalls::handle_NtQueryInformationProcess(c, h, ProcessBasicInformation, info_out, sizeof(PROCESS_EXTENDED_BASIC_INFORMATION),
                                                       emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);
        const auto value = info.read();
        ASSERT_EQ(value.Size, sizeof(PROCESS_EXTENDED_BASIC_INFORMATION));
        ASSERT_EQ(value.BasicInfo.UniqueProcessId, 0x55u);
        ASSERT_EQ(value.BasicInfo.ExitStatus, static_cast<NTSTATUS>(0xC0000005));
    }

    TEST(CrossProcessTest, NtQueryInformationProcessSyscallDeniesAccessForChildWithoutQueryRight)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_TERMINATE; // deliberately lacks PROCESS_QUERY_LIMITED_INFORMATION (0x1000)
        parent.process.child_processes[7] = record;

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessBasicInformation, 0, sizeof(PROCESS_BASIC_INFORMATION64), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
    }

    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReportsInvalidHandleForUnknownChild)
    {
        auto parent = create_empty_emulator();
        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessBasicInformation, 0, sizeof(PROCESS_BASIC_INFORMATION64), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_INVALID_HANDLE);
    }

    // Chromium's base::ProcessMetrics::GetCumulativeCPUUsage calls QueryProcessCycleTime against a
    // handle to a spawned child (gpu-process, renderer, ...), not just its own process - this is the
    // cross-process round trip that answers it, mirroring ProcessWow64Information's own
    // resolve_child_target/process_control_channel pattern just above.
    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReturnsCycleTimeForLiveChild)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_out, 0u);
        const emulator_object<PROCESS_CYCLE_TIME_INFORMATION> info{parent.memory, info_out};

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessCycleTime, info_out, sizeof(PROCESS_CYCLE_TIME_INFORMATION), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);
        const auto value = info.read();
        ASSERT_EQ(value.AccumulatedCycles, child.get_executed_instructions());
        ASSERT_EQ(value.CurrentCycleCount, 0u);
    }

    // Like ProcessWow64Information just above, a resolve_child_record failure of any kind (access
    // denied here) falls through to this function's own generic STATUS_NOT_SUPPORTED rather than
    // propagating the specific NTSTATUS - this asserts the access check genuinely runs (a child with a
    // live channel but insufficient granted_access must not read AccumulatedCycles from it), not just
    // that the end-to-end status happens to match.
    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReturnsNotSupportedForCycleTimeWithoutQueryRight)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_TERMINATE; // deliberately lacks PROCESS_QUERY_LIMITED_INFORMATION (0x1000)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtQueryInformationProcess(c, h, ProcessCycleTime, 0, sizeof(PROCESS_CYCLE_TIME_INFORMATION),
                                                                       emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_NOT_SUPPORTED);
    }

    // fake_control_channel's request() always returns nullopt, simulating a dead/unresponsive channel -
    // the child is otherwise fully authorized, distinguishing this from the access-denied case above.
    // Unlike ProcessWow64Information's sibling branch, this must still succeed (with a zeroed value)
    // rather than falling through to STATUS_NOT_SUPPORTED: real Chromium's own QueryProcessCycleTime
    // call site (base::ProcessMetrics::GetCumulativeCPUUsage) NOTREACHED()s on any failure, and most of
    // the short-lived children this queries in practice (failed gpu-process attempts, etc.) have
    // already self-exited by the time this fires, leaving the channel dead with no way for this parent
    // to have captured a real final value - this is the exact scenario a live SolidWorksSetup.exe/
    // WebView2 repro hit.
    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReturnsZeroedCycleTimeOnDeadChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_out, 0u);
        const emulator_object<PROCESS_CYCLE_TIME_INFORMATION> info{parent.memory, info_out};

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessCycleTime, info_out, sizeof(PROCESS_CYCLE_TIME_INFORMATION), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);
        const auto value = info.read();
        ASSERT_EQ(value.AccumulatedCycles, 0u);
        ASSERT_EQ(value.CurrentCycleCount, 0u);
    }

    // The other real-world case a self-exited child leaves behind: no control channel was ever
    // registered at all (e.g. handle_NtTerminateProcess already dropped it after a parent-initiated
    // terminate, or - per resolve_child_record's own docs - a snapshot-restored process whose child
    // channels were never serialized). Must behave identically to the dead-channel case above, since
    // both represent "this parent knows the child but cannot reach it right now".
    TEST(CrossProcessTest, NtQueryInformationProcessSyscallReturnsZeroedCycleTimeWithoutLiveChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        ASSERT_EQ(parent.find_child_control_channel(7), nullptr);

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_out, 0u);
        const emulator_object<PROCESS_CYCLE_TIME_INFORMATION> info{parent.memory, info_out};

        const auto status = syscalls::handle_NtQueryInformationProcess(
            c, h, ProcessCycleTime, info_out, sizeof(PROCESS_CYCLE_TIME_INFORMATION), emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);
        const auto value = info.read();
        ASSERT_EQ(value.AccumulatedCycles, 0u);
        ASSERT_EQ(value.CurrentCycleCount, 0u);
    }

    namespace
    {
        section make_pagefile_section(const std::vector<std::byte>& content, const ACCESS_MASK granted_access)
        {
            return section::from_pagefile_backing(content.size(), PAGE_READWRITE, SEC_COMMIT, granted_access, content);
        }
    }

    TEST(CrossProcessTest, NtDuplicateObjectSyscallDuplicatesPagefileSectionIntoChild)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const std::vector<std::byte> content = {std::byte{'h'}, std::byte{'i'}, std::byte{'!'}};
        const auto source_handle = parent.process.sections.store(make_pagefile_section(content, 0x000F001F));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto target_process = make_pseudo_handle(7, handle_types::process);

        const auto handle_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(handle_out, 0u);
        const emulator_object<handle> target_handle{parent.memory, handle_out};

        const auto status = syscalls::handle_NtDuplicateObject(c, CURRENT_PROCESS, source_handle, target_process, target_handle, 0, 0,
                                                               DUPLICATE_SAME_ACCESS);

        ASSERT_EQ(status, STATUS_SUCCESS);

        const auto minted = target_handle.read();
        ASSERT_NE(minted.bits, 0u);

        auto* const child_section = child.process.sections.get(minted);
        ASSERT_NE(child_section, nullptr);
        ASSERT_EQ(child_section->object->backing_storage, content);
        ASSERT_EQ(child_section->object->maximum_size, content.size());
        ASSERT_EQ(child_section->granted_access, static_cast<ACCESS_MASK>(0x000F001F));
    }

    TEST(CrossProcessTest, NtDuplicateObjectSyscallRejectsImageBackedSection)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        auto image_section = make_pagefile_section({std::byte{1}}, 0x000F001F);
        image_section.object->allocation_attributes = SEC_IMAGE;
        const auto source_handle = parent.process.sections.store(std::move(image_section));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto target_process = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtDuplicateObject(c, CURRENT_PROCESS, source_handle, target_process,
                                                               emulator_object<handle>{parent.memory, 0}, 0, 0, DUPLICATE_SAME_ACCESS);

        ASSERT_EQ(status, STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, NtDuplicateObjectSyscallRejectsFileBackedSection)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        auto file_section = make_pagefile_section({std::byte{1}}, 0x000F001F);
        file_section.object->file_name = u"C:\\some\\file.dll";
        const auto source_handle = parent.process.sections.store(std::move(file_section));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto target_process = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtDuplicateObject(c, CURRENT_PROCESS, source_handle, target_process,
                                                               emulator_object<handle>{parent.memory, 0}, 0, 0, DUPLICATE_SAME_ACCESS);

        ASSERT_EQ(status, STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, NtDuplicateObjectSyscallRejectsNonSectionSource)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto source_handle = parent.process.events.store({});

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto target_process = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtDuplicateObject(c, CURRENT_PROCESS, source_handle, target_process,
                                                               emulator_object<handle>{parent.memory, 0}, 0, 0, DUPLICATE_SAME_ACCESS);

        ASSERT_EQ(status, STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, NtDuplicateObjectSyscallRejectsNonCurrentSourceProcess)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto source_handle = parent.process.sections.store(make_pagefile_section({std::byte{1}}, 0x000F001F));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto target_process = make_pseudo_handle(7, handle_types::process);

        const auto status = syscalls::handle_NtDuplicateObject(c, NULL_HANDLE, source_handle, target_process,
                                                               emulator_object<handle>{parent.memory, 0}, 0, 0, DUPLICATE_SAME_ACCESS);

        ASSERT_EQ(status, STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, NtDuplicateObjectSyscallRejectsNonChildTargetProcess)
    {
        auto parent = create_empty_emulator();

        const auto source_handle = parent.process.sections.store(make_pagefile_section({std::byte{1}}, 0x000F001F));

        const auto c = make_context(parent);

        const auto status = syscalls::handle_NtDuplicateObject(c, CURRENT_PROCESS, source_handle, NULL_HANDLE,
                                                               emulator_object<handle>{parent.memory, 0}, 0, 0, DUPLICATE_SAME_ACCESS);

        ASSERT_EQ(status, STATUS_NOT_SUPPORTED);
    }
} // namespace sogen::test
