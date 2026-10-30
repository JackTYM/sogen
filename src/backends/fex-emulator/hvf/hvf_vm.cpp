#include "hvf_vm.hpp"
#include "hvf_vcpu.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <Hypervisor/Hypervisor.h>

namespace sogen::fex::hvf
{
    namespace
    {
        // The vCPU's TCR_EL1 limits stage-1 output addresses to 40 bits.
        constexpr uint32_t max_ipa_bits = 40;

        [[noreturn]] void throw_hv_error(const char* what, const hv_return_t result)
        {
            std::array<char, 160> message{};
            snprintf(message.data(), message.size(), "HVF: %s failed: 0x%x", what, static_cast<uint32_t>(result));
            throw std::runtime_error(message.data());
        }

        void check_hv(const char* what, const hv_return_t result)
        {
            if (result != HV_SUCCESS)
            {
                throw_hv_error(what, result);
            }
        }

        hv_memory_flags_t to_hv_flags(const int prot)
        {
            hv_memory_flags_t flags = 0;
            flags |= (prot & PROT_READ) ? HV_MEMORY_READ : 0;
            flags |= (prot & PROT_WRITE) ? HV_MEMORY_WRITE : 0;
            flags |= (prot & PROT_EXEC) ? HV_MEMORY_EXEC : 0;
            return flags;
        }

        bool host_supports_hypervisor()
        {
            int supported = 0;
            size_t size = sizeof(supported);
            return sysctlbyname("kern.hv_support", &supported, &size, nullptr, 0) == 0 && supported != 0;
        }

        uint32_t create_hypervisor_vm()
        {
            if (!host_supports_hypervisor())
            {
                throw std::runtime_error("FEX hypervisor mode requires Hypervisor.framework support, which this machine lacks");
            }

            uint32_t ipa_bits = 0;
            hv_return_t result = HV_UNSUPPORTED;
            if (__builtin_available(macOS 15.0, *))
            {
                check_hv("hv_vm_config_get_max_ipa_size", hv_vm_config_get_max_ipa_size(&ipa_bits));
                ipa_bits = std::min(ipa_bits, max_ipa_bits);

                auto* config = hv_vm_config_create();
                result = hv_vm_config_set_ipa_size(config, ipa_bits);
                if (result == HV_SUCCESS)
                {
                    result = hv_vm_create(config);
                }
                os_release(config);
            }
            else
            {
                throw std::runtime_error("FEX hypervisor mode requires macOS 15 or newer");
            }

            switch (result)
            {
            case HV_SUCCESS:
                return ipa_bits;
            case HV_DENIED:
                throw std::runtime_error("FEX hypervisor mode requires the com.apple.security.hypervisor entitlement");
            case HV_UNSUPPORTED:
                throw std::runtime_error("FEX hypervisor mode requires Hypervisor.framework support, which this machine lacks");
            case HV_BUSY:
                throw std::runtime_error("FEX hypervisor mode needs the process's only Hypervisor.framework VM, which is in use");
            default:
                throw_hv_error("hv_vm_create", result);
            }
        }

        void require_hardware_tso()
        {
            hv_vcpu_t probe{};
            hv_vcpu_exit_t* exit{};
            check_hv("hv_vcpu_create", hv_vcpu_create(&probe, &exit, nullptr));
            const bool supported = enable_hardware_tso(probe);
            hv_vcpu_destroy(probe);

            if (!supported)
            {
                throw std::runtime_error("FEX hypervisor mode requires hardware TSO (ACTLR_EL1.EnTSO), which this machine lacks");
            }
        }
    }

    bool hypervisor_stage2::is_host_executable(const uint64_t host_address, const size_t size) const
    {
        const mach_vm_address_t end = host_address + size;
        mach_vm_address_t probe = host_address;
        while (probe < end)
        {
            mach_vm_address_t region = probe;
            mach_vm_size_t region_size = 0;
            vm_region_basic_info_data_64_t info{};
            mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object_name = MACH_PORT_NULL;
            if (mach_vm_region(mach_task_self(), &region, &region_size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info),
                               &info_count, &object_name) != KERN_SUCCESS ||
                region >= end)
            {
                return false;
            }

            if ((info.protection & VM_PROT_EXECUTE) != 0)
            {
                return true;
            }

            probe = region + region_size;
        }

        return false;
    }

    void hypervisor_stage2::map(const uint64_t host_address, const uint64_t ipa, const size_t size, const int prot)
    {
        const hv_return_t result = hv_vm_map(reinterpret_cast<void*>(host_address), ipa, size, to_hv_flags(prot));
        if (result != HV_SUCCESS)
        {
            std::array<char, 160> what{};
            snprintf(what.data(), what.size(), "hv_vm_map(0x%llx, ipa 0x%llx, 0x%zx)", static_cast<unsigned long long>(host_address),
                     static_cast<unsigned long long>(ipa), size);
            throw_hv_error(what.data(), result);
        }
    }

    void hypervisor_stage2::unmap(const uint64_t ipa, const size_t size)
    {
        check_hv("hv_vm_unmap", hv_vm_unmap(ipa, size));
    }

    void hypervisor_stage2::protect(const uint64_t ipa, const size_t size, const int prot)
    {
        check_hv("hv_vm_protect", hv_vm_protect(ipa, size, to_hv_flags(prot)));
    }

    vm& vm::get()
    {
        static vm instance;
        return instance;
    }

    vm::vm()
    {
        const uint32_t ipa_bits = create_hypervisor_vm();

        try
        {
            require_hardware_tso();
            this->space_.emplace(this->stage2_, 1ULL << ipa_bits);
            this->space_->map(this->runtime_.address(), guest_runtime::size, PROT_READ | PROT_EXEC);
        }
        catch (...)
        {
            this->space_.reset();
            hv_vm_destroy();
            throw;
        }
    }

    template <typename F>
    void vm::edit(F&& f)
    {
        const std::scoped_lock lock(this->mutex_);
        const uint64_t generation = this->space_->stage1_generation();
        f(*this->space_);
        if (this->space_->stage1_generation() != generation)
        {
            for (auto* cpu : this->vcpus_)
            {
                cpu->invalidate_stage1();
            }
        }
    }

    void vm::map(const uint64_t address, const size_t size, const int prot)
    {
        this->edit([&](address_space& space) { space.map(address, size, prot); });
    }

    void vm::unmap(const uint64_t address, const size_t size)
    {
        this->edit([&](address_space& space) { space.unmap(address, size); });
    }

    void vm::protect(const uint64_t address, const size_t size, const int prot)
    {
        this->edit([&](address_space& space) { space.protect(address, size, prot); });
    }

    void vm::refresh_backing(const uint64_t address, const size_t size)
    {
        this->edit([&](address_space& space) { space.refresh_backing(address, size); });
    }

    uint64_t vm::stage1_root() const
    {
        return this->space_->stage1_root();
    }

    uint64_t vm::stage1_generation() const
    {
        return this->space_->stage1_generation();
    }

    uint64_t vm::add_host_call(const uint64_t function, const bool uses_vector_registers)
    {
        return this->runtime_.host_call_stub(this->host_calls_.add(function, uses_vector_registers));
    }

    std::optional<host_call> vm::get_host_call(const size_t index) const
    {
        return this->host_calls_.get(index);
    }

    void vm::attach(vcpu& cpu)
    {
        const std::scoped_lock lock(this->mutex_);
        this->vcpus_.push_back(&cpu);
    }

    void vm::detach(vcpu& cpu)
    {
        const std::scoped_lock lock(this->mutex_);
        std::erase(this->vcpus_, &cpu);
    }
}
