#include "emulation_test_utils.hpp"

#include <devices/named_pipe.hpp>

namespace sogen::test
{
    namespace
    {
        emulator_thread& bind_vcpu_to_new_thread(windows_emulator& emu)
        {
            const auto thread_handle = emu.process.create_thread(emu.memory, 0x1000, 0, 0x1000, 0, true);
            auto* thread = emu.process.threads.get(thread_handle);
            emu.vcpu(0).active_thread = thread;
            return *thread;
        }

        io_device_context make_read_context(windows_emulator& emu, const uint32_t buffer_length)
        {
            io_device_context ctx{emu.memory};
            ctx.vcpu = &emu.vcpu(0);
            ctx.output_buffer = emu.memory.allocate_memory(0x1000, memory_permission::read_write);
            ctx.output_buffer_length = buffer_length;
            return ctx;
        }
    }

    // A synchronous (non-overlapped) pipe handle must keep blocking exactly as before: real Windows
    // blocks a synchronous ReadFile with no data available until data arrives.
    TEST(NamedPipeTest, SynchronousPendedReadParksCallingThread)
    {
        auto emu = create_empty_emulator();
        auto& thread = bind_vcpu_to_new_thread(emu);

        named_pipe pipe{};
        pipe.name = u"\\Device\\NamedPipe\\test";
        pipe.is_synchronous_handle = true;

        const auto ctx = make_read_context(emu, 64);
        const auto status = pipe.try_deliver_read(emu, ctx);

        ASSERT_EQ(status, STATUS_PENDING);
        ASSERT_TRUE(emu.vcpu(0).switch_thread.load());
        ASSERT_EQ(thread.await_objects.size(), 1u);
        ASSERT_EQ(thread.await_objects[0], pipe.read_ready_event);
        ASSERT_TRUE(pipe.pending_read.has_value());
    }

    // An overlapped pipe handle must return STATUS_PENDING to the calling thread immediately, without
    // parking it -- this is finding #277/#278's fix: real Windows never blocks the caller of an
    // asynchronous NtReadFile, which is what lets Chrome_IOThread's DoWork() loop keep draining its
    // task queue instead of freezing on the first pended read.
    TEST(NamedPipeTest, OverlappedPendedReadReturnsPendingWithoutParkingCallingThread)
    {
        auto emu = create_empty_emulator();
        auto& thread = bind_vcpu_to_new_thread(emu);

        named_pipe pipe{};
        pipe.name = u"\\Device\\NamedPipe\\test";
        pipe.is_synchronous_handle = false;

        const auto ctx = make_read_context(emu, 64);
        const auto status = pipe.try_deliver_read(emu, ctx);

        ASSERT_EQ(status, STATUS_PENDING);
        ASSERT_FALSE(emu.vcpu(0).switch_thread.load());
        ASSERT_TRUE(thread.await_objects.empty());
        ASSERT_TRUE(pipe.pending_read.has_value());
    }

    // The non-blocking path must still deliver the read once data actually arrives, through the same
    // work()/complete_read() mechanism a synchronous pipe's replay-on-wake uses.
    TEST(NamedPipeTest, OverlappedPendedReadStillCompletesAsynchronouslyOnceDataArrives)
    {
        auto emu = create_empty_emulator();
        bind_vcpu_to_new_thread(emu);

        named_pipe pipe{};
        pipe.name = u"\\Device\\NamedPipe\\test";
        pipe.is_synchronous_handle = false;

        auto ctx = make_read_context(emu, 64);
        const auto iosb_address = emu.memory.allocate_memory(0x1000, memory_permission::read_write);
        ASSERT_NE(iosb_address, 0u);
        ctx.io_status_block = emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>>{emu.memory, iosb_address};

        const auto status = pipe.try_deliver_read(emu, ctx);
        ASSERT_EQ(status, STATUS_PENDING);
        ASSERT_FALSE(emu.vcpu(0).switch_thread.load());

        pipe.write_queue.emplace_back("hello");
        pipe.work(emu);

        ASSERT_FALSE(pipe.pending_read.has_value());

        const auto iosb = ctx.io_status_block.read();
        ASSERT_EQ(iosb.Information, 5u);

        std::string data(5, '\0');
        ASSERT_TRUE(emu.memory.try_read_memory(ctx.output_buffer, data.data(), data.size()));
        ASSERT_EQ(data, "hello");

        auto* e = emu.process.events.get(pipe.read_ready_event);
        ASSERT_NE(e, nullptr);
        ASSERT_TRUE(e->signaled);
    }
} // namespace sogen::test
