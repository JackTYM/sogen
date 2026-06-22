#include "kvm_x86_64_emulator.hpp"

#include <fcntl.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <address_utils.hpp>
#include <utils/object.hpp>
#include <utils/cpu_features.hpp>

namespace sogen::kvm
{
    namespace
    {
        constexpr uint64_t page_size = 0x1000;
        constexpr uint64_t page_table_entry_present = 1ull << 0;
        constexpr uint64_t page_table_entry_writable = 1ull << 1;
        constexpr uint64_t page_table_entry_user = 1ull << 2;
        constexpr uint64_t page_table_entry_nx = 1ull << 63;
        constexpr uint64_t page_table_entry_address_mask = 0x000FFFFFFFFFF000ull;
        constexpr uint64_t guest_physical_memory_base = 0x0000000100000000ull;
        constexpr uint64_t internal_page_table_base = 0x0000007000000000ull;
        constexpr uint64_t unmapped_guest_page = (std::numeric_limits<uint64_t>::max)();

        constexpr uint32_t MSR_STAR = 0xC0000081;
        constexpr uint32_t MSR_LSTAR = 0xC0000082;
        constexpr uint32_t MSR_SFMASK = 0xC0000084;
        constexpr uint32_t MSR_EFER = 0xC0000080;
        constexpr uint32_t MSR_FS_BASE = 0xC0000100;
        constexpr uint32_t MSR_GS_BASE = 0xC0000101;

        uint64_t align_down_to_page(const uint64_t value)
        {
            return value & ~(page_size - 1);
        }

        bool is_page_aligned(const uint64_t value)
        {
            return (value % page_size) == 0;
        }

        [[noreturn]] void throw_kvm_error(const char* action)
        {
            std::ostringstream stream;
            stream << action << " failed: " << std::strerror(errno) << " (errno=" << errno << ")";
            throw std::runtime_error(stream.str());
        }

#define KVM_CHECK(expr)             \
    do                              \
    {                               \
        if ((expr) < 0)             \
        {                           \
            throw_kvm_error(#expr); \
        }                           \
    } while (false)

        class fd_handle
        {
          public:
            fd_handle() = default;
            explicit fd_handle(const int fd)
                : fd_(fd)
            {
            }

            fd_handle(const fd_handle&) = delete;
            fd_handle& operator=(const fd_handle&) = delete;

            fd_handle(fd_handle&& other) noexcept
                : fd_(std::exchange(other.fd_, -1))
            {
            }
            fd_handle& operator=(fd_handle&& other) noexcept
            {
                if (this != &other)
                {
                    reset();
                    fd_ = std::exchange(other.fd_, -1);
                }
                return *this;
            }

            ~fd_handle()
            {
                reset();
            }

            int get() const
            {
                return fd_;
            }
            explicit operator bool() const
            {
                return fd_ >= 0;
            }

            void reset()
            {
                if (fd_ >= 0)
                {
                    ::close(fd_);
                    fd_ = -1;
                }
            }

          private:
            int fd_ = -1;
        };

        struct mmap_deleter
        {
            size_t size;
            void operator()(uint8_t* ptr) const
            {
                if (ptr)
                {
                    ::munmap(ptr, size);
                }
            }
        };

        struct mapped_page
        {
            void* host_page = nullptr;
            uint64_t guest_physical_address = unmapped_guest_page;
            uint64_t guest_page_base = unmapped_guest_page;
            uint32_t map_flags = 0;
            size_t page_execution_hook_count = 0;
            memory_permission permissions = memory_permission::none;
            bool is_mmio = false;
            std::shared_ptr<uint8_t> owned_page{};
        };

        std::shared_ptr<uint8_t> allocate_backing_memory(const size_t size)
        {
            auto* raw_memory = static_cast<uint8_t*>(::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
            if (raw_memory == MAP_FAILED)
            {
                throw std::runtime_error("mmap failed while backing guest memory");
            }
            std::memset(raw_memory, 0, size);
            return std::shared_ptr<uint8_t>(raw_memory, mmap_deleter{size});
        }

        enum class kvm_reg_kind
        {
            gp,
            sregs_seg,
            sregs_seg_base,
            sregs_cr,
            sregs_efer,
            sregs_dtable,
            fpu_ctrl,
            fpu_st,
            fpu_xmm,
            fpu_mxcsr,
            debugreg,
            zero,
        };

        struct kvm_reg_mapping
        {
            kvm_reg_kind kind = kvm_reg_kind::zero;
            uint32_t index = 0;
            size_t logical_size = sizeof(uint64_t);
        };

        struct gp_register_access
        {
            size_t offset = 0;
            size_t width = sizeof(uint64_t);
            bool zero_extend_32 = false;
        };

        std::optional<gp_register_access> classify_gp_register_access(const x86_register reg)
        {
            switch (reg)
            {
            case x86_register::al:
            case x86_register::bl:
            case x86_register::cl:
            case x86_register::dl:
            case x86_register::sil:
            case x86_register::dil:
            case x86_register::bpl:
            case x86_register::spl:
            case x86_register::r8b:
            case x86_register::r9b:
            case x86_register::r10b:
            case x86_register::r11b:
            case x86_register::r12b:
            case x86_register::r13b:
            case x86_register::r14b:
            case x86_register::r15b:
                return gp_register_access{.offset = 0, .width = sizeof(uint8_t)};
            case x86_register::ah:
            case x86_register::bh:
            case x86_register::ch:
            case x86_register::dh:
                return gp_register_access{.offset = 1, .width = sizeof(uint8_t)};
            case x86_register::ax:
            case x86_register::bx:
            case x86_register::cx:
            case x86_register::dx:
            case x86_register::si:
            case x86_register::di:
            case x86_register::bp:
            case x86_register::sp:
            case x86_register::r8w:
            case x86_register::r9w:
            case x86_register::r10w:
            case x86_register::r11w:
            case x86_register::r12w:
            case x86_register::r13w:
            case x86_register::r14w:
            case x86_register::r15w:
            case x86_register::ip:
            case x86_register::flags:
                return gp_register_access{.offset = 0, .width = sizeof(uint16_t)};
            case x86_register::eax:
            case x86_register::ebx:
            case x86_register::ecx:
            case x86_register::edx:
            case x86_register::esi:
            case x86_register::edi:
            case x86_register::ebp:
            case x86_register::esp:
            case x86_register::r8d:
            case x86_register::r9d:
            case x86_register::r10d:
            case x86_register::r11d:
            case x86_register::r12d:
            case x86_register::r13d:
            case x86_register::r14d:
            case x86_register::r15d:
            case x86_register::eip:
            case x86_register::eflags:
                return gp_register_access{.offset = 0, .width = sizeof(uint32_t), .zero_extend_32 = true};
            case x86_register::rax:
            case x86_register::rbx:
            case x86_register::rcx:
            case x86_register::rdx:
            case x86_register::rsi:
            case x86_register::rdi:
            case x86_register::rbp:
            case x86_register::rsp:
            case x86_register::r8:
            case x86_register::r9:
            case x86_register::r10:
            case x86_register::r11:
            case x86_register::r12:
            case x86_register::r13:
            case x86_register::r14:
            case x86_register::r15:
            case x86_register::rip:
            case x86_register::rflags:
                return gp_register_access{.offset = 0, .width = sizeof(uint64_t)};
            default:
                return std::nullopt;
            }
        }

        // GP register index in kvm_regs field order:
        // 0=rax 1=rbx 2=rcx 3=rdx 4=rsi 5=rdi 6=rsp 7=rbp
        // 8=r8 9=r9 10=r10 11=r11 12=r12 13=r13 14=r14 15=r15
        // 16=rip 17=rflags
        static uint64_t* gp_field(struct kvm_regs& regs, const uint32_t index)
        {
            // kvm structs use __u64 (unsigned long long); uint64_t is unsigned long on this ABI.
            // The sizes are identical — reinterpret_cast is safe.
            uint64_t* fields[] = {
                reinterpret_cast<uint64_t*>(&regs.rax), reinterpret_cast<uint64_t*>(&regs.rbx), reinterpret_cast<uint64_t*>(&regs.rcx),
                reinterpret_cast<uint64_t*>(&regs.rdx), reinterpret_cast<uint64_t*>(&regs.rsi), reinterpret_cast<uint64_t*>(&regs.rdi),
                reinterpret_cast<uint64_t*>(&regs.rsp), reinterpret_cast<uint64_t*>(&regs.rbp), reinterpret_cast<uint64_t*>(&regs.r8),
                reinterpret_cast<uint64_t*>(&regs.r9),  reinterpret_cast<uint64_t*>(&regs.r10), reinterpret_cast<uint64_t*>(&regs.r11),
                reinterpret_cast<uint64_t*>(&regs.r12), reinterpret_cast<uint64_t*>(&regs.r13), reinterpret_cast<uint64_t*>(&regs.r14),
                reinterpret_cast<uint64_t*>(&regs.r15), reinterpret_cast<uint64_t*>(&regs.rip), reinterpret_cast<uint64_t*>(&regs.rflags),
            };
            return fields[index];
        }

        // Segment index in kvm_sregs:
        // 0=cs 1=ds 2=es 3=fs 4=gs 5=ss
        static struct kvm_segment* seg_field(struct kvm_sregs& sregs, const uint32_t index)
        {
            struct kvm_segment* segs[] = {
                &sregs.cs, &sregs.ds, &sregs.es, &sregs.fs, &sregs.gs, &sregs.ss,
            };
            return segs[index];
        }

        static const struct kvm_segment* seg_field(const struct kvm_sregs& sregs, const uint32_t index)
        {
            const struct kvm_segment* segs[] = {
                &sregs.cs, &sregs.ds, &sregs.es, &sregs.fs, &sregs.gs, &sregs.ss,
            };
            return segs[index];
        }

        kvm_reg_mapping map_kvm_register(const x86_register reg)
        {
            switch (reg)
            {
            case x86_register::al:
            case x86_register::ah:
            case x86_register::ax:
            case x86_register::eax:
            case x86_register::rax:
                return {kvm_reg_kind::gp, 0, sizeof(uint64_t)};
            case x86_register::bl:
            case x86_register::bh:
            case x86_register::bx:
            case x86_register::ebx:
            case x86_register::rbx:
                return {kvm_reg_kind::gp, 1, sizeof(uint64_t)};
            case x86_register::cl:
            case x86_register::ch:
            case x86_register::cx:
            case x86_register::ecx:
            case x86_register::rcx:
                return {kvm_reg_kind::gp, 2, sizeof(uint64_t)};
            case x86_register::dl:
            case x86_register::dh:
            case x86_register::dx:
            case x86_register::edx:
            case x86_register::rdx:
                return {kvm_reg_kind::gp, 3, sizeof(uint64_t)};
            case x86_register::si:
            case x86_register::sil:
            case x86_register::esi:
            case x86_register::rsi:
                return {kvm_reg_kind::gp, 4, sizeof(uint64_t)};
            case x86_register::di:
            case x86_register::dil:
            case x86_register::edi:
            case x86_register::rdi:
                return {kvm_reg_kind::gp, 5, sizeof(uint64_t)};
            case x86_register::sp:
            case x86_register::spl:
            case x86_register::esp:
            case x86_register::rsp:
                return {kvm_reg_kind::gp, 6, sizeof(uint64_t)};
            case x86_register::bp:
            case x86_register::bpl:
            case x86_register::ebp:
            case x86_register::rbp:
                return {kvm_reg_kind::gp, 7, sizeof(uint64_t)};
            case x86_register::r8:
            case x86_register::r8d:
            case x86_register::r8w:
            case x86_register::r8b:
                return {kvm_reg_kind::gp, 8, sizeof(uint64_t)};
            case x86_register::r9:
            case x86_register::r9d:
            case x86_register::r9w:
            case x86_register::r9b:
                return {kvm_reg_kind::gp, 9, sizeof(uint64_t)};
            case x86_register::r10:
            case x86_register::r10d:
            case x86_register::r10w:
            case x86_register::r10b:
                return {kvm_reg_kind::gp, 10, sizeof(uint64_t)};
            case x86_register::r11:
            case x86_register::r11d:
            case x86_register::r11w:
            case x86_register::r11b:
                return {kvm_reg_kind::gp, 11, sizeof(uint64_t)};
            case x86_register::r12:
            case x86_register::r12d:
            case x86_register::r12w:
            case x86_register::r12b:
                return {kvm_reg_kind::gp, 12, sizeof(uint64_t)};
            case x86_register::r13:
            case x86_register::r13d:
            case x86_register::r13w:
            case x86_register::r13b:
                return {kvm_reg_kind::gp, 13, sizeof(uint64_t)};
            case x86_register::r14:
            case x86_register::r14d:
            case x86_register::r14w:
            case x86_register::r14b:
                return {kvm_reg_kind::gp, 14, sizeof(uint64_t)};
            case x86_register::r15:
            case x86_register::r15d:
            case x86_register::r15w:
            case x86_register::r15b:
                return {kvm_reg_kind::gp, 15, sizeof(uint64_t)};
            case x86_register::ip:
            case x86_register::eip:
            case x86_register::rip:
                return {kvm_reg_kind::gp, 16, sizeof(uint64_t)};
            case x86_register::flags:
                return {kvm_reg_kind::gp, 17, sizeof(uint16_t)};
            case x86_register::eflags:
                return {kvm_reg_kind::gp, 17, sizeof(uint32_t)};
            case x86_register::rflags:
                return {kvm_reg_kind::gp, 17, sizeof(uint64_t)};
            case x86_register::cs:
                return {kvm_reg_kind::sregs_seg, 0, sizeof(uint16_t)};
            case x86_register::ds:
                return {kvm_reg_kind::sregs_seg, 1, sizeof(uint16_t)};
            case x86_register::es:
                return {kvm_reg_kind::sregs_seg, 2, sizeof(uint16_t)};
            case x86_register::fs:
                return {kvm_reg_kind::sregs_seg, 3, sizeof(uint16_t)};
            case x86_register::fs_base:
                return {kvm_reg_kind::sregs_seg_base, 3, sizeof(uint64_t)};
            case x86_register::gs:
                return {kvm_reg_kind::sregs_seg, 4, sizeof(uint16_t)};
            case x86_register::gs_base:
                return {kvm_reg_kind::sregs_seg_base, 4, sizeof(uint64_t)};
            case x86_register::ss:
                return {kvm_reg_kind::sregs_seg, 5, sizeof(uint16_t)};
            case x86_register::cr0:
                return {kvm_reg_kind::sregs_cr, 0, sizeof(uint64_t)};
            case x86_register::cr2:
                return {kvm_reg_kind::sregs_cr, 2, sizeof(uint64_t)};
            case x86_register::cr3:
                return {kvm_reg_kind::sregs_cr, 3, sizeof(uint64_t)};
            case x86_register::cr4:
                return {kvm_reg_kind::sregs_cr, 4, sizeof(uint64_t)};
            case x86_register::cr8:
                return {kvm_reg_kind::sregs_cr, 8, sizeof(uint64_t)};
            case x86_register::gdtr:
                return {kvm_reg_kind::sregs_dtable, 0, 10};
            case x86_register::idtr:
                return {kvm_reg_kind::sregs_dtable, 1, 10};
            case x86_register::msr:
                return {kvm_reg_kind::sregs_efer, 0, sizeof(uint64_t)};
            case x86_register::dr0:
                return {kvm_reg_kind::debugreg, 0, sizeof(uint64_t)};
            case x86_register::dr1:
                return {kvm_reg_kind::debugreg, 1, sizeof(uint64_t)};
            case x86_register::dr2:
                return {kvm_reg_kind::debugreg, 2, sizeof(uint64_t)};
            case x86_register::dr3:
                return {kvm_reg_kind::debugreg, 3, sizeof(uint64_t)};
            case x86_register::dr6:
                return {kvm_reg_kind::debugreg, 6, sizeof(uint64_t)};
            case x86_register::dr7:
                return {kvm_reg_kind::debugreg, 7, sizeof(uint64_t)};
            case x86_register::fpcw:
                return {kvm_reg_kind::fpu_ctrl, 0, sizeof(uint16_t)};
            case x86_register::fpsw:
                return {kvm_reg_kind::fpu_ctrl, 1, sizeof(uint16_t)};
            case x86_register::fptag:
                return {kvm_reg_kind::fpu_ctrl, 2, sizeof(uint16_t)};
            case x86_register::mxcsr:
                return {kvm_reg_kind::fpu_mxcsr, 0, sizeof(uint32_t)};
            default:
                break;
            }

            if (reg >= x86_register::xmm0 && reg <= x86_register::xmm15)
            {
                const auto index = static_cast<uint32_t>(static_cast<int>(reg) - static_cast<int>(x86_register::xmm0));
                return {kvm_reg_kind::fpu_xmm, index, 16};
            }

            if (reg >= x86_register::st0 && reg <= x86_register::st7)
            {
                const auto index = static_cast<uint32_t>(static_cast<int>(reg) - static_cast<int>(x86_register::st0));
                return {kvm_reg_kind::fpu_st, index, 10};
            }

            // YMM, ZMM, and other unsupported registers return zero
            return {kvm_reg_kind::zero, 0, 32};
        }

        kvm_segment make_kvm_segment(const uint16_t selector, const bool is_code)
        {
            kvm_segment seg{};
            seg.base = 0;
            seg.limit = 0xFFFFF;
            seg.selector = selector;
            seg.type = is_code ? 0xBu : 0x3u;
            seg.present = 1;
            seg.dpl = (selector == 0x33 || selector == 0x2B || selector == 0x53) ? 3 : 0;
            seg.db = is_code ? 0 : 1;
            seg.s = 1;
            seg.l = is_code ? 1 : 0;
            seg.g = 1;
            seg.unusable = 0;
            return seg;
        }

        struct kvm_register_snapshot
        {
            struct kvm_regs regs;
            struct kvm_sregs sregs;
            struct kvm_fpu fpu;
            struct kvm_debugregs debugregs;
            uint64_t msr_star;
            uint64_t msr_sfmask;
        };

        class kvm_x86_64_emulator : public x86_64_emulator
        {
          public:
            kvm_x86_64_emulator()
            {
                this->ensure_platform_support();
                this->initialize_long_mode_page_tables();
                this->initialize_cpuid();
                this->initialize_virtual_processor_state();
                this->initialize_exception_handling();
                this->initialize_syscall_intercept_page();
                this->update_guest_debug();
            }

            ~kvm_x86_64_emulator() override
            {
                if (kvm_run_)
                {
                    ::munmap(kvm_run_, kvm_run_size_);
                    kvm_run_ = nullptr;
                }
                utils::reset_object_with_delayed_destruction(this->memory_write_hooks_);
                utils::reset_object_with_delayed_destruction(this->memory_read_hooks_);
                utils::reset_object_with_delayed_destruction(this->memory_execution_hooks_);
                utils::reset_object_with_delayed_destruction(this->memory_violation_hooks_);
                utils::reset_object_with_delayed_destruction(this->interrupt_hooks_);
                utils::reset_object_with_delayed_destruction(this->basic_block_hooks_);
                utils::reset_object_with_delayed_destruction(this->instruction_hooks_);
            }

            void set_memory_execution_hook_mode(const memory_execution_hook_mode mode) override
            {
                this->memory_execution_hook_mode_ = mode;
            }

            void start(const size_t count) override
            {
                if (count != 0 && count != 1)
                {
                    throw std::runtime_error("KVM backend does not support exact instruction counts");
                }

                this->stop_requested_ = false;
                bool armed_single_step = false;

                if (this->deferred_patched_breakpoint_)
                {
                    if (this->read_instruction_pointer() == *this->deferred_patched_breakpoint_)
                    {
                        if (!this->arm_patched_breakpoint_single_step(*this->deferred_patched_breakpoint_, count == 1))
                        {
                            throw std::runtime_error("Nested KVM execution single-step state is not supported");
                        }
                        armed_single_step = true;
                    }
                    else
                    {
                        this->set_patched_execution_breakpoint_state(*this->deferred_patched_breakpoint_, true);
                        this->deferred_patched_breakpoint_.reset();
                    }
                }

                if (!armed_single_step && this->deferred_execution_page_)
                {
                    if (!this->arm_execution_single_step(*this->deferred_execution_page_, count == 1))
                    {
                        throw std::runtime_error("Nested KVM execution single-step state is not supported");
                    }
                    this->deferred_execution_page_.reset();
                    armed_single_step = true;
                }

                if (!armed_single_step && count == 1 && !this->arm_execution_single_step(std::nullopt, true))
                {
                    throw std::runtime_error("Nested KVM execution single-step state is not supported");
                }

                while (!this->stop_requested_)
                {
                    kvm_run_->immediate_exit = 0;
                    this->run_active_ = true;
                    const int ret = ::ioctl(vcpu_fd_.get(), KVM_RUN, 0);
                    this->run_active_ = false;

                    if (ret < 0)
                    {
                        if (errno == EINTR)
                        {
                            if (this->stop_requested_)
                            {
                                return;
                            }
                            continue;
                        }
                        throw_kvm_error("KVM_RUN");
                    }

                    switch (kvm_run_->exit_reason)
                    {
                    case KVM_EXIT_INTR:
                        if (this->stop_requested_)
                        {
                            return;
                        }
                        continue;

                    case KVM_EXIT_HLT: {
                        const auto rip = this->read_instruction_pointer();
                        if (this->syscall_hook_ && rip == (this->syscall_hook_page_ + 1))
                        {
                            if (this->handle_syscall_halt())
                            {
                                if (this->stop_requested_)
                                {
                                    return;
                                }
                                continue;
                            }
                        }
                        else if (this->exception_stubs_page_ != 0 && rip >= this->exception_stubs_page_ + 3 &&
                                 rip < this->exception_stubs_page_ + 256 * 4 + 3 && (rip - this->exception_stubs_page_ - 3) % 4 == 0)
                        {
                            const auto vector = static_cast<uint8_t>((rip - this->exception_stubs_page_ - 3) / 4);
                            if (this->handle_exception_halt(vector))
                            {
                                if (this->stop_requested_)
                                {
                                    return;
                                }
                                continue;
                            }
                        }
                        return;
                    }

                    case KVM_EXIT_MMIO:
                        if (this->handle_mmio_exit())
                        {
                            continue;
                        }
                        return;

                    case KVM_EXIT_DEBUG:
                        if (this->handle_debug_exit())
                        {
                            if (this->stop_requested_)
                            {
                                return;
                            }
                            continue;
                        }
                        return;

                    case KVM_EXIT_SHUTDOWN: {
                        const auto r = this->get_kvm_regs();
                        const auto s = this->get_kvm_sregs();
                        std::ostringstream ss;
                        ss << "KVM: guest triple-fault (KVM_EXIT_SHUTDOWN)" << std::hex << " post-reset RIP=0x" << r.rip << " RSP=0x"
                           << r.rsp << " CS.base=0x" << s.cs.base << " CR3=0x" << s.cr3;
                        throw std::runtime_error(ss.str());
                    }

                    case KVM_EXIT_FAIL_ENTRY: {
                        std::ostringstream s;
                        s << "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason=0x" << std::hex
                          << kvm_run_->fail_entry.hardware_entry_failure_reason;
                        throw std::runtime_error(s.str());
                    }

                    case KVM_EXIT_INTERNAL_ERROR: {
                        std::ostringstream s;
                        s << "KVM_EXIT_INTERNAL_ERROR: suberror=0x" << std::hex << kvm_run_->internal.suberror;
                        throw std::runtime_error(s.str());
                    }

                    default: {
                        std::ostringstream s;
                        s << "Unhandled KVM exit reason: " << kvm_run_->exit_reason;
                        throw std::runtime_error(s.str());
                    }
                    }
                }
            }

            void stop() override
            {
                this->stop_requested_ = true;
                if (this->run_active_)
                {
                    kvm_run_->immediate_exit = 1;
                }
            }

            void load_gdt(const pointer_type address, const uint32_t limit) override
            {
                auto sregs = this->get_kvm_sregs();
                sregs.gdt.base = address;
                sregs.gdt.limit = static_cast<uint16_t>(limit);
                this->set_kvm_sregs(sregs);
            }

            void set_segment_base(const x86_register base, const pointer_type value) override
            {
                const auto mapping = map_kvm_register(base);
                auto sregs = this->get_kvm_sregs();
                seg_field(sregs, mapping.index)->base = value;
                this->set_kvm_sregs(sregs);
            }

            pointer_type get_segment_base(const x86_register base) override
            {
                const auto mapping = map_kvm_register(base);
                const auto sregs = this->get_kvm_sregs();
                return seg_field(sregs, mapping.index)->base;
            }

            void map_mmio(const uint64_t address, const size_t size, mmio_read_callback read_cb, mmio_write_callback write_cb) override
            {
                if (!is_page_aligned(address) || !is_page_aligned(size))
                {
                    throw std::runtime_error("KVM MMIO mappings must be page aligned");
                }

                mmio_region region{
                    .address = address,
                    .size = size,
                    .read_cb = std::move(read_cb),
                    .write_cb = std::move(write_cb),
                };

                for (size_t offset = 0; offset < size; offset += page_size)
                {
                    const auto guest_address = address + offset;
                    auto& page = this->mapped_pages_[guest_address];
                    if (!page)
                    {
                        page = std::make_unique<mapped_page>();
                    }

                    if (page->host_page == nullptr)
                    {
                        auto backing = allocate_backing_memory(page_size);
                        page->owned_page = std::move(backing);
                        page->host_page = page->owned_page.get();
                        this->assign_guest_physical_page(guest_address, *page);
                        this->apply_patched_execution_breakpoints(guest_address);
                    }

                    // MMIO pages have no KVM slot (GPA deliberately unmapped → KVM_EXIT_MMIO);
                    // is_mmio keeps the PTE present so the CPU reaches the GPA and KVM exits.
                    page->permissions = memory_permission::none;
                    page->is_mmio = true;
                    this->ensure_virtual_mapping(guest_address);
                }

                this->mmio_regions_[address] = std::move(region);
            }

            void map_memory(const uint64_t address, const size_t size, const memory_permission permissions) override
            {
                if (!is_page_aligned(address) || !is_page_aligned(size))
                {
                    throw std::runtime_error("KVM memory mappings must be page aligned");
                }

                bool can_batch_map = true;
                for (size_t offset = 0; offset < size; offset += page_size)
                {
                    const auto guest_address = address + offset;
                    const auto entry = this->mapped_pages_.find(guest_address);
                    if (entry != this->mapped_pages_.end() && entry->second && entry->second->host_page != nullptr)
                    {
                        can_batch_map = false;
                        break;
                    }
                }

                if (can_batch_map)
                {
                    auto backing = allocate_backing_memory(size);
                    auto* backing_base = backing.get();

                    for (size_t offset = 0; offset < size; offset += page_size)
                    {
                        const auto guest_address = address + offset;
                        auto& page = this->mapped_pages_[guest_address];
                        if (!page)
                        {
                            page = std::make_unique<mapped_page>();
                        }
                        page->owned_page = backing;
                        page->host_page = backing_base + offset;
                        this->assign_guest_physical_page(guest_address, *page);
                        page->permissions = permissions;
                        this->apply_patched_execution_breakpoints(guest_address);
                        this->ensure_virtual_mapping(guest_address);
                    }

                    this->remap_pages(address, size);
                    return;
                }

                for (size_t offset = 0; offset < size; offset += page_size)
                {
                    const auto guest_address = address + offset;
                    auto& page = this->mapped_pages_[guest_address];
                    if (!page)
                    {
                        page = std::make_unique<mapped_page>();
                    }

                    if (page->host_page == nullptr)
                    {
                        auto backing = allocate_backing_memory(page_size);
                        page->owned_page = std::move(backing);
                        page->host_page = page->owned_page.get();
                        this->assign_guest_physical_page(guest_address, *page);
                        this->apply_patched_execution_breakpoints(guest_address);
                    }

                    page->permissions = permissions;
                    this->remap_page(*page);
                    this->ensure_virtual_mapping(guest_address);
                }
            }

            void map_host_memory(const uint64_t address, const size_t size, void* host_pointer,
                                 const memory_permission permissions) override
            {
                if (!is_page_aligned(address) || !is_page_aligned(size))
                {
                    throw std::runtime_error("KVM host memory mappings must be page aligned");
                }
                if ((reinterpret_cast<uintptr_t>(host_pointer) % page_size) != 0)
                {
                    throw std::runtime_error("KVM host memory mappings require a page-aligned host pointer");
                }

                auto* host_base = static_cast<uint8_t*>(host_pointer);
                for (size_t offset = 0; offset < size; offset += page_size)
                {
                    const auto guest_address = address + offset;
                    auto& page = this->mapped_pages_[guest_address];
                    if (!page)
                    {
                        page = std::make_unique<mapped_page>();
                    }
                    page->owned_page = nullptr;
                    page->host_page = host_base + offset;
                    this->assign_guest_physical_page(guest_address, *page);
                    page->permissions = permissions;
                    this->apply_patched_execution_breakpoints(guest_address);
                    this->ensure_virtual_mapping(guest_address);
                }

                this->remap_pages(address, size);
            }

            void unmap_memory(const uint64_t address, const size_t size) override
            {
                if (!is_page_aligned(address) || !is_page_aligned(size))
                {
                    throw std::runtime_error("KVM memory unmappings must be page aligned");
                }

                bool flushed_virtual_mappings = false;
                for (size_t offset = size; offset > 0; offset -= page_size)
                {
                    const auto guest_address = address + offset - page_size;
                    const auto entry = this->mapped_pages_.find(guest_address);
                    if (entry == this->mapped_pages_.end())
                    {
                        continue;
                    }

                    if (entry->second->map_flags != 0)
                    {
                        this->remove_kvm_slot(entry->second->guest_physical_address, page_size);
                        entry->second->map_flags = 0;
                    }

                    flushed_virtual_mappings = this->clear_virtual_mapping(guest_address) || flushed_virtual_mappings;
                    this->mark_patched_execution_breakpoints_unmapped(guest_address);
                    this->release_guest_physical_page(entry->second->guest_physical_address);
                    this->mapped_pages_.erase(entry);
                }

                if (flushed_virtual_mappings)
                {
                    this->flush_virtual_address_mappings();
                }

                const auto mmio = this->mmio_regions_.find(address);
                if (mmio != this->mmio_regions_.end() && mmio->second.size == size)
                {
                    this->mmio_regions_.erase(mmio);
                }
            }

            bool try_read_memory(const uint64_t address, void* data, const size_t size) const override
            {
                if (!this->access_memory(address, data, size, false))
                {
                    return false;
                }
                this->overlay_patched_breakpoints(address, data, size);
                return true;
            }

            void read_memory(const uint64_t address, void* data, const size_t size) const override
            {
                if (!this->try_read_memory(address, data, size))
                {
                    throw std::runtime_error("Failed to read KVM guest memory");
                }
            }

            bool try_write_memory(const uint64_t address, const void* data, const size_t size) override
            {
                if (!this->access_memory(address, const_cast<void*>(data), size, true))
                {
                    return false;
                }
                this->overlay_patched_breakpoints(address, data, size);
                return true;
            }

            void write_memory(const uint64_t address, const void* data, const size_t size) override
            {
                if (!this->try_write_memory(address, data, size))
                {
                    throw std::runtime_error("Failed to write KVM guest memory");
                }
            }

            void apply_memory_protection(const uint64_t address, const size_t size, const memory_permission permissions) override
            {
                if (!is_page_aligned(address) || !is_page_aligned(size))
                {
                    throw std::runtime_error("KVM protection changes must be page aligned");
                }

                for (size_t offset = 0; offset < size; offset += page_size)
                {
                    const auto guest_address = address + offset;
                    const auto entry = this->mapped_pages_.find(guest_address);
                    if (entry == this->mapped_pages_.end())
                    {
                        continue;
                    }
                    entry->second->permissions = permissions;
                }

                this->remap_pages(address, size);
            }

            size_t write_raw_register(const int reg, const void* value, const size_t size) override
            {
                const auto mapping = map_kvm_register(static_cast<x86_register>(reg));

                switch (mapping.kind)
                {
                case kvm_reg_kind::gp: {
                    auto regs = this->get_kvm_regs();
                    auto* field = gp_field(regs, mapping.index);
                    if (const auto access = classify_gp_register_access(static_cast<x86_register>(reg)))
                    {
                        uint64_t incoming = 0;
                        const auto copy_size = (std::min)(size, access->width);
                        std::memcpy(&incoming, value, copy_size);
                        if (access->zero_extend_32)
                        {
                            *field = static_cast<uint32_t>(incoming);
                        }
                        else if (access->width >= sizeof(uint64_t))
                        {
                            *field = incoming;
                        }
                        else
                        {
                            const auto mask = ((uint64_t{1} << (access->width * 8)) - 1) << (access->offset * 8);
                            *field = (*field & ~mask) | ((incoming << (access->offset * 8)) & mask);
                        }
                    }
                    else
                    {
                        std::memcpy(field, value, (std::min)(size, sizeof(uint64_t)));
                    }
                    this->set_kvm_regs(regs);
                    break;
                }
                case kvm_reg_kind::sregs_seg: {
                    auto sregs = this->get_kvm_sregs();
                    auto* seg = seg_field(sregs, mapping.index);
                    std::memcpy(&seg->selector, value, (std::min)(size, sizeof(seg->selector)));
                    this->set_kvm_sregs(sregs);
                    break;
                }
                case kvm_reg_kind::sregs_seg_base: {
                    auto sregs = this->get_kvm_sregs();
                    auto* seg = seg_field(sregs, mapping.index);
                    std::memcpy(&seg->base, value, (std::min)(size, sizeof(seg->base)));
                    this->set_kvm_sregs(sregs);
                    break;
                }
                case kvm_reg_kind::sregs_cr: {
                    auto sregs = this->get_kvm_sregs();
                    uint64_t* cr = cr_field(sregs, mapping.index);
                    if (cr)
                    {
                        std::memcpy(cr, value, (std::min)(size, sizeof(uint64_t)));
                        this->set_kvm_sregs(sregs);
                    }
                    break;
                }
                case kvm_reg_kind::sregs_efer: {
                    auto sregs = this->get_kvm_sregs();
                    std::memcpy(&sregs.efer, value, (std::min)(size, sizeof(sregs.efer)));
                    this->set_kvm_sregs(sregs);
                    break;
                }
                case kvm_reg_kind::sregs_dtable: {
                    auto sregs = this->get_kvm_sregs();
                    struct kvm_dtable* dt = mapping.index == 0 ? &sregs.gdt : &sregs.idt;
                    struct
                    {
                        uint16_t limit;
                        uint64_t base;
                    } __attribute__((packed)) legacy{};
                    const auto copy_size = (std::min)(size, sizeof(legacy));
                    std::memcpy(&legacy, value, copy_size);
                    dt->limit = legacy.limit;
                    dt->base = legacy.base;
                    this->set_kvm_sregs(sregs);
                    break;
                }
                case kvm_reg_kind::fpu_ctrl: {
                    auto fpu = this->get_kvm_fpu();
                    if (mapping.index == 0)
                    {
                        std::memcpy(&fpu.fcw, value, (std::min)(size, sizeof(fpu.fcw)));
                    }
                    else if (mapping.index == 1)
                    {
                        std::memcpy(&fpu.fsw, value, (std::min)(size, sizeof(fpu.fsw)));
                    }
                    else
                    {
                        uint16_t fp_tag{};
                        std::memcpy(&fp_tag, value, (std::min)(size, sizeof(fp_tag)));
                        fpu.ftwx = static_cast<uint8_t>(fp_tag);
                    }
                    this->set_kvm_fpu(fpu);
                    break;
                }
                case kvm_reg_kind::fpu_st: {
                    auto fpu = this->get_kvm_fpu();
                    std::memcpy(fpu.fpr[mapping.index], value, (std::min)(size, sizeof(fpu.fpr[0])));
                    this->set_kvm_fpu(fpu);
                    break;
                }
                case kvm_reg_kind::fpu_xmm: {
                    if (mapping.index < 16)
                    {
                        auto fpu = this->get_kvm_fpu();
                        std::memcpy(fpu.xmm[mapping.index], value, (std::min)(size, sizeof(fpu.xmm[0])));
                        this->set_kvm_fpu(fpu);
                    }
                    break;
                }
                case kvm_reg_kind::fpu_mxcsr: {
                    auto fpu = this->get_kvm_fpu();
                    std::memcpy(&fpu.mxcsr, value, (std::min)(size, sizeof(fpu.mxcsr)));
                    this->set_kvm_fpu(fpu);
                    break;
                }
                case kvm_reg_kind::debugreg: {
                    auto dbg = this->get_kvm_debugregs();
                    uint64_t* dr = dr_field(dbg, mapping.index);
                    if (dr)
                    {
                        std::memcpy(dr, value, (std::min)(size, sizeof(uint64_t)));
                        this->set_kvm_debugregs(dbg);
                    }
                    break;
                }
                case kvm_reg_kind::zero:
                    break;
                }

                return size;
            }

            size_t read_raw_register(const int reg, void* value, const size_t size) override
            {
                const auto mapping = map_kvm_register(static_cast<x86_register>(reg));

                switch (mapping.kind)
                {
                case kvm_reg_kind::gp: {
                    auto regs = this->get_kvm_regs();
                    const auto* field = gp_field(regs, mapping.index);
                    if (const auto access = classify_gp_register_access(static_cast<x86_register>(reg)))
                    {
                        const auto narrowed = *field >> (access->offset * 8);
                        std::memcpy(value, &narrowed, (std::min)(size, access->width));
                    }
                    else
                    {
                        std::memcpy(value, field, (std::min)(size, sizeof(uint64_t)));
                    }
                    break;
                }
                case kvm_reg_kind::sregs_seg: {
                    const auto sregs = this->get_kvm_sregs();
                    const auto* seg = seg_field(sregs, mapping.index);
                    std::memcpy(value, &seg->selector, (std::min)(size, sizeof(seg->selector)));
                    break;
                }
                case kvm_reg_kind::sregs_seg_base: {
                    const auto sregs = this->get_kvm_sregs();
                    const auto* seg = seg_field(sregs, mapping.index);
                    std::memcpy(value, &seg->base, (std::min)(size, sizeof(seg->base)));
                    break;
                }
                case kvm_reg_kind::sregs_cr: {
                    const auto sregs = this->get_kvm_sregs();
                    struct kvm_sregs mut = sregs;
                    const uint64_t* cr = cr_field(mut, mapping.index);
                    if (cr)
                    {
                        std::memcpy(value, cr, (std::min)(size, sizeof(uint64_t)));
                    }
                    break;
                }
                case kvm_reg_kind::sregs_efer: {
                    const auto sregs = this->get_kvm_sregs();
                    std::memcpy(value, &sregs.efer, (std::min)(size, sizeof(sregs.efer)));
                    break;
                }
                case kvm_reg_kind::sregs_dtable: {
                    const auto sregs = this->get_kvm_sregs();
                    const struct kvm_dtable* dt = mapping.index == 0 ? &sregs.gdt : &sregs.idt;
                    struct
                    {
                        uint16_t limit;
                        uint64_t base;
                    } __attribute__((packed)) legacy{};
                    legacy.limit = dt->limit;
                    legacy.base = dt->base;
                    std::memcpy(value, &legacy, (std::min)(size, sizeof(legacy)));
                    break;
                }
                case kvm_reg_kind::fpu_ctrl: {
                    const auto fpu = this->get_kvm_fpu();
                    if (mapping.index == 0)
                    {
                        std::memcpy(value, &fpu.fcw, (std::min)(size, sizeof(fpu.fcw)));
                    }
                    else if (mapping.index == 1)
                    {
                        std::memcpy(value, &fpu.fsw, (std::min)(size, sizeof(fpu.fsw)));
                    }
                    else
                    {
                        uint16_t fp_tag = fpu.ftwx;
                        std::memcpy(value, &fp_tag, (std::min)(size, sizeof(fp_tag)));
                    }
                    break;
                }
                case kvm_reg_kind::fpu_st: {
                    const auto fpu = this->get_kvm_fpu();
                    std::memcpy(value, fpu.fpr[mapping.index], (std::min)(size, sizeof(fpu.fpr[0])));
                    break;
                }
                case kvm_reg_kind::fpu_xmm: {
                    if (mapping.index < 16)
                    {
                        const auto fpu = this->get_kvm_fpu();
                        std::memcpy(value, fpu.xmm[mapping.index], (std::min)(size, sizeof(fpu.xmm[0])));
                    }
                    else
                    {
                        std::memset(value, 0, (std::min)(size, size_t{16}));
                    }
                    break;
                }
                case kvm_reg_kind::fpu_mxcsr: {
                    const auto fpu = this->get_kvm_fpu();
                    std::memcpy(value, &fpu.mxcsr, (std::min)(size, sizeof(fpu.mxcsr)));
                    break;
                }
                case kvm_reg_kind::debugreg: {
                    auto dbg = this->get_kvm_debugregs();
                    const uint64_t* dr = dr_field(dbg, mapping.index);
                    if (dr)
                    {
                        std::memcpy(value, dr, (std::min)(size, sizeof(uint64_t)));
                    }
                    break;
                }
                case kvm_reg_kind::zero:
                    std::memset(value, 0, (std::min)(size, mapping.logical_size));
                    break;
                }

                return mapping.logical_size;
            }

            bool read_descriptor_table(const int reg, descriptor_table_register& table) override
            {
                if (reg != static_cast<int>(x86_register::gdtr) && reg != static_cast<int>(x86_register::idtr))
                {
                    return false;
                }
                const auto sregs = this->get_kvm_sregs();
                const struct kvm_dtable* dt = (reg == static_cast<int>(x86_register::gdtr)) ? &sregs.gdt : &sregs.idt;
                table.base = dt->base;
                table.limit = dt->limit;
                return true;
            }

            emulator_hook* hook_instruction(const int instruction_type, instruction_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                auto& entry = this->instruction_hooks_[hook];
                entry.type = static_cast<x86_hookable_instructions>(instruction_type);
                entry.callback = std::move(callback);
                if (entry.type == x86_hookable_instructions::syscall)
                {
                    this->syscall_hook_ = &entry;
                }
                return hook;
            }

            emulator_hook* hook_basic_block(basic_block_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                this->basic_block_hooks_[hook] = std::move(callback);
                return hook;
            }

            emulator_hook* hook_interrupt(interrupt_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                this->interrupt_hooks_[hook] = std::move(callback);
                return hook;
            }

            emulator_hook* hook_memory_violation(memory_violation_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                this->memory_violation_hooks_[hook] = std::move(callback);
                return hook;
            }

            emulator_hook* hook_memory_execution(memory_execution_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                this->memory_execution_hooks_[hook] = execution_hook_entry{.address = std::nullopt, .callback = std::move(callback)};
                return hook;
            }

            emulator_hook* hook_memory_range_execution(const uint64_t address, const uint64_t size,
                                                       memory_execution_hook_callback callback) override
            {
                if (size == 1)
                {
                    return this->hook_memory_execution(address, std::move(callback));
                }

                // KVM cannot intercept execution by address range; store without address so this hook
                // does not affect page_execution_hook_count (which would wrongly set NX on code pages).
                auto* hook = this->make_hook();
                this->memory_execution_hooks_[hook] =
                    execution_hook_entry{.address = std::nullopt, .size = 0, .callback = std::move(callback)};
                return hook;
            }

            emulator_hook* hook_memory_execution(const uint64_t address, memory_execution_hook_callback callback) override
            {
                auto* hook = this->make_hook();

                if (this->memory_execution_hook_mode_ == memory_execution_hook_mode::int3)
                {
                    this->install_patched_execution_breakpoint(address);
                    this->memory_execution_hooks_[hook] =
                        execution_hook_entry{.address = address, .patched_breakpoint = true, .callback = std::move(callback)};
                }
                else
                {
                    this->memory_execution_hooks_[hook] = execution_hook_entry{.address = address, .callback = std::move(callback)};
                }

                return hook;
            }

            emulator_hook* hook_memory_read(const uint64_t address, const uint64_t size, memory_access_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                this->memory_read_hooks_[hook] =
                    memory_access_hook_entry{.address = address, .size = size, .callback = std::move(callback)};
                return hook;
            }

            emulator_hook* hook_memory_write(const uint64_t address, const uint64_t size, memory_access_hook_callback callback) override
            {
                auto* hook = this->make_hook();
                this->memory_write_hooks_[hook] =
                    memory_access_hook_entry{.address = address, .size = size, .callback = std::move(callback)};
                return hook;
            }

            void delete_hook(emulator_hook* hook) override
            {
                const auto instruction_it = this->instruction_hooks_.find(hook);
                if (instruction_it != this->instruction_hooks_.end() && &instruction_it->second == this->syscall_hook_)
                {
                    this->syscall_hook_ = nullptr;
                }

                this->instruction_hooks_.erase(hook);
                this->basic_block_hooks_.erase(hook);
                this->interrupt_hooks_.erase(hook);
                this->memory_violation_hooks_.erase(hook);

                if (const auto execution_it = this->memory_execution_hooks_.find(hook); execution_it != this->memory_execution_hooks_.end())
                {
                    this->deactivate_execution_hook(hook);
                    this->memory_execution_hooks_.erase(execution_it);
                }

                this->memory_read_hooks_.erase(hook);
                this->memory_write_hooks_.erase(hook);
            }

            void serialize_state(utils::buffer_serializer& buffer, const bool) const override
            {
                buffer.write_vector(this->save_registers());
            }

            void deserialize_state(utils::buffer_deserializer& buffer, const bool) override
            {
                this->restore_registers(buffer.read_vector<std::byte>());
            }

            std::vector<std::byte> save_registers() const override
            {
                kvm_register_snapshot snap{};
                snap.regs = this->get_kvm_regs();
                snap.sregs = this->get_kvm_sregs();
                snap.fpu = this->get_kvm_fpu();
                snap.debugregs = this->get_kvm_debugregs();
                snap.msr_star = this->get_msr(MSR_STAR);
                snap.msr_sfmask = this->get_msr(MSR_SFMASK);

                std::vector<std::byte> bytes(sizeof(kvm_register_snapshot));
                std::memcpy(bytes.data(), &snap, bytes.size());
                return bytes;
            }

            void restore_registers(const std::vector<std::byte>& register_data) override
            {
                if (register_data.size() != sizeof(kvm_register_snapshot))
                {
                    throw std::runtime_error("Unexpected KVM register snapshot size");
                }

                kvm_register_snapshot snap{};
                std::memcpy(&snap, register_data.data(), register_data.size());

                this->set_kvm_regs(snap.regs);
                this->set_kvm_sregs(snap.sregs);
                this->set_kvm_fpu(snap.fpu);
                this->set_kvm_debugregs(snap.debugregs);
                this->set_msr(MSR_STAR, snap.msr_star);
                this->set_msr(MSR_SFMASK, snap.msr_sfmask);
            }

            bool has_violation() const override
            {
                return false;
            }

            std::string get_name() const override
            {
                return "Linux KVM";
            }

            bool supports_instruction_counting() const override
            {
                return false;
            }

            bool supports_global_memory_execution_hooks() const override
            {
                return false;
            }

          private:
            struct instruction_hook_entry
            {
                x86_hookable_instructions type = x86_hookable_instructions::invalid;
                instruction_hook_callback callback{};
            };

            struct execution_hook_entry
            {
                std::optional<uint64_t> address{};
                uint64_t size{1};
                bool patched_breakpoint{false};
                memory_execution_hook_callback callback{};
            };

            struct memory_access_hook_entry
            {
                uint64_t address{};
                uint64_t size{};
                memory_access_hook_callback callback{};
            };

            struct mmio_region
            {
                uint64_t address{};
                size_t size{};
                mmio_read_callback read_cb{};
                mmio_write_callback write_cb{};
            };

            struct pending_execution_step
            {
                std::optional<uint64_t> page_base{};
                std::optional<uint64_t> patched_breakpoint{};
                bool stop_after_step{};
            };

            struct patched_execution_breakpoint
            {
                std::optional<std::byte> original_byte{};
                size_t hook_count{};
                bool applied{};
            };

            struct kvm_slot_entry
            {
                uint32_t id{};
                uint64_t gpa_base{};
                uint64_t size{};
                void* host_base{};
                uint32_t kvm_flags{};
            };

            fd_handle kvm_fd_{};
            fd_handle vm_fd_{};
            fd_handle vcpu_fd_{};
            struct kvm_run* kvm_run_ = nullptr;
            size_t kvm_run_size_ = 0;

            std::unordered_map<uint64_t, std::unique_ptr<mapped_page>> mapped_pages_{};
            std::vector<uint64_t> guest_pages_by_gpa_{};
            std::vector<size_t> free_guest_gpa_indices_{};
            std::unordered_map<uint64_t, uint64_t*> page_table_views_{};
            uint64_t pml4_gpa_ = 0;
            uint64_t next_internal_gpa_ = internal_page_table_base;

            std::map<uint64_t, kvm_slot_entry> active_kvm_slots_{};
            std::vector<uint32_t> free_kvm_slot_ids_{};
            uint32_t next_kvm_slot_id_ = 0;

            std::atomic_bool stop_requested_ = false;
            std::atomic_bool run_active_ = false;
            std::optional<pending_execution_step> pending_execution_step_{};
            uint64_t syscall_hook_page_ = 0;
            uint64_t exception_stubs_page_ = 0;
            uint64_t gdt_page_ = 0;
            uint64_t idt_page_ = 0;
            uint64_t tss_page_ = 0;
            uint64_t ring0_stack_page_ = 0;
            size_t next_hook_id_ = 1;

            std::unordered_map<emulator_hook*, instruction_hook_entry> instruction_hooks_{};
            std::unordered_map<emulator_hook*, basic_block_hook_callback> basic_block_hooks_{};
            std::unordered_map<emulator_hook*, interrupt_hook_callback> interrupt_hooks_{};
            std::unordered_map<emulator_hook*, memory_violation_hook_callback> memory_violation_hooks_{};
            std::unordered_map<emulator_hook*, execution_hook_entry> memory_execution_hooks_{};
            std::unordered_map<emulator_hook*, memory_access_hook_entry> memory_read_hooks_{};
            std::unordered_map<emulator_hook*, memory_access_hook_entry> memory_write_hooks_{};
            std::unordered_map<uint64_t, patched_execution_breakpoint> patched_execution_breakpoints_{};
            std::optional<uint64_t> deferred_patched_breakpoint_{};
            std::optional<uint64_t> deferred_execution_page_{};
            std::unordered_map<uint64_t, mmio_region> mmio_regions_{};
            instruction_hook_entry* syscall_hook_ = nullptr;
            memory_execution_hook_mode memory_execution_hook_mode_ = memory_execution_hook_mode::automatic;

            // --------------------------------------------------------------------------
            // Platform setup
            // --------------------------------------------------------------------------

            void ensure_platform_support()
            {
                const int kvm_fd = ::open("/dev/kvm", O_RDWR | O_CLOEXEC);
                if (kvm_fd < 0)
                {
                    throw std::runtime_error("KVM is not available; ensure /dev/kvm exists and is accessible");
                }
                kvm_fd_ = fd_handle(kvm_fd);

                const int api_version = ::ioctl(kvm_fd_.get(), KVM_GET_API_VERSION, 0);
                if (api_version != 12)
                {
                    throw std::runtime_error("Unexpected KVM API version (expected 12)");
                }

                const int vm_fd = ::ioctl(kvm_fd_.get(), KVM_CREATE_VM, 0);
                if (vm_fd < 0)
                {
                    throw_kvm_error("KVM_CREATE_VM");
                }
                vm_fd_ = fd_handle(vm_fd);

                const int vcpu_fd = ::ioctl(vm_fd_.get(), KVM_CREATE_VCPU, 0);
                if (vcpu_fd < 0)
                {
                    throw_kvm_error("KVM_CREATE_VCPU");
                }
                vcpu_fd_ = fd_handle(vcpu_fd);

                const int mmap_size = ::ioctl(kvm_fd_.get(), KVM_GET_VCPU_MMAP_SIZE, 0);
                if (mmap_size < 0)
                {
                    throw_kvm_error("KVM_GET_VCPU_MMAP_SIZE");
                }
                kvm_run_size_ = static_cast<size_t>(mmap_size);

                kvm_run_ =
                    static_cast<struct kvm_run*>(::mmap(nullptr, kvm_run_size_, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd_.get(), 0));
                if (kvm_run_ == MAP_FAILED)
                {
                    kvm_run_ = nullptr;
                    throw_kvm_error("mmap kvm_run");
                }
            }

            void initialize_long_mode_page_tables()
            {
                this->pml4_gpa_ = this->allocate_internal_page(false, false);
            }

            void initialize_virtual_processor_state()
            {
                struct kvm_sregs sregs{};
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_SREGS, &sregs));

                sregs.cs = make_kvm_segment(0x33, true);
                sregs.ss = make_kvm_segment(0x2B, false);
                sregs.ds = make_kvm_segment(0x2B, false);
                sregs.es = make_kvm_segment(0x2B, false);
                sregs.fs = make_kvm_segment(0x53, false);
                sregs.gs = make_kvm_segment(0x2B, false);

                // CR0: PE|MP|ET|NE|WP|AM|PG = 0x80000033
                sregs.cr0 = 0x80000033ull;
                // CR4: PAE|FSGSBASE|OSFXSR|OSXMMEXCPT + optional OSXSAVE
                // FSGSBASE (bit 16) is required by ntdll's RDGSBASE/WRGSBASE instructions
                const bool avx_ok = utils::cpu_features::avx_enabled();
                sregs.cr4 = avx_ok ? 0x50620ull : 0x10620ull;
                sregs.cr3 = this->pml4_gpa_;
                // EFER: LME|LMA|SCE|NXE = 0xD01 (NXE required to avoid reserved-bit PF on NX PTEs)
                sregs.efer = 0xD01ull;

                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_SREGS, &sregs));

                struct kvm_regs regs{};
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_REGS, &regs));
                regs.rflags = 0x2ull;
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_REGS, &regs));

                // STAR: kernel CS=0x08, user CS=0x23 (STAR = (user_cs-16)<<48 | kernel_cs<<32)
                this->set_msr(MSR_STAR, (0x23ull << 48) | (0x08ull << 32));
                this->set_msr(MSR_SFMASK, 0);

                struct kvm_fpu fpu{};
                fpu.fcw = 0x037F;
                fpu.fsw = 0;
                fpu.ftwx = 0xFF;
                fpu.mxcsr = 0x1F80;
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_FPU, &fpu));
            }

            void initialize_exception_handling()
            {
                this->gdt_page_ = this->allocate_internal_page(false);
                this->idt_page_ = this->allocate_internal_page(false);
                this->tss_page_ = this->allocate_internal_page(false);
                this->ring0_stack_page_ = this->allocate_internal_page(false);
                this->exception_stubs_page_ = this->allocate_internal_page(true);

                auto* gdt = static_cast<uint8_t*>(this->mapped_pages_.at(this->gdt_page_)->host_page);

                // Index 1 (0x08): 64-bit kernel code, ring-0
                // access=0x9A (P=1, DPL=0, S=1, type=1010), granularity=0xAF (G=1, L=1, D=0, limit[16:19]=0xF)
                const uint64_t kernel_cs_desc = 0x00AF9A0000000000ULL | 0xFFFFULL;
                std::memcpy(gdt + 0x08, &kernel_cs_desc, 8);

                // Index 2-3 (0x10-0x1F): 64-bit TSS descriptor (16 bytes)
                // type=0x89 (P=1, DPL=0, type=1001 = available 64-bit TSS), G=0 byte-granular
                {
                    const uint64_t base = this->tss_page_;
                    const uint32_t limit = 103;
                    uint64_t tss_lo = static_cast<uint64_t>(limit & 0xFFFF);
                    tss_lo |= (base & 0xFFFF) << 16;
                    tss_lo |= ((base >> 16) & 0xFF) << 32;
                    tss_lo |= static_cast<uint64_t>(0x89) << 40;
                    tss_lo |= static_cast<uint64_t>((limit >> 16) & 0xF) << 48;
                    tss_lo |= ((base >> 24) & 0xFF) << 56;
                    const uint64_t tss_hi = (base >> 32) & 0xFFFFFFFFULL;
                    std::memcpy(gdt + 0x10, &tss_lo, 8);
                    std::memcpy(gdt + 0x18, &tss_hi, 8);
                }

                // Index 5 (0x28): user data, ring-3 (selector 0x2B = index5|RPL3)
                // access=0xF3 (P=1, DPL=3, S=1, type=0011), granularity=0xCF (G=1, D=1)
                const uint64_t user_data_desc = 0x00CFF30000000000ULL | 0xFFFFULL;
                std::memcpy(gdt + 0x28, &user_data_desc, 8);

                // Index 6 (0x30): user code, ring-3 (selector 0x33 = index6|RPL3)
                // access=0xFA (P=1, DPL=3, S=1, type=1010), granularity=0xAF (G=1, L=1, D=0)
                const uint64_t user_cs_desc = 0x00AFFA0000000000ULL | 0xFFFFULL;
                std::memcpy(gdt + 0x30, &user_cs_desc, 8);

                // Index 10 (0x50): user FS, ring-3 (selector 0x53 = index10|RPL3)
                std::memcpy(gdt + 0x50, &user_data_desc, 8);

                // Fill TSS — set RSP0 to top of ring-0 stack page
                struct tss64_layout
                {
                    uint32_t reserved0;
                    uint64_t rsp0;
                    uint64_t rsp1;
                    uint64_t rsp2;
                    uint64_t reserved1;
                    uint64_t ist[7];
                    uint8_t reserved2[10];
                    uint16_t iopb_offset;
                } __attribute__((packed));
                static_assert(sizeof(tss64_layout) == 104);

                auto* tss = static_cast<tss64_layout*>(this->mapped_pages_.at(this->tss_page_)->host_page);
                std::memset(tss, 0, sizeof(*tss));
                tss->rsp0 = this->ring0_stack_page_ + page_size;
                tss->iopb_offset = sizeof(tss64_layout);

                // Fill IDT — 256 interrupt gates pointing to stubs, CS=0x08
                struct idt_entry_layout
                {
                    uint16_t offset_low;
                    uint16_t selector;
                    uint8_t ist;
                    uint8_t type_attr;
                    uint16_t offset_mid;
                    uint32_t offset_high;
                    uint32_t reserved;
                } __attribute__((packed));
                static_assert(sizeof(idt_entry_layout) == 16);

                auto* idt = static_cast<idt_entry_layout*>(this->mapped_pages_.at(this->idt_page_)->host_page);
                for (uint32_t v = 0; v < 256; ++v)
                {
                    const uint64_t handler = this->exception_stubs_page_ + v * 4;
                    idt[v].offset_low = static_cast<uint16_t>(handler & 0xFFFF);
                    idt[v].selector = 0x08;
                    idt[v].ist = 0;
                    idt[v].type_attr = 0xEE; // P=1, DPL=3, 64-bit interrupt gate (ring-3 accessible)
                    idt[v].offset_mid = static_cast<uint16_t>((handler >> 16) & 0xFFFF);
                    idt[v].offset_high = static_cast<uint32_t>((handler >> 32) & 0xFFFFFFFF);
                    idt[v].reserved = 0;
                }

                // Fill exception stubs: mov al, <vector>; hlt; nop (4 bytes per stub)
                auto* stubs = static_cast<uint8_t*>(this->mapped_pages_.at(this->exception_stubs_page_)->host_page);
                for (uint32_t v = 0; v < 256; ++v)
                {
                    stubs[v * 4 + 0] = 0xB0;
                    stubs[v * 4 + 1] = static_cast<uint8_t>(v);
                    stubs[v * 4 + 2] = 0xF4; // hlt
                    stubs[v * 4 + 3] = 0x90; // nop
                }

                // Load GDT, IDT, TR into vCPU
                auto sregs = this->get_kvm_sregs();
                sregs.gdt.base = this->gdt_page_;
                sregs.gdt.limit = 0x57; // covers through 0x53 (FS, index 10, offset 0x50 + 8 - 1)
                sregs.idt.base = this->idt_page_;
                sregs.idt.limit = 4095;

                kvm_segment tr{};
                tr.base = this->tss_page_;
                tr.limit = 103;
                tr.selector = 0x10;
                tr.type = 0x9; // available 64-bit TSS
                tr.present = 1;
                tr.dpl = 0;
                tr.db = 0;
                tr.s = 0; // system descriptor
                tr.l = 0;
                tr.g = 0;
                tr.unusable = 0;
                sregs.tr = tr;

                this->set_kvm_sregs(sregs);
            }

            void initialize_syscall_intercept_page()
            {
                this->syscall_hook_page_ = this->allocate_internal_page(true);
                auto* code = static_cast<uint8_t*>(this->mapped_pages_.at(this->syscall_hook_page_)->host_page);
                code[0] = 0xF4; // HLT

                this->set_msr(MSR_LSTAR, this->syscall_hook_page_);
            }

            void initialize_cpuid()
            {
                constexpr uint32_t max_entries = 100;
                const size_t buf_size = sizeof(struct kvm_cpuid2) + max_entries * sizeof(struct kvm_cpuid_entry2);
                std::vector<uint8_t> buf(buf_size, 0);
                auto* cpuid = reinterpret_cast<struct kvm_cpuid2*>(buf.data());
                cpuid->nent = max_entries;

                if (::ioctl(kvm_fd_.get(), KVM_GET_SUPPORTED_CPUID, cpuid) >= 0)
                {
                    (void)::ioctl(vcpu_fd_.get(), KVM_SET_CPUID2, cpuid);
                }
            }

            void update_guest_debug()
            {
                struct kvm_guest_debug dbg{};
                dbg.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;
                if (this->pending_execution_step_.has_value())
                {
                    dbg.control |= KVM_GUESTDBG_SINGLESTEP;
                }
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_GUEST_DEBUG, &dbg));
            }

            // --------------------------------------------------------------------------
            // Run loop handlers
            // --------------------------------------------------------------------------

            bool handle_syscall_halt()
            {
                if (!this->syscall_hook_)
                {
                    return false;
                }

                auto regs = this->get_kvm_regs();
                auto sregs = this->get_kvm_sregs();

                const auto post_syscall_rcx = regs.rcx;
                const auto post_syscall_r10 = regs.r10;
                const auto saved_rflags = regs.r11;
                const auto pre_syscall_rip = post_syscall_rcx - 2;

                regs.rip = pre_syscall_rip;
                regs.rcx = post_syscall_r10;
                regs.rflags = saved_rflags;
                sregs.cs = make_kvm_segment(0x33, true);
                sregs.ss = make_kvm_segment(0x2B, false);

                this->set_kvm_regs(regs);
                this->set_kvm_sregs(sregs);

                this->syscall_hook_->callback(0);

                auto post_regs = this->get_kvm_regs();
                if (post_regs.rip == pre_syscall_rip)
                {
                    post_regs.rip = post_syscall_rcx;
                }

                auto post_sregs = this->get_kvm_sregs();
                post_sregs.cs = make_kvm_segment(0x33, true);
                post_sregs.ss = make_kvm_segment(0x2B, false);

                this->set_kvm_regs(post_regs);
                this->set_kvm_sregs(post_sregs);

                return true;
            }

            bool handle_exception_halt(const uint8_t vector)
            {
                auto regs = this->get_kvm_regs();
                const auto sregs = this->get_kvm_sregs();

                // Exception frame layout on ring-0 stack (ring-3 → ring-0 transition):
                // With error code:    [rsp+0]=error_code, [rsp+8]=rip, [rsp+16]=cs, [rsp+24]=rflags, [rsp+32]=rsp, [rsp+40]=ss
                // Without error code: [rsp+0]=rip,        [rsp+8]=cs,  [rsp+16]=rflags, [rsp+24]=rsp, [rsp+32]=ss
                static constexpr auto has_error_code = [](const uint8_t v) -> bool {
                    return v == 8 || v == 10 || v == 11 || v == 12 || v == 13 || v == 14 || v == 17 || v == 21;
                };

                const uint64_t rsp = regs.rsp;
                const size_t ec_size = has_error_code(vector) ? 8 : 0;

                uint64_t error_code = 0;
                uint64_t frame_rip = 0;
                uint64_t frame_rflags = 0;
                uint64_t frame_rsp = 0;

                if (ec_size)
                {
                    this->read_memory(rsp, &error_code, 8);
                }
                this->read_memory(rsp + ec_size, &frame_rip, 8);
                this->read_memory(rsp + ec_size + 16, &frame_rflags, 8);
                this->read_memory(rsp + ec_size + 24, &frame_rsp, 8);

                // #BP (vector 3) is a trap: the CPU saves RIP *after* the int3 opcode.
                // Windows expects CONTEXT.Rip to point *at* the int3 (so that exception
                // filters doing "Rip += 1" land on the correct next instruction).
                if (vector == 3)
                {
                    frame_rip -= 1;
                }

                // Restore user-mode register state before invoking hooks
                regs.rip = frame_rip;
                regs.rsp = frame_rsp;
                regs.rflags = frame_rflags;
                this->set_kvm_regs(regs);

                auto user_sregs = sregs;
                user_sregs.cs = make_kvm_segment(0x33, true);
                user_sregs.ss = make_kvm_segment(0x2B, false);
                this->set_kvm_sregs(user_sregs);

                if (vector == 14) // #PF — page fault
                {
                    const uint64_t fault_address = sregs.cr2;
                    // error_code bit 4 (I/D) = instruction fetch; bit 1 (W/R) = write; else read
                    const auto operation = (error_code & 0x10)  ? memory_operation::exec
                                           : (error_code & 0x2) ? memory_operation::write
                                                                : memory_operation::read;
                    const auto vtype = (error_code & 0x1) ? memory_violation_type::protection : memory_violation_type::unmapped;

                    for (auto& [_, hook] : this->memory_violation_hooks_)
                    {
                        const auto result = hook(fault_address, 1, operation, vtype);
                        if (result != memory_violation_continuation::stop)
                        {
                            return true;
                        }
                    }
                    return false;
                }

                // For all other exceptions dispatch to interrupt hooks
                for (auto& [_, hook] : this->interrupt_hooks_)
                {
                    hook(static_cast<int>(vector));
                }
                return !this->interrupt_hooks_.empty();
            }

            bool try_fill_mmio_read_from_va(const uint64_t va, const uint32_t len)
            {
                const auto pml4_index = static_cast<size_t>((va >> 39) & 0x1FF);
                const auto pdpt_index = static_cast<size_t>((va >> 30) & 0x1FF);
                const auto pd_index = static_cast<size_t>((va >> 21) & 0x1FF);
                const auto pt_index = static_cast<size_t>((va >> 12) & 0x1FF);

                const auto* pml4 = this->get_page_table_entries(this->pml4_gpa_);
                if (!pml4 || !(pml4[pml4_index] & page_table_entry_present))
                {
                    return false;
                }
                const auto* pdpt = this->get_page_table_entries(pml4[pml4_index] & page_table_entry_address_mask);
                if (!pdpt || !(pdpt[pdpt_index] & page_table_entry_present))
                {
                    return false;
                }
                const auto* pd = this->get_page_table_entries(pdpt[pdpt_index] & page_table_entry_address_mask);
                if (!pd || !(pd[pd_index] & page_table_entry_present))
                {
                    return false;
                }
                const auto* pt = this->get_page_table_entries(pd[pd_index] & page_table_entry_address_mask);
                if (!pt || !(pt[pt_index] & page_table_entry_present))
                {
                    return false;
                }

                const auto gpa_page_base = pt[pt_index] & page_table_entry_address_mask;
                const auto guest_va_page = this->translate_guest_physical_address(gpa_page_base);
                if (!guest_va_page.has_value())
                {
                    return false;
                }

                const auto it = this->mapped_pages_.find(*guest_va_page);
                if (it == this->mapped_pages_.end() || !it->second || !it->second->host_page)
                {
                    return false;
                }

                const auto page_offset = static_cast<size_t>(va & 0xFFF);
                const auto* src = static_cast<const uint8_t*>(it->second->host_page) + page_offset;
                std::memcpy(kvm_run_->mmio.data, src, (std::min)(static_cast<size_t>(len), sizeof(kvm_run_->mmio.data)));
                return true;
            }

            bool handle_mmio_exit()
            {
                const auto gpa = kvm_run_->mmio.phys_addr;
                const uint32_t len = kvm_run_->mmio.len;
                const bool is_write = kvm_run_->mmio.is_write != 0;

                // Zero the read buffer upfront so any incomplete handler returns 0 to the guest
                // rather than stale data from a prior MMIO exit.
                if (!is_write)
                {
                    std::memset(kvm_run_->mmio.data, 0, sizeof(kvm_run_->mmio.data));
                }

                const auto va_opt = this->translate_guest_physical_address(gpa);
                const auto mmio_address = va_opt.value_or(gpa);

                if (auto* region = this->find_mmio_region(mmio_address))
                {
                    const auto offset = mmio_address - region->address;
                    if (is_write)
                    {
                        region->write_cb(offset, kvm_run_->mmio.data, len);
                    }
                    else
                    {
                        region->read_cb(offset, kvm_run_->mmio.data, len);
                    }
                    return true;
                }

                const auto operation = is_write ? memory_operation::write : memory_operation::read;
                const auto gs_base_before = is_write ? 0ULL : this->get_segment_base(x86_register::gs);
                for (auto& [_, hook] : this->memory_violation_hooks_)
                {
                    const auto result = hook(mmio_address, len, operation, memory_violation_type::unmapped);
                    if (result == memory_violation_continuation::restart)
                    {
                        // The hook re-mapped something (e.g., fixed GS base or mapped a guard page).
                        // KVM cannot re-execute the intercepted instruction, so we provide the read
                        // data ourselves. Compute the corrected VA: if GS base changed, adjust by
                        // the delta; otherwise the original VA is now accessible.
                        if (!is_write && va_opt.has_value())
                        {
                            const auto gs_base_after = this->get_segment_base(x86_register::gs);
                            const auto corrected_va = *va_opt + (gs_base_after - gs_base_before);
                            this->try_fill_mmio_read_from_va(corrected_va, len);
                        }
                        return true;
                    }
                    if (result == memory_violation_continuation::resume)
                    {
                        return true;
                    }
                }

                return false;
            }

            bool handle_debug_exit()
            {
                const auto& arch = kvm_run_->debug.arch;

                if (arch.exception == 3)
                {
                    // int3 breakpoint: arch.pc points to instruction after the int3
                    const auto bp_address = arch.pc - 1;
                    if (this->handle_patched_execution_breakpoint(bp_address))
                    {
                        return true;
                    }

                    // Non-patched INT3 (e.g. DebugBreak): still in ring-3, so just fix
                    // RIP to point at the int3 (Windows STATUS_BREAKPOINT convention)
                    // and dispatch to interrupt hooks.
                    auto regs = this->get_kvm_regs();
                    regs.rip = bp_address;
                    this->set_kvm_regs(regs);

                    for (auto& [_, hook] : this->interrupt_hooks_)
                    {
                        hook(3);
                    }
                    return !this->interrupt_hooks_.empty();
                }

                if (arch.exception == 1)
                {
                    // Single-step (BS bit in DR6) or hardware breakpoint
                    if (arch.dr6 & (1ull << 14))
                    {
                        return this->complete_execution_step();
                    }
                }

                return false;
            }

            // --------------------------------------------------------------------------
            // Execution hook machinery (int3 patched breakpoints + single-step)
            // --------------------------------------------------------------------------

            bool handle_patched_execution_breakpoint(const uint64_t address)
            {
                if (!this->patched_execution_breakpoints_.contains(address))
                {
                    return false;
                }

                auto regs = this->get_kvm_regs();
                regs.rip = address;
                this->set_kvm_regs(regs);
                this->set_patched_execution_breakpoint_state(address, false);

                std::vector<memory_execution_hook_callback> callbacks{};
                for (const auto& [_, hook] : this->memory_execution_hooks_)
                {
                    if (hook.address && hook.patched_breakpoint && *hook.address == address)
                    {
                        callbacks.push_back(hook.callback);
                    }
                }

                for (const auto& callback : callbacks)
                {
                    callback(address);
                }

                this->deferred_patched_breakpoint_ = address;
                if (!this->stop_requested_)
                {
                    if (!this->arm_patched_breakpoint_single_step(address, false))
                    {
                        throw std::runtime_error("Nested KVM execution single-step state is not supported");
                    }
                }

                return true;
            }

            bool complete_execution_step()
            {
                if (!this->pending_execution_step_)
                {
                    return false;
                }

                const auto state = std::exchange(this->pending_execution_step_, std::nullopt);
                this->update_guest_debug();

                if (state->page_base)
                {
                    this->remap_hook_page(*state->page_base);
                }
                if (state->patched_breakpoint)
                {
                    this->set_patched_execution_breakpoint_state(*state->patched_breakpoint, true);
                }

                if (state->stop_after_step)
                {
                    this->stop_requested_ = true;
                }

                return true;
            }

            bool arm_execution_single_step(const std::optional<uint64_t> page_base, const bool stop_after_step)
            {
                if (this->pending_execution_step_)
                {
                    if (!page_base || this->pending_execution_step_->page_base)
                    {
                        return false;
                    }
                    this->pending_execution_step_->page_base = page_base;
                }
                else
                {
                    pending_execution_step state{};
                    state.page_base = page_base;
                    state.stop_after_step = stop_after_step;
                    this->pending_execution_step_ = state;
                }

                this->update_guest_debug();

                if (page_base)
                {
                    const auto entry = this->mapped_pages_.find(*page_base);
                    if (entry != this->mapped_pages_.end() && entry->second)
                    {
                        this->remap_page(*entry->second);
                    }
                }

                return true;
            }

            bool arm_patched_breakpoint_single_step(const uint64_t address, const bool stop_after_step)
            {
                if (!this->arm_execution_single_step(std::nullopt, stop_after_step))
                {
                    return false;
                }
                this->pending_execution_step_->patched_breakpoint = address;
                this->deferred_patched_breakpoint_.reset();
                return true;
            }

            // --------------------------------------------------------------------------
            // int3 patched breakpoint management
            // --------------------------------------------------------------------------

            void try_apply_patched_execution_breakpoint(const uint64_t address, patched_execution_breakpoint& breakpoint)
            {
                const auto page_base = align_down_to_page(address);
                const auto entry = this->mapped_pages_.find(page_base);
                if (entry == this->mapped_pages_.end() || !entry->second || entry->second->host_page == nullptr)
                {
                    breakpoint.applied = false;
                    return;
                }

                const auto offset = static_cast<size_t>(address - page_base);
                auto* page = static_cast<std::byte*>(entry->second->host_page);
                if (breakpoint.applied && page[offset] == std::byte{0xCC})
                {
                    return;
                }

                breakpoint.original_byte = page[offset];
                page[offset] = std::byte{0xCC};
                breakpoint.applied = true;
            }

            void apply_patched_execution_breakpoints(const uint64_t page_base)
            {
                for (auto& [address, breakpoint] : this->patched_execution_breakpoints_)
                {
                    if (align_down_to_page(address) == page_base)
                    {
                        this->try_apply_patched_execution_breakpoint(address, breakpoint);
                    }
                }
            }

            void mark_patched_execution_breakpoints_unmapped(const uint64_t page_base)
            {
                for (auto& [address, breakpoint] : this->patched_execution_breakpoints_)
                {
                    if (align_down_to_page(address) == page_base)
                    {
                        breakpoint.original_byte.reset();
                        breakpoint.applied = false;
                    }
                }
            }

            void install_patched_execution_breakpoint(const uint64_t address)
            {
                auto existing = this->patched_execution_breakpoints_.find(address);
                if (existing != this->patched_execution_breakpoints_.end())
                {
                    ++existing->second.hook_count;
                    if (!existing->second.applied)
                    {
                        this->try_apply_patched_execution_breakpoint(address, existing->second);
                    }
                    return;
                }
                auto breakpoint = patched_execution_breakpoint{.hook_count = 1};
                this->try_apply_patched_execution_breakpoint(address, breakpoint);
                this->patched_execution_breakpoints_[address] = breakpoint;
            }

            void uninstall_patched_execution_breakpoint(const uint64_t address)
            {
                auto existing = this->patched_execution_breakpoints_.find(address);
                if (existing == this->patched_execution_breakpoints_.end())
                {
                    return;
                }
                if (existing->second.hook_count > 1)
                {
                    --existing->second.hook_count;
                    return;
                }
                if (existing->second.applied && existing->second.original_byte)
                {
                    auto original = *existing->second.original_byte;
                    (void)this->access_memory(address, &original, sizeof(original), true);
                }
                this->patched_execution_breakpoints_.erase(existing);
            }

            void set_patched_execution_breakpoint_state(const uint64_t address, const bool applied)
            {
                const auto existing = this->patched_execution_breakpoints_.find(address);
                if (existing == this->patched_execution_breakpoints_.end())
                {
                    return;
                }
                if (applied && !existing->second.original_byte)
                {
                    this->try_apply_patched_execution_breakpoint(address, existing->second);
                    return;
                }
                if (!existing->second.original_byte)
                {
                    return;
                }
                auto value = applied ? std::byte{0xCC} : *existing->second.original_byte;
                if (!this->access_memory(address, &value, sizeof(value), true))
                {
                    existing->second.applied = false;
                    existing->second.original_byte.reset();
                    return;
                }
                existing->second.applied = applied;
            }

            void deactivate_execution_hook(emulator_hook* hook)
            {
                const auto execution_it = this->memory_execution_hooks_.find(hook);
                if (execution_it == this->memory_execution_hooks_.end())
                {
                    return;
                }
                const auto& execution = execution_it->second;
                if (!execution.address)
                {
                    return;
                }
                if (execution.patched_breakpoint)
                {
                    this->uninstall_patched_execution_breakpoint(*execution.address);
                }
            }

            void overlay_patched_breakpoints(const uint64_t address, void* data, const size_t size) const
            {
                auto current_address = address;
                auto* cursor = static_cast<std::byte*>(data);
                auto remaining = size;

                while (remaining > 0)
                {
                    const auto page_base = align_down_to_page(current_address);
                    const auto offset = static_cast<size_t>(current_address - page_base);
                    const auto chunk = (std::min)(remaining, static_cast<size_t>(page_size - offset));

                    for (const auto& [breakpoint_address, breakpoint] : this->patched_execution_breakpoints_)
                    {
                        if (!breakpoint.original_byte || breakpoint_address < current_address ||
                            breakpoint_address >= current_address + chunk)
                        {
                            continue;
                        }
                        cursor[breakpoint_address - current_address] = *breakpoint.original_byte;
                    }

                    current_address += chunk;
                    cursor += chunk;
                    remaining -= chunk;
                }
            }

            void overlay_patched_breakpoints(const uint64_t address, const void* data, const size_t size)
            {
                auto current_address = address;
                const auto* cursor = static_cast<const std::byte*>(data);
                auto remaining = size;

                while (remaining > 0)
                {
                    const auto page_base = align_down_to_page(current_address);
                    const auto entry = this->mapped_pages_.find(page_base);
                    if (entry == this->mapped_pages_.end() || !entry->second || entry->second->host_page == nullptr)
                    {
                        return;
                    }

                    const auto offset_in_page = static_cast<size_t>(current_address - page_base);
                    const auto chunk = (std::min)(remaining, static_cast<size_t>(page_size - offset_in_page));
                    auto* page_ptr = static_cast<std::byte*>(entry->second->host_page) + offset_in_page;

                    for (auto& [breakpoint_address, breakpoint] : this->patched_execution_breakpoints_)
                    {
                        if (breakpoint_address < current_address || breakpoint_address >= current_address + chunk)
                        {
                            continue;
                        }
                        const auto bp_offset = static_cast<size_t>(breakpoint_address - current_address);
                        breakpoint.original_byte = cursor[bp_offset];
                        page_ptr[bp_offset] = std::byte{0xCC};
                        breakpoint.applied = true;
                    }

                    current_address += chunk;
                    cursor += chunk;
                    remaining -= chunk;
                }
            }

            // --------------------------------------------------------------------------
            // MMIO region lookup
            // --------------------------------------------------------------------------

            mmio_region* find_mmio_region(const uint64_t address)
            {
                for (auto& [base, region] : this->mmio_regions_)
                {
                    if (address >= base && address < base + region.size)
                    {
                        return &region;
                    }
                }
                return nullptr;
            }

            // --------------------------------------------------------------------------
            // KVM register access helpers
            // --------------------------------------------------------------------------

            struct kvm_regs get_kvm_regs() const
            {
                struct kvm_regs regs{};
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_REGS, &regs));
                return regs;
            }

            void set_kvm_regs(const struct kvm_regs& regs)
            {
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_REGS, &regs));
            }

            struct kvm_sregs get_kvm_sregs() const
            {
                struct kvm_sregs sregs{};
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_SREGS, &sregs));
                return sregs;
            }

            void set_kvm_sregs(const struct kvm_sregs& sregs)
            {
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_SREGS, &sregs));
            }

            struct kvm_fpu get_kvm_fpu() const
            {
                struct kvm_fpu fpu{};
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_FPU, &fpu));
                return fpu;
            }

            void set_kvm_fpu(const struct kvm_fpu& fpu)
            {
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_FPU, &fpu));
            }

            struct kvm_debugregs get_kvm_debugregs() const
            {
                struct kvm_debugregs dbg{};
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_DEBUGREGS, &dbg));
                return dbg;
            }

            void set_kvm_debugregs(const struct kvm_debugregs& dbg)
            {
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_DEBUGREGS, &dbg));
            }

            uint64_t get_msr(const uint32_t index) const
            {
                alignas(kvm_msrs) std::byte buf[sizeof(kvm_msrs) + sizeof(kvm_msr_entry)]{};
                auto& msrs = *reinterpret_cast<kvm_msrs*>(buf);
                msrs.nmsrs = 1;
                msrs.entries[0].index = index;
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_GET_MSRS, &msrs));
                return msrs.entries[0].data;
            }

            void set_msr(const uint32_t index, const uint64_t value)
            {
                alignas(kvm_msrs) std::byte buf[sizeof(kvm_msrs) + sizeof(kvm_msr_entry)]{};
                auto& msrs = *reinterpret_cast<kvm_msrs*>(buf);
                msrs.nmsrs = 1;
                msrs.entries[0].index = index;
                msrs.entries[0].data = value;
                KVM_CHECK(::ioctl(vcpu_fd_.get(), KVM_SET_MSRS, &msrs));
            }

            static uint64_t* cr_field(struct kvm_sregs& sregs, const uint32_t cr_index)
            {
                switch (cr_index)
                {
                case 0:
                    return reinterpret_cast<uint64_t*>(&sregs.cr0);
                case 2:
                    return reinterpret_cast<uint64_t*>(&sregs.cr2);
                case 3:
                    return reinterpret_cast<uint64_t*>(&sregs.cr3);
                case 4:
                    return reinterpret_cast<uint64_t*>(&sregs.cr4);
                case 8:
                    return reinterpret_cast<uint64_t*>(&sregs.cr8);
                default:
                    return nullptr;
                }
            }

            static uint64_t* dr_field(struct kvm_debugregs& dbg, const uint32_t dr_index)
            {
                switch (dr_index)
                {
                case 0:
                    return reinterpret_cast<uint64_t*>(&dbg.db[0]);
                case 1:
                    return reinterpret_cast<uint64_t*>(&dbg.db[1]);
                case 2:
                    return reinterpret_cast<uint64_t*>(&dbg.db[2]);
                case 3:
                    return reinterpret_cast<uint64_t*>(&dbg.db[3]);
                case 6:
                    return reinterpret_cast<uint64_t*>(&dbg.dr6);
                case 7:
                    return reinterpret_cast<uint64_t*>(&dbg.dr7);
                default:
                    return nullptr;
                }
            }

            // --------------------------------------------------------------------------
            // KVM memory slot management
            // --------------------------------------------------------------------------

            uint32_t allocate_kvm_slot_id()
            {
                if (!free_kvm_slot_ids_.empty())
                {
                    const auto id = free_kvm_slot_ids_.back();
                    free_kvm_slot_ids_.pop_back();
                    return id;
                }
                return next_kvm_slot_id_++;
            }

            void free_kvm_slot_id(const uint32_t id)
            {
                free_kvm_slot_ids_.push_back(id);
            }

            void kvm_delete_slot(const uint32_t slot_id, const uint64_t gpa_base)
            {
                struct kvm_userspace_memory_region region{};
                region.slot = slot_id;
                region.guest_phys_addr = gpa_base;
                region.memory_size = 0;
                (void)::ioctl(vm_fd_.get(), KVM_SET_USER_MEMORY_REGION, &region);
            }

            void kvm_create_slot(const uint32_t slot_id, const uint64_t gpa_base, const uint64_t size, void* host_base,
                                 const uint32_t kvm_flags)
            {
                struct kvm_userspace_memory_region region{};
                region.slot = slot_id;
                region.flags = kvm_flags;
                region.guest_phys_addr = gpa_base;
                region.memory_size = size;
                region.userspace_addr = reinterpret_cast<uint64_t>(host_base);
                KVM_CHECK(::ioctl(vm_fd_.get(), KVM_SET_USER_MEMORY_REGION, &region));
            }

            // Remove all KVM slots that overlap [gpa_start, gpa_start+size), potentially
            // splitting boundary slots and recreating the non-overlapping parts.
            void remove_kvm_slots_in_range(const uint64_t gpa_start, const uint64_t gpa_end)
            {
                struct split_piece
                {
                    uint64_t gpa;
                    uint64_t size;
                    void* host_base;
                    uint32_t kvm_flags;
                };
                std::vector<split_piece> pieces_to_add{};

                auto it = active_kvm_slots_.lower_bound(gpa_start);
                if (it != active_kvm_slots_.begin())
                {
                    --it;
                    if (it->second.gpa_base + it->second.size <= gpa_start)
                    {
                        ++it;
                    }
                }

                while (it != active_kvm_slots_.end() && it->second.gpa_base < gpa_end)
                {
                    auto& slot = it->second;
                    const uint64_t slot_end = slot.gpa_base + slot.size;

                    kvm_delete_slot(slot.id, slot.gpa_base);
                    free_kvm_slot_id(slot.id);

                    if (slot.gpa_base < gpa_start)
                    {
                        pieces_to_add.push_back({slot.gpa_base, gpa_start - slot.gpa_base, slot.host_base, slot.kvm_flags});
                    }

                    if (slot_end > gpa_end)
                    {
                        const auto right_offset = gpa_end - slot.gpa_base;
                        pieces_to_add.push_back(
                            {gpa_end, slot_end - gpa_end, static_cast<uint8_t*>(slot.host_base) + right_offset, slot.kvm_flags});
                    }

                    it = active_kvm_slots_.erase(it);
                }

                for (const auto& piece : pieces_to_add)
                {
                    const auto new_id = allocate_kvm_slot_id();
                    kvm_create_slot(new_id, piece.gpa, piece.size, piece.host_base, piece.kvm_flags);
                    active_kvm_slots_[piece.gpa] = {new_id, piece.gpa, piece.size, piece.host_base, piece.kvm_flags};
                }
            }

            void remove_kvm_slot(const uint64_t gpa, const uint64_t size)
            {
                remove_kvm_slots_in_range(gpa, gpa + size);
            }

            // --------------------------------------------------------------------------
            // Page permission → KVM slot flags
            // --------------------------------------------------------------------------

            static uint32_t to_kvm_map_flags(const memory_permission permissions)
            {
                if ((permissions & memory_permission::read) == memory_permission::none)
                {
                    return 0; // signal: no slot
                }
                if ((permissions & memory_permission::write) == memory_permission::none)
                {
                    return KVM_MEM_READONLY;
                }
                return 0;
            }

            static bool should_map_in_kvm(const memory_permission permissions)
            {
                return (permissions & memory_permission::read) != memory_permission::none;
            }

            memory_permission effective_page_permissions(const mapped_page& page) const
            {
                auto permissions = page.permissions;
                const auto stepped_page = this->pending_execution_step_ ? this->pending_execution_step_->page_base : std::nullopt;
                if (page.page_execution_hook_count != 0 && stepped_page != page.guest_page_base)
                {
                    permissions &= ~memory_permission::exec;
                }
                return permissions;
            }

            // --------------------------------------------------------------------------
            // Page remapping (register with KVM slots + update PTE NX bit)
            // --------------------------------------------------------------------------

            static uint64_t make_pte_entry(const uint64_t gpa, const memory_permission permissions, const bool is_mmio = false)
            {
                if (!should_map_in_kvm(permissions))
                {
                    // MMIO pages need a present PTE so the CPU reaches the GPA and KVM exits with
                    // KVM_EXIT_MMIO.  Guard/inaccessible pages use PTE=0 so any access causes a
                    // #PF through the IDT rather than an MMIO exit (MMIO exits can't replay the
                    // faulting instruction, which breaks guard-page write semantics).
                    if (is_mmio)
                    {
                        return gpa | page_table_entry_present | page_table_entry_writable | page_table_entry_user | page_table_entry_nx;
                    }
                    return 0;
                }
                const bool exec_allowed = (permissions & memory_permission::exec) != memory_permission::none;
                return gpa | page_table_entry_present | page_table_entry_writable | page_table_entry_user |
                       (exec_allowed ? 0 : page_table_entry_nx);
            }

            void set_pte_entry(const uint64_t guest_address, const uint64_t new_entry)
            {
                const auto page_base = align_down_to_page(guest_address);
                const auto pml4_index = static_cast<size_t>((page_base >> 39) & 0x1FF);
                const auto pdpt_index = static_cast<size_t>((page_base >> 30) & 0x1FF);
                const auto pd_index = static_cast<size_t>((page_base >> 21) & 0x1FF);
                const auto pt_index = static_cast<size_t>((page_base >> 12) & 0x1FF);

                auto* pml4 = this->get_page_table_entries(this->pml4_gpa_);
                if (!pml4 || !(pml4[pml4_index] & page_table_entry_present))
                {
                    return;
                }
                auto* pdpt = this->get_page_table_entries(pml4[pml4_index] & page_table_entry_address_mask);
                if (!pdpt || !(pdpt[pdpt_index] & page_table_entry_present))
                {
                    return;
                }
                auto* pd = this->get_page_table_entries(pdpt[pdpt_index] & page_table_entry_address_mask);
                if (!pd || !(pd[pd_index] & page_table_entry_present))
                {
                    return;
                }
                auto* pt = this->get_page_table_entries(pd[pd_index] & page_table_entry_address_mask);
                if (!pt)
                {
                    return;
                }
                pt[pt_index] = new_entry;
            }

            void remap_page(mapped_page& page)
            {
                if (page.guest_physical_address == unmapped_guest_page)
                {
                    throw std::runtime_error("KVM page remapped without assigned guest physical address");
                }

                const auto new_permissions = this->effective_page_permissions(page);
                const bool had_mapping = page.map_flags != 0;

                if (had_mapping)
                {
                    remove_kvm_slots_in_range(page.guest_physical_address, page.guest_physical_address + page_size);
                }

                page.map_flags = 0;

                if (!should_map_in_kvm(new_permissions))
                {
                    this->set_pte_entry(page.guest_page_base, make_pte_entry(page.guest_physical_address, new_permissions, page.is_mmio));
                    return;
                }

                const auto kvm_flags = to_kvm_map_flags(new_permissions);
                const auto slot_id = allocate_kvm_slot_id();
                kvm_create_slot(slot_id, page.guest_physical_address, page_size, page.host_page, kvm_flags);
                active_kvm_slots_[page.guest_physical_address] = {slot_id, page.guest_physical_address, page_size, page.host_page,
                                                                  kvm_flags};
                page.map_flags = 1;

                this->set_pte_entry(page.guest_page_base, make_pte_entry(page.guest_physical_address, new_permissions, page.is_mmio));
            }

            void remap_pages(const uint64_t address, const size_t size)
            {
                if (size == 0)
                {
                    return;
                }

                size_t offset = 0;
                while (offset < size)
                {
                    const auto run_address = address + offset;
                    auto entry = this->mapped_pages_.find(run_address);
                    if (entry == this->mapped_pages_.end() || !entry->second || entry->second->host_page == nullptr)
                    {
                        offset += page_size;
                        continue;
                    }

                    auto* const run_host_base = static_cast<uint8_t*>(entry->second->host_page);
                    const auto run_gpa = entry->second->guest_physical_address;
                    const auto run_permissions = this->effective_page_permissions(*entry->second);
                    const auto run_kvm_flags = to_kvm_map_flags(run_permissions);
                    const auto run_should_map = should_map_in_kvm(run_permissions);
                    const auto run_had_mapping = entry->second->map_flags != 0;

                    size_t run_size = page_size;
                    while (offset + run_size < size)
                    {
                        const auto next_address = address + offset + run_size;
                        const auto next_entry = this->mapped_pages_.find(next_address);
                        if (next_entry == this->mapped_pages_.end() || !next_entry->second || next_entry->second->host_page == nullptr)
                        {
                            break;
                        }

                        const auto next_permissions = this->effective_page_permissions(*next_entry->second);
                        const auto next_kvm_flags = to_kvm_map_flags(next_permissions);
                        const auto next_should_map = should_map_in_kvm(next_permissions);
                        const auto next_had_mapping = next_entry->second->map_flags != 0;
                        auto* const expected_host = run_host_base + run_size;
                        const auto expected_gpa = run_gpa + run_size;

                        if (next_entry->second->host_page != expected_host || next_kvm_flags != run_kvm_flags ||
                            next_should_map != run_should_map || next_had_mapping != run_had_mapping ||
                            next_entry->second->guest_physical_address != expected_gpa)
                        {
                            break;
                        }
                        run_size += page_size;
                    }

                    // Remove old slot(s) for this range
                    if (run_had_mapping)
                    {
                        remove_kvm_slots_in_range(run_gpa, run_gpa + run_size);
                    }

                    // Create new slot if needed
                    if (run_should_map)
                    {
                        const auto slot_id = allocate_kvm_slot_id();
                        kvm_create_slot(slot_id, run_gpa, run_size, run_host_base, run_kvm_flags);
                        active_kvm_slots_[run_gpa] = {slot_id, run_gpa, run_size, run_host_base, run_kvm_flags};
                    }

                    // Update map_flags and PTEs for all pages in the run
                    for (size_t run_offset = 0; run_offset < run_size; run_offset += page_size)
                    {
                        auto& run_entry = this->mapped_pages_.at(run_address + run_offset);
                        run_entry->map_flags = run_should_map ? 1 : 0;
                        const auto pte = make_pte_entry(run_entry->guest_physical_address, run_permissions, run_entry->is_mmio);
                        this->set_pte_entry(run_entry->guest_page_base, pte);
                    }

                    offset += run_size;
                }
            }

            void remap_hook_page(const uint64_t address)
            {
                const auto page_base = align_down_to_page(address);
                const auto entry = this->mapped_pages_.find(page_base);
                if (entry != this->mapped_pages_.end() && entry->second && entry->second->host_page != nullptr)
                {
                    this->remap_page(*entry->second);
                }
            }

            // --------------------------------------------------------------------------
            // Guest physical page pool
            // --------------------------------------------------------------------------

            uint64_t allocate_internal_page(const bool executable = false, const bool map_into_guest = true)
            {
                auto backing = allocate_backing_memory(page_size);
                auto* raw_page = backing.get();
                const auto page_gpa = this->next_internal_gpa_;
                this->next_internal_gpa_ += page_size;

                auto page = std::make_unique<mapped_page>();
                page->owned_page = std::move(backing);
                page->host_page = raw_page;
                page->guest_physical_address = page_gpa;
                page->permissions = executable ? memory_permission::all : memory_permission::read_write;

                this->mapped_pages_[page_gpa] = std::move(page);
                this->remap_page(*this->mapped_pages_[page_gpa]);
                this->page_table_views_[page_gpa] = reinterpret_cast<uint64_t*>(raw_page);

                if (map_into_guest)
                {
                    this->ensure_virtual_mapping(page_gpa);
                }

                return page_gpa;
            }

            uint64_t allocate_guest_physical_page()
            {
                size_t page_index = 0;
                if (!this->free_guest_gpa_indices_.empty())
                {
                    page_index = this->free_guest_gpa_indices_.back();
                    this->free_guest_gpa_indices_.pop_back();
                    if (this->guest_pages_by_gpa_[page_index] != unmapped_guest_page)
                    {
                        throw std::runtime_error("KVM guest physical free list corruption");
                    }
                }
                else
                {
                    page_index = this->guest_pages_by_gpa_.size();
                    const auto page_gpa = guest_physical_memory_base + (static_cast<uint64_t>(page_index) * page_size);
                    if (page_gpa >= internal_page_table_base)
                    {
                        throw std::runtime_error("KVM guest physical address space exhausted");
                    }
                    this->guest_pages_by_gpa_.push_back(unmapped_guest_page);
                }
                return guest_physical_memory_base + (static_cast<uint64_t>(page_index) * page_size);
            }

            void assign_guest_physical_page(const uint64_t guest_address, mapped_page& page)
            {
                if (page.guest_physical_address != unmapped_guest_page)
                {
                    throw std::runtime_error("KVM guest physical page already assigned");
                }
                page.guest_page_base = align_down_to_page(guest_address);
                page.guest_physical_address = this->allocate_guest_physical_page();
                page.page_execution_hook_count = std::ranges::count_if(this->memory_execution_hooks_, [&](const auto& e) {
                    const auto& hook = e.second;
                    return hook.address && !hook.patched_breakpoint && hook.size != 0 &&
                           regions_with_length_intersect(*hook.address, hook.size, page.guest_page_base, page_size);
                });
                this->guest_pages_by_gpa_[this->guest_physical_page_index(page.guest_physical_address)] = page.guest_page_base;
            }

            void release_guest_physical_page(const uint64_t guest_physical_address)
            {
                if (guest_physical_address == unmapped_guest_page)
                {
                    throw std::runtime_error("KVM guest physical page released before assignment");
                }
                if (guest_physical_address < guest_physical_memory_base || guest_physical_address >= internal_page_table_base)
                {
                    return;
                }
                const auto page_index = this->guest_physical_page_index(guest_physical_address);
                if (this->guest_pages_by_gpa_[page_index] == unmapped_guest_page)
                {
                    throw std::runtime_error("KVM guest physical page double release");
                }
                this->guest_pages_by_gpa_[page_index] = unmapped_guest_page;
                this->free_guest_gpa_indices_.push_back(page_index);
            }

            std::optional<uint64_t> translate_guest_physical_address(const uint64_t gpa) const
            {
                if (gpa < guest_physical_memory_base)
                {
                    return std::nullopt;
                }
                const auto page_base = align_down_to_page(gpa);
                const auto page_index = static_cast<size_t>((page_base - guest_physical_memory_base) / page_size);
                if (page_index >= this->guest_pages_by_gpa_.size())
                {
                    return std::nullopt;
                }
                const auto guest_page = this->guest_pages_by_gpa_[page_index];
                if (guest_page == unmapped_guest_page)
                {
                    return std::nullopt;
                }
                return guest_page + (gpa - page_base);
            }

            size_t guest_physical_page_index(const uint64_t gpa) const
            {
                if (gpa < guest_physical_memory_base || !is_page_aligned(gpa))
                {
                    throw std::runtime_error("Invalid KVM guest physical page");
                }
                const auto page_index = static_cast<size_t>((gpa - guest_physical_memory_base) / page_size);
                if (page_index >= this->guest_pages_by_gpa_.size())
                {
                    throw std::runtime_error("Unknown KVM guest physical page");
                }
                return page_index;
            }

            // --------------------------------------------------------------------------
            // Guest page table management
            // --------------------------------------------------------------------------

            uint64_t* get_page_table_entries(const uint64_t page_gpa)
            {
                const auto it = this->page_table_views_.find(page_gpa);
                if (it == this->page_table_views_.end())
                {
                    return nullptr;
                }
                return it->second;
            }

            uint64_t ensure_child_table(const uint64_t table_gpa, const size_t index)
            {
                auto* const table_entries = this->get_page_table_entries(table_gpa);
                auto& entry = table_entries[index];
                if ((entry & page_table_entry_present) == 0)
                {
                    const auto child_gpa = this->allocate_internal_page(false, false);
                    entry = child_gpa | page_table_entry_present | page_table_entry_writable | page_table_entry_user;
                    return child_gpa;
                }
                return entry & page_table_entry_address_mask;
            }

            void ensure_virtual_mapping(const uint64_t guest_address)
            {
                const auto page_base = align_down_to_page(guest_address);
                const auto mapped = this->mapped_pages_.find(page_base);
                if (mapped == this->mapped_pages_.end() || !mapped->second)
                {
                    throw std::runtime_error("KVM virtual mapping requested for missing page");
                }

                const auto pml4_index = static_cast<size_t>((page_base >> 39) & 0x1FF);
                const auto pdpt_index = static_cast<size_t>((page_base >> 30) & 0x1FF);
                const auto pd_index = static_cast<size_t>((page_base >> 21) & 0x1FF);
                const auto pt_index = static_cast<size_t>((page_base >> 12) & 0x1FF);

                const auto pdpt_gpa = this->ensure_child_table(this->pml4_gpa_, pml4_index);
                const auto pd_gpa = this->ensure_child_table(pdpt_gpa, pdpt_index);
                const auto pt_gpa = this->ensure_child_table(pd_gpa, pd_index);

                auto* const pt_entries = this->get_page_table_entries(pt_gpa);
                const auto& page = mapped->second;
                pt_entries[pt_index] = make_pte_entry(page->guest_physical_address, page->permissions, page->is_mmio);
            }

            bool clear_virtual_mapping(const uint64_t guest_address)
            {
                const auto page_base = align_down_to_page(guest_address);
                const auto pml4_index = static_cast<size_t>((page_base >> 39) & 0x1FF);
                const auto pdpt_index = static_cast<size_t>((page_base >> 30) & 0x1FF);
                const auto pd_index = static_cast<size_t>((page_base >> 21) & 0x1FF);
                const auto pt_index = static_cast<size_t>((page_base >> 12) & 0x1FF);

                auto* pml4_entries = this->get_page_table_entries(this->pml4_gpa_);
                const auto pml4_entry = pml4_entries[pml4_index];
                if ((pml4_entry & page_table_entry_present) == 0)
                {
                    return false;
                }

                auto* pdpt_entries = this->get_page_table_entries(pml4_entry & page_table_entry_address_mask);
                const auto pdpt_entry = pdpt_entries[pdpt_index];
                if ((pdpt_entry & page_table_entry_present) == 0)
                {
                    return false;
                }

                auto* pd_entries = this->get_page_table_entries(pdpt_entry & page_table_entry_address_mask);
                const auto pd_entry = pd_entries[pd_index];
                if ((pd_entry & page_table_entry_present) == 0)
                {
                    return false;
                }

                auto* pt_entries = this->get_page_table_entries(pd_entry & page_table_entry_address_mask);
                auto& pt_entry = pt_entries[pt_index];
                if ((pt_entry & page_table_entry_present) == 0)
                {
                    return false;
                }

                pt_entry = 0;
                return true;
            }

            void flush_virtual_address_mappings()
            {
                auto sregs = this->get_kvm_sregs();
                // Re-writing CR3 to itself flushes the guest TLB
                const auto cr3 = sregs.cr3;
                sregs.cr3 = cr3;
                this->set_kvm_sregs(sregs);
            }

            // --------------------------------------------------------------------------
            // Host-side memory access
            // --------------------------------------------------------------------------

            bool access_memory(const uint64_t address, void* data, size_t size, const bool is_write) const
            {
                auto current_address = address;
                auto* cursor = static_cast<std::byte*>(data);

                while (size > 0)
                {
                    const auto page_base = align_down_to_page(current_address);
                    const auto entry = this->mapped_pages_.find(page_base);
                    if (entry == this->mapped_pages_.end() || !entry->second || entry->second->host_page == nullptr)
                    {
                        return false;
                    }

                    const auto offset = static_cast<size_t>(current_address - page_base);
                    const auto chunk = (std::min)(size, static_cast<size_t>(page_size - offset));
                    auto* page_ptr = static_cast<std::byte*>(entry->second->host_page) + offset;

                    if (is_write)
                    {
                        std::memcpy(page_ptr, cursor, chunk);
                    }
                    else
                    {
                        std::memcpy(cursor, page_ptr, chunk);
                    }

                    current_address += chunk;
                    cursor += chunk;
                    size -= chunk;
                }

                return true;
            }

            void advance_rip(const uint64_t amount)
            {
                auto regs = this->get_kvm_regs();
                regs.rip += amount;
                this->set_kvm_regs(regs);
            }

            void clear_pending_exception_state()
            {
                struct kvm_vcpu_events events{};
                if (::ioctl(vcpu_fd_.get(), KVM_GET_VCPU_EVENTS, &events) >= 0)
                {
                    events.exception.injected = 0;
                    events.exception.pending = 0;
                    events.interrupt.injected = 0;
                    (void)::ioctl(vcpu_fd_.get(), KVM_SET_VCPU_EVENTS, &events);
                }
            }

            emulator_hook* make_hook()
            {
                return reinterpret_cast<emulator_hook*>(this->next_hook_id_++);
            }
        };

    } // namespace

    std::unique_ptr<x86_64_emulator> create_x86_64_emulator()
    {
        return std::make_unique<kvm_x86_64_emulator>();
    }

} // namespace sogen::kvm
