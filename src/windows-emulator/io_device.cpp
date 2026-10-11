#include "std_include.hpp"
#include "io_device.hpp"
#include "windows_emulator.hpp"
#include "devices/afd_endpoint.hpp"
#include "devices/mount_point_manager.hpp"
#include "devices/security_support_provider.hpp"
#include "devices/named_pipe.hpp"
#include "devices/network_store_interface.hpp"
#include "devices/gpu_bridge.hpp"
#include "devices/steam_bridge.hpp"
#include "devices/console.hpp"
#include <iostream>
#include <random>

namespace sogen
{

    namespace
    {
        struct dummy_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& context) override
            {
                if (context.output_buffer && context.output_buffer_length)
                {
                    win_emu.emu().set_memory(context.output_buffer, 0, context.output_buffer_length);
                }

                if (context.io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = context.output_buffer_length;
                    context.io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }
        };

        // A device whose handle opens but that fails every ioctl. Used for the PnP DevQuery interface that
        // audioses probes for device properties: a clean error makes the caller fall back to its defaults,
        // whereas returning success with no data leads it to read uninitialized output.
        struct unsupported_io_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator&, const io_device_context&) override
            {
                return STATUS_NOT_SUPPORTED;
            }
        };

        // bcryptprimitives seeds each process's user-mode RNG (ProcessPrng, RtlGenRandom, ...) from these KsecDD
        // ioctls on \Device\CNG. Identical seeds make every process generate the same "unguessable" tokens,
        // which Chromium treats as forged IPC when two renderers report the same V8 context or frame token.
        constexpr ULONG IOCTL_KSEC_RNG = 0x390004;
        constexpr ULONG IOCTL_KSEC_RNG_REKEY = 0x390008;

        struct cng_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& context) override
            {
                std::vector<std::byte> output(context.output_buffer ? context.output_buffer_length : 0);
                if (context.io_control_code == IOCTL_KSEC_RNG || context.io_control_code == IOCTL_KSEC_RNG_REKEY)
                {
                    std::random_device entropy{};
                    for (auto& byte : output)
                    {
                        byte = static_cast<std::byte>(entropy());
                    }
                }

                if (!output.empty())
                {
                    win_emu.emu().write_memory(context.output_buffer, output.data(), output.size());
                }

                if (context.io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = output.size();
                    context.io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }
        };

        struct transport_stub_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& context) override
            {
                if (context.output_buffer && context.output_buffer_length)
                {
                    win_emu.emu().set_memory(context.output_buffer, 0, context.output_buffer_length);
                }

                if (context.io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = context.output_buffer_length;
                    context.io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }
        };

        // Factories for the devices defined locally in this file, matching the shared create_*
        // (device_creation_context) signature so they slot straight into the registry.
        std::unique_ptr<io_device> create_dummy_device(const device_creation_context&)
        {
            return std::make_unique<dummy_device>();
        }

        std::unique_ptr<io_device> create_cng_device(const device_creation_context&)
        {
            return std::make_unique<cng_device>();
        }

        std::unique_ptr<io_device> create_transport_stub_device(const device_creation_context&)
        {
            return std::make_unique<transport_stub_device>();
        }

        std::unique_ptr<io_device> create_unsupported_io_device(const device_creation_context&)
        {
            return std::make_unique<unsupported_io_device>();
        }

        std::unique_ptr<io_device> create_named_pipe_device(const device_creation_context&)
        {
            return std::make_unique<named_pipe>();
        }

    }

    const std::map<std::u16string_view, device_factory>& get_device_registry()
    {
        using namespace std::string_view_literals;

        static const std::map<std::u16string_view, device_factory> registry = {
            // Dummy
            {u"CNG"sv, create_cng_device},
            {u"RasAcd"sv, create_dummy_device},
            {u"PcwDrv"sv, create_dummy_device},
            {u"DeviceApi\\CMApi"sv, create_dummy_device},
            {u"DeviceApi\\CMNotify"sv, create_dummy_device},
            {u"ConDrv\\Server"sv, create_dummy_device},
            {u"MMCSS\\MmThread"sv, create_dummy_device},
            {u"DeviceApi\\Dev\\Query"sv, create_unsupported_io_device},
            // Generic
            {u"Console"sv, create_console_device},
            // Multimedia Class Scheduler. avrt!AvSetMmThreadCharacteristics opens this to raise the audio
            // render worker's scheduling priority; the emulator has no priority classes, so accepting the open
            // and succeeding its ioctls is enough for the worker to proceed.
            {u"MMCSS\\MmThread"sv, create_dummy_device},
            {u"Nsi"sv, create_network_store_interface},
            {u"MountPointManager"sv, create_mount_point_manager},
            {u"KsecDD"sv, create_security_support_provider},
            {u"NamedPipe"sv, create_named_pipe_device},
            {u"SogenGpu"sv, create_gpu_bridge},
            {u"SogenSteam"sv, create_steam_bridge},
            // AFD
            {u"Afd\\Endpoint"sv, create_afd_endpoint},
            {u"Afd\\AsyncConnectHlp"sv, create_afd_async_connect_hlp},
            // Transport
            {u"Tcp"sv, create_transport_stub_device},
            {u"Tcp6"sv, create_transport_stub_device},
            {u"Udp"sv, create_transport_stub_device},
            {u"RawIp"sv, create_transport_stub_device},
        };
        return registry;
    }

    bool needs_32_bit_devices(const windows_emulator& win_emu)
    {
        return win_emu.process.is_wow64_process;
    }

    std::unique_ptr<io_device> create_device(const std::u16string_view device, const device_creation_context& context)
    {
        const auto& registry = get_device_registry();
        const auto it = registry.find(device);
        if (it == registry.end())
        {
            throw std::runtime_error("Unsupported device: " + u16_to_u8(device));
        }

        return it->second(context);
    }

    emulator_thread& io_device_context::thread() const
    {
        if (!this->vcpu)
        {
            throw std::runtime_error("I/O request has no issuing vCPU");
        }

        return this->vcpu->thread();
    }

    void io_device::post_completion(windows_emulator& win_emu, const io_device_context& c, const NTSTATUS status,
                                    const uint64_t information, const bool completed_synchronously) const
    {
        if (completed_synchronously && NT_SUCCESS(status) &&
            (this->get_completion_notification_modes() & FILE_SKIP_COMPLETION_PORT_ON_SUCCESS))
        {
            return;
        }

        const auto association = this->get_completion_port();
        if (!association)
        {
            return;
        }

        if (auto* completion = win_emu.process.io_completions.get(association->port))
        {
            io_completion_message message{};
            message.key_context = association->key;
            message.apc_context = c.apc_context;
            message.io_status_block.Status = status;
            message.io_status_block.Information = information;
            completion->enqueue(message);
        }
    }

    NTSTATUS io_device::execute_ioctl(windows_emulator& win_emu, const io_device_context& c, const bool completes_pending_request)
    {
        if (c.io_status_block)
        {
            c.io_status_block.write({});
        }

        const auto result = this->io_control(win_emu, c);
        write_io_status(c.io_status_block, result);

        // A synchronously-completing IOCTL must signal the optional completion event the caller passed, so a
        // thread that issues the request and then waits on the event is released. Asynchronous devices return
        // STATUS_PENDING and signal the event themselves once the delayed operation completes.
        if (result != STATUS_PENDING && c.event.bits)
        {
            if (auto* e = win_emu.process.events.get(c.event); e)
            {
                e->set_signaled(true);
            }
        }

        const auto information = c.io_status_block ? c.io_status_block.read().Information : 0;

        // WoW64 hands a pending request a transient 64-bit IO_STATUS_BLOCK, so the caller's own 32-bit block
        // (its OVERLAPPED, passed as ApcContext when no APC routine is used) is never updated by the write
        // above; the real kernel writes the 32-bit block directly when the request completes asynchronously.
        if (completes_pending_request && result != STATUS_PENDING && win_emu.process.is_wow64_process && c.apc_context && !c.apc_routine)
        {
            const auto status32 = static_cast<uint32_t>(result);
            const auto information32 = static_cast<uint32_t>(information);
            win_emu.emu().write_memory(c.apc_context, &status32, sizeof(status32));
            win_emu.emu().write_memory(c.apc_context + sizeof(status32), &information32, sizeof(information32));
        }

        // A handle associated with an I/O completion port (NtSetInformationFile's FileCompletionInformation,
        // i.e. CreateIoCompletionPort on an already-open handle) posts a completion packet for every finished
        // request on real Windows, even one that completed synchronously - callers built around
        // GetQueuedCompletionStatus never poll the event or the synchronous return value. Requests without an
        // APC context (no OVERLAPPED, e.g. mswsock's internal synchronous AFD calls) and requests that fail
        // synchronously post nothing; a thread pool TP_IO would otherwise receive completions it never started.
        if (result != STATUS_PENDING && c.apc_context != 0 && (completes_pending_request || !NT_ERROR(result)))
        {
            this->post_completion(win_emu, c, result, information, !completes_pending_request);
        }

        return result;
    }

    NTSTATUS io_device_container::io_control(windows_emulator& win_emu, const io_device_context& context)
    {
        this->assert_validity();
        win_emu.callbacks.on_ioctrl(*this->device_, this->device_name_, context.io_control_code);
        return this->device_->io_control(win_emu, context);
    }

    void io_device_container::work(windows_emulator& win_emu)
    {
        this->assert_validity();
        this->device_->work(win_emu);
    }

    void io_device_container::serialize_object(utils::buffer_serializer& buffer) const
    {
        this->assert_validity();

        buffer.write(this->is_32_bit_);
        buffer.write_string(this->device_name_);
        this->device_->serialize(buffer);
    }

    void io_device_container::deserialize_object(utils::buffer_deserializer& buffer)
    {
        buffer.read(this->is_32_bit_);
        buffer.read_string(this->device_name_);

        this->setup();
        this->device_->deserialize(buffer);
    }

} // namespace sogen
