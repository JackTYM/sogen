#pragma once

#include "hvf_address_space.hpp"
#include "hvf_guest_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace sogen::fex::hvf
{
    class vcpu;

    class hypervisor_stage2 final : public stage2_mapper
    {
      public:
        bool is_host_executable(uint64_t host_address, size_t size) const override;
        void map(uint64_t host_address, uint64_t ipa, size_t size, int prot) override;
        void unmap(uint64_t ipa, size_t size) override;
        void protect(uint64_t ipa, size_t size, int prot) override;
    };

    // Hypervisor.framework allows one VM per process, and FEXCore's allocator hooks, which mirror its memory into
    // the VM, are process-wide as well, so the VM lives for the rest of the process once created.
    class vm
    {
      public:
        // Creates the VM on first use. Throws, with the reason, when Hypervisor.framework or hardware TSO is
        // unavailable.
        static vm& get();

        vm(const vm&) = delete;
        vm& operator=(const vm&) = delete;
        vm(vm&&) = delete;
        vm& operator=(vm&&) = delete;

        void map(uint64_t address, size_t size, int prot);
        void unmap(uint64_t address, size_t size);
        void protect(uint64_t address, size_t size, int prot);
        void refresh_backing(uint64_t address, size_t size);

        uint64_t stage1_root() const;
        uint64_t stage1_generation() const;

        const guest_runtime& runtime() const
        {
            return this->runtime_;
        }

        // Returns the stub the JIT calls instead of function.
        uint64_t add_host_call(uint64_t function, bool uses_vector_registers);
        std::optional<host_call> get_host_call(size_t index) const;

        void attach(vcpu& cpu);
        void detach(vcpu& cpu);

      private:
        vm();
        ~vm() = default;

        std::mutex mutex_;
        hypervisor_stage2 stage2_;
        std::optional<address_space> space_;
        guest_runtime runtime_;
        host_call_table host_calls_;
        std::vector<vcpu*> vcpus_;

        template <typename F>
        void edit(F&& f);
    };
}
