#include "emulation_test_utils.hpp"
#include <syscall_utils.hpp>

namespace sogen::syscalls
{
    NTSTATUS handle_NtSetInformationJobObject(const syscall_context& c, handle job_handle, uint32_t info_class, uint64_t info,
                                              uint32_t length);
    NTSTATUS handle_NtAssignProcessToJobObject(const syscall_context& c, handle job_handle, handle process_handle);
    NTSTATUS handle_NtAllocateVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                            uint64_t zero_bits, emulator_object<uint64_t> bytes_to_allocate, uint32_t allocation_type,
                                            uint32_t page_protection);
}

namespace sogen::test
{
    namespace
    {
        constexpr uint32_t extended_limit_information_class = 9;
        constexpr ULONG extended_limit_information_size = 144;
        constexpr uint64_t limit_flags_offset = 16;
        constexpr uint64_t active_process_limit_offset = 40;
        constexpr uint64_t process_memory_limit_offset = 112;
        constexpr uint32_t active_process_flag = 0x0008;
        constexpr uint32_t process_memory_flag = 0x0100;

        class JobObjectTest : public testing::Test
        {
          protected:
            void SetUp() override
            {
                emulator_interfaces interfaces{};
                interfaces.ui = std::make_unique<null_ui_backend>();
                interfaces.audio = std::make_unique<null_audio_backend>();
                interfaces.console = std::make_unique<null_console_backend>();
                this->emu = std::make_unique<windows_emulator>(
                    create_x86_64_emulator_from_environment(),
                    emulator_settings{.disable_logging = true, .emulation_root = get_emulator_root(), .load_registry = false},
                    emulator_callbacks{}, std::move(interfaces));
                this->storage = this->emu->memory.allocate_memory(0x1000, memory_permission::read_write);
                const auto thread =
                    this->emu->process.threads.store(emulator_thread{this->emu->memory, this->emu->process, 0, 0, 0x100000, 0, 8, true});
                this->emu->vcpu(0).active_thread = this->emu->process.threads.get(thread);
                this->job = this->emu->process.jobs.store(job_object{});
            }

            syscall_context context()
            {
                return {.win_emu = *this->emu, .emu = this->emu->vcpu(0).cpu, .vcpu = this->emu->vcpu(0), .proc = this->emu->process};
            }

            NTSTATUS set_limit_flags(const uint32_t flags)
            {
                this->emu->emu().write_memory(this->storage + limit_flags_offset, &flags, sizeof(flags));
                return syscalls::handle_NtSetInformationJobObject(this->context(), this->job, extended_limit_information_class,
                                                                  this->storage, extended_limit_information_size);
            }

            NTSTATUS set_limits(const uint32_t flags, const uint32_t active_processes, const uint64_t process_memory)
            {
                this->emu->emu().write_memory(this->storage + active_process_limit_offset, &active_processes, sizeof(active_processes));
                this->emu->emu().write_memory(this->storage + process_memory_limit_offset, &process_memory, sizeof(process_memory));
                return this->set_limit_flags(flags);
            }

            NTSTATUS assign_current_process()
            {
                return syscalls::handle_NtAssignProcessToJobObject(this->context(), this->job, CURRENT_PROCESS);
            }

            NTSTATUS commit(const uint64_t size)
            {
                const auto base_address = this->storage + 0x800;
                const auto region_size = this->storage + 0x808;
                this->emu->emu().write_memory<uint64_t>(base_address, 0);
                this->emu->emu().write_memory<uint64_t>(region_size, size);

                constexpr uint32_t mem_commit_reserve = 0x3000;
                constexpr uint32_t page_readwrite = 0x04;
                return syscalls::handle_NtAllocateVirtualMemory(
                    this->context(), CURRENT_PROCESS, emulator_object<uint64_t>{this->emu->emu(), base_address}, 0,
                    emulator_object<uint64_t>{this->emu->emu(), region_size}, mem_commit_reserve, page_readwrite);
            }

            std::unique_ptr<windows_emulator> emu{};
            uint64_t storage{};
            handle job{};
        };
    }

    TEST_F(JobObjectTest, AcceptsTheLimitFlagsLibuvSets)
    {
        constexpr uint32_t die_on_unhandled_exception = 0x0400;
        constexpr uint32_t breakaway_ok = 0x0800;
        constexpr uint32_t silent_breakaway_ok = 0x1000;
        constexpr uint32_t kill_on_job_close = 0x2000;
        constexpr auto flags = die_on_unhandled_exception | breakaway_ok | silent_breakaway_ok | kill_on_job_close;

        ASSERT_EQ(STATUS_SUCCESS, this->set_limit_flags(flags));
        EXPECT_EQ(flags, this->emu->process.jobs.get(this->job)->limit_flags);
    }

    TEST_F(JobObjectTest, RejectsLimitsThatAreNotEnforced)
    {
        constexpr uint32_t job_memory_limit = 0x0200;
        EXPECT_EQ(STATUS_NOT_SUPPORTED, this->set_limit_flags(job_memory_limit));
    }

    TEST_F(JobObjectTest, RefusesAProcessPastTheActiveProcessLimit)
    {
        ASSERT_EQ(STATUS_SUCCESS, this->set_limits(active_process_flag, 0, 0));
        EXPECT_EQ(STATUS_QUOTA_EXCEEDED, this->assign_current_process());
    }

    TEST_F(JobObjectTest, ReassigningAMemberDoesNotCountAgainstTheActiveProcessLimit)
    {
        ASSERT_EQ(STATUS_SUCCESS, this->set_limits(active_process_flag, 1, 0));
        EXPECT_EQ(STATUS_SUCCESS, this->assign_current_process());
        EXPECT_EQ(STATUS_SUCCESS, this->assign_current_process());
    }

    TEST_F(JobObjectTest, FailsACommitPastTheProcessMemoryLimit)
    {
        constexpr uint64_t headroom = 0x100000;
        const auto limit = this->emu->memory.compute_private_commit() + headroom;
        ASSERT_EQ(STATUS_SUCCESS, this->set_limits(process_memory_flag, 0, limit));
        ASSERT_EQ(STATUS_SUCCESS, this->assign_current_process());

        EXPECT_EQ(STATUS_COMMITMENT_LIMIT, this->commit(headroom * 2));
        EXPECT_EQ(STATUS_SUCCESS, this->commit(headroom / 2));
        EXPECT_EQ(STATUS_COMMITMENT_LIMIT, this->commit(headroom / 2 + 0x1000));
    }

    TEST_F(JobObjectTest, AssignsTheCurrentProcess)
    {
        EXPECT_EQ(STATUS_SUCCESS, syscalls::handle_NtAssignProcessToJobObject(this->context(), this->job, CURRENT_PROCESS));
    }

    TEST_F(JobObjectTest, RejectsInvalidHandlesWhenAssigning)
    {
        EXPECT_EQ(STATUS_INVALID_HANDLE,
                  syscalls::handle_NtAssignProcessToJobObject(this->context(), handle{.bits = 0x1234}, CURRENT_PROCESS));
        EXPECT_EQ(STATUS_INVALID_HANDLE, syscalls::handle_NtAssignProcessToJobObject(this->context(), this->job, handle{.bits = 0x1234}));
    }
}
