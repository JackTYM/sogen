#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace sogen::fex::hvf
{
    constexpr uint16_t first_vector_call = 0x100;
    constexpr uint16_t tlb_flush_done_call = 0x1F1;
    constexpr uint16_t exit_call = 0x1FF;

    constexpr size_t vector_count = 16;
    constexpr size_t max_host_calls = 128;

    constexpr uint32_t encode_hvc(const uint16_t immediate)
    {
        return 0xD4000002U | (static_cast<uint32_t>(immediate) << 5);
    }

    // The code the vCPU runs besides JIT output, on one page: an EL1 vector table whose entries all leave the VM,
    // the stub the dispatcher returns to, a stage-1 TLB flush, and the stubs the JIT's host calls are pointed at.
    class guest_runtime
    {
      public:
        static constexpr size_t size = 0x4000;
        static constexpr size_t vector_stride = 0x80;

        guest_runtime();
        ~guest_runtime();

        guest_runtime(const guest_runtime&) = delete;
        guest_runtime& operator=(const guest_runtime&) = delete;
        guest_runtime(guest_runtime&&) = delete;
        guest_runtime& operator=(guest_runtime&&) = delete;

        uint64_t address() const
        {
            return reinterpret_cast<uint64_t>(this->page_);
        }

        uint64_t vector_table() const
        {
            return this->address();
        }

        uint64_t exit_stub() const;
        uint64_t tlb_flush_stub() const;
        uint64_t host_call_stub(size_t index) const;

      private:
        uint32_t* page_{};
    };

    struct host_call
    {
        uint64_t function{};
        bool uses_vector_registers{};
    };

    class host_call_table
    {
      public:
        size_t add(uint64_t function, bool uses_vector_registers);
        std::optional<host_call> get(size_t index) const;

      private:
        mutable std::mutex mutex_;
        std::vector<host_call> calls_;
    };

    struct host_call_registers
    {
        std::array<uint64_t, 9> x{};
        alignas(16) std::array<std::array<uint8_t, 16>, 8> q{};
    };

    // Calls function with x0-x8 and q0-q7 loaded from registers, and stores the result registers x0, x1, q0 and q1
    // back into it.
    extern "C" void sogen_fex_hvf_call_host(host_call_registers* registers, uint64_t function);
}
