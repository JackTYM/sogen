#include "emulation_test_utils.hpp"

#include <syscall_utils.hpp>
#include <process_control_server.hpp>

namespace sogen::syscalls
{
    NTSTATUS handle_NtResumeThread(const syscall_context& c, handle thread_handle, emulator_object<ULONG> previous_suspend_count);
    NTSTATUS handle_NtReadVirtualMemory(const syscall_context& c, handle process_handle, uint64_t base_address, uint64_t buffer,
                                        ULONG number_of_bytes_to_read, emulator_object<ULONG> number_of_bytes_read);
    NTSTATUS handle_NtWriteVirtualMemory(const syscall_context& c, handle process_handle, uint64_t base_address, uint64_t buffer,
                                         ULONG number_of_bytes_to_write, emulator_object<ULONG> number_of_bytes_write);
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

    // Handler-level cross-instance proof (same two-genuinely-separate-windows_emulator-instances
    // pattern as ReadMemoryRoundTrips): the bytes landed in parent's own guest memory can only have
    // come from child's address space via the control channel, never from parent's own freshly
    // allocated (zeroed) memory.
    TEST(CrossProcessTest, NtReadVirtualMemorySyscallResolvesChildAndReadsRemoteBytes)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        std::vector<std::byte> pattern(0x40);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(0x30 + i);
        }
        ASSERT_TRUE(child.memory.try_write_memory(child_base, pattern.data(), pattern.size()));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto local_buffer = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_read{parent.memory, out_address};

        const auto status =
            syscalls::handle_NtReadVirtualMemory(c, h, child_base, local_buffer, static_cast<ULONG>(pattern.size()), number_of_bytes_read);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(number_of_bytes_read.read(), pattern.size());

        std::vector<std::byte> actual(pattern.size());
        ASSERT_TRUE(parent.memory.try_read_memory(local_buffer, actual.data(), actual.size()));
        ASSERT_EQ(actual, pattern);
    }

    // Two genuinely separate instances again: the write goes through the channel to child's address
    // space only, proving the requester-reads-local/sends-to-target direction is wired correctly, not
    // reversed.
    TEST(CrossProcessTest, NtWriteVirtualMemorySyscallResolvesChildAndWritesRemoteBytes)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        const auto local_buffer = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        std::vector<std::byte> pattern(0x40);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(0x70 + i);
        }
        ASSERT_TRUE(parent.memory.try_write_memory(local_buffer, pattern.data(), pattern.size()));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_written{parent.memory, out_address};

        const auto status = syscalls::handle_NtWriteVirtualMemory(c, h, child_base, local_buffer, static_cast<ULONG>(pattern.size()),
                                                                  number_of_bytes_written);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(number_of_bytes_written.read(), pattern.size());

        std::vector<std::byte> actual(pattern.size());
        ASSERT_TRUE(child.memory.try_read_memory(child_base, actual.data(), actual.size()));
        ASSERT_EQ(actual, pattern);
    }

    TEST(CrossProcessTest, NtReadVirtualMemorySyscallDeniesAccessWithoutVmReadRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0; // deliberately lacks PROCESS_VM_READ (0x0010)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_read{parent.memory, out_address};
        number_of_bytes_read.write(0xDEADBEEF);

        const auto status = syscalls::handle_NtReadVirtualMemory(c, h, 0x1000, 0x2000, 0x10, number_of_bytes_read);

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
        ASSERT_EQ(number_of_bytes_read.read(), 0u);
    }

    TEST(CrossProcessTest, NtWriteVirtualMemorySyscallDeniesAccessWithoutVmWriteRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_VM_READ; // deliberately lacks PROCESS_VM_WRITE|PROCESS_VM_OPERATION (0x0020|0x0008)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_written{parent.memory, out_address};
        number_of_bytes_written.write(0xDEADBEEF);

        const auto status = syscalls::handle_NtWriteVirtualMemory(c, h, 0x1000, 0x2000, 0x10, number_of_bytes_written);

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
        ASSERT_EQ(number_of_bytes_written.read(), 0u);
    }

    TEST(CrossProcessTest, NtReadVirtualMemorySyscallReportsProcessIsTerminatingOnDeadChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto local_buffer = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_read{parent.memory, out_address};
        number_of_bytes_read.write(0xDEADBEEF);

        const auto status = syscalls::handle_NtReadVirtualMemory(c, h, 0x1000, local_buffer, 0x10, number_of_bytes_read);

        ASSERT_EQ(status, STATUS_PROCESS_IS_TERMINATING);
        ASSERT_EQ(number_of_bytes_read.read(), 0u);
    }

    TEST(CrossProcessTest, NtWriteVirtualMemorySyscallReportsProcessIsTerminatingOnDeadChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto local_buffer = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);
        std::vector<std::byte> pattern(0x10, std::byte{0x55});
        ASSERT_TRUE(parent.memory.try_write_memory(local_buffer, pattern.data(), pattern.size()));

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_written{parent.memory, out_address};
        number_of_bytes_written.write(0xDEADBEEF);

        const auto status =
            syscalls::handle_NtWriteVirtualMemory(c, h, 0x1000, local_buffer, static_cast<ULONG>(pattern.size()), number_of_bytes_written);

        ASSERT_EQ(status, STATUS_PROCESS_IS_TERMINATING);
        ASSERT_EQ(number_of_bytes_written.read(), 0u);
    }

    // child_base has only one committed page; the request spans two, so the second page is unmapped in
    // the TARGET's own address space - proving the partial-transfer status/byte-count and the
    // min(target-produced, locally-writable) composition are both reported correctly, not just the
    // whole-request-succeeds path.
    TEST(CrossProcessTest, NtReadVirtualMemorySyscallReportsPartialCopyWhenTargetMemoryPartiallyUnmapped)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        std::vector<std::byte> pattern(0x1000);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(i % 251);
        }
        ASSERT_TRUE(child.memory.try_write_memory(child_base, pattern.data(), pattern.size()));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto local_buffer = parent.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_read{parent.memory, out_address};

        const auto status = syscalls::handle_NtReadVirtualMemory(c, h, child_base, local_buffer, 0x2000, number_of_bytes_read);

        ASSERT_EQ(status, STATUS_PARTIAL_COPY);
        ASSERT_EQ(number_of_bytes_read.read(), 0x1000u);

        std::vector<std::byte> actual(pattern.size());
        ASSERT_TRUE(parent.memory.try_read_memory(local_buffer, actual.data(), actual.size()));
        ASSERT_EQ(actual, pattern);
    }

    // Mirror of the read case: local_buffer has only one committed page, so the LOCAL source is what's
    // partially unmapped this time - proving the requester sends only what it actually read (never
    // garbage or short-read padding) and the resulting partial status is still reported against the
    // full originally-requested size.
    TEST(CrossProcessTest, NtWriteVirtualMemorySyscallReportsPartialCopyWhenLocalSourceIsPartiallyUnmapped)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);
        std::vector<std::byte> sentinel(0x2000, std::byte{0xEE});
        ASSERT_TRUE(child.memory.try_write_memory(child_base, sentinel.data(), sentinel.size()));

        const auto local_buffer = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);
        std::vector<std::byte> pattern(0x1000);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(i % 200);
        }
        ASSERT_TRUE(parent.memory.try_write_memory(local_buffer, pattern.data(), pattern.size()));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_written{parent.memory, out_address};

        const auto status = syscalls::handle_NtWriteVirtualMemory(c, h, child_base, local_buffer, 0x2000, number_of_bytes_written);

        ASSERT_EQ(status, STATUS_PARTIAL_COPY);
        ASSERT_EQ(number_of_bytes_written.read(), 0x1000u);

        std::vector<std::byte> actual(pattern.size());
        ASSERT_TRUE(child.memory.try_read_memory(child_base, actual.data(), actual.size()));
        ASSERT_EQ(actual, pattern);

        std::vector<std::byte> actual_tail(0x1000);
        ASSERT_TRUE(child.memory.try_read_memory(child_base + 0x1000, actual_tail.data(), actual_tail.size()));
        const std::vector<std::byte> expected_tail(0x1000, std::byte{0xEE});
        ASSERT_EQ(actual_tail, expected_tail);
    }

    // Mirror of the target-partial case above: this time the TARGET fully produces the requested
    // range, but the LOCAL destination buffer only has its first page mapped - exercising the other
    // arm of min(target-produced, locally-writable), where the local side (not the target) is what
    // limits the transferred count.
    TEST(CrossProcessTest, NtReadVirtualMemorySyscallReportsPartialCopyWhenLocalDestinationIsPartiallyUnmapped)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        std::vector<std::byte> pattern(0x2000);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(i % 251);
        }
        ASSERT_TRUE(child.memory.try_write_memory(child_base, pattern.data(), pattern.size()));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto local_buffer = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_read{parent.memory, out_address};

        const auto status = syscalls::handle_NtReadVirtualMemory(c, h, child_base, local_buffer, 0x2000, number_of_bytes_read);

        ASSERT_EQ(status, STATUS_PARTIAL_COPY);
        ASSERT_EQ(number_of_bytes_read.read(), 0x1000u);

        std::vector<std::byte> actual(0x1000);
        ASSERT_TRUE(parent.memory.try_read_memory(local_buffer, actual.data(), actual.size()));
        const std::vector<std::byte> expected(pattern.begin(), pattern.begin() + 0x1000);
        ASSERT_EQ(actual, expected);
    }

    // Spans three of perform_chunked_remote_transfer's 1 MiB chunks (memory.cpp), each executed as a
    // separate control-channel round trip - proving the multi-chunk loop reassembles a large transfer
    // correctly, not just single-chunk requests under the cap. The pattern embeds the chunk index
    // (rather than repeating every 256 bytes, which exactly divides the 1 MiB chunk size and so would
    // let a wrong-remote-offset-per-chunk bug fetch content that coincidentally matches), so a chunk
    // fetched from the wrong remote offset produces visibly wrong bytes instead of passing by
    // coincidence.
    TEST(CrossProcessTest, NtReadVirtualMemorySyscallChunksTransfersLargerThanOneMebibyte)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        constexpr size_t chunk_size = 1u << 20;
        constexpr size_t total_size = 3u * chunk_size;

        const auto child_base = child.memory.allocate_memory(total_size, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        std::vector<std::byte> pattern(total_size);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            const auto chunk_index = i / chunk_size;
            pattern[i] = static_cast<std::byte>((chunk_index * 37 + i % chunk_size) & 0xFF);
        }
        ASSERT_TRUE(child.memory.try_write_memory(child_base, pattern.data(), pattern.size()));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto local_buffer = parent.memory.allocate_memory(total_size, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        const auto out_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(out_address, 0u);
        const emulator_object<ULONG> number_of_bytes_read{parent.memory, out_address};

        const auto status =
            syscalls::handle_NtReadVirtualMemory(c, h, child_base, local_buffer, static_cast<ULONG>(total_size), number_of_bytes_read);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(number_of_bytes_read.read(), total_size);

        std::vector<std::byte> actual(total_size);
        ASSERT_TRUE(parent.memory.try_read_memory(local_buffer, actual.data(), actual.size()));
        ASSERT_EQ(actual, pattern);
    }

} // namespace sogen::test
