#include "emulation_test_utils.hpp"

#include <syscall_utils.hpp>
#include <process_control_server.hpp>

namespace sogen::syscalls
{
    NTSTATUS handle_NtReadVirtualMemory(const syscall_context& c, handle process_handle, uint64_t base_address, uint64_t buffer,
                                        ULONG number_of_bytes_to_read, emulator_object<ULONG> number_of_bytes_read);
    NTSTATUS handle_NtWriteVirtualMemory(const syscall_context& c, handle process_handle, uint64_t base_address, uint64_t buffer,
                                         ULONG number_of_bytes_to_write, emulator_object<ULONG> number_of_bytes_write);
    NTSTATUS handle_NtAllocateVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                            uint64_t zero_bits, emulator_object<uint64_t> bytes_to_allocate, uint32_t allocation_type,
                                            uint32_t page_protection);
    NTSTATUS handle_NtProtectVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                           emulator_object<uint32_t> bytes_to_protect, uint32_t protection,
                                           emulator_object<uint32_t> old_protection);
    NTSTATUS handle_NtFreeVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                        emulator_object<uint64_t> bytes_to_allocate, uint32_t free_type);
    NTSTATUS handle_NtQueryVirtualMemory(const syscall_context& c, handle process_handle, uint64_t base_address, uint32_t info_class,
                                         uint64_t memory_information, uint64_t memory_information_length,
                                         emulator_object<uint64_t> return_length);
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

    // Mirror of NtReadVirtualMemorySyscallReportsPartialCopyWhenTargetMemoryPartiallyUnmapped: the
    // LOCAL source is fully readable (the requester reads everything it means to send, so
    // request.payload.size() equals the full requested chunk), but the TARGET's own destination
    // memory is only partially committed - proving number_of_bytes_write reflects the target's actual
    // reported bytes_written, not how much the requester successfully read locally.
    TEST(CrossProcessTest, NtWriteVirtualMemorySyscallReportsPartialCopyWhenTargetMemoryPartiallyUnmapped)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        const auto local_buffer = parent.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);
        std::vector<std::byte> pattern(0x2000);
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

        std::vector<std::byte> actual(0x1000);
        ASSERT_TRUE(child.memory.try_read_memory(child_base, actual.data(), actual.size()));
        const std::vector<std::byte> expected(pattern.begin(), pattern.begin() + 0x1000);
        ASSERT_EQ(actual, expected);
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

    // Mirror of the read direction's multi-chunk test: spans three 1 MiB chunks in the write
    // direction, each a separate control-channel round trip, using the same chunk-index-dependent
    // pattern so a wrong-remote-offset-per-chunk bug can't pass by coincidence.
    TEST(CrossProcessTest, NtWriteVirtualMemorySyscallChunksTransfersLargerThanOneMebibyte)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        constexpr size_t chunk_size = 1u << 20;
        constexpr size_t total_size = 3u * chunk_size;

        const auto child_base = child.memory.allocate_memory(total_size, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        const auto local_buffer = parent.memory.allocate_memory(total_size, memory_permission::read_write);
        ASSERT_NE(local_buffer, 0u);

        std::vector<std::byte> pattern(total_size);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            const auto chunk_index = i / chunk_size;
            pattern[i] = static_cast<std::byte>((chunk_index * 37 + i % chunk_size) & 0xFF);
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

        const auto status =
            syscalls::handle_NtWriteVirtualMemory(c, h, child_base, local_buffer, static_cast<ULONG>(total_size), number_of_bytes_written);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(number_of_bytes_written.read(), total_size);

        std::vector<std::byte> actual(total_size);
        ASSERT_TRUE(child.memory.try_read_memory(child_base, actual.data(), actual.size()));
        ASSERT_EQ(actual, pattern);
    }

    // The returned base can only have come from CHILD's own address-space picker (find_free_host_
    // allocation_base against child.memory, driven through execute_allocate_memory's size-only
    // allocate_memory overload) - proving base_address==0 auto-placement is resolved against the
    // remote target, not the requester's own memory manager, and that the region lands committed
    // (MEM_RESERVE|MEM_COMMIT) with the requested permissions in CHILD's address space.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteAllocatesWithZeroBase)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x2000);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_NE(base_address.read(), 0u);
        ASSERT_EQ(bytes_to_allocate.read(), 0x2000u);

        const auto region_info = child.memory.get_region_info(base_address.read());
        ASSERT_TRUE(region_info.is_committed);
        ASSERT_EQ(region_info.allocation_length, 0x2000u);
        ASSERT_EQ(map_emulator_to_nt_protection(region_info.permissions), static_cast<uint32_t>(PAGE_READWRITE));
    }

    // Mirror of the zero-base case: the requested (non-zero) base is honoured exactly, at an address
    // picked via CHILD's own find_free_allocation_base so the test doesn't hardcode an address that
    // might collide with the emulator's default layout.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteAllocatesAtRequestedBase)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto candidate_base = child.memory.find_free_allocation_base(0x1000);
        ASSERT_NE(candidate_base, 0u);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(candidate_base);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x1000);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(base_address.read(), candidate_base);
        ASSERT_EQ(bytes_to_allocate.read(), 0x1000u);

        const auto region_info = child.memory.get_region_info(candidate_base);
        ASSERT_TRUE(region_info.is_committed);
    }

    // Exercises the newly-implemented commit-onto-existing-reservation branch: child already has a
    // MEM_RESERVE-only region (reserve_only allocate_memory), and a remote MEM_COMMIT-only request
    // targeting that exact base must commit onto it via memory_manager::commit_memory rather than
    // falling into the fresh-reserve-and-commit path - proving execute_allocate_memory's first gap
    // (it previously always called allocate_memory, never commit_memory) is closed.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteCommitsOntoExistingReservation)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto reserved_base = child.memory.allocate_memory(0x2000, memory_permission::read_write, /*reserve_only=*/true);
        ASSERT_NE(reserved_base, 0u);
        ASSERT_FALSE(child.memory.get_region_info(reserved_base).is_committed);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(reserved_base);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x2000);

        const auto status = syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_COMMIT, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(base_address.read(), reserved_base);
        ASSERT_EQ(bytes_to_allocate.read(), 0x2000u);

        const auto region_info = child.memory.get_region_info(reserved_base);
        ASSERT_TRUE(region_info.is_committed);
        ASSERT_EQ(region_info.allocation_base, reserved_base);
    }

    // Exercises execute_allocate_memory's second newly-closed gap: a zero size is now rejected with
    // STATUS_INVALID_PARAMETER instead of silently succeeding, matching handle_NtAllocateVirtualMemoryEx's
    // own request.size == 0 check. The out-params must be left untouched, since the handler only writes
    // them back on STATUS_SUCCESS.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteRejectsZeroSize)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0xDEADBEEFu);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_INVALID_PARAMETER);
        ASSERT_EQ(base_address.read(), 0xDEADBEEFu);
        ASSERT_EQ(bytes_to_allocate.read(), 0u);
    }

    // The other newly-closed gap: an allocation_type with neither MEM_RESERVE nor MEM_COMMIT set is
    // invalid (matching handle_NtAllocateVirtualMemoryEx's !commit && !reserve check).
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteRejectsInvalidAllocationType)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x1000);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_TOP_DOWN, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_INVALID_PARAMETER);
    }

    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallDeniesAccessWithoutVmOperationRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0; // deliberately lacks PROCESS_VM_OPERATION (0x0008)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x1000);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
    }

    // Proves the shared STATUS_PROCESS_IS_TERMINATING path (already covered end-to-end for read/write)
    // also fires correctly for allocate - the one op among this task's four that has the most new logic
    // ahead of the channel round trip (validation, access-mask check), so this confirms none of that new
    // logic accidentally short-circuits the dead-channel handling.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallReportsProcessIsTerminatingOnDeadChannel)
    {
        auto parent = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0xDEADBEEFu);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x1000);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        ASSERT_EQ(status, STATUS_PROCESS_IS_TERMINATING);
        ASSERT_EQ(base_address.read(), 0xDEADBEEFu);
        ASSERT_EQ(bytes_to_allocate.read(), 0x1000u);
    }

    // Handler-level cross-instance proof: old_protection can only have come from CHILD's own region
    // (read_write, the permissions it was allocated with), and CHILD's actual permissions afterward must
    // reflect the new protection - proving both the round trip and the out-param plumbing are wired.
    TEST(CrossProcessTest, NtProtectVirtualMemorySyscallRemoteReportsOldProtection)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(child_base);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint32_t> bytes_to_protect{parent.memory, size_out};
        bytes_to_protect.write(0x1000);

        const auto old_protection_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(old_protection_out, 0u);
        const emulator_object<uint32_t> old_protection{parent.memory, old_protection_out};

        const auto status = syscalls::handle_NtProtectVirtualMemory(c, h, base_address, bytes_to_protect, PAGE_READONLY, old_protection);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(base_address.read(), child_base);
        ASSERT_EQ(bytes_to_protect.read(), 0x1000u);
        ASSERT_EQ(old_protection.read(), static_cast<uint32_t>(PAGE_READWRITE));

        const auto region_info = child.memory.get_region_info(child_base);
        ASSERT_EQ(map_emulator_to_nt_protection(region_info.permissions), static_cast<uint32_t>(PAGE_READONLY));
    }

    // Also pins down a deliberate decision: unlike the same-process path (which always writes the
    // page-aligned base/size back before doing any further validation), the remote branch resolves
    // access BEFORE reading/aligning/writing base_address/bytes_to_protect at all, so an
    // access-denied call leaves both completely untouched - the same "touch nothing on early
    // failure" contract the remote branch had before this op was wired up (when it was an
    // unconditional STATUS_NOT_SUPPORTED).
    TEST(CrossProcessTest, NtProtectVirtualMemorySyscallDeniesAccessWithoutVmOperationRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0; // deliberately lacks PROCESS_VM_OPERATION (0x0008)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0x1234); // deliberately unaligned, to prove it's untouched rather than aligned

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint32_t> bytes_to_protect{parent.memory, size_out};
        bytes_to_protect.write(0xDEADBEEF);

        const auto status = syscalls::handle_NtProtectVirtualMemory(c, h, base_address, bytes_to_protect, PAGE_READONLY,
                                                                    emulator_object<uint32_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
        ASSERT_EQ(base_address.read(), 0x1234u);
        ASSERT_EQ(bytes_to_protect.read(), 0xDEADBEEFu);
    }

    // Handler-level cross-instance proof: the release can only have taken effect if it reached CHILD's
    // memory manager - CHILD's own get_region_info no longer reports the region as reserved afterward.
    TEST(CrossProcessTest, NtFreeVirtualMemorySyscallRemoteReleasesRegion)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);
        ASSERT_TRUE(child.memory.get_region_info(child_base).is_reserved);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(child_base);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0);

        const auto status = syscalls::handle_NtFreeVirtualMemory(c, h, base_address, bytes_to_allocate, MEM_RELEASE);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(base_address.read(), child_base);
        ASSERT_EQ(bytes_to_allocate.read(), 0x1000u);
        ASSERT_FALSE(child.memory.get_region_info(child_base).is_reserved);
    }

    TEST(CrossProcessTest, NtFreeVirtualMemorySyscallDeniesAccessWithoutVmOperationRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0; // deliberately lacks PROCESS_VM_OPERATION (0x0008)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0x1000);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};

        const auto status = syscalls::handle_NtFreeVirtualMemory(c, h, base_address, bytes_to_allocate, MEM_RELEASE);

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
    }

    // Handler-level cross-instance proof: the returned MEMORY_BASIC_INFORMATION64 can only describe
    // CHILD's region (its own BaseAddress/RegionSize/permissions), landing in PARENT's own guest memory
    // at memory_information - proving the payload round trip and the write-into-requester's-memory step
    // are both wired correctly.
    TEST(CrossProcessTest, NtQueryVirtualMemorySyscallRemoteReturnsRegionInfo)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_address, 0u);

        const auto return_length_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(return_length_out, 0u);
        const emulator_object<uint64_t> return_length{parent.memory, return_length_out};

        const auto status = syscalls::handle_NtQueryVirtualMemory(c, h, child_base, MemoryBasicInformation, info_address,
                                                                  sizeof(EMU_MEMORY_BASIC_INFORMATION64), return_length);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(return_length.read(), sizeof(EMU_MEMORY_BASIC_INFORMATION64));

        const emulator_object<EMU_MEMORY_BASIC_INFORMATION64> info{parent.memory, info_address};
        const auto info_value = info.read();

        ASSERT_EQ(info_value.BaseAddress, child_base);
        ASSERT_EQ(info_value.RegionSize, static_cast<int64_t>(0x2000));
        ASSERT_EQ(info_value.State, static_cast<uint32_t>(MEM_COMMIT));
        ASSERT_EQ(info_value.Protect, static_cast<uint32_t>(PAGE_READWRITE));
    }

    TEST(CrossProcessTest, NtQueryVirtualMemorySyscallDeniesAccessWithoutQueryInformationRight)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0; // deliberately lacks PROCESS_QUERY_INFORMATION (0x0400)
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_address, 0u);

        const auto status =
            syscalls::handle_NtQueryVirtualMemory(c, h, 0x1000, MemoryBasicInformation, info_address,
                                                  sizeof(EMU_MEMORY_BASIC_INFORMATION64), emulator_object<uint64_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_ACCESS_DENIED);
    }

    // execute_allocate_memory rejects PAGE_WRITECOPY/PAGE_EXECUTE_WRITECOPY as an initial allocation
    // protection, matching handle_NtAllocateVirtualMemoryEx's own base_protection check
    // (memory.cpp) - try_map_nt_to_emulator_protection alone accepts both flags, so this check has to
    // be explicit and separate, both locally and here.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteRejectsWriteCopyProtection)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0x1000);

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_WRITECOPY);

        ASSERT_EQ(status, STATUS_INVALID_PAGE_PROTECTION);
    }

    // A combined-invalid request (zero size AND a rejected protection) must report the same status the
    // same-process path would: handle_NtAllocateVirtualMemoryEx checks request.size == 0 before it
    // ever looks at the protection flags (memory.cpp), so execute_allocate_memory has to check size
    // first too, or this exact case would return STATUS_INVALID_PAGE_PROTECTION instead.
    TEST(CrossProcessTest, NtAllocateVirtualMemorySyscallRemoteChecksSizeBeforeProtection)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto base_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_out, 0u);
        const emulator_object<uint64_t> base_address{parent.memory, base_out};
        base_address.write(0);

        const auto size_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(size_out, 0u);
        const emulator_object<uint64_t> bytes_to_allocate{parent.memory, size_out};
        bytes_to_allocate.write(0); // zero size, deliberately combined with a rejected protection below

        const auto status =
            syscalls::handle_NtAllocateVirtualMemory(c, h, base_address, 0, bytes_to_allocate, MEM_RESERVE | MEM_COMMIT, PAGE_WRITECOPY);

        ASSERT_EQ(status, STATUS_INVALID_PARAMETER);
    }

    // Proves MemoryMappedFilenameInformation is genuinely wired remotely (not the blanket
    // STATUS_NOT_SUPPORTED every non-MemoryBasicInformation class used to get): the filename set on
    // CHILD's region via set_region_mapped_filename round-trips through the control channel and lands
    // as a real UNICODE_STRING in PARENT's own guest memory, matching what get_mapped_filename's
    // to_device_path() transform (the same one the same-process handler uses) independently produces
    // for the same input.
    TEST(CrossProcessTest, NtQueryVirtualMemorySyscallRemoteReturnsMappedFilename)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);
        child.memory.set_region_mapped_filename(child_base, u"C:\\Windows\\System32\\ntdll.dll");

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto expected_path = windows_path(u"C:\\Windows\\System32\\ntdll.dll").to_device_path();
        const auto required_length = static_cast<uint32_t>(sizeof(UNICODE_STRING<EmulatorTraits<Emu64>>) +
                                                           expected_path.size() * sizeof(char16_t) + sizeof(char16_t));

        const auto info_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_address, 0u);

        const auto return_length_out = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(return_length_out, 0u);
        const emulator_object<uint64_t> return_length{parent.memory, return_length_out};

        const auto status = syscalls::handle_NtQueryVirtualMemory(c, h, child_base, MemoryMappedFilenameInformation, info_address,
                                                                  required_length, return_length);

        ASSERT_EQ(status, STATUS_SUCCESS);
        ASSERT_EQ(return_length.read(), required_length);

        const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> info{parent.memory, info_address};
        const auto info_value = info.read();

        ASSERT_EQ(info_value.Length, static_cast<USHORT>(expected_path.size() * sizeof(char16_t)));

        std::u16string actual(expected_path.size(), u'\0');
        ASSERT_TRUE(parent.memory.try_read_memory(info_value.Buffer, actual.data(), actual.size() * sizeof(char16_t)));
        ASSERT_EQ(actual, expected_path);
    }

    // Proves MemoryImageInformation is genuinely wired remotely rather than blanket-rejected: with no
    // module mapped at the queried address in CHILD, the executor reports STATUS_INVALID_ADDRESS (the
    // same status the same-process handler reports for the identical "no module here" case,
    // memory.cpp), not STATUS_NOT_SUPPORTED.
    TEST(CrossProcessTest, NtQueryVirtualMemorySyscallRemoteImageInformationReportsInvalidAddress)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_address, 0u);

        const auto status =
            syscalls::handle_NtQueryVirtualMemory(c, h, 0x10000, MemoryImageInformation, info_address, sizeof(MEMORY_IMAGE_INFORMATION64),
                                                  emulator_object<uint64_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_INVALID_ADDRESS);
    }

    // Handler-level cross-instance proof for MemoryRegionInformation: AllocationBase/RegionSize/
    // CommitSize can only have come from CHILD's own reservation - proving this class is genuinely
    // wired (previously a blanket STATUS_NOT_SUPPORTED), not just MemoryBasicInformation.
    TEST(CrossProcessTest, NtQueryVirtualMemorySyscallRemoteReturnsRegionInformation)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto child_base = child.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_address, 0u);

        const auto status =
            syscalls::handle_NtQueryVirtualMemory(c, h, child_base, MemoryRegionInformation, info_address,
                                                  sizeof(MEMORY_REGION_INFORMATION64), emulator_object<uint64_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);

        const emulator_object<MEMORY_REGION_INFORMATION64> info{parent.memory, info_address};
        const auto info_value = info.read();

        ASSERT_EQ(info_value.AllocationBase, child_base);
        ASSERT_EQ(info_value.RegionSize, static_cast<int64_t>(0x2000));
        ASSERT_EQ(info_value.CommitSize, static_cast<int64_t>(0x2000));
    }

    // Proves execute_query_memory now calls the real, section-image-aware
    // map_emulator_to_nt_allocation_protection (shared from memory_utils.hpp) instead of the plain
    // map_emulator_to_nt_protection it used before: a section_image region's read-write initial
    // permission must come back as PAGE_WRITECOPY, not PAGE_READWRITE - the one case the two formulas
    // disagree on.
    TEST(CrossProcessTest, NtQueryVirtualMemorySyscallRemoteAllocationProtectIsSectionImageAware)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        const auto candidate_base = child.memory.find_free_allocation_base(0x1000);
        ASSERT_NE(candidate_base, 0u);
        ASSERT_TRUE(
            child.memory.allocate_memory(candidate_base, 0x1000, memory_permission::read_write, false, memory_region_kind::section_image));

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        parent.process.child_processes[7] = record;
        parent.register_child_control_channel(7, std::make_unique<loopback_process_control_channel>(child));

        const auto c = make_context(parent);
        const auto h = make_pseudo_handle(7, handle_types::process);

        const auto info_address = parent.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(info_address, 0u);

        const auto status =
            syscalls::handle_NtQueryVirtualMemory(c, h, candidate_base, MemoryBasicInformation, info_address,
                                                  sizeof(EMU_MEMORY_BASIC_INFORMATION64), emulator_object<uint64_t>{parent.memory, 0});

        ASSERT_EQ(status, STATUS_SUCCESS);

        const emulator_object<EMU_MEMORY_BASIC_INFORMATION64> info{parent.memory, info_address};
        const auto info_value = info.read();

        ASSERT_EQ(info_value.AllocationProtect, static_cast<uint32_t>(PAGE_WRITECOPY));
    }
} // namespace sogen::test
