#include "emulation_test_utils.hpp"

#ifdef __APPLE__
#include <memory_manager.hpp>

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace sogen::test
{
    namespace
    {
        constexpr size_t guest_page_size = 0x1000;
        constexpr size_t host_page_size = 0x4000;
        constexpr uint16_t long_mode_code_selector = 0x08;
        constexpr uint64_t long_mode_code_descriptor = 0x00AF9B000000FFFF;

        struct observed_access
        {
            uint64_t address{};
            size_t size{};
            uint64_t value{};
        };

        std::optional<vm_prot_t> host_protection(const uint64_t address)
        {
            mach_vm_address_t region = address;
            mach_vm_size_t size = 0;
            vm_region_basic_info_data_64_t info{};
            mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object = MACH_PORT_NULL;
            if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count,
                               &object) != KERN_SUCCESS ||
                region > address)
            {
                return std::nullopt;
            }
            return info.protection;
        }

        // One 16KB host page: guest code in its first 4KB slot, watched data in the second, and two more
        // data slots, so every access in these tests shares a host page with a watch and with the code.
        class FexWatchpointTest : public testing::Test
        {
          protected:
            void SetUp() override
            {
                try
                {
                    this->emu_ = create_x86_64_emulator(backend_type::fex);
                }
                catch (const std::exception&)
                {
                    GTEST_SKIP() << "FEX backend is not available";
                }

                this->memory_.emplace(*this->emu_);
                this->memory_->reserve_host_memory_ranges();

                this->block_ = this->memory_->allocate_memory(host_page_size, memory_permission::read_write);
                const uint64_t gdt = this->memory_->allocate_memory(guest_page_size, memory_permission::read_write);
                ASSERT_NE(this->block_, 0u);
                ASSERT_NE(gdt, 0u);
                ASSERT_EQ(this->block_ % host_page_size, 0u);

                this->emu_->write_memory<uint64_t>(gdt + long_mode_code_selector, long_mode_code_descriptor);
                this->emu_->load_gdt(gdt, guest_page_size);
                this->emu_->reg<uint16_t>(x86_register::cs, long_mode_code_selector);

                // FEX raises HLT as #GP with RIP left on the hlt, so this is what ends each run.
                this->emu_->hook_interrupt([](cpu_interface& cpu, int) { cpu.stop(); });
                this->emu_->hook_memory_violation([this](cpu_interface& cpu, uint64_t, size_t, memory_operation, memory_violation_type) {
                    ++this->violations_;
                    cpu.stop();
                    return memory_violation_continuation::stop;
                });
            }

            void load_code(const std::span<const uint8_t> code)
            {
                this->memory_->write_memory(this->code(), code.data(), code.size());
                ASSERT_TRUE(this->memory_->protect_memory(this->code(), guest_page_size, memory_permission::read_exec));
            }

            void run_code()
            {
                this->emu_->reg(x86_register::rip, this->code());
                this->emu_->start(0);
            }

            void enable_page_fault_watches()
            {
                this->emu_->set_memory_watch_hook_mode(hook_interface::memory_watch_hook_mode::page_fault);
            }

            emulator_hook* record_writes(const uint64_t address, const uint64_t size, std::vector<observed_access>& accesses)
            {
                return this->emu_->hook_memory_write(address, size, make_recorder(accesses));
            }

            emulator_hook* record_reads(const uint64_t address, const uint64_t size, std::vector<observed_access>& accesses)
            {
                return this->emu_->hook_memory_read(address, size, make_recorder(accesses));
            }

            uint64_t code() const
            {
                return this->block_;
            }

            uint64_t data() const
            {
                return this->block_ + guest_page_size;
            }

            uint64_t guest_qword(const uint64_t address) const
            {
                return this->emu_->read_memory<uint64_t>(address);
            }

            std::unique_ptr<x86_64_emulator> emu_{};
            std::optional<memory_manager> memory_{};
            uint64_t block_{};
            int violations_{};

          private:
            static memory_access_hook_callback make_recorder(std::vector<observed_access>& accesses)
            {
                return [&accesses](cpu_interface&, const uint64_t address, const void* data, const size_t size) {
                    observed_access access{.address = address, .size = size};
                    std::memcpy(&access.value, data, std::min(size, sizeof(access.value)));
                    accesses.push_back(access);
                };
            }
        };

        // mov [rax], rbx; hlt
        constexpr std::array<uint8_t, 4> store_rbx_to_rax = {0x48, 0x89, 0x18, 0xF4};
        // mov rbx, [rax]; hlt
        constexpr std::array<uint8_t, 4> load_rbx_from_rax = {0x48, 0x8B, 0x18, 0xF4};
    }

    TEST_F(FexWatchpointTest, AutomaticModeStaysNoOp)
    {
        // mov [rax], rbx; mov rcx, [rax]; hlt
        constexpr std::array<uint8_t, 7> code = {0x48, 0x89, 0x18, 0x48, 0x8B, 0x08, 0xF4};
        this->load_code(code);

        const auto protection_before = host_protection(this->data());

        std::vector<observed_access> writes{};
        std::vector<observed_access> reads{};
        auto* write_hook = this->record_writes(this->data(), sizeof(uint64_t), writes);
        auto* read_hook = this->record_reads(this->data(), sizeof(uint64_t), reads);

        EXPECT_EQ(host_protection(this->data()), protection_before);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 0x1122334455667788ULL);
        this->run_code();

        EXPECT_TRUE(writes.empty());
        EXPECT_TRUE(reads.empty());
        EXPECT_EQ(this->violations_, 0);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rcx), 0x1122334455667788ULL);
        EXPECT_EQ(this->guest_qword(this->data()), 0x1122334455667788ULL);

        this->emu_->delete_hook(write_hook);
        this->emu_->delete_hook(read_hook);
    }

    TEST_F(FexWatchpointTest, WriteWatchReportsWrittenBytes)
    {
        this->enable_page_fault_watches();
        this->load_code(store_rbx_to_rax);

        std::vector<observed_access> writes{};
        auto* hook = this->record_writes(this->data(), sizeof(uint64_t), writes);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 0x1122334455667788ULL);
        this->run_code();

        ASSERT_EQ(writes.size(), 1u);
        EXPECT_EQ(writes.front().address, this->data());
        EXPECT_EQ(writes.front().size, sizeof(uint64_t));
        EXPECT_EQ(writes.front().value, 0x1122334455667788ULL);
        EXPECT_EQ(this->guest_qword(this->data()), 0x1122334455667788ULL);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 3);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, ReadWatchOnHostPageSharedWithCodeReportsReadBytes)
    {
        this->enable_page_fault_watches();
        this->load_code(load_rbx_from_rax);
        this->emu_->write_memory<uint64_t>(this->data() + 8, 0xCAFEBABEDEADBEEFULL);

        std::vector<observed_access> reads{};
        auto* hook = this->record_reads(this->data() + 8, sizeof(uint64_t), reads);

        this->emu_->reg(x86_register::rax, this->data() + 8);
        this->run_code();

        ASSERT_EQ(reads.size(), 1u);
        EXPECT_EQ(reads.front().address, this->data() + 8);
        EXPECT_EQ(reads.front().value, 0xCAFEBABEDEADBEEFULL);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rbx), 0xCAFEBABEDEADBEEFULL);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, AccessesOutsideWatchedRangeOnSameHostPagePassThrough)
    {
        this->enable_page_fault_watches();

        // mov [rax+0x20], rbx; mov rcx, [rax+0x40]; mov [rdx], rbx; mov rcx, [rsi]; hlt
        constexpr std::array<uint8_t, 15> code = {0x48, 0x89, 0x58, 0x20, 0x48, 0x8B, 0x48, 0x40, 0x48, 0x89, 0x1A, 0x48, 0x8B, 0x0E, 0xF4};
        this->load_code(code);

        const uint64_t third_slot = this->block_ + 2 * guest_page_size;
        const uint64_t fourth_slot = this->block_ + 3 * guest_page_size;
        this->emu_->write_memory<uint64_t>(fourth_slot, 0x0102030405060708ULL);

        std::vector<observed_access> accesses{};
        auto* write_hook = this->record_writes(this->data(), sizeof(uint64_t), accesses);
        auto* read_hook = this->record_reads(this->data(), sizeof(uint64_t), accesses);
        EXPECT_EQ(host_protection(this->data()), VM_PROT_NONE);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 0xAAAAAAAAAAAAAAAAULL);
        this->emu_->reg(x86_register::rdx, third_slot);
        this->emu_->reg(x86_register::rsi, fourth_slot);
        this->run_code();

        EXPECT_TRUE(accesses.empty());
        EXPECT_EQ(this->violations_, 0);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 14);
        EXPECT_EQ(this->guest_qword(this->data() + 0x20), 0xAAAAAAAAAAAAAAAAULL);
        EXPECT_EQ(this->guest_qword(third_slot), 0xAAAAAAAAAAAAAAAAULL);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rcx), 0x0102030405060708ULL);

        this->emu_->delete_hook(write_hook);
        this->emu_->delete_hook(read_hook);
    }

    TEST_F(FexWatchpointTest, OverlappingWatchesAreRefcountedUntilLastDeletion)
    {
        this->enable_page_fault_watches();
        this->load_code(store_rbx_to_rax);

        const auto unwatched_protection = host_protection(this->data());
        ASSERT_TRUE(unwatched_protection.has_value());

        std::vector<observed_access> first{};
        std::vector<observed_access> second{};
        auto* first_hook = this->record_writes(this->data(), sizeof(uint64_t), first);
        auto* second_hook = this->record_writes(this->data() + 4, sizeof(uint64_t), second);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 1);
        this->run_code();

        EXPECT_EQ(first.size(), 1u);
        EXPECT_EQ(second.size(), 1u);

        this->emu_->delete_hook(first_hook);
        EXPECT_NE(host_protection(this->data()), unwatched_protection);

        this->emu_->reg(x86_register::rbx, 2);
        this->run_code();

        EXPECT_EQ(first.size(), 1u);
        ASSERT_EQ(second.size(), 2u);
        EXPECT_EQ(second.back().value, 2u);

        this->emu_->delete_hook(second_hook);
        EXPECT_EQ(host_protection(this->data()), unwatched_protection);

        this->emu_->reg(x86_register::rbx, 3);
        this->run_code();

        EXPECT_EQ(second.size(), 2u);
        EXPECT_EQ(this->guest_qword(this->data()), 3u);
        EXPECT_EQ(this->violations_, 0);
    }

    TEST_F(FexWatchpointTest, HugeNominalRangeOnlyProtectsMappedPages)
    {
        this->enable_page_fault_watches();
        this->load_code(store_rbx_to_rax);

        constexpr uint64_t nominal_size = 64ULL << 30;
        const uint64_t range_start = this->data() - (4ULL << 20);
        const uint64_t unmapped_probe = this->data() + (16ULL << 30);
        const auto probe_before = host_protection(unmapped_probe);

        std::vector<observed_access> writes{};
        auto* hook = this->record_writes(range_start, nominal_size, writes);

        EXPECT_EQ(host_protection(unmapped_probe), probe_before);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 0x55);
        this->run_code();

        ASSERT_EQ(writes.size(), 1u);
        EXPECT_EQ(writes.front().address, this->data());
        EXPECT_EQ(writes.front().value, 0x55u);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, WholeAddressSpaceReadWatchSeesGuestRead)
    {
        this->enable_page_fault_watches();
        this->load_code(load_rbx_from_rax);
        this->emu_->write_memory<uint64_t>(this->data(), 0x77);

        std::vector<observed_access> reads{};
        auto* hook = this->record_reads(0, std::numeric_limits<uint64_t>::max(), reads);

        this->emu_->reg(x86_register::rax, this->data());
        this->run_code();

        ASSERT_EQ(reads.size(), 1u);
        EXPECT_EQ(reads.front().address, this->data());
        EXPECT_EQ(reads.front().value, 0x77u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rbx), 0x77u);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, CallbackStopHaltsAfterTheAccessingInstruction)
    {
        this->enable_page_fault_watches();

        // mov [rax], rbx; mov rbx, 7; hlt
        constexpr std::array<uint8_t, 11> code = {0x48, 0x89, 0x18, 0x48, 0xC7, 0xC3, 0x07, 0x00, 0x00, 0x00, 0xF4};
        this->load_code(code);

        int hits = 0;
        auto* hook = this->emu_->hook_memory_write(this->data(), sizeof(uint64_t), [&](cpu_interface& cpu, uint64_t, const void*, size_t) {
            ++hits;
            cpu.stop();
        });

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 9);
        this->run_code();

        EXPECT_EQ(hits, 1);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 3);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rbx), 9u);
        EXPECT_EQ(this->guest_qword(this->data()), 9u);

        this->emu_->start(0);

        EXPECT_EQ(hits, 1);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rbx), 7u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 10);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, SingleStepOverWatchedWriteFiresAndStops)
    {
        this->enable_page_fault_watches();
        this->load_code(store_rbx_to_rax);

        std::vector<observed_access> writes{};
        auto* hook = this->record_writes(this->data(), sizeof(uint64_t), writes);

        this->emu_->reg(x86_register::rip, this->code());
        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 0x42);
        this->emu_->start(1);

        ASSERT_EQ(writes.size(), 1u);
        EXPECT_EQ(writes.front().value, 0x42u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 3);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rflags) & 0x100, 0u);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, SingleStepOverWatchedReadOnHostPageSharedWithCodeFires)
    {
        this->enable_page_fault_watches();
        this->load_code(load_rbx_from_rax);
        this->emu_->write_memory<uint64_t>(this->data(), 0x31);

        std::vector<observed_access> reads{};
        auto* hook = this->record_reads(this->data(), sizeof(uint64_t), reads);

        this->emu_->reg(x86_register::rip, this->code());
        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->start(1);

        ASSERT_EQ(reads.size(), 1u);
        EXPECT_EQ(reads.front().value, 0x31u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 3);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rbx), 0x31u);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(hook);
    }

    TEST_F(FexWatchpointTest, BreakpointStepOverComposesWithWatch)
    {
        this->emu_->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);
        this->enable_page_fault_watches();
        this->load_code(store_rbx_to_rax);

        int breakpoint_hits = 0;
        auto* breakpoint = this->emu_->hook_memory_execution(this->code(), [&](cpu_interface& cpu, uint64_t) {
            ++breakpoint_hits;
            cpu.stop();
        });

        std::vector<observed_access> writes{};
        auto* watch = this->record_writes(this->data(), sizeof(uint64_t), writes);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rbx, 0x99);
        this->run_code();

        EXPECT_EQ(breakpoint_hits, 1);
        EXPECT_TRUE(writes.empty());
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code());

        this->emu_->start(0);

        EXPECT_EQ(breakpoint_hits, 1);
        ASSERT_EQ(writes.size(), 1u);
        EXPECT_EQ(writes.front().value, 0x99u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 3);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(watch);
        this->emu_->delete_hook(breakpoint);
    }

    TEST_F(FexWatchpointTest, BreakpointInstalledBeforeFirstStartOnReadWatchedHostPage)
    {
        this->emu_->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);
        this->enable_page_fault_watches();

        // mov rbx, [rax]; mov [rax], rcx; hlt
        constexpr std::array<uint8_t, 7> code = {0x48, 0x8B, 0x18, 0x48, 0x89, 0x08, 0xF4};
        this->load_code(code);
        this->emu_->write_memory<uint64_t>(this->data(), 0x66);

        std::vector<observed_access> reads{};
        auto* watch = this->record_reads(this->data(), sizeof(uint64_t), reads);

        const uint64_t breakpoint_address = this->code() + 3;
        int breakpoint_hits = 0;
        auto* breakpoint = this->emu_->hook_memory_execution(breakpoint_address, [&](cpu_interface& cpu, uint64_t) {
            ++breakpoint_hits;
            cpu.stop();
        });

        this->emu_->write_memory<uint8_t>(breakpoint_address, 0x48);

        std::array<uint8_t, code.size()> visible{};
        ASSERT_TRUE(this->emu_->try_read_memory(this->code(), visible.data(), visible.size()));
        EXPECT_EQ(visible, code);

        this->emu_->reg(x86_register::rax, this->data());
        this->emu_->reg(x86_register::rcx, 0x88);
        this->run_code();

        EXPECT_EQ(breakpoint_hits, 1);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), breakpoint_address);
        ASSERT_EQ(reads.size(), 1u);
        EXPECT_EQ(reads.front().value, 0x66u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rbx), 0x66u);

        this->emu_->start(0);

        EXPECT_EQ(breakpoint_hits, 1);
        EXPECT_EQ(this->guest_qword(this->data()), 0x88u);
        EXPECT_EQ(this->emu_->reg<uint64_t>(x86_register::rip), this->code() + 6);
        EXPECT_EQ(this->violations_, 0);

        this->emu_->delete_hook(breakpoint);
        this->emu_->delete_hook(watch);
    }

    TEST_F(FexWatchpointTest, HostSideAccessesBypassWatches)
    {
        this->enable_page_fault_watches();

        std::vector<observed_access> accesses{};
        auto* write_hook = this->record_writes(this->data(), sizeof(uint64_t), accesses);
        auto* read_hook = this->record_reads(this->data(), sizeof(uint64_t), accesses);

        this->emu_->write_memory<uint64_t>(this->data(), 0x1234);
        EXPECT_EQ(this->guest_qword(this->data()), 0x1234u);
        EXPECT_TRUE(accesses.empty());
        EXPECT_EQ(host_protection(this->data()), VM_PROT_NONE);

        this->emu_->delete_hook(write_hook);
        this->emu_->delete_hook(read_hook);
    }
} // namespace sogen::test
#endif
