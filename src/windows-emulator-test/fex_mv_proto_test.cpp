#include <backend_selection.hpp>
#include <memory_manager.hpp>
#include <fex_x86_64_emulator.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace sogen::test
{
    namespace
    {
        constexpr std::array<uint8_t, 61> guest_mp_writer{0x31, 0xC9, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xF0, 0x0F, 0xC1, 0x02, 0x44, 0x8D, 0x41, 0x01, 0x41, 0xD1, 0xE0, 0x8B, 0x02, 0x44, 0x39, 0xC0, 0x72, 0xF9, 0x49, 0x89, 0xC9, 0x49, 0xC1, 0xE1, 0x08, 0x49, 0x01, 0xF9, 0x41, 0xC7, 0x01, 0x01, 0x00, 0x00, 0x00, 0x41, 0xC7, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xC1, 0x39, 0xF1, 0x72, 0xC7, 0x0F, 0x05};
        constexpr std::array<uint8_t, 63> guest_mp_reader{0x31, 0xC9, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xF0, 0x0F, 0xC1, 0x02, 0x44, 0x8D, 0x41, 0x01, 0x41, 0xD1, 0xE0, 0x8B, 0x02, 0x44, 0x39, 0xC0, 0x72, 0xF9, 0x49, 0x89, 0xC9, 0x49, 0xC1, 0xE1, 0x08, 0x49, 0x01, 0xF9, 0x45, 0x8B, 0x91, 0x80, 0x00, 0x00, 0x00, 0x45, 0x8B, 0x19, 0x41, 0xD1, 0xE2, 0x45, 0x09, 0xDA, 0x44, 0x88, 0x14, 0x0B, 0xFF, 0xC1, 0x39, 0xF1, 0x72, 0xC5, 0x0F, 0x05};
        constexpr std::array<uint8_t, 39> guest_chase{0x41, 0xB8, 0xFF, 0xFF, 0x01, 0x00, 0xBE, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x04, 0xF7, 0x48, 0x01, 0xF0, 0x48, 0x89, 0x04, 0xF7, 0x48, 0x01, 0xC6, 0x48, 0x83, 0xC6, 0x07, 0x4C, 0x21, 0xC6, 0x48, 0xFF, 0xC9, 0x75, 0xE6, 0x0F, 0x05};
        constexpr std::array<uint8_t, 27> guest_alu{0x31, 0xC0, 0xBA, 0x39, 0x30, 0x00, 0x00, 0x69, 0xC0, 0x6D, 0x4E, 0xC6, 0x41, 0x01, 0xD0, 0x31, 0xC8, 0xC1, 0xC0, 0x05, 0x48, 0xFF, 0xC9, 0x75, 0xEE, 0x0F, 0x05};

        constexpr std::array<uint8_t, 12> guest_spin{0x48, 0xFF, 0xC0, 0x48, 0x8D, 0x15, 0xF6, 0xFF, 0xFF, 0xFF, 0xFF, 0xE2};
        constexpr std::array<uint8_t, 4> guest_syscall_loop{0x0F, 0x05, 0xEB, 0xFC};
        constexpr uint64_t batch_size = 16384;
        constexpr uint64_t long_mode_code_selector = 0x08;
        constexpr uint64_t long_mode_code_descriptor = 0x00AF9B000000FFFF;
        constexpr size_t page = 0x1000;

        bool use_hypervisor()
        {
            const char* mode = std::getenv("MV_MODE");
            return mode != nullptr && std::string_view{mode} == "hvf";
        }

        struct rig
        {
            std::unique_ptr<x86_64_emulator> emu;
            std::unique_ptr<memory_manager> memory;
            uint64_t code{};
            uint64_t data{};
            uint64_t gdt{};
            static constexpr uint64_t data_size = 64ull << 20;
            static constexpr uint64_t code_size = 2ull << 20;
            std::atomic<uint64_t> syscalls[2]{};
            uint64_t syscall_limit{};

            explicit rig(const size_t vcpus)
            {
                emu = use_hypervisor() ? fex::create_hypervisor_x86_64_emulator(vcpus) : fex::create_x86_64_emulator(vcpus);
                memory = std::make_unique<memory_manager>(*emu);
                memory->reserve_host_memory_ranges();
                code = memory->allocate_memory(code_size, memory_permission::read_write);
                data = memory->allocate_memory(data_size, memory_permission::read_write);
                gdt = memory->allocate_memory(page, memory_permission::read_write);
                emu->write_memory<uint64_t>(gdt + long_mode_code_selector, long_mode_code_descriptor);
                write_code(0x000, guest_mp_writer);
                write_code(0x400, guest_mp_reader);
                write_code(0x800, guest_chase);
                write_code(0xC00, guest_alu);
                write_code(0x1000, guest_syscall_loop);
                write_code(0x1400, guest_spin);
                memory->protect_memory(code, code_size, memory_permission::read_exec);
                for (size_t i = 0; i < vcpus; ++i)
                {
                    auto& cpu = emu->get_cpu(i);
                    cpu.load_gdt(gdt, page);
                    cpu.reg<uint16_t>(x86_register::cs, long_mode_code_selector);
                }
                emu->hook_instruction(x86_hookable_instructions::syscall, [this](cpu_interface& raw, uint64_t) {
                    auto& cpu = dynamic_cast<x86_64_cpu&>(raw);
                    if (syscall_limit == 0 || ++syscalls[cpu.index()] >= syscall_limit)
                    {
                        cpu.stop();
                    }
                    return instruction_hook_continuation::skip_instruction;
                });
            }

            template <size_t N>
            void write_code(const uint64_t offset, const std::array<uint8_t, N>& bytes)
            {
                memory->write_memory(code + offset, bytes.data(), bytes.size());
            }

            void clear(const uint64_t offset, const size_t size)
            {
                const std::vector<uint8_t> zeros(size);
                memory->write_memory(data + offset, zeros.data(), zeros.size());
            }
        };

        struct start_gate
        {
            std::atomic<int> arrived{0};
            int parties{};
            void wait()
            {
                arrived.fetch_add(1);
                while (arrived.load() < parties)
                {
                }
            }
        };

        double run_parallel(rig& r, const std::vector<std::function<void(x86_64_cpu&)>>& setups, const uint64_t entry_offset_for_all,
                            const std::vector<uint64_t>& entries)
        {
            (void)entry_offset_for_all;
            start_gate gate{.parties = static_cast<int>(setups.size()) + 1};
            std::vector<std::thread> threads;
            for (size_t i = 0; i < setups.size(); ++i)
            {
                threads.emplace_back([&, i] {
                    auto& cpu = r.emu->get_cpu(i);
                    setups[i](cpu);
                    cpu.reg(x86_register::rip, entries[i]);
                    gate.wait();
                    cpu.start(0);
                });
            }
            gate.wait();
            const auto t0 = std::chrono::steady_clock::now();
            for (auto& t : threads)
            {
                t.join();
            }
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
    }

    TEST(FexMvProto, MessagePassingNeverShowsTheForbiddenOutcome)
    {
        const int batches = std::getenv("MV_BATCHES") ? std::atoi(std::getenv("MV_BATCHES")) : 100;
        rig r{2};
        const uint64_t cells = r.data;
        const uint64_t results = r.data + batch_size * 256;
        const uint64_t barrier = results + 4 * batch_size;
        uint64_t histogram[4]{};
        for (int batch = 0; batch < batches; ++batch)
        {
            r.clear(0, batch_size * 256 + 4 * batch_size + 4096);
            const auto common = [&](x86_64_cpu& cpu) {
                cpu.reg(x86_register::rdi, cells);
                cpu.reg(x86_register::rsi, batch_size);
                cpu.reg(x86_register::rdx, barrier);
                cpu.reg(x86_register::rbx, results);
                cpu.reg(x86_register::rsp, r.data + rig::data_size - 0x1000 - 0x2000 * (&cpu == &r.emu->get_cpu(1)));
            };
            run_parallel(r, {common, common}, 0, {r.code, r.code + 0x400});
            std::vector<uint8_t> out(batch_size);
            r.memory->read_memory(results, out.data(), out.size());
            for (const auto value : out)
            {
                ++histogram[value & 3];
            }
        }
        std::printf("MP mode=%s tso_env=%s iters=%llu  r1r2: 00=%llu 01=%llu 10(FORBIDDEN under TSO)=%llu 11=%llu\n",
                    use_hypervisor() ? "hvf" : "in-process", std::getenv("EMULATOR_FEX_HVF_NOTSO") ? "software" : (std::getenv("EMULATOR_FEX_HVF_FAKETSO") ? "FAKE(fex hw-tso, EnTSO off)" : "default"),
                    static_cast<unsigned long long>(batches * batch_size), static_cast<unsigned long long>(histogram[0]),
                    static_cast<unsigned long long>(histogram[1]), static_cast<unsigned long long>(histogram[2]),
                    static_cast<unsigned long long>(histogram[3]));
        if (!std::getenv("EMULATOR_FEX_HVF_FAKETSO"))
        {
            EXPECT_EQ(histogram[2], 0u);
        }
    }

    TEST(FexMvProto, Throughput)
    {
        rig r{2};
        const auto setup = [&](const uint64_t iterations, const bool chase) {
            return [&, iterations, chase](x86_64_cpu& cpu) {
                const bool second = &cpu == &r.emu->get_cpu(1);
                cpu.reg(x86_register::rdi, r.data + (16ull << 20) + (second ? (2ull << 20) : 0));
                cpu.reg(x86_register::rcx, iterations);
                (void)chase;
                cpu.reg(x86_register::rsp, r.data + rig::data_size - 0x1000 - 0x2000 * second);
            };
        };
        struct workload
        {
            const char* name;
            uint64_t entry;
            uint64_t iterations;
        };
        const workload workloads[] = {{"chase", r.code + 0x800, 40'000'000}, {"alu", r.code + 0xC00, 200'000'000}};
        for (const auto& w : workloads)
        {
            run_parallel(r, {setup(100000, true)}, 0, {w.entry});
            for (int rep = 0; rep < 3; ++rep)
            {
                const double one = run_parallel(r, {setup(w.iterations, true)}, 0, {w.entry});
                const double two = run_parallel(r, {setup(w.iterations, true), setup(w.iterations, true)}, 0, {w.entry, w.entry});
                std::printf("THROUGHPUT mode=%s %s iterations=%llu  1vcpu=%.1f ms  2vcpu(each same work)=%.1f ms\n",
                            use_hypervisor() ? "hvf" : "in-process", w.name, static_cast<unsigned long long>(w.iterations), one, two);
            }
        }
    }

    namespace
    {
        void write_block_slots(rig& r, const uint64_t offset, const size_t blocks)
        {
            std::vector<uint8_t> image(blocks * 16, 0xCC);
            for (size_t i = 0; i < blocks; ++i)
            {
                uint8_t* slot = image.data() + i * 16;
                if (i + 1 == blocks)
                {
                    slot[0] = 0x0F;
                    slot[1] = 0x05;
                    continue;
                }
                // inc eax; lea rdx, [rip+7]; jmp rdx -- an indirect jump ends the FEX block
                const uint8_t body[] = {0xFF, 0xC0, 0x48, 0x8D, 0x15, 0x07, 0x00, 0x00, 0x00, 0xFF, 0xE2};
                std::memcpy(slot, body, sizeof(body));
            }
            r.memory->protect_memory(r.code, rig::code_size, memory_permission::read_write);
            r.memory->write_memory(r.code + offset, image.data(), image.size());
            r.memory->protect_memory(r.code, rig::code_size, memory_permission::read_exec);
        }
    }

    TEST(FexMvProto, SyscallStorm)
    {
        const uint64_t limit = std::getenv("MV_SYSCALLS") ? std::strtoull(std::getenv("MV_SYSCALLS"), nullptr, 10) : 300000;
        for (const size_t vcpus : {1, 2, 1, 2, 1, 2})
        {
            rig r{2};
            r.syscall_limit = limit;
            const auto setup = [&](x86_64_cpu& cpu) {
                const bool second = cpu.index() == 1;
                cpu.reg(x86_register::rsp, r.data + rig::data_size - 0x1000 - 0x2000 * second);
            };
            std::vector<std::function<void(x86_64_cpu&)>> setups(vcpus, setup);
            const std::vector<uint64_t> entries(vcpus, r.code + 0x1000);
            const double ms = run_parallel(r, setups, 0, entries);
            std::printf("SYSCALLS mode=%s vcpus=%zu per-vcpu=%llu  wall=%.1f ms  => %.2f us per syscall per vcpu\n",
                        use_hypervisor() ? "hvf" : "in-process", vcpus, static_cast<unsigned long long>(limit), ms,
                        ms * 1000.0 / static_cast<double>(limit));
        }
    }

    TEST(FexMvProto, CompileStorm)
    {
        const size_t blocks = std::getenv("MV_BLOCKS") ? std::strtoul(std::getenv("MV_BLOCKS"), nullptr, 10) : 30000;
        for (const size_t vcpus : {1, 2, 1, 2, 1, 2})
        {
            rig r{2};
            write_block_slots(r, 0x10000, blocks);
            const uint64_t second_offset = 0x110000;
            {
                std::vector<uint8_t> image(blocks * 16, 0xCC);
                for (size_t i = 0; i < blocks; ++i)
                {
                    uint8_t* slot = image.data() + i * 16;
                    if (i + 1 == blocks)
                    {
                        slot[0] = 0x0F;
                        slot[1] = 0x05;
                        continue;
                    }
                    const uint8_t body[] = {0xFF, 0xC0, 0x48, 0x8D, 0x15, 0x07, 0x00, 0x00, 0x00, 0xFF, 0xE2};
                    std::memcpy(slot, body, sizeof(body));
                }
                r.memory->protect_memory(r.code, rig::code_size, memory_permission::read_write);
                r.memory->write_memory(r.code + second_offset, image.data(), image.size());
                r.memory->protect_memory(r.code, rig::code_size, memory_permission::read_exec);
            }
            const auto setup = [&](x86_64_cpu& cpu) {
                const bool second = cpu.index() == 1;
                cpu.reg(x86_register::rsp, r.data + rig::data_size - 0x1000 - 0x2000 * second);
                cpu.reg(x86_register::rax, 0);
            };
            std::vector<std::function<void(x86_64_cpu&)>> setups(vcpus, setup);
            const std::vector<uint64_t> entries{r.code + 0x10000, r.code + second_offset};
            const double ms = run_parallel(r, setups, 0, std::vector<uint64_t>(entries.begin(), entries.begin() + static_cast<long>(vcpus)));
            std::printf("COMPILE mode=%s vcpus=%zu blocks-per-vcpu=%zu  cold-run wall=%.1f ms => %.2f us per block\n",
                        use_hypervisor() ? "hvf" : "in-process", vcpus, blocks, ms, ms * 1000.0 / static_cast<double>(blocks));
        }
    }

    TEST(FexMvProto, CodeRewrittenAfterBothVcpusRanItRunsAsTheNewBytes)
    {
        rig r{2};
        const uint64_t address = r.code + 0x2000;
        uint8_t code[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x05}; // mov eax, imm32; syscall
        r.memory->write_memory(address, code, sizeof(code));
        const auto run = [&](const size_t index) {
            auto& cpu = r.emu->get_cpu(index);
            cpu.reg(x86_register::rsp, r.data + rig::data_size - 0x1000 - 0x2000 * index);
            cpu.reg(x86_register::rip, address);
            cpu.start(0);
            return cpu.reg<uint32_t>(x86_register::eax);
        };
        EXPECT_EQ(run(0), 1u);
        EXPECT_EQ(run(1), 1u);
        const uint32_t rewritten = 2;
        r.memory->write_memory(address + 1, &rewritten, sizeof(rewritten));
        EXPECT_EQ(run(1), 2u);
        EXPECT_EQ(run(0), 2u);
        const uint32_t again = 3;
        r.memory->write_memory(address + 1, &again, sizeof(again));
        EXPECT_EQ(run(0), 3u);
        EXPECT_EQ(run(1), 3u);
    }

    TEST(FexMvProto, StopFromAnotherThreadLatency)
    {
        rig r{2};
        for (int rep = 0; rep < 6; ++rep)
        {
            std::atomic<int> running{0};
            std::vector<std::thread> threads;
            for (size_t i = 0; i < 2; ++i)
            {
                threads.emplace_back([&, i] {
                    auto& cpu = r.emu->get_cpu(i);
                    cpu.reg(x86_register::rcx, uint64_t{1} << 40);
                    cpu.reg(x86_register::rsp, r.data + rig::data_size - 0x1000 - 0x2000 * i);
                    cpu.reg(x86_register::rip, r.code + 0x1400);
                    running.fetch_add(1);
                    cpu.start(0);
                });
            }
            while (running.load() < 2)
            {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            const auto t0 = std::chrono::steady_clock::now();
            r.emu->get_cpu(0).stop();
            r.emu->get_cpu(1).stop();
            for (auto& t : threads)
            {
                t.join();
            }
            std::printf("STOP mode=%s rep=%d latency=%.3f ms\n", use_hypervisor() ? "hvf" : "in-process", rep,
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        }
    }
}
