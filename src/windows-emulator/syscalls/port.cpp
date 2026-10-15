#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "../port.hpp"
#include "../alpc.hpp"

namespace sogen
{

    namespace syscalls
    {
        namespace
        {
            // The kernel's SRM command port: lsass connects here and the kernel connects back to LSA.
            constexpr std::u16string_view srm_command_port_name = u"\\SeRmCommandPort";
            constexpr std::u16string_view lsa_command_port_name = u"\\SeLsaCommandPort";
            constexpr uint64_t srm_command_port_max_message_length = 0x200;
        }

        NTSTATUS handle_NtConnectPort(const syscall_context& c, const emulator_object<handle> client_port_handle,
                                      const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> server_port_name,
                                      const emulator_object<SECURITY_QUALITY_OF_SERVICE> /*security_qos*/,
                                      const emulator_object<PORT_VIEW64> client_shared_memory,
                                      const emulator_object<REMOTE_PORT_VIEW64> /*server_shared_memory*/,
                                      const emulator_object<ULONG> maximum_message_length, const emulator_pointer connection_info,
                                      const emulator_object<ULONG> connection_info_length)
        {
            auto port_name = read_unicode_string(c.emu, server_port_name);
            c.win_emu.callbacks.on_generic_access("Connecting port", port_name);

            if (port_name == srm_command_port_name)
            {
                alpc::create_kernel_listener(c.proc, port_name, srm_command_port_max_message_length, lsa_command_port_name);
            }

            if (alpc::is_guest_port_name(c.proc, port_name))
            {
                return alpc::connect_lpc_port(c, port_name, connection_info, connection_info_length.value(), maximum_message_length.value(),
                                              client_port_handle.value());
            }

            port_creation_data data{};
            if (client_shared_memory)
            {
                client_shared_memory.access([&](PORT_VIEW64& view) {
                    data.view_size = view.ViewSize;
                    data.view_base = c.win_emu.memory.allocate_memory(static_cast<size_t>(data.view_size), memory_permission::read_write);
                    view.ViewBase = data.view_base;
                    view.ViewRemoteBase = view.ViewBase;
                });
            }

            port_container container{std::u16string(port_name), c.win_emu, data};

            const auto handle = c.proc.ports.store(std::move(container));
            client_port_handle.write(handle);

            if (connection_info)
            {
                std::vector<uint8_t> zero_mem{};
                zero_mem.resize(connection_info_length.read(), 0);
                c.emu.write_memory(connection_info, zero_mem.data(), zero_mem.size());
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSecureConnectPort(const syscall_context& c, emulator_object<handle> client_port_handle,
                                            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> server_port_name,
                                            emulator_object<SECURITY_QUALITY_OF_SERVICE> security_qos,
                                            emulator_object<PORT_VIEW64> client_shared_memory, emulator_pointer /*server_sid*/,
                                            emulator_object<REMOTE_PORT_VIEW64> server_shared_memory,
                                            emulator_object<ULONG> maximum_message_length, emulator_pointer connection_info,
                                            emulator_object<ULONG> connection_info_length)
        {
            return handle_NtConnectPort(c, client_port_handle, server_port_name, security_qos, client_shared_memory, server_shared_memory,
                                        maximum_message_length, connection_info, connection_info_length);
        }

        NTSTATUS handle_NtAlpcCreatePort(const syscall_context& c, const emulator_object<handle> port_handle,
                                         const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                         const emulator_pointer port_attributes)
        {
            if (!port_handle)
            {
                return STATUS_INVALID_PARAMETER;
            }

            std::u16string port_name{};

            if (object_attributes)
            {
                const auto attributes = object_attributes.read();

                if (attributes.ObjectName)
                {
                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> name{c.emu, attributes.ObjectName};
                    port_name = std::u16string(read_unicode_string(c.emu, name));
                }
            }

            c.win_emu.callbacks.on_generic_access("Creating ALPC port", port_name);

            handle new_handle{};
            const auto status = alpc::create_port(c, std::move(port_name), port_attributes, new_handle);
            if (NT_SUCCESS(status))
            {
                port_handle.write(new_handle);
            }

            return status;
        }

        NTSTATUS handle_NtCreatePort(const syscall_context& c, const emulator_object<handle> port_handle,
                                     const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                     const ULONG /*max_connection_info_length*/, const ULONG max_message_length,
                                     const ULONG /*max_pool_usage*/)
        {
            if (!port_handle)
            {
                return STATUS_INVALID_PARAMETER;
            }

            std::u16string port_name{};

            if (object_attributes)
            {
                const auto attributes = object_attributes.read();

                if (attributes.ObjectName)
                {
                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> name{c.emu, attributes.ObjectName};
                    port_name = std::u16string(read_unicode_string(c.emu, name));
                }
            }

            c.win_emu.callbacks.on_generic_access("Creating port", port_name);

            handle new_handle{};
            const auto status = alpc::create_port(c, std::move(port_name), 0, max_message_length, new_handle);
            if (NT_SUCCESS(status))
            {
                port_handle.write(new_handle);
            }

            return status;
        }

        NTSTATUS handle_NtAlpcConnectPort(const syscall_context& c, const emulator_object<handle> port_handle,
                                          const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> server_port_name,
                                          const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                          const emulator_pointer port_attributes, const ULONG flags,
                                          const emulator_pointer /*required_server_sid*/, const emulator_pointer connection_message,
                                          const emulator_object<EmulatorTraits<Emu64>::SIZE_T> buffer_length,
                                          const emulator_pointer out_message_attributes, const emulator_pointer in_message_attributes,
                                          const emulator_object<LARGE_INTEGER> timeout)
        {
            auto port_name = read_unicode_string(c.emu, server_port_name);
            c.win_emu.callbacks.on_generic_access("Connecting port", port_name);

            if (alpc::is_guest_port_name(c.proc, port_name))
            {
                return alpc::connect_port(c, port_name, port_attributes, flags, connection_message, buffer_length.value(),
                                          out_message_attributes, in_message_attributes, timeout.value(), port_handle.value());
            }

            if (std::getenv("SOGEN_TRACE_ALPC_CONNECT"))
            {
                fprintf(stderr, "[alpc-connect-trace] pid=%u tid=%u connecting to \"%s\"\n", c.proc.process_id, c.thread().id,
                        u16_to_u8(port_name).c_str());
            }

            port_container container{std::u16string(port_name), c.win_emu, {}};

            const auto handle = c.proc.ports.store(std::move(container));
            port_handle.write(handle);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcConnectPortEx(
            const syscall_context& c, const emulator_object<handle> port_handle,
            const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> connection_port_object_attributes,
            const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*client_port_object_attributes*/,
            const emulator_pointer port_attributes, const ULONG flags, const emulator_pointer /*server_security_requirements*/,
            const emulator_pointer connection_message, const emulator_object<EmulatorTraits<Emu64>::SIZE_T> buffer_length,
            const emulator_pointer out_message_attributes, const emulator_pointer in_message_attributes,
            const emulator_object<LARGE_INTEGER> timeout)
        {
            if (!connection_port_object_attributes)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attributes = connection_port_object_attributes.read();
            if (!attributes.ObjectName)
            {
                return STATUS_INVALID_PARAMETER;
            }

            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> port_name{c.emu, attributes.ObjectName};
            return handle_NtAlpcConnectPort(c, port_handle, port_name, connection_port_object_attributes, port_attributes, flags, {},
                                            connection_message, buffer_length, out_message_attributes, in_message_attributes, timeout);
        }

        // IAudioClient::SetEventHandle (audioses!CCrossProcessBaseClientEndpoint::SetEventHandle) forwards
        // dsound's render buffer-ready event to the audio server by sending it as an ALPC HANDLE message
        // attribute with DesiredAccess = SYNCHRONIZE|EVENT_MODIFY_STATE (0x100002). sogen has no audio server
        // to receive and periodically signal it, so we capture the handle here and let the per-context-switch
        // audio-engine tick signal it (see perform_context_switch_work). Without this, dsound's event-driven
        // render thread blocks forever, never submits a WASAPI buffer, and its DirectSound play cursor stays 0.
        bool capture_audio_render_event(const syscall_context& c, const emulator_object<ALPC_MESSAGE_ATTRIBUTES>& attributes)
        {
            if (!attributes)
            {
                return false;
            }

            const auto header = attributes.read();
            if (!(header.ValidAttributes & ALPC_MESSAGE_HANDLE_ATTRIBUTE))
            {
                return false;
            }

            const bool wow64 = c.proc.is_wow64_process;
            uint64_t offset = sizeof(ALPC_MESSAGE_ATTRIBUTES);
            if (header.ValidAttributes & ALPC_MESSAGE_SECURITY_ATTRIBUTE)
            {
                offset += wow64 ? 0x0c : 0x18;
            }
            if (header.ValidAttributes & ALPC_MESSAGE_VIEW_ATTRIBUTE)
            {
                offset += wow64 ? 0x10 : 0x20;
            }
            if (header.ValidAttributes & ALPC_MESSAGE_CONTEXT_ATTRIBUTE)
            {
                offset += wow64 ? 0x14 : 0x20;
            }

            const auto attr_base = attributes.value() + offset;
            const auto handle_value = wow64 ? uint64_t{emulator_object<uint32_t>{c.emu, attr_base + 4}.read()}
                                            : emulator_object<uint64_t>{c.emu, attr_base + 8}.read();
            const auto desired_access =
                wow64 ? emulator_object<ULONG>{c.emu, attr_base + 0x0c}.read() : emulator_object<ULONG>{c.emu, attr_base + 0x14}.read();

            constexpr ULONG event_signal_access = 0x100002; // SYNCHRONIZE | EVENT_MODIFY_STATE
            if ((desired_access & event_signal_access) != event_signal_access)
            {
                return false;
            }

            const auto h = make_handle(handle_value);
            auto* event = c.proc.events.get(h);
            if (!event || event->type != SynchronizationEvent)
            {
                return false; // only the auto-reset render event SetEventHandle forwards
            }

            auto& events = c.proc.audio_render_events;
            if (std::none_of(events.begin(), events.end(), [&](const handle e) { return e.bits == h.bits; }))
            {
                events.push_back(h);
                constexpr size_t max_tracked_events = 4;
                if (events.size() > max_tracked_events)
                {
                    events.erase(events.begin(), events.end() - max_tracked_events);
                }
            }
            return true;
        }

        // SetEventHandle sends its registration to a notification ALPC port whose name the real audio server
        // hands back during stream setup; sogen never supplies one, so the client connects to an empty-named
        // port that resolves to dummy_port and replies STATUS_NOT_SUPPORTED. That makes SetEventHandle fail,
        // which fails CEngineRendererConnection::Initialize and drives dsound's create->fail->destroy churn --
        // so the event-driven render thread that would wait on the captured event is never even created. Once
        // we have captured the event, acknowledge the send with an empty successful LPC reply so the client's
        // event-driven Initialize completes and its render thread starts.
        void write_empty_success_reply(const emulator_object<PORT_MESSAGE64>& send_message,
                                       const emulator_object<PORT_MESSAGE64>& receive_message)
        {
            if (!receive_message || !send_message)
            {
                return;
            }

            auto reply = lpc_port_message::read(send_message);
            reply.native.u2.s2.Type = LPC_REPLY;
            reply.native.u1.s1.DataLength = 0;
            reply.native.u1.s1.TotalLength = static_cast<CSHORT>(reply.wire_size());
            reply.write(receive_message);
        }

        NTSTATUS handle_NtListenPort(const syscall_context& c, const handle port_handle, const emulator_pointer connection_request)
        {
            const auto* port = c.proc.ports.get(port_handle);
            if (!port || port->alpc_port_id() == 0)
            {
                return STATUS_INVALID_HANDLE;
            }

            return alpc::listen_port(c, port->alpc_port_id(), connection_request);
        }

        NTSTATUS handle_NtAcceptConnectPort(const syscall_context& c, const emulator_object<handle> server_port_handle,
                                            const handle connection_port_handle, const emulator_pointer connection_request,
                                            const BOOLEAN accept_connection, const emulator_pointer /*server_view*/,
                                            const emulator_pointer /*client_view*/)
        {
            const auto flags = c.proc.is_wow64_process ? alpc::msgflg_wow64_call : 0;
            return alpc::accept_connect_port(c, server_port_handle.value(), connection_port_handle, flags, 0, 0, connection_request,
                                             accept_connection != 0);
        }

        NTSTATUS handle_NtCompleteConnectPort(const syscall_context& c, const handle port_handle)
        {
            const auto* port = c.proc.ports.get(port_handle);
            if (!port || port->alpc_port_id() == 0)
            {
                return STATUS_INVALID_HANDLE;
            }

            return alpc::complete_connect_port(c, port->alpc_port_id());
        }

        NTSTATUS handle_NtReplyWaitReceivePort(const syscall_context& c, const handle port_handle, const emulator_pointer port_context,
                                               const emulator_object<PORT_MESSAGE64> reply_message,
                                               const emulator_object<PORT_MESSAGE64> receive_message)
        {
            const auto* port = c.proc.ports.get(port_handle);
            if (!port || port->alpc_port_id() == 0)
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto port_id = port->alpc_port_id();
            if (port_context)
            {
                const auto context = c.proc.alpc.ports.at(port_id).context;
                if (c.proc.is_wow64_process)
                {
                    c.emu.write_memory<uint32_t>(port_context, static_cast<uint32_t>(context));
                }
                else
                {
                    c.emu.write_memory<uint64_t>(port_context, context);
                }
            }

            const auto flags = c.proc.is_wow64_process ? alpc::msgflg_wow64_call : 0;
            return alpc::send_wait_receive(c, port_id, flags, reply_message.value(), 0, receive_message.value(), 0, 0, 0);
        }

        NTSTATUS handle_NtRequestWaitReplyPort(const syscall_context& c, const handle port_handle,
                                               const emulator_object<PORT_MESSAGE64> request_message,
                                               const emulator_object<PORT_MESSAGE64> reply_message)
        {
            const auto* port = c.proc.ports.get(port_handle);
            if (!port)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (port->alpc_port_id() == 0)
            {
                return STATUS_NOT_SUPPORTED;
            }

            const auto flags = alpc::msgflg_sync_request | (c.proc.is_wow64_process ? alpc::msgflg_wow64_call : 0);
            return alpc::send_wait_receive(c, port->alpc_port_id(), flags, request_message.value(), 0, reply_message.value(), 0, 0, 0);
        }

        NTSTATUS handle_NtAlpcDisconnectPort(const syscall_context& c, const handle port_handle, const ULONG /*flags*/)
        {
            if (const auto* port = c.proc.ports.get(port_handle); port && port->alpc_port_id() != 0)
            {
                alpc::close_port(c.win_emu, port->alpc_port_id());
                return STATUS_SUCCESS;
            }

            c.proc.ports.erase(port_handle);
            return STATUS_SUCCESS;
        }

        // The emulator has no per-thread impersonation context (see NtSetInformationThread's ThreadImpersonationToken
        // case), so the calling thread keeps its own identity. Like FSCTL_PIPE_IMPERSONATE, success is what a server
        // that impersonates its client to check the caller can observe.
        NTSTATUS handle_NtAlpcImpersonateClientOfPort(const syscall_context& c, const handle port_handle,
                                                      const emulator_pointer /*message*/, const ULONG /*flags*/)
        {
            if (!c.proc.ports.get(port_handle))
            {
                return STATUS_INVALID_HANDLE;
            }

            return STATUS_SUCCESS;
        }

        // Locate the HANDLE attribute inside an ALPC attribute buffer: an 8-byte {Allocated; Valid} header
        // followed by the per-attribute structs, laid out highest-bit-first for the attributes the caller
        // allocated room for. Returns nothing when the caller reserved no room for a handle attribute.
        //
        // The attribute structs preceding the HANDLE one embed pointers/SIZE_T, so their sizes differ between a
        // native 64-bit process and a 32-bit WoW64 one. Placing the attribute at 64-bit offsets for a WoW64
        // client makes rpcrt4 read the handle count/handle from the wrong place, aborting the import (observed
        // as HRESULT_FROM_WIN32 FILE_NOT_FOUND -> AUDCLNT_E_DEVICE_INVALIDATED).
        std::optional<uint64_t> find_handle_attribute(const syscall_context& c, const emulator_object<ALPC_MESSAGE_ATTRIBUTES>& attributes,
                                                      const ULONG allocated_attributes)
        {
            if (!attributes || !(allocated_attributes & ALPC_MESSAGE_HANDLE_ATTRIBUTE))
            {
                return std::nullopt;
            }

            const bool wow64 = c.proc.is_wow64_process;
            uint64_t offset = sizeof(ALPC_MESSAGE_ATTRIBUTES);
            if (allocated_attributes & ALPC_MESSAGE_SECURITY_ATTRIBUTE)
            {
                offset += wow64 ? 0x0C : 0x20;
            }
            if (allocated_attributes & ALPC_MESSAGE_VIEW_ATTRIBUTE)
            {
                offset += wow64 ? 0x10 : 0x20;
            }
            if (allocated_attributes & ALPC_MESSAGE_CONTEXT_ATTRIBUTE)
            {
                offset += wow64 ? 0x14 : 0x20;
            }

            return attributes.value() + offset;
        }

        template <typename Traits>
        emulator_object<ALPC_HANDLE_ATTR<Traits>> handle_attribute_at(const syscall_context& c, const uint64_t address)
        {
            return {c.emu, address};
        }

        // Deliver reply handles (e.g. the shared render section in an audio Initialize reply) to the receiver via
        // an ALPC HANDLE message attribute. We only emit a single attribute (the common case for NDR system
        // handles).
        void write_reply_handle_attribute(const syscall_context& c, const emulator_object<ALPC_MESSAGE_ATTRIBUTES>& attributes,
                                          const std::vector<alpc_reply_handle>& handles)
        {
            if (handles.empty())
            {
                return;
            }

            auto header = attributes.read();
            const auto attr_base = find_handle_attribute(c, attributes, header.AllocatedAttributes);
            if (!attr_base)
            {
                return;
            }

            // On a real ALPC receive the KERNEL (not the caller) fills the whole handle attribute: a non-zero
            // Flags value that marks the slot as carrying a duplicated handle, then the Handle/ObjectType/
            // GrantedAccess. A live capture of the audio CreateRemoteStream reply showed Flags=0x001243fb, so we
            // replicate it to match the real kernel's receive layout. rpcrt4 reads ObjectType as the delivered
            // HANDLE COUNT (it fetches each handle via NtAlpcQueryInformationMessage, not from the Handle field).
            constexpr ULONG alpc_received_handle_flags = 0x001243fb & ~0x00040000u; // clear ALPC_HANDLEFLG_INDIRECT
            const auto& h = handles.front();

            const auto write_attribute = [&]<typename Traits>() {
                handle_attribute_at<Traits>(c, *attr_base)
                    .write({
                        .Flags = alpc_received_handle_flags,
                        .Handle = static_cast<typename Traits::HANDLE>(h.handle),
                        .ObjectType = static_cast<ULONG>(handles.size()),
                        .DesiredAccess = h.desired_access,
                    });
            };

            if (c.proc.is_wow64_process)
            {
                write_attribute.template operator()<EmulatorTraits<Emu32>>();
            }
            else
            {
                write_attribute.template operator()<EmulatorTraits<Emu64>>();
            }

            // Report exactly the attributes the reply carries (CONTEXT|HANDLE), matching the real kernel, rather
            // than OR-ing HANDLE onto whatever stale ValidAttributes the caller's buffer happened to contain.
            header.ValidAttributes = (header.ValidAttributes & ALPC_MESSAGE_CONTEXT_ATTRIBUTE) | ALPC_MESSAGE_HANDLE_ATTRIBUTE;
            attributes.write(header);
        }

        // Extract the handle a client sent to the server via an ALPC HANDLE message attribute (e.g. the render
        // event handle in a WASAPI SetEventHandle command). Inverse of write_reply_handle_attribute.
        uint64_t read_send_handle_attribute(const syscall_context& c, const emulator_object<ALPC_MESSAGE_ATTRIBUTES>& attributes)
        {
            if (!attributes)
            {
                return 0;
            }

            const auto attr_base = find_handle_attribute(c, attributes, attributes.read().AllocatedAttributes);
            if (!attr_base)
            {
                return 0;
            }

            return c.proc.is_wow64_process ? handle_attribute_at<EmulatorTraits<Emu32>>(c, *attr_base).read().Handle
                                           : handle_attribute_at<EmulatorTraits<Emu64>>(c, *attr_base).read().Handle;
        }

        NTSTATUS handle_NtAlpcSendWaitReceivePort(const syscall_context& c, const handle port_handle, const ULONG flags,
                                                  const emulator_object<PORT_MESSAGE64> send_message,
                                                  const emulator_object<ALPC_MESSAGE_ATTRIBUTES> send_message_attributes,
                                                  const emulator_object<PORT_MESSAGE64> receive_message,
                                                  const emulator_object<EmulatorTraits<Emu64>::SIZE_T> buffer_length,
                                                  const emulator_object<ALPC_MESSAGE_ATTRIBUTES> receive_message_attributes,
                                                  const emulator_object<LARGE_INTEGER> timeout)
        {
            auto* port = c.proc.ports.get(port_handle);
            if (!port)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (port->alpc_port_id() != 0)
            {
                return alpc::send_wait_receive(c, port->alpc_port_id(), flags, send_message.value(), send_message_attributes.value(),
                                               receive_message.value(), buffer_length.value(), receive_message_attributes.value(),
                                               timeout.value());
            }

            if (capture_audio_render_event(c, send_message_attributes))
            {
                write_empty_success_reply(send_message, receive_message);
                if (buffer_length && receive_message)
                {
                    buffer_length.write(
                        static_cast<typename EmulatorTraits<Emu64>::SIZE_T>(lpc_port_message::read(send_message).wire_size()));
                }
                return STATUS_SUCCESS;
            }

            lpc_message_context context{c.emu};
            context.send_message = send_message;
            context.receive_message = receive_message;
            context.receive_buffer_length = buffer_length ? buffer_length.read() : 0;
            context.send_handle = read_send_handle_attribute(c, send_message_attributes);

            const auto result = port->handle_message(c.win_emu, context);

            if (receive_message && NT_SUCCESS(result.status))
            {
                if (context.receive_buffer_length < result.total_length())
                {
                    // The port handler should always queue messages that are too large to be written,
                    // so if we still got a message that doesn't fit in the receive buffer, something
                    // has gone wrong.
                    throw std::runtime_error("Unexpected success result returned by port handler");
                }

                result.message.write(receive_message);

                if (!result.payload.empty())
                {
                    c.emu.write_memory(receive_message.value() + result.message.header_size(), result.payload.data(),
                                       result.payload.size());
                }

                write_reply_handle_attribute(c, receive_message_attributes, result.handles);

                // Stash the delivered handles so rpcrt4 can pull them back via
                // NtAlpcQueryInformationMessage(AlpcMessageHandleInformation), which is how its system-handle
                // unmarshal actually imports them (it does not read the handle attribute's Handle field).
                if (!result.handles.empty())
                {
                    c.proc.pending_alpc_message_handles = result.handles;
                }
            }

            if (receive_message && buffer_length)
            {
                buffer_length.write(static_cast<typename EmulatorTraits<Emu64>::SIZE_T>(result.total_length()));
            }

            return result.status;
        }

        NTSTATUS handle_NtAlpcQueryInformation(const syscall_context& c, const handle port_handle, const uint32_t information_class,
                                               const emulator_pointer information, const uint32_t length,
                                               const emulator_pointer return_length)
        {
            const auto* port = c.proc.ports.get(port_handle);
            if (!port)
            {
                return STATUS_NOT_SUPPORTED;
            }

            if (port->alpc_port_id() == 0)
            {
                return alpc::query_emulated_port_information(c, information_class, information, length, return_length);
            }

            return alpc::query_information(c, port->alpc_port_id(), information_class, information, length, return_length);
        }

        NTSTATUS handle_NtAlpcCreatePortSection(const syscall_context& c, const handle /*port_handle*/, const ULONG /*flags*/,
                                                const handle /*section_handle*/, const EmulatorTraits<Emu64>::SIZE_T section_size,
                                                const emulator_pointer alpc_section_handle, const emulator_pointer actual_section_size)
        {
            return alpc::create_port_section(c, section_size, alpc_section_handle, actual_section_size);
        }

        NTSTATUS handle_NtAlpcDeletePortSection(const syscall_context& c, const handle /*port_handle*/, const ULONG /*flags*/,
                                                const uint64_t section_handle)
        {
            return alpc::delete_port_section(c, section_handle);
        }

        NTSTATUS handle_NtAlpcCreateSectionView(const syscall_context& c, const handle /*port_handle*/, const ULONG /*flags*/,
                                                const emulator_pointer view_attributes)
        {
            return alpc::create_section_view(c, view_attributes);
        }

        NTSTATUS handle_NtAlpcDeleteSectionView(const syscall_context& c, const handle /*port_handle*/, const ULONG /*flags*/,
                                                const emulator_pointer view_base)
        {
            return alpc::delete_section_view(c, view_base);
        }

        NTSTATUS handle_NtAlpcAcceptConnectPort(const syscall_context& c, const emulator_object<handle> port_handle,
                                                const handle connection_port_handle, const ULONG flags,
                                                const emulator_pointer /*object_attributes*/, const emulator_pointer port_attributes,
                                                const emulator_pointer port_context, const emulator_pointer connection_request,
                                                const emulator_pointer /*connection_message_attributes*/, const BOOLEAN accept_connection)
        {
            return alpc::accept_connect_port(c, port_handle.value(), connection_port_handle, flags, port_attributes, port_context,
                                             connection_request, accept_connection != 0);
        }

        // rpcrt4's client-side system-handle unmarshal imports a handle delivered with an ALPC reply by
        // calling NtAlpcQueryInformationMessage(AlpcMessageHandleInformation = 3, index). The output is an
        // ALPC_MESSAGE_HANDLE_INFORMATION {ULONG Index; ULONG Reserved; ULONG Handle; ULONG ObjectType;
        // ULONG GrantedAccess;} (0x14 bytes); rpcrt4 reads Handle@+8, ObjectType@+0xc, GrantedAccess@+0x10.
        // We return the handle stashed by the matching NtAlpcSendWaitReceivePort reply.
        NTSTATUS handle_NtAlpcQueryInformationMessage(const syscall_context& c, const handle /*port_handle*/,
                                                      const emulator_object<PORT_MESSAGE64> /*port_message*/,
                                                      const uint32_t message_information_class, const emulator_pointer message_information,
                                                      const uint32_t length, const emulator_object<ULONG> return_length)
        {
            constexpr uint32_t alpc_message_handle_information = 3;
            if (message_information_class != alpc_message_handle_information)
            {
                return STATUS_NOT_SUPPORTED;
            }

            constexpr uint32_t info_size = 0x14;
            if (!message_information || length < info_size)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            // The caller passes the requested handle index in the first dword of the buffer.
            const auto index = c.emu.read_memory<uint32_t>(message_information);
            const auto& handles = c.proc.pending_alpc_message_handles;
            if (index >= handles.size())
            {
                return STATUS_NO_MORE_ENTRIES;
            }

            const auto& h = handles[index];
            emulator_object<uint32_t>{c.emu, message_information + 0x00}.write(index);
            emulator_object<uint32_t>{c.emu, message_information + 0x04}.write(0);
            emulator_object<uint32_t>{c.emu, message_information + 0x08}.write(static_cast<uint32_t>(h.handle));
            emulator_object<uint32_t>{c.emu, message_information + 0x0c}.write(h.object_type);
            emulator_object<uint32_t>{c.emu, message_information + 0x10}.write(h.desired_access);

            return_length.write_if_valid(info_size);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcSetInformation(const syscall_context& c, const handle port_handle, const uint32_t information_class,
                                             const emulator_pointer information, const uint32_t length)
        {
            const auto* port = c.proc.ports.get(port_handle);
            if (!port)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (port->alpc_port_id() == 0)
            {
                return STATUS_SUCCESS;
            }

            return alpc::set_information(c, port->alpc_port_id(), information_class, information, length);
        }

        NTSTATUS handle_NtAlpcCreateSecurityContext()
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtAlpcDeleteSecurityContext()
        {
            return STATUS_NOT_SUPPORTED;
        }
    }

} // namespace sogen
