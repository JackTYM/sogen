#pragma once

namespace sogen
{
    struct handle;
    struct io_completion_message;
    struct process_context;
    struct worker_factory;
    class emulator_thread;
    class windows_emulator;

    namespace worker_factory_support
    {
        bool enqueue_release_completion(process_context& process, handle worker_factory_handle);
        void on_io_completion_message_dequeued(process_context& process, const io_completion_message& message);
        void mark_worker_ready(process_context& process, worker_factory& factory, const emulator_thread& thread);
        void ensure_minimum_workers(windows_emulator& win_emu, worker_factory& factory);
        void create_workers_for_pending_work(windows_emulator& win_emu);
    }
} // namespace sogen
