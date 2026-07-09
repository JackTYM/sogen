#include "std_include.hpp"
#include "io_device.hpp"
#include "windows_emulator.hpp"
#include "devices/afd_endpoint.hpp"
#include "devices/mount_point_manager.hpp"
#include "devices/security_support_provider.hpp"
#include "devices/named_pipe.hpp"
#include "devices/network_store_interface.hpp"
#include "devices/gpu_bridge.hpp"
#include <iostream>

namespace sogen
{

    namespace
    {
        struct dummy_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator&, const io_device_context&) override
            {
                return STATUS_SUCCESS;
            }
        };

        struct transport_stub_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& context) override
            {
                if (context.output_buffer && context.output_buffer_length)
                {
                    std::vector<std::byte> output(context.output_buffer_length, std::byte{0});
                    win_emu.emu().write_memory(context.output_buffer, output.data(), output.size());
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

        // \Device\DeviceApi\CMApi: the cfgmgr32 client channel. dxgi.dll enumerates display adapters through
        // CM_Get_Device_ID_List for the display class, so we answer that query with a single software display
        // adapter; without it dxgi builds a null adapter-property map and crashes during vkEnumeratePhysicalDevices.
        struct configuration_manager_device : stateless_device
        {
            NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& c) override
            {
                // CM_Get_Device_ID_List over IOCTL 0x470803 (METHOD_NEITHER). Request header (24 bytes):
                //   { u32 cbSize; u32 filter; u64 payload; u32 payload_size; u32 out_size }
                // filter low byte 0x80 == CM_GETIDLIST_FILTER_CLASS; payload -> UTF-16 class-GUID string.
                // Reply is written into the output buffer as
                //   { u32 reserved; NTSTATUS status @+0x04; u32 list_bytes @+0x08; u32 reserved; wchar list[] @+0x10 }
                // "buffer too small" is signalled at +0x04, not via the IRP status, so we always return SUCCESS; the
                // same IOCTL serves the size query (out len 0x14) and the list query (out len 0x14 + list bytes).
                if (c.io_control_code != 0x470803 || c.input_buffer_length < 0x18 || !c.output_buffer || c.output_buffer_length < 0x10)
                {
                    return STATUS_SUCCESS;
                }

                uint32_t filter = 0;
                uint64_t payload = 0;
                uint32_t payload_size = 0;
                win_emu.emu().read_memory(c.input_buffer + 0x04, &filter, sizeof(filter));
                win_emu.emu().read_memory(c.input_buffer + 0x08, &payload, sizeof(payload));
                win_emu.emu().read_memory(c.input_buffer + 0x10, &payload_size, sizeof(payload_size));

                constexpr uint32_t cm_getidlist_filter_class = 0x80;
                if ((filter & 0xFF) != cm_getidlist_filter_class || !payload || payload_size < sizeof(char16_t))
                {
                    return STATUS_SUCCESS;
                }

                const uint32_t guid_chars = std::min<uint32_t>(payload_size / sizeof(char16_t), 64);
                std::u16string guid(guid_chars, u'\0');
                win_emu.emu().read_memory(payload, guid.data(), guid_chars * sizeof(char16_t));
                if (const auto terminator = guid.find(u'\0'); terminator != std::u16string::npos)
                {
                    guid.resize(terminator);
                }
                for (auto& ch : guid)
                {
                    if (ch >= u'A' && ch <= u'Z')
                    {
                        ch = static_cast<char16_t>(ch - u'A' + u'a');
                    }
                }

                // Only the display-adapter class is emulated; anything else reports no devices (as before).
                if (guid != u"{4d36e968-e325-11ce-bfc1-08002be10318}")
                {
                    return STATUS_SUCCESS;
                }

                // One software display adapter (Microsoft Basic Render Driver), as a double-NUL-terminated list.
                constexpr std::u16string_view instance = u"ROOT\\BasicRender\\0000";
                std::vector<char16_t> list(instance.begin(), instance.end());
                list.push_back(u'\0'); // string terminator
                list.push_back(u'\0'); // multi-sz terminator
                const auto list_bytes = static_cast<uint32_t>(list.size() * sizeof(char16_t));

                struct id_list_reply
                {
                    uint32_t reserved0 = 0;
                    int32_t status = 0;
                    uint32_t list_bytes = 0;
                    uint32_t reserved1 = 0;
                } reply{};
                reply.list_bytes = list_bytes;

                if (list_bytes <= c.output_buffer_length - sizeof(id_list_reply))
                {
                    reply.status = static_cast<int32_t>(STATUS_SUCCESS);
                    win_emu.emu().write_memory(c.output_buffer, &reply, sizeof(reply));
                    win_emu.emu().write_memory(c.output_buffer + sizeof(id_list_reply), list.data(), list_bytes);
                }
                else
                {
                    reply.status = static_cast<int32_t>(STATUS_BUFFER_TOO_SMALL);
                    win_emu.emu().write_memory(c.output_buffer, &reply, sizeof(reply));
                }

                return STATUS_SUCCESS;
            }
        };
    }

    bool needs_32_bit_devices(const windows_emulator& win_emu)
    {
        return win_emu.process.is_wow64_process;
    }

    std::unique_ptr<io_device> create_device(const std::u16string_view device, const bool is_32_bit)
    {
        if (device == u"CNG"                    //
            || device == u"RasAcd"              //
            || device == u"PcwDrv"              //
            || device == u"DeviceApi\\CMNotify" //
            || device == u"ConDrv\\Server"      //
            // \Device\MMCSS\MmThread: the Multimedia Class Scheduler. avrt!AvSetMmThreadCharacteristics opens it
            // to register dsound's now-running audio render thread for real-time priority scheduling. sogen has
            // no MMCSS, but the thread only needs the open + control IOCTLs to succeed to keep streaming.
            || device == u"MMCSS\\MmThread"     //
            || device == u"MMCSS")
        {
            return std::make_unique<dummy_device>();
        }

        if (device == u"DeviceApi\\CMApi")
        {
            return std::make_unique<configuration_manager_device>();
        }

        if (device == u"Nsi")
        {
            return create_network_store_interface();
        }

        if (device == u"Afd\\Endpoint")
        {
            return create_afd_endpoint(is_32_bit);
        }

        if (device == u"Afd\\AsyncConnectHlp")
        {
            return create_afd_async_connect_hlp(is_32_bit);
        }

        if (device == u"MountPointManager")
        {
            return create_mount_point_manager();
        }

        if (device == u"KsecDD")
        {
            return create_security_support_provider();
        }

        if (device == u"NamedPipe")
        {
            return std::make_unique<named_pipe>();
        }

        if (device == u"SogenGpu")
        {
            return create_gpu_bridge();
        }

        if (device == u"Tcp" || device == u"Tcp6" || device == u"Udp" || device == u"RawIp")
        {
            return std::make_unique<transport_stub_device>();
        }

        throw std::runtime_error("Unsupported device: " + u16_to_u8(device));
    }

    NTSTATUS io_device::execute_ioctl(windows_emulator& win_emu, const io_device_context& c)
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
                e->signaled = true;
            }
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
