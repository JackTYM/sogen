#include "std_include.hpp"
#include "syscall_dispatcher.hpp"
#include "cpu_context.hpp"
#include "emulator_utils.hpp"
#include "syscall_utils.hpp"

#include <numeric>
#include <cwctype>
#include <algorithm>
#include <utils/time.hpp>
#include <utils/finally.hpp>

namespace sogen
{

    namespace syscalls
    {
        // syscalls/event.cpp:
        NTSTATUS handle_NtSetEvent(const syscall_context& c, uint64_t handle, emulator_object<LONG> previous_state);
        NTSTATUS handle_NtPulseEvent(const syscall_context& c, uint64_t handle, emulator_object<LONG> previous_state);
        NTSTATUS handle_NtTraceEvent();
        NTSTATUS handle_NtQueryEvent(const syscall_context& c, handle event_handle, uint32_t event_information_class,
                                     emulator_object<EVENT_BASIC_INFORMATION> event_information, uint32_t event_information_length,
                                     emulator_object<uint32_t> return_length);
        NTSTATUS handle_NtClearEvent(const syscall_context& c, handle event_handle);
        NTSTATUS handle_NtSetEventBoostPriority(const syscall_context& c, handle event_handle);
        NTSTATUS handle_NtCreateEvent(const syscall_context& c, emulator_object<handle> event_handle, ACCESS_MASK desired_access,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, EVENT_TYPE event_type,
                                      BOOLEAN initial_state);
        NTSTATUS handle_NtOpenEvent(const syscall_context& c, emulator_object<uint64_t> event_handle, ACCESS_MASK desired_access,
                                    emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtCreateKeyedEvent(const syscall_context& c, emulator_object<handle> keyed_event_handle, ACCESS_MASK desired_access,
                                           emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, ULONG reserved);
        NTSTATUS handle_NtOpenKeyedEvent(const syscall_context& c, emulator_object<handle> keyed_event_handle, ACCESS_MASK desired_access,
                                         emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtReleaseKeyedEvent(const syscall_context& c, handle keyed_event_handle, uint64_t key_value, BOOLEAN alertable,
                                            uint64_t timeout);
        NTSTATUS handle_NtWaitForKeyedEvent(const syscall_context& c, handle keyed_event_handle, uint64_t key_value, BOOLEAN alertable,
                                            uint64_t timeout);

        // syscalls/exception.cpp
        NTSTATUS handle_NtRaiseHardError(const syscall_context& c, NTSTATUS error_status, ULONG number_of_parameters,
                                         emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> unicode_string_parameter_mask,
                                         uint64_t parameters, HARDERROR_RESPONSE_OPTION valid_response_option,
                                         emulator_object<HARDERROR_RESPONSE> response);
        NTSTATUS handle_NtRaiseException(const syscall_context& c,
                                         emulator_object<EMU_EXCEPTION_RECORD<EmulatorTraits<Emu64>>> exception_record,
                                         emulator_object<CONTEXT64> thread_context, BOOLEAN handle_exception);

        // syscalls/file.cpp
        NTSTATUS handle_NtSetInformationFile(const syscall_context& c, handle file_handle,
                                             emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                             uint64_t file_information, ULONG length, FILE_INFORMATION_CLASS info_class);
        NTSTATUS handle_NtQueryVolumeInformationFile(const syscall_context& c, handle file_handle,
                                                     emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                     uint64_t fs_information, ULONG length, FS_INFORMATION_CLASS fs_information_class);
        NTSTATUS handle_NtQueryDirectoryFileEx(const syscall_context& c, handle file_handle, handle event_handle,
                                               EMULATOR_CAST(emulator_pointer, PIO_APC_ROUTINE) apc_routine, emulator_pointer apc_context,
                                               emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                               uint64_t file_information, uint32_t length, uint32_t info_class, ULONG query_flags,
                                               emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> file_name);
        NTSTATUS handle_NtQueryDirectoryFile(const syscall_context& c, handle file_handle, handle event_handle,
                                             EMULATOR_CAST(emulator_pointer, PIO_APC_ROUTINE) apc_routine, emulator_pointer apc_context,
                                             emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                             uint64_t file_information, uint32_t length, uint32_t info_class, BOOLEAN return_single_entry,
                                             emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> file_name, BOOLEAN restart_scan);
        NTSTATUS handle_NtQueryInformationFile(const syscall_context& c, handle file_handle,
                                               emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                               uint64_t file_information, uint32_t length, uint32_t info_class);
        NTSTATUS handle_NtQueryInformationByName(const syscall_context& c,
                                                 emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                 emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                 uint64_t file_information, uint32_t length, uint32_t info_class);
        NTSTATUS handle_NtReadFile(const syscall_context& c, handle file_handle, uint64_t /*event*/, uint64_t /*apc_routine*/,
                                   uint64_t /*apc_context*/, emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                   uint64_t buffer, ULONG length, emulator_object<LARGE_INTEGER> /*byte_offset*/,
                                   emulator_object<ULONG> /*key*/);
        NTSTATUS handle_NtWriteFile(const syscall_context& c, handle file_handle, uint64_t /*event*/, uint64_t /*apc_routine*/,
                                    uint64_t /*apc_context*/, emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                    uint64_t buffer, ULONG length, emulator_object<LARGE_INTEGER> /*byte_offset*/,
                                    emulator_object<ULONG> /*key*/);
        NTSTATUS handle_NtCopyFileChunk(const syscall_context& c, handle source_handle, handle destination_handle, handle event_handle,
                                        emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, ULONG length,
                                        emulator_object<LARGE_INTEGER> source_offset, emulator_object<LARGE_INTEGER> destination_offset,
                                        emulator_object<ULONG> source_key, emulator_object<ULONG> destination_key, ULONG flags);
        NTSTATUS handle_NtLockFile(const syscall_context& c, handle file_handle, handle event_handle, uint64_t apc_routine,
                                   uint64_t apc_context, emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                   emulator_object<LARGE_INTEGER> byte_offset, emulator_object<LARGE_INTEGER> length, ULONG key,
                                   BOOLEAN fail_immediately, BOOLEAN exclusive_lock);
        NTSTATUS handle_NtUnlockFile(const syscall_context& c, handle file_handle,
                                     emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                     emulator_object<LARGE_INTEGER> byte_offset, emulator_object<LARGE_INTEGER> length, ULONG key);
        NTSTATUS handle_NtCreateFile(const syscall_context& c, emulator_object<handle> file_handle, ACCESS_MASK desired_access,
                                     emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                     emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> /*io_status_block*/,
                                     emulator_object<LARGE_INTEGER> /*allocation_size*/, ULONG /*file_attributes*/, ULONG /*share_access*/,
                                     ULONG create_disposition, ULONG create_options, uint64_t ea_buffer, ULONG ea_length);
        NTSTATUS handle_NtQueryAttributesFile(const syscall_context& c,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              emulator_object<FILE_BASIC_INFORMATION> file_information);
        NTSTATUS handle_NtQueryFullAttributesFile(const syscall_context& c,
                                                  emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                  emulator_object<FILE_NETWORK_OPEN_INFORMATION> file_information);
        NTSTATUS handle_NtOpenFile(const syscall_context& c, emulator_object<handle> file_handle, ACCESS_MASK desired_access,
                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                   emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, ULONG share_access,
                                   ULONG open_options);
        NTSTATUS handle_NtOpenDirectoryObject(const syscall_context& c, emulator_object<handle> directory_handle,
                                              ACCESS_MASK /*desired_access*/,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtCreateDirectoryObject(const syscall_context& /*c*/, emulator_object<handle> /*directory_handle*/,
                                                ACCESS_MASK /*desired_access*/,
                                                emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtOpenSymbolicLinkObject(const syscall_context& c, emulator_object<handle> link_handle,
                                                 ACCESS_MASK /*desired_access*/,
                                                 emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtQuerySymbolicLinkObject(const syscall_context& c, handle link_handle,
                                                  emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> link_target,
                                                  emulator_object<ULONG> returned_length);
        NTSTATUS handle_NtCreateNamedPipeFile(const syscall_context& c, emulator_object<handle> file_handle, ULONG desired_access,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, ULONG share_access,
                                              ULONG create_disposition, ULONG create_options, ULONG named_pipe_type, ULONG read_mode,
                                              ULONG completion_mode, ULONG maximum_instances, ULONG inbound_quota, ULONG outbound_quota,
                                              emulator_object<LARGE_INTEGER> default_timeout);
        NTSTATUS handle_NtFsControlFile(const syscall_context& c, handle file_handle, handle event, emulator_pointer apc_routine,
                                        emulator_pointer apc_context,
                                        emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, ULONG fs_control_code,
                                        emulator_pointer input_buffer, ULONG input_buffer_length, emulator_pointer output_buffer,
                                        ULONG output_buffer_length);
        NTSTATUS handle_NtFlushBuffersFile(const syscall_context& c, handle file_handle,
                                           emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block);
        NTSTATUS handle_NtFlushBuffersFileEx(const syscall_context& c, handle file_handle, ULONG flags, uint64_t parameters,
                                             ULONG parameters_size,
                                             emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block);
        NTSTATUS handle_NtDeleteFile(const syscall_context& c, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtCancelIoFile(const syscall_context& c, handle file_handle,
                                       emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block);
        NTSTATUS handle_NtCancelIoFileEx(const syscall_context& c, handle file_handle, uint64_t io_request_to_cancel,
                                         emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block);

        // syscalls/locale.cpp:
        NTSTATUS handle_NtInitializeNlsFiles(const syscall_context& c, emulator_object<uint64_t> base_address,
                                             emulator_object<LCID> default_locale_id,
                                             emulator_object<LARGE_INTEGER> /*default_casing_table_size*/);
        NTSTATUS handle_NtQueryDefaultLocale(const syscall_context&, BOOLEAN /*user_profile*/, emulator_object<LCID> default_locale_id);
        NTSTATUS handle_NtGetNlsSectionPtr(const syscall_context& c, ULONG section_type, ULONG section_data,
                                           emulator_pointer /*context_data*/, emulator_object<uint64_t> section_pointer,
                                           emulator_object<ULONG> section_size);
        NTSTATUS handle_NtGetMUIRegistryInfo(const syscall_context& c, ULONG flags, emulator_object<ULONG> data_size, uint64_t data);
        NTSTATUS handle_NtIsUILanguageComitted(const syscall_context& c);
        NTSTATUS handle_NtUserGetKeyboardLayout(const syscall_context& c);
        NTSTATUS handle_NtQueryDefaultUILanguage(const syscall_context&, emulator_object<LANGID> language_id);
        NTSTATUS handle_NtQueryInstallUILanguage(const syscall_context&, emulator_object<LANGID> language_id);

        // syscalls/memory.cpp:
        NTSTATUS handle_NtQueryVirtualMemory(const syscall_context& c, handle process_handle, uint64_t base_address, uint32_t info_class,
                                             uint64_t memory_information, uint64_t memory_information_length,
                                             emulator_object<uint64_t> return_length);
        NTSTATUS handle_NtProtectVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                               emulator_object<uint32_t> bytes_to_protect, uint32_t protection,
                                               emulator_object<uint32_t> old_protection);
        NTSTATUS handle_NtAllocateVirtualMemoryEx(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                                  emulator_object<uint64_t> bytes_to_allocate, uint32_t allocation_type,
                                                  uint32_t page_protection, emulator_object<MEM_EXTENDED_PARAMETER64> extended_parameters,
                                                  ULONG extended_parameter_count);
        NTSTATUS handle_NtAllocateVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                                uint64_t zero_bits, emulator_object<uint64_t> bytes_to_allocate, uint32_t allocation_type,
                                                uint32_t page_protection);
        NTSTATUS handle_NtFreeVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                            emulator_object<uint64_t> bytes_to_allocate, uint32_t free_type);
        NTSTATUS handle_NtReadVirtualMemory(const syscall_context& c, handle process_handle, emulator_pointer base_address,
                                            emulator_pointer buffer, uint64_t number_of_bytes_to_read,
                                            emulator_object<uint64_t> number_of_bytes_read);
        NTSTATUS handle_NtWriteVirtualMemory(const syscall_context& c, handle process_handle, emulator_pointer base_address,
                                             emulator_pointer buffer, uint64_t number_of_bytes_to_write,
                                             emulator_object<uint64_t> number_of_bytes_write);
        NTSTATUS handle_NtSetInformationVirtualMemory(const syscall_context& c, handle process_handle, uint32_t vm_info_class,
                                                      uint64_t number_of_entries, uint64_t virtual_addresses, uint64_t vm_information,
                                                      ULONG vm_information_length);
        BOOL handle_NtLockVirtualMemory();
        NTSTATUS handle_NtUnlockVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                              emulator_object<uint64_t> number_of_bytes, ULONG lock_type);
        NTSTATUS handle_NtFlushVirtualMemory(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                             emulator_object<uint64_t> region_size,
                                             emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block);

        // syscalls/mutant.cpp:
        NTSTATUS handle_NtReleaseMutant(const syscall_context& c, handle mutant_handle, emulator_object<LONG> previous_count);
        NTSTATUS handle_NtOpenMutant(const syscall_context& c, emulator_object<handle> mutant_handle, ACCESS_MASK desired_access,
                                     emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtCreateMutant(const syscall_context& c, emulator_object<handle> mutant_handle, ACCESS_MASK desired_access,
                                       emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, BOOLEAN initial_owner);

        // syscalls/namespace.cpp:
        NTSTATUS handle_NtCreatePrivateNamespace(const syscall_context& c, emulator_object<handle> namespace_handle,
                                                 ACCESS_MASK desired_access,
                                                 emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                 emulator_object<OBJECT_BOUNDARY_DESCRIPTOR> boundary_descriptor);

        NTSTATUS handle_NtOpenPrivateNamespace(const syscall_context& c, emulator_object<handle> namespace_handle,
                                               ACCESS_MASK desired_access,
                                               emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                               emulator_object<OBJECT_BOUNDARY_DESCRIPTOR> boundary_descriptor);
        NTSTATUS handle_NtDeletePrivateNamespace(const syscall_context& c, handle namespace_handle);

        // syscalls/object.cpp:
        NTSTATUS handle_NtClose(const syscall_context& c, handle h);
        NTSTATUS handle_NtDuplicateObject(const syscall_context& c, handle source_process_handle, handle source_handle,
                                          handle target_process_handle, emulator_object<handle> target_handle, ACCESS_MASK desired_access,
                                          ULONG handle_attributes, ULONG options);
        NTSTATUS handle_NtQueryObject(const syscall_context& c, handle handle, OBJECT_INFORMATION_CLASS object_information_class,
                                      emulator_pointer object_information, ULONG object_information_length,
                                      emulator_object<ULONG> return_length);
        NTSTATUS handle_NtCompareObjects(const syscall_context& c, handle first, handle second);
        DWORD handle_NtUserMsgWaitForMultipleObjectsEx(const syscall_context& c, ULONG count, emulator_object<handle> handles,
                                                       DWORD timeout, DWORD wake_mask, DWORD flags);
        NTSTATUS handle_NtWaitForMultipleObjects(const syscall_context& c, ULONG count, emulator_object<handle> handles,
                                                 WAIT_TYPE wait_type, BOOLEAN alertable, emulator_object<LARGE_INTEGER> timeout);
        NTSTATUS handle_NtWaitForMultipleObjects32(const syscall_context& c, ULONG count, emulator_object<uint32_t> handles,
                                                   WAIT_TYPE wait_type, BOOLEAN alertable, emulator_object<LARGE_INTEGER> timeout);
        NTSTATUS handle_NtWaitForSingleObject(const syscall_context& c, handle h, BOOLEAN alertable,
                                              emulator_object<LARGE_INTEGER> timeout);
        NTSTATUS handle_NtSetInformationObject(const syscall_context& c, handle handle, uint32_t object_info_class,
                                               uint64_t object_information, ULONG object_information_length);
        NTSTATUS handle_NtQuerySecurityObject(const syscall_context& c, handle /*h*/, SECURITY_INFORMATION /*security_information*/,
                                              emulator_pointer security_descriptor, ULONG length, emulator_object<ULONG> length_needed);
        NTSTATUS handle_NtSetSecurityObject(const syscall_context& c, handle object_handle, SECURITY_INFORMATION security_information,
                                            uint64_t security_descriptor);

        // syscalls/port.cpp:
        NTSTATUS handle_NtConnectPort(const syscall_context& c, emulator_object<handle> client_port_handle,
                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> server_port_name,
                                      emulator_object<SECURITY_QUALITY_OF_SERVICE> /*security_qos*/,
                                      emulator_object<PORT_VIEW64> client_shared_memory,
                                      emulator_object<REMOTE_PORT_VIEW64> /*server_shared_memory*/,
                                      emulator_object<ULONG> /*maximum_message_length*/, emulator_pointer connection_info,
                                      emulator_object<ULONG> connection_info_length);
        NTSTATUS handle_NtSecureConnectPort(const syscall_context& c, emulator_object<handle> client_port_handle,
                                            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> server_port_name,
                                            emulator_object<SECURITY_QUALITY_OF_SERVICE> security_qos,
                                            emulator_object<PORT_VIEW64> client_shared_memory, emulator_pointer /*server_sid*/,
                                            emulator_object<REMOTE_PORT_VIEW64> server_shared_memory,
                                            emulator_object<ULONG> maximum_message_length, emulator_pointer connection_info,
                                            emulator_object<ULONG> connection_info_length);
        NTSTATUS handle_NtAlpcCreatePort(const syscall_context& c, emulator_object<handle> port_handle,
                                         emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                         emulator_pointer port_attributes);
        NTSTATUS handle_NtAlpcConnectPort(const syscall_context& c, emulator_object<handle> port_handle,
                                          emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> server_port_name,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                          emulator_pointer /*port_attributes*/, ULONG /*flags*/, emulator_pointer /*required_server_sid*/,
                                          emulator_pointer /*connection_message*/,
                                          emulator_object<EmulatorTraits<Emu64>::SIZE_T> /*buffer_length*/,
                                          emulator_pointer /*out_message_attributes*/, emulator_pointer /*in_message_attributes*/,
                                          emulator_object<LARGE_INTEGER> /*timeout*/);
        NTSTATUS handle_NtAlpcConnectPortEx(const syscall_context& c, emulator_object<handle> port_handle,
                                            emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> connection_port_object_attributes,
                                            emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*client_port_object_attributes*/,
                                            emulator_pointer port_attributes, ULONG flags,
                                            emulator_pointer /*server_security_requirements*/, emulator_pointer connection_message,
                                            emulator_object<EmulatorTraits<Emu64>::SIZE_T> buffer_length,
                                            emulator_pointer out_message_attributes, emulator_pointer in_message_attributes,
                                            emulator_object<LARGE_INTEGER> timeout);
        NTSTATUS handle_NtAlpcSendWaitReceivePort(const syscall_context& c, handle port_handle, ULONG /*flags*/,
                                                  emulator_object<PORT_MESSAGE64> send_message,
                                                  emulator_object<ALPC_MESSAGE_ATTRIBUTES>
                                                  /*send_message_attributes*/,
                                                  emulator_object<PORT_MESSAGE64> receive_message,
                                                  emulator_object<EmulatorTraits<Emu64>::SIZE_T> /*buffer_length*/,
                                                  emulator_object<ALPC_MESSAGE_ATTRIBUTES>
                                                  /*receive_message_attributes*/,
                                                  emulator_object<LARGE_INTEGER> /*timeout*/);
        NTSTATUS handle_NtAlpcQueryInformation();
        NTSTATUS handle_NtAlpcSetInformation();
        NTSTATUS handle_NtAlpcCreateSecurityContext();
        NTSTATUS handle_NtAlpcDeleteSecurityContext();

        // syscalls/process.cpp:
        NTSTATUS handle_NtQueryInformationProcess(const syscall_context& c, handle process_handle, uint32_t info_class,
                                                  uint64_t process_information, uint32_t process_information_length,
                                                  emulator_object<uint32_t> return_length);
        NTSTATUS handle_NtSetInformationProcess(const syscall_context& c, handle process_handle, uint32_t info_class,
                                                uint64_t process_information, uint32_t process_information_length);
        NTSTATUS handle_NtOpenProcess(const syscall_context& c, emulator_object<handle> process_handle, ACCESS_MASK desired_access,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                      emulator_object<CLIENT_ID64> client_id);
        NTSTATUS handle_NtOpenProcessToken(const syscall_context&, handle process_handle, ACCESS_MASK /*desired_access*/,
                                           emulator_object<handle> token_handle);
        NTSTATUS handle_NtOpenProcessTokenEx(const syscall_context& c, handle process_handle, ACCESS_MASK desired_access,
                                             ULONG /*handle_attributes*/, emulator_object<handle> token_handle);
        NTSTATUS handle_NtTerminateProcess(const syscall_context& c, handle process_handle, NTSTATUS exit_status);
        NTSTATUS handle_NtFlushProcessWriteBuffers(const syscall_context& c);

        // syscalls/registry.cpp:
        NTSTATUS handle_NtOpenKey(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK /*desired_access*/,
                                  emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtOpenKeyEx(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                    emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, ULONG /*open_options*/);
        NTSTATUS handle_NtQueryKey(const syscall_context& c, handle key_handle, KEY_INFORMATION_CLASS key_information_class,
                                   uint64_t key_information, ULONG length, emulator_object<ULONG> result_length);
        NTSTATUS handle_NtQueryValueKey(const syscall_context& c, handle key_handle,
                                        emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> value_name,
                                        KEY_VALUE_INFORMATION_CLASS key_value_information_class, uint64_t key_value_information,
                                        ULONG length, emulator_object<ULONG> result_length);
        NTSTATUS handle_NtQueryMultipleValueKey(const syscall_context& c, handle key_handle, emulator_object<KEY_VALUE_ENTRY> value_entries,
                                                ULONG entry_count, uint64_t value_buffer, emulator_object<ULONG> buffer_length,
                                                emulator_object<ULONG> required_buffer_length);
        NTSTATUS handle_NtCreateKey(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                    emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, ULONG /*title_index*/,
                                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*class*/, ULONG /*create_options*/,
                                    emulator_object<ULONG> /*disposition*/);
        NTSTATUS handle_NtSetValueKey(const syscall_context& c, handle key_handle,
                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> value_name, ULONG /*title_index*/, ULONG type,
                                      uint64_t data, ULONG data_size);
        NTSTATUS handle_NtDeleteValueKey(const syscall_context& c, handle key_handle,
                                         emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> value_name);
        NTSTATUS handle_NtDeleteKey(const syscall_context& c, handle key_handle);
        NTSTATUS handle_NtNotifyChangeKey(const syscall_context& c, handle key_handle, handle event, uint64_t apc_routine,
                                          uint64_t apc_context, uint64_t io_status_block, ULONG completion_filter, BOOLEAN watch_subtree,
                                          uint64_t buffer, ULONG buffer_size, BOOLEAN asynchronous);
        NTSTATUS handle_NtSetInformationKey(const syscall_context& c, handle key_handle, KEY_SET_INFORMATION_CLASS key_information_class,
                                            uint64_t key_information, ULONG length);
        NTSTATUS handle_NtEnumerateKey(const syscall_context& c, handle key_handle, ULONG index,
                                       KEY_INFORMATION_CLASS key_information_class, uint64_t key_information, ULONG length,
                                       emulator_object<ULONG> result_length);
        NTSTATUS handle_NtEnumerateValueKey(const syscall_context& c, handle key_handle, ULONG index,
                                            KEY_VALUE_INFORMATION_CLASS key_value_information_class, uint64_t key_value_information,
                                            ULONG length, emulator_object<ULONG> result_length);

        // syscalls/section.cpp:
        NTSTATUS handle_NtCreateSection(const syscall_context& c, emulator_object<handle> section_handle, ACCESS_MASK /*desired_access*/,
                                        emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                        emulator_object<ULARGE_INTEGER> maximum_size, ULONG section_page_protection,
                                        ULONG allocation_attributes, handle file_handle);
        NTSTATUS handle_NtOpenSection(const syscall_context& c, emulator_object<handle> section_handle, ACCESS_MASK /*desired_access*/,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtQuerySection(const syscall_context& c, handle section_handle, SECTION_INFORMATION_CLASS section_information_class,
                                       uint64_t section_information, EmulatorTraits<Emu64>::SIZE_T section_information_length,
                                       emulator_object<EmulatorTraits<Emu64>::SIZE_T> result_length);
        NTSTATUS handle_NtMapViewOfSection(const syscall_context& c, handle section_handle, handle process_handle,
                                           emulator_object<uint64_t> base_address,
                                           EMULATOR_CAST(EmulatorTraits<Emu64>::ULONG_PTR, ULONG_PTR) /*zero_bits*/,
                                           EMULATOR_CAST(EmulatorTraits<Emu64>::SIZE_T, SIZE_T) /*commit_size*/,
                                           emulator_object<LARGE_INTEGER> /*section_offset*/,
                                           emulator_object<EMULATOR_CAST(EmulatorTraits<Emu64>::SIZE_T, SIZE_T)> view_size,
                                           SECTION_INHERIT /*inherit_disposition*/, ULONG /*allocation_type*/, ULONG /*win32_protect*/);
        NTSTATUS handle_NtMapViewOfSectionEx(const syscall_context& c, handle section_handle, handle process_handle,
                                             emulator_object<uint64_t> base_address, emulator_object<LARGE_INTEGER> section_offset,
                                             emulator_object<EMULATOR_CAST(EmulatorTraits<Emu64>::SIZE_T, SIZE_T)> view_size,
                                             ULONG allocation_type, ULONG page_protection,
                                             uint64_t extended_parameters, // PMEM_EXTENDED_PARAMETER
                                             ULONG extended_parameter_count);
        NTSTATUS handle_NtUnmapViewOfSection(const syscall_context& c, handle process_handle, uint64_t base_address);
        NTSTATUS handle_NtUnmapViewOfSectionEx(const syscall_context& c, handle process_handle, uint64_t base_address, ULONG /*flags*/);
        NTSTATUS handle_NtAreMappedFilesTheSame(const syscall_context& c, uint64_t file1_address, uint64_t file2_address);

        // syscalls/semaphore.cpp:
        NTSTATUS handle_NtOpenSemaphore(const syscall_context& c, emulator_object<handle> semaphore_handle, ACCESS_MASK /*desired_access*/,
                                        emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtReleaseSemaphore(const syscall_context& c, handle semaphore_handle, ULONG release_count,
                                           emulator_object<LONG> previous_count);
        NTSTATUS handle_NtCreateSemaphore(const syscall_context& c, emulator_object<handle> semaphore_handle,
                                          ACCESS_MASK /*desired_access*/,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, LONG initial_count,
                                          LONG maximum_count);

        // syscalls/system.cpp:
        NTSTATUS handle_NtQuerySystemInformation(const syscall_context& c, uint32_t info_class, uint64_t system_information,
                                                 uint32_t system_information_length, emulator_object<uint32_t> return_length);
        NTSTATUS handle_NtQuerySystemInformationEx(const syscall_context& c, uint32_t info_class, uint64_t input_buffer,
                                                   uint32_t input_buffer_length, uint64_t system_information,
                                                   uint32_t system_information_length, emulator_object<uint32_t> return_length);
        NTSTATUS handle_NtAllocateUuids(const syscall_context& c, emulator_object<ULARGE_INTEGER> time, emulator_object<ULONG> range,
                                        emulator_object<ULONG> sequence, emulator_object<uint64_t> seed);
        NTSTATUS handle_NtSetSystemInformation(const syscall_context& c, uint32_t system_info_class, uint64_t system_information,
                                               ULONG system_information_length);
        NTSTATUS handle_NtPowerInformation(const syscall_context& c, uint32_t information_level, uint64_t input_buffer,
                                           uint32_t input_buffer_length, uint64_t output_buffer, uint32_t output_buffer_length);

        // syscalls/thread.cpp:
        NTSTATUS handle_NtSetInformationThread(const syscall_context& c, handle thread_handle, THREADINFOCLASS info_class,
                                               uint64_t thread_information, uint32_t thread_information_length);

        NTSTATUS handle_NtQueryInformationThread(const syscall_context& c, handle thread_handle, uint32_t info_class,
                                                 uint64_t thread_information, uint32_t thread_information_length,
                                                 emulator_object<uint32_t> return_length);
        NTSTATUS handle_NtOpenThread(const syscall_context&, emulator_object<handle> thread_handle, ACCESS_MASK desired_access,
                                     emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                     emulator_object<CLIENT_ID64> client_id);
        NTSTATUS handle_NtOpenThreadToken(const syscall_context&, handle thread_handle, ACCESS_MASK /*desired_access*/,
                                          BOOLEAN /*open_as_self*/, emulator_object<handle> token_handle);
        NTSTATUS handle_NtOpenThreadTokenEx(const syscall_context& c, handle thread_handle, ACCESS_MASK desired_access,
                                            BOOLEAN open_as_self, ULONG /*handle_attributes*/, emulator_object<handle> token_handle);
        NTSTATUS handle_NtTerminateThread(const syscall_context& c, handle thread_handle, NTSTATUS exit_status);
        NTSTATUS handle_NtDelayExecution(const syscall_context& c, BOOLEAN alertable, emulator_object<LARGE_INTEGER> delay_interval);
        NTSTATUS handle_NtAlertThreadByThreadId(const syscall_context& c, uint64_t thread_id);
        NTSTATUS handle_NtAlertThreadByThreadIdEx(const syscall_context& c, uint64_t thread_id,
                                                  emulator_object<EMU_RTL_SRWLOCK<EmulatorTraits<Emu64>>> lock);
        NTSTATUS handle_NtWaitForAlertByThreadId(const syscall_context& c, uint64_t, emulator_object<LARGE_INTEGER> timeout);
        NTSTATUS handle_NtYieldExecution(const syscall_context& c);
        NTSTATUS handle_NtSetThreadExecutionState(const syscall_context& c, ULONG new_flags, emulator_object<ULONG> previous_flags);
        NTSTATUS handle_NtSuspendThread(const syscall_context& c, handle thread_handle, emulator_object<ULONG> previous_suspend_count);
        NTSTATUS handle_NtResumeThread(const syscall_context& c, handle thread_handle, emulator_object<ULONG> previous_suspend_count);
        NTSTATUS handle_NtContinue(const syscall_context& c, emulator_object<CONTEXT64> thread_context, BOOLEAN raise_alert);
        NTSTATUS handle_NtContinueEx(const syscall_context& c, emulator_object<CONTEXT64> thread_context, uint64_t continue_argument);
        NTSTATUS handle_NtGetNextThread(const syscall_context& c, handle process_handle, handle thread_handle,
                                        ACCESS_MASK /*desired_access*/, ULONG /*handle_attributes*/, ULONG flags,
                                        emulator_object<handle> new_thread_handle);
        NTSTATUS handle_NtGetContextThread(const syscall_context& c, handle thread_handle, emulator_object<CONTEXT64> thread_context);
        NTSTATUS handle_NtSetContextThread(const syscall_context& c, handle thread_handle, emulator_object<CONTEXT64> thread_context);
        NTSTATUS handle_NtCreateThreadEx(const syscall_context& c, emulator_object<handle> thread_handle, ACCESS_MASK /*desired_access*/,
                                         emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>>
                                         /*object_attributes*/,
                                         handle process_handle, uint64_t start_routine, uint64_t argument, ULONG create_flags,
                                         EmulatorTraits<Emu64>::SIZE_T /*zero_bits*/, EmulatorTraits<Emu64>::SIZE_T stack_size,
                                         EmulatorTraits<Emu64>::SIZE_T maximum_stack_size,
                                         emulator_object<PS_ATTRIBUTE_LIST<EmulatorTraits<Emu64>>> attribute_list);
        NTSTATUS handle_NtGetCurrentProcessorNumberEx(const syscall_context&, emulator_object<PROCESSOR_NUMBER> processor_number);
        ULONG handle_NtGetCurrentProcessorNumber();
        NTSTATUS handle_NtQueueApcThreadEx2(const syscall_context& c, handle thread_handle, handle reserve_handle, uint32_t apc_flags,
                                            uint64_t apc_routine, uint64_t apc_argument1, uint64_t apc_argument2, uint64_t apc_argument3);
        NTSTATUS handle_NtQueueApcThreadEx(const syscall_context& c, handle thread_handle, handle reserve_handle, uint64_t apc_routine,
                                           uint64_t apc_argument1, uint64_t apc_argument2, uint64_t apc_argument3);
        NTSTATUS handle_NtQueueApcThread(const syscall_context& c, handle thread_handle, uint64_t apc_routine, uint64_t apc_argument1,
                                         uint64_t apc_argument2, uint64_t apc_argument3);
        NTSTATUS handle_NtCallbackReturn(const syscall_context& c, emulator_pointer callback_result, ULONG callback_result_length,
                                         NTSTATUS callback_status);

        // syscalls/timer.cpp:
        NTSTATUS handle_NtQueryTimerResolution(const syscall_context&, emulator_object<ULONG> maximum_time,
                                               emulator_object<ULONG> minimum_time, emulator_object<ULONG> current_time);
        NTSTATUS handle_NtSetTimerResolution(const syscall_context&, ULONG /*desired_resolution*/, BOOLEAN set_resolution,
                                             emulator_object<ULONG> current_resolution);
        NTSTATUS handle_NtCreateTimer2(const syscall_context& c, emulator_object<handle> timer_handle, uint64_t reserved,
                                       emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, ULONG attributes,
                                       ACCESS_MASK desired_access);
        NTSTATUS handle_NtCreateTimer(const syscall_context& c, emulator_object<handle> timer_handle, ACCESS_MASK desired_access,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, ULONG timer_type);
        NTSTATUS handle_NtOpenTimer(const syscall_context& c, emulator_object<handle> timer_handle, ACCESS_MASK desired_access,
                                    emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtSetTimer(const syscall_context& c, handle timer_handle, uint64_t due_time, uint64_t apc_routine,
                                   uint64_t apc_context, BOOLEAN resume_timer, LONG period, emulator_object<BOOLEAN> previous_state);
        NTSTATUS handle_NtSetTimer2(const syscall_context& c, handle timer_handle, uint64_t due_time, uint64_t period, uint64_t parameters);
        NTSTATUS handle_NtSetTimerEx(const syscall_context& c, handle timer_handle, uint32_t timer_set_info_class,
                                     uint64_t timer_set_information, ULONG timer_set_information_length);
        NTSTATUS handle_NtCancelTimer(const syscall_context& c, handle timer_handle, emulator_object<BOOLEAN> current_state);
        NTSTATUS handle_NtCancelTimer2(const syscall_context& c, handle timer_handle, uint64_t parameters);

        // syscalls/token.cpp:
        NTSTATUS
        handle_NtDuplicateToken(const syscall_context&, handle existing_token_handle, ACCESS_MASK /*desired_access*/,
                                emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>>
                                /*object_attributes*/,
                                BOOLEAN /*effective_only*/, TOKEN_TYPE type, emulator_object<handle> new_token_handle);
        NTSTATUS handle_NtQueryInformationToken(const syscall_context& c, handle token_handle,
                                                TOKEN_INFORMATION_CLASS token_information_class, uint64_t token_information,
                                                ULONG token_information_length, emulator_object<ULONG> return_length);
        NTSTATUS handle_NtQuerySecurityAttributesToken(const syscall_context& c, handle token_handle,
                                                       emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> attributes,
                                                       ULONG number_of_attributes, uint64_t buffer, ULONG buffer_length,
                                                       emulator_object<ULONG> return_length);
        NTSTATUS handle_NtAdjustPrivilegesToken(const syscall_context& c, handle token_handle, BOOLEAN disable_all_privileges,
                                                uint64_t new_state, ULONG buffer_length, uint64_t previous_state,
                                                emulator_object<ULONG> return_length);
        NTSTATUS handle_NtFlushInstructionCache(const syscall_context& c, handle process_handle, emulator_object<uint64_t> base_address,
                                                uint64_t region_size);

        // syscalls/license.cpp
        NTSTATUS handle_NtQueryLicenseValue(const syscall_context& c, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> value_name,
                                            emulator_object<uint32_t> type, uint64_t data, uint64_t data_size,
                                            emulator_object<uint32_t> result_data_size);

        // syscalls/user.cpp:
        NTSTATUS handle_NtUserTraceLoggingSendMixedModeTelemetry(const syscall_context& c);
        NTSTATUS handle_NtUserCitSetInfo(const syscall_context& c);
        NTSTATUS handle_NtUserRegisterWindowMessage(const syscall_context& c);
        uint64_t handle_NtUserGetThreadState(const syscall_context& c, ULONG routine);
        uint64_t handle_NtUserSetThreadState(const syscall_context& c, uint64_t value, uint64_t mask);
        uint64_t completion_NtUserGetThreadState(const syscall_context& c, ULONG routine);
        NTSTATUS handle_NtUserProcessConnect(const syscall_context& c, handle process_handle, ULONG length, emulator_pointer user_connect);
        NTSTATUS handle_NtUserInitializeClientPfnArrays(const syscall_context& c, emulator_pointer apfn_client_a,
                                                        emulator_pointer apfn_client_w, emulator_pointer apfn_client_worker,
                                                        emulator_pointer hmod_user);
        uint64_t handle_NtUserRemoteConnectState(const syscall_context& c);
        hdesk handle_NtUserGetThreadDesktop(const syscall_context& c, ULONG thread_id);
        hdc handle_NtUserGetDCEx(const syscall_context& c, hwnd window, uint64_t clip_region, ULONG flags);
        hdc handle_NtUserGetDC(const syscall_context& c, hwnd window);
        hdc handle_NtUserGetWindowDC(const syscall_context& c, hwnd window);
        uint64_t handle_NtUserGetControlBrush(const syscall_context& c, hwnd window, hdc dc, uint32_t control_type);
        BOOL handle_NtUserReleaseDC();
        hwnd handle_NtUserSetCapture(const syscall_context& c, hwnd window);
        BOOL handle_NtUserReleaseCapture(const syscall_context& c);
        BOOL handle_NtUserRegisterRawInputDevices(const syscall_context& c, emulator_pointer devices, uint32_t device_count, uint32_t size);
        uint32_t handle_NtUserGetRawInputData(const syscall_context& c, emulator_pointer raw_input, uint32_t command, emulator_pointer data,
                                              emulator_object<uint32_t> size_ptr, uint32_t header_size);
        BOOL handle_NtUserDefSetText(const syscall_context& c, hwnd window, emulator_object<LARGE_STRING> text);
        BOOL handle_NtUserGetOemBitmapSize(const syscall_context& c, uint32_t bitmap_id, emulator_pointer size_ptr);
        BOOL handle_NtUserSetWindowState(const syscall_context& c, hwnd window, uint32_t flags);
        BOOL handle_NtUserClearWindowState(const syscall_context& c, hwnd window, uint32_t flags);
        BOOL handle_NtUserDisableProcessWindowsGhosting(const syscall_context& c);
        BOOL handle_NtUserBitBltSysBmp(const syscall_context& c, hdc dc, int x, int y, uint32_t bitmap_index);
        BOOL handle_NtUserGetClientRect(const syscall_context& c, hwnd window, emulator_pointer rect_ptr);
        hdc handle_NtUserBeginPaint(const syscall_context& c, hwnd window, emulator_object<EMU_PAINTSTRUCT> paint_struct);
        BOOL handle_NtUserEndPaint(const syscall_context& c, hwnd window, emulator_object<EMU_PAINTSTRUCT> paint_struct);
        BOOL handle_NtUserGetCursorPos(const syscall_context& c, emulator_pointer point_ptr);
        BOOL handle_NtUserTransformPoint(const syscall_context& c, emulator_pointer point, uint32_t from_dpi, uint32_t to_dpi,
                                         uint32_t flags);
        int32_t handle_NtUserShowCursor(const syscall_context& c, BOOL show);
        uint32_t handle_NtUserGetKeyState(const syscall_context& c, int32_t virtual_key);
        uint32_t handle_NtUserGetAsyncKeyState(const syscall_context& c, int32_t virtual_key);
        BOOL handle_NtUserClipCursor(const syscall_context& c, emulator_pointer rect);
        BOOL handle_NtUserSetCursorPos(const syscall_context& c, int32_t x, int32_t y);
        hcursor handle_NtUserSetCursor(const syscall_context& c, hcursor cursor);
        hcursor handle_NtUserGetCursor(const syscall_context& c);
        hicon handle_NtUserCreateEmptyCursorObject();
        BOOL handle_NtUserSetCursorIconData();
        BOOL handle_NtUserSetCursorIconDataEx();
        BOOL handle_NtUserGetRequiredCursorSizes();
        NTSTATUS handle_NtUserFindExistingCursorIcon();
        BOOL handle_NtUserDestroyCursor(const syscall_context& c, hicon icon, DWORD flags);
        hicon handle_NtUserGetCursorFrameInfo(const syscall_context& c, hicon icon, UINT frame, emulator_object<uint32_t> rate_jiffies,
                                              emulator_object<uint32_t> frame_count);
        BOOL handle_NtUserGetIconSize(const syscall_context& c, hicon icon, UINT frame, emulator_object<int> cx, emulator_object<int> cy);
        BOOL handle_NtUserDrawIconEx(const syscall_context& c, hdc dc, int x, int y, hicon icon, int cx, int cy, UINT istep,
                                     uint64_t flicker_brush, UINT di_flags);
        BOOL handle_NtUserMessageBeep();
        uint64_t handle_NtUserFindWindowEx(const syscall_context& c, hwnd parent, hwnd child_after,
                                           emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name,
                                           emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> window_name);
        BOOL handle_NtUserMoveWindow(const syscall_context& c, hwnd hwnd, int x, int y, int width, int height, BOOL repaint);
        uint64_t handle_NtUserGetProcessWindowStation();
        uint64_t handle_NtUserCallHwndParam(const syscall_context& c, hwnd hwnd, uint64_t param, uint32_t code);
        uint16_t handle_NtUserRegisterClassExWOW(const syscall_context& c, emulator_object<EMU_WNDCLASSEX> wnd_class_ex,
                                                 emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name,
                                                 emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_version,
                                                 emulator_object<CLSMENUNAME<EmulatorTraits<Emu64>>> class_menu_name, DWORD function_id,
                                                 DWORD flags, emulator_pointer wow);
        BOOL handle_NtUserUnregisterClass(const syscall_context& c, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name,
                                          emulator_pointer instance, emulator_object<CLSMENUNAME<EmulatorTraits<Emu64>>> class_menu_name);
        BOOL handle_NtUserGetClassInfoEx(const syscall_context& c, hinstance /*instance*/,
                                         emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name,
                                         emulator_object<EMU_WNDCLASSEX> wnd_class_ex, emulator_pointer menu_name, BOOL /*ansi*/);
        int handle_NtUserGetClassName(const syscall_context& c, hwnd win_hwnd, BOOL real,
                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name);
        NTSTATUS handle_NtUserSetWindowsHookEx(const syscall_context& c);
        NTSTATUS handle_NtUserUnhookWindowsHookEx(const syscall_context& c);
        hwnd handle_NtUserCreateWindowEx(const syscall_context& c, DWORD ex_style, emulator_object<LARGE_STRING> class_name,
                                         emulator_object<LARGE_STRING> cls_version, emulator_object<LARGE_STRING> window_name, DWORD style,
                                         int x, int y, int width, int height, hwnd parent, hmenu menu, hinstance instance, pointer l_param,
                                         DWORD flags, pointer acbi_buffer);
        hwnd completion_NtUserCreateWindowEx(const syscall_context& c, DWORD ex_style, emulator_object<LARGE_STRING> class_name,
                                             emulator_object<LARGE_STRING> cls_version, emulator_object<LARGE_STRING> window_name,
                                             DWORD style, int x, int y, int width, int height, hwnd parent, hmenu menu, hinstance instance,
                                             pointer l_param, DWORD flags, pointer acbi_buffer);
        BOOL handle_NtUserDestroyWindow(const syscall_context& c, hwnd window);
        BOOL completion_NtUserDestroyWindow(const syscall_context& c, hwnd window);
        BOOL handle_NtUserSetProp(const syscall_context& c, hwnd window, uint16_t atom, uint64_t data);
        BOOL handle_NtUserSetProp2(const syscall_context& c, hwnd window, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> str,
                                   uint64_t data);
        uint64_t handle_NtUserChangeWindowMessageFilterEx();
        BOOL handle_NtUserShowWindow(const syscall_context& c, hwnd hwnd, LONG cmd_show);
        BOOL completion_NtUserShowWindow(const syscall_context& c, hwnd hwnd, LONG cmd_show);
        uint64_t handle_NtUserMessageCall(const syscall_context& c, hwnd hwnd, UINT msg, uint64_t w_param, uint64_t l_param,
                                          uint64_t result_info, DWORD type, BOOL ansi);
        uint64_t completion_NtUserMessageCall(const syscall_context& c, hwnd hwnd, UINT msg, uint64_t w_param, uint64_t l_param,
                                              uint64_t result_info, DWORD type, BOOL ansi);
        uint64_t handle_NtUserDispatchMessage(const syscall_context& c, emulator_object<msg> message);
        BOOL handle_NtUserTranslateMessage(const syscall_context& c, emulator_object<msg> message, UINT flags);
        BOOL handle_NtUserGetMessage(const syscall_context& c, emulator_object<msg> message, hwnd hwnd, UINT msg_filter_min,
                                     UINT msg_filter_max);
        BOOL handle_NtUserPeekMessage(const syscall_context& c, emulator_object<msg> message, hwnd hwnd, UINT msg_filter_min,
                                      UINT msg_filter_max, UINT remove_message);
        BOOL handle_NtUserWaitMessage(const syscall_context& c);
        BOOL handle_NtUserInvalidateRect(const syscall_context& c, hwnd hwnd, emulator_object<RECT> rect, BOOL erase);
        BOOL handle_NtUserValidateRect(const syscall_context& c, hwnd hwnd, emulator_object<RECT> rect);
        BOOL handle_NtUserUpdateWindow(const syscall_context& c, hwnd hwnd);
        int32_t handle_NtUserGetKeyNameText(const syscall_context& c, int32_t l_param, emulator_pointer buffer, int32_t character_count);
        BOOL handle_NtUserPostMessage(const syscall_context& c, hwnd hwnd, UINT msg, uint64_t wParam, uint64_t lParam);
        BOOL handle_NtUserPostThreadMessage(const syscall_context& c, DWORD id_thread, UINT msg, uint64_t wParam, uint64_t lParam);
        BOOL handle_NtUserPostQuitMessage(const syscall_context& c, int exit_code);
        NTSTATUS handle_NtUserEnumDisplayDevices(const syscall_context& c,
                                                 emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> str_device, DWORD dev_num,
                                                 emulator_object<EMU_DISPLAY_DEVICEW> display_device, DWORD flags);
        NTSTATUS handle_NtUserEnumDisplaySettings(const syscall_context& c,
                                                  emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> device_name, DWORD mode_num,
                                                  emulator_object<EMU_DEVMODEW> dev_mode, DWORD flags);
        LONG handle_NtUserChangeDisplaySettings(const syscall_context& c,
                                                emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> device_name,
                                                emulator_object<EMU_DEVMODEW> dev_mode, hwnd window, DWORD flags, uint64_t param);
        NTSTATUS handle_NtUserBuildHwndList(const syscall_context& c, hdesk desktop, hwnd hwnd_next, BOOL children, BOOL remove_immersive,
                                            DWORD thread_id, UINT hwnd_max, emulator_pointer hwnd_list, emulator_object<UINT> hwnd_needed);
        BOOL handle_NtUserEnumDisplayMonitors(const syscall_context& c, hdc hdc_in, uint64_t clip_rect_ptr, uint64_t callback,
                                              uint64_t param);
        BOOL completion_NtUserEnumDisplayMonitors(const syscall_context& c, hdc hdc_in, uint64_t clip_rect_ptr, uint64_t callback,
                                                  uint64_t param);
        BOOL handle_NtUserGetHDevName(const syscall_context& c, handle hdev, emulator_pointer device_name);
        // Minimal stub: DWM/compositor redirection-info query, undocumented signature. Always reports
        // "not redirected" (FALSE) -- real d3d9.dll (Present's pre-flight window-state check) treats
        // this as a benign fallback to the non-composited path. Real args beyond hwnd are unread.
        BOOL handle_NtUserHwndQueryRedirectionInfo(const syscall_context& c, hwnd window);
        BOOL handle_NtUserGetMonitorInfo(const syscall_context& c, handle hmonitor, emulator_pointer pmi);
        emulator_pointer handle_NtUserMapDesktopObject(const syscall_context& c, handle handle);
        BOOL handle_NtUserTransformRect(const syscall_context& c, emulator_object<RECT> rect, hwnd hwnd, uint32_t type, uint64_t unknown);
        BOOL handle_NtUserSetWindowPos(const syscall_context& c, hwnd hWnd, hwnd hwnd_insert_after, int x, int y, int cx, int cy,
                                       UINT flags);
        NTSTATUS handle_NtUserSetForegroundWindow(const syscall_context& c);
        hwnd handle_NtUserGetForegroundWindow(const syscall_context& c);
        hwnd handle_NtUserSetFocus(const syscall_context& c, hwnd hwnd);
        emulator_pointer handle_NtUserSetWindowLongPtr(const syscall_context& c, handle hWnd, int nIndex, emulator_pointer dwNewLong,
                                                       BOOL Ansi);
        emulator_pointer handle_NtUserSetClassLongPtr(const syscall_context& c, handle hWnd, int nIndex, emulator_pointer dwNewLong,
                                                      BOOL Ansi);
        uint32_t handle_NtUserSetWindowLong(const syscall_context& c, handle hWnd, int nIndex, uint32_t dwNewLong, BOOL Ansi);
        emulator_pointer handle_NtUserGetWindowLongPtr(const syscall_context& c, handle hWnd, int nIndex, BOOL Ansi);
        uint32_t handle_NtUserGetWindowLong(const syscall_context& c, handle hWnd, int nIndex, BOOL Ansi);
        uint64_t handle_NtUserGetAncestor(const syscall_context& c, hwnd child_hwnd, UINT flags);
        BOOL handle_NtUserRedrawWindow(const syscall_context& c, hwnd hwnd, emulator_object<RECT> update_rect, uint64_t update_rgn,
                                       UINT flags);
        NTSTATUS handle_NtUserGetCPD(const syscall_context& c);
        BOOL handle_NtUserSetWindowFNID(const syscall_context& c, hwnd hwnd, WORD fnid);
        BOOL handle_NtUserSetDialogPointer(const syscall_context& c, hwnd hwnd, emulator_pointer ptr);
        BOOL handle_NtUserSetDialogSystemMenu(const syscall_context& c, hwnd hwnd);
        BOOL handle_NtUserSetMsgBox(const syscall_context& c, hwnd hwnd);
        BOOL handle_NtUserEnableWindow(const syscall_context& c, hwnd hwnd, BOOL enable);
        BOOL handle_NtUserDeleteMenu(const syscall_context& c, uint64_t menu, UINT position, UINT flags);
        uint64_t handle_NtUserGetSystemMenu(const syscall_context& c, hwnd hwnd, BOOL revert);
        BOOL handle_NtUserAllowSetForegroundWindow();
        ULONG handle_NtUserGetAtomName(const syscall_context& c, RTL_ATOM atom,
                                       emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> atom_name);
        NTSTATUS handle_NtUserGetDisplayConfigBufferSizes(const syscall_context& c, UINT32 flags, emulator_pointer counts_buffer);
        NTSTATUS handle_NtUserQueryDisplayConfig(const syscall_context& c, UINT32 flags, emulator_pointer num_elements,
                                                 emulator_pointer path_array, emulator_pointer mode_array,
                                                 emulator_pointer current_topology_id);
        NTSTATUS handle_NtUserDisplayConfigGetDeviceInfo(const syscall_context& c, emulator_pointer packet);
        uint64_t handle_NtUserInitThreadCoreMessagingIocp2(const syscall_context& c, handle window_handle,
                                                           emulator_object<uint32_t> completion_queue_index);
        BOOL handle_NtUserDrainThreadCoreMessagingCompletions2();
        uint64_t handle_NtUserScheduleDispatchNotification(const syscall_context& c, hwnd hwnd);
        uint64_t handle_NtUserSetTimer(const syscall_context& c, hwnd hwnd, uint64_t timer_id, uint32_t elapsed_ms, uint64_t timer_proc);
        BOOL handle_NtUserKillTimer(const syscall_context& c, hwnd hwnd, uint64_t timer_id);
        BOOL handle_NtUserValidateTimerCallback(const syscall_context& c, uint64_t timer_proc);
        uint32_t handle_NtUserGetQueueStatusReadonly(const syscall_context& c, UINT flags);
        uint32_t handle_NtUserGetQueueStatus(const syscall_context& c, UINT flags);
        NTSTATUS handle_NtUserCreateAcceleratorTable();
        int32_t handle_NtUserTranslateAccelerator();
        hmenu handle_NtUserCreateMenu(const syscall_context& c);
        BOOL handle_NtUserThunkedMenuItemInfo(const syscall_context& c, hmenu menu, UINT position, BOOL by_position, BOOL insert,
                                              emulator_object<EMU_MENUITEMINFO> item_info,
                                              emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> item_text);
        hmenu handle_NtUserCreatePopupMenu(const syscall_context& c);
        BOOL handle_NtUserSetMenu();
        BOOL handle_NtUserRemoveMenu(const syscall_context& c, hmenu menu, UINT position, UINT flags);
        BOOL handle_NtUserDestroyMenu(const syscall_context& c, hmenu menu);
        BOOL handle_NtUserDrawMenuBar(const syscall_context& c, hwnd hwnd);
        BOOL handle_NtUserSetWindowCompositionAttribute(const syscall_context& c, hwnd hwnd, emulator_pointer data);
        BOOL handle_NtUserCreateCaret();
        BOOL handle_NtUserDestroyCaret();
        BOOL handle_NtUserSetCaretPos();
        BOOL handle_NtUserShowCaret();
        BOOL handle_NtUserHideCaret();
        BOOL handle_NtUserGetObjectInformation();
        uint64_t handle_NtUserQueryWindow(const syscall_context& c, hwnd window_handle, uint32_t query_type);
        int handle_NtUserSetScrollInfo();
        BOOL handle_NtUserIsTouchWindow();
        BOOL handle_NtUserGetWindowPlacement();
        BOOL handle_NtUserTrackMouseEvent();
        BOOL handle_NtUserSetWindowRgn();
        BOOL handle_NtUserAlterWindowStyle();
        BOOL handle_NtUserSetActiveWindow();
        NTSTATUS handle_NtUserSelectPalette();
        BOOL handle_NtUserSwapMouseButton();
        hwnd handle_NtUserWindowFromPoint(const syscall_context& c, int32_t x, int32_t y);

        // syscalls/gdi.cpp:
        NTSTATUS handle_NtDxgkIsFeatureEnabled(const syscall_context& c);
        NTSTATUS handle_NtGdiInit(const syscall_context& c);
        NTSTATUS handle_NtGdiInit2(const syscall_context& c);
        uint32_t handle_NtGdiGetDeviceCaps(const syscall_context& c, hdc dc, uint32_t index);
        uint32_t handle_NtGdiGetDeviceCapsAll(const syscall_context& c, hdc dc, emulator_pointer caps);
        uint32_t handle_NtGdiComputeXformCoefficients(const syscall_context& c, hdc dc);
        BOOL handle_NtGdiFlush(const syscall_context& c);
        uint64_t handle_NtGdiCreateSolidBrush(const syscall_context& c, uint32_t color, uint64_t unused);
        uint64_t handle_NtGdiCreatePatternBrushInternal(const syscall_context& c, handle bitmap, uint32_t unused);
        uint64_t handle_NtGdiCreatePen(const syscall_context& c, uint32_t style, uint32_t width, uint32_t color);
        uint64_t handle_NtGdiCreatePaletteInternal(const syscall_context& c);
        uint64_t handle_NtGdiCreateHalftonePalette(const syscall_context& c);
        NTSTATUS handle_NtGdiDoPalette();
        uint64_t handle_NtGdiCreateCompatibleDC(const syscall_context& c, hdc dc);
        int32_t handle_NtGdiSaveDC(const syscall_context& c, hdc dc);
        BOOL handle_NtGdiRestoreDC(const syscall_context& c, hdc dc, int32_t saved_dc);
        uint64_t handle_NtGdiAddFontMemResourceEx(const syscall_context& c, emulator_pointer buffer, uint32_t buffer_size,
                                                  emulator_pointer design_vector, uint32_t design_vector_size,
                                                  emulator_object<uint32_t> num_fonts);
        BOOL handle_NtGdiRemoveFontMemResourceEx(const syscall_context& c, uint64_t font_handle);
        uint64_t handle_NtGdiCreateCompatibleBitmap(const syscall_context& c, hdc dc, uint32_t width, uint32_t height);
        uint64_t handle_NtGdiCreateBitmap(const syscall_context& c, uint32_t width, uint32_t height, uint32_t planes, uint32_t bits_pixel,
                                          emulator_pointer bits);
        uint64_t handle_NtGdiCreateDIBSection(const syscall_context& c, hdc dc, uint64_t section_app, uint32_t offset,
                                              emulator_pointer info, uint32_t usage, uint32_t header_size, uint32_t flags,
                                              uint64_t color_space, emulator_object<emulator_pointer> bits);
        uint64_t handle_NtGdiCreateDIBitmapInternal(const syscall_context& c, hdc dc, uint32_t width, uint32_t height, uint32_t usage,
                                                    emulator_pointer bits, emulator_pointer info, uint32_t info_header_size, uint32_t init,
                                                    uint32_t offset, uint32_t cj, uint32_t i_usage);
        int handle_NtGdiSetDIBitsToDeviceInternal(const syscall_context& c, hdc dc, int x_dest, int y_dest, uint32_t width, uint32_t height,
                                                  int x_src, int y_src, uint32_t start_scan, uint32_t scan_lines, emulator_pointer bits,
                                                  emulator_pointer info, uint32_t color_use, uint32_t max_bits, uint32_t max_info,
                                                  uint32_t transform_coordinates, uint64_t color_transform);
        int handle_NtGdiGetDIBitsInternal(const syscall_context& c, hdc dc, handle bitmap, uint32_t start_scan, uint32_t scan_lines,
                                          emulator_pointer bits, emulator_pointer info, uint32_t usage, uint32_t max_bits,
                                          uint32_t max_info);
        LONG handle_NtGdiGetBitmapBits(const syscall_context& c, handle bitmap, LONG cb_buffer, emulator_pointer bits);
        int handle_NtGdiStretchDIBitsInternal(const syscall_context& c, hdc dc, int x_dst, int y_dst, int dst_width, int dst_height,
                                              int x_src, int y_src, int src_width, int src_height, emulator_pointer bits,
                                              emulator_pointer info, uint32_t usage, uint32_t rop, uint32_t max_info, uint32_t max_bits,
                                              uint64_t color_transform);
        uint32_t handle_NtGdiDeleteObjectApp(const syscall_context& c, uint32_t handle_value);
        uint64_t handle_NtGdiSelectBitmap(const syscall_context& c, hdc dc, handle bitmap);
        uint64_t handle_NtGdiSelectFont(const syscall_context& c, hdc dc, uint64_t font);
        hdc handle_NtGdiGetDCforBitmap(const syscall_context& c, handle bitmap);
        BOOL handle_NtGdiGetDCDword(const syscall_context& c, hdc dc, uint32_t index, emulator_pointer result);
        BOOL handle_NtGdiSetBrushOrg(const syscall_context& c, hdc dc, int x, int y, emulator_pointer prev);
        uint64_t handle_NtGdiHfontCreate(const syscall_context& c, emulator_pointer logfont, uint32_t angle);
        uint32_t handle_NtGdiExtGetObjectW(const syscall_context& c, uint32_t handle_value, uint32_t size, emulator_pointer buffer);
        uint32_t handle_NtGdiEnumFonts();
        uint32_t handle_NtGdiGetTextCharsetInfo(const syscall_context& c, hdc dc, emulator_pointer sig, uint32_t flags);
        uint32_t handle_NtGdiQueryFontAssocInfo(const syscall_context& c, hdc dc);
        uint32_t handle_NtGdiGetTextMetricsW(const syscall_context& c, hdc dc, emulator_pointer ptm, uint32_t cj);
        int32_t handle_NtGdiGetTextFaceW(const syscall_context& c, hdc dc, int32_t count, emulator_pointer face_name, BOOL alias_name);
        uint32_t handle_NtGdiGetGlyphOutline(const syscall_context& c, hdc dc, UINT character, UINT format, emulator_pointer glyph_metrics,
                                             DWORD buffer_size, emulator_pointer buffer, emulator_pointer mat2);
        uint32_t handle_NtGdiGetOutlineTextMetricsInternalW(const syscall_context& c, hdc dc, uint32_t cj_copy, emulator_pointer metrics,
                                                            emulator_pointer unknown);
        BOOL handle_NtGdiGetTextExtent(const syscall_context& c, hdc dc, emulator_pointer text, int32_t char_count, emulator_pointer size,
                                       ULONG flags);
        BOOL handle_NtGdiGetCharWidthW(const syscall_context& c, hdc dc, UINT first_char, UINT char_count, emulator_pointer chars,
                                       UINT flags, emulator_pointer buffer);
        NTSTATUS handle_NtGdiExtCreateRegion();
        NTSTATUS handle_NtGdiTransparentBlt();
        uint64_t handle_NtGdiCreateRectRgn(const syscall_context& c, LONG x_left, LONG y_top, LONG x_right, LONG y_bottom);
        int32_t handle_NtGdiGetRandomRgn(const syscall_context& c, hdc dc, uint64_t region, LONG index);
        int32_t handle_NtGdiIntersectClipRect(const syscall_context& c, hdc dc, LONG x_left, LONG y_top, LONG x_right, LONG y_bottom);
        uint32_t handle_NtGdiGetCharSet(const syscall_context& c, hdc dc);
        int32_t handle_NtGdiExtSelectClipRgn(const syscall_context& c, hdc dc, uint64_t region, LONG mode);
        BOOL handle_NtGdiLineTo(const syscall_context& c, hdc dc, LONG x_end, LONG y_end);
        BOOL handle_NtGdiRectangle(const syscall_context& c, hdc dc, LONG left, LONG top, LONG right, LONG bottom);
        BOOL handle_NtGdiPatBlt(const syscall_context& c, hdc dc, LONG x, LONG y, LONG width, LONG height, DWORD rop);
        COLORREF handle_NtGdiSetPixel(const syscall_context& c, hdc dc, int x, int y, COLORREF color);
        COLORREF handle_NtGdiGetPixel(const syscall_context& c, hdc dc, int x, int y);
        BOOL handle_NtGdiBitBlt(const syscall_context& c, hdc dst_dc, int x_dst, int y_dst, int width, int height, hdc src_dc, int x_src,
                                int y_src, DWORD rop, DWORD cr_back_color, FLONG fl);
        BOOL handle_NtGdiStretchBlt(const syscall_context& c, hdc dst_dc, int x_dst, int y_dst, int w_dst, int h_dst, hdc src_dc, int x_src,
                                    int y_src, int w_src, int h_src, DWORD rop, DWORD cr_back_color);
        BOOL handle_NtGdiPolyPatBlt(const syscall_context& c, hdc dc, DWORD rop, emulator_pointer poly, DWORD count, DWORD mode);
        BOOL handle_NtGdiExtTextOutW(const syscall_context& c, hdc dc, LONG x, LONG y, UINT options, emulator_pointer rect,
                                     emulator_pointer text, UINT count, emulator_pointer dx, DWORD code_page);
        BOOL handle_NtGdiGetRealizationInfo(const syscall_context& c, hdc dc, emulator_pointer realization_info, uint64_t font);
        NTSTATUS handle_NtGdiGetEntry(const syscall_context& c, uint32_t handle_value, emulator_pointer entry_ptr);
        NTSTATUS handle_NtGdiSetLayout(const syscall_context& c);
        NTSTATUS handle_NtGdiGetDCObject(const syscall_context& c);
        BOOL handle_NtGdiMoveToEx(const syscall_context& c, hdc dc, LONG x, LONG y, emulator_pointer old_point_ptr);
        uint64_t handle_NtGdiSelectBrushLocal(const syscall_context& c, hdc dc, uint32_t brush, emulator_pointer old_brush_ptr);
        uint64_t handle_NtGdiSelectPenLocal(const syscall_context& c, hdc dc, uint32_t pen, emulator_pointer old_pen_ptr);
        hdc handle_NtGdiOpenDCW(const syscall_context& c);
        NTSTATUS handle_NtGdiDdDDIEnumAdapters2(const syscall_context& c, emulator_object<EMU_D3DKMT_ENUMADAPTERS2> enum_adapters);
        NTSTATUS handle_NtDxgkEnumAdapters3(const syscall_context& c, emulator_object<EMU_D3DKMT_ENUMADAPTERS3> enum_adapters);
        NTSTATUS handle_NtDxgkGetProperties(const syscall_context& c, emulator_object<EMU_D3DKMT_GET_PROPERTIES> get_properties);
        NTSTATUS handle_NtGdiDdDDICloseAdapter(const syscall_context& c);
        NTSTATUS handle_NtGdiDdDDIQueryAdapterInfo(const syscall_context& c, emulator_object<EMU_D3DKMT_QUERYADAPTERINFO> query_adapter);
        NTSTATUS handle_NtGdiDdDDICreateDevice(const syscall_context& c, emulator_object<EMU_D3DKMT_CREATEDEVICE> device_desc);
        NTSTATUS handle_NtGdiDdDDICreatePagingQueue(const syscall_context& c, emulator_object<EMU_D3DKMT_CREATEPAGINGQUEUE> queue_desc);
        NTSTATUS handle_NtGdiDdDDICreateSynchronizationObject(const syscall_context& c,
                                                              emulator_object<EMU_D3DKMT_CREATESYNCHRONIZATIONOBJECT> sync_desc);
        NTSTATUS handle_NtGdiDdDDILock2(const syscall_context& c, emulator_object<EMU_D3DKMT_LOCK2> lock_desc);
        NTSTATUS handle_NtGdiGetCurrentDpiInfo(const syscall_context& c, uint64_t hDC, emulator_object<EMU_CURRENT_DPI_INFO> dpi_info);
        NTSTATUS handle_NtGdiDdDDIEscape(const syscall_context& c, emulator_object<EMU_D3DKMT_ESCAPE> escape_desc);
        NTSTATUS handle_NtGdiDdDDICreateContext(const syscall_context& c, emulator_object<EMU_D3DKMT_CREATECONTEXT> context_desc);
        NTSTATUS handle_NtGdiDdDDICreateAllocation(const syscall_context& c, emulator_object<EMU_D3DKMT_CREATEALLOCATION> allocation_desc);
        NTSTATUS handle_NtGdiDdDDIQueryResourceInfo(const syscall_context& c, emulator_object<EMU_D3DKMT_QUERYRESOURCEINFO> resource_info);
        NTSTATUS handle_NtGdiDdDDIOpenResource(const syscall_context& c, emulator_object<EMU_D3DKMT_OPENRESOURCE> open_resource);
        NTSTATUS handle_NtGdiDdDDILock(const syscall_context& c, emulator_object<EMU_D3DKMT_LOCK> lock_desc);
        NTSTATUS handle_NtGdiDdDDIUnlock(const syscall_context& c);
        NTSTATUS handle_NtGdiDdDDIGetDisplayModeList(const syscall_context& c,
                                                     emulator_object<EMU_D3DKMT_GETDISPLAYMODELIST> display_mode_list);
        NTSTATUS handle_NtGdiDdDDIGetSharedPrimaryHandle(const syscall_context& c,
                                                         emulator_object<EMU_D3DKMT_GETSHAREDPRIMARYHANDLE> shared_primary);
        NTSTATUS handle_NtGdiDdDDIGetDeviceState(const syscall_context& c, emulator_object<EMU_D3DKMT_GETDEVICESTATE> device_state);
        NTSTATUS handle_NtGdiDdDDIMarkDeviceAsError(const syscall_context& c, emulator_object<EMU_D3DKMT_MARKDEVICEASERROR> mark_error);
        NTSTATUS handle_NtGdiDdDDIGetCachedHybridQueryValue(const syscall_context& c, emulator_object<uint32_t> value);
        NTSTATUS handle_NtGdiDdDDICacheHybridQueryValue(const syscall_context& c);
        NTSTATUS handle_NtGdiDdDDIDestroyAllocation2(const syscall_context& c,
                                                     emulator_object<EMU_D3DKMT_DESTROYALLOCATION2> destroy_allocation);
        NTSTATUS handle_NtGdiDdDDIDestroyAllocation(const syscall_context& c,
                                                    emulator_object<EMU_D3DKMT_DESTROYALLOCATION> destroy_allocation);
        NTSTATUS handle_NtGdiDdDDIDestroyContext();
        NTSTATUS handle_NtGdiDdDDIDestroyDevice();
        NTSTATUS handle_NtGdiDdDDIOpenAdapterFromLuid(const syscall_context& c,
                                                      emulator_object<EMU_D3DKMT_OPENADAPTERFROMLUID> open_adapter);
        NTSTATUS handle_NtGdiDdDDIOpenAdapterFromHdc(const syscall_context& c, emulator_object<EMU_D3DKMT_OPENADAPTERFROMHDC> open_adapter);
        NTSTATUS handle_NtGdiDdDDISubmitCommand(const syscall_context& c, emulator_object<EMU_D3DKMT_SUBMITCOMMAND> submit_desc);
        NTSTATUS handle_NtGdiDdDDIPresent(const syscall_context& c, emulator_object<EMU_D3DKMT_PRESENT> present_desc);

        // syscalls/trace.cpp:
        NTSTATUS handle_NtTraceControl(const syscall_context& c, ULONG function_code, uint64_t input_buffer, ULONG input_buffer_length,
                                       uint64_t output_buffer, ULONG output_buffer_length, emulator_object<ULONG> return_length);

        // syscalls/io_completion.cpp:
        NTSTATUS handle_NtCreateIoCompletion(const syscall_context& c, emulator_object<handle> io_completion_handle,
                                             ACCESS_MASK desired_access,
                                             emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                             ULONG number_of_concurrent_threads);
        NTSTATUS handle_NtSetIoCompletion(const syscall_context& c, handle io_completion_handle, emulator_pointer key_context,
                                          emulator_pointer apc_context, NTSTATUS io_status,
                                          EMULATOR_CAST(EmulatorTraits<Emu64>::ULONG_PTR, ULONG_PTR) io_status_information);
        NTSTATUS handle_NtSetIoCompletionEx(const syscall_context& c, handle io_completion_handle, handle io_completion_packet_handle,
                                            emulator_pointer key_context, emulator_pointer apc_context, NTSTATUS io_status,
                                            EMULATOR_CAST(EmulatorTraits<Emu64>::ULONG_PTR, ULONG_PTR) io_status_information);
        NTSTATUS handle_NtRemoveIoCompletion(const syscall_context& c, handle io_completion_handle,
                                             emulator_object<emulator_pointer> key_context, emulator_object<emulator_pointer> apc_context,
                                             emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                             emulator_object<LARGE_INTEGER> timeout);
        NTSTATUS handle_NtRemoveIoCompletionEx(
            const syscall_context& c, handle io_completion_handle,
            emulator_object<FILE_IO_COMPLETION_INFORMATION<EmulatorTraits<Emu64>>> io_completion_information, ULONG count,
            emulator_object<ULONG> num_entries_removed, emulator_object<LARGE_INTEGER> timeout, BOOLEAN alertable);
        NTSTATUS handle_NtCreateWaitCompletionPacket(const syscall_context& c, emulator_object<handle> wait_packet_handle,
                                                     ACCESS_MASK desired_access,
                                                     emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes);
        NTSTATUS handle_NtAssociateWaitCompletionPacket(const syscall_context& c, handle wait_completion_packet_handle,
                                                        handle io_completion_handle, handle target_object_handle,
                                                        emulator_pointer key_context, emulator_pointer apc_context, NTSTATUS io_status,
                                                        EMULATOR_CAST(EmulatorTraits<Emu64>::ULONG_PTR, ULONG_PTR) io_status_information,
                                                        emulator_object<BOOLEAN> already_signaled);
        NTSTATUS handle_NtCancelWaitCompletionPacket(const syscall_context& c, handle wait_completion_packet_handle,
                                                     BOOLEAN remove_signaled_packet);

        // syscalls/worker_factory.cpp:
        NTSTATUS handle_NtCreateWorkerFactory(const syscall_context& c, emulator_object<handle> worker_factory_handle,
                                              ACCESS_MASK desired_access,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              handle io_completion_handle, handle worker_process_handle, emulator_pointer start_routine,
                                              emulator_pointer start_parameter, ULONG max_thread_count,
                                              EMULATOR_CAST(EmulatorTraits<Emu64>::SIZE_T, SIZE_T) stack_reserve,
                                              EMULATOR_CAST(EmulatorTraits<Emu64>::SIZE_T, SIZE_T) stack_commit);
        NTSTATUS handle_NtWorkerFactoryWorkerReady(const syscall_context& c, handle worker_factory_handle);
        NTSTATUS handle_NtSetInformationWorkerFactory(const syscall_context& c, handle worker_factory_handle,
                                                      WORKERFACTORYINFOCLASS info_class, emulator_pointer worker_factory_information,
                                                      ULONG worker_factory_information_length);
        NTSTATUS handle_NtShutdownWorkerFactory(const syscall_context& c, handle worker_factory_handle,
                                                emulator_object<LONG> pending_worker_count);
        NTSTATUS handle_NtReleaseWorkerFactoryWorker(const syscall_context& c, handle worker_factory_handle);
        NTSTATUS handle_NtWaitForWorkViaWorkerFactory(const syscall_context& c, handle worker_factory_handle,
                                                      emulator_object<FILE_IO_COMPLETION_INFORMATION<EmulatorTraits<Emu64>>> mini_packets,
                                                      ULONG count, emulator_object<ULONG> packets_returned, emulator_pointer deferred_work);

        NTSTATUS handle_NtQueryPerformanceCounter(const syscall_context& c, const emulator_object<LARGE_INTEGER> performance_counter,
                                                  const emulator_object<LARGE_INTEGER> performance_frequency)
        {
            try
            {
                if (performance_counter)
                {
                    performance_counter.access([&](LARGE_INTEGER& value) {
                        value.QuadPart = c.win_emu.clock().steady_now().time_since_epoch().count(); //
                    });
                }

                if (performance_frequency)
                {
                    performance_frequency.access([&](LARGE_INTEGER& value) {
                        value.QuadPart = c.proc.kusd.get().QpcFrequency; //
                    });
                }

                return STATUS_SUCCESS;
            }
            catch (...)
            {
                return STATUS_ACCESS_VIOLATION;
            }
        }

        NTSTATUS handle_NtManageHotPatch(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtApphelpCacheControl(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDeviceIoControlFile(const syscall_context& c, const handle file_handle, const handle event,
                                              const emulator_pointer /*PIO_APC_ROUTINE*/ apc_routine, const emulator_pointer apc_context,
                                              const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                              const ULONG io_control_code, const emulator_pointer input_buffer,
                                              const ULONG input_buffer_length, const emulator_pointer output_buffer,
                                              const ULONG output_buffer_length)
        {
            auto* device = c.proc.devices.get(file_handle);
            if (!device)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (auto* e = c.proc.events.get(event))
            {
                e->signaled = false;
            }

            io_device_context context{c.emu};
            context.event = event;
            context.apc_routine = apc_routine;
            context.apc_context = apc_context;
            context.io_status_block = io_status_block;
            context.io_control_code = io_control_code;
            context.input_buffer = input_buffer;
            context.input_buffer_length = input_buffer_length;
            context.output_buffer = output_buffer;
            context.output_buffer_length = output_buffer_length;

            return device->execute_ioctl(c.win_emu, context);
        }

        NTSTATUS handle_NtQueryWnfStateData(const syscall_context& c, uint64_t state_name, uint64_t /*type_id*/,
                                            uint64_t /*explicit_scope*/, emulator_object<uint32_t> change_stamp, uint64_t /*buffer*/,
                                            emulator_object<uint32_t> buffer_size)
        {
            if (getenv("EMULATOR_LOG_RPC"))
            {
                c.win_emu.log.error("[wnf-dbg] NtQueryWnfStateData name=0x%llx\n", (unsigned long long)state_name);
            }
            if (change_stamp)
            {
                change_stamp.write(1);
            }
            if (buffer_size)
            {
                buffer_size.write(0);
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryWnfStateNameInformation(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtTestAlert(const syscall_context& c)
        {
            c.win_emu.yield_thread(true);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUserSystemParametersInfo(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUpdateWnfStateData(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateWnfStateName(const syscall_context& /*c*/, uint64_t /*state_name*/, uint32_t /*name_lifetime*/,
                                             uint32_t /*data_scope*/, BOOLEAN /*persist_data*/, uint64_t /*type_id*/,
                                             ULONG /*maximum_state_size*/, uint64_t /*security_descriptor*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDeleteWnfStateName(const syscall_context& /*c*/, uint64_t /*state_name*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDeleteWnfStateData(const syscall_context& /*c*/, uint64_t /*state_name*/, uint64_t /*explicit_scope*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFlushKey(const syscall_context& /*c*/, handle /*key_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlertThread(const syscall_context& /*c*/, handle /*thread_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlertResumeThread(const syscall_context& /*c*/, handle /*thread_handle*/,
                                            emulator_object<ULONG> previous_suspend_count)
        {
            previous_suspend_count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlertMultipleThreadByThreadId(const syscall_context& /*c*/, uint64_t /*thread_ids*/, ULONG /*thread_count*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtExtendSection(const syscall_context& /*c*/, handle /*section_handle*/,
                                        emulator_object<LARGE_INTEGER> /*new_section_size*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtIsProcessInJob(const syscall_context& /*c*/, handle /*process_handle*/, handle /*job_handle*/)
        {
            return static_cast<NTSTATUS>(0x00000124L); // STATUS_PROCESS_NOT_IN_JOB
        }

        NTSTATUS handle_NtGetNextProcess(const syscall_context& /*c*/, handle /*process_handle*/, ACCESS_MASK /*desired_access*/,
                                         ULONG /*handle_attributes*/, ULONG /*flags*/, emulator_object<handle> /*new_process_handle*/)
        {
            return STATUS_NO_MORE_ENTRIES;
        }

        NTSTATUS handle_NtImpersonateThread(const syscall_context& /*c*/, handle /*server_thread*/, handle /*client_thread*/,
                                            uint64_t /*security_qos*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtImpersonateAnonymousToken(const syscall_context& /*c*/, handle /*thread_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAdjustGroupsToken(const syscall_context& /*c*/, handle /*token_handle*/, BOOLEAN /*reset_to_default*/,
                                            uint64_t /*new_state*/, ULONG /*buffer_length*/, uint64_t /*previous_state*/,
                                            emulator_object<ULONG> /*return_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFilterToken(const syscall_context& /*c*/, handle /*existing_token_handle*/, ULONG /*flags*/,
                                      uint64_t /*sids_to_disable*/, uint64_t /*privileges_to_delete*/, uint64_t /*restricted_sids*/,
                                      emulator_object<handle> new_token_handle)
        {
            new_token_handle.write(DUMMY_IMPERSONATION_TOKEN);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtMakePermanentObject(const syscall_context& /*c*/, handle /*object_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtMakeTemporaryObject(const syscall_context& /*c*/, handle /*object_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCancelSynchronousIoFile(const syscall_context& /*c*/, handle /*thread_handle*/, uint64_t /*io_request_to_cancel*/,
                                                  emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = 0;
            });

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtConvertBetweenAuxiliaryCounterAndPerformanceCounter(const syscall_context& /*c*/, ULONG /*convert_to_auxiliary*/,
                                                                              uint64_t /*performance_or_auxiliary*/,
                                                                              emulator_object<uint64_t> result,
                                                                              emulator_object<uint64_t> /*conversion_error*/)
        {
            result.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGetDevicePowerState(const syscall_context& /*c*/, handle /*device_handle*/, uint64_t /*device_power_state*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSignalAndWaitForSingleObject(const syscall_context& c, handle signal_handle, handle /*wait_handle*/,
                                                       BOOLEAN /*alertable*/, emulator_object<LARGE_INTEGER> /*timeout*/)
        {
            handle_NtSetEvent(c, signal_handle.bits, emulator_object<LONG>{c.emu, uint64_t{0}});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReadVirtualMemoryEx(const syscall_context& c, handle process_handle, emulator_pointer base_address,
                                              emulator_pointer buffer, uint64_t number_of_bytes_to_read,
                                              emulator_object<uint64_t> number_of_bytes_read, ULONG /*flags*/)
        {
            return handle_NtReadVirtualMemory(c, process_handle, base_address, buffer, number_of_bytes_to_read, number_of_bytes_read);
        }

        NTSTATUS handle_NtCreateSectionEx(const syscall_context& c, emulator_object<handle> section_handle, ACCESS_MASK desired_access,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                          emulator_object<ULARGE_INTEGER> maximum_size, ULONG section_page_protection,
                                          ULONG allocation_attributes, handle file_handle, uint64_t /*extended_parameters*/,
                                          ULONG /*extended_parameter_count*/)
        {
            return handle_NtCreateSection(c, section_handle, desired_access, object_attributes, maximum_size, section_page_protection,
                                          allocation_attributes, file_handle);
        }

        NTSTATUS handle_NtDisplayString(const syscall_context& c, const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> string)
        {
            if (c.win_emu.callbacks.on_debug_string)
            {
                const auto text = read_unicode_string(c.emu, string);
                if (!text.empty())
                {
                    c.win_emu.callbacks.on_debug_string(std::string(text.begin(), text.end()));
                }
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRenameKey(const syscall_context& /*c*/, handle /*key_handle*/,
                                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*new_name*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationToken(const syscall_context& /*c*/, handle /*token_handle*/, uint32_t /*token_information_class*/,
                                              uint64_t /*token_information*/, ULONG /*token_information_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationSymbolicLink(const syscall_context& /*c*/, handle /*link_handle*/, uint32_t /*symlink_info_class*/,
                                                     uint64_t /*symlink_information*/, ULONG /*symlink_information_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLoadKey(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                  emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*source_file*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLoadKey2(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*source_file*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUnloadKey(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCompareTokens(const syscall_context& /*c*/, const handle first_token_handle, const handle second_token_handle,
                                        emulator_object<BOOLEAN> equal)
        {
            // Kernel: SeCompareTokens; invalid handle → STATUS_INVALID_HANDLE; same object → Equal=TRUE.
            // Sogen pseudo-handles are always valid; compare by value for equality.
            if (first_token_handle.bits == 0 || second_token_handle.bits == 0)
            {
                return STATUS_INVALID_HANDLE;
            }
            equal.write(first_token_handle.bits == second_token_handle.bits ? TRUE : FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenKeyTransacted(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                            emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                            handle /*transaction_handle*/)
        {
            return handle_NtOpenKey(c, key_handle, desired_access, object_attributes);
        }

        NTSTATUS handle_NtCreateKeyTransacted(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              ULONG /*title_index*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*class_name*/,
                                              ULONG create_options, handle /*transaction_handle*/, emulator_object<ULONG> disposition)
        {
            return handle_NtCreateKey(c, key_handle, desired_access, object_attributes, 0,
                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, uint64_t{0}}, create_options, disposition);
        }

        NTSTATUS handle_NtFlushInstallUILanguage(const syscall_context& /*c*/, uint32_t /*language*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGetEnvironmentVariableEx(const syscall_context& /*c*/,
                                                   emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*name*/, uint64_t /*value*/,
                                                   emulator_object<ULONG> /*value_size*/, uint32_t /*flags*/)
        {
            return STATUS_NOT_FOUND;
        }

        NTSTATUS handle_NtQuerySystemTime(const syscall_context& c, emulator_object<LARGE_INTEGER> system_time)
        {
            // Kernel: user-mode path probes pointer for alignment and read access.
            if (!system_time)
            {
                return STATUS_ACCESS_VIOLATION;
            }
            const auto& kusd = c.proc.kusd.get();
            system_time.access([&](LARGE_INTEGER& val) {
                val.LowPart = kusd.SystemTime.LowPart;
                val.HighPart = kusd.SystemTime.High1Time;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetSystemTime(const syscall_context& /*c*/, emulator_object<LARGE_INTEGER> new_system_time,
                                        emulator_object<LARGE_INTEGER> previous_system_time)
        {
            // Kernel: requires SeSystemtimePrivilege; validates a1; updates system clock.
            // Sogen: clock is read-only; write to previous is always zero; return success.
            if (!new_system_time)
            {
                return STATUS_ACCESS_VIOLATION;
            }
            previous_system_time.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryMutant(const syscall_context& c, handle mutant_handle, uint32_t info_class, uint64_t mutant_info,
                                      ULONG info_length, emulator_object<ULONG> return_length)
        {
            // Kernel: class 0 = MutantBasicInformation (8 bytes), class 1 = MutantOwnerInformation (16 bytes)
            if (info_class > 1)
            {
                return STATUS_INVALID_INFO_CLASS;
            }

            // Both classes use exact-size checks (kernel: a4 != 8 / a4 != 16)
            constexpr ULONG basic_size = sizeof(LONG) + sizeof(BOOLEAN) + sizeof(BOOLEAN); // 6 + 2 pad = 8
            static_assert(basic_size == 6);
            constexpr ULONG basic_buf = 8;
            constexpr ULONG owner_buf = 16;

            const ULONG expected_size = (info_class == 0) ? basic_buf : owner_buf;

            return_length.write_if_valid(expected_size);

            if (info_length != expected_size)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            if (!mutant_info)
            {
                return STATUS_SUCCESS;
            }

            const auto* m = c.proc.mutants.get(mutant_handle);

            if (info_class == 0)
            {
                // CurrentCount: 1 = free, 0 = owned once, negative = owned recursively
                const LONG current_count = (m && m->locked_count > 0) ? (1 - static_cast<LONG>(m->locked_count)) : 1;
                const BOOLEAN owned_by_caller =
                    (m && m->locked_count > 0 && m->owning_thread_id == c.win_emu.current_thread().id) ? TRUE : FALSE;
                const BOOLEAN abandoned = (m && m->abandoned) ? TRUE : FALSE;

                static_assert(sizeof(LONG) == 4);
                static_assert(sizeof(BOOLEAN) == 1);

                emulator_object<LONG>{c.emu, mutant_info}.write(current_count);
                emulator_object<BOOLEAN>{c.emu, mutant_info + 4}.write(owned_by_caller);
                emulator_object<BOOLEAN>{c.emu, mutant_info + 5}.write(abandoned);
            }
            else
            {
                // MutantOwnerInformation: CLIENT_ID of owning thread (UniqueProcess + UniqueThread)
                emulator_object<uint64_t>{c.emu, mutant_info}.write(0);
                emulator_object<uint64_t>{c.emu, mutant_info + 8}.write(0);
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQuerySemaphore(const syscall_context& c, handle semaphore_handle, uint32_t info_class, uint64_t semaphore_info,
                                         ULONG info_length, emulator_object<ULONG> return_length)
        {
            // Kernel: only class 0 (SemaphoreBasicInformation); exact size == 8
            if (info_class != 0)
            {
                return STATUS_INVALID_INFO_CLASS;
            }

            constexpr ULONG info_size = sizeof(LONG) + sizeof(LONG); // CurrentCount + MaximumCount
            static_assert(info_size == 8);

            return_length.write_if_valid(info_size);

            if (info_length != info_size)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }
            if (semaphore_info)
            {
                const auto* sem = c.proc.semaphores.get(semaphore_handle);
                const auto count = sem ? static_cast<LONG>(sem->current_count) : 0L;
                const auto max_count = sem ? static_cast<LONG>(sem->max_count) : 1L;
                emulator_object<LONG>{c.emu, semaphore_info}.write(count);
                emulator_object<LONG>{c.emu, semaphore_info + sizeof(LONG)}.write(max_count);
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryOpenSubKeys(const syscall_context& /*c*/,
                                           emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                           emulator_object<ULONG> handle_count)
        {
            handle_count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryOpenSubKeysEx(const syscall_context& /*c*/,
                                             emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                             ULONG /*buffer_length*/, uint64_t /*buffer*/, emulator_object<ULONG> handle_count)
        {
            handle_count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAcquireProcessActivityReference(const syscall_context& /*c*/, emulator_object<handle> activity_reference,
                                                          handle /*process_handle*/, ULONG /*flags*/)
        {
            activity_reference.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateDirectoryObjectEx(const syscall_context& c, emulator_object<handle> directory_handle,
                                                  ACCESS_MASK desired_access,
                                                  emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                  handle /*shadow_directory_handle*/, ULONG /*flags*/)
        {
            return handle_NtCreateDirectoryObject(c, directory_handle, desired_access, object_attributes);
        }

        NTSTATUS handle_NtQueryAuxiliaryCounterFrequency(const syscall_context& /*c*/, emulator_object<ULONGLONG> aux_counter_frequency)
        {
            aux_counter_frequency.write_if_valid(10000000ULL);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtNotifyChangeDirectoryFile(const syscall_context& /*c*/, handle /*file_handle*/, handle /*event*/,
                                                    uint64_t /*apc_routine*/, uint64_t /*apc_context*/,
                                                    emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                    uint64_t /*buffer*/, ULONG /*buffer_length*/, ULONG /*completion_filter*/,
                                                    BOOLEAN /*watch_tree*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = static_cast<NTSTATUS>(0x00000103L); // STATUS_PENDING
            });
            return static_cast<NTSTATUS>(0x00000103L); // STATUS_PENDING
        }

        NTSTATUS handle_NtNotifyChangeMultipleKeys(const syscall_context& /*c*/, handle /*master_key_handle*/, ULONG /*count*/,
                                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*subordinate_objects*/,
                                                   handle /*event*/, uint64_t /*apc_routine*/, uint64_t /*apc_context*/,
                                                   emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                   uint64_t /*buffer*/, ULONG /*buffer_length*/, ULONG /*completion_filter*/,
                                                   BOOLEAN /*watch_tree*/, emulator_object<LARGE_INTEGER> /*timeout*/,
                                                   BOOLEAN /*asynchronous*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) { isb.Status = static_cast<NTSTATUS>(0x00000103L); });
            return static_cast<NTSTATUS>(0x00000103L);
        }

        NTSTATUS handle_NtNotifyChangeSession(const syscall_context& /*c*/, handle /*session_handle*/, ULONG /*change_sequence_number*/,
                                              uint64_t /*change_time_stamp*/, uint32_t /*event_type*/,
                                              ULONG /*previous_session_global_state*/, ULONG /*new_session_global_state*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtResetEvent(const syscall_context& c, handle event_handle, emulator_object<LONG> previous_state)
        {
            auto* e = c.proc.events.get(event_handle);
            if (!e)
            {
                return STATUS_INVALID_HANDLE;
            }
            previous_state.write_if_valid(e->signaled ? 1L : 0L);
            e->signaled = false;
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryTimer(const syscall_context& c, handle timer_handle, uint32_t info_class, uint64_t timer_info,
                                     ULONG info_length, emulator_object<ULONG> return_length)
        {
            // Kernel: only class 0 (TimerBasicInformation); exact size == 16
            if (info_class != 0)
            {
                return STATUS_INVALID_INFO_CLASS;
            }

            // TIMER_BASIC_INFORMATION: LARGE_INTEGER DueTime (8) + BOOLEAN TimerState (1) + 7 pad = 16
            constexpr ULONG info_size = 16;

            return_length.write_if_valid(info_size);

            if (info_length != info_size)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            if (timer_info)
            {
                const auto* t = c.proc.timers.get(timer_handle);
                (void)t;
                // RemainingTime at offset 0 (8 bytes), TimerState at offset 8 (1 byte)
                emulator_object<LARGE_INTEGER>{c.emu, timer_info}.write({});
                emulator_object<BOOLEAN>{c.emu, timer_info + sizeof(LARGE_INTEGER)}.write(FALSE);
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenIoCompletion(const syscall_context& c, emulator_object<handle> io_completion_handle,
                                           ACCESS_MASK /*desired_access*/,
                                           emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            const auto attributes = object_attributes.read();
            const auto name = read_unicode_string(c.emu, attributes.ObjectName);
            for (auto& entry : c.proc.io_completions)
            {
                if (entry.second.name == name)
                {
                    ++entry.second.ref_count;
                    io_completion_handle.write(c.proc.io_completions.make_handle(entry.first));
                    return STATUS_SUCCESS;
                }
            }
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        NTSTATUS handle_NtPrivilegeCheck(const syscall_context& /*c*/, handle /*client_token*/, uint64_t /*required_privileges*/,
                                         emulator_object<BOOLEAN> result)
        {
            result.write(TRUE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetDefaultLocale(const syscall_context& /*c*/, BOOLEAN /*user_profile*/, LCID /*default_locale_id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetDefaultUILanguage(const syscall_context& /*c*/, LANGID /*default_language_id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetUuidSeed(const syscall_context& /*c*/, uint64_t /*seed*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtIsSystemResumeAutomatic(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtInitializeRegistry(const syscall_context& /*c*/, USHORT /*boot_condition*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLoadKeyEx(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                    emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*source_file*/, ULONG /*flags*/,
                                    handle /*trust_class_key*/, handle /*event*/, ACCESS_MASK /*desired_access*/,
                                    emulator_object<handle> /*root_handle*/, uint64_t /*reserved*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLoadKey3(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*source_file*/, ULONG /*flags*/,
                                   uint64_t /*extended_parameters*/, ULONG /*extended_parameter_count*/, ACCESS_MASK /*desired_access*/,
                                   emulator_object<handle> /*root_handle*/, ULONG /*reserved*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReplaceKey(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*new_file*/,
                                     handle /*target_handle*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*old_file*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRestoreKey(const syscall_context& /*c*/, handle /*key_handle*/, handle /*file_handle*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSaveKey(const syscall_context& /*c*/, handle /*key_handle*/, handle /*file_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSaveKeyEx(const syscall_context& /*c*/, handle /*key_handle*/, handle /*file_handle*/, ULONG /*format*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUnloadKey2(const syscall_context& /*c*/, emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/,
                                     ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUnloadKeyEx(const syscall_context& /*c*/,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*target_key*/, handle /*event_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetEventEx(const syscall_context& c, handle event_handle, uint64_t /*flags*/)
        {
            auto* e = c.proc.events.get(event_handle);
            if (!e)
            {
                return STATUS_INVALID_HANDLE;
            }
            e->signaled = true;
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetDebugFilterState(const syscall_context& /*c*/, ULONG /*component_id*/, ULONG /*level*/, BOOLEAN /*state*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtShutdownSystem(const syscall_context& /*c*/, uint32_t /*shutdown_action*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetVolumeInformationFile(const syscall_context& /*c*/, handle /*file_handle*/,
                                                   emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                   uint64_t /*fs_info*/, ULONG /*length*/, uint32_t /*fs_info_class*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = 0;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetEaFile(const syscall_context& /*c*/, handle /*file_handle*/,
                                    emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, uint64_t /*ea_buffer*/,
                                    ULONG /*ea_buffer_size*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = 0;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtWriteFileGather(const syscall_context& /*c*/, handle /*file_handle*/, handle /*event*/, uint64_t /*apc_routine*/,
                                          uint64_t /*apc_context*/, emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                          uint64_t /*segment_array*/, ULONG length, emulator_object<LARGE_INTEGER> /*byte_offset*/,
                                          emulator_object<ULONG> /*key*/)
        {
            io_status_block.access([&](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = length;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReadFileScatter(const syscall_context& /*c*/, handle /*file_handle*/, handle /*event*/, uint64_t /*apc_routine*/,
                                          uint64_t /*apc_context*/, emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                          uint64_t /*segment_array*/, ULONG length, emulator_object<LARGE_INTEGER> /*byte_offset*/,
                                          emulator_object<ULONG> /*key*/)
        {
            io_status_block.access([&](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = length;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtMapCMFModule(const syscall_context& /*c*/, ULONG /*flags*/, ULONG /*ordinal*/, emulator_object<ULONG> /*count*/,
                                       uint64_t /*manifest_address*/, emulator_object<uint64_t> /*module_base*/)
        {
            return STATUS_UNSUCCESSFUL;
        }

        NTSTATUS handle_NtOpenSession(const syscall_context& /*c*/, emulator_object<handle> session_handle, ACCESS_MASK /*desired_access*/,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            session_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryIoCompletion(const syscall_context& c, handle /*io_completion_handle*/, uint32_t io_completion_info_class,
                                            uint64_t io_completion_info, ULONG io_completion_info_length,
                                            emulator_object<ULONG> return_length)
        {
            // Kernel: info_class != 0 → STATUS_INVALID_INFO_CLASS; length != 4 → STATUS_INFO_LENGTH_MISMATCH.
            if (io_completion_info_class != 0)
            {
                return STATUS_INVALID_INFO_CLASS;
            }
            constexpr ULONG info_size = sizeof(ULONG);
            return_length.write_if_valid(info_size);
            if (io_completion_info_length != info_size)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }
            if (io_completion_info)
            {
                emulator_object<ULONG>{c.emu, io_completion_info}.write(0);
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryEaFile(const syscall_context& /*c*/, handle /*file_handle*/,
                                      emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, uint64_t /*buffer*/,
                                      ULONG /*length*/, BOOLEAN /*return_single_entry*/, uint64_t /*ea_list*/, ULONG /*ea_list_length*/,
                                      emulator_object<ULONG> /*ea_index*/, BOOLEAN /*restart_scan*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) { isb.Status = static_cast<NTSTATUS>(0x80000052L); });
            return static_cast<NTSTATUS>(0x80000052L);
        }

        NTSTATUS handle_NtQueryQuotaInformationFile(const syscall_context& /*c*/, handle /*file_handle*/,
                                                    emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                    uint64_t /*buffer*/, ULONG /*length*/, BOOLEAN /*return_single_entry*/,
                                                    uint64_t /*sid_list*/, ULONG /*sid_list_length*/, uint64_t /*start_sid*/,
                                                    BOOLEAN /*restart_scan*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = 0;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryDirectoryObject(const syscall_context& /*c*/, handle /*directory_handle*/, uint64_t /*buffer*/,
                                               ULONG /*length*/, BOOLEAN /*return_single_entry*/, BOOLEAN /*restart_scan*/,
                                               emulator_object<ULONG> context, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            context.write_if_valid(0);
            return static_cast<NTSTATUS>(0x8000001AL); // STATUS_NO_MORE_ENTRIES
        }

        NTSTATUS handle_NtQueryInformationAtom(const syscall_context& /*c*/, USHORT /*atom*/, uint32_t /*atom_info_class*/,
                                               uint64_t /*atom_info*/, ULONG atom_info_length, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(atom_info_length);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtResumeProcess(const syscall_context& c, handle process_handle)
        {
            // Kernel: ObpReferenceObjectByHandleWithTag validates handle before calling PsMultiResumeProcess.
            // Sogen only has one process; accept any valid process handle as a no-op.
            if (!c.proc.is_current_process_handle(process_handle))
            {
                return STATUS_INVALID_HANDLE;
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSuspendProcess(const syscall_context& c, handle process_handle)
        {
            // Kernel: ObpReferenceObjectByHandleWithTag validates handle before calling PsSuspendProcess.
            // Sogen only has one process; accept any valid process handle as a no-op.
            if (!c.proc.is_current_process_handle(process_handle))
            {
                return STATUS_INVALID_HANDLE;
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSaveMergedKeys(const syscall_context& /*c*/, handle /*high_precedence_key*/, handle /*low_precedence_key*/,
                                         handle /*file_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetDefaultHardErrorPort(const syscall_context& /*c*/, handle /*port_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckByType(const syscall_context& /*c*/, uint64_t /*security_descriptor*/, uint64_t /*principal_self_sid*/,
                                            handle /*client_token*/, ACCESS_MASK /*desired_access*/, uint64_t /*object_type_list*/,
                                            ULONG /*object_type_list_length*/, uint64_t /*generic_mapping*/, uint64_t /*privilege_set*/,
                                            emulator_object<ULONG> privilege_set_length, emulator_object<ACCESS_MASK> granted_access,
                                            emulator_object<ULONG> access_status)
        {
            privilege_set_length.write_if_valid(sizeof(ULONG));
            granted_access.write_if_valid(GENERIC_ALL);
            access_status.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckByTypeResultList(const syscall_context& /*c*/, uint64_t /*security_descriptor*/,
                                                      uint64_t /*principal_self_sid*/, handle /*client_token*/,
                                                      ACCESS_MASK /*desired_access*/, uint64_t /*object_type_list*/,
                                                      ULONG /*object_type_list_length*/, uint64_t /*generic_mapping*/,
                                                      uint64_t /*privilege_set*/, emulator_object<ULONG> /*privilege_set_length*/,
                                                      emulator_object<ACCESS_MASK> /*granted_access_list*/,
                                                      emulator_object<ULONG> /*access_status_list*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRegisterThreadTerminatePort(const syscall_context& /*c*/, handle /*port_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReplyPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*reply_message*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRequestPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*request_message*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtThawRegistry(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQuerySystemEnvironmentValue(const syscall_context& /*c*/,
                                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*variable_name*/,
                                                      uint64_t /*variable_value*/, USHORT value_length,
                                                      emulator_object<USHORT> return_length)
        {
            return_length.write_if_valid(value_length);
            return STATUS_NOT_FOUND;
        }

        NTSTATUS handle_NtQuerySystemEnvironmentValueEx(const syscall_context& /*c*/,
                                                        emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*variable_name*/,
                                                        uint64_t /*vendor_guid*/, uint64_t /*value*/, emulator_object<ULONG> value_length,
                                                        emulator_object<ULONG> /*attributes*/)
        {
            value_length.write_if_valid(0);
            return STATUS_NOT_FOUND;
        }

        NTSTATUS handle_NtSetSystemEnvironmentValue(const syscall_context& /*c*/,
                                                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*variable_name*/,
                                                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*variable_value*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetSystemEnvironmentValueEx(const syscall_context& /*c*/,
                                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*variable_name*/,
                                                      uint64_t /*vendor_guid*/, uint64_t /*value*/, ULONG /*value_length*/,
                                                      ULONG /*attributes*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRevertContainerImpersonation(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDrawText(const syscall_context& /*c*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*text*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtEnableLastKnownGood(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDisableLastKnownGood(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryEnvironmentVariableInfoEx(const syscall_context& /*c*/,
                                                         emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*variable_name*/,
                                                         uint64_t /*flags*/, emulator_object<ULONG> /*variable_info*/)
        {
            return STATUS_NOT_FOUND;
        }

        NTSTATUS handle_NtQuerySecurityPolicy(const syscall_context& /*c*/, uint64_t /*policy*/, uint64_t /*value*/, ULONG value_length,
                                              emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(value_length);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetQuotaInformationFile(const syscall_context& /*c*/, handle /*file_handle*/,
                                                  emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                  uint64_t /*buffer*/, ULONG /*length*/)
        {
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = 0;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetSystemPowerState(const syscall_context& /*c*/, uint32_t /*system_action*/, uint32_t /*min_system_state*/,
                                              ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtTranslateFilePath(const syscall_context& /*c*/, uint64_t /*input_file_path*/, ULONG /*output_type*/,
                                            uint64_t /*output_file_path*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCreateKeyTransacted_Stub(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                   ULONG title_index, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name,
                                                   ULONG create_options, handle /*transaction_handle*/, emulator_object<ULONG> disposition)
        {
            return handle_NtCreateKey(c, key_handle, desired_access, object_attributes, title_index, class_name, create_options,
                                      disposition);
        }

        NTSTATUS handle_NtOpenKeyTransactedEx(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              ULONG /*open_options*/, handle /*transaction_handle*/)
        {
            return handle_NtOpenKey(c, key_handle, desired_access, object_attributes);
        }

        NTSTATUS handle_NtOpenKeyTransactedEx_Stub(const syscall_context& c, emulator_object<handle> key_handle, ACCESS_MASK desired_access,
                                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                   ULONG open_options, handle transaction_handle)
        {
            return handle_NtOpenKeyTransactedEx(c, key_handle, desired_access, object_attributes, open_options, transaction_handle);
        }

        NTSTATUS handle_NtAcceptConnectPort(const syscall_context& /*c*/, emulator_object<handle> port_handle, uint64_t /*port_context*/,
                                            uint64_t /*connection_request*/, BOOLEAN /*accept_connection*/, uint64_t /*server_view*/,
                                            uint64_t /*client_view*/)
        {
            port_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateProcessEx(const syscall_context& /*c*/, emulator_object<handle> /*process_handle*/,
                                          ACCESS_MASK /*desired_access*/,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                          handle /*parent_process*/, ULONG /*flags*/, handle /*section_handle*/, handle /*debug_port*/,
                                          handle /*exception_port*/, ULONG /*job_member_level*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCreateProfileEx(const syscall_context& /*c*/, emulator_object<handle> profile_handle, handle /*process_handle*/,
                                          uint64_t /*profile_base*/, ULONG /*profile_size*/, ULONG /*bucket_size*/, uint64_t /*buffer*/,
                                          ULONG /*buffer_size*/, uint32_t /*profile_source*/, ULONG /*group_count*/,
                                          uint64_t /*group_affinity*/)
        {
            profile_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateToken(const syscall_context& /*c*/, emulator_object<handle> token_handle, ACCESS_MASK /*desired_access*/,
                                      emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                      uint32_t /*token_type*/, uint64_t /*authentication_id*/, uint64_t /*expiration_time*/,
                                      uint64_t /*user*/, uint64_t /*groups*/, uint64_t /*privileges*/, uint64_t /*owner*/,
                                      uint64_t /*primary_group*/, uint64_t /*default_dacl*/, uint64_t /*token_source*/)
        {
            token_handle.write(DUMMY_IMPERSONATION_TOKEN);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateTokenEx(const syscall_context& /*c*/, emulator_object<handle> token_handle, ACCESS_MASK /*desired_access*/,
                                        emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                        uint32_t /*token_type*/, uint64_t /*authentication_id*/, uint64_t /*expiration_time*/,
                                        uint64_t /*user*/, uint64_t /*groups*/, uint64_t /*privileges*/, uint64_t /*user_attributes*/,
                                        uint64_t /*device_attributes*/, uint64_t /*device_groups*/, uint64_t /*mandatory_policy*/,
                                        uint64_t /*owner*/, uint64_t /*primary_group*/, uint64_t /*default_dacl*/,
                                        uint64_t /*token_source*/)
        {
            token_handle.write(DUMMY_IMPERSONATION_TOKEN);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtEnumerateSystemEnvironmentValuesEx(const syscall_context& /*c*/, ULONG /*info_class*/, uint64_t /*buffer*/,
                                                             emulator_object<ULONG> buffer_length)
        {
            buffer_length.write_if_valid(0);
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtPrepareComplete(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                          emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPropagationComplete(const syscall_context& /*c*/, handle /*resource_manager_handle*/, ULONG /*request_cookie*/,
                                              ULONG /*buffer_length*/, uint64_t /*buffer*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPropagationFailed(const syscall_context& /*c*/, handle /*resource_manager_handle*/, ULONG /*request_cookie*/,
                                            NTSTATUS /*propagation_status*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPssCaptureVaSpaceBulk(const syscall_context& /*c*/, handle /*process_handle*/, uint64_t /*base_address*/,
                                                uint64_t /*capture_info*/, uint64_t /*buffer*/, ULONG /*buffer_length*/,
                                                emulator_object<ULONG> bytes_written)
        {
            bytes_written.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationPort(const syscall_context& /*c*/, handle /*port_handle*/, uint32_t /*port_info_class*/,
                                               uint64_t /*port_info*/, ULONG /*port_info_length*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationWorkerFactory(const syscall_context& /*c*/, handle /*worker_factory_handle*/,
                                                        uint32_t /*worker_factory_info_class*/, uint64_t /*worker_factory_info*/,
                                                        ULONG /*worker_factory_info_length*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtWaitForDebugEvent(const syscall_context& /*c*/, handle /*debug_object_handle*/, BOOLEAN /*alertable*/,
                                            emulator_object<LARGE_INTEGER> /*timeout*/, uint64_t /*wait_state_change*/)
        {
            return static_cast<NTSTATUS>(0x00000102L); // STATUS_TIMEOUT
        }

        NTSTATUS handle_NtWaitLowEventPair(const syscall_context& /*c*/, handle /*event_pair_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtWriteRequestData(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*message*/,
                                           ULONG /*data_entry_index*/, uint64_t /*buffer*/, ULONG /*buffer_size*/,
                                           emulator_object<ULONG> number_of_bytes_written)
        {
            number_of_bytes_written.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtdllRunOnceInitMuiCrits(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtosSecureKernelImportBugcheck(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryIntervalProfile(const syscall_context& /*c*/, uint32_t /*profile_source*/, emulator_object<ULONG> interval)
        {
            interval.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetIntervalProfile(const syscall_context& /*c*/, ULONG /*interval*/, uint32_t /*source*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtStartProfile(const syscall_context& /*c*/, handle /*profile_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtStopProfile(const syscall_context& /*c*/, handle /*profile_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReplyWaitReceivePort(const syscall_context& /*c*/, handle /*port_handle*/,
                                               emulator_object<uint64_t> /*port_context*/, uint64_t /*reply_message*/,
                                               uint64_t /*receive_message*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReplyWaitReceivePortEx(const syscall_context& /*c*/, handle /*port_handle*/,
                                                 emulator_object<uint64_t> /*port_context*/, uint64_t /*reply_message*/,
                                                 uint64_t /*receive_message*/, emulator_object<LARGE_INTEGER> /*timeout*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReplyWaitReplyPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*reply_message*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtResetWriteWatch(const syscall_context& /*c*/, handle /*process_handle*/, uint64_t /*base_address*/,
                                          ULONG /*region_size*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSerializeBoot(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUnloadDriver(const syscall_context& /*c*/,
                                       emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*driver_service_name*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenRegistryTransaction(const syscall_context& /*c*/, emulator_object<handle> transaction_handle,
                                                  ACCESS_MASK /*desired_access*/,
                                                  emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            transaction_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPrivilegeObjectAuditAlarm(const syscall_context& /*c*/,
                                                    emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/,
                                                    uint64_t /*handle_id*/, handle /*client_token*/, ACCESS_MASK /*desired_access*/,
                                                    uint64_t /*privileges*/, BOOLEAN /*access_granted*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPrivilegedServiceAuditAlarm(const syscall_context& /*c*/,
                                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/,
                                                      emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*service_name*/,
                                                      handle /*client_token*/, uint64_t /*privileges*/, BOOLEAN /*access_granted*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRegisterProtocolAddressInformation(const syscall_context& /*c*/, handle /*resource_manager*/,
                                                             uint64_t /*protocol_id*/, ULONG /*protocol_information_size*/,
                                                             uint64_t /*protocol_information*/, ULONG /*create_options*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAssignProcessToJobObject(const syscall_context& /*c*/, handle /*job_handle*/, handle /*process_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetIRTimer(const syscall_context& /*c*/, handle /*timer_handle*/, emulator_object<LARGE_INTEGER> /*due_time*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReplacePartitionUnit(const syscall_context& /*c*/,
                                               emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*target_instance_path*/,
                                               emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*spare_instance_path*/,
                                               ULONG /*flags*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCloseObjectAuditAlarm(const syscall_context& /*c*/,
                                                emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/,
                                                uint64_t /*handle_id*/, BOOLEAN /*generate_on_close*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDeleteObjectAuditAlarm(const syscall_context& /*c*/,
                                                 emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/,
                                                 uint64_t /*handle_id*/, BOOLEAN /*generate_on_close*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenObjectAuditAlarm(const syscall_context& /*c*/,
                                               emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/,
                                               uint64_t /*handle_id*/,
                                               emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_type_name*/,
                                               emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_name*/,
                                               uint64_t /*security_descriptor*/, handle /*client_token*/, ACCESS_MASK /*desired_access*/,
                                               ACCESS_MASK /*granted_access*/, uint64_t /*privileges*/, BOOLEAN /*object_creation*/,
                                               BOOLEAN /*access_granted*/, emulator_object<BOOLEAN> generate_on_close)
        {
            generate_on_close.write_if_valid(FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckAndAuditAlarm(
            const syscall_context& /*c*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/, uint64_t /*handle_id*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_type_name*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_name*/, uint64_t /*security_descriptor*/,
            ACCESS_MASK /*desired_access*/, uint64_t /*generic_mapping*/, BOOLEAN /*object_creation*/,
            emulator_object<ACCESS_MASK> granted_access, emulator_object<BOOLEAN> access_status, emulator_object<BOOLEAN> generate_on_close)
        {
            granted_access.write_if_valid(GENERIC_ALL);
            access_status.write_if_valid(TRUE);
            generate_on_close.write_if_valid(FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckByTypeAndAuditAlarm(
            const syscall_context& /*c*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/, uint64_t /*handle_id*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_type_name*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_name*/, uint64_t /*security_descriptor*/,
            uint64_t /*principal_self_sid*/, ACCESS_MASK /*desired_access*/, uint32_t /*audit_type*/, ULONG /*flags*/,
            uint64_t /*object_type_list*/, ULONG /*object_type_list_length*/, uint64_t /*generic_mapping*/, BOOLEAN /*object_creation*/,
            emulator_object<ACCESS_MASK> granted_access, emulator_object<BOOLEAN> access_status, emulator_object<BOOLEAN> generate_on_close)
        {
            granted_access.write_if_valid(GENERIC_ALL);
            access_status.write_if_valid(TRUE);
            generate_on_close.write_if_valid(FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckByTypeResultListAndAuditAlarm(
            const syscall_context& /*c*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/, uint64_t /*handle_id*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_type_name*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_name*/, uint64_t /*security_descriptor*/,
            uint64_t /*principal_self_sid*/, ACCESS_MASK /*desired_access*/, uint32_t /*audit_type*/, ULONG /*flags*/,
            uint64_t /*object_type_list*/, ULONG /*object_type_list_length*/, uint64_t /*generic_mapping*/, BOOLEAN /*object_creation*/,
            emulator_object<ACCESS_MASK> /*granted_access_list*/, emulator_object<BOOLEAN> /*access_status_list*/,
            emulator_object<BOOLEAN> generate_on_close)
        {
            generate_on_close.write_if_valid(FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAccessCheckByTypeResultListAndAuditAlarmByHandle(
            const syscall_context& /*c*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*subsystem_name*/, uint64_t /*handle_id*/,
            handle /*client_token*/, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_type_name*/,
            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*object_name*/, uint64_t /*security_descriptor*/,
            uint64_t /*principal_self_sid*/, ACCESS_MASK /*desired_access*/, uint32_t /*audit_type*/, ULONG /*flags*/,
            uint64_t /*object_type_list*/, ULONG /*object_type_list_length*/, uint64_t /*generic_mapping*/, BOOLEAN /*object_creation*/,
            emulator_object<ACCESS_MASK> /*granted_access_list*/, emulator_object<BOOLEAN> /*access_status_list*/,
            emulator_object<BOOLEAN> generate_on_close)
        {
            generate_on_close.write_if_valid(FALSE);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtChangeProcessState(const syscall_context& /*c*/, handle /*process_state_change*/, handle /*process_handle*/,
                                             uint32_t /*state_change_type*/, uint64_t /*extended_information*/,
                                             ULONG /*extended_information_length*/, ULONG64 /*reserved*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtChangeThreadState(const syscall_context& /*c*/, handle /*thread_state_change*/, handle /*thread_handle*/,
                                            uint32_t /*state_change_type*/, uint64_t /*extended_information*/,
                                            ULONG /*extended_information_length*/, ULONG64 /*reserved*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCommitComplete(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                         emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCompactKeys(const syscall_context& /*c*/, ULONG /*count*/, uint64_t /*key_array*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCompressKey(const syscall_context& /*c*/, handle /*key*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreatePagingFile(const syscall_context& /*c*/,
                                           emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*page_file_name*/,
                                           emulator_object<LARGE_INTEGER> /*min_size*/, emulator_object<LARGE_INTEGER> /*max_size*/,
                                           ULONG /*priority*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreatePartition(const syscall_context& /*c*/, handle /*parent_partition_handle*/,
                                          emulator_object<handle> partition_handle, ACCESS_MASK /*desired_access*/,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                          ULONG /*preferred_node*/)
        {
            partition_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenPartition(const syscall_context& /*c*/, emulator_object<handle> partition_handle,
                                        ACCESS_MASK /*desired_access*/,
                                        emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            partition_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtManagePartition(const syscall_context& /*c*/, handle /*target_handle*/, handle /*source_handle*/,
                                          uint32_t /*partition_information_class*/, uint64_t /*partition_information*/,
                                          ULONG /*partition_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreatePort(const syscall_context& /*c*/, emulator_object<handle> port_handle,
                                     emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                     ULONG /*max_connection_info_length*/, ULONG /*max_message_length*/, ULONG /*max_pool_usage*/)
        {
            port_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateWaitablePort(const syscall_context& /*c*/, emulator_object<handle> port_handle,
                                             emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                             ULONG /*max_connection_info_length*/, ULONG /*max_message_length*/, ULONG /*max_pool_usage*/)
        {
            port_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtListenPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*connection_request*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtImpersonateClientOfPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*message*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateProcess(const syscall_context& /*c*/, emulator_object<handle> /*process_handle*/,
                                        ACCESS_MASK /*desired_access*/,
                                        emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                        handle /*parent_process*/, BOOLEAN /*inherit_object_table*/, handle /*section_handle*/,
                                        handle /*debug_port*/, handle /*exception_port*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCreateProcessStateChange(const syscall_context& /*c*/, emulator_object<handle> state_change_handle,
                                                   ACCESS_MASK /*desired_access*/,
                                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                   handle /*process_handle*/, ULONG64 /*reserved*/)
        {
            state_change_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateThreadStateChange(const syscall_context& /*c*/, emulator_object<handle> state_change_handle,
                                                  ACCESS_MASK /*desired_access*/,
                                                  emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                  handle /*thread_handle*/, ULONG64 /*reserved*/)
        {
            state_change_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateProfile(const syscall_context& /*c*/, emulator_object<handle> profile_handle, handle /*process_handle*/,
                                        uint64_t /*profile_base*/, ULONG /*profile_size*/, ULONG /*bucket_size*/, uint64_t /*buffer*/,
                                        ULONG /*buffer_size*/, uint32_t /*profile_source*/, uint64_t /*affinity*/)
        {
            profile_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateThread(const syscall_context& /*c*/, emulator_object<handle> /*thread_handle*/,
                                       ACCESS_MASK /*desired_access*/,
                                       emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                       handle /*process_handle*/, uint64_t /*client_id*/, uint64_t /*thread_context*/,
                                       uint64_t /*initial_teb*/, BOOLEAN /*create_suspended*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtFreezeRegistry(const syscall_context& /*c*/, ULONG /*time_out_in_seconds*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGetCompleteWnfStateSubscription(const syscall_context& /*c*/, uint64_t /*old_descriptors*/,
                                                          uint64_t /*old_descriptor_count*/, ULONG /*flags*/, ULONG /*data_size*/,
                                                          uint64_t /*new_descriptors*/, emulator_object<ULONG> new_descriptor_count)
        {
            new_descriptor_count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGetWriteWatch(const syscall_context& /*c*/, handle /*process_handle*/, ULONG /*flags*/, uint64_t /*base_address*/,
                                        ULONG /*region_size*/, uint64_t /*user_address_array*/,
                                        emulator_object<ULONG> entries_in_user_address_array, emulator_object<ULONG> /*granularity*/)
        {
            entries_in_user_address_array.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtInitiatePowerAction(const syscall_context& /*c*/, uint32_t /*action*/, uint32_t /*min_system_state*/,
                                              ULONG /*flags*/, BOOLEAN /*invoke_shutdown*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLockProductActivationKeys(const syscall_context& /*c*/, emulator_object<ULONG> unsafe_sources_count,
                                                    emulator_object<ULONG> sources_count)
        {
            unsafe_sources_count.write_if_valid(0);
            sources_count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLockRegistryKey(const syscall_context& /*c*/, handle /*key_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPlugPlayControl(const syscall_context& /*c*/, uint32_t /*plug_play_control_class*/,
                                          uint64_t /*plug_play_control_data*/, ULONG /*plug_play_control_data_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPrePrepareComplete(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                             emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRollbackComplete(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                           emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSinglePhaseReject(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                            emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateMailslotFile(const syscall_context& /*c*/, emulator_object<handle> mailslot_handle,
                                             ACCESS_MASK /*desired_access*/,
                                             emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                             emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                             ULONG /*create_options*/, ULONG /*mailslot_quota*/, ULONG /*max_message_size*/,
                                             emulator_object<LARGE_INTEGER> /*read_timeout*/)
        {
            mailslot_handle.write_if_valid({});
            io_status_block.access([](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& isb) {
                isb.Status = STATUS_SUCCESS;
                isb.Information = 0;
            });
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAddBootEntry(const syscall_context& /*c*/, uint64_t /*boot_entry*/, emulator_object<ULONG> /*id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDeleteBootEntry(const syscall_context& /*c*/, ULONG /*id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtEnumerateBootEntries(const syscall_context& /*c*/, uint64_t /*buffer*/, emulator_object<ULONG> buffer_length)
        {
            buffer_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFilterBootOption(const syscall_context& /*c*/, uint32_t /*filter_operation*/, ULONG /*object_type*/,
                                           ULONG /*element_type*/, uint64_t /*data*/, ULONG /*data_size*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtModifyBootEntry(const syscall_context& /*c*/, uint64_t /*boot_entry*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetBootEntryOrder(const syscall_context& /*c*/, uint64_t /*ids*/, ULONG /*count*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetBootOptions(const syscall_context& /*c*/, uint64_t /*boot_options*/, ULONG /*fields_to_change*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryBootEntryOrder(const syscall_context& /*c*/, uint64_t /*ids*/, emulator_object<ULONG> count)
        {
            count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryBootOptions(const syscall_context& /*c*/, uint64_t /*boot_options*/,
                                           emulator_object<ULONG> boot_options_length)
        {
            boot_options_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAddDriverEntry(const syscall_context& /*c*/, uint64_t /*driver_entry*/, emulator_object<ULONG> /*id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDeleteDriverEntry(const syscall_context& /*c*/, ULONG /*id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtEnumerateDriverEntries(const syscall_context& /*c*/, uint64_t /*buffer*/, emulator_object<ULONG> buffer_length)
        {
            buffer_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtModifyDriverEntry(const syscall_context& /*c*/, uint64_t /*driver_entry*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryDriverEntryOrder(const syscall_context& /*c*/, uint64_t /*ids*/, emulator_object<ULONG> count)
        {
            count.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetDriverEntryOrder(const syscall_context& /*c*/, uint64_t /*ids*/, ULONG /*count*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLoadDriver(const syscall_context& /*c*/,
                                     emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*driver_service_name*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateJobObject(const syscall_context& /*c*/, emulator_object<handle> job_handle, ACCESS_MASK /*desired_access*/,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            job_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenJobObject(const syscall_context& /*c*/, emulator_object<handle> job_handle, ACCESS_MASK /*desired_access*/,
                                        emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            job_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtTerminateJobObject(const syscall_context& /*c*/, handle /*job_handle*/, NTSTATUS /*exit_status*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationJobObject(const syscall_context& /*c*/, handle /*job_handle*/, uint32_t /*job_object_info_class*/,
                                                  uint64_t /*job_object_info*/, ULONG /*job_object_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGetCachedSigningLevel(const syscall_context& /*c*/, handle /*file*/, emulator_object<ULONG> flags,
                                                emulator_object<ULONG> signing_level, uint64_t /*thumbnail_hash*/,
                                                emulator_object<ULONG> thumbnail_hash_size, emulator_object<ULONG> /*thumbnail_algorithm*/)
        {
            flags.write_if_valid(0);
            signing_level.write_if_valid(0);
            thumbnail_hash_size.write_if_valid(0);
            return STATUS_NOT_FOUND;
        }

        NTSTATUS handle_NtSetCachedSigningLevel(const syscall_context& /*c*/, ULONG /*flags*/, ULONG /*signing_level*/,
                                                uint64_t /*source_files*/, ULONG /*source_file_count*/, handle /*target_file*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetCachedSigningLevel2(const syscall_context& /*c*/, ULONG /*flags*/, ULONG /*signing_level*/,
                                                 uint64_t /*source_files*/, ULONG /*source_file_count*/, handle /*target_file*/,
                                                 uint64_t /*level_information*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCompareSigningLevels(const syscall_context& /*c*/, ULONG /*first_signing_level*/, ULONG /*second_signing_level*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDebugActiveProcess(const syscall_context& /*c*/, handle /*process_handle*/, handle /*debug_object_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtDebugContinue(const syscall_context& /*c*/, handle /*debug_object_handle*/, uint64_t /*client_id*/,
                                        NTSTATUS /*continue_status*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGetNotificationResourceManager(const syscall_context& /*c*/, handle /*resource_manager_handle*/,
                                                         uint64_t /*transaction_notification*/, ULONG /*notification_length*/,
                                                         emulator_object<LARGE_INTEGER> /*timeout*/, emulator_object<ULONG> return_length,
                                                         ULONG /*asynchronous*/, ULONG64 /*async_context*/)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateResourceManager(const syscall_context& /*c*/, emulator_object<handle> resource_manager_handle,
                                                ACCESS_MASK /*desired_access*/, handle /*tm_handle*/, uint64_t /*resource_manager_guid*/,
                                                emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                ULONG /*create_options*/,
                                                emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*description*/)
        {
            resource_manager_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenResourceManager(const syscall_context& /*c*/, emulator_object<handle> resource_manager_handle,
                                              ACCESS_MASK /*desired_access*/, handle /*tm_handle*/, uint64_t /*resource_manager_guid*/,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            resource_manager_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationResourceManager(const syscall_context& /*c*/, handle /*resource_manager_handle*/,
                                                          uint32_t /*resource_manager_info_class*/, uint64_t /*resource_manager_info*/,
                                                          ULONG /*resource_manager_info_length*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationResourceManager(const syscall_context& /*c*/, handle /*resource_manager_handle*/,
                                                        uint32_t /*resource_manager_info_class*/, uint64_t /*resource_manager_info*/,
                                                        ULONG /*resource_manager_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateIRTimer(const syscall_context& /*c*/, emulator_object<handle> timer_handle, uint64_t /*timer_type*/)
        {
            timer_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFreezeTransactions(const syscall_context& /*c*/, emulator_object<LARGE_INTEGER> /*freeze_timeout*/,
                                             emulator_object<LARGE_INTEGER> /*thaw_timeout*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtThawTransactions(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        // Transaction / TxF stubs
        NTSTATUS handle_NtCreateTransaction(const syscall_context& /*c*/, emulator_object<handle> transaction_handle,
                                            ACCESS_MASK /*desired_access*/,
                                            emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                            uint64_t /*uow*/, handle /*tm_handle*/, ULONG /*create_options*/, ULONG /*isolation_level*/,
                                            ULONG /*isolation_flags*/, emulator_object<LARGE_INTEGER> /*timeout*/,
                                            emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*description*/)
        {
            transaction_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenTransaction(const syscall_context& /*c*/, emulator_object<handle> transaction_handle,
                                          ACCESS_MASK /*desired_access*/,
                                          emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/, uint64_t /*uow*/,
                                          handle /*tm_handle*/)
        {
            transaction_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCommitTransaction(const syscall_context& /*c*/, handle /*transaction_handle*/, BOOLEAN /*wait*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRollbackTransaction(const syscall_context& /*c*/, handle /*transaction_handle*/, BOOLEAN /*wait*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationTransaction(const syscall_context& /*c*/, handle /*transaction_handle*/,
                                                      uint32_t /*transaction_info_class*/, uint64_t /*transaction_info*/,
                                                      ULONG /*transaction_info_length*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationTransaction(const syscall_context& /*c*/, handle /*transaction_handle*/,
                                                    uint32_t /*transaction_info_class*/, uint64_t /*transaction_info*/,
                                                    ULONG /*transaction_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateTransactionManager(const syscall_context& /*c*/, emulator_object<handle> tm_handle,
                                                   ACCESS_MASK /*desired_access*/,
                                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                   emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*log_file_name*/,
                                                   ULONG /*create_options*/, ULONG /*commit_strength*/)
        {
            tm_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenTransactionManager(const syscall_context& /*c*/, emulator_object<handle> tm_handle,
                                                 ACCESS_MASK /*desired_access*/,
                                                 emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                 emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*log_file_name*/,
                                                 uint64_t /*tm_identity*/, ULONG /*open_options*/)
        {
            tm_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationTransactionManager(const syscall_context& /*c*/, handle /*tm_handle*/, uint32_t /*tm_info_class*/,
                                                             uint64_t /*tm_info*/, ULONG /*tm_info_length*/,
                                                             emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationTransactionManager(const syscall_context& /*c*/, handle /*tm_handle*/, uint32_t /*tm_info_class*/,
                                                           uint64_t /*tm_info*/, ULONG /*tm_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRecoverTransactionManager(const syscall_context& /*c*/, handle /*tm_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRenameTransactionManager(const syscall_context& /*c*/,
                                                   emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*log_file_name*/,
                                                   uint64_t /*existing_transaction_manager_guid*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRollforwardTransactionManager(const syscall_context& /*c*/, handle /*tm_handle*/,
                                                        emulator_object<LARGE_INTEGER> /*virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateEnlistment(const syscall_context& /*c*/, emulator_object<handle> enlistment_handle,
                                           ACCESS_MASK /*desired_access*/, handle /*resource_manager_handle*/,
                                           handle /*transaction_handle*/,
                                           emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                           ULONG /*create_options*/, ULONG /*notification_mask*/, uint64_t /*enlistment_key*/)
        {
            enlistment_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenEnlistment(const syscall_context& /*c*/, emulator_object<handle> enlistment_handle,
                                         ACCESS_MASK /*desired_access*/, handle /*resource_manager_handle*/, uint64_t /*enlistment_guid*/,
                                         emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            enlistment_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                                     uint32_t /*enlistment_info_class*/, uint64_t /*enlistment_info*/,
                                                     ULONG /*enlistment_info_length*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                                   uint32_t /*enlistment_info_class*/, uint64_t /*enlistment_info*/,
                                                   ULONG /*enlistment_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCommitEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                           emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRollbackEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                             emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPrePrepareEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                               emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtPrepareEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                            emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtReadOnlyEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/,
                                             emulator_object<LARGE_INTEGER> /*tm_virtual_clock*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRecoverEnlistment(const syscall_context& /*c*/, handle /*enlistment_handle*/, uint64_t /*enlistment_key*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRecoverResourceManager(const syscall_context& /*c*/, handle /*resource_manager_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtEnumerateTransactionObject(const syscall_context& /*c*/, handle /*root_directory_handle*/,
                                                     uint32_t /*query_type*/, uint64_t /*object_cursor*/, ULONG /*object_cursor_length*/,
                                                     emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return static_cast<NTSTATUS>(0x8000001AL); // STATUS_NO_MORE_ENTRIES
        }

        NTSTATUS handle_NtCreateRegistryTransaction(const syscall_context& /*c*/, emulator_object<handle> transaction_handle,
                                                    ACCESS_MASK /*desired_access*/,
                                                    emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                    ULONG /*create_options*/)
        {
            transaction_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCommitRegistryTransaction(const syscall_context& /*c*/, handle /*transaction_handle*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRollbackRegistryTransaction(const syscall_context& /*c*/, handle /*transaction_handle*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        // ALPC stubs
        NTSTATUS handle_NtAlpcAcceptConnectPort(const syscall_context& /*c*/, emulator_object<handle> port_handle,
                                                handle /*connection_port_handle*/, ULONG /*flags*/,
                                                emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                uint64_t /*port_attributes*/, uint64_t /*port_context*/, uint64_t /*connection_request*/,
                                                uint64_t /*connection_message_attributes*/, BOOLEAN /*accept_connection*/)
        {
            port_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcCancelMessage(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                            uint64_t /*message_context*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcCreatePortSection(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                handle /*section_handle*/, ULONG /*section_size*/,
                                                emulator_object<uint64_t> alpc_section_handle, emulator_object<ULONG> actual_section_size)
        {
            alpc_section_handle.write_if_valid(0);
            actual_section_size.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcCreateResourceReserve(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                    ULONG /*message_size*/, emulator_object<uint64_t> resource_id)
        {
            resource_id.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcCreateSectionView(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                uint64_t /*view_attributes*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcDeletePortSection(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                uint64_t /*section_handle*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcDeleteResourceReserve(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                    uint64_t /*resource_id*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcDeleteSectionView(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                uint64_t /*view_base*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcDisconnectPort(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcImpersonateClientContainerOfPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*message*/,
                                                               ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcImpersonateClientOfPort(const syscall_context& /*c*/, handle /*port_handle*/, uint64_t /*message*/,
                                                      uint64_t /*required_server_sid*/, ULONG /*flags*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcOpenSenderProcess(const syscall_context& /*c*/, emulator_object<handle> process_handle,
                                                handle /*port_handle*/, uint64_t /*message*/, ULONG /*flags*/,
                                                ACCESS_MASK /*desired_access*/,
                                                emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            process_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcOpenSenderThread(const syscall_context& /*c*/, emulator_object<handle> thread_handle, handle /*port_handle*/,
                                               uint64_t /*message*/, ULONG /*flags*/, ACCESS_MASK /*desired_access*/,
                                               emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            thread_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        // rpcrt4's client-side system-handle unmarshal imports a handle delivered with an ALPC reply by
        // calling NtAlpcQueryInformationMessage(AlpcMessageHandleInformation = 3, index). The output is an
        // ALPC_MESSAGE_HANDLE_INFORMATION {ULONG Index; ULONG Reserved; ULONG Handle; ULONG ObjectType;
        // ULONG GrantedAccess;} (0x14 bytes); rpcrt4 reads Handle@+8, ObjectType@+0xc, GrantedAccess@+0x10.
        // We return the handle stashed by the matching NtAlpcSendWaitReceivePort reply.
        NTSTATUS handle_NtAlpcQueryInformationMessage(const syscall_context& c, handle /*port_handle*/, uint64_t /*message*/,
                                                      uint32_t message_info_class, uint64_t message_info, ULONG length,
                                                      emulator_object<ULONG> return_length)
        {
            constexpr uint32_t alpc_message_handle_information = 3;
            if (message_info_class != alpc_message_handle_information)
            {
                return STATUS_NOT_SUPPORTED;
            }

            constexpr uint32_t info_size = 0x14;
            if (!message_info || length < info_size)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            // The caller passes the requested handle index in the first dword of the buffer.
            const auto index = c.emu.read_memory<uint32_t>(message_info);
            const auto& handles = c.proc.pending_alpc_message_handles;
            if (index >= handles.size())
            {
                return STATUS_NO_MORE_ENTRIES;
            }

            const auto& h = handles[index];
            emulator_object<uint32_t>{c.emu, message_info + 0x00}.write(index);
            emulator_object<uint32_t>{c.emu, message_info + 0x04}.write(0);
            emulator_object<uint32_t>{c.emu, message_info + 0x08}.write(static_cast<uint32_t>(h.handle));
            emulator_object<uint32_t>{c.emu, message_info + 0x0c}.write(h.object_type);
            emulator_object<uint32_t>{c.emu, message_info + 0x10}.write(h.desired_access);

            return_length.write_if_valid(info_size);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAlpcRevokeSecurityContext(const syscall_context& /*c*/, handle /*port_handle*/, ULONG /*flags*/,
                                                    uint64_t /*context_handle*/)
        {
            return STATUS_SUCCESS;
        }

        // Enclave stubs (not supported)
        NTSTATUS handle_NtCreateEnclave(const syscall_context& /*c*/, handle /*process_handle*/, uint64_t /*base_address*/,
                                        ULONG64 /*zero_bits*/, ULONG64 /*size*/, ULONG64 /*initial_commit*/, ULONG /*enclave_type*/,
                                        uint64_t /*enclave_info*/, ULONG /*enclave_info_length*/, emulator_object<ULONG> /*enclave_error*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtInitializeEnclave(const syscall_context& /*c*/, handle /*process_handle*/, uint64_t /*base_address*/,
                                            uint64_t /*enclave_info*/, ULONG /*enclave_info_length*/,
                                            emulator_object<ULONG> /*enclave_error*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtLoadEnclaveData(const syscall_context& /*c*/, handle /*process_handle*/, uint64_t /*base_address*/,
                                          uint64_t /*buffer*/, ULONG64 /*buffer_size*/, ULONG /*protect*/, uint64_t /*page_info*/,
                                          ULONG /*page_info_size*/, emulator_object<ULONG64> /*number_of_bytes_written*/,
                                          emulator_object<ULONG> /*enclave_error*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCallEnclave(const syscall_context& /*c*/, uint64_t /*routine*/, uint64_t /*parameter*/,
                                      BOOLEAN /*wait_for_thread*/, uint64_t /*return_value*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtTerminateEnclave(const syscall_context& /*c*/, uint64_t /*base_address*/, BOOLEAN /*wait_for_threads*/)
        {
            return STATUS_SUCCESS;
        }

        // CpuPartition stubs
        NTSTATUS handle_NtCreateCpuPartition(const syscall_context& /*c*/, emulator_object<handle> partition_handle,
                                             ACCESS_MASK /*desired_access*/,
                                             emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                             ULONG /*reserved*/)
        {
            partition_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenCpuPartition(const syscall_context& /*c*/, emulator_object<handle> partition_handle,
                                           ACCESS_MASK /*desired_access*/,
                                           emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/)
        {
            partition_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationCpuPartition(const syscall_context& /*c*/, handle /*partition_handle*/,
                                                       uint32_t /*cpu_partition_info_class*/, uint64_t /*cpu_partition_info*/,
                                                       ULONG /*cpu_partition_info_length*/, emulator_object<ULONG> return_length)
        {
            return_length.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationCpuPartition(const syscall_context& /*c*/, handle /*partition_handle*/,
                                                     uint32_t /*cpu_partition_info_class*/, uint64_t /*cpu_partition_info*/,
                                                     ULONG /*cpu_partition_info_length*/)
        {
            return STATUS_SUCCESS;
        }

        // CrossVm stubs
        NTSTATUS handle_NtAcquireCrossVmMutant(const syscall_context& /*c*/, handle /*mutant_handle*/, BOOLEAN /*wait*/,
                                               emulator_object<LARGE_INTEGER> /*timeout*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateCrossVmEvent(const syscall_context& /*c*/, emulator_object<handle> event_handle,
                                             ACCESS_MASK /*desired_access*/,
                                             emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                             ULONG /*event_type*/, BOOLEAN /*initial_state*/, uint64_t /*obj_attribute*/)
        {
            event_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateCrossVmMutant(const syscall_context& /*c*/, emulator_object<handle> mutant_handle,
                                              ACCESS_MASK /*desired_access*/,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                              uint64_t /*obj_attribute*/)
        {
            mutant_handle.write_if_valid({});
            return STATUS_SUCCESS;
        }

        // IoRing stubs
        NTSTATUS handle_NtCreateIoRing(const syscall_context& /*c*/, emulator_object<handle> io_ring_handle,
                                       ULONG /*create_parameters_length*/, uint64_t /*create_parameters*/,
                                       ULONG /*output_parameters_length*/, uint64_t /*output_parameters*/)
        {
            io_ring_handle.write_if_valid({});
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtQueryIoRingCapabilities(const syscall_context& /*c*/, ULONG /*capabilities_length*/, uint64_t /*capabilities*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationIoRing(const syscall_context& /*c*/, handle /*io_ring_handle*/, ULONG /*io_ring_info_class*/,
                                               ULONG /*info_length*/, uint64_t /*info*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSubmitIoRing(const syscall_context& /*c*/, handle /*io_ring_handle*/, ULONG /*flags*/, ULONG /*wait_operations*/,
                                       emulator_object<LARGE_INTEGER> /*timeout*/)
        {
            return STATUS_SUCCESS;
        }

        // Physical page stubs
        NTSTATUS handle_NtAllocateUserPhysicalPages(const syscall_context& /*c*/, handle /*process_handle*/,
                                                    emulator_object<ULONG64> number_of_pages, uint64_t /*user_pfn_array*/)
        {
            number_of_pages.write_if_valid(0);
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtAllocateUserPhysicalPagesEx(const syscall_context& /*c*/, handle /*process_handle*/,
                                                      emulator_object<ULONG64> number_of_pages, uint64_t /*user_pfn_array*/,
                                                      uint64_t /*extended_parameters*/, ULONG /*extended_parameters_count*/)
        {
            number_of_pages.write_if_valid(0);
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtFreeUserPhysicalPages(const syscall_context& /*c*/, handle /*process_handle*/,
                                                emulator_object<ULONG64> number_of_pages, uint64_t /*user_pfn_array*/)
        {
            number_of_pages.write_if_valid(0);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtMapUserPhysicalPages(const syscall_context& /*c*/, uint64_t /*virtual_address*/, ULONG64 /*number_of_pages*/,
                                               uint64_t /*user_pfn_array*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtMapUserPhysicalPagesScatter(const syscall_context& /*c*/, uint64_t /*virtual_addresses*/,
                                                      ULONG64 /*number_of_pages*/, uint64_t /*user_pfn_array*/)
        {
            return STATUS_NOT_SUPPORTED;
        }

        // LowBox token stub
        NTSTATUS handle_NtCreateLowBoxToken(const syscall_context& /*c*/, emulator_object<handle> token_handle,
                                            handle /*existing_token_handle*/, ACCESS_MASK /*desired_access*/,
                                            emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                            uint64_t /*package_sid*/, ULONG /*capability_count*/, uint64_t /*capabilities*/,
                                            ULONG /*handle_count*/, uint64_t /*handles*/)
        {
            token_handle.write(DUMMY_IMPERSONATION_TOKEN);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateSymbolicLinkObject(const syscall_context& /*c*/, emulator_object<handle> link_handle,
                                                   ACCESS_MASK /*desired_access*/,
                                                   emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> /*object_attributes*/,
                                                   emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*link_target*/)
        {
            link_handle.write(KNOWN_DLLS_SYMLINK);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryInformationJobObject(const syscall_context& /*c*/, handle /*job_handle*/, uint32_t /*job_info_class*/,
                                                    uint64_t /*job_info*/, ULONG /*job_info_length*/,
                                                    emulator_object<ULONG> /*return_length*/)
        {
            return STATUS_INVALID_HANDLE;
        }

        NTSTATUS handle_NtAccessCheck(const syscall_context& c, const uint64_t security_descriptor, const handle /*client_token*/,
                                      const ACCESS_MASK desired_access, const uint64_t /*generic_mapping*/,
                                      const uint64_t /*privilege_set*/, const emulator_object<ULONG> /*privilege_set_length*/,
                                      const emulator_object<ACCESS_MASK> granted_access, const emulator_object<NTSTATUS> access_status)
        {
            // Kernel: NtAccessCheck → SeAccessCheckByType. Observable contracts:
            //   - SE_DACL_PRESENT not set or Dacl == NULL → null DACL → grant all
            //   - Dacl present with AceCount == 0 → empty DACL → deny all
            //   - Otherwise walk ACEs (below)
            constexpr uint16_t k_dacl_present = 0x0004;
            constexpr uint16_t k_self_relative = 0x8000;

#pragma pack(push, 1)
            struct MinSD
            {
                uint8_t Revision;
                uint8_t Sbz1;
                uint16_t Control;
                uint8_t _pad[4];
                uint64_t Owner;
                uint64_t Group;
                uint64_t Sacl;
                uint64_t Dacl;
            };
            static_assert(sizeof(MinSD) == 40, "SECURITY_DESCRIPTOR layout mismatch");
            static_assert(offsetof(MinSD, Control) == 2);
            static_assert(offsetof(MinSD, Dacl) == 32);

            struct MinACL
            {
                uint8_t AclRevision;
                uint8_t Sbz1;
                uint16_t AclSize;
                uint16_t AceCount;
                uint16_t Sbz2;
            };
            static_assert(sizeof(MinACL) == 8, "ACL header layout mismatch");

            struct MinACE
            {
                uint8_t AceType;
                uint8_t AceFlags;
                uint16_t AceSize;
                uint32_t Mask;
                uint32_t SidStart;
            };
#pragma pack(pop)

            constexpr uint8_t ACCESS_DENIED_ACE_TYPE = 1;
            constexpr uint8_t ACCESS_ALLOWED_ACE_TYPE = 0;

            MinSD sd{};
            if (!c.emu.try_read_memory(security_descriptor, &sd, sizeof(sd)))
            {
                return STATUS_INVALID_PARAMETER;
            }

            if ((sd.Control & k_dacl_present) == 0 || sd.Dacl == 0)
            {
                granted_access.write(desired_access);
                access_status.write(STATUS_SUCCESS);
                return STATUS_SUCCESS;
            }

            uint64_t dacl_ptr = sd.Dacl;
            if ((sd.Control & k_self_relative) != 0)
            {
                dacl_ptr = security_descriptor + static_cast<uint32_t>(sd.Dacl);
            }

            MinACL acl{};
            if (!c.emu.try_read_memory(dacl_ptr, &acl, sizeof(acl)))
            {
                return STATUS_INVALID_PARAMETER;
            }

            if (acl.AceCount == 0)
            {
                granted_access.write(0);
                access_status.write(STATUS_ACCESS_DENIED);
                return STATUS_SUCCESS;
            }

            ACCESS_MASK remaining = desired_access;
            uint64_t ace_ptr = dacl_ptr + sizeof(MinACL);
            for (uint16_t i = 0; i < acl.AceCount && remaining != 0; ++i)
            {
                MinACE ace{};
                if (!c.emu.try_read_memory(ace_ptr, &ace, sizeof(ace)))
                {
                    break;
                }
                if (ace.AceType == ACCESS_DENIED_ACE_TYPE && (ace.Mask & desired_access) != 0)
                {
                    granted_access.write(0);
                    access_status.write(STATUS_ACCESS_DENIED);
                    return STATUS_SUCCESS;
                }
                if (ace.AceType == ACCESS_ALLOWED_ACE_TYPE)
                {
                    remaining &= ~ace.Mask;
                }
                ace_ptr += ace.AceSize;
            }

            if (remaining == 0)
            {
                granted_access.write(desired_access);
                access_status.write(STATUS_SUCCESS);
            }
            else
            {
                granted_access.write(0);
                access_status.write(STATUS_ACCESS_DENIED);
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateUserProcess()
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCreateDebugObject()
        {
            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtAddAtomEx(const syscall_context& c, const uint64_t atom_name, const ULONG length,
                                    const emulator_object<RTL_ATOM> atom, const ULONG /*flags*/)
        {
            std::u16string name{};
            name.resize(length / 2);

            c.emu.read_memory(atom_name, name.data(), length);

            uint16_t index = c.proc.add_or_find_atom(name);
            atom.write(index);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAddAtom(const syscall_context& c, const uint64_t atom_name, const ULONG length,
                                  const emulator_object<RTL_ATOM> atom)
        {
            return handle_NtAddAtomEx(c, atom_name, length, atom, 0);
        }

        NTSTATUS handle_NtDeleteAtom(const syscall_context& c, const RTL_ATOM atom)
        {
            c.proc.delete_atom(atom);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFindAtom(const syscall_context& c, const uint64_t atom_name, const ULONG length,
                                   const emulator_object<uint16_t> atom)
        {
            const auto name = read_string<char16_t>(c.emu, atom_name, length / 2);
            const auto index = c.proc.find_atom(name);
            if (!index)
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            if (atom)
            {
                atom.write(*index);
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryDebugFilterState(const syscall_context& /*c*/)
        {
            return FALSE;
        }

        NTSTATUS handle_NtUserGetDpiForCurrentProcess(const syscall_context& /*c*/)
        {
            return 96;
        }

        uint64_t handle_NtUserGetProcessDpiAwarenessContext(const syscall_context& /*c*/, const uint64_t /*hProcess*/)
        {
            constexpr uint64_t DPI_AWARENESS_CONTEXT_SYSTEM_AWARE = static_cast<uint64_t>(-2);
            return DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
        }

        NTSTATUS handle_NtUserModifyUserStartupInfoFlags(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSystemDebugControl(const syscall_context& /*c*/)
        {
            return STATUS_DEBUGGER_INACTIVE;
        }

        NTSTATUS handle_NtRequestWaitReplyPort(const syscall_context& /*c*/)
        {
            return STATUS_INVALID_HANDLE;
        }

        NTSTATUS handle_NtUserGetProcessUIContextInformation(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUserMapVirtualKeyEx(const syscall_context& /*c*/)
        {
            return 0;
        }

        BOOL handle_NtUserRegisterHotKey()
        {
            return TRUE;
        }

        BOOL handle_NtUserUnregisterHotKey()
        {
            return TRUE;
        }

        uint32_t handle_NtUserSendInput(const syscall_context& /*c*/, const uint32_t count, const uint64_t /*inputs*/,
                                        const int32_t /*cb_size*/)
        {
            return count;
        }

        NTSTATUS handle_NtGdiDdDDISetVidPnSourceOwner()
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGdiDdDDIDestroySynchronizationObject()
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGdiDdDDIUnlock2()
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtGdiDdDDIDestroyPagingQueue()
        {
            return STATUS_SUCCESS;
        }

        BOOL handle_NtUserGetWindowDisplayAffinity(const syscall_context& /*c*/, const uint64_t /*hWnd*/,
                                                   const emulator_object<uint32_t> affinity)
        {
            if (affinity)
            {
                affinity.write(0);
            }
            return TRUE;
        }

        NTSTATUS handle_NtUserToUnicodeEx(const syscall_context& /*c*/)
        {
            return 0;
        }

        NTSTATUS handle_NtUserSetProcessDpiAwarenessContext(const syscall_context& /*c*/)
        {
            return 0;
        }

        ULONG handle_NtUserGetRawInputDeviceList()
        {
            return 0;
        }

        ULONG handle_NtUserGetKeyboardType()
        {
            return 0;
        }

        NTSTATUS handle_NtSubscribeWnfStateChange(const syscall_context& c, uint64_t state_name_ptr, uint64_t last_known_stamp,
                                                  uint64_t state_data_type, uint64_t type_id, uint64_t scope,
                                                  uint64_t subscription_handle_out, uint64_t callback, uint64_t callback_context)
        {
            if (getenv("EMULATOR_LOG_RPC"))
            {
                uint64_t state_name_val = 0;
                if (state_name_ptr)
                {
                    c.emu.read_memory(state_name_ptr, &state_name_val, sizeof(state_name_val));
                }
                c.win_emu.log.error("[wnf-dbg] NtSubscribeWnfStateChange ptr=0x%llx nameVal=0x%llx lastStamp=%llu "
                                    "type=%llu typeId=%llu scope=%llu subHdl=0x%llx cb=0x%llx ctx=0x%llx\n",
                                    (unsigned long long)state_name_ptr, (unsigned long long)state_name_val,
                                    (unsigned long long)last_known_stamp, (unsigned long long)state_data_type, (unsigned long long)type_id,
                                    (unsigned long long)scope, (unsigned long long)subscription_handle_out, (unsigned long long)callback,
                                    (unsigned long long)callback_context);
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUnsubscribeWnfStateChange(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetWnfProcessNotificationEvent(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationDebugObject(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRemoveProcessDebug(const syscall_context& /*c*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtNotifyChangeDirectoryFileEx(const syscall_context& /*c*/)
        {
            return STATUS_PENDING;
        }

        uint64_t handle_NtUserCallNoParam()
        {
            return 0;
        }

        NTSTATUS handle_NtAllocateLocallyUniqueId(const syscall_context& c, const emulator_object<LUID> luid)
        {
            luid.access([&](LUID& l) {
                const std::uint64_t value = c.proc.next_luid++;
                l.LowPart = static_cast<std::uint32_t>(value);
                l.HighPart = static_cast<std::int32_t>(value >> 32);
            });

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAllocateReserveObject(const syscall_context& c, const emulator_object<handle> memory_reserve_handle,
                                                const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                const DWORD type)
        {
            std::u16string name{};
            if (object_attributes)
            {
                const auto attributes = object_attributes.read();
                if (attributes.ObjectName != 0)
                {
                    name = read_unicode_string(c.emu, attributes.ObjectName);
                }
            }

            switch (type)
            {
            case 1: { // MemoryReserveIoCompletion
                // TODO: This probably isn't 100% correct.
                wait_completion_packet packet{};
                packet.name = std::move(name);
                memory_reserve_handle.write(c.proc.wait_completion_packets.store(std::move(packet)));
                return STATUS_SUCCESS;
            }
            case 0:
            default: { // MemoryReserveUserApc
                return STATUS_NOT_SUPPORTED;
            }
            }
        }
    }

    // NOLINTNEXTLINE(readability-function-size,hicpp-function-size)
    void syscall_dispatcher::add_handlers(std::map<std::string, syscall_handler>& handler_mapping)
    {
#define add_handler(syscall)                                                            \
    do                                                                                  \
    {                                                                                   \
        handler_mapping[#syscall] = make_syscall_handler<syscalls::handle_##syscall>(); \
    } while (0)

        add_handler(NtSetInformationThread);
        add_handler(NtSetThreadExecutionState);
        add_handler(NtSetEvent);
        add_handler(NtPulseEvent);
        add_handler(NtSetEventBoostPriority);
        add_handler(NtCreateKeyedEvent);
        add_handler(NtOpenKeyedEvent);
        add_handler(NtReleaseKeyedEvent);
        add_handler(NtWaitForKeyedEvent);
        add_handler(NtClose);
        add_handler(NtOpenKey);
        add_handler(NtAllocateVirtualMemory);
        add_handler(NtQueryInformationProcess);
        add_handler(NtSetInformationProcess);
        add_handler(NtSetInformationVirtualMemory);
        add_handler(NtFreeVirtualMemory);
        add_handler(NtQueryVirtualMemory);
        add_handler(NtOpenThread);
        add_handler(NtOpenThreadToken);
        add_handler(NtOpenThreadTokenEx);
        add_handler(NtQueryPerformanceCounter);
        add_handler(NtQuerySystemInformation);
        add_handler(NtPowerInformation);
        add_handler(NtCreateEvent);
        add_handler(NtProtectVirtualMemory);
        add_handler(NtLockVirtualMemory);
        add_handler(NtUnlockVirtualMemory);
        add_handler(NtFlushVirtualMemory);
        add_handler(NtOpenDirectoryObject);
        add_handler(NtCreateDirectoryObject);
        add_handler(NtTraceEvent);
        add_handler(NtAllocateVirtualMemoryEx);
        add_handler(NtCreateIoCompletion);
        add_handler(NtSetIoCompletion);
        add_handler(NtSetIoCompletionEx);
        add_handler(NtRemoveIoCompletion);
        add_handler(NtCreateWaitCompletionPacket);
        add_handler(NtCreateWorkerFactory);
        add_handler(NtWorkerFactoryWorkerReady);
        add_handler(NtSetInformationWorkerFactory);
        add_handler(NtShutdownWorkerFactory);
        add_handler(NtWaitForWorkViaWorkerFactory);
        add_handler(NtManageHotPatch);
        add_handler(NtOpenSection);
        add_handler(NtMapViewOfSection);
        add_handler(NtMapViewOfSectionEx);
        add_handler(NtOpenSymbolicLinkObject);
        add_handler(NtQuerySymbolicLinkObject);
        add_handler(NtQuerySystemInformationEx);
        add_handler(NtOpenFile);
        add_handler(NtQueryVolumeInformationFile);
        add_handler(NtApphelpCacheControl);
        add_handler(NtCreateSection);
        add_handler(NtQuerySection);
        add_handler(NtConnectPort);
        add_handler(NtSecureConnectPort);
        add_handler(NtCreateFile);
        add_handler(NtDeviceIoControlFile);
        add_handler(NtQueryWnfStateData);
        add_handler(NtSubscribeWnfStateChange);
        add_handler(NtCreateWnfStateName);
        add_handler(NtDeleteWnfStateName);
        add_handler(NtDeleteWnfStateData);
        add_handler(NtFlushKey);
        add_handler(NtAlertThread);
        add_handler(NtAlertResumeThread);
        add_handler(NtAlertMultipleThreadByThreadId);
        add_handler(NtExtendSection);
        add_handler(NtIsProcessInJob);
        add_handler(NtGetNextProcess);
        add_handler(NtImpersonateThread);
        add_handler(NtImpersonateAnonymousToken);
        add_handler(NtAdjustGroupsToken);
        add_handler(NtFilterToken);
        add_handler(NtMakePermanentObject);
        add_handler(NtMakeTemporaryObject);
        add_handler(NtCancelSynchronousIoFile);
        add_handler(NtConvertBetweenAuxiliaryCounterAndPerformanceCounter);
        add_handler(NtGetDevicePowerState);
        add_handler(NtSignalAndWaitForSingleObject);
        add_handler(NtReadVirtualMemoryEx);
        add_handler(NtCreateSectionEx);
        add_handler(NtDisplayString);
        add_handler(NtRenameKey);
        add_handler(NtSetInformationToken);
        add_handler(NtSetInformationSymbolicLink);
        add_handler(NtLoadKey);
        add_handler(NtLoadKey2);
        add_handler(NtUnloadKey);
        add_handler(NtCompareTokens);
        add_handler(NtOpenKeyTransacted);
        add_handler(NtCreateKeyTransacted);
        add_handler(NtFlushInstallUILanguage);
        add_handler(NtGetEnvironmentVariableEx);
        add_handler(NtQuerySystemTime);
        add_handler(NtSetSystemTime);
        add_handler(NtQueryMutant);
        add_handler(NtQuerySemaphore);
        add_handler(NtQueryOpenSubKeys);
        add_handler(NtQueryOpenSubKeysEx);
        add_handler(NtAcquireProcessActivityReference);
        add_handler(NtCreateDirectoryObjectEx);
        add_handler(NtQueryAuxiliaryCounterFrequency);
        add_handler(NtNotifyChangeDirectoryFile);
        add_handler(NtNotifyChangeMultipleKeys);
        add_handler(NtNotifyChangeSession);
        add_handler(NtResetEvent);
        add_handler(NtQueryTimer);
        add_handler(NtOpenIoCompletion);
        add_handler(NtPrivilegeCheck);
        add_handler(NtSetDefaultLocale);
        add_handler(NtSetDefaultUILanguage);
        add_handler(NtSetUuidSeed);
        add_handler(NtIsSystemResumeAutomatic);
        add_handler(NtInitializeRegistry);
        add_handler(NtLoadKeyEx);
        add_handler(NtLoadKey3);
        add_handler(NtReplaceKey);
        add_handler(NtRestoreKey);
        add_handler(NtSaveKey);
        add_handler(NtSaveKeyEx);
        add_handler(NtUnloadKey2);
        add_handler(NtUnloadKeyEx);
        add_handler(NtSetEventEx);
        add_handler(NtSetDebugFilterState);
        add_handler(NtShutdownSystem);
        add_handler(NtSetVolumeInformationFile);
        add_handler(NtSetEaFile);
        add_handler(NtWriteFileGather);
        add_handler(NtReadFileScatter);
        add_handler(NtMapCMFModule);
        add_handler(NtOpenSession);
        add_handler(NtQueryIoCompletion);
        add_handler(NtQueryEaFile);
        add_handler(NtQueryQuotaInformationFile);
        add_handler(NtQueryDirectoryObject);
        add_handler(NtQueryInformationAtom);
        add_handler(NtResumeProcess);
        add_handler(NtSuspendProcess);
        add_handler(NtSaveMergedKeys);
        add_handler(NtSetDefaultHardErrorPort);
        add_handler(NtAccessCheckByType);
        add_handler(NtAccessCheckByTypeResultList);
        add_handler(NtRegisterThreadTerminatePort);
        add_handler(NtReplyPort);
        add_handler(NtRequestPort);
        add_handler(NtThawRegistry);
        add_handler(NtQuerySystemEnvironmentValue);
        add_handler(NtQuerySystemEnvironmentValueEx);
        add_handler(NtSetSystemEnvironmentValue);
        add_handler(NtSetSystemEnvironmentValueEx);
        add_handler(NtRevertContainerImpersonation);
        add_handler(NtDrawText);
        add_handler(NtEnableLastKnownGood);
        add_handler(NtDisableLastKnownGood);
        add_handler(NtQueryEnvironmentVariableInfoEx);
        add_handler(NtQuerySecurityPolicy);
        add_handler(NtSetQuotaInformationFile);
        add_handler(NtSetSystemPowerState);
        add_handler(NtTranslateFilePath);
        add_handler(NtCreateKeyTransacted_Stub);
        add_handler(NtOpenKeyTransactedEx);
        add_handler(NtOpenKeyTransactedEx_Stub);
        add_handler(NtAcceptConnectPort);
        add_handler(NtCreateProcessEx);
        add_handler(NtCreateProfileEx);
        add_handler(NtCreateToken);
        add_handler(NtCreateTokenEx);
        add_handler(NtEnumerateSystemEnvironmentValuesEx);
        add_handler(NtPrepareComplete);
        add_handler(NtPropagationComplete);
        add_handler(NtPropagationFailed);
        add_handler(NtPssCaptureVaSpaceBulk);
        add_handler(NtQueryInformationPort);
        add_handler(NtQueryInformationWorkerFactory);
        add_handler(NtWaitForDebugEvent);
        add_handler(NtWaitLowEventPair);
        add_handler(NtWriteRequestData);
        add_handler(NtdllRunOnceInitMuiCrits);
        add_handler(NtosSecureKernelImportBugcheck);
        add_handler(NtQueryIntervalProfile);
        add_handler(NtSetIntervalProfile);
        add_handler(NtStartProfile);
        add_handler(NtStopProfile);
        add_handler(NtReplyWaitReceivePort);
        add_handler(NtReplyWaitReceivePortEx);
        add_handler(NtReplyWaitReplyPort);
        add_handler(NtResetWriteWatch);
        add_handler(NtSerializeBoot);
        add_handler(NtUnloadDriver);
        add_handler(NtOpenRegistryTransaction);
        add_handler(NtPrivilegeObjectAuditAlarm);
        add_handler(NtPrivilegedServiceAuditAlarm);
        add_handler(NtRegisterProtocolAddressInformation);
        add_handler(NtAssignProcessToJobObject);
        add_handler(NtSetIRTimer);
        add_handler(NtReplacePartitionUnit);
        add_handler(NtCloseObjectAuditAlarm);
        add_handler(NtDeleteObjectAuditAlarm);
        add_handler(NtOpenObjectAuditAlarm);
        add_handler(NtAccessCheckAndAuditAlarm);
        add_handler(NtAccessCheckByTypeAndAuditAlarm);
        add_handler(NtAccessCheckByTypeResultListAndAuditAlarm);
        add_handler(NtAccessCheckByTypeResultListAndAuditAlarmByHandle);
        add_handler(NtChangeProcessState);
        add_handler(NtChangeThreadState);
        add_handler(NtCommitComplete);
        add_handler(NtCompactKeys);
        add_handler(NtCompressKey);
        add_handler(NtCreatePagingFile);
        add_handler(NtCreatePartition);
        add_handler(NtOpenPartition);
        add_handler(NtManagePartition);
        add_handler(NtCreatePort);
        add_handler(NtCreateWaitablePort);
        add_handler(NtListenPort);
        add_handler(NtImpersonateClientOfPort);
        add_handler(NtCreateProcess);
        add_handler(NtCreateProcessStateChange);
        add_handler(NtCreateThreadStateChange);
        add_handler(NtCreateProfile);
        add_handler(NtCreateThread);
        add_handler(NtFreezeRegistry);
        add_handler(NtGetCompleteWnfStateSubscription);
        add_handler(NtGetWriteWatch);
        add_handler(NtInitiatePowerAction);
        add_handler(NtLockProductActivationKeys);
        add_handler(NtLockRegistryKey);
        add_handler(NtPlugPlayControl);
        add_handler(NtPrePrepareComplete);
        add_handler(NtRollbackComplete);
        add_handler(NtSinglePhaseReject);
        add_handler(NtCreateMailslotFile);
        add_handler(NtAddBootEntry);
        add_handler(NtDeleteBootEntry);
        add_handler(NtEnumerateBootEntries);
        add_handler(NtFilterBootOption);
        add_handler(NtModifyBootEntry);
        add_handler(NtSetBootEntryOrder);
        add_handler(NtSetBootOptions);
        add_handler(NtQueryBootEntryOrder);
        add_handler(NtQueryBootOptions);
        add_handler(NtAddDriverEntry);
        add_handler(NtDeleteDriverEntry);
        add_handler(NtEnumerateDriverEntries);
        add_handler(NtModifyDriverEntry);
        add_handler(NtQueryDriverEntryOrder);
        add_handler(NtSetDriverEntryOrder);
        add_handler(NtLoadDriver);
        add_handler(NtCreateJobObject);
        add_handler(NtOpenJobObject);
        add_handler(NtTerminateJobObject);
        add_handler(NtSetInformationJobObject);
        add_handler(NtGetCachedSigningLevel);
        add_handler(NtSetCachedSigningLevel);
        add_handler(NtSetCachedSigningLevel2);
        add_handler(NtCompareSigningLevels);
        add_handler(NtDebugActiveProcess);
        add_handler(NtDebugContinue);
        add_handler(NtGetNotificationResourceManager);
        add_handler(NtCreateResourceManager);
        add_handler(NtOpenResourceManager);
        add_handler(NtQueryInformationResourceManager);
        add_handler(NtSetInformationResourceManager);
        add_handler(NtCreateIRTimer);
        add_handler(NtFreezeTransactions);
        add_handler(NtThawTransactions);
        add_handler(NtCreateTransaction);
        add_handler(NtOpenTransaction);
        add_handler(NtCommitTransaction);
        add_handler(NtRollbackTransaction);
        add_handler(NtQueryInformationTransaction);
        add_handler(NtSetInformationTransaction);
        add_handler(NtCreateTransactionManager);
        add_handler(NtOpenTransactionManager);
        add_handler(NtQueryInformationTransactionManager);
        add_handler(NtSetInformationTransactionManager);
        add_handler(NtRecoverTransactionManager);
        add_handler(NtRenameTransactionManager);
        add_handler(NtRollforwardTransactionManager);
        add_handler(NtCreateEnlistment);
        add_handler(NtOpenEnlistment);
        add_handler(NtQueryInformationEnlistment);
        add_handler(NtSetInformationEnlistment);
        add_handler(NtCommitEnlistment);
        add_handler(NtRollbackEnlistment);
        add_handler(NtPrePrepareEnlistment);
        add_handler(NtPrepareEnlistment);
        add_handler(NtReadOnlyEnlistment);
        add_handler(NtRecoverEnlistment);
        add_handler(NtRecoverResourceManager);
        add_handler(NtEnumerateTransactionObject);
        add_handler(NtCreateRegistryTransaction);
        add_handler(NtCommitRegistryTransaction);
        add_handler(NtRollbackRegistryTransaction);
        add_handler(NtAlpcAcceptConnectPort);
        add_handler(NtAlpcCancelMessage);
        add_handler(NtAlpcCreatePortSection);
        add_handler(NtAlpcCreateResourceReserve);
        add_handler(NtAlpcCreateSectionView);
        add_handler(NtAlpcDeletePortSection);
        add_handler(NtAlpcDeleteResourceReserve);
        add_handler(NtAlpcDeleteSectionView);
        add_handler(NtAlpcDisconnectPort);
        add_handler(NtAlpcImpersonateClientContainerOfPort);
        add_handler(NtAlpcImpersonateClientOfPort);
        add_handler(NtAlpcOpenSenderProcess);
        add_handler(NtAlpcOpenSenderThread);
        add_handler(NtAlpcQueryInformationMessage);
        add_handler(NtAlpcRevokeSecurityContext);
        add_handler(NtCreateEnclave);
        add_handler(NtInitializeEnclave);
        add_handler(NtLoadEnclaveData);
        add_handler(NtCallEnclave);
        add_handler(NtTerminateEnclave);
        add_handler(NtCreateCpuPartition);
        add_handler(NtOpenCpuPartition);
        add_handler(NtQueryInformationCpuPartition);
        add_handler(NtSetInformationCpuPartition);
        add_handler(NtAcquireCrossVmMutant);
        add_handler(NtCreateCrossVmEvent);
        add_handler(NtCreateCrossVmMutant);
        add_handler(NtCreateIoRing);
        add_handler(NtQueryIoRingCapabilities);
        add_handler(NtSetInformationIoRing);
        add_handler(NtSubmitIoRing);
        add_handler(NtAllocateUserPhysicalPages);
        add_handler(NtAllocateUserPhysicalPagesEx);
        add_handler(NtFreeUserPhysicalPages);
        add_handler(NtMapUserPhysicalPages);
        add_handler(NtMapUserPhysicalPagesScatter);
        add_handler(NtCreateLowBoxToken);
        add_handler(NtCreateSymbolicLinkObject);
        add_handler(NtOpenProcess);
        add_handler(NtOpenProcessToken);
        add_handler(NtOpenProcessTokenEx);
        add_handler(NtQuerySecurityAttributesToken);
        add_handler(NtAdjustPrivilegesToken);
        add_handler(NtQueryLicenseValue);
        add_handler(NtTestAlert);
        add_handler(NtContinue);
        add_handler(NtContinueEx);
        add_handler(NtTerminateProcess);
        add_handler(NtFlushProcessWriteBuffers);
        add_handler(NtWriteFile);
        add_handler(NtCopyFileChunk);
        add_handler(NtLockFile);
        add_handler(NtUnlockFile);
        add_handler(NtRaiseHardError);
        add_handler(NtCreateSemaphore);
        add_handler(NtOpenSemaphore);
        add_handler(NtReadVirtualMemory);
        add_handler(NtWriteVirtualMemory);
        add_handler(NtQueryInformationToken);
        add_handler(NtDxgkIsFeatureEnabled);
        add_handler(NtAddAtomEx);
        add_handler(NtAddAtom);
        add_handler(NtFindAtom);
        add_handler(NtDeleteAtom);
        add_handler(NtUserGetAtomName);
        add_handler(NtInitializeNlsFiles);
        add_handler(NtUnmapViewOfSection);
        add_handler(NtUnmapViewOfSectionEx);
        add_handler(NtDuplicateObject);
        add_handler(NtQueryInformationThread);
        add_handler(NtQueryWnfStateNameInformation);
        add_handler(NtAlpcSendWaitReceivePort);
        add_handler(NtGdiInit);
        add_handler(NtGdiGetDeviceCaps);
        add_handler(NtGdiGetDeviceCapsAll);
        add_handler(NtGdiComputeXformCoefficients);
        add_handler(NtGdiFlush);
        add_handler(NtGdiCreateSolidBrush);
        add_handler(NtGdiCreatePatternBrushInternal);
        add_handler(NtGdiCreatePen);
        add_handler(NtGdiCreateCompatibleDC);
        add_handler(NtGdiSaveDC);
        add_handler(NtGdiRestoreDC);
        add_handler(NtGdiAddFontMemResourceEx);
        add_handler(NtGdiRemoveFontMemResourceEx);
        add_handler(NtGdiCreateCompatibleBitmap);
        add_handler(NtGdiCreateBitmap);
        add_handler(NtGdiCreateDIBitmapInternal);
        add_handler(NtGdiSetDIBitsToDeviceInternal);
        add_handler(NtGdiGetDIBitsInternal);
        add_handler(NtGdiGetBitmapBits);
        add_handler(NtGdiStretchDIBitsInternal);
        add_handler(NtGdiDeleteObjectApp);
        add_handler(NtGdiSelectBitmap);
        add_handler(NtGdiGetDCforBitmap);
        add_handler(NtGdiGetDCDword);
        add_handler(NtGdiSetBrushOrg);
        add_handler(NtGdiHfontCreate);
        add_handler(NtGdiExtGetObjectW);
        add_handler(NtGdiEnumFonts);
        add_handler(NtGdiGetTextCharsetInfo);
        add_handler(NtGdiQueryFontAssocInfo);
        add_handler(NtGdiGetTextMetricsW);
        add_handler(NtGdiGetTextFaceW);
        add_handler(NtGdiGetTextExtent);
        add_handler(NtGdiGetCharWidthW);
        add_handler(NtGdiGetGlyphOutline);
        add_handler(NtGdiCreateRectRgn);
        add_handler(NtGdiGetRandomRgn);
        add_handler(NtGdiIntersectClipRect);
        add_handler(NtGdiGetCharSet);
        add_handler(NtGdiExtSelectClipRgn);
        add_handler(NtGdiLineTo);
        add_handler(NtGdiRectangle);
        add_handler(NtGdiPatBlt);
        add_handler(NtGdiBitBlt);
        add_handler(NtGdiStretchBlt);
        add_handler(NtGdiPolyPatBlt);
        add_handler(NtGdiExtTextOutW);
        add_handler(NtGdiGetRealizationInfo);
        add_handler(NtGdiGetEntry);
        add_handler(NtGdiInit2);
        add_handler(NtGdiMoveToEx);
        add_handler(NtGdiSelectBrushLocal);
        add_handler(NtGdiSelectPenLocal);
        add_handler(NtUserGetThreadState);
        add_handler(NtUserSetThreadState);
        add_handler(NtUserProcessConnect);
        add_handler(NtUserInitializeClientPfnArrays);
        add_handler(NtUserRemoteConnectState);
        add_handler(NtUserGetThreadDesktop);
        add_handler(NtOpenKeyEx);
        add_handler(NtUserTraceLoggingSendMixedModeTelemetry);
        add_handler(NtUserCitSetInfo);
        add_handler(NtUserDisplayConfigGetDeviceInfo);
        add_handler(NtOpenEvent);
        add_handler(NtGetMUIRegistryInfo);
        add_handler(NtIsUILanguageComitted);
        add_handler(NtQueryDefaultUILanguage);
        add_handler(NtQueryInstallUILanguage);
        add_handler(NtUpdateWnfStateData);
        add_handler(NtRaiseException);
        add_handler(NtQueryInformationJobObject);
        add_handler(NtSetSystemInformation);
        add_handler(NtAllocateUuids);
        add_handler(NtQueryInformationFile);
        add_handler(NtCreateThreadEx);
        add_handler(NtQueryDebugFilterState);
        add_handler(NtWaitForSingleObject);
        add_handler(NtTerminateThread);
        add_handler(NtDelayExecution);
        add_handler(NtWaitForAlertByThreadId);
        add_handler(NtAlertThreadByThreadIdEx);
        add_handler(NtAlertThreadByThreadId);
        add_handler(NtReadFile);
        add_handler(NtSetInformationFile);
        add_handler(NtUserRegisterWindowMessage);
        add_handler(NtQueryValueKey);
        add_handler(NtQueryMultipleValueKey);
        add_handler(NtQueryKey);
        add_handler(NtGetNlsSectionPtr);
        add_handler(NtAccessCheck);
        add_handler(NtCreateKey);
        add_handler(NtSetValueKey);
        add_handler(NtDeleteValueKey);
        add_handler(NtDeleteKey);
        add_handler(NtNotifyChangeKey);
        add_handler(NtGetCurrentProcessorNumberEx);
        add_handler(NtGetCurrentProcessorNumber);
        add_handler(NtQueryObject);
        add_handler(NtCompareObjects);
        add_handler(NtQueryAttributesFile);
        add_handler(NtWaitForMultipleObjects);
        add_handler(NtWaitForMultipleObjects32);
        add_handler(NtCreateMutant);
        add_handler(NtReleaseMutant);
        add_handler(NtCreatePrivateNamespace);
        add_handler(NtOpenPrivateNamespace);
        add_handler(NtDeletePrivateNamespace);
        add_handler(NtDuplicateToken);
        add_handler(NtQueryTimerResolution);
        add_handler(NtSetInformationKey);
        add_handler(NtUserGetKeyboardLayout);
        add_handler(NtQueryDirectoryFileEx);
        add_handler(NtQueryDirectoryFile);
        add_handler(NtUserSystemParametersInfo);
        add_handler(NtGetContextThread);
        add_handler(NtYieldExecution);
        add_handler(NtUserModifyUserStartupInfoFlags);
        add_handler(NtUserGetDCEx);
        add_handler(NtUserGetDC);
        add_handler(NtUserGetWindowDC);
        add_handler(NtUserGetControlBrush);
        add_handler(NtUserGetOemBitmapSize);
        add_handler(NtUserSetCapture);
        add_handler(NtUserReleaseCapture);
        add_handler(NtUserRegisterRawInputDevices);
        add_handler(NtUserGetRawInputData);
        add_handler(NtUserDefSetText);
        add_handler(NtUserSetWindowState);
        add_handler(NtUserClearWindowState);
        add_handler(NtUserDisableProcessWindowsGhosting);
        add_handler(NtUserBitBltSysBmp);
        add_handler(NtUserGetClientRect);
        add_handler(NtUserBeginPaint);
        add_handler(NtUserEndPaint);
        add_handler(NtUserGetDpiForCurrentProcess);
        add_handler(NtReleaseSemaphore);
        add_handler(NtEnumerateKey);
        add_handler(NtEnumerateValueKey);
        add_handler(NtAlpcCreatePort);
        add_handler(NtAlpcConnectPortEx);
        add_handler(NtAlpcConnectPort);
        add_handler(NtAlpcQueryInformation);
        add_handler(NtGetNextThread);
        add_handler(NtSetInformationObject);
        add_handler(NtUserGetCursorPos);
        add_handler(NtUserTransformPoint);
        add_handler(NtUserShowCursor);
        add_handler(NtUserClipCursor);
        add_handler(NtUserSetCursorPos);
        add_handler(NtUserGetKeyState);
        add_handler(NtUserGetAsyncKeyState);
        add_handler(NtUserReleaseDC);
        add_handler(NtUserFindExistingCursorIcon);
        add_handler(NtUserCreateEmptyCursorObject);
        add_handler(NtUserSetCursorIconData);
        add_handler(NtUserSetCursorIconDataEx);
        add_handler(NtUserGetRequiredCursorSizes);
        add_handler(NtUserDestroyCursor);
        add_handler(NtUserGetCursorFrameInfo);
        add_handler(NtUserGetIconSize);
        add_handler(NtUserDrawIconEx);
        add_handler(NtUserMessageBeep);
        add_handler(NtSetContextThread);
        add_handler(NtUserFindWindowEx);
        add_handler(NtUserMoveWindow);
        add_handler(NtSystemDebugControl);
        add_handler(NtRequestWaitReplyPort);
        add_handler(NtQueryDefaultLocale);
        add_handler(NtSetTimerResolution);
        add_handler(NtSuspendThread);
        add_handler(NtResumeThread);
        add_handler(NtClearEvent);
        add_handler(NtTraceControl);
        add_handler(NtUserGetProcessUIContextInformation);
        add_handler(NtQueueApcThreadEx2);
        add_handler(NtQueueApcThreadEx);
        add_handler(NtQueueApcThread);
        add_handler(NtCreateUserProcess);
        add_handler(NtCreateNamedPipeFile);
        add_handler(NtFsControlFile);
        add_handler(NtQueryFullAttributesFile);
        add_handler(NtFlushBuffersFile);
        add_handler(NtFlushBuffersFileEx);
        add_handler(NtDeleteFile);
        add_handler(NtCancelIoFile);
        add_handler(NtCancelIoFileEx);
        add_handler(NtAreMappedFilesTheSame);
        add_handler(NtUserGetProcessWindowStation);
        add_handler(NtUserCallHwndParam);
        add_handler(NtUserRegisterClassExWOW);
        add_handler(NtUserUnregisterClass);
        add_handler(NtUserSetWindowsHookEx);
        add_handler(NtUserUnhookWindowsHookEx);
        add_handler(NtUserCreateWindowEx);
        add_handler(NtUserShowWindow);
        add_handler(NtUserMessageCall);
        add_handler(NtUserDispatchMessage);
        add_handler(NtUserTranslateMessage);
        add_handler(NtUserGetMessage);
        add_handler(NtUserPeekMessage);
        add_handler(NtUserWaitMessage);
        add_handler(NtUserInvalidateRect);
        add_handler(NtUserValidateRect);
        add_handler(NtUserUpdateWindow);
        add_handler(NtUserMapVirtualKeyEx);
        add_handler(NtUserToUnicodeEx);
        add_handler(NtUserSetProcessDpiAwarenessContext);
        add_handler(NtUserGetRawInputDeviceList);
        add_handler(NtUserGetKeyboardType);
        add_handler(NtUserEnumDisplayDevices);
        add_handler(NtUserEnumDisplaySettings);
        add_handler(NtUserChangeDisplaySettings);
        add_handler(NtUserBuildHwndList);
        add_handler(NtUserEnumDisplayMonitors);
        add_handler(NtUserSetProp);
        add_handler(NtUserSetProp2);
        add_handler(NtUserChangeWindowMessageFilterEx);
        add_handler(NtUserDestroyWindow);
        add_handler(NtQueryInformationByName);
        add_handler(NtUserSetCursor);
        add_handler(NtUserGetCursor);
        add_handler(NtUserHwndQueryRedirectionInfo);
        add_handler(NtOpenMutant);
        add_handler(NtOpenTimer);
        add_handler(NtCreateTimer);
        add_handler(NtCreateTimer2);
        add_handler(NtSetTimer);
        add_handler(NtSetTimer2);
        add_handler(NtSetTimerEx);
        add_handler(NtCancelTimer);
        add_handler(NtCancelTimer2);
        add_handler(NtAssociateWaitCompletionPacket);
        add_handler(NtCancelWaitCompletionPacket);
        add_handler(NtSetWnfProcessNotificationEvent);
        add_handler(NtUnsubscribeWnfStateChange);
        add_handler(NtQuerySecurityObject);
        add_handler(NtQueryEvent);
        add_handler(NtRemoveIoCompletionEx);
        add_handler(NtCreateDebugObject);
        add_handler(NtReleaseWorkerFactoryWorker);
        add_handler(NtAlpcCreateSecurityContext);
        add_handler(NtAlpcDeleteSecurityContext);
        add_handler(NtSetSecurityObject);
        add_handler(NtSetInformationDebugObject);
        add_handler(NtRemoveProcessDebug);
        add_handler(NtNotifyChangeDirectoryFileEx);
        add_handler(NtUserGetHDevName);
        add_handler(NtUserGetMonitorInfo);
        add_handler(NtFlushInstructionCache);
        add_handler(NtUserMapDesktopObject);
        add_handler(NtAlpcSetInformation);
        add_handler(NtUserTransformRect);
        add_handler(NtUserSetWindowPos);
        add_handler(NtUserSetForegroundWindow);
        add_handler(NtUserGetForegroundWindow);
        add_handler(NtUserSetFocus);
        add_handler(NtUserSetWindowLongPtr);
        add_handler(NtUserSetClassLongPtr);
        add_handler(NtUserSetWindowLong);
        add_handler(NtUserGetWindowLongPtr);
        add_handler(NtUserGetWindowLong);
        add_handler(NtUserGetAncestor);
        add_handler(NtUserPostMessage);
        add_handler(NtUserPostThreadMessage);
        add_handler(NtUserRedrawWindow);
        add_handler(NtUserGetCPD);
        add_handler(NtUserSetWindowFNID);
        add_handler(NtUserSetDialogPointer);
        add_handler(NtUserSetDialogSystemMenu);
        add_handler(NtUserSetMsgBox);
        add_handler(NtUserEnableWindow);
        add_handler(NtUserDeleteMenu);
        add_handler(NtUserGetSystemMenu);
        add_handler(NtCallbackReturn);
        add_handler(NtUserPostQuitMessage);
        add_handler(NtUserGetClassInfoEx);
        add_handler(NtUserGetClassName);
        add_handler(NtUserCallNoParam);
        add_handler(NtUserGetDisplayConfigBufferSizes);
        add_handler(NtUserQueryDisplayConfig);
        add_handler(NtGdiDdDDIEnumAdapters2);
        add_handler(NtDxgkEnumAdapters3);
        add_handler(NtDxgkGetProperties);
        add_handler(NtGdiDdDDICloseAdapter);
        add_handler(NtGdiDdDDIQueryAdapterInfo);
        add_handler(NtGdiDdDDICreateDevice);
        add_handler(NtGdiDdDDICreatePagingQueue);
        add_handler(NtGdiDdDDICreateSynchronizationObject);
        add_handler(NtGdiDdDDILock2);
        add_handler(NtUserGetProcessDpiAwarenessContext);
        add_handler(NtGdiGetCurrentDpiInfo);
        add_handler(NtUserRegisterHotKey);
        add_handler(NtUserSendInput);
        add_handler(NtUserGetWindowDisplayAffinity);
        add_handler(NtGdiDdDDISetVidPnSourceOwner);
        add_handler(NtUserUnregisterHotKey);
        add_handler(NtGdiDdDDIDestroySynchronizationObject);
        add_handler(NtGdiDdDDIUnlock2);
        add_handler(NtGdiDdDDIDestroyPagingQueue);
        add_handler(NtGdiDdDDIEscape);
        add_handler(NtGdiDdDDICreateContext);
        add_handler(NtGdiDdDDICreateAllocation);
        add_handler(NtGdiDdDDIQueryResourceInfo);
        add_handler(NtGdiDdDDIOpenResource);
        add_handler(NtGdiDdDDILock);
        add_handler(NtGdiDdDDIGetDisplayModeList);
        add_handler(NtGdiDdDDIGetSharedPrimaryHandle);
        add_handler(NtGdiDdDDIGetDeviceState);
        add_handler(NtGdiDdDDIMarkDeviceAsError);
        add_handler(NtGdiDdDDIGetCachedHybridQueryValue);
        add_handler(NtGdiDdDDICacheHybridQueryValue);
        add_handler(NtGdiDdDDIUnlock);
        add_handler(NtGdiDdDDIDestroyAllocation2);
        add_handler(NtGdiDdDDIDestroyAllocation);
        add_handler(NtGdiDdDDIDestroyContext);
        add_handler(NtGdiDdDDIDestroyDevice);
        add_handler(NtAllocateLocallyUniqueId);
        add_handler(NtUserAllowSetForegroundWindow);
        add_handler(NtGdiOpenDCW);
        add_handler(NtGdiDdDDIOpenAdapterFromLuid);
        add_handler(NtGdiDdDDIOpenAdapterFromHdc);
        add_handler(NtGdiDdDDISubmitCommand);
        add_handler(NtGdiDdDDIPresent);
        add_handler(NtGdiSelectFont);
        add_handler(NtUserInitThreadCoreMessagingIocp2);
        add_handler(NtUserDrainThreadCoreMessagingCompletions2);
        add_handler(NtUserSetTimer);
        add_handler(NtUserKillTimer);
        add_handler(NtUserValidateTimerCallback);
        add_handler(NtAllocateReserveObject);
        add_handler(NtUserMsgWaitForMultipleObjectsEx);
        add_handler(NtUserGetQueueStatusReadonly);
        add_handler(NtUserGetQueueStatus);
        add_handler(NtUserScheduleDispatchNotification);
        add_handler(NtGdiExtCreateRegion);
        add_handler(NtUserSetWindowRgn);
        add_handler(NtUserAlterWindowStyle);
        add_handler(NtUserSetActiveWindow);
        add_handler(NtGdiTransparentBlt);
        add_handler(NtUserCreateAcceleratorTable);
        add_handler(NtUserTranslateAccelerator);
        add_handler(NtGdiSetLayout);
        add_handler(NtGdiGetDCObject);
        add_handler(NtUserCreateMenu);
        add_handler(NtUserThunkedMenuItemInfo);
        add_handler(NtUserIsTouchWindow);
        add_handler(NtUserCreatePopupMenu);
        add_handler(NtUserSetMenu);
        add_handler(NtUserRemoveMenu);
        add_handler(NtUserDestroyMenu);
        add_handler(NtUserDrawMenuBar);
        add_handler(NtUserSetWindowCompositionAttribute);
        add_handler(NtUserGetWindowPlacement);
        add_handler(NtUserCreateCaret);
        add_handler(NtUserDestroyCaret);
        add_handler(NtUserSetCaretPos);
        add_handler(NtUserShowCaret);
        add_handler(NtUserHideCaret);
        add_handler(NtUserGetObjectInformation);
        add_handler(NtUserQueryWindow);
        add_handler(NtUserSetScrollInfo);
        add_handler(NtUserTrackMouseEvent);
        add_handler(NtGdiGetOutlineTextMetricsInternalW);
        add_handler(NtGdiSetPixel);
        add_handler(NtGdiGetPixel);
        add_handler(NtGdiCreatePaletteInternal);
        add_handler(NtGdiCreateHalftonePalette);
        add_handler(NtGdiDoPalette);
        add_handler(NtUserSelectPalette);
        add_handler(NtGdiCreateDIBSection);
        add_handler(NtUserGetKeyNameText);
        add_handler(NtUserWindowFromPoint);
        add_handler(NtUserSwapMouseButton);

#undef add_handler
    }

    void syscall_dispatcher::add_callbacks()
    {
#define add_callback(syscall, completion_state)                                                                                      \
    do                                                                                                                               \
    {                                                                                                                                \
        this->completion_handlers_[callback_id::syscall] = make_syscall_handler<syscalls::completion_##syscall>();                   \
        syscall_dispatcher::completion_state_factories_[callback_id::syscall] = [] { return std::make_unique<completion_state>(); }; \
    } while (0)

#define add_stateless_callback(syscall)                                                                            \
    do                                                                                                             \
    {                                                                                                              \
        this->completion_handlers_[callback_id::syscall] = make_syscall_handler<syscalls::completion_##syscall>(); \
    } while (0)

        add_stateless_callback(NtUserGetThreadState);
        add_callback(NtUserCreateWindowEx, window_create_state);
        add_callback(NtUserDestroyWindow, window_destroy_state);
        add_callback(NtUserShowWindow, window_show_state);
        add_callback(NtUserMessageCall, message_call_state);
        add_stateless_callback(NtUserEnumDisplayMonitors);

#undef add_callback
#undef add_stateless_callback
    }

} // namespace sogen
