#include "emulation_test_utils.hpp"

#if defined(__APPLE__) || defined(__ANDROID__)
#include <memory_manager.hpp>

#include <array>
#endif

namespace sogen::test
{
    namespace
    {
        std::unique_ptr<x86_64_emulator> try_create_fex_emulator()
        {
            try
            {
                return create_x86_64_emulator(backend_type::fex);
            }
            catch (const std::exception&)
            {
                return {};
            }
        }

#if defined(__APPLE__) || defined(__ANDROID__)
        constexpr uint16_t long_mode_code_selector = 0x08;
        constexpr uint64_t long_mode_code_descriptor = 0x00AF9B000000FFFF;

        uint64_t setup_guest_code(x86_64_emulator& emu, memory_manager& memory, const std::array<uint8_t, 11>& guest_code)
        {
            // The Apple backend's host-range rescan is windowed and skips the __PAGEZERO gap, which only
            // a full scan records; windows_emulator runs one at startup, so a bare harness must too.
            memory.reserve_host_memory_ranges();

            constexpr size_t page_size = 0x1000;
            const uint64_t code = memory.allocate_memory(page_size, memory_permission::read_write);
            const uint64_t gdt = memory.allocate_memory(page_size, memory_permission::read_write);
            if (code == 0 || gdt == 0)
            {
                return 0;
            }

            emu.write_memory<uint64_t>(gdt + long_mode_code_selector, long_mode_code_descriptor);
            emu.load_gdt(gdt, page_size);
            emu.reg<uint16_t>(x86_register::cs, long_mode_code_selector);

            memory.write_memory(code, guest_code.data(), guest_code.size());
            if (!memory.protect_memory(code, page_size, memory_permission::read_exec))
            {
                return 0;
            }

            return code;
        }

        // FEX raises HLT as #GP with RIP left on the hlt, so without a stopping hook start(0) would
        // re-execute it forever.
        void stop_on_interrupt(x86_64_emulator& emu)
        {
            emu.hook_interrupt([](cpu_interface& cpu, int) { cpu.stop(); });
        }
#endif
    }

#if defined(__APPLE__) || defined(__ANDROID__)
    TEST(FexSingleStepTest, StartOneExecutesExactlyOneInstruction)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        memory_manager memory{*emu};
        // mov eax, 1; mov eax, 2; hlt
        constexpr std::array<uint8_t, 11> guest_code = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);

        emu->reg(x86_register::rip, code);
        emu->start(1);

        EXPECT_EQ(emu->reg<uint64_t>(x86_register::rip), code + 5);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 1u);

        emu->start(1);

        EXPECT_EQ(emu->reg<uint64_t>(x86_register::rip), code + 10);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 2u);
    }

    TEST(FexSingleStepTest, StartTwoStillThrows)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        EXPECT_THROW(emu->start(2), std::runtime_error);
    }

    TEST(FexSingleStepTest, GuestSetTrapFlagStillReachesInterruptHooks)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        memory_manager memory{*emu};
        // pushfq; or dword [rsp], 0x100; popfq; nop; hlt
        constexpr std::array<uint8_t, 11> guest_code = {0x9C, 0x81, 0x0C, 0x24, 0x00, 0x01, 0x00, 0x00, 0x9D, 0x90, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);

        constexpr size_t page_size = 0x1000;
        const uint64_t stack = memory.allocate_memory(page_size, memory_permission::read_write);
        ASSERT_NE(stack, 0u);

        std::vector<int> vectors{};
        emu->hook_interrupt([&](cpu_interface& cpu, const int vector) {
            vectors.push_back(vector);
            cpu.stop();
        });

        emu->reg(x86_register::rip, code);
        emu->reg(x86_register::rsp, stack + page_size / 2);
        emu->start(0);

        ASSERT_EQ(vectors.size(), 1u);
        EXPECT_EQ(vectors.front(), 1);
        EXPECT_EQ(emu->reg<uint64_t>(x86_register::rip), code + 10);
    }

    TEST(FexBreakpointTest, Int3BreakpointStopsAtAddressAndCanContinue)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        emu->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);

        memory_manager memory{*emu};
        // mov eax, 1; mov eax, 2; hlt
        constexpr std::array<uint8_t, 11> guest_code = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);
        stop_on_interrupt(*emu);

        const uint64_t breakpoint_address = code + 5;
        int hit_count = 0;
        auto* hook = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface& cpu, const uint64_t address) {
            ++hit_count;
            EXPECT_EQ(address, breakpoint_address);
            cpu.stop();
        });
        ASSERT_NE(hook, nullptr);

        emu->reg(x86_register::rip, code);
        emu->start(0);

        EXPECT_EQ(hit_count, 1);
        EXPECT_EQ(emu->reg<uint64_t>(x86_register::rip), breakpoint_address);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 1u);

        uint8_t byte_at_breakpoint = 0;
        ASSERT_TRUE(emu->try_read_memory(breakpoint_address, &byte_at_breakpoint, 1));
        EXPECT_EQ(byte_at_breakpoint, 0xB8);

        emu->delete_hook(hook);
        emu->start(0);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 2u);
    }

    TEST(FexBreakpointTest, RefcountedBreakpointSurvivesOneDeletion)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        emu->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);

        memory_manager memory{*emu};
        constexpr std::array<uint8_t, 11> guest_code = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);

        const uint64_t breakpoint_address = code + 5;
        int hits_a = 0;
        int hits_b = 0;
        auto* hook_a = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface& cpu, uint64_t) {
            ++hits_a;
            cpu.stop();
        });
        auto* hook_b = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface&, uint64_t) { ++hits_b; });

        emu->reg(x86_register::rip, code);
        emu->start(0);
        EXPECT_EQ(hits_a, 1);
        EXPECT_EQ(hits_b, 1);

        emu->delete_hook(hook_a);

        uint8_t byte_at_breakpoint = 0;
        ASSERT_TRUE(emu->try_read_memory(breakpoint_address, &byte_at_breakpoint, 1));
        EXPECT_EQ(byte_at_breakpoint, 0xB8);

        emu->delete_hook(hook_b);
    }

    TEST(FexBreakpointTest, NonStoppingBreakpointHitsAgainWhenResumedOnIt)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        emu->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);

        memory_manager memory{*emu};
        // nop; mov eax, 1; hlt
        constexpr std::array<uint8_t, 11> guest_code = {0x90, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xF4, 0x90, 0x90, 0x90, 0x90};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);
        stop_on_interrupt(*emu);

        const uint64_t breakpoint_address = code + 1;
        int hit_count = 0;
        auto* hook = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface&, uint64_t) { ++hit_count; });

        emu->reg(x86_register::rip, code);
        emu->start(0);
        EXPECT_EQ(hit_count, 1);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 1u);
        EXPECT_EQ(*reinterpret_cast<const volatile uint8_t*>(breakpoint_address), 0xCC);

        emu->reg(x86_register::rip, breakpoint_address);
        emu->reg<uint32_t>(x86_register::eax, 0);
        EXPECT_NO_THROW(emu->start(0));
        EXPECT_EQ(hit_count, 2);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 1u);

        emu->delete_hook(hook);
    }

    TEST(FexBreakpointTest, RefcountedBreakpointStaysPlantedUntilLastDeletion)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        emu->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);

        memory_manager memory{*emu};
        constexpr std::array<uint8_t, 11> guest_code = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);
        stop_on_interrupt(*emu);

        const uint64_t breakpoint_address = code + 5;
        const auto* planted_byte = reinterpret_cast<const volatile uint8_t*>(breakpoint_address);
        int hits_a = 0;
        int hits_b = 0;
        auto* hook_a = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface&, uint64_t) { ++hits_a; });
        auto* hook_b = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface&, uint64_t) { ++hits_b; });
        EXPECT_EQ(*planted_byte, 0xCC);

        emu->delete_hook(hook_a);
        EXPECT_EQ(*planted_byte, 0xCC);

        emu->delete_hook(hook_b);
        EXPECT_EQ(*planted_byte, 0xB8);

        int hits_c = 0;
        auto* hook_c = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface& cpu, uint64_t) {
            ++hits_c;
            cpu.stop();
        });
        EXPECT_EQ(*planted_byte, 0xCC);

        emu->reg(x86_register::rip, code);
        emu->start(0);
        EXPECT_EQ(hits_a, 0);
        EXPECT_EQ(hits_b, 0);
        EXPECT_EQ(hits_c, 1);
        EXPECT_EQ(emu->reg<uint64_t>(x86_register::rip), breakpoint_address);

        emu->delete_hook(hook_c);
        emu->start(0);
        EXPECT_EQ(hits_c, 1);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 2u);
    }

    TEST(FexBreakpointTest, GuestWriteToBreakpointUpdatesOriginalAndKeepsItPlanted)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        emu->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);

        memory_manager memory{*emu};
        constexpr std::array<uint8_t, 11> guest_code = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);
        stop_on_interrupt(*emu);

        const uint64_t breakpoint_address = code + 5;
        int hit_count = 0;
        auto* hook = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface& cpu, uint64_t) {
            ++hit_count;
            cpu.stop();
        });

        // mov ecx, 3
        constexpr std::array<uint8_t, 5> replacement = {0xB9, 0x03, 0x00, 0x00, 0x00};
        emu->write_memory(breakpoint_address, replacement.data(), replacement.size());

        uint8_t byte_at_breakpoint = 0;
        ASSERT_TRUE(emu->try_read_memory(breakpoint_address, &byte_at_breakpoint, 1));
        EXPECT_EQ(byte_at_breakpoint, 0xB9);
        EXPECT_EQ(*reinterpret_cast<const volatile uint8_t*>(breakpoint_address), 0xCC);

        emu->reg(x86_register::rip, code);
        emu->start(0);
        EXPECT_EQ(hit_count, 1);

        emu->delete_hook(hook);
        emu->start(0);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 1u);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::ecx), 3u);
    }

    TEST(FexBreakpointTest, BreakpointIsAppliedWhenItsPageIsMappedAndRemapped)
    {
        const auto emu = try_create_fex_emulator();
        if (!emu)
        {
            GTEST_SKIP() << "FEX backend is not available";
        }

        emu->set_memory_execution_hook_mode(hook_interface::memory_execution_hook_mode::int3);

        memory_manager memory{*emu};
        constexpr std::array<uint8_t, 11> guest_code = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xF4};
        const uint64_t code = setup_guest_code(*emu, memory, guest_code);
        ASSERT_NE(code, 0u);
        stop_on_interrupt(*emu);

        constexpr size_t page_size = 0x1000;
        const auto remap_code_page = [&] {
            ASSERT_TRUE(memory.decommit_memory(code, page_size));
            ASSERT_TRUE(memory.commit_memory(code, page_size, memory_permission::read_write));
            memory.write_memory(code, guest_code.data(), guest_code.size());
            ASSERT_TRUE(memory.protect_memory(code, page_size, memory_permission::read_exec));
        };

        ASSERT_TRUE(memory.decommit_memory(code, page_size));

        const uint64_t breakpoint_address = code + 5;
        int hit_count = 0;
        auto* hook = emu->hook_memory_execution(breakpoint_address, [&](cpu_interface& cpu, uint64_t) {
            ++hit_count;
            cpu.stop();
        });

        ASSERT_TRUE(memory.commit_memory(code, page_size, memory_permission::read_write));
        memory.write_memory(code, guest_code.data(), guest_code.size());
        ASSERT_TRUE(memory.protect_memory(code, page_size, memory_permission::read_exec));
        EXPECT_EQ(*reinterpret_cast<const volatile uint8_t*>(breakpoint_address), 0xCC);

        remap_code_page();
        EXPECT_EQ(*reinterpret_cast<const volatile uint8_t*>(breakpoint_address), 0xCC);

        uint8_t byte_at_breakpoint = 0;
        ASSERT_TRUE(emu->try_read_memory(breakpoint_address, &byte_at_breakpoint, 1));
        EXPECT_EQ(byte_at_breakpoint, 0xB8);

        emu->reg(x86_register::rip, code);
        emu->start(0);
        EXPECT_EQ(hit_count, 1);
        EXPECT_EQ(emu->reg<uint64_t>(x86_register::rip), breakpoint_address);

        emu->delete_hook(hook);
        emu->start(0);
        EXPECT_EQ(emu->reg<uint32_t>(x86_register::eax), 2u);
    }
#endif
} // namespace sogen::test
