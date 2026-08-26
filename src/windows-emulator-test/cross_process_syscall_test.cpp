#include "emulation_test_utils.hpp"

#include <syscall_utils.hpp>
#include <process_control_server.hpp>

namespace sogen::syscalls
{
    NTSTATUS handle_NtResumeThread(const syscall_context& c, handle thread_handle, emulator_object<ULONG> previous_suspend_count);
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

        constexpr ACCESS_MASK PROCESS_VM_READ = 0x0010;
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

    // Exercises handle_NtResumeThread itself (thread.cpp), not just the executor - the actual guest
    // syscall entry point that resolves the child handle, round-trips the control request, and writes
    // the out-param back into the requester's own guest memory.
    TEST(CrossProcessTest, NtResumeThreadSyscallResolvesChildAndWritesPreviousSuspendCount)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_thread_handle =
            child.process.create_thread(child.memory, 0x1000, 0, 0x1000, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, true);
        auto* const child_thread = child.process.threads.get(child_thread_handle);
        ASSERT_NE(child_thread, nullptr);
        ASSERT_EQ(child_thread->suspended, 1u);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::thread);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> previous_suspend_count{parent.memory, out_address};

        const auto status = syscalls::handle_NtResumeThread(c, h, previous_suspend_count);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(previous_suspend_count.read(), 1u);
        ASSERT_EQ(child_thread->suspended, 0u);
    }

    TEST(CrossProcessTest, NtResumeThreadSyscallDeniesAccessWithoutSuspendResumeRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_VM_READ; // deliberately lacks PROCESS_SUSPEND_RESUME (0x0800)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::thread);

        const auto status = syscalls::handle_NtResumeThread(c, h, emulator_object<ULONG>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
    }

    // fake_control_channel's request() always returns nullopt, simulating a dead/unresponsive channel
    // (distinct from resolve_child_target's own STATUS_NOT_SUPPORTED "no channel registered" case,
    // already covered by KnownChildWithoutLiveChannelIsNotSupported) - this is the path a real
    // fd_process_control_channel takes once send_framed/recv_framed fails.
    TEST(CrossProcessTest, NtResumeThreadSyscallReportsProcessIsTerminatingOnDeadChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::thread);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> previous_suspend_count{parent.memory, out_address};
        previous_suspend_count.write(0xDEADBEEF);

        const auto status = syscalls::handle_NtResumeThread(c, h, previous_suspend_count);

        ASSERT_EQ(status, STATUS_PROCESS_IS_TERMINATING);
        ASSERT_EQ(previous_suspend_count.read(), 0xDEADBEEFu);
    }
} // namespace sogen::test
