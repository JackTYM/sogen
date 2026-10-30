#if SOGEN_ENABLE_FEX_HVF

#include <gtest/gtest.h>

#include <fex_x86_64_emulator.hpp>
#include <hvf_address_space.hpp>
#include <hvf_guest_runtime.hpp>
#include <memory_manager.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/mman.h>

namespace sogen::test
{
    namespace
    {
        using fex::hvf::address_space;
        using fex::hvf::stage1_page_size;
        using fex::hvf::stage2_page_size;

        constexpr uint64_t base = 0x7000'0000'0000ULL;
        constexpr uint64_t ipa_limit = 1ULL << 36;
        constexpr int rw = PROT_READ | PROT_WRITE;

        struct stage2_call
        {
            enum class kind
            {
                map,
                unmap,
                protect,
            };

            kind type{};
            uint64_t host_address{};
            uint64_t ipa{};
            uint64_t size{};
            int prot{};

            bool operator==(const stage2_call&) const = default;
        };

        stage2_call map_call(const uint64_t host_address, const uint64_t ipa, const uint64_t size, const int prot)
        {
            return {.type = stage2_call::kind::map, .host_address = host_address, .ipa = ipa, .size = size, .prot = prot};
        }

        stage2_call unmap_call(const uint64_t ipa, const uint64_t size)
        {
            return {.type = stage2_call::kind::unmap, .ipa = ipa, .size = size};
        }

        stage2_call protect_call(const uint64_t ipa, const uint64_t size, const int prot)
        {
            return {.type = stage2_call::kind::protect, .ipa = ipa, .size = size, .prot = prot};
        }

        class recording_stage2 final : public fex::hvf::stage2_mapper
        {
          public:
            std::vector<stage2_call> calls;
            stage2_call table_pool{};
            bool host_executable = false;

            bool is_host_executable(uint64_t /*host_address*/, size_t /*size*/) const override
            {
                return this->host_executable;
            }

            void map(const uint64_t host_address, const uint64_t ipa, const size_t size, const int prot) override
            {
                const auto call = map_call(host_address, ipa, size, prot);
                if (this->table_pool.size == 0)
                {
                    this->table_pool = call;
                    return;
                }

                this->calls.push_back(call);
            }

            void unmap(const uint64_t ipa, const size_t size) override
            {
                this->calls.push_back(unmap_call(ipa, size));
            }

            void protect(const uint64_t ipa, const size_t size, const int prot) override
            {
                this->calls.push_back(protect_call(ipa, size, prot));
            }

            // Walks the stage-1 tables the way the vCPU's MMU would, reading them through the table pool's
            // stage-2 mapping.
            uint64_t stage1_descriptor(const address_space& space, const uint64_t address) const
            {
                uint64_t table_ipa = space.stage1_root();
                for (int shift = 39; shift >= 12; shift -= 9)
                {
                    const auto* table =
                        reinterpret_cast<const uint64_t*>(this->table_pool.host_address + (table_ipa - this->table_pool.ipa));
                    const uint64_t descriptor = table[(address >> shift) & 0x1FF];
                    if (shift == 12 || (descriptor & 0b11) != 0b11)
                    {
                        return descriptor;
                    }

                    table_ipa = descriptor & 0x0000FFFFFFFFF000ULL;
                }

                return 0;
            }
        };

        using vector128 = uint64_t __attribute__((vector_size(16)));

        struct gpr_pair
        {
            uint64_t first;
            uint64_t second;
        };

        struct indirect_result
        {
            uint64_t values[4];
        };

        struct vector_pair
        {
            vector128 first;
            vector128 second;
        };

        gpr_pair combine_gprs(const uint64_t a0, const uint64_t a1, const uint64_t a2, const uint64_t a3, const uint64_t a4,
                              const uint64_t a5, const uint64_t a6, const uint64_t a7)
        {
            return {.first = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7, .second = a0 ^ a7};
        }

        indirect_result make_indirect_result(const uint64_t a0, const uint64_t a1)
        {
            return {.values = {a0, a1, a0 + a1, a0 * a1}};
        }

        vector_pair combine_vectors(const vector128 v0, const vector128 v1, const vector128 v2, const vector128 v3, const vector128 v4,
                                    const vector128 v5, const vector128 v6, const vector128 v7)
        {
            return {.first = v0 + v1 + v2 + v3, .second = v4 + v5 + v6 + v7};
        }

        uint32_t instruction_at(const uint64_t address, const size_t index = 0)
        {
            uint32_t instruction{};
            std::memcpy(&instruction, reinterpret_cast<const void*>(address + (index * sizeof(instruction))), sizeof(instruction));
            return instruction;
        }

        std::unique_ptr<x86_64_emulator> create_hypervisor_emulator(std::string& failure)
        {
            try
            {
                return fex::create_x86_64_emulator(fex::execution_mode::hypervisor);
            }
            catch (const std::exception& e)
            {
                failure = e.what();
                return {};
            }
        }

        // A long-mode code segment and RX code at the returned address, followed by hlt.
        uint64_t prepare_guest_code(x86_64_emulator& emu, memory_manager& memory, const std::vector<uint8_t>& code)
        {
            constexpr size_t page = 0x1000;
            const uint64_t code_address = memory.allocate_memory(page, memory_permission::read_write);
            const uint64_t gdt = memory.allocate_memory(page, memory_permission::read_write);
            if (code_address == 0 || gdt == 0)
            {
                return 0;
            }

            constexpr uint16_t long_mode_code_selector = 0x08;
            constexpr uint64_t long_mode_code_descriptor = 0x00AF9B000000FFFF;
            emu.write_memory<uint64_t>(gdt + long_mode_code_selector, long_mode_code_descriptor);
            emu.load_gdt(gdt, page);
            emu.reg<uint16_t>(x86_register::cs, long_mode_code_selector);

            memory.write_memory(code_address, code.data(), code.size());
            if (!memory.protect_memory(code_address, page, memory_permission::read_exec))
            {
                return 0;
            }

            emu.reg(x86_register::rip, code_address);
            return code_address;
        }

        class FexHvfAddressSpaceTest : public testing::Test
        {
          protected:
            recording_stage2 stage2{};
            address_space space{stage2, ipa_limit};

            uint64_t map_and_get_ipa(const uint64_t address, const size_t size, const int prot = rw)
            {
                this->stage2.calls.clear();
                this->space.map(address, size, prot);
                EXPECT_EQ(this->stage2.calls.size(), 1U);
                return this->stage2.calls.empty() ? 0 : this->stage2.calls.back().ipa;
            }
        };
    }

    TEST_F(FexHvfAddressSpaceTest, ContiguousUnmappedPagesBecomeOneStage2Mapping)
    {
        this->space.map(base, 3 * stage2_page_size, rw);

        ASSERT_EQ(this->stage2.calls.size(), 1U);
        EXPECT_EQ(this->stage2.calls[0].type, stage2_call::kind::map);
        EXPECT_EQ(this->stage2.calls[0].host_address, base);
        EXPECT_EQ(this->stage2.calls[0].size, 3 * stage2_page_size);
        EXPECT_EQ(this->stage2.calls[0].prot, rw);
    }

    TEST_F(FexHvfAddressSpaceTest, RangesWidenToWholeStage2Pages)
    {
        this->space.map(base + 0x3000, 0x2000, rw);

        ASSERT_EQ(this->stage2.calls.size(), 1U);
        EXPECT_EQ(this->stage2.calls[0].host_address, base);
        EXPECT_EQ(this->stage2.calls[0].size, 2 * stage2_page_size);
    }

    TEST_F(FexHvfAddressSpaceTest, ProtectOnlyChangesPagesWhoseProtectionDiffers)
    {
        const auto ipa = this->map_and_get_ipa(base, 4 * stage2_page_size);
        this->space.protect(base + stage2_page_size, stage2_page_size, PROT_READ);
        this->stage2.calls.clear();

        this->space.protect(base, 4 * stage2_page_size, PROT_READ);

        const std::vector expected{
            protect_call(ipa, stage2_page_size, PROT_READ),
            protect_call(ipa + 2 * stage2_page_size, 2 * stage2_page_size, PROT_READ),
        };
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, MapReprotectsAlreadyMappedPages)
    {
        const auto ipa = this->map_and_get_ipa(base, stage2_page_size);
        this->stage2.calls.clear();

        this->space.map(base, 2 * stage2_page_size, PROT_READ);

        ASSERT_EQ(this->stage2.calls.size(), 2U);
        EXPECT_EQ(this->stage2.calls[0], protect_call(ipa, stage2_page_size, PROT_READ));
        EXPECT_EQ(this->stage2.calls[1].type, stage2_call::kind::map);
        EXPECT_EQ(this->stage2.calls[1].host_address, base + stage2_page_size);
    }

    TEST_F(FexHvfAddressSpaceTest, ProtectSplitsAtIpaDiscontinuities)
    {
        const auto first = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        const auto second = this->map_and_get_ipa(base + stage2_page_size, stage2_page_size);
        this->stage2.calls.clear();

        this->space.protect(base, 2 * stage2_page_size, PROT_READ);

        const std::vector expected{
            protect_call(first, stage2_page_size, PROT_READ),
            protect_call(second, stage2_page_size, PROT_READ),
        };
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, UnmapSplitsAtIpaDiscontinuities)
    {
        const auto first = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        const auto second = this->map_and_get_ipa(base + stage2_page_size, stage2_page_size);
        this->stage2.calls.clear();

        this->space.unmap(base, 4 * stage2_page_size);

        const std::vector expected{unmap_call(first, stage2_page_size), unmap_call(second, stage2_page_size)};
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, RepeatedMapAndUnmapKeepsIpaUseBounded)
    {
        uint64_t highest_ipa_end = 0;
        for (uint64_t i = 0; i < 1000; ++i)
        {
            const uint64_t address = base + (i % 7) * 0x1000000;
            const size_t size = (1 + (i % 5)) * stage2_page_size;
            const auto ipa = this->map_and_get_ipa(address, size);
            highest_ipa_end = std::max(highest_ipa_end, ipa + size);
            this->space.unmap(address, size);
        }

        EXPECT_LE(highest_ipa_end, this->stage2.table_pool.ipa + this->stage2.table_pool.size + 5 * stage2_page_size);
    }

    TEST_F(FexHvfAddressSpaceTest, ReleasedIpasAreReusedBestFit)
    {
        const auto a = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        const auto c = this->map_and_get_ipa(base + 0x200000, 2 * stage2_page_size);
        this->map_and_get_ipa(base + 0x300000, stage2_page_size);

        this->space.unmap(base + 0x200000, 2 * stage2_page_size);
        this->space.unmap(base, stage2_page_size);

        EXPECT_EQ(this->map_and_get_ipa(base + 0x400000, stage2_page_size), a);
        EXPECT_EQ(this->map_and_get_ipa(base + 0x500000, 2 * stage2_page_size), c);
    }

    TEST_F(FexHvfAddressSpaceTest, ReleasedNeighboursCoalesce)
    {
        const auto a = this->map_and_get_ipa(base, stage2_page_size);
        this->map_and_get_ipa(base + 0x100000, stage2_page_size);
        this->map_and_get_ipa(base + 0x200000, stage2_page_size);
        this->map_and_get_ipa(base + 0x300000, stage2_page_size);

        this->space.unmap(base, stage2_page_size);
        this->space.unmap(base + 0x200000, stage2_page_size);
        this->space.unmap(base + 0x100000, stage2_page_size);

        EXPECT_EQ(this->map_and_get_ipa(base + 0x400000, 3 * stage2_page_size), a);
    }

    TEST_F(FexHvfAddressSpaceTest, ReleasingTheTopRunLowersTheCursor)
    {
        this->map_and_get_ipa(base, stage2_page_size);
        const auto b = this->map_and_get_ipa(base + 0x100000, stage2_page_size);

        this->space.unmap(base + 0x100000, stage2_page_size);

        EXPECT_EQ(this->map_and_get_ipa(base + 0x200000, 4 * stage2_page_size), b);
    }

    TEST_F(FexHvfAddressSpaceTest, Stage1EntriesCoverEveryGuestPageOfAStage2Page)
    {
        const auto ipa = this->map_and_get_ipa(base, stage2_page_size);

        for (uint64_t offset = 0; offset < stage2_page_size; offset += stage1_page_size)
        {
            const auto descriptor = this->stage2.stage1_descriptor(this->space, base + offset);
            EXPECT_EQ(descriptor & 0b11, 0b11U);
            EXPECT_EQ(descriptor & 0x0000FFFFFFFFF000ULL, ipa + offset);
        }

        EXPECT_EQ(this->stage2.stage1_descriptor(this->space, base + stage2_page_size), 0U);
    }

    TEST_F(FexHvfAddressSpaceTest, UnmapClearsStage1Entries)
    {
        this->map_and_get_ipa(base, stage2_page_size);
        this->space.unmap(base, stage2_page_size);

        for (uint64_t offset = 0; offset < stage2_page_size; offset += stage1_page_size)
        {
            EXPECT_EQ(this->stage2.stage1_descriptor(this->space, base + offset), 0U);
        }
    }

    TEST_F(FexHvfAddressSpaceTest, OnlyRemovingAMappingChangesTheStage1Generation)
    {
        const auto initial = this->space.stage1_generation();

        this->space.map(base, 2 * stage2_page_size, rw);
        this->space.protect(base, stage2_page_size, PROT_READ);
        this->space.refresh_backing(base, 2 * stage2_page_size);
        EXPECT_EQ(this->space.stage1_generation(), initial);

        this->space.unmap(base, stage2_page_size);
        EXPECT_NE(this->space.stage1_generation(), initial);
    }

    TEST_F(FexHvfAddressSpaceTest, RefreshRemapsTheSameIpaWithTheSameProtection)
    {
        const auto ipa = this->map_and_get_ipa(base, 2 * stage2_page_size);
        this->space.protect(base + stage2_page_size, stage2_page_size, PROT_READ);
        this->stage2.calls.clear();

        this->space.refresh_backing(base, 3 * stage2_page_size);

        const std::vector expected{
            unmap_call(ipa, stage2_page_size),
            map_call(base, ipa, stage2_page_size, rw),
            unmap_call(ipa + stage2_page_size, stage2_page_size),
            map_call(base + stage2_page_size, ipa + stage2_page_size, stage2_page_size, PROT_READ),
        };
        EXPECT_EQ(this->stage2.calls, expected);
    }

    TEST_F(FexHvfAddressSpaceTest, ProtectingAnUnmappedPageThrows)
    {
        this->map_and_get_ipa(base, stage2_page_size);
        this->stage2.calls.clear();

        EXPECT_THROW(this->space.protect(base, 2 * stage2_page_size, PROT_READ), std::runtime_error);
        EXPECT_TRUE(this->stage2.calls.empty());
    }

    TEST_F(FexHvfAddressSpaceTest, HostExecutableMemoryIsNeverMapped)
    {
        const auto ipa = this->map_and_get_ipa(base, stage2_page_size);
        this->space.unmap(base, stage2_page_size);
        this->stage2.calls.clear();

        this->stage2.host_executable = true;
        EXPECT_THROW(this->space.map(base, stage2_page_size, PROT_READ | PROT_EXEC), std::runtime_error);
        EXPECT_TRUE(this->stage2.calls.empty());

        this->stage2.host_executable = false;
        EXPECT_EQ(this->map_and_get_ipa(base, stage2_page_size), ipa);
    }

    TEST(FexHvfGuestRuntimeTest, HostCallIdsAreTheHvcImmediate)
    {
        EXPECT_EQ(fex::hvf::encode_hvc(0), 0xD4000002U);
        EXPECT_EQ(fex::hvf::encode_hvc(0x101), 0xD4002022U);
        EXPECT_EQ(fex::hvf::encode_hvc(0x1FF), 0xD4003FE2U);
    }

    TEST(FexHvfGuestRuntimeTest, EveryVectorEntryLeavesTheVmWithItsOwnId)
    {
        const fex::hvf::guest_runtime runtime{};

        for (size_t entry = 0; entry < fex::hvf::vector_count; ++entry)
        {
            const auto address = runtime.vector_table() + (entry * fex::hvf::guest_runtime::vector_stride);
            EXPECT_EQ(instruction_at(address), fex::hvf::encode_hvc(static_cast<uint16_t>(fex::hvf::first_vector_call + entry)));
        }
    }

    TEST(FexHvfGuestRuntimeTest, ExitAndTlbFlushStubsLeaveTheVm)
    {
        const fex::hvf::guest_runtime runtime{};

        EXPECT_EQ(instruction_at(runtime.exit_stub()), fex::hvf::encode_hvc(fex::hvf::exit_call));

        const auto flush = runtime.tlb_flush_stub();
        EXPECT_EQ(instruction_at(flush, 0), 0xD508831FU);
        EXPECT_EQ(instruction_at(flush, 1), 0xD5033B9FU);
        EXPECT_EQ(instruction_at(flush, 2), 0xD5033FDFU);
        EXPECT_EQ(instruction_at(flush, 3), fex::hvf::encode_hvc(fex::hvf::tlb_flush_done_call));
    }

    TEST(FexHvfGuestRuntimeTest, HostCallStubsLeaveTheVmAndReturn)
    {
        const fex::hvf::guest_runtime runtime{};

        for (const size_t index : {size_t{0}, size_t{1}, fex::hvf::max_host_calls - 1})
        {
            const auto stub = runtime.host_call_stub(index);
            EXPECT_GE(stub, runtime.exit_stub());
            EXPECT_LE(stub + 8, runtime.address() + fex::hvf::guest_runtime::size);
            EXPECT_EQ(instruction_at(stub, 0), fex::hvf::encode_hvc(static_cast<uint16_t>(index)));
            EXPECT_EQ(instruction_at(stub, 1), 0xD65F03C0U);
        }

        EXPECT_EQ(runtime.host_call_stub(1) - runtime.host_call_stub(0), 8U);
    }

    TEST(FexHvfGuestRuntimeTest, HostCallsAreSharedPerFunctionAndRegisterUse)
    {
        fex::hvf::host_call_table table{};

        EXPECT_EQ(table.add(0x1000, false), 0U);
        EXPECT_EQ(table.add(0x2000, false), 1U);
        EXPECT_EQ(table.add(0x1000, false), 0U);
        EXPECT_EQ(table.add(0x1000, true), 2U);

        const auto call = table.get(2);
        ASSERT_TRUE(call.has_value());
        EXPECT_EQ(call->function, 0x1000U);
        EXPECT_TRUE(call->uses_vector_registers);
        EXPECT_FALSE(table.get(3).has_value());
    }

    TEST(FexHvfGuestRuntimeTest, HostCallTableRefusesMoreThanTheStubCount)
    {
        fex::hvf::host_call_table table{};
        for (uint64_t i = 0; i < fex::hvf::max_host_calls; ++i)
        {
            table.add(0x1000 + i, false);
        }

        EXPECT_THROW(table.add(0x1000, true), std::runtime_error);
        EXPECT_EQ(table.add(0x1000, false), 0U);
    }

    TEST(FexHvfHostCallTest, ArgumentRegistersAreReplayedAndResultRegistersCaptured)
    {
        fex::hvf::host_call_registers registers{};
        for (uint32_t i = 0; i < 8; ++i)
        {
            registers.x[i] = 1ULL << (i * 4);
        }

        fex::hvf::sogen_fex_hvf_call_host(&registers, reinterpret_cast<uint64_t>(&combine_gprs));

        EXPECT_EQ(registers.x[0], 0x11111111U);
        EXPECT_EQ(registers.x[1], 0x10000001U);
    }

    TEST(FexHvfHostCallTest, IndirectResultRegisterIsReplayed)
    {
        indirect_result result{};
        fex::hvf::host_call_registers registers{};
        registers.x[0] = 6;
        registers.x[1] = 7;
        registers.x[8] = reinterpret_cast<uint64_t>(&result);

        fex::hvf::sogen_fex_hvf_call_host(&registers, reinterpret_cast<uint64_t>(&make_indirect_result));

        EXPECT_EQ(result.values[0], 6U);
        EXPECT_EQ(result.values[1], 7U);
        EXPECT_EQ(result.values[2], 13U);
        EXPECT_EQ(result.values[3], 42U);
    }

    TEST(FexHvfHostCallTest, VectorArgumentsAreReplayedAndVectorResultsCaptured)
    {
        fex::hvf::host_call_registers registers{};
        for (uint32_t i = 0; i < 8; ++i)
        {
            const vector128 value{i + 1, (i + 1) * 100};
            std::memcpy(registers.q[i].data(), &value, sizeof(value));
        }

        fex::hvf::sogen_fex_hvf_call_host(&registers, reinterpret_cast<uint64_t>(&combine_vectors));

        vector128 first{};
        vector128 second{};
        std::memcpy(&first, registers.q[0].data(), sizeof(first));
        std::memcpy(&second, registers.q[1].data(), sizeof(second));
        EXPECT_EQ(first[0], 10U);
        EXPECT_EQ(first[1], 1000U);
        EXPECT_EQ(second[0], 26U);
        EXPECT_EQ(second[1], 2600U);
    }

    TEST(FexHvfModeTest, GuestRunsInTheVmUntilHlt)
    {
        std::string failure{};
        const auto emu = create_hypervisor_emulator(failure);
        if (!emu)
        {
            GTEST_SKIP() << failure;
        }

        EXPECT_EQ(emu->get_name(), "FEX (HVF)");

        memory_manager memory{*emu};
        memory.reserve_host_memory_ranges();
        // mov rax, rbx; add rax, rcx; hlt
        ASSERT_NE(prepare_guest_code(*emu, memory, {0x48, 0x89, 0xD8, 0x48, 0x01, 0xC8, 0xF4}), 0U);

        int interrupt = -1;
        emu->hook_interrupt([&](cpu_interface& cpu, const int vector) {
            interrupt = vector;
            cpu.stop();
        });

        emu->reg(x86_register::rbx, 40);
        emu->reg(x86_register::rcx, 2);
        emu->start(0);

        EXPECT_EQ(interrupt, 13);
        EXPECT_EQ(emu->reg(x86_register::rax), 42U);
    }

    TEST(FexHvfModeTest, GuestStoreToReadOnlyMemoryIsReportedAsWrite)
    {
        std::string failure{};
        const auto emu = create_hypervisor_emulator(failure);
        if (!emu)
        {
            GTEST_SKIP() << failure;
        }

        memory_manager memory{*emu};
        memory.reserve_host_memory_ranges();
        const uint64_t target = memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(target, 0U);
        ASSERT_TRUE(memory.protect_memory(target, 0x1000, memory_permission::read));
        // mov [rax], rbx; hlt
        ASSERT_NE(prepare_guest_code(*emu, memory, {0x48, 0x89, 0x18, 0xF4}), 0U);

        bool observed = false;
        uint64_t observed_address = 0;
        memory_operation observed_operation = memory_operation::read;
        memory_violation_type observed_type = memory_violation_type::unmapped;
        emu->hook_memory_violation(
            [&](cpu_interface& cpu, const uint64_t address, size_t, const memory_operation operation, const memory_violation_type type) {
                observed = true;
                observed_address = address;
                observed_operation = operation;
                observed_type = type;
                cpu.stop();
                return memory_violation_continuation::stop;
            });

        emu->reg(x86_register::rax, target);
        emu->reg(x86_register::rbx, 0x1122334455667788ULL);
        emu->start(0);

        EXPECT_TRUE(observed);
        EXPECT_EQ(observed_address, target);
        EXPECT_EQ(observed_operation, memory_operation::write);
        EXPECT_EQ(observed_type, memory_violation_type::protection);
    }

    TEST(FexHvfModeTest, MisalignedAtomicOnHostAliasedMemoryIsEmulated)
    {
        std::string failure{};
        const auto emu = create_hypervisor_emulator(failure);
        if (!emu)
        {
            GTEST_SKIP() << failure;
        }

        constexpr size_t host_page_size = 0x4000;
        void* const host_buffer = ::mmap(nullptr, host_page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        ASSERT_NE(host_buffer, MAP_FAILED);

        memory_manager memory{*emu};
        memory.reserve_host_memory_ranges();
        const uint64_t target = memory.allocate_host_memory(host_page_size, host_buffer, memory_permission::read_write);
        ASSERT_NE(target, 0U);
        // lock xadd [rax], rbx; hlt
        ASSERT_NE(prepare_guest_code(*emu, memory, {0xF0, 0x48, 0x0F, 0xC1, 0x18, 0xF4}), 0U);

        constexpr uint64_t operand_offset = 0x3C;
        const auto operand = static_cast<uint8_t*>(host_buffer) + operand_offset;
        const uint64_t initial_value = 0x10;
        std::memcpy(operand, &initial_value, sizeof(initial_value));

        bool violation_observed = false;
        emu->hook_memory_violation([&](cpu_interface& cpu, uint64_t, size_t, memory_operation, memory_violation_type) {
            violation_observed = true;
            cpu.stop();
            return memory_violation_continuation::stop;
        });
        emu->hook_interrupt([](cpu_interface& cpu, int) { cpu.stop(); });

        emu->reg(x86_register::rax, target + operand_offset);
        emu->reg(x86_register::rbx, 1);
        emu->start(0);

        uint64_t final_value{};
        std::memcpy(&final_value, operand, sizeof(final_value));
        EXPECT_FALSE(violation_observed);
        EXPECT_EQ(final_value, initial_value + 1);
        EXPECT_EQ(emu->reg(x86_register::rbx), initial_value);

        ::munmap(host_buffer, host_page_size);
    }

    TEST(FexHvfModeTest, EmulatorCanBeCreatedAgainAfterDestruction)
    {
        for (int round = 0; round < 2; ++round)
        {
            std::string failure{};
            const auto emu = create_hypervisor_emulator(failure);
            if (!emu)
            {
                GTEST_SKIP() << failure;
            }

            memory_manager memory{*emu};
            memory.reserve_host_memory_ranges();
            memory.reserve_host_memory_ranges();
            // inc rax; hlt
            ASSERT_NE(prepare_guest_code(*emu, memory, {0x48, 0xFF, 0xC0, 0xF4}), 0U);
            emu->hook_interrupt([](cpu_interface& cpu, int) { cpu.stop(); });
            emu->reg(x86_register::rax, 41);
            emu->start(0);

            EXPECT_EQ(emu->reg(x86_register::rax), 42U);
        }
    }
}

#endif
