#include "emulation_test_utils.hpp"

#include <cross_process.hpp>
#include <syscall_utils.hpp>
#include <process_control_server.hpp>
#include <cross_process_memory.hpp>
#include <memory_utils.hpp>

#include <array>
#include <cstring>

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
        constexpr ACCESS_MASK MAXIMUM_ALLOWED = 0x02000000;

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

        // A client-side channel whose response's request_id never matches what was sent - simulating a
        // desynchronized transport (the real fd_process_control_channel's dead_ path, see
        // child_process_spawn.cpp) even though the underlying operation executed against peer.
        class desyncing_process_control_channel final : public process_control_channel
        {
          public:
            explicit desyncing_process_control_channel(windows_emulator& peer)
                : peer_(&peer)
            {
            }

            std::optional<process_control_response> request(const process_control_request& request, int /*timeout_ms*/) override
            {
                process_control_request sent = request;
                sent.request_id = 1;

                process_control_response response{};
                execute_process_control_request(*this->peer_, sent, response);
                response.request_id = sent.request_id + 1;

                if (response.request_id != sent.request_id)
                {
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
        };

        // A server-side channel double for pump_process_control: try_receive() drains a queue of
        // canned requests, respond() records what was sent back.
        class queued_process_control_channel final : public process_control_channel
        {
          public:
            std::optional<process_control_response> request(const process_control_request&, int /*timeout_ms*/) override
            {
                return std::nullopt;
            }

            std::optional<process_control_request> try_receive() override
            {
                if (this->pending.empty())
                {
                    return std::nullopt;
                }

                auto next = this->pending.front();
                this->pending.erase(this->pending.begin());
                return next;
            }

            void respond(const process_control_response& response) override
            {
                this->responses.push_back(response);
            }

            std::vector<process_control_request> pending{};
            std::vector<process_control_response> responses{};
        };
    }

    TEST(CrossProcessTest, NonChildHandleFallsThrough)
    {
        auto emu = create_empty_emulator();
        const auto c = make_context(emu);

        const auto result = resolve_child_target(c, handle{}, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, UnknownChildRecordIsInvalidHandle)
    {
        auto emu = create_empty_emulator();
        const auto c = make_context(emu);

        const auto h = make_pseudo_handle(42, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_INVALID_HANDLE);
    }

    TEST(CrossProcessTest, KnownChildWithoutLiveChannelIsNotSupported)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        emu.process.child_processes[7] = record;

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, InsufficientAccessIsDenied)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0;
        emu.process.child_processes[7] = record;
        emu.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_ACCESS_DENIED);
    }

    TEST(CrossProcessTest, SufficientAccessResolvesProcessHandle)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        emu.process.child_processes[7] = record;
        emu.register_child_control_channel(7, std::make_unique<fake_control_channel>());
        auto* const registered = emu.find_child_control_channel(7);

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<child_target>(result));
        const auto& target = std::get<child_target>(result);
        ASSERT_EQ(target.record_id, 7u);
        ASSERT_EQ(target.channel, registered);
    }

    TEST(CrossProcessTest, SufficientAccessResolvesThreadHandleToSameRecord)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        emu.process.child_processes[7] = record;
        emu.register_child_control_channel(7, std::make_unique<fake_control_channel>());
        auto* const registered = emu.find_child_control_channel(7);

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::thread);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<child_target>(result));
        const auto& target = std::get<child_target>(result);
        ASSERT_EQ(target.record_id, 7u);
        ASSERT_EQ(target.channel, registered);
    }

    TEST(CrossProcessTest, ResolveGrantedProcessAccessPassesThroughSpecificMask)
    {
        ASSERT_EQ(resolve_granted_process_access(PROCESS_VM_READ), PROCESS_VM_READ);
    }

    TEST(CrossProcessTest, ResolveGrantedProcessAccessMapsMaximumAllowedToAllAccess)
    {
        ASSERT_EQ(resolve_granted_process_access(MAXIMUM_ALLOWED), PROCESS_ALL_ACCESS);
    }

    TEST(CrossProcessTest, ResolveGrantedProcessAccessMapsGenericAllToAllAccess)
    {
        ASSERT_EQ(resolve_granted_process_access(GENERIC_ALL), PROCESS_ALL_ACCESS);
    }

    // Uses two genuinely separate windows_emulator instances (rather than one emulator whose channel
    // loops back onto itself, like most of this file's other tests) so a passing assertion actually
    // proves cross-instance behavior: emulator_a and emulator_b each get their own, independently
    // written pattern at the same guest address, and the channel - bound only to emulator_b - must
    // return emulator_b's bytes, never emulator_a's.
    TEST(CrossProcessTest, ReadMemoryRoundTrips)
    {
        auto emulator_a = create_empty_emulator();
        auto emulator_b = create_empty_emulator();
        loopback_process_control_channel channel{emulator_b};

        const auto base_a = emulator_a.memory.allocate_memory(0x1000, memory_permission::read_write);
        const auto base_b = emulator_b.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_a, 0u);
        ASSERT_NE(base_b, 0u);

        std::vector<std::byte> pattern_a(0x40);
        std::vector<std::byte> pattern_b(0x40);
        for (size_t i = 0; i < pattern_a.size(); ++i)
        {
            pattern_a[i] = static_cast<std::byte>(i);
            pattern_b[i] = static_cast<std::byte>(0xC0 + i);
        }
        ASSERT_TRUE(emulator_a.memory.try_write_memory(base_a, pattern_a.data(), pattern_a.size()));
        ASSERT_TRUE(emulator_b.memory.try_write_memory(base_b, pattern_b.data(), pattern_b.size()));

        process_control_request request{};
        request.op = process_control_op::read_memory;
        request.address = base_b;
        request.size = pattern_b.size();

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->payload, pattern_b);
        ASSERT_NE(response->payload, pattern_a);
    }

    // Two genuinely separate instances again (see ReadMemoryRoundTrips): the write goes through the
    // channel to emulator_b only, and emulator_a's own memory at the same address - never touched by
    // the channel - must still hold exactly what it held before, proving the write didn't leak across
    // address spaces.
    TEST(CrossProcessTest, WriteMemoryRoundTrips)
    {
        auto emulator_a = create_empty_emulator();
        auto emulator_b = create_empty_emulator();
        loopback_process_control_channel channel{emulator_b};

        const auto base_a = emulator_a.memory.allocate_memory(0x1000, memory_permission::read_write);
        const auto base_b = emulator_b.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_a, 0u);
        ASSERT_NE(base_b, 0u);

        std::vector<std::byte> original_a(0x40, std::byte{0x11});
        ASSERT_TRUE(emulator_a.memory.try_write_memory(base_a, original_a.data(), original_a.size()));

        std::vector<std::byte> pattern(0x40);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(0x80 + i);
        }

        process_control_request request{};
        request.op = process_control_op::write_memory;
        request.address = base_b;
        request.payload = pattern;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->bytes_written, pattern.size());

        std::vector<std::byte> actual_b(pattern.size());
        ASSERT_TRUE(emulator_b.memory.try_read_memory(base_b, actual_b.data(), actual_b.size()));
        ASSERT_EQ(actual_b, pattern);

        std::vector<std::byte> actual_a(original_a.size());
        ASSERT_TRUE(emulator_a.memory.try_read_memory(base_a, actual_a.data(), actual_a.size()));
        ASSERT_EQ(actual_a, original_a);
    }

    TEST(CrossProcessTest, ReadAcrossPageBoundary)
    {
        auto b = create_empty_emulator();
        loopback_process_control_channel channel{b};

        constexpr size_t size = 0x3000;
        const auto base = b.memory.allocate_memory(size, memory_permission::read_write);
        ASSERT_NE(base, 0u);

        std::vector<std::byte> pattern(size);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(i % 251);
        }
        ASSERT_TRUE(b.memory.try_write_memory(base, pattern.data(), pattern.size()));

        process_control_request request{};
        request.op = process_control_op::read_memory;
        request.address = base;
        request.size = pattern.size();

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->payload, pattern);
    }

    // Composes copy_guest_range_out then copy_guest_range_in exactly the way
    // handle_NtReadVirtualMemory/handle_NtWriteVirtualMemory do (syscalls/memory.cpp), against a
    // forward-overlapping same-process source/destination range spanning more than one page boundary
    // (src=region+0, dst=region+0x1000, 0x2000 bytes copied, so [0x1000, 0x2000) is both read from and
    // written to). Proves the two-pass design is memmove-safe: the destination ends up with exactly
    // the source's ORIGINAL bytes, not a version corrupted by an earlier chunk's write clobbering a
    // later chunk's not-yet-read source data (see cross_process_memory.hpp's copy_guest_range_out doc
    // comment for why the prior interleaved-chunk implementation was vulnerable to this and this one
    // isn't).
    TEST(CrossProcessTest, SameProcessOverlappingForwardCopyIsMemmoveSafe)
    {
        auto emu = create_empty_emulator();

        constexpr size_t region_size = 0x4000;
        const auto region = emu.memory.allocate_memory(region_size, memory_permission::read_write);
        ASSERT_NE(region, 0u);

        std::vector<std::byte> pattern(region_size);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(i % 256);
        }
        ASSERT_TRUE(emu.memory.try_write_memory(region, pattern.data(), pattern.size()));

        constexpr uint64_t src_offset = 0;
        constexpr uint64_t dst_offset = 0x1000;
        constexpr size_t copy_size = 0x2000;

        const auto src_addr = region + src_offset;
        const auto dst_addr = region + dst_offset;

        std::vector<std::byte> staging(copy_size);
        const auto bytes_read = copy_guest_range_out(emu.memory, src_addr, staging);
        ASSERT_EQ(bytes_read, copy_size);

        const auto bytes_written = copy_guest_range_in(emu.memory, dst_addr, std::span(staging).first(bytes_read));
        ASSERT_EQ(bytes_written, copy_size);

        std::vector<std::byte> actual(copy_size);
        ASSERT_TRUE(emu.memory.try_read_memory(dst_addr, actual.data(), actual.size()));

        const std::vector<std::byte> expected(pattern.begin() + static_cast<ptrdiff_t>(src_offset),
                                              pattern.begin() + static_cast<ptrdiff_t>(src_offset + copy_size));
        ASSERT_EQ(actual, expected);
    }

    TEST(CrossProcessTest, ReadPartialAtUnmappedTail)
    {
        auto b = create_empty_emulator();
        loopback_process_control_channel channel{b};

        const auto base = b.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base, 0u);

        process_control_request request{};
        request.op = process_control_op::read_memory;
        request.address = base;
        request.size = 0x2000;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_PARTIAL_COPY);
        ASSERT_EQ(response->payload.size(), 0x1000u);
    }

    TEST(CrossProcessTest, ReadWhollyUnmappedReturnsInvalidAddress)
    {
        auto b = create_empty_emulator();
        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::read_memory;
        request.address = 0x1000;
        request.size = 0x10;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_INVALID_ADDRESS);
        ASSERT_EQ(response->payload.size(), 0u);
    }

    // Two genuinely separate instances again (see ReadMemoryRoundTrips): emulator_a holds an
    // independent, already-allocated region with a sentinel pattern for the whole test, untouched by
    // any request on the channel (which only ever addresses emulator_b) - proving the allocate/write/
    // read sequence against emulator_b neither reads from nor corrupts emulator_a's address space.
    TEST(CrossProcessTest, AllocateThenWriteThenRead)
    {
        auto emulator_a = create_empty_emulator();
        auto emulator_b = create_empty_emulator();
        loopback_process_control_channel channel{emulator_b};

        const auto base_a = emulator_a.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base_a, 0u);
        std::vector<std::byte> sentinel_a(0x20, std::byte{0xEE});
        ASSERT_TRUE(emulator_a.memory.try_write_memory(base_a, sentinel_a.data(), sentinel_a.size()));

        process_control_request allocate_request{};
        allocate_request.op = process_control_op::allocate_memory;
        allocate_request.address = 0;
        allocate_request.size = 0x1000;
        allocate_request.allocation_type = MEM_COMMIT;
        allocate_request.protection = PAGE_READWRITE;

        const auto allocate_response = channel.request(allocate_request, process_control_default_timeout_ms);
        ASSERT_TRUE(allocate_response.has_value());
        ASSERT_EQ(allocate_response->status, STATUS_SUCCESS);
        ASSERT_NE(allocate_response->base_address, 0u);

        const auto base_b = allocate_response->base_address;

        std::vector<std::byte> pattern(0x20);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(0x40 + i);
        }

        process_control_request write_request{};
        write_request.op = process_control_op::write_memory;
        write_request.address = base_b;
        write_request.payload = pattern;

        const auto write_response = channel.request(write_request, process_control_default_timeout_ms);
        ASSERT_TRUE(write_response.has_value());
        ASSERT_EQ(write_response->status, STATUS_SUCCESS);
        ASSERT_EQ(write_response->bytes_written, pattern.size());

        process_control_request read_request{};
        read_request.op = process_control_op::read_memory;
        read_request.address = base_b;
        read_request.size = pattern.size();

        const auto read_response = channel.request(read_request, process_control_default_timeout_ms);
        ASSERT_TRUE(read_response.has_value());
        ASSERT_EQ(read_response->status, STATUS_SUCCESS);
        ASSERT_EQ(read_response->payload, pattern);

        std::vector<std::byte> actual_a(sentinel_a.size());
        ASSERT_TRUE(emulator_a.memory.try_read_memory(base_a, actual_a.data(), actual_a.size()));
        ASSERT_EQ(actual_a, sentinel_a);
    }

    TEST(CrossProcessTest, ProtectMemoryReportsOldProtection)
    {
        auto b = create_empty_emulator();
        const auto base = b.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base, 0u);

        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::protect_memory;
        request.address = base;
        request.size = 0x1000;
        request.protection = PAGE_READONLY;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->old_protection, static_cast<uint32_t>(PAGE_READWRITE));

        const auto region_info = b.memory.get_region_info(base);
        ASSERT_EQ(map_emulator_to_nt_protection(region_info.permissions), static_cast<uint32_t>(PAGE_READONLY));
    }

    TEST(CrossProcessTest, FreeMemoryReleasesRegion)
    {
        auto b = create_empty_emulator();
        const auto base = b.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(base, 0u);

        loopback_process_control_channel channel{b};

        process_control_request free_request{};
        free_request.op = process_control_op::free_memory;
        free_request.address = base;
        free_request.free_type = MEM_RELEASE;

        const auto free_response = channel.request(free_request, process_control_default_timeout_ms);
        ASSERT_TRUE(free_response.has_value());
        ASSERT_EQ(free_response->status, STATUS_SUCCESS);

        process_control_request read_request{};
        read_request.op = process_control_op::read_memory;
        read_request.address = base;
        read_request.size = 0x10;

        const auto read_response = channel.request(read_request, process_control_default_timeout_ms);
        ASSERT_TRUE(read_response.has_value());
        ASSERT_EQ(read_response->status, STATUS_INVALID_ADDRESS);
    }

    TEST(CrossProcessTest, QueryMemoryMatchesLocalQuery)
    {
        auto b = create_empty_emulator();
        const auto base = b.memory.allocate_memory(0x2000, memory_permission::read_write);
        ASSERT_NE(base, 0u);

        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::query_memory;
        request.address = base;
        request.info_class = MemoryBasicInformation;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->payload.size(), sizeof(EMU_MEMORY_BASIC_INFORMATION64));

        EMU_MEMORY_BASIC_INFORMATION64 actual{};
        std::memcpy(&actual, response->payload.data(), sizeof(actual));

        const auto region_info = b.memory.get_region_info(base);
        EMU_MEMORY_BASIC_INFORMATION64 expected{};
        expected.BaseAddress = region_info.start;
        expected.AllocationBase = region_info.allocation_base;
        expected.PartitionId = 0;
        expected.RegionSize = static_cast<int64_t>(region_info.length);
        expected.State = region_info.is_committed ? MEM_COMMIT : (region_info.is_reserved ? MEM_RESERVE : MEM_FREE);
        expected.Protect = map_emulator_to_nt_protection(region_info.permissions);
        expected.AllocationProtect = map_emulator_to_nt_protection(region_info.initial_permissions);
        expected.Type = region_info.is_reserved ? memory_region_policy::to_memory_basic_information_type(region_info.kind) : 0;

        ASSERT_EQ(0, std::memcmp(&actual, &expected, sizeof(actual)));
    }

    TEST(CrossProcessTest, TerminateStopsTarget)
    {
        auto b = create_empty_emulator();
        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::terminate;
        request.exit_status = STATUS_SUCCESS;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);

        ASSERT_TERMINATED_SUCCESSFULLY(b);
    }

    TEST(CrossProcessTest, AdoptSectionMintsUsableHandle)
    {
        auto b = create_empty_emulator();
        loopback_process_control_channel channel{b};

        const std::vector<std::byte> content = {std::byte{'h'}, std::byte{'i'}, std::byte{'!'}};

        process_control_request request{};
        request.op = process_control_op::adopt_section;
        request.maximum_size = content.size();
        request.page_protection = PAGE_READWRITE;
        request.allocation_attributes = SEC_COMMIT;
        request.granted_access = 0x000F001F;
        request.payload = content;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_NE(response->minted_handle_bits, 0u);

        handle minted{};
        minted.bits = response->minted_handle_bits;

        auto* const section_entry = b.process.sections.get(minted);
        ASSERT_NE(section_entry, nullptr);
        ASSERT_EQ(section_entry->object->backing_storage, content);
        ASSERT_EQ(section_entry->object->maximum_size, content.size());
        ASSERT_EQ(section_entry->granted_access, static_cast<ACCESS_MASK>(0x000F001F));
    }

    TEST(CrossProcessTest, ResumeThreadClearsSuspendedInitialThread)
    {
        auto b = create_empty_emulator();
        const auto h = b.process.create_thread(b.memory, 0x1000, 0, 0x1000, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, true);
        auto* const thread = b.process.threads.get(h);
        ASSERT_NE(thread, nullptr);
        ASSERT_EQ(thread->suspended, 1u);

        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::resume_thread;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->previous_suspend_count, 1u);
        ASSERT_EQ(thread->suspended, 0u);
    }

    TEST(CrossProcessTest, ResumeThreadOnAlreadyRunningThreadReportsZeroAndStaysZero)
    {
        auto b = create_empty_emulator();
        const auto h = b.process.create_thread(b.memory, 0x1000, 0, 0x1000, 0, true);
        auto* const thread = b.process.threads.get(h);
        ASSERT_NE(thread, nullptr);
        ASSERT_EQ(thread->suspended, 0u);

        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::resume_thread;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(response->previous_suspend_count, 0u);
        ASSERT_EQ(thread->suspended, 0u);
    }

    // resume_thread carries no thread id (see process_control_server.cpp's find_initial_thread doc
    // comment) - it always targets the lowest-id thread, exactly like NtCreateUserProcess's one
    // pseudo thread handle always denotes the child's initial thread on real Windows. Two threads
    // here, created out of id order relative to which one should be picked, prove the selection is by
    // id and not simply "whichever thread was created/inserted last".
    TEST(CrossProcessTest, ResumeThreadTargetsInitialThreadNotLaterOnes)
    {
        auto b = create_empty_emulator();
        const auto initial_handle = b.process.create_thread(b.memory, 0x1000, 0, 0x1000, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, true);
        const auto other_handle = b.process.create_thread(b.memory, 0x2000, 0, 0x1000, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, false);

        auto* const initial_thread = b.process.threads.get(initial_handle);
        auto* const other_thread = b.process.threads.get(other_handle);
        ASSERT_NE(initial_thread, nullptr);
        ASSERT_NE(other_thread, nullptr);
        ASSERT_LT(initial_thread->id, other_thread->id);

        loopback_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::resume_thread;

        const auto response = channel.request(request, process_control_default_timeout_ms);
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_EQ(initial_thread->suspended, 0u);
        ASSERT_EQ(other_thread->suspended, 1u);
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

    // Spans three of perform_chunked_remote_transfer's 1 MiB chunks (memory.cpp), each executed as a
    // separate control-channel round trip - proving the multi-chunk loop reassembles a large transfer
    // correctly, not just single-chunk requests under the cap.
    TEST(CrossProcessTest, NtReadVirtualMemorySyscallChunksTransfersLargerThanOneMebibyte)
    {
        auto parent = create_empty_emulator();
        auto child = create_empty_emulator();

        constexpr size_t total_size = 3u * 1024u * 1024u;

        const auto child_base = child.memory.allocate_memory(total_size, memory_permission::read_write);
        ASSERT_NE(child_base, 0u);

        std::vector<std::byte> pattern(total_size);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pattern[i] = static_cast<std::byte>(i % 256);
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

    TEST(CrossProcessTest, SerializationRoundTrip)
    {
        constexpr std::array all_ops = {
            process_control_op::read_memory,    process_control_op::write_memory,  process_control_op::allocate_memory,
            process_control_op::protect_memory, process_control_op::free_memory,   process_control_op::query_memory,
            process_control_op::terminate,      process_control_op::resume_thread, process_control_op::adopt_section,
        };

        for (const auto op : all_ops)
        {
            process_control_request request{};
            request.request_id = 0x1122334455667788ull;
            request.op = op;
            request.address = 0x10000;
            request.size = 0x2000;
            request.allocation_type = MEM_COMMIT;
            request.protection = PAGE_READWRITE;
            request.free_type = MEM_RELEASE;
            request.info_class = MemoryBasicInformation;
            request.exit_status = STATUS_SUCCESS;
            request.maximum_size = 0x4000;
            request.page_protection = PAGE_READONLY;
            request.allocation_attributes = 1;
            request.granted_access = 0x1F;
            request.payload = {std::byte{1}, std::byte{2}, std::byte{3}};

            utils::buffer_serializer request_buffer{};
            request.serialize(request_buffer);

            utils::buffer_deserializer request_deserializer{request_buffer};
            process_control_request restored_request{};
            restored_request.deserialize(request_deserializer);

            ASSERT_EQ(restored_request.request_id, request.request_id);
            ASSERT_EQ(restored_request.op, request.op);
            ASSERT_EQ(restored_request.address, request.address);
            ASSERT_EQ(restored_request.size, request.size);
            ASSERT_EQ(restored_request.allocation_type, request.allocation_type);
            ASSERT_EQ(restored_request.protection, request.protection);
            ASSERT_EQ(restored_request.free_type, request.free_type);
            ASSERT_EQ(restored_request.info_class, request.info_class);
            ASSERT_EQ(restored_request.exit_status, request.exit_status);
            ASSERT_EQ(restored_request.maximum_size, request.maximum_size);
            ASSERT_EQ(restored_request.page_protection, request.page_protection);
            ASSERT_EQ(restored_request.allocation_attributes, request.allocation_attributes);
            ASSERT_EQ(restored_request.granted_access, request.granted_access);
            ASSERT_EQ(restored_request.payload, request.payload);

            process_control_response response{};
            response.request_id = request.request_id;
            response.status = STATUS_PARTIAL_COPY;
            response.bytes_written = 0x30;
            response.base_address = 0x20000;
            response.region_size = 0x3000;
            response.old_protection = PAGE_EXECUTE_READWRITE;
            response.previous_suspend_count = 2;
            response.minted_handle_bits = 0xAABBCCDDull;
            response.payload = {std::byte{9}, std::byte{8}};

            utils::buffer_serializer response_buffer{};
            response.serialize(response_buffer);

            utils::buffer_deserializer response_deserializer{response_buffer};
            process_control_response restored_response{};
            restored_response.deserialize(response_deserializer);

            ASSERT_EQ(restored_response.request_id, response.request_id);
            ASSERT_EQ(restored_response.status, response.status);
            ASSERT_EQ(restored_response.bytes_written, response.bytes_written);
            ASSERT_EQ(restored_response.base_address, response.base_address);
            ASSERT_EQ(restored_response.region_size, response.region_size);
            ASSERT_EQ(restored_response.old_protection, response.old_protection);
            ASSERT_EQ(restored_response.previous_suspend_count, response.previous_suspend_count);
            ASSERT_EQ(restored_response.minted_handle_bits, response.minted_handle_bits);
            ASSERT_EQ(restored_response.payload, response.payload);
        }
    }

    TEST(CrossProcessTest, MismatchedRequestIdIsTreatedAsTransportFailure)
    {
        auto b = create_empty_emulator();
        desyncing_process_control_channel channel{b};

        process_control_request request{};
        request.op = process_control_op::terminate;
        request.exit_status = STATUS_SUCCESS;

        const auto response = channel.request(request, process_control_default_timeout_ms);

        ASSERT_FALSE(response.has_value());
        ASSERT_TERMINATED_SUCCESSFULLY(b);
    }

    TEST(CrossProcessTest, PumpProcessControlDrainsAndResponds)
    {
        auto b = create_empty_emulator();
        queued_process_control_channel channel{};

        process_control_request request{};
        request.request_id = 7;
        request.op = process_control_op::terminate;
        request.exit_status = STATUS_SUCCESS;
        channel.pending.push_back(request);

        pump_process_control(b, channel);

        ASSERT_TERMINATED_SUCCESSFULLY(b);
        ASSERT_EQ(channel.responses.size(), 1u);
        ASSERT_EQ(channel.responses[0].request_id, 7u);
        ASSERT_EQ(channel.responses[0].status, STATUS_SUCCESS);
    }
} // namespace sogen::test
