#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../io_completion_wait.hpp"
#include "../syscall_utils.hpp"
#include "../cross_process.hpp"
#include "wait_trace.hpp"

#include <utils/string.hpp>

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtSetEvent(const syscall_context& c, uint64_t handle, emulator_object<LONG> previous_state);
        NTSTATUS handle_NtReleaseMutant(const syscall_context& c, handle mutant_handle, emulator_object<LONG> previous_count);
        NTSTATUS handle_NtReleaseSemaphore(const syscall_context& c, handle semaphore_handle, ULONG release_count,
                                           emulator_object<LONG> previous_count);

        namespace
        {
            constexpr ACCESS_MASK PROCESS_DUP_HANDLE = 0x0040;

            // TargetProcess::Init hands the sandbox's shared pagefile-backed IPC section to a
            // suspended child via DuplicateHandle(cur, shared_section_, child, &out,
            // FILE_MAP_READ|FILE_MAP_WRITE|SECTION_QUERY, false, 0). On real Windows this is genuine
            // shared memory: the broker's SharedMemIPCServer and the child's CrossCall client poll the
            // same physical pages. Two separate host address spaces here cannot share a guest section,
            // so this reconstructs an equivalent section in the child from a content snapshot instead
            // (adopt_section, process_control_server.cpp) - the section is content-copied, not truly
            // shared. A real sandbox cross-call issued through it after this point would therefore
            // never reach the broker. That is plausibly survivable for this one scenario only because
            // sogen does not enforce the sandbox's restricted token: the child's intercepted syscalls
            // succeed directly here and never fall through to the broker's cross-call path in the first
            // place - a claim that must be verified empirically against the real repro (Task 9), not
            // assumed.
            NTSTATUS duplicate_section_into_child(const syscall_context& c, const handle source_handle, const handle target_process_handle,
                                                  const emulator_object<handle> target_handle, const ACCESS_MASK desired_access,
                                                  const ULONG options)
            {
                const auto resolved_source_handle = c.proc.resolve_object_pseudo_handle(source_handle, c.vcpu.active_thread);
                if (resolved_source_handle.value.type != handle_types::section)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                auto* const source_section = c.proc.sections.get(resolved_source_handle);
                if (!source_section || source_section->object->is_image() || !source_section->object->file_name.empty())
                {
                    return STATUS_NOT_SUPPORTED;
                }

                const auto child = resolve_child_target(c, target_process_handle, PROCESS_DUP_HANDLE);
                if (std::holds_alternative<NTSTATUS>(child))
                {
                    return std::get<NTSTATUS>(child);
                }

                const bool same_access = (options & DUPLICATE_SAME_ACCESS) != 0;
                if (!same_access && (desired_access & ~source_section->granted_access) != 0)
                {
                    return STATUS_ACCESS_DENIED;
                }

                const auto& target = std::get<child_target>(child);

                process_control_request request{};
                request.op = process_control_op::adopt_section;
                request.maximum_size = source_section->object->maximum_size;
                request.page_protection = source_section->object->section_page_protection;
                request.allocation_attributes = source_section->object->allocation_attributes;
                request.granted_access = same_access ? source_section->granted_access : desired_access;
                request.payload = source_section->object->backing_storage;

                const auto response = send_process_control_request(c, target, request);
                if (!response)
                {
                    c.win_emu.log.error("NtDuplicateObject: control channel to child %u is dead/unresponsive\n", target.record_id);
                    return STATUS_PROCESS_IS_TERMINATING;
                }

                if (response->status != STATUS_SUCCESS)
                {
                    return static_cast<NTSTATUS>(response->status);
                }

                target_handle.write(make_handle(response->minted_handle_bits));
                return STATUS_SUCCESS;
            }

            // Mirrors duplicate_section_into_child for SharedMemIPCServer::Init's ping/pong event pair
            // (sandbox/win/src/sharedmem_ipc_server.cc), duplicated into the child right after the
            // shared IPC section - see that function's comment for why minting an unshared local event
            // (adopt_event, process_control_server.cpp) is survivable here rather than a real gap.
            NTSTATUS duplicate_event_into_child(const syscall_context& c, const handle source_handle, const handle target_process_handle,
                                                const emulator_object<handle> target_handle, const ACCESS_MASK /*desired_access*/,
                                                const ULONG /*options*/)
            {
                const auto resolved_source_handle = c.proc.resolve_object_pseudo_handle(source_handle, c.vcpu.active_thread);
                auto* const source_event = c.proc.events.get(resolved_source_handle);
                if (!source_event)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                const auto child = resolve_child_target(c, target_process_handle, PROCESS_DUP_HANDLE);
                if (std::holds_alternative<NTSTATUS>(child))
                {
                    return std::get<NTSTATUS>(child);
                }

                const auto& target = std::get<child_target>(child);

                process_control_request request{};
                request.op = process_control_op::adopt_event;
                request.allocation_type = static_cast<uint32_t>(source_event->type);
                request.page_protection = source_event->signaled ? 1 : 0;

                const auto response = send_process_control_request(c, target, request);
                if (!response)
                {
                    c.win_emu.log.error("NtDuplicateObject: control channel to child %u is dead/unresponsive\n", target.record_id);
                    return STATUS_PROCESS_IS_TERMINATING;
                }

                if (response->status != STATUS_SUCCESS)
                {
                    return static_cast<NTSTATUS>(response->status);
                }

                target_handle.write(make_handle(response->minted_handle_bits));
                return STATUS_SUCCESS;
            }

            // Mirrors duplicate_event_into_child for SharedMemIPCServer::Init's g_alive_mutex
            // (sandbox/win/src/sharedmem_ipc_server.cc), duplicated into the child once after all the
            // ping/pong events - see that function's comment for why an unshared local mutant
            // (adopt_mutant, process_control_server.cpp) is survivable here rather than a real gap.
            NTSTATUS duplicate_mutant_into_child(const syscall_context& c, const handle source_handle, const handle target_process_handle,
                                                 const emulator_object<handle> target_handle, const ACCESS_MASK /*desired_access*/,
                                                 const ULONG /*options*/)
            {
                const auto resolved_source_handle = c.proc.resolve_object_pseudo_handle(source_handle, c.vcpu.active_thread);
                auto* const source_mutant = c.proc.mutants.get(resolved_source_handle);
                if (!source_mutant)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                const auto child = resolve_child_target(c, target_process_handle, PROCESS_DUP_HANDLE);
                if (std::holds_alternative<NTSTATUS>(child))
                {
                    return std::get<NTSTATUS>(child);
                }

                const auto& target = std::get<child_target>(child);

                process_control_request request{};
                request.op = process_control_op::adopt_mutant;
                request.allocation_type = source_mutant->locked_count;
                request.size = source_mutant->owning_thread_id;
                request.page_protection = source_mutant->abandoned ? 1 : 0;

                const auto response = send_process_control_request(c, target, request);
                if (!response)
                {
                    c.win_emu.log.error("NtDuplicateObject: control channel to child %u is dead/unresponsive\n", target.record_id);
                    return STATUS_PROCESS_IS_TERMINATING;
                }

                if (response->status != STATUS_SUCCESS)
                {
                    return static_cast<NTSTATUS>(response->status);
                }

                target_handle.write(make_handle(response->minted_handle_bits));
                return STATUS_SUCCESS;
            }

            // The reverse of duplicate_section_into_child/duplicate_event_into_child/
            // duplicate_mutant_into_child above: the guest is pulling a handle the CHILD owns back
            // into itself (e.g. a mojo/sandbox broker receiving a section its child created via
            // DuplicateHandle(child_handle, h, GetCurrentProcess(), &out, ...)). export_handle asks
            // the child to describe the object; the new local handle is minted here, in the current
            // process's own handle store, matching real DuplicateHandle's guarantee that the new
            // handle always lands in whatever process target_process_handle names.
            NTSTATUS duplicate_object_from_child(const syscall_context& c, const handle source_process_handle, const handle source_handle,
                                                 const emulator_object<handle> target_handle, const ACCESS_MASK desired_access,
                                                 const ULONG options)
            {
                const auto child = resolve_child_target(c, source_process_handle, PROCESS_DUP_HANDLE);
                if (std::holds_alternative<NTSTATUS>(child))
                {
                    return std::get<NTSTATUS>(child);
                }

                const auto& target = std::get<child_target>(child);

                process_control_request request{};
                request.op = process_control_op::export_handle;
                request.address = source_handle.bits;

                const auto response = send_process_control_request(c, target, request);
                if (!response)
                {
                    c.win_emu.log.error("NtDuplicateObject: control channel to child %u is dead/unresponsive\n", target.record_id);
                    return STATUS_PROCESS_IS_TERMINATING;
                }

                if (response->status != STATUS_SUCCESS)
                {
                    return static_cast<NTSTATUS>(response->status);
                }

                const bool same_access = (options & DUPLICATE_SAME_ACCESS) != 0;

                if (response->exported_object_type == handle_types::section)
                {
                    if (!same_access && (desired_access & ~response->granted_access) != 0)
                    {
                        return STATUS_ACCESS_DENIED;
                    }

                    auto s =
                        section::from_pagefile_backing(response->maximum_size, response->page_protection, response->allocation_attributes,
                                                       same_access ? response->granted_access : desired_access, response->payload);

                    target_handle.write(c.proc.sections.store(std::move(s)));
                    return STATUS_SUCCESS;
                }

                if (response->exported_object_type == handle_types::event)
                {
                    event e{};
                    e.type = static_cast<EVENT_TYPE>(response->allocation_type);
                    e.signaled = response->page_protection != 0;

                    target_handle.write(c.proc.events.store(std::move(e)));
                    return STATUS_SUCCESS;
                }

                if (response->exported_object_type == handle_types::mutant)
                {
                    mutant m{};
                    m.locked_count = response->allocation_type;
                    m.owning_thread_id = static_cast<uint32_t>(response->size);
                    m.abandoned = response->page_protection != 0;

                    target_handle.write(c.proc.mutants.store(std::move(m)));
                    return STATUS_SUCCESS;
                }

                return STATUS_NOT_SUPPORTED;
            }
        }

        NTSTATUS handle_NtClose(const syscall_context& c, const handle h)
        {
            const auto value = h.value;

            if (h.h == 0xDEADC0DE || h.h == 0xDEADBEEF)
            {
                c.win_emu.callbacks.on_suspicious_activity("Anti-debug check with invalid handle");

                return STATUS_INVALID_HANDLE;
            }

            if (value.is_pseudo)
            {
                return STATUS_SUCCESS;
            }

            if (value.type == handle_types::wait_completion_packet)
            {
                auto* wait_packet = c.proc.wait_completion_packets.get(h);
                if (wait_packet && wait_packet->ref_count == 1)
                {
                    io_completion_wait::cleanup_wait_packet_on_close(c.proc, h);
                }
            }

            if (value.type == handle_types::worker_factory)
            {
                auto* factory = c.proc.worker_factories.get(h);
                if (factory && factory->ref_count == 1)
                {
                    io_completion_wait::release_handle_reference(c.proc, factory->io_completion_handle);
                }
            }

            if (value.type == handle_types::job)
            {
                auto* job = c.proc.jobs.get(h);

                // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE (see handle_NtSetInformationJobObject) is a real
                // NT behaviour that fires on the last handle closing, not on any explicit terminate
                // call - a sandbox broker relying on it to reap a child it's done with never issues
                // NtTerminateProcess against that child at all.
                if (job && job->ref_count == 1 && job->kill_on_close)
                {
                    static const bool trace_job_kill = std::getenv("SOGEN_TRACE_JOB_KILL") != nullptr;
                    if (trace_job_kill)
                    {
                        c.win_emu.log.error("[job-kill-trace] last handle closed on kill-on-close job, assigned_children=%zu\n",
                                            job->assigned_child_record_ids.size());
                    }

                    for (const auto record_id : job->assigned_child_record_ids)
                    {
                        const auto child_it = c.proc.child_processes.find(record_id);
                        if (child_it == c.proc.child_processes.end() || child_it->second.exit_status != STATUS_PENDING)
                        {
                            continue;
                        }

                        if (trace_job_kill)
                        {
                            c.win_emu.log.error("[job-kill-trace] force-killing child record_id=%u\n", record_id);
                        }

                        auto* channel = c.win_emu.find_child_control_channel(record_id);
                        if (channel)
                        {
                            channel->force_kill();
                        }

                        child_it->second.exit_status = STATUS_SUCCESS;
                        c.win_emu.drop_child_control_channel(record_id);
                    }
                }
            }

            if (value.type == handle_types::file)
            {
                auto* file = c.proc.files.get(h);
                if (file && file->ref_count == 1)
                {
                    for (auto it = c.proc.file_locks.begin(); it != c.proc.file_locks.end();)
                    {
                        auto& locks = it->second.locks;
                        std::erase_if(locks, [&](const file_lock_range& lock) { return lock.owner == h; });

                        if (locks.empty())
                        {
                            it = c.proc.file_locks.erase(it);
                            continue;
                        }

                        ++it;
                    }
                }
            }

            auto* handle_store = c.proc.get_handle_store(h);
            if (handle_store && handle_store->erase(h))
            {
                return STATUS_SUCCESS;
            }

            return STATUS_INVALID_HANDLE;
        }

        NTSTATUS handle_NtDuplicateObject(const syscall_context& c, const handle source_process_handle, const handle source_handle,
                                          const handle target_process_handle, const emulator_object<handle> target_handle,
                                          const ACCESS_MASK desired_access, const ULONG /*handle_attributes*/, const ULONG options)
        {
            const bool source_is_current = c.proc.is_current_process_handle(source_process_handle);
            const bool target_is_current = c.proc.is_current_process_handle(target_process_handle);

            if (!source_is_current)
            {
                if (!target_is_current)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                return duplicate_object_from_child(c, source_process_handle, source_handle, target_handle, desired_access, options);
            }

            if (!target_is_current)
            {
                const auto resolved_for_child = c.proc.resolve_object_pseudo_handle(source_handle, c.vcpu.active_thread);

                if (std::getenv("SOGEN_TRACE_DUPLICATE_OBJECT_INTO_CHILD"))
                {
                    c.win_emu.log.info("[duplicate-object-into-child-trace] pid=%u source_handle=0x%llx resolved_type=%u\n",
                                       c.proc.process_id, static_cast<unsigned long long>(source_handle.bits),
                                       resolved_for_child.value.type);
                }

                if (resolved_for_child.value.type == handle_types::event)
                {
                    return duplicate_event_into_child(c, source_handle, target_process_handle, target_handle, desired_access, options);
                }
                if (resolved_for_child.value.type == handle_types::mutant)
                {
                    return duplicate_mutant_into_child(c, source_handle, target_process_handle, target_handle, desired_access, options);
                }

                return duplicate_section_into_child(c, source_handle, target_process_handle, target_handle, desired_access, options);
            }

            const auto resolved_source_handle = c.proc.resolve_object_pseudo_handle(source_handle, c.vcpu.active_thread);

            if (resolved_source_handle.value.is_pseudo)
            {
                target_handle.write(resolved_source_handle);
                return STATUS_SUCCESS;
            }

            const bool same_access = (options & DUPLICATE_SAME_ACCESS) != 0;

            if (!same_access && resolved_source_handle.value.type == handle_types::section)
            {
                const auto* section = c.proc.sections.get(resolved_source_handle);
                if (section && (desired_access & ~section->granted_access) != 0)
                {
                    return STATUS_ACCESS_DENIED;
                }
            }

            auto* store = c.proc.get_handle_store(resolved_source_handle);
            if (!store)
            {
                return STATUS_NOT_SUPPORTED;
            }

            const auto new_handle =
                store->duplicate(resolved_source_handle, same_access ? std::nullopt : std::optional<ACCESS_MASK>{desired_access});
            if (!new_handle)
            {
                return STATUS_INVALID_HANDLE;
            }

            target_handle.write(*new_handle);
            return STATUS_SUCCESS;
        }

        std::u16string get_type_name(const handle_types::type type)
        {
            switch (type)
            {
            case handle_types::file:
                return u"File";
            case handle_types::device:
                return u"Device";
            case handle_types::event:
                return u"Event";
            case handle_types::section:
                return u"Section";
            case handle_types::symlink:
                return u"Symlink";
            case handle_types::directory:
                return u"Directory";
            case handle_types::semaphore:
                return u"Semaphore";
            case handle_types::port:
                return u"Port";
            case handle_types::thread:
                return u"Thread";
            case handle_types::registry:
                return u"Registry";
            case handle_types::mutant:
                return u"Mutant";
            case handle_types::token:
                return u"Token";
            case handle_types::window:
                return u"Window";
            case handle_types::timer:
                return u"Timer";
            case handle_types::desktop:
                return u"Desktop";
            case handle_types::window_station:
                return u"WindowStation";
            case handle_types::io_completion:
                return u"IoCompletion";
            case handle_types::wait_completion_packet:
                return u"WaitCompletionPacket";
            case handle_types::worker_factory:
                return u"TpWorkerFactory";
            case handle_types::private_namespace:
                return u"Directory";
            case handle_types::process:
                return u"Process";
            case handle_types::job:
                return u"Job";
            default:
                return u"";
            }
        }

        NTSTATUS handle_NtQueryObject(const syscall_context& c, const handle handle,
                                      const OBJECT_INFORMATION_CLASS object_information_class, const emulator_pointer object_information,
                                      const ULONG object_information_length, const emulator_object<ULONG> return_length)
        {
            const auto effective_handle = c.proc.resolve_object_pseudo_handle(handle, c.vcpu.active_thread);

            if (object_information_class == ObjectNameInformation)
            {
                std::u16string device_path;
                switch (effective_handle.value.type)
                {
                case handle_types::reserved: {
                    return STATUS_NOT_SUPPORTED;
                }

                case handle_types::file: {
                    const auto* file = c.proc.files.get(effective_handle);
                    if (!file)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    device_path = windows_path(file->name).to_device_path();
                    break;
                }
                case handle_types::device: {
                    const auto* device = c.proc.devices.get(effective_handle);
                    if (!device)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    device_path = device->get_device_path();
                    break;
                }
                case handle_types::directory: {
                    // Directory handles are pseudo handles representing specific object directories
                    if (effective_handle == KNOWN_DLLS_DIRECTORY)
                    {
                        device_path = u"\\KnownDlls";
                    }
                    else if (effective_handle == KNOWN_DLLS32_DIRECTORY)
                    {
                        device_path = u"\\KnownDlls32";
                    }
                    else if (effective_handle == BASE_NAMED_OBJECTS_DIRECTORY)
                    {
                        device_path = u"\\Sessions\\1\\BaseNamedObjects";
                    }
                    else if (effective_handle == RPC_CONTROL_DIRECTORY)
                    {
                        device_path = u"\\RPC Control";
                    }
                    else
                    {
                        // Unknown directory handle
                        return STATUS_INVALID_HANDLE;
                    }
                    break;
                }
                case handle_types::registry: {
                    const auto* registry = c.proc.registry_keys.get(effective_handle);
                    if (!registry)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    // Build the full registry path in device format
                    auto registry_path = (registry->hive.get() / registry->path.get()).u16string();

                    // Convert backslashes to forward slashes for consistency
                    std::ranges::replace(registry_path, u'/', u'\\');

                    // Convert to uppercase as Windows registry paths are case-insensitive
                    std::ranges::transform(registry_path, registry_path.begin(), std::towupper);

                    device_path = registry_path;
                    break;
                }
                case handle_types::desktop: {
                    const auto* desk = c.proc.desktops.get(effective_handle);
                    if (!desk)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    device_path = u"\\Windows\\Desktop\\";
                    device_path.append(desk->name);
                    break;
                }
                case handle_types::io_completion: {
                    const auto* io = c.proc.io_completions.get(effective_handle);
                    if (!io)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    device_path = io->name;
                    break;
                }
                case handle_types::wait_completion_packet: {
                    const auto* packet = c.proc.wait_completion_packets.get(effective_handle);
                    if (!packet)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    device_path = packet->name;
                    break;
                }
                case handle_types::worker_factory: {
                    const auto* factory = c.proc.worker_factories.get(effective_handle);
                    if (!factory)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    device_path = factory->name;
                    break;
                }
                case handle_types::private_namespace: {
                    const auto* ns = c.proc.private_namespaces.get(effective_handle);
                    if (!ns)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    break;
                }
                case handle_types::process: {
                    if (effective_handle != GUEST_PROCESS_HANDLE)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    break;
                }
                case handle_types::thread: {
                    const auto* thread = c.proc.threads.get(effective_handle);
                    if (!thread)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    break;
                }
                default:
                    c.win_emu.log.error("Unsupported handle type for name information query: %X\n", effective_handle.value.type);
                    c.emu.stop();
                    return STATUS_NOT_SUPPORTED;
                }

                const auto required_size =
                    sizeof(UNICODE_STRING<EmulatorTraits<Emu64>>) + ((device_path.size() + (device_path.empty() ? 0 : 1)) * 2);
                return_length.write_if_valid(static_cast<ULONG>(required_size));

                if (required_size > object_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                if (device_path.empty())
                {
                    UNICODE_STRING<EmulatorTraits<Emu64>> zero_buf{};
                    c.emu.write_memory(object_information, zero_buf);
                }
                else
                {
                    emulator_allocator allocator(c.emu, object_information, object_information_length);
                    allocator.make_unicode_string(device_path);
                }

                return STATUS_SUCCESS;
            }

            if (object_information_class == ObjectTypeInformation)
            {
                const auto name = get_type_name(static_cast<handle_types::type>(effective_handle.value.type));

                const auto required_size = sizeof(OBJECT_TYPE_INFORMATION) + (name.size() + 1) * 2;
                return_length.write_if_valid(static_cast<ULONG>(required_size));

                if (required_size > object_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_allocator allocator(c.emu, object_information, object_information_length);
                const auto info = allocator.reserve<OBJECT_TYPE_INFORMATION>();
                info.access([&](OBJECT_TYPE_INFORMATION& i) {
                    allocator.make_unicode_string(i.TypeName, name); //
                });

                return STATUS_SUCCESS;
            }

            if (object_information_class == ObjectTypesInformation)
            {
                const auto name = get_type_name(static_cast<handle_types::type>(effective_handle.value.type));
                constexpr auto type_start_offset = align_up(sizeof(OBJECT_TYPES_INFORMATION), sizeof(uint64_t));

                const auto required_size = type_start_offset + sizeof(OBJECT_TYPE_INFORMATION) + (name.size() + 1) * 2;
                return_length.write_if_valid(static_cast<ULONG>(required_size));

                if (required_size > object_information_length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }

                emulator_allocator allocator(c.emu, object_information, object_information_length);
                const auto types_info = allocator.reserve<OBJECT_TYPES_INFORMATION>();
                types_info.access([&](OBJECT_TYPES_INFORMATION& i) {
                    i.NumberOfTypes = 1; //
                });

                allocator.skip_until(type_start_offset);

                const auto info = allocator.reserve<OBJECT_TYPE_INFORMATION>();
                info.access([&](OBJECT_TYPE_INFORMATION& i) {
                    allocator.make_unicode_string(i.TypeName, name); //
                });

                return STATUS_SUCCESS;
            }

            if (object_information_class == ObjectBasicInformation)
            {
                return handle_query<OBJECT_BASIC_INFORMATION>(c.emu, object_information, object_information_length, return_length,
                                                              [&](OBJECT_BASIC_INFORMATION& info) {
                                                                  info.GrantedAccess = GENERIC_ALL;
                                                                  info.HandleCount = 1;
                                                                  info.PointerCount = 2;
                                                              });
            }

            if (object_information_class == ObjectHandleFlagInformation)
            {
                return handle_query<OBJECT_HANDLE_FLAG_INFORMATION>(c.emu, object_information, object_information_length, return_length,
                                                                    [&](OBJECT_HANDLE_FLAG_INFORMATION& info) {
                                                                        info.Inherit = 0;
                                                                        info.ProtectFromClose = 0;
                                                                    });
            }

            c.win_emu.log.error("Unsupported object info class: %X\n", object_information_class);
            c.emu.stop();
            return STATUS_NOT_SUPPORTED;
        }

        template <typename Store>
        void collect_wait32_candidate(Store& store, const uint32_t id, std::optional<handle>& resolved, uint32_t& candidate_count)
        {
            if (!store.get_by_index(id))
            {
                return;
            }

            ++candidate_count;
            if (!resolved)
            {
                resolved = store.make_handle(id);
            }
        }

        std::optional<handle> resolve_wait32_handle(const syscall_context& c, const uint32_t raw_handle)
        {
            const auto decoded = make_handle(static_cast<uint64_t>(raw_handle));
            if (decoded.value.type != handle_types::reserved)
            {
                return decoded;
            }

            // wait32 can give raw 32 bit handles without type bits
            const auto id = static_cast<uint32_t>(decoded.value.id);
            if (id == 0)
            {
                return std::nullopt;
            }

            std::optional<handle> resolved{};
            uint32_t candidate_count = 0;

            collect_wait32_candidate(c.proc.events, id, resolved, candidate_count);
            collect_wait32_candidate(c.proc.threads, id, resolved, candidate_count);
            collect_wait32_candidate(c.proc.mutants, id, resolved, candidate_count);
            collect_wait32_candidate(c.proc.semaphores, id, resolved, candidate_count);
            collect_wait32_candidate(c.proc.ports, id, resolved, candidate_count);
            collect_wait32_candidate(c.proc.io_completions, id, resolved, candidate_count);
            collect_wait32_candidate(c.proc.timers, id, resolved, candidate_count);

            if (candidate_count == 1)
            {
                return resolved;
            }

            return std::nullopt;
        }

        // Resolve a handle for a wait operation: expand object pseudo handles, and recover the real typed
        // handle for raw/un-typed (reserved) handles by matching their id against the waitable stores -- the
        // same recovery the wait32 path does. WoW64 and some callers hand us handles without type bits.
        handle resolve_wait_handle(const syscall_context& c, const handle h)
        {
            const auto resolved = c.proc.resolve_object_pseudo_handle(h, c.vcpu.active_thread);
            if (resolved.value.type != handle_types::reserved || resolved.value.is_pseudo)
            {
                return resolved;
            }

            if (const auto recovered = resolve_wait32_handle(c, static_cast<uint32_t>(resolved.bits)))
            {
                return *recovered;
            }

            return resolved;
        }

        NTSTATUS validate_wait_handle(const syscall_context& c, const handle h)
        {
            const auto validate_handle_in_store = [&](auto& store) -> NTSTATUS {
                return store.get(h) ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
            };

            switch (h.value.type)
            {
            case handle_types::process: {
                // The synthetic Steam process never signals, so a liveness wait times out ("alive").
                if (h == GUEST_PROCESS_HANDLE || h == STEAM_PROCESS_HANDLE)
                {
                    return STATUS_SUCCESS;
                }

                const auto child = resolve_child_record(c, h, SYNCHRONIZE);
                return std::holds_alternative<NTSTATUS>(child) ? std::get<NTSTATUS>(child) : STATUS_SUCCESS;
            }

            case handle_types::file:
                if (h.value.is_pseudo)
                {
                    return STATUS_SUCCESS;
                }

                return validate_handle_in_store(c.proc.files);

            case handle_types::event:
                if (h.value.is_pseudo)
                {
                    return STATUS_SUCCESS;
                }

                return validate_handle_in_store(c.proc.events);

            case handle_types::thread:
                return validate_handle_in_store(c.proc.threads);

            case handle_types::mutant:
                return validate_handle_in_store(c.proc.mutants);

            case handle_types::semaphore:
                return validate_handle_in_store(c.proc.semaphores);

            case handle_types::port:
                return validate_handle_in_store(c.proc.ports);

            case handle_types::io_completion:
                return validate_handle_in_store(c.proc.io_completions);

            case handle_types::timer:
                if (h.value.is_pseudo)
                {
                    return STATUS_SUCCESS;
                }

                return validate_handle_in_store(c.proc.timers);

            case handle_types::reserved:
                // A null or un-typed handle that resolve_wait_handle could not map to a waitable object.
                // Windows returns STATUS_INVALID_HANDLE for this (e.g. waiting on a null handle, which the
                // game does every frame and simply ignores) -- it is not an unsupported object type.
                return STATUS_INVALID_HANDLE;

            default:
                c.win_emu.log.error("Wait handle type not supported: %u\n", static_cast<uint32_t>(h.value.type));
                return STATUS_OBJECT_TYPE_MISMATCH;
            }
        }

        void trace_process_handle_wait(const syscall_context& c, const handle resolved, const emulator_object<LARGE_INTEGER> timeout)
        {
            if (resolved.value.type != handle_types::process)
            {
                return;
            }

            std::string detail{};

            if (resolved == GUEST_PROCESS_HANDLE)
            {
                detail = "target=own-process";
            }
            else
            {
                const auto child = c.proc.child_processes.find(resolved.value.id);
                detail = child != c.proc.child_processes.end() ? "target_pid=" + std::to_string(child->second.pid)
                                                               : "target=unknown-record-" + std::to_string(resolved.value.id);
            }

            if (timeout.value())
            {
                detail += " timeout_100ns=" + std::to_string(timeout.read().QuadPart);
            }
            else
            {
                detail += " timeout=infinite";
            }

            c.win_emu.callbacks.on_generic_access("Waiting on process handle", std::u16string(detail.begin(), detail.end()));
        }

        void trace_wait_target(const syscall_context& c, const handle resolved, const emulator_object<LARGE_INTEGER> timeout)
        {
            static const bool enabled = std::getenv("SOGEN_TRACE_WAIT_TARGETS") != nullptr;
            if (!enabled)
            {
                return;
            }

            std::u16string object_name{};

            switch (resolved.value.type)
            {
            case handle_types::event:
                if (const auto* e = c.proc.events.get(resolved))
                {
                    object_name = e->name;
                }
                break;
            case handle_types::mutant:
                if (const auto* m = c.proc.mutants.get(resolved))
                {
                    object_name = m->name;
                }
                break;
            case handle_types::semaphore:
                if (const auto* s = c.proc.semaphores.get(resolved))
                {
                    object_name = s->name;
                }
                break;
            default:
                break;
            }

            const auto type_name = get_type_name(static_cast<handle_types::type>(resolved.value.type));
            const auto name_u8 = object_name.empty() ? std::string("<unnamed>") : u16_to_u8(object_name);

            if (timeout.value())
            {
                const auto quad_part = timeout.read().QuadPart;
                const auto requested_ms = quad_part < 0 ? static_cast<double>(-quad_part) / 10000.0 : -1.0;
                c.win_emu.log.error("[wait-target-trace] tid=%u type=%s name=%s timeout=finite requested_ms=%.1f\n", c.thread().id,
                                    u16_to_u8(type_name).c_str(), name_u8.c_str(), requested_ms);

                if (requested_ms > 1000.0 && std::getenv("SOGEN_TRACE_WAIT_TARGET_CALLER_STACK") != nullptr)
                {
                    const auto rip = c.emu.read_instruction_pointer();
                    const auto rsp = c.emu.read_stack_pointer();
                    const auto* rip_mod = c.win_emu.mod_manager.find_by_address(rip);
                    c.win_emu.log.error("[WAIT_TARGET_CALLER] tid=%u rip=0x%llx (%s+0x%llx) rsp=0x%llx requested_ms=%.1f\n", c.thread().id,
                                        static_cast<unsigned long long>(rip), rip_mod ? rip_mod->name.c_str() : "?",
                                        rip_mod ? static_cast<unsigned long long>(rip - rip_mod->image_base) : 0ULL,
                                        static_cast<unsigned long long>(rsp), requested_ms);
                    for (uint64_t i = 0; i < 64; ++i)
                    {
                        uint64_t value{};
                        if (!c.win_emu.memory.try_read_memory(rsp + (i * 8), &value, sizeof(value)))
                        {
                            break;
                        }
                        const auto* mod = c.win_emu.mod_manager.find_by_address(value);
                        char mod_suffix[128] = {};
                        if (mod)
                        {
                            snprintf(mod_suffix, sizeof(mod_suffix), "%s+0x%llx", mod->name.c_str(),
                                     static_cast<unsigned long long>(value - mod->image_base));
                        }
                        fprintf(stderr, "  [rsp+0x%llx] = 0x%llx %s\n", static_cast<unsigned long long>(i * 8),
                                static_cast<unsigned long long>(value), mod_suffix);
                    }
                    fflush(stderr);
                }
            }
            else
            {
                c.win_emu.log.error("[wait-target-trace] tid=%u type=%s name=%s timeout=infinite\n", c.thread().id,
                                    u16_to_u8(type_name).c_str(), name_u8.c_str());
            }
        }

        NTSTATUS handle_NtCompareObjects(const syscall_context& c, const handle first, const handle second)
        {
            const auto first_resolved = c.proc.resolve_object_pseudo_handle(first, c.vcpu.active_thread);
            const auto second_resolved = c.proc.resolve_object_pseudo_handle(second, c.vcpu.active_thread);
            if (std::getenv("SOGEN_DEBUG_COMPARE_OBJECTS") != nullptr)
            {
                fprintf(stderr,
                        "[COMPARE_OBJECTS] first=0x%llx(id=%u,type=%u,pseudo=%u) second=0x%llx(id=%u,type=%u,pseudo=%u) "
                        "first_resolved=0x%llx(id=%u,type=%u,pseudo=%u) second_resolved=0x%llx(id=%u,type=%u,pseudo=%u)\n",
                        static_cast<unsigned long long>(first.bits), first.value.id, first.value.type, first.value.is_pseudo,
                        static_cast<unsigned long long>(second.bits), second.value.id, second.value.type, second.value.is_pseudo,
                        static_cast<unsigned long long>(first_resolved.bits), first_resolved.value.id, first_resolved.value.type,
                        first_resolved.value.is_pseudo, static_cast<unsigned long long>(second_resolved.bits), second_resolved.value.id,
                        second_resolved.value.type, second_resolved.value.is_pseudo);
                fflush(stderr);
            }

            if (std::getenv("SOGEN_DEBUG_COMPARE_OBJECTS_STACK") != nullptr)
            {
                const auto rip = c.emu.read_instruction_pointer();
                const auto rsp = c.emu.read_stack_pointer();
                const auto* rip_mod = c.win_emu.mod_manager.find_by_address(rip);
                fprintf(stderr, "[COMPARE_OBJECTS_STACK] tid=%u rip=0x%llx (%s+0x%llx) rsp=0x%llx\n", c.thread().id,
                        static_cast<unsigned long long>(rip), rip_mod ? rip_mod->name.c_str() : "?",
                        rip_mod ? static_cast<unsigned long long>(rip - rip_mod->image_base) : 0ULL, static_cast<unsigned long long>(rsp));
                for (const auto& mod : c.win_emu.mod_manager.modules() | std::views::values)
                {
                    fprintf(stderr, "  [MODULE] %s base=0x%llx size=0x%llx\n", mod.name.c_str(),
                            static_cast<unsigned long long>(mod.image_base), static_cast<unsigned long long>(mod.size_of_image));
                }
                for (uint64_t i = 0; i < 64; ++i)
                {
                    uint64_t value{};
                    if (!c.win_emu.memory.try_read_memory(rsp + (i * 8), &value, sizeof(value)))
                    {
                        break;
                    }
                    const auto* mod = c.win_emu.mod_manager.find_by_address(value);
                    char mod_suffix[128] = {};
                    if (mod)
                    {
                        snprintf(mod_suffix, sizeof(mod_suffix), "%s+0x%llx", mod->name.c_str(),
                                 static_cast<unsigned long long>(value - mod->image_base));
                    }
                    fprintf(stderr, "  [rsp+0x%llx] = 0x%llx %s\n", static_cast<unsigned long long>(i * 8),
                            static_cast<unsigned long long>(value), mod_suffix);
                }
                fflush(stderr);
            }
            return (first_resolved == second_resolved) ? STATUS_SUCCESS : STATUS_NOT_SAME_OBJECT;
        }

        DWORD handle_NtUserMsgWaitForMultipleObjectsEx(const syscall_context& c, const ULONG count, const emulator_object<handle> handles,
                                                       const DWORD timeout, const DWORD wake_mask, const DWORD flags)
        {
            constexpr DWORD mwmo_waitall = 0x0001;
            constexpr DWORD wait_failed = 0xFFFFFFFF;
            constexpr DWORD infinite_timeout = 0xFFFFFFFF;

            if (count > 64)
            {
                return wait_failed;
            }

            const bool wait_all = (flags & mwmo_waitall) != 0;
            auto& t = c.thread();
            t.await_objects = {};
            t.await_any = false;
            t.await_msg_mask = {};
            t.await_time = {};

            std::vector<handle> wait_handles{};
            wait_handles.reserve(count);
            for (ULONG i = 0; i < count; ++i)
            {
                const auto h = handles.read(i);

                if (c.proc.is_object_pseudo_handle(h))
                {
                    return wait_failed;
                }

                if (!NT_SUCCESS(validate_wait_handle(c, h)))
                {
                    return wait_failed;
                }

                wait_handles.push_back(h);
            }

            t.await_objects = std::move(wait_handles);
            t.await_any = !wait_all;
            if (wake_mask != 0)
            {
                t.await_msg_mask = wake_mask;
            }

            if (timeout != infinite_timeout)
            {
                t.await_time = c.win_emu.clock().steady_now() + std::chrono::milliseconds{timeout};
            }

            c.win_emu.yield_thread(c.vcpu, false);
            return {};
        }

        NTSTATUS handle_NtWaitForMultipleObjects(const syscall_context& c, const ULONG count, const emulator_object<handle> handles,
                                                 const WAIT_TYPE wait_type, const BOOLEAN alertable,
                                                 const emulator_object<LARGE_INTEGER> timeout)
        {
            if (wait_type != WaitAny && wait_type != WaitAll)
            {
                c.win_emu.log.error("Wait type not supported!\n");
                c.emu.stop();
                return STATUS_NOT_SUPPORTED;
            }

            if (count == 0 || count > 64) // MAXIMUM_WAIT_OBJECTS
            {
                return STATUS_INVALID_PARAMETER;
            }

            auto& t = c.thread();
            t.await_objects = {};
            t.await_any = false;

            std::vector<handle> wait_handles{};
            wait_handles.reserve(count);

            for (ULONG i = 0; i < count; ++i)
            {
                const auto raw_handle = handles.read(i);

                // Unlike NtWaitForSingleObject, pseudo handles (current process/thread) are not allowed in
                // NtWaitForMultipleObjects; Windows rejects them without resolving.
                if (c.proc.is_object_pseudo_handle(raw_handle))
                {
                    t.await_time = {};
                    return STATUS_INVALID_HANDLE;
                }

                const auto h = resolve_wait_handle(c, raw_handle);

                const auto validation_status = validate_wait_handle(c, h);
                if (!NT_SUCCESS(validation_status))
                {
                    t.await_time = {};
                    return validation_status;
                }

                trace_process_handle_wait(c, h, timeout);
                wait_handles.push_back(h);
            }

            t.await_objects = std::move(wait_handles);
            t.await_any = wait_type == WaitAny;

            if (timeout.value() && !t.await_time.has_value())
            {
                t.await_time = utils::convert_delay_interval_to_time_point(c.win_emu.clock(), timeout.read());
            }

            c.win_emu.yield_thread(c.vcpu, alertable);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtWaitForMultipleObjects32(const syscall_context& c, const ULONG count, const emulator_object<uint32_t> handles,
                                                   const WAIT_TYPE wait_type, const BOOLEAN alertable,
                                                   const emulator_object<LARGE_INTEGER> timeout)
        {
            if (wait_type != WaitAny && wait_type != WaitAll)
            {
                c.win_emu.log.error("Wait type not supported!\n");
                c.emu.stop();
                return STATUS_NOT_SUPPORTED;
            }

            if (count == 0 || count > 64) // MAXIMUM_WAIT_OBJECTS
            {
                return STATUS_INVALID_PARAMETER;
            }

            auto& t = c.thread();
            t.await_objects = {};
            t.await_any = false;

            std::vector<handle> wait_handles{};
            wait_handles.reserve(count);

            for (ULONG i = 0; i < count; ++i)
            {
                const auto raw_handle = handles.read(i);
                const auto h = resolve_wait32_handle(c, raw_handle);
                if (!h)
                {
                    t.await_time = {};
                    return STATUS_INVALID_HANDLE;
                }

                if (c.proc.is_object_pseudo_handle(*h))
                {
                    t.await_time = {};
                    return STATUS_INVALID_HANDLE;
                }

                const auto validation_status = validate_wait_handle(c, *h);
                if (!NT_SUCCESS(validation_status))
                {
                    t.await_time = {};
                    return validation_status;
                }

                trace_process_handle_wait(c, *h, timeout);
                wait_handles.push_back(*h);
            }

            t.await_objects = std::move(wait_handles);
            t.await_any = wait_type == WaitAny;

            if (timeout.value() && !t.await_time.has_value())
            {
                t.await_time = utils::convert_delay_interval_to_time_point(c.win_emu.clock(), timeout.read());
            }

            c.win_emu.yield_thread(c.vcpu, alertable);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtWaitForSingleObject(const syscall_context& c, const handle h, const BOOLEAN alertable,
                                              const emulator_object<LARGE_INTEGER> timeout)
        {
            const auto resolved_handle = resolve_wait_handle(c, h);
            const auto validation_status = validate_wait_handle(c, resolved_handle);
            if (!NT_SUCCESS(validation_status))
            {
                return validation_status;
            }

            auto& t = c.thread();
            t.await_objects = {resolved_handle};
            t.await_any = false;

            trace_process_handle_wait(c, resolved_handle, timeout);
            trace_wait_target(c, resolved_handle, timeout);
            record_wait_start(resolved_handle, c.thread().id);

            if (timeout.value() && !t.await_time.has_value())
            {
                t.await_time = utils::convert_delay_interval_to_time_point(c.win_emu.clock(), timeout.read());
            }

            c.win_emu.yield_thread(c.vcpu, alertable);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSignalAndWaitForSingleObject(const syscall_context& c, const handle signal_handle, const handle wait_handle,
                                                       const BOOLEAN alertable, const emulator_object<LARGE_INTEGER> timeout)
        {
            const emulator_object<LONG> no_previous_state{c.emu.memory()};

            NTSTATUS signal_status{};

            switch (signal_handle.value.type)
            {
            case handle_types::event:
                signal_status = handle_NtSetEvent(c, signal_handle.bits, no_previous_state);
                break;

            case handle_types::mutant:
                signal_status = handle_NtReleaseMutant(c, signal_handle, no_previous_state);
                break;

            case handle_types::semaphore:
                signal_status = handle_NtReleaseSemaphore(c, signal_handle, 1, no_previous_state);
                break;

            default:
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            if (!NT_SUCCESS(signal_status))
            {
                return signal_status;
            }

            return handle_NtWaitForSingleObject(c, wait_handle, alertable, timeout);
        }

        NTSTATUS handle_NtSetInformationObject(const syscall_context& c, const handle /*h*/,
                                               const OBJECT_INFORMATION_CLASS object_information_class,
                                               const emulator_pointer /*object_information*/, const ULONG object_information_length)
        {
            if (object_information_class == ObjectHandleFlagInformation)
            {
                if (object_information_length < sizeof(OBJECT_HANDLE_FLAG_INFORMATION))
                {
                    return STATUS_INFO_LENGTH_MISMATCH;
                }

                return STATUS_SUCCESS;
            }

            c.win_emu.log.error("Unsupported object info class for NtSetInformationObject: %X\n",
                                static_cast<uint32_t>(object_information_class));
            c.emu.stop();
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtQuerySecurityObject(const syscall_context& c, const handle /*h*/, const SECURITY_INFORMATION security_information,
                                              const emulator_pointer security_descriptor, const ULONG length,
                                              const emulator_object<ULONG> length_needed)
        {
            if ((security_information &
                 (OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION)) == 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            // Owner SID: S-1-5-32-544 (Administrators)
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
            const uint8_t owner_sid[] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00};

            // Group SID: S-1-5-18 (Local System)
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
            const uint8_t group_sid[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x12, 0x00, 0x00, 0x00};

            // DACL structure
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
            const uint8_t dacl_data[] = {
                0x02, 0x00, 0x9C, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x0F, 0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x0F, 0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x05, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x01, 0x02, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00, 0x00, 0x0B, 0x14, 0x00, 0x00, 0x00, 0x00, 0xE0,
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x14, 0x00, 0x00, 0x00, 0x00, 0xE0,
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x18, 0x00, 0x00, 0x00, 0x00, 0x10,
                0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00, 0x00, 0x0B, 0x14, 0x00,
                0x00, 0x00, 0x00, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};

            // SACL structure
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
            const uint8_t sacl_data[] = {0x02, 0x00, 0x1C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x14, 0x00, 0x01, 0x00,
                                         0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x00};

            ULONG total_size = sizeof(SECURITY_DESCRIPTOR_RELATIVE);

            if (security_information & OWNER_SECURITY_INFORMATION)
            {
                total_size += sizeof(owner_sid);
            }

            if (security_information & GROUP_SECURITY_INFORMATION)
            {
                total_size += sizeof(group_sid);
            }

            if (security_information & DACL_SECURITY_INFORMATION)
            {
                total_size += sizeof(dacl_data);
            }

            if (security_information & LABEL_SECURITY_INFORMATION)
            {
                total_size += sizeof(sacl_data);
            }

            length_needed.write(total_size);

            if (length < total_size)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            if (!security_descriptor)
            {
                return STATUS_INVALID_PARAMETER;
            }

            SECURITY_DESCRIPTOR_RELATIVE sd = {};
            sd.Revision = SECURITY_DESCRIPTOR_REVISION;
            sd.Control = SE_SELF_RELATIVE;

            ULONG current_offset = sizeof(sd);

            if (security_information & OWNER_SECURITY_INFORMATION)
            {
                sd.Owner = current_offset;
                c.emu.write_memory(security_descriptor + current_offset, owner_sid);
                current_offset += sizeof(owner_sid);
            }

            if (security_information & GROUP_SECURITY_INFORMATION)
            {
                sd.Group = current_offset;
                c.emu.write_memory(security_descriptor + current_offset, group_sid);
                current_offset += sizeof(group_sid);
            }

            if (security_information & DACL_SECURITY_INFORMATION)
            {
                sd.Control |= SE_DACL_PRESENT;
                sd.Dacl = current_offset;
                c.emu.write_memory(security_descriptor + current_offset, dacl_data);
                current_offset += sizeof(dacl_data);
            }

            if (security_information & LABEL_SECURITY_INFORMATION)
            {
                sd.Control |= SE_SACL_PRESENT | SE_SACL_AUTO_INHERITED;
                sd.Sacl = current_offset;
                c.emu.write_memory(security_descriptor + current_offset, sacl_data);
                current_offset += sizeof(sacl_data);
            }

            assert(current_offset == total_size);

            c.emu.write_memory(security_descriptor, sd);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetSecurityObject()
        {
            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
