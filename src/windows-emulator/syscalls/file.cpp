#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "utils/io.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <utils/finally.hpp>
#include <utils/string.hpp>
#include <utils/wildcard.hpp>
#include "utils/stat.hpp"

#include <sys/stat.h>

#include "../devices/named_pipe.hpp"

namespace sogen
{

    namespace syscalls
    {
        namespace
        {
            bool pipe_io_trace_enabled()
            {
                static const bool value = std::getenv("SOGEN_TRACE_PIPE_IO") != nullptr;
                return value;
            }

            bool pipe_io_bytes_trace_enabled()
            {
                static const bool value = std::getenv("SOGEN_TRACE_PIPE_IO_BYTES") != nullptr;
                return value;
            }

            bool has_valid_filename_characters(const std::u16string_view path)
            {
                constexpr std::u16string_view invalid_characters = u"\"<>|*?";
                return path.find_first_of(invalid_characters) == std::u16string_view::npos;
            }

            std::u16string resolve_system_root_path(const syscall_context& c, std::u16string filename)
            {
                auto filename_upper = filename;
                std::ranges::transform(filename_upper, filename_upper.begin(), ::towupper);

                constexpr std::u16string_view system_root_prefix = u"\\SYSTEMROOT";
                if (filename_upper != system_root_prefix &&
                    !(filename_upper.size() > system_root_prefix.size() && filename_upper.starts_with(system_root_prefix) &&
                      windows_path_detail::is_slash(filename_upper[system_root_prefix.size()])))
                {
                    return filename;
                }

                const auto system_root =
                    c.proc.kusd.access([](const KUSER_SHARED_DATA64& kusd) { return std::u16string{kusd.NtSystemRoot.arr}; });
                const auto suffix = std::u16string_view{filename}.substr(system_root_prefix.size());
                filename = system_root;

                if (!suffix.empty() && windows_path_detail::is_slash(filename.back()) && windows_path_detail::is_slash(suffix.front()))
                {
                    filename.append(suffix.substr(1));
                }
                else
                {
                    filename.append(suffix);
                }

                return filename;
            }

            // The counterpart of windows_path::to_device_path. Sogen reports file names in volume-device form
            // (NtQueryObject, NtQueryVirtualMemory, ...) and code that consumes them - ntmarta walking a
            // directory's parents for an ACL check, for one - opens them again as-is. Rewrite the volume back
            // into its drive letter so the path reaches the file system instead of the device registry. A bare
            // volume with no path behind it is a real volume handle and stays a device.
            std::u16string resolve_volume_device_path(std::u16string filename)
            {
                constexpr std::u16string_view volume_prefix = u"\\Device\\HarddiskVolume";
                if (!filename.starts_with(volume_prefix))
                {
                    return filename;
                }

                const std::u16string_view remainder{filename};
                const auto separator = remainder.find(u'\\', volume_prefix.size());
                if (separator == std::u16string_view::npos)
                {
                    return filename;
                }

                const auto number = u16_to_u8(remainder.substr(volume_prefix.size(), separator - volume_prefix.size()));

                int volume_index{};
                const auto* number_start = number.data();
                const auto* number_end = number_start + number.size();
                const auto [parse_end, parse_error] = std::from_chars(number_start, number_end, volume_index);
                if (parse_error != std::errc{} || parse_end != number_end || volume_index < 1 || volume_index > 26)
                {
                    return filename;
                }

                std::u16string path = u"\\??\\";
                path.push_back(static_cast<char16_t>(u'a' + volume_index - 1));
                path.push_back(u':');
                path.append(remainder.substr(separator));

                return path;
            }

            std::pair<utils::file_handle, NTSTATUS> open_file(const file_system& file_sys, const windows_path& path,
                                                              const std::u16string& mode)
            {
                FILE* file{};
                const auto error = open_unicode(&file, file_sys.translate(path), mode);

                if (file)
                {
                    return {file, STATUS_SUCCESS};
                }

                using fh = utils::file_handle;

                switch (error)
                {
                case ENOENT:
                    return {fh{}, STATUS_OBJECT_NAME_NOT_FOUND};
                case EACCES:
                    return {fh{}, STATUS_ACCESS_DENIED};
                case EISDIR:
                    return {fh{}, STATUS_FILE_IS_A_DIRECTORY};
                default:
                    return {fh{}, STATUS_NOT_SUPPORTED};
                }
            }

            NTSTATUS perform_file_rename(const syscall_context& c, file& f, const uint64_t root_directory, std::u16string new_name,
                                         const bool replace_if_exists)
            {
                if (root_directory)
                {
                    const auto* root = c.proc.files.get(root_directory);
                    if (!root)
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    const auto has_separator = root->name.ends_with(u"\\") || root->name.ends_with(u"/");
                    new_name = root->name + (has_separator ? u"" : u"\\") + new_name;
                }

                c.win_emu.log.warn("--> File rename requested: %s --> %s\n", u16_to_u8(f.name).c_str(), u16_to_u8(new_name).c_str());

                std::error_code ec{};
                const bool file_exists = std::filesystem::exists(c.win_emu.file_sys.translate(new_name), ec);

                if (ec)
                {
                    return STATUS_ACCESS_DENIED;
                }

                if (!replace_if_exists && file_exists)
                {
                    return STATUS_OBJECT_NAME_EXISTS;
                }

                f.handle.defer_rename(c.win_emu.file_sys.translate(f.name), c.win_emu.file_sys.translate(new_name));

                return STATUS_SUCCESS;
            }
        }

        NTSTATUS handle_NtSetInformationFile(const syscall_context& c, const handle file_handle,
                                             const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                             const uint64_t file_information, const ULONG length, const FILE_INFORMATION_CLASS info_class)
        {
            auto* f = c.proc.files.get(file_handle);
            if (!f)
            {
                auto* device = c.proc.devices.get(file_handle);
                if (!device)
                {
                    return STATUS_INVALID_HANDLE;
                }

                if (info_class == FileCompletionInformation || info_class == FileReplaceCompletionInformation)
                {
                    if (length < sizeof(FILE_COMPLETION_INFORMATION))
                    {
                        return STATUS_INFO_LENGTH_MISMATCH;
                    }

                    const auto info = c.emu.read_memory<FILE_COMPLETION_INFORMATION>(file_information);

                    if (!c.proc.io_completions.get(info.Port))
                    {
                        return STATUS_INVALID_HANDLE;
                    }

                    if (pipe_io_trace_enabled())
                    {
                        const auto* pipe = device->get_internal_device<named_pipe>();
                        c.win_emu.log.info("[pipe-io-trace] NtSetInformationFile(%s) pipe='%s' port=0x%llx key=0x%llx tid=%u\n",
                                           info_class == FileReplaceCompletionInformation ? "FileReplaceCompletionInformation"
                                                                                          : "FileCompletionInformation",
                                           pipe ? u16_to_u8(pipe->name).c_str() : "<non-pipe-device>",
                                           static_cast<unsigned long long>(info.Port), static_cast<unsigned long long>(info.Key),
                                           c.thread().id);
                    }

                    device->associate_completion_port(handle{.bits = info.Port}, info.Key);
                }

                return STATUS_SUCCESS;
            }

            if (info_class == FileBasicInformation)
            {
                return STATUS_SUCCESS;
            }

            if (info_class == FileRenameInformation)
            {
                if (length < sizeof(FILE_RENAME_INFORMATION))
                {
                    return STATUS_BUFFER_OVERFLOW;
                }

                const auto info = c.emu.read_memory<FILE_RENAME_INFORMATION>(file_information);
                auto new_name =
                    read_string<char16_t>(c.emu, file_information + offsetof(FILE_RENAME_INFORMATION, FileName), info.FileNameLength / 2);

                return perform_file_rename(c, *f, info.RootDirectory, std::move(new_name), info.ReplaceIfExists != 0);
            }

            if (info_class == FileRenameInformationEx)
            {
                if (length < sizeof(FILE_RENAME_INFORMATION_EX))
                {
                    return STATUS_BUFFER_OVERFLOW;
                }

                const auto info = c.emu.read_memory<FILE_RENAME_INFORMATION_EX>(file_information);
                auto new_name = read_string<char16_t>(c.emu, file_information + offsetof(FILE_RENAME_INFORMATION_EX, FileName),
                                                      info.FileNameLength / 2);

                return perform_file_rename(c, *f, info.RootDirectory, std::move(new_name),
                                           (info.Flags & FILE_RENAME_REPLACE_IF_EXISTS) != 0);
            }

            if (info_class == FileDispositionInformation)
            {
                if (length < sizeof(FILE_DISPOSITION_INFORMATION))
                {
                    return STATUS_BUFFER_OVERFLOW;
                }

                if (!(f->access_mask & DELETE))
                {
                    return STATUS_ACCESS_DENIED;
                }

                const auto info = c.emu.read_memory<FILE_DISPOSITION_INFORMATION>(file_information);

                c.win_emu.callbacks.on_generic_access(info.DeleteFile ? "Marking for deletion" : "Clearing deletion marker", f->name);

                f->handle.defer_delete(info.DeleteFile ? c.win_emu.file_sys.translate(f->name) : std::filesystem::path{});

                return STATUS_SUCCESS;
            }

            if (info_class == FileDispositionInformationEx)
            {
                if (length < sizeof(FILE_DISPOSITION_INFORMATION_EX))
                {
                    return STATUS_INFO_LENGTH_MISMATCH;
                }

                const auto info = c.emu.read_memory<FILE_DISPOSITION_INFORMATION_EX>(file_information);
                const bool wants_delete = (info.Flags & FILE_DISPOSITION_DELETE) != 0;

                if (wants_delete && !(f->access_mask & DELETE))
                {
                    return STATUS_ACCESS_DENIED;
                }

                c.win_emu.callbacks.on_generic_access(wants_delete ? "Marking for deletion" : "Clearing deletion marker", f->name);

                f->handle.defer_delete(wants_delete ? c.win_emu.file_sys.translate(f->name) : std::filesystem::path{});

                return STATUS_SUCCESS;
            }

            if (info_class == FilePositionInformation)
            {
                if (!f->handle)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                if (io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = sizeof(FILE_POSITION_INFORMATION);
                    io_status_block.write(block);
                }

                if (length != sizeof(FILE_POSITION_INFORMATION))
                {
                    return STATUS_BUFFER_OVERFLOW;
                }

                const emulator_object<FILE_POSITION_INFORMATION> info{c.emu, file_information};
                const auto i = info.read();

                if (!f->handle.seek_to(i.CurrentByteOffset.QuadPart))
                {
                    return STATUS_INVALID_PARAMETER;
                }

                return STATUS_SUCCESS;
            }

            if (info_class == FileEndOfFileInformation)
            {
                if (!f->handle)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                if (length < sizeof(FILE_END_OF_FILE_INFORMATION))
                {
                    return STATUS_BUFFER_OVERFLOW;
                }

                const auto info = c.emu.read_memory<FILE_END_OF_FILE_INFORMATION>(file_information);

                if (!f->handle.resize(info.EndOfFile.QuadPart))
                {
                    return STATUS_INVALID_PARAMETER;
                }

                return STATUS_SUCCESS;
            }

            if (info_class == FileAllocationInformation)
            {
                return STATUS_SUCCESS;
            }

            if (info_class == FileCompletionInformation || info_class == FileReplaceCompletionInformation)
            {
                if (length < sizeof(FILE_COMPLETION_INFORMATION))
                {
                    return STATUS_INFO_LENGTH_MISMATCH;
                }

                const auto info = c.emu.read_memory<FILE_COMPLETION_INFORMATION>(file_information);

                if (!c.proc.io_completions.get(info.Port))
                {
                    return STATUS_INVALID_HANDLE;
                }

                return STATUS_SUCCESS;
            }

            if (info_class == FileStorageReserveIdInformation)
            {
                if (length < sizeof(FILE_STORAGE_RESERVE_ID_INFORMATION))
                {
                    return STATUS_INFO_LENGTH_MISMATCH;
                }

                return STATUS_SUCCESS;
            }

            if (info_class == FileTrackingInformation)
            {
                if (length < offsetof(FILE_TRACKING_INFORMATION, ObjectInformation))
                {
                    return STATUS_INFO_LENGTH_MISMATCH;
                }

                return STATUS_SUCCESS;
            }

            c.win_emu.log.error("Unsupported set file info class: 0x%X\n", info_class);
            c.emu.stop();

            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtQueryVolumeInformationFile(const syscall_context& c, const handle file_handle,
                                                     const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                     const uint64_t fs_information, const ULONG length,
                                                     const FS_INFORMATION_CLASS fs_information_class)
        {
            switch (fs_information_class)
            {
            case FileFsDeviceInformation:
                return handle_query<FILE_FS_DEVICE_INFORMATION>(c.emu, fs_information, length, io_status_block,
                                                                [&](FILE_FS_DEVICE_INFORMATION& info) {
                                                                    if (file_handle == STDOUT_HANDLE)
                                                                    {
                                                                        info.DeviceType = FILE_DEVICE_CONSOLE;
                                                                        info.Characteristics = 0x20000;
                                                                    }
                                                                    else
                                                                    {
                                                                        info.DeviceType = FILE_DEVICE_DISK;
                                                                        info.Characteristics = 0x20020;
                                                                    }
                                                                });

            case FileFsSizeInformation:
                return handle_query<FILE_FS_SIZE_INFORMATION>(c.emu, fs_information, length, io_status_block,
                                                              [&](FILE_FS_SIZE_INFORMATION& info) {
                                                                  info.BytesPerSector = 0x1000;
                                                                  info.SectorsPerAllocationUnit = 0x1000;
                                                                  info.TotalAllocationUnits.QuadPart = 0x10000;
                                                                  info.AvailableAllocationUnits.QuadPart = 0x1000;
                                                              });

            case FileFsFullSizeInformation:
                return handle_query<FILE_FS_FULL_SIZE_INFORMATION>(c.emu, fs_information, length, io_status_block,
                                                                   [&](FILE_FS_FULL_SIZE_INFORMATION& info) {
                                                                       info.BytesPerSector = 0x1000;
                                                                       info.SectorsPerAllocationUnit = 0x1000;
                                                                       info.TotalAllocationUnits.QuadPart = 0x10000;
                                                                       info.CallerAvailableAllocationUnits.QuadPart = 0x1000;
                                                                       info.ActualAvailableAllocationUnits.QuadPart = 0x1000;
                                                                   });

            case FileFsVolumeInformation:
                return handle_query<FILE_FS_VOLUME_INFORMATION>(c.emu, fs_information, length, io_status_block,
                                                                [&](FILE_FS_VOLUME_INFORMATION&) {});

            case FileFsAttributeInformation:
                return handle_query<_FILE_FS_ATTRIBUTE_INFORMATION>(
                    c.emu, fs_information, length, io_status_block, [&](_FILE_FS_ATTRIBUTE_INFORMATION& info) {
                        info.FileSystemAttributes = 0x40006; // FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_NAMED_STREAMS
                        info.MaximumComponentNameLength = 255;
                        constexpr auto name = u"NTFS"sv;
                        info.FileSystemNameLength = static_cast<ULONG>(name.size() * sizeof(char16_t));
                        memcpy(info.FileSystemName, name.data(), info.FileSystemNameLength);
                    });

            default:
                c.win_emu.log.error("Unsupported fs info class: 0x%X\n", fs_information_class);
                c.emu.stop();
                return write_io_status(io_status_block, STATUS_NOT_SUPPORTED, true);
            }
        }

        // Host filesystem timestamps are real wall-clock time, not virtualized like KUSER_SHARED_DATA's clock -
        // under the deterministic instruction-tick clock (windows_emulator::uses_relative_time), two runs that
        // touch the same file at different real moments would otherwise observe different values. Substitute a
        // fixed point in time instead of zeroing: a zero FILETIME is the 1601 epoch, and some directory
        // enumeration consumers reject entries with a timestamp older than 1970 (see PR #1210).
        LARGE_INTEGER get_file_time(const bool deterministic, const timespec ts)
        {
            if (deterministic)
            {
                return utils::convert_unix_to_windows_time(1700000000); // 2023-11-14, arbitrary but fixed
            }

            return convert_timespec_to_filetime(ts);
        }

        std::vector<file_entry> scan_directory(const file_system& file_sys, const windows_path& win_path,
                                               const std::u16string_view file_mask, const bool deterministic_time)
        {
            std::vector<file_entry> files{};

            const auto dir = file_sys.translate(win_path);
            const auto make_file_entry = [deterministic_time](const std::filesystem::path& file_path,
                                                              const std::filesystem::path& host_path, const bool is_directory) {
                file_entry entry{.file_path = file_path, .is_directory = is_directory};

                struct compat_stat file_stat{};
                if (compat_stat(host_path, &file_stat))
                {
                    entry.file_size = is_directory ? 0 : static_cast<uint64_t>(file_stat.st_size);
                    entry.creation_time = get_file_time(deterministic_time, file_stat.st_ctimespec);
                    entry.last_access_time = get_file_time(deterministic_time, file_stat.st_atimespec);
                    entry.last_write_time = get_file_time(deterministic_time, file_stat.st_mtimespec);
                }

                return entry;
            };

            if (file_mask.empty() || file_mask == u"*")
            {
                files.emplace_back(make_file_entry(".", dir, true));
                files.emplace_back(make_file_entry("..", dir.parent_path(), true));
            }

            std::error_code ec{};
            for (const auto& file : std::filesystem::directory_iterator(dir, ec))
            {
                if (!file_mask.empty() && !utils::wildcard::match_filename(file.path().filename().u16string(), file_mask))
                {
                    continue;
                }

                files.emplace_back(make_file_entry(file.path().filename(), file.path(), file.is_directory()));
            }

            file_sys.access_mapped_entries(win_path, [&](const std::pair<windows_path, std::filesystem::path>& entry) {
                const auto filename = entry.first.leaf();

                if (!file_mask.empty() && !utils::wildcard::match_filename(filename, file_mask))
                {
                    return;
                }

                const std::filesystem::directory_entry dir_entry(entry.second, ec);
                if (ec || !dir_entry.exists())
                {
                    return;
                }

                files.emplace_back(make_file_entry(filename, entry.second, dir_entry.is_directory()));
            });

            return files;
        }

        template <typename T>
        NTSTATUS handle_file_enumeration(const syscall_context& c,
                                         const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                         const uint64_t file_information, const uint32_t length, const ULONG query_flags,
                                         const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> file_mask, file* f)
        {
            if (!f->enumeration_state || query_flags & SL_RESTART_SCAN)
            {
                const auto mask = file_mask ? read_unicode_string(c.emu, file_mask) : u"";
                c.win_emu.callbacks.on_generic_access("Enumerating directory", f->name + mask);

                f->enumeration_state.emplace(file_enumeration_state{});
                f->enumeration_state->files = scan_directory(c.win_emu.file_sys, f->name, mask, c.win_emu.uses_relative_time());
            }

            auto& enum_state = *f->enumeration_state;

            uint64_t current_offset{0};
            emulator_object<T> object{c.emu.memory()};

            size_t current_index = enum_state.current_index;

            if (current_index >= enum_state.files.size())
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Information = 0;
                io_status_block.write(block);

                return STATUS_NO_MORE_FILES;
            }

            do
            {
                const auto new_offset = align_up(current_offset, 8);
                const auto& current_file = enum_state.files[current_index];
                const auto file_name = current_file.file_path.u16string();
                const auto required_size = sizeof(T) + (file_name.size() * 2) - 2;
                const auto end_offset = new_offset + required_size;

                if (end_offset > length)
                {
                    if (current_offset == 0)
                    {
                        IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                        block.Information = end_offset;
                        io_status_block.write(block);

                        return STATUS_BUFFER_OVERFLOW;
                    }

                    break;
                }

                if (object)
                {
                    const auto object_offset = object.value() - file_information;

                    object.access([&](T& dir_info) {
                        dir_info.NextEntryOffset = static_cast<ULONG>(new_offset - object_offset); //
                    });
                }

                T info{};
                info.NextEntryOffset = 0;
                info.FileIndex = static_cast<ULONG>(current_index);

                info.CreationTime = current_file.creation_time;
                info.LastAccessTime = current_file.last_access_time;
                info.LastWriteTime = current_file.last_write_time;
                info.ChangeTime = info.LastWriteTime;

                info.FileAttributes = current_file.is_directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                info.FileNameLength = static_cast<ULONG>(file_name.size() * 2);
                info.EndOfFile.QuadPart = current_file.file_size;

                object.set_address(file_information + new_offset);
                object.write(info);

                c.emu.write_memory(object.value() + offsetof(T, FileName), file_name.data(), info.FileNameLength);

                ++current_index;
                current_offset = end_offset;
            } while ((query_flags & SL_RETURN_SINGLE_ENTRY) == 0 && current_index < enum_state.files.size());

            if ((query_flags & SL_NO_CURSOR_UPDATE) == 0)
            {
                enum_state.current_index = current_index;
            }

            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
            block.Information = current_offset;
            io_status_block.write(block);

            return current_index <= enum_state.files.size() ? STATUS_SUCCESS : STATUS_NO_MORE_FILES;
        }

        NTSTATUS handle_NtQueryDirectoryFileEx(const syscall_context& c, const handle file_handle, const handle /*event_handle*/,
                                               const EMULATOR_CAST(emulator_pointer, PIO_APC_ROUTINE) /*apc_routine*/,
                                               const emulator_pointer /*apc_context*/,
                                               const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                               const uint64_t file_information, const uint32_t length, const uint32_t info_class,
                                               const ULONG query_flags,
                                               const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> file_name)
        {
            auto* f = c.proc.files.get(file_handle);
            if (!f || !f->is_directory())
            {
                return STATUS_INVALID_HANDLE;
            }

            if (info_class == FileDirectoryInformation)
            {
                return handle_file_enumeration<FILE_DIRECTORY_INFORMATION>(c, io_status_block, file_information, length, query_flags,
                                                                           file_name, f);
            }

            if (info_class == FileFullDirectoryInformation)
            {
                return handle_file_enumeration<FILE_FULL_DIR_INFORMATION>(c, io_status_block, file_information, length, query_flags,
                                                                          file_name, f);
            }

            if (info_class == FileBothDirectoryInformation)
            {
                return handle_file_enumeration<FILE_BOTH_DIR_INFORMATION>(c, io_status_block, file_information, length, query_flags,
                                                                          file_name, f);
            }

            if (info_class == FileIdBothDirectoryInformation)
            {
                return handle_file_enumeration<FILE_ID_BOTH_DIR_INFORMATION>(c, io_status_block, file_information, length, query_flags,
                                                                             file_name, f);
            }

            c.win_emu.log.error("Unsupported query directory file info class: %X\n", info_class);
            c.emu.stop();

            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtQueryDirectoryFile(const syscall_context& c, const handle file_handle, const handle event_handle,
                                             const EMULATOR_CAST(emulator_pointer, PIO_APC_ROUTINE) apc_routine,
                                             const emulator_pointer apc_context,
                                             const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                             const uint64_t file_information, const uint32_t length, const uint32_t info_class,
                                             const BOOLEAN return_single_entry,
                                             const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> file_name,
                                             const BOOLEAN restart_scan)
        {
            ULONG query_flags = 0;
            if (return_single_entry)
            {
                query_flags |= SL_RETURN_SINGLE_ENTRY;
            }
            if (restart_scan)
            {
                query_flags |= SL_RESTART_SCAN;
            }
            return handle_NtQueryDirectoryFileEx(c, file_handle, event_handle, apc_routine, apc_context, io_status_block, file_information,
                                                 length, info_class, query_flags, file_name);
        }

        NTSTATUS handle_NtQueryInformationFile(const syscall_context& c, const handle file_handle,
                                               const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                               const uint64_t file_information, uint32_t length, const uint32_t info_class)
        {
            if (info_class == 0 || info_class >= FileMaximumInformation)
            {
                return STATUS_INVALID_INFO_CLASS;
            }

            auto block = io_status_block.try_read();
            if (!block.has_value())
            {
                return STATUS_ACCESS_VIOLATION;
            }

            const auto _ = utils::finally([&] {
                if (io_status_block)
                {
                    (void)io_status_block.try_write(block.value());
                }
            });

            const auto ret = [&](const NTSTATUS status, size_t size = 0) {
                block->Status = status;
                block->Information = size;
                return status;
            };

            const auto* f = c.proc.files.get(file_handle);
            if (!f)
            {
                auto* device = c.proc.devices.get(file_handle);
                auto* pipe = device ? device->get_internal_device<named_pipe>() : nullptr;

                if (pipe && (info_class == FileNameInformation || info_class == FileNormalizedNameInformation))
                {
                    std::u16string relative_name = pipe->name;
                    for (const std::u16string_view prefix :
                         {std::u16string_view(u"\\Device\\NamedPipe"), std::u16string_view(u"\\??\\Pipe")})
                    {
                        if (utils::string::starts_with_ignore_case(std::u16string_view(relative_name), prefix))
                        {
                            relative_name = relative_name.substr(prefix.size());
                            break;
                        }
                    }

                    const auto required_length =
                        static_cast<uint32_t>((relative_name.size() * 2) + offsetof(FILE_NAME_INFORMATION, FileName));

                    if (length < sizeof(FILE_NAME_INFORMATION))
                    {
                        return ret(STATUS_INFO_LENGTH_MISMATCH);
                    }

                    const uint32_t copy_length = std::min(length, required_length) - offsetof(FILE_NAME_INFORMATION, FileName);

                    c.emu.write_memory(file_information, FILE_NAME_INFORMATION{
                                                             .FileNameLength = static_cast<ULONG>(relative_name.size() * 2),
                                                             .FileName = {},
                                                         });
                    c.emu.write_memory(file_information + offsetof(FILE_NAME_INFORMATION, FileName), relative_name.c_str(), copy_length);

                    const uint32_t total_copied = copy_length + offsetof(FILE_NAME_INFORMATION, FileName);
                    return ret(total_copied == required_length ? STATUS_SUCCESS : STATUS_BUFFER_OVERFLOW, total_copied);
                }

                return ret(STATUS_INVALID_HANDLE);
            }

            if (info_class == FileAttributeTagInformation)
            {
                if (!f->handle && !f->is_directory())
                {
                    return ret(STATUS_NOT_SUPPORTED);
                }

                constexpr auto required_length = sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                const emulator_object<FILE_ATTRIBUTE_TAG_INFORMATION> info{c.emu, file_information};
                FILE_ATTRIBUTE_TAG_INFORMATION i{};

                i.FileAttributes = f->is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

                info.write(i);

                return ret(STATUS_SUCCESS, required_length);
            }

            if (info_class == FileIsRemoteDeviceInformation)
            {
                if (!f->handle)
                {
                    return ret(STATUS_NOT_SUPPORTED);
                }

                constexpr auto required_length = sizeof(FILE_IS_REMOTE_DEVICE_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                const emulator_object<FILE_IS_REMOTE_DEVICE_INFORMATION> info{c.emu, file_information};
                FILE_IS_REMOTE_DEVICE_INFORMATION i{};

                i.IsRemote = FALSE;

                info.write(i);

                return ret(STATUS_SUCCESS, required_length);
            }

            if (info_class == FileRemoteProtocolInformation)
            {
                return ret(STATUS_INVALID_PARAMETER);
            }

            if (info_class == FileStorageReserveIdInformation)
            {
                constexpr auto required_length = sizeof(FILE_STORAGE_RESERVE_ID_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                const emulator_object<FILE_STORAGE_RESERVE_ID_INFORMATION> info{c.emu, file_information};
                FILE_STORAGE_RESERVE_ID_INFORMATION i{};

                i.StorageReserveId = StorageReserveIdNone;

                info.write(i);

                return ret(STATUS_SUCCESS, required_length);
            }

            if (info_class == FileIdInformation)
            {
                if (!f->handle)
                {
                    return ret(STATUS_NOT_SUPPORTED);
                }

                constexpr auto required_length = sizeof(FILE_ID_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                struct compat_stat file_stat{};
                if (!compat_fstat(f->handle.file_descriptor(), &file_stat))
                {
                    return ret(STATUS_INVALID_HANDLE);
                }

                const emulator_object<FILE_ID_INFORMATION> info{c.emu, file_information};
                FILE_ID_INFORMATION i{};

                i.VolumeSerialNumber = f->drive_number;
                memset(&i.FileId, 0, sizeof(i.FileId));
                memcpy(&i.FileId.Identifier[0], &file_stat.st_ino, sizeof(file_stat.st_ino));

                info.write(i);

                return ret(STATUS_SUCCESS, required_length);
            }

            if (info_class == FileStreamInformation)
            {
                const std::u16string stream_name = u"::$DATA";
                const auto stream_name_len = static_cast<uint32_t>(stream_name.size() * sizeof(char16_t));

                const uint32_t required_length = offsetof(FILE_STREAM_INFORMATION, StreamName) + stream_name_len;

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                struct compat_stat file_stat{};
                if (f->handle && !compat_fstat(f->handle.file_descriptor(), &file_stat))
                {
                    return ret(STATUS_INVALID_HANDLE);
                }

                FILE_STREAM_INFORMATION info{};
                info.NextEntryOffset = 0;
                info.StreamNameLength = stream_name_len;

                if (f->is_directory())
                {
                    info.StreamSize.QuadPart = 0;
                    info.StreamAllocationSize.QuadPart = 0;
                }
                else
                {
                    info.StreamSize.QuadPart = file_stat.st_size;
                    info.StreamAllocationSize.QuadPart = file_stat.st_size;
                }

                c.emu.write_memory(file_information, info);
                c.emu.write_memory(file_information + offsetof(FILE_STREAM_INFORMATION, StreamName), stream_name.c_str(), stream_name_len);

                return ret(STATUS_SUCCESS, required_length);
            }

            if (info_class == FileVolumeNameInformation)
            {
                const auto volume_name = u8_to_u16("\\Device\\HarddiskVolume" + std::to_string(f->drive_number));
                const auto name_bytes = static_cast<uint32_t>(volume_name.size() * sizeof(char16_t));

                const uint32_t required_length = offsetof(FILE_VOLUME_NAME_INFORMATION, DeviceName) + name_bytes;

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                FILE_VOLUME_NAME_INFORMATION vni{};
                vni.DeviceNameLength = name_bytes;

                c.emu.write_memory(file_information, vni);
                c.emu.write_memory(file_information + offsetof(FILE_VOLUME_NAME_INFORMATION, DeviceName), volume_name.c_str(), name_bytes);

                return ret(STATUS_SUCCESS, required_length);
            }

            const auto all_info = info_class == FileAllInformation;
            size_t all_length = 0;

            if (all_info && length < sizeof(FILE_ALL_INFORMATION))
            {
                return ret(STATUS_INFO_LENGTH_MISMATCH);
            }

            if (info_class == FileBasicInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_BASIC_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                struct compat_stat file_stat{};
                if (f->handle && !compat_fstat(f->handle.file_descriptor(), &file_stat))
                {
                    return STATUS_INVALID_HANDLE;
                }

                const bool is_directory = f->handle ? (file_stat.st_mode & S_IFDIR) != 0 : f->is_directory();

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, BasicInformation);
                }

                const emulator_object<FILE_BASIC_INFORMATION> info{c.emu, address};
                FILE_BASIC_INFORMATION i{};
                i.CreationTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_ctimespec);
                i.LastAccessTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_atimespec);
                i.LastWriteTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_mtimespec);
                i.ChangeTime = i.LastWriteTime;
                i.FileAttributes = is_directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileStandardInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_STANDARD_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, StandardInformation);
                }

                const emulator_object<FILE_STANDARD_INFORMATION> info{c.emu, address};
                FILE_STANDARD_INFORMATION i{};
                i.Directory = f->is_directory() ? TRUE : FALSE;
                i.NumberOfLinks = 1;

                if (f->handle)
                {
                    i.EndOfFile.QuadPart = f->handle.size();
                    i.AllocationSize.QuadPart = static_cast<LONGLONG>(align_up(i.EndOfFile.QuadPart, 512));
                }

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileInternalInformation || all_info)
            {
                const uint32_t required_length = sizeof(FILE_INTERNAL_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                struct compat_stat file_stat{};
                if (f->handle && !compat_fstat(f->handle.file_descriptor(), &file_stat))
                {
                    return STATUS_INVALID_HANDLE;
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, InternalInformation);
                }

                const emulator_object<FILE_INTERNAL_INFORMATION> info{c.emu, address};
                FILE_INTERNAL_INFORMATION i{};

                i.IndexNumber.QuadPart = static_cast<LONGLONG>(file_stat.st_ino);

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileEaInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_EA_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, EaInformation);
                }
                const emulator_object<FILE_EA_INFORMATION> info{c.emu, address};
                FILE_EA_INFORMATION i{};

                i.EaSize = 0;

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileAccessInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_ACCESS_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, AccessInformation);
                }

                const emulator_object<FILE_ACCESS_INFORMATION> info{c.emu, address};
                FILE_ACCESS_INFORMATION i{};

                i.AccessFlags = f->access_mask;

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FilePositionInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_POSITION_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, PositionInformation);
                }

                const emulator_object<FILE_POSITION_INFORMATION> info{c.emu, address};
                FILE_POSITION_INFORMATION i{};

                i.CurrentByteOffset.QuadPart = f->handle ? f->handle.tell() : 0;

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileModeInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_MODE_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, ModeInformation);
                }

                const emulator_object<FILE_MODE_INFORMATION> info{c.emu, address};
                FILE_MODE_INFORMATION i{};

                i.Mode = f->file_mode;

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileAlignmentInformation || all_info)
            {
                constexpr auto required_length = sizeof(FILE_ALIGNMENT_INFORMATION);

                if (length < required_length)
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, AlignmentInformation);
                }

                const emulator_object<FILE_ALIGNMENT_INFORMATION> info{c.emu, address};
                FILE_ALIGNMENT_INFORMATION i{};

                i.AlignmentRequirement = FILE_BYTE_ALIGNMENT;

                info.write(i);

                if (!all_info)
                {
                    return ret(STATUS_SUCCESS, required_length);
                }

                length -= required_length;
                all_length += required_length;
            }

            if (info_class == FileNameInformation || info_class == FileNormalizedNameInformation || all_info)
            {
                const auto relative_path = u"\\" + windows_path(f->name).without_drive().u16string();
                const auto required_length = static_cast<uint32_t>((relative_path.size() * 2) + offsetof(FILE_NAME_INFORMATION, FileName));

                if (length < sizeof(FILE_NAME_INFORMATION))
                {
                    return ret(STATUS_INFO_LENGTH_MISMATCH);
                }

                const uint32_t copy_length = std::min(length, required_length) - offsetof(FILE_NAME_INFORMATION, FileName);

                auto address = file_information;
                if (all_info)
                {
                    address += offsetof(FILE_ALL_INFORMATION, NameInformation);
                }

                c.emu.write_memory(address, FILE_NAME_INFORMATION{
                                                .FileNameLength = static_cast<ULONG>(relative_path.size() * 2),
                                                .FileName = {},
                                            });

                c.emu.write_memory(address + offsetof(FILE_NAME_INFORMATION, FileName), relative_path.c_str(), copy_length);

                const uint32_t total_copied = copy_length + offsetof(FILE_NAME_INFORMATION, FileName);

                if (!all_info)
                {
                    return ret(total_copied == required_length ? STATUS_SUCCESS : STATUS_BUFFER_OVERFLOW, total_copied);
                }

                length -= total_copied;
                all_length += total_copied;
            }

            if (all_info)
            {
                return ret(STATUS_SUCCESS, all_length);
            }

            c.win_emu.log.error("Unsupported query file info class: 0x%X\n", info_class);
            c.emu.stop();

            return ret(STATUS_NOT_SUPPORTED);
        }

        NTSTATUS handle_NtQueryInformationByName(const syscall_context& c,
                                                 const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                 const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                                 const uint64_t file_information, const uint32_t length, const uint32_t info_class)
        {
            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
            block.Status = STATUS_SUCCESS;
            block.Information = 0;

            const auto _ = utils::finally([&] {
                if (io_status_block)
                {
                    (void)io_status_block.try_write(block);
                }
            });

            const auto attributes = object_attributes.read();
            auto filename = read_unicode_string(c.emu, attributes.ObjectName);

            c.win_emu.callbacks.on_generic_access("Query file info", filename);

            const auto ret = [&](const NTSTATUS status) {
                block.Status = status;
                return status;
            };

            if (info_class == FileStatBasicInformation)
            {
                block.Information = sizeof(EMU_FILE_STAT_BASIC_INFORMATION);

                if (length < block.Information)
                {
                    return ret(STATUS_BUFFER_OVERFLOW);
                }

                auto [native_file_handle, status] = open_file(c.win_emu.file_sys, filename, u"r");
                if (status != STATUS_SUCCESS)
                {
                    return ret(status);
                }

                struct compat_stat file_stat{};
                if (!compat_fstat(native_file_handle.file_descriptor(), &file_stat))
                {
                    return STATUS_INVALID_HANDLE;
                }

                const auto is_directory = (file_stat.st_mode & S_IFDIR) != 0;

                EMU_FILE_STAT_BASIC_INFORMATION i{};

                i.CreationTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_ctimespec);
                i.LastAccessTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_atimespec);
                i.LastWriteTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_mtimespec);
                i.ChangeTime = i.LastWriteTime;
                i.FileAttributes = is_directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

                const auto file_size = is_directory ? 0LL : file_stat.st_size;
                i.EndOfFile.QuadPart = file_size;
                i.AllocationSize.QuadPart = static_cast<LONGLONG>(align_up(file_size, 512));

                c.emu.write_memory(file_information, i);

                return ret(STATUS_SUCCESS);
            }

            c.win_emu.log.error("Unsupported query name info class: %X\n", info_class);
            c.emu.stop();

            return ret(STATUS_NOT_SUPPORTED);
        }

        void commit_file_data(const std::string_view data, memory_interface& emu,
                              const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, const uint64_t buffer)
        {
            if (io_status_block)
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Information = data.size();
                io_status_block.write(block);
            }

            emu.write_memory(buffer, data.data(), data.size());
        }

        void write_lock_io_status(const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, const NTSTATUS status,
                                  const uint64_t information = 0)
        {
            if (!io_status_block)
            {
                return;
            }

            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
            block.Status = status;
            block.Information = information;
            io_status_block.write(block);
        }

        std::optional<uint64_t> get_lock_range_end(const LARGE_INTEGER& byte_offset, const LARGE_INTEGER& length)
        {
            if (byte_offset.QuadPart < 0 || length.QuadPart == 0)
            {
                return std::nullopt;
            }

            const auto offset = static_cast<uint64_t>(byte_offset.QuadPart);
            const auto range_length = static_cast<uint64_t>(length.QuadPart);
            if (offset > std::numeric_limits<uint64_t>::max() - range_length)
            {
                return std::nullopt;
            }

            return offset + range_length;
        }

        bool does_lock_range_overlap(const file_lock_range& existing, const uint64_t offset, const uint64_t end)
        {
            const auto existing_end = existing.offset + existing.length;
            return existing.offset < end && offset < existing_end;
        }

        // Completion of an asynchronous (overlapped) file operation: signal the optional completion event and,
        // for an APC-based request (ReadFileEx/WriteFileEx), queue the I/O completion APC on the issuing thread.
        // The APC routine follows the PIO_APC_ROUTINE contract (ApcContext, IoStatusBlock, Reserved); it is
        // delivered the next time the thread enters an alertable wait.
        void deliver_file_io_completion(const syscall_context& c, const uint64_t event, const uint64_t apc_routine,
                                        const uint64_t apc_context,
                                        const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                        const NTSTATUS status, const uint64_t information)
        {
            if (event)
            {
                if (auto* e = c.proc.events.get(event))
                {
                    e->signaled = true;
                }
            }

            if (apc_routine)
            {
                // A WoW64 completion routine (kernel32!BasepIoCompletion) reads the I/O status block with the
                // 32-bit layout (NTSTATUS Status @0, ULONG Information @4). Our 64-bit block keeps Information at
                // @8, so the 32-bit reader would see 0 bytes transferred. Mirror Status/Information into the
                // 32-bit slots so the completion routine reports the correct byte count.
                if (c.proc.is_wow64_process && io_status_block)
                {
                    const auto status32 = static_cast<uint32_t>(static_cast<ULONG>(status));
                    const auto information32 = static_cast<uint32_t>(information);
                    c.emu.write_memory(io_status_block.value(), &status32, sizeof(status32));
                    c.emu.write_memory(io_status_block.value() + sizeof(status32), &information32, sizeof(information32));
                }

                c.thread().pending_apcs.push_back({
                    .flags = 0,
                    .apc_routine = apc_routine,
                    .apc_argument1 = apc_context,
                    .apc_argument2 = io_status_block.value(),
                    .apc_argument3 = 0,
                    .restamp_io_status_block = c.proc.is_wow64_process && io_status_block,
                    .io_status = static_cast<int32_t>(static_cast<ULONG>(status)),
                    .io_information = static_cast<uint32_t>(information),
                });
            }
        }

        NTSTATUS handle_NtReadFile(const syscall_context& c, const handle file_handle, const uint64_t event, const uint64_t apc_routine,
                                   const uint64_t apc_context,
                                   const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, const uint64_t buffer,
                                   const ULONG length, const emulator_object<LARGE_INTEGER> byte_offset,
                                   const emulator_object<ULONG> /*key*/)
        {
            std::string temp_buffer{};
            temp_buffer.resize(length);

            if (file_handle == STDIN_HANDLE)
            {
                char chr{};
                if (std::cin.readsome(&chr, 1) <= 0)
                {
                    std::cin.read(&chr, 1);
                }

                std::cin.putback(chr);

                const auto read_count = std::cin.readsome(temp_buffer.data(), static_cast<std::streamsize>(temp_buffer.size()));
                const auto count = std::max(read_count, static_cast<std::streamsize>(0));

                commit_file_data(std::string_view(temp_buffer.data(), static_cast<size_t>(count)), c.emu, io_status_block, buffer);
                return STATUS_SUCCESS;
            }

            if (file_handle == NUL_HANDLE)
            {
                if (io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = 0;
                    io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }

            const auto* container = c.proc.devices.get(file_handle);
            if (container)
            {
                if (auto* pipe = container->get_internal_device<named_pipe>())
                {
                    if (pipe_io_trace_enabled())
                    {
                        c.win_emu.log.info("[pipe-io-trace] NtReadFile pipe='%s' length=%u queued=%zu tid=%u\n",
                                           u16_to_u8(pipe->name).c_str(), length, pipe->write_queue.size(), c.thread().id);
                    }

                    if (pipe->write_queue.size() > 0 && pipe->name.find(u"mojo.") != std::u16string::npos &&
                        std::getenv("SOGEN_TRACE_PIPE_IO_CALLER_STACK") != nullptr)
                    {
                        const auto rip = c.emu.read_instruction_pointer();
                        const auto rsp = c.emu.read_stack_pointer();
                        const auto* rip_mod = c.win_emu.mod_manager.find_by_address(rip);
                        fprintf(stderr,
                                "[PIPE_IO_CALLER_STACK] pid=%d guest_pid=%u tid=%u site=NtReadFile pipe='%s' rip=0x%llx (%s+0x%llx) "
                                "rsp=0x%llx\n",
                                ::getpid(), c.proc.process_id, c.thread().id, u16_to_u8(pipe->name).c_str(),
                                static_cast<unsigned long long>(rip), rip_mod ? rip_mod->name.c_str() : "?",
                                rip_mod ? static_cast<unsigned long long>(rip - rip_mod->image_base) : 0ULL,
                                static_cast<unsigned long long>(rsp));
                        for (uint64_t i = 0; i < 384; ++i)
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

                    io_device_context ctx{c.emu};
                    ctx.event = handle{.bits = event};
                    ctx.apc_routine = apc_routine;
                    ctx.apc_context = apc_context;
                    ctx.io_status_block = io_status_block;
                    ctx.output_buffer = buffer;
                    ctx.output_buffer_length = length;
                    ctx.vcpu = &c.vcpu;

                    return pipe->try_deliver_read(c.win_emu, ctx);
                }
            }

            const auto* f = c.proc.files.get(file_handle);
            if (!f)
            {
                if (std::getenv("SOGEN_TRACE_PIPE_IO"))
                {
                    c.win_emu.log.info("[pipe-io-trace] NtReadFile INVALID_HANDLE handle=0x%llx type=%u tid=%u\n",
                                       static_cast<unsigned long long>(file_handle.bits), static_cast<unsigned>(file_handle.value.type),
                                       c.thread().id);
                }

                return STATUS_INVALID_HANDLE;
            }

            if (byte_offset)
            {
                const auto offset = byte_offset.read();
                const bool use_current_file_pointer = offset.LowPart == FILE_USE_FILE_POINTER_POSITION && offset.HighPart == -1;

                if (!use_current_file_pointer)
                {
                    if (offset.QuadPart < 0)
                    {
                        return STATUS_INVALID_PARAMETER;
                    }

                    if (!f->handle.seek_to(offset.QuadPart))
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                }
            }

            const auto bytes_read = fread(temp_buffer.data(), 1, temp_buffer.size(), f->handle);

            if (bytes_read > 0)
            {
                commit_file_data(std::string_view(temp_buffer.data(), bytes_read), c.emu, io_status_block, buffer);
                deliver_file_io_completion(c, event, apc_routine, apc_context, io_status_block, STATUS_SUCCESS, bytes_read);
                return STATUS_SUCCESS;
            }

            const auto status = length > 0 ? STATUS_END_OF_FILE : STATUS_SUCCESS;

            if (io_status_block)
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Status = status;
                block.Information = 0;
                io_status_block.write(block);
            }

            deliver_file_io_completion(c, event, apc_routine, apc_context, io_status_block, status, 0);
            return status;
        }

        NTSTATUS handle_NtWriteFile(const syscall_context& c, const handle file_handle, const uint64_t event, const uint64_t apc_routine,
                                    const uint64_t apc_context,
                                    const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, const uint64_t buffer,
                                    const ULONG length, const emulator_object<LARGE_INTEGER> byte_offset,
                                    const emulator_object<ULONG> /*key*/)
        {
            std::string temp_buffer{};
            temp_buffer.resize(length);
            c.emu.read_memory(buffer, temp_buffer.data(), temp_buffer.size());

            if (file_handle == STDOUT_HANDLE)
            {
                if (io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = length;
                    io_status_block.write(block);
                }

                c.win_emu.callbacks.on_stdout(temp_buffer);

                return STATUS_SUCCESS;
            }

            if (file_handle == NUL_HANDLE)
            {
                if (io_status_block)
                {
                    IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                    block.Information = length;
                    io_status_block.write(block);
                }

                return STATUS_SUCCESS;
            }

            const auto* container = c.proc.devices.get(file_handle);
            if (container)
            {
                if (auto* pipe = container->get_internal_device<named_pipe>())
                {
                    if (pipe_io_trace_enabled())
                    {
                        c.win_emu.log.info("[pipe-io-trace] NtWriteFile pipe='%s' length=%zu tid=%u\n", u16_to_u8(pipe->name).c_str(),
                                           temp_buffer.size(), c.thread().id);
                    }

                    if (pipe_io_bytes_trace_enabled())
                    {
                        c.win_emu.log.info("[pipe-io-bytes-trace] NtWriteFile pipe='%s' length=%zu tid=%u bytes=%s\n",
                                           u16_to_u8(pipe->name).c_str(), temp_buffer.size(), c.thread().id,
                                           utils::string::to_hex_string(temp_buffer).c_str());
                    }

                    deliver_bytes_to_named_pipe(c.proc, pipe->name, temp_buffer, pipe);
                    c.win_emu.broadcast_named_pipe_write(pipe->name, temp_buffer);

                    io_device_context ctx{c.emu};
                    ctx.event = handle{.bits = event};
                    ctx.apc_routine = apc_routine;
                    ctx.apc_context = apc_context;
                    ctx.io_status_block = io_status_block;
                    ctx.vcpu = &c.vcpu;

                    return pipe->complete_write(c.win_emu, ctx, temp_buffer.size());
                }
            }

            const auto* f = c.proc.files.get(file_handle);
            if (!f)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (byte_offset)
            {
                const auto offset = byte_offset.read();
                const bool use_end_of_file = offset.LowPart == FILE_WRITE_TO_END_OF_FILE && offset.HighPart == -1;
                const bool use_current_file_pointer = offset.LowPart == FILE_USE_FILE_POINTER_POSITION && offset.HighPart == -1;

                if (use_end_of_file)
                {
                    if (!f->handle.seek_to(0, SEEK_END))
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                }
                else if (!use_current_file_pointer)
                {
                    if (offset.QuadPart < 0)
                    {
                        return STATUS_INVALID_PARAMETER;
                    }

                    if (!f->handle.seek_to(offset.QuadPart))
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                }
            }

            const auto bytes_written = fwrite(temp_buffer.data(), 1, temp_buffer.size(), f->handle);

            if (io_status_block)
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Information = bytes_written;
                io_status_block.write(block);
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCopyFileChunk(const syscall_context& c, const handle source_handle, const handle destination_handle,
                                        const handle /*event_handle*/,
                                        const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, const ULONG length,
                                        const emulator_object<LARGE_INTEGER> source_offset,
                                        const emulator_object<LARGE_INTEGER> destination_offset,
                                        const emulator_object<ULONG> /*source_key*/, const emulator_object<ULONG> /*destination_key*/,
                                        const ULONG /*flags*/)
        {
            const auto* source = c.proc.files.get(source_handle);
            const auto* destination = c.proc.files.get(destination_handle);
            if (!source || !destination || !source->handle || !destination->handle)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (source_offset)
            {
                const auto offset = source_offset.read();
                if (offset.QuadPart < 0 || !source->handle.seek_to(offset.QuadPart))
                {
                    return STATUS_INVALID_PARAMETER;
                }
            }

            std::string temp_buffer{};
            temp_buffer.resize(length);
            const auto bytes_read = fread(temp_buffer.data(), 1, temp_buffer.size(), source->handle);

            if (destination_offset)
            {
                const auto offset = destination_offset.read();
                if (offset.QuadPart < 0 || !destination->handle.seek_to(offset.QuadPart))
                {
                    return STATUS_INVALID_PARAMETER;
                }
            }

            const auto bytes_written = fwrite(temp_buffer.data(), 1, bytes_read, destination->handle);
            (void)fflush(destination->handle);

            if (io_status_block)
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Information = bytes_written;
                io_status_block.write(block);
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtLockFile(const syscall_context& c, const handle file_handle, const handle /*event_handle*/,
                                   const uint64_t /*apc_routine*/, const uint64_t /*apc_context*/,
                                   const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                   const emulator_object<LARGE_INTEGER> byte_offset, const emulator_object<LARGE_INTEGER> length,
                                   const ULONG key, const BOOLEAN fail_immediately, const BOOLEAN exclusive_lock)
        {
            if (!io_status_block || !byte_offset || !length)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto* f = c.proc.files.get(file_handle);
            if (!f || !f->is_file())
            {
                write_lock_io_status(io_status_block, STATUS_INVALID_HANDLE);
                return STATUS_INVALID_HANDLE;
            }

            const auto lock_offset = byte_offset.read();
            const auto lock_length = length.read();
            const auto lock_end = get_lock_range_end(lock_offset, lock_length);
            if (!lock_end)
            {
                write_lock_io_status(io_status_block, STATUS_INVALID_LOCK_RANGE);
                return STATUS_INVALID_LOCK_RANGE;
            }

            const auto offset = static_cast<uint64_t>(lock_offset.QuadPart);
            const auto range_length = static_cast<uint64_t>(lock_length.QuadPart);
            const auto is_exclusive = exclusive_lock != FALSE;
            const auto lock_key = f->host_path.u16string();

            if (const auto lock_it = c.proc.file_locks.find(lock_key); lock_it != c.proc.file_locks.end())
            {
                for (const auto& existing : lock_it->second.locks)
                {
                    if (existing.owner == file_handle || !does_lock_range_overlap(existing, offset, *lock_end))
                    {
                        continue;
                    }

                    if (!existing.exclusive && !is_exclusive)
                    {
                        continue;
                    }

                    // Blocking lock completion is not modeled yet; surface the conflict immediately.
                    (void)fail_immediately;
                    write_lock_io_status(io_status_block, STATUS_LOCK_NOT_GRANTED);
                    return STATUS_LOCK_NOT_GRANTED;
                }
            }

            c.proc.file_locks[lock_key].locks.push_back(file_lock_range{
                .offset = offset,
                .length = range_length,
                .key = key,
                .exclusive = is_exclusive,
                .owner = file_handle,
            });

            write_lock_io_status(io_status_block, STATUS_SUCCESS);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtUnlockFile(const syscall_context& c, const handle file_handle,
                                     const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                     const emulator_object<LARGE_INTEGER> byte_offset, const emulator_object<LARGE_INTEGER> length,
                                     const ULONG key)
        {
            if (!io_status_block || !byte_offset || !length)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto* f = c.proc.files.get(file_handle);
            if (!f || !f->is_file())
            {
                write_lock_io_status(io_status_block, STATUS_INVALID_HANDLE);
                return STATUS_INVALID_HANDLE;
            }

            const auto lock_offset = byte_offset.read();
            const auto lock_length = length.read();
            if (!get_lock_range_end(lock_offset, lock_length))
            {
                write_lock_io_status(io_status_block, STATUS_INVALID_LOCK_RANGE);
                return STATUS_INVALID_LOCK_RANGE;
            }

            const auto offset = static_cast<uint64_t>(lock_offset.QuadPart);
            const auto range_length = static_cast<uint64_t>(lock_length.QuadPart);
            const auto lock_key = f->host_path.u16string();
            const auto lock_it = c.proc.file_locks.find(lock_key);
            if (lock_it == c.proc.file_locks.end())
            {
                write_lock_io_status(io_status_block, STATUS_RANGE_NOT_LOCKED);
                return STATUS_RANGE_NOT_LOCKED;
            }

            auto& locks = lock_it->second.locks;
            const auto entry = std::ranges::find_if(locks, [&](const file_lock_range& existing) {
                return existing.owner == file_handle && existing.offset == offset && existing.length == range_length && existing.key == key;
            });

            if (entry == locks.end())
            {
                write_lock_io_status(io_status_block, STATUS_RANGE_NOT_LOCKED);
                return STATUS_RANGE_NOT_LOCKED;
            }

            locks.erase(entry);
            if (locks.empty())
            {
                c.proc.file_locks.erase(lock_it);
            }

            write_lock_io_status(io_status_block, STATUS_SUCCESS);
            return STATUS_SUCCESS;
        }

        constexpr std::u16string map_mode(const ACCESS_MASK desired_access, const ULONG create_disposition)
        {
            const auto wants_write = (desired_access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)) != 0;
            const auto wants_read =
                (desired_access & (GENERIC_READ | FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE)) != 0;

            if (desired_access & FILE_APPEND_DATA)
            {
                return u"a+b";
            }

            switch (create_disposition)
            {
            case FILE_CREATE:
            case FILE_SUPERSEDE:
                if (wants_write)
                {
                    return wants_read ? u"w+b" : u"wb";
                }
                break;

            case FILE_OPEN:
            case FILE_OPEN_IF:
                if (wants_write)
                {
                    return u"r+b";
                }
                if (wants_read)
                {
                    return u"rb";
                }
                break;

            case FILE_OVERWRITE:
            case FILE_OVERWRITE_IF:
                if (wants_write)
                {
                    return u"w+b";
                }
                break;

            default:
                break;
            }

            return {};
        }

        std::optional<std::u16string_view> get_io_device_name(const std::u16string_view filename)
        {
            constexpr std::u16string_view device_prefix = u"\\Device\\";
            if (filename.starts_with(device_prefix))
            {
                return filename.substr(device_prefix.size());
            }

            constexpr std::u16string_view unc_prefix = u"\\??\\";
            if (!filename.starts_with(unc_prefix))
            {
                return std::nullopt;
            }

            const auto path = filename.substr(unc_prefix.size());

            const std::set<std::u16string, std::less<>> devices{
                u"Nsi",
                u"MountPointManager",
                u"SogenGpu",
                u"SogenSteam",
            };

            if (devices.contains(path))
            {
                return path;
            }

            return std::nullopt;
        }

        NTSTATUS handle_named_pipe_create(const syscall_context& c, const emulator_object<handle>& out_handle,
                                          const std::u16string_view filename, const OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>& attributes,
                                          ACCESS_MASK desired_access, ULONG create_options)
        {
            (void)attributes; // This isn't being consumed atm, suppressing errors

            c.win_emu.callbacks.on_generic_access("Creating/opening named pipe", filename);

            mark_named_pipe_connected(c.win_emu, c.proc, filename, c.proc.process_id);
            c.win_emu.broadcast_named_pipe_connect(filename, c.proc.process_id);

            io_device_creation_data data{};

            std::u16string device_name = u"NamedPipe";

            io_device_container container{device_name, c.win_emu, data};

            if (auto* pipe_device = container.get_internal_device<named_pipe>())
            {
                pipe_device->name = std::u16string(filename);
                pipe_device->access = desired_access;
                pipe_device->is_synchronous_handle = (create_options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) != 0;
            }

            const auto handle = c.proc.devices.store(std::move(container));
            out_handle.write(handle);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateFile(const syscall_context& c, const emulator_object<handle> file_handle, ACCESS_MASK desired_access,
                                     const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                     const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> /*io_status_block*/,
                                     const emulator_object<LARGE_INTEGER> /*allocation_size*/, ULONG /*file_attributes*/,
                                     ULONG /*share_access*/, ULONG create_disposition, ULONG create_options, uint64_t ea_buffer,
                                     ULONG ea_length)
        {
            if (create_options & FILE_DELETE_ON_CLOSE && !(desired_access & DELETE))
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attributes = object_attributes.read();
            auto filename = read_unicode_string(c.emu, attributes.ObjectName);

            // Check for console device paths
            // Convert to uppercase for case-insensitive comparison
            std::u16string filename_upper = filename;
            std::ranges::transform(filename_upper, filename_upper.begin(), ::towupper);

            filename = resolve_volume_device_path(resolve_system_root_path(c, std::move(filename)));

            if (pipe_io_trace_enabled())
            {
                c.win_emu.log.info("[pipe-io-trace] NtCreateFile pid=%u tid=%u path='%s'\n", c.proc.process_id, c.thread().id,
                                   u16_to_u8(filename).c_str());
            }

            // Handle console output device
            if (filename_upper == u"\\??\\CONOUT$" || filename_upper == u"\\DEVICE\\CONOUT$" || filename_upper == u"CONOUT$" ||
                filename_upper == u"\\??\\CON" || filename_upper == u"\\DEVICE\\CONSOLE" || filename_upper == u"CON")
            {
                c.win_emu.callbacks.on_generic_access("Opening console output", filename);
                file_handle.write(STDOUT_HANDLE);
                return STATUS_SUCCESS;
            }

            // Handle console input device
            if (filename_upper == u"\\??\\CONIN$" || filename_upper == u"\\DEVICE\\CONIN$" || filename_upper == u"CONIN$")
            {
                c.win_emu.callbacks.on_generic_access("Opening console input", filename);
                file_handle.write(STDIN_HANDLE);
                return STATUS_SUCCESS;
            }

            // Handle NUL device
            if (filename_upper == u"\\??\\NUL" || filename_upper == u"NUL")
            {
                c.win_emu.callbacks.on_generic_access("Opening NUL", filename);
                file_handle.write(NUL_HANDLE);
                return STATUS_SUCCESS;
            }

            if (is_named_pipe_path(filename))
            {
                return handle_named_pipe_create(c, file_handle, filename, attributes, desired_access, create_options);
            }

            auto printer = utils::finally([&] {
                c.win_emu.callbacks.on_generic_access("Opening file", filename); //
            });

            const auto io_device_name = get_io_device_name(filename);
            if (io_device_name.has_value())
            {
                if (*io_device_name == u"DeviceApi\\Dev\\Query")
                {
                    return STATUS_OBJECT_NAME_NOT_FOUND;
                }

                const io_device_creation_data data{
                    .buffer = ea_buffer,
                    .length = ea_length,
                };

                io_device_container container{std::u16string(*io_device_name), c.win_emu, data};

                const auto handle = c.proc.devices.store(std::move(container));
                file_handle.write(handle);

                return STATUS_SUCCESS;
            }

            handle root_handle{};
            root_handle.bits = attributes.RootDirectory;
            if (root_handle.value.is_pseudo && (filename == u"\\Reference" || filename == u"\\Connect"))
            {
                file_handle.write(root_handle);
                return STATUS_SUCCESS;
            }

            if (filename == u"\\??\\CONOUT$")
            {
                file_handle.write(STDOUT_HANDLE);
                return STATUS_SUCCESS;
            }

            file f{};
            f.access_mask = desired_access;
            f.file_mode = create_options & FILE_MODE_MASK;
            f.name = std::move(filename);

            if (attributes.RootDirectory)
            {
                const auto* root = c.proc.files.get(attributes.RootDirectory);
                if (!root)
                {
                    return STATUS_INVALID_HANDLE;
                }

                const auto has_separator = root->name.ends_with(u"\\") || root->name.ends_with(u"/");
                f.name = root->name + (has_separator ? u"" : u"\\") + f.name;
            }

            printer.cancel();

            std::error_code ec{};

            const windows_path path = f.name;

            if (!path.is_absolute())
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            f.drive_number = path.get_drive().value() - 'a' + 1;

            const auto host_path = c.win_emu.file_sys.translate(path);
            const bool file_exists = std::filesystem::exists(host_path, ec);

            if (file_exists && std::filesystem::is_directory(host_path, ec))
            {
                if (create_options & FILE_NON_DIRECTORY_FILE)
                {
                    return STATUS_FILE_IS_A_DIRECTORY;
                }

                if (create_disposition == FILE_CREATE)
                {
                    return STATUS_OBJECT_NAME_COLLISION;
                }

                if (create_disposition != FILE_OPEN && create_disposition != FILE_OPEN_IF)
                {
                    return STATUS_ACCESS_DENIED;
                }

                c.win_emu.callbacks.on_generic_access("Opening folder", f.name);

                f.host_path = host_path;

                const auto handle = c.proc.files.store(std::move(f));
                file_handle.write(handle);

                return STATUS_SUCCESS;
            }

            if (create_options & FILE_DIRECTORY_FILE)
            {
                c.win_emu.callbacks.on_generic_access("Opening folder", f.name);

                if (file_exists)
                {
                    return STATUS_NOT_A_DIRECTORY;
                }

                if (create_disposition != FILE_CREATE && create_disposition != FILE_OPEN_IF)
                {
                    return STATUS_OBJECT_NAME_NOT_FOUND;
                }

                if (!std::filesystem::is_directory(host_path.parent_path(), ec))
                {
                    return STATUS_OBJECT_PATH_NOT_FOUND;
                }

                create_directory(host_path, ec);

                if (ec)
                {
                    return STATUS_ACCESS_DENIED;
                }

                f.host_path = host_path;

                const auto handle = c.proc.files.store(std::move(f));
                file_handle.write(handle);

                return STATUS_SUCCESS;
            }

            c.win_emu.callbacks.on_generic_access("Opening file", f.name);

            if (create_disposition == FILE_CREATE && file_exists)
            {
                return STATUS_OBJECT_NAME_COLLISION;
            }

            if ((create_disposition == FILE_OVERWRITE || create_disposition == FILE_OPEN) && !file_exists)
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            if (create_disposition == FILE_OPEN_IF && !file_exists)
            {
                std::ofstream touch(host_path, std::ios::binary | std::ios::app);
                if (!touch)
                {
                    return STATUS_ACCESS_DENIED;
                }
            }

            std::u16string mode = map_mode(desired_access, create_disposition);

            if (mode.empty() && create_disposition == FILE_CREATE)
            {
                std::ofstream touch(host_path, std::ios::binary | std::ios::app);
                if (!touch)
                {
                    return STATUS_ACCESS_DENIED;
                }

                mode = u"rb";
            }

            if (mode.empty() && (desired_access & DELETE))
            {
                mode = u"rb";
            }

            if (mode.empty() || path.is_relative())
            {
                return STATUS_NOT_SUPPORTED;
            }

            auto [native_file_handle, status] = open_file(c.win_emu.file_sys, path, mode);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            f.handle = std::move(native_file_handle);
            f.open_mode = mode;
            f.host_path = host_path;

            if (create_options & FILE_DELETE_ON_CLOSE)
            {
                c.win_emu.callbacks.on_generic_access("Marking for deletion", f.name);
                f.handle.defer_delete(host_path);
            }

            const auto handle = c.proc.files.store(std::move(f));
            file_handle.write(handle);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryFullAttributesFile(const syscall_context& c,
                                                  const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                                  const emulator_object<FILE_NETWORK_OPEN_INFORMATION> file_information)
        {
            if (!object_attributes)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attributes = object_attributes.read();
            if (!attributes.ObjectName)
            {
                return STATUS_INVALID_PARAMETER;
            }

            auto filename =
                read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, attributes.ObjectName});

            if (attributes.RootDirectory)
            {
                const auto* root = c.proc.files.get(attributes.RootDirectory);
                if (!root)
                {
                    return STATUS_INVALID_HANDLE;
                }

                const auto has_separator = root->name.ends_with(u"\\") || root->name.ends_with(u"/");
                filename = root->name + (has_separator ? u"" : u"\\") + filename;
            }

            filename = resolve_volume_device_path(resolve_system_root_path(c, std::move(filename)));

            c.win_emu.callbacks.on_generic_access("Querying file attributes", filename);

            const windows_path filepath(filename);
            if (!has_valid_filename_characters(filepath.u16string()))
            {
                return STATUS_OBJECT_NAME_INVALID;
            }

            if (filepath.is_relative())
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            const auto local_filename = c.win_emu.file_sys.translate(filepath);

            struct compat_stat file_stat{};
            if (!compat_stat(local_filename, &file_stat))
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            file_information.access([&](FILE_NETWORK_OPEN_INFORMATION& info) {
                info.CreationTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_ctimespec);
                info.LastAccessTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_atimespec);
                info.LastWriteTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_mtimespec);
                info.AllocationSize.QuadPart = static_cast<LONGLONG>(file_stat.st_size);
                info.EndOfFile.QuadPart = static_cast<LONGLONG>(file_stat.st_size);
                info.ChangeTime = info.LastWriteTime;
                info.FileAttributes = (file_stat.st_mode & S_IFDIR) != 0 ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            });

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtQueryAttributesFile(const syscall_context& c,
                                              const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              const emulator_object<FILE_BASIC_INFORMATION> file_information)
        {
            if (!object_attributes)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attributes = object_attributes.read();
            if (!attributes.ObjectName)
            {
                return STATUS_INVALID_PARAMETER;
            }

            auto filename =
                read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, attributes.ObjectName});

            if (attributes.RootDirectory)
            {
                const auto* root = c.proc.files.get(attributes.RootDirectory);
                if (!root)
                {
                    return STATUS_INVALID_HANDLE;
                }

                const auto has_separator = root->name.ends_with(u"\\") || root->name.ends_with(u"/");
                filename = root->name + (has_separator ? u"" : u"\\") + filename;
            }

            filename = resolve_volume_device_path(resolve_system_root_path(c, std::move(filename)));

            c.win_emu.callbacks.on_generic_access("Querying file attributes", filename);

            const windows_path filepath(filename);
            if (!has_valid_filename_characters(filepath.u16string()))
            {
                return STATUS_OBJECT_NAME_INVALID;
            }

            if (filepath.is_relative())
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            const auto local_filename = c.win_emu.file_sys.translate(filepath);

            struct compat_stat file_stat{};
            if (!compat_stat(local_filename, &file_stat))
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            file_information.access([&](FILE_BASIC_INFORMATION& info) {
                info.CreationTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_ctimespec);
                info.LastAccessTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_atimespec);
                info.LastWriteTime = get_file_time(c.win_emu.uses_relative_time(), file_stat.st_mtimespec);
                info.ChangeTime = info.LastWriteTime;
                info.FileAttributes = (file_stat.st_mode & S_IFDIR) != 0 ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            });

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenFile(const syscall_context& c, const emulator_object<handle> file_handle, const ACCESS_MASK desired_access,
                                   const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                   const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, const ULONG share_access,
                                   const ULONG open_options)
        {
            return handle_NtCreateFile(c, file_handle, desired_access, object_attributes, io_status_block, {c.emu.memory()}, 0,
                                       share_access, FILE_OPEN, open_options, 0, 0);
        }

        NTSTATUS handle_NtOpenDirectoryObject(const syscall_context& c, const emulator_object<handle> directory_handle,
                                              const ACCESS_MASK /*desired_access*/,
                                              const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            const auto attributes = object_attributes.read();
            const auto object_name = read_unicode_string(c.emu, attributes.ObjectName);

            if (object_name == u"\\KnownDlls")
            {
                directory_handle.write(KNOWN_DLLS_DIRECTORY);
                return STATUS_SUCCESS;
            }

            if (object_name == u"\\KnownDlls32")
            {
                directory_handle.write(KNOWN_DLLS32_DIRECTORY);
                return STATUS_SUCCESS;
            }

            if (object_name == u"\\Sessions\\1\\BaseNamedObjects")
            {
                directory_handle.write(BASE_NAMED_OBJECTS_DIRECTORY);
                return STATUS_SUCCESS;
            }

            if (object_name == u"\\RPC Control")
            {
                directory_handle.write(RPC_CONTROL_DIRECTORY);
                return STATUS_SUCCESS;
            }

            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCreateDirectoryObject(const syscall_context& /*c*/, const emulator_object<handle> /*directory_handle*/,
                                                const ACCESS_MASK /*desired_access*/,
                                                const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            const auto attributes = object_attributes.read();

            if (attributes.ObjectName == 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtOpenSymbolicLinkObject(const syscall_context& c, const emulator_object<handle> link_handle,
                                                 ACCESS_MASK /*desired_access*/,
                                                 const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            const auto attributes = object_attributes.read();
            const auto object_name = read_unicode_string(c.emu, attributes.ObjectName);

            if (object_name == u"KnownDllPath")
            {
                link_handle.write(KNOWN_DLLS_SYMLINK);
                return STATUS_SUCCESS;
            }

            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtQuerySymbolicLinkObject(const syscall_context& c, const handle link_handle,
                                                  const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> link_target,
                                                  const emulator_object<ULONG> returned_length)
        {
            auto write_target = [&](const std::u16string& target) -> NTSTATUS {
                const auto str_length = static_cast<uint16_t>(target.size() * sizeof(char16_t));
                const auto max_length = static_cast<ULONG>(str_length + sizeof(char16_t));

                returned_length.write(max_length);

                bool too_small = false;
                link_target.access([&](UNICODE_STRING<EmulatorTraits<Emu64>>& str) {
                    if (str.MaximumLength < max_length)
                    {
                        too_small = true;
                        return;
                    }

                    str.Length = str_length;
                    c.emu.write_memory(str.Buffer, target.data(), max_length);
                });

                return too_small ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
            };

            const auto& system_root = c.win_emu.version.get_system_root();

            if (link_handle == KNOWN_DLLS_SYMLINK)
            {
                return write_target((system_root / "System32").u16string());
            }

            if (link_handle == KNOWN_DLLS32_SYMLINK)
            {
                return write_target((system_root / "SysWOW64").u16string());
            }

            return STATUS_NOT_SUPPORTED;
        }

        NTSTATUS handle_NtCreateNamedPipeFile(const syscall_context& c, emulator_object<handle> file_handle, ULONG desired_access,
                                              emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                              emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block, ULONG share_access,
                                              ULONG create_disposition, ULONG create_options, ULONG named_pipe_type, ULONG read_mode,
                                              ULONG completion_mode, ULONG maximum_instances, ULONG inbound_quota, ULONG outbound_quota,
                                              emulator_object<LARGE_INTEGER> default_timeout)
        {
            (void)desired_access;
            (void)share_access;
            (void)create_disposition;

            const auto attributes = object_attributes.read();
            const auto filename = read_unicode_string(c.emu, attributes.ObjectName);

            if (!is_named_pipe_path(filename))
            {
                return STATUS_NOT_SUPPORTED;
            }

            c.win_emu.callbacks.on_generic_access("Creating named pipe", filename);

            io_device_creation_data data{};
            io_device_container container{u"NamedPipe", c.win_emu, data};

            if (auto* pipe_device = container.get_internal_device<named_pipe>())
            {
                pipe_device->name = filename;
                pipe_device->is_server_instance = true;
                pipe_device->pipe_type = named_pipe_type;
                pipe_device->read_mode = read_mode;
                pipe_device->completion_mode = completion_mode;
                pipe_device->max_instances = maximum_instances;
                pipe_device->inbound_quota = inbound_quota;
                pipe_device->outbound_quota = outbound_quota;
                pipe_device->default_timeout = default_timeout.read();
                pipe_device->is_synchronous_handle = (create_options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) != 0;
            }
            else
            {
                return STATUS_NOT_SUPPORTED;
            }

            handle pipe_handle = c.proc.devices.store(std::move(container));
            file_handle.write(pipe_handle);

            c.win_emu.register_named_pipe_server(pipe_short_name(filename));
            c.win_emu.broadcast_named_pipe_server_created(filename);

            IO_STATUS_BLOCK<EmulatorTraits<Emu64>> iosb{};
            iosb.Status = STATUS_SUCCESS;
            iosb.Information = 0;
            io_status_block.write(iosb);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtFsControlFile(const syscall_context& c, const handle file_handle, const handle event,
                                        const emulator_pointer apc_routine, const emulator_pointer apc_context,
                                        const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block,
                                        const ULONG fs_control_code, const emulator_pointer input_buffer, const ULONG input_buffer_length,
                                        const emulator_pointer output_buffer, const ULONG output_buffer_length)
        {
            auto* device = c.proc.devices.get(file_handle);
            if (!device)
            {
                constexpr ULONG FSCTL_GET_REPARSE_POINT = 0x900A8;
                if (fs_control_code == FSCTL_GET_REPARSE_POINT && c.proc.files.get(file_handle))
                {
                    // The target is a regular file or directory, not a reparse point. Returning
                    // STATUS_INVALID_HANDLE here makes callers (e.g. path canonicalization) treat
                    // the buffer as populated; Windows reports STATUS_NOT_A_REPARSE_POINT instead.
                    return STATUS_NOT_A_REPARSE_POINT;
                }

                c.win_emu.log.warn("NtFsControlFile on non-device handle (control code 0x%X)\n", static_cast<uint32_t>(fs_control_code));
                return STATUS_INVALID_HANDLE;
            }

            if (auto* e = c.proc.events.get(event))
            {
                e->signaled = false;
            }

            if (pipe_io_trace_enabled() && fs_control_code == FSCTL_PIPE_LISTEN)
            {
                c.win_emu.log.info("[pipe-io-trace] NtFsControlFile FSCTL_PIPE_LISTEN tid=%u file_handle=0x%llx "
                                   "io_status_block=0x%llx event=0x%llx apc_routine=0x%llx apc_context=0x%llx\n",
                                   c.thread().id, static_cast<unsigned long long>(file_handle.bits),
                                   static_cast<unsigned long long>(io_status_block.value()), static_cast<unsigned long long>(event.bits),
                                   static_cast<unsigned long long>(apc_routine), static_cast<unsigned long long>(apc_context));

                const auto esp = c.emu.reg<uint32_t>(x86_register::esp);
                const auto ebp = c.emu.reg<uint32_t>(x86_register::ebp);

                c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_LISTEN registers tid=%u eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x "
                                   "esi=0x%x edi=0x%x esp=0x%x ebp=0x%x eip=0x%x\n",
                                   c.thread().id, c.emu.reg<uint32_t>(x86_register::eax), c.emu.reg<uint32_t>(x86_register::ebx),
                                   c.emu.reg<uint32_t>(x86_register::ecx), c.emu.reg<uint32_t>(x86_register::edx),
                                   c.emu.reg<uint32_t>(x86_register::esi), c.emu.reg<uint32_t>(x86_register::edi), esp, ebp,
                                   c.emu.reg<uint32_t>(x86_register::eip));

                uint32_t frame_ebp = ebp;
                for (int frame = 0; frame < 32; ++frame)
                {
                    uint32_t saved_ebp = 0;
                    uint32_t frame_return_address = 0;
                    const bool saved_ebp_ok = c.emu.try_read_memory(frame_ebp, &saved_ebp, sizeof(saved_ebp));
                    const bool frame_ret_ok = c.emu.try_read_memory(frame_ebp + 4, &frame_return_address, sizeof(frame_return_address));
                    const auto* frame_ret_mod_name = c.win_emu.mod_manager.find_name(frame_return_address);
                    const auto* frame_ret_mod = c.win_emu.mod_manager.find_by_address(frame_return_address);
                    const auto frame_ret_offset = frame_ret_mod ? frame_return_address - frame_ret_mod->image_base : frame_return_address;

                    c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_LISTEN ebp-chain frame=%d ebp=0x%x (esp+0x%llx) "
                                       "saved_ebp=0x%x (ok=%d) return_address=0x%x (%s+0x%llx) (ok=%d)\n",
                                       frame, frame_ebp, static_cast<unsigned long long>(frame_ebp - esp), saved_ebp, saved_ebp_ok ? 1 : 0,
                                       frame_return_address, frame_ret_mod_name ? frame_ret_mod_name : "<unknown>",
                                       static_cast<unsigned long long>(frame_ret_offset), frame_ret_ok ? 1 : 0);

                    if (!saved_ebp_ok || saved_ebp == 0 || saved_ebp <= frame_ebp)
                    {
                        break;
                    }
                    frame_ebp = saved_ebp;
                }

                uint32_t return_address = 0;
                const bool return_address_read_ok = c.emu.try_read_memory(esp, &return_address, sizeof(return_address));
                const auto* return_mod_name = c.win_emu.mod_manager.find_name(return_address);
                const auto* return_mod = c.win_emu.mod_manager.find_by_address(return_address);
                const auto return_offset = return_mod ? return_address - return_mod->image_base : return_address;

                std::array<uint32_t, 200> stack_words{};
                const bool stack_read_ok = c.emu.try_read_memory(esp, stack_words.data(), sizeof(stack_words));

                std::string stack_dump{};
                for (const auto word : stack_words)
                {
                    stack_dump += utils::string::to_hex_number(word) + " ";
                }

                c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_LISTEN caller tid=%u esp=0x%x return_address=0x%x (%s+0x%llx) "
                                   "(read_ok=%d) stack(esp..)=[%s](read_ok=%d)\n",
                                   c.thread().id, esp, return_address, return_mod_name ? return_mod_name : "<unknown>",
                                   static_cast<unsigned long long>(return_offset), return_address_read_ok ? 1 : 0, stack_dump.c_str(),
                                   stack_read_ok ? 1 : 0);

                for (size_t i = 0; i + 1 < stack_words.size(); ++i)
                {
                    const auto candidate = stack_words.at(i);
                    if (candidate == 0 || stack_words.at(i + 1) != 0)
                    {
                        continue;
                    }

                    uint32_t deref_value = 0;
                    const bool deref_read_ok = c.emu.try_read_memory(candidate, &deref_value, sizeof(deref_value));
                    const auto* deref_mod_name = c.win_emu.mod_manager.find_name(deref_value);

                    c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_LISTEN candidate esp+0x%zx=0x%x -> *candidate=0x%x "
                                       "(read_ok=%d) resolves_to=%s\n",
                                       i * sizeof(uint32_t), candidate, deref_value, deref_read_ok ? 1 : 0,
                                       deref_mod_name ? deref_mod_name : "<unresolved>");
                }

                if (return_address_read_ok && !return_mod)
                {
                    std::array<uint8_t, 16> code_bytes{};
                    const bool code_read_ok = c.emu.try_read_memory(return_address, code_bytes.data(), code_bytes.size());
                    std::string code_dump{};
                    for (const auto byte : code_bytes)
                    {
                        code_dump += utils::string::to_hex_number(byte) + " ";
                    }

                    constexpr uint32_t max_scan_back = 0x400000;
                    const uint32_t page_aligned = return_address & ~0xFFFu;
                    uint32_t mz_base = 0;
                    uint32_t readable_pages = 0;

                    for (uint32_t offset = 0; offset <= max_scan_back; offset += 0x1000)
                    {
                        const uint32_t probe = page_aligned - offset;
                        uint16_t mz_signature = 0;
                        const bool page_readable = c.emu.try_read_memory(probe, &mz_signature, sizeof(mz_signature));
                        if (page_readable)
                        {
                            ++readable_pages;
                        }
                        if (page_readable && mz_signature == 0x5A4D)
                        {
                            mz_base = probe;
                            break;
                        }
                    }

                    c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_LISTEN caller unresolved by module_manager; "
                                       "bytes at return_address (read_ok=%d)=[%s] readable_pages_scanned_back=%u/1025\n",
                                       code_read_ok ? 1 : 0, code_dump.c_str(), readable_pages);

                    if (mz_base != 0)
                    {
                        uint32_t e_lfanew = 0;
                        c.emu.try_read_memory(mz_base + 0x3C, &e_lfanew, sizeof(e_lfanew));

                        uint32_t pe_signature = 0;
                        uint16_t machine = 0;
                        uint32_t size_of_image = 0;
                        uint32_t export_dir_rva = 0;
                        std::string export_name{};

                        c.emu.try_read_memory(mz_base + e_lfanew, &pe_signature, sizeof(pe_signature));
                        c.emu.try_read_memory(mz_base + e_lfanew + 4, &machine, sizeof(machine));
                        c.emu.try_read_memory(mz_base + e_lfanew + 0x50, &size_of_image, sizeof(size_of_image));
                        c.emu.try_read_memory(mz_base + e_lfanew + 0x78, &export_dir_rva, sizeof(export_dir_rva));

                        if (export_dir_rva != 0)
                        {
                            uint32_t export_name_rva = 0;
                            if (c.emu.try_read_memory(mz_base + export_dir_rva + 0xC, &export_name_rva, sizeof(export_name_rva)) &&
                                export_name_rva != 0)
                            {
                                std::array<char, 256> name_buffer{};
                                if (c.emu.try_read_memory(mz_base + export_name_rva, name_buffer.data(), name_buffer.size() - 1))
                                {
                                    export_name = name_buffer.data();
                                }
                            }
                        }

                        c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_LISTEN caller unresolved by module_manager; backward MZ scan "
                                           "found header at 0x%x (-0x%llx from return_address) pe_sig=0x%x machine=0x%x "
                                           "size_of_image=0x%x export_name='%s'\n",
                                           mz_base, static_cast<unsigned long long>(return_address - mz_base), pe_signature, machine,
                                           size_of_image, export_name.empty() ? "<none>" : export_name.c_str());
                    }
                }
            }

            if (pipe_io_trace_enabled() && fs_control_code == FSCTL_PIPE_WAIT)
            {
                int64_t requested_timeout = 0;
                uint8_t timeout_specified = 0;
                const bool timeout_read_ok = c.emu.try_read_memory(input_buffer, &requested_timeout, sizeof(requested_timeout));
                c.emu.try_read_memory(input_buffer + 12, &timeout_specified, sizeof(timeout_specified));

                constexpr size_t wait_name_offset = 14;
                std::u16string wait_target_name{};
                bool wait_target_name_known = false;
                if (input_buffer_length >= wait_name_offset)
                {
                    const auto wait_name_length = c.emu.read_memory<uint32_t>(input_buffer + 8);
                    const auto wait_name_bytes = std::min<size_t>(wait_name_length, input_buffer_length - wait_name_offset);
                    wait_target_name.resize(wait_name_bytes / sizeof(char16_t), u'\0');
                    if (!wait_target_name.empty())
                    {
                        c.emu.read_memory(input_buffer + wait_name_offset, wait_target_name.data(),
                                          wait_target_name.size() * sizeof(char16_t));
                        wait_target_name_known = c.win_emu.is_named_pipe_server_known(wait_target_name);
                    }
                }

                c.win_emu.log.info("[pipe-io-trace] NtFsControlFile FSCTL_PIPE_WAIT pid=%u tid=%u file_handle=0x%llx "
                                   "requested_timeout=%lld (100ns units, read_ok=%d) timeout_specified=%u target_name='%s' "
                                   "server_already_known=%d\n",
                                   c.proc.process_id, c.thread().id, static_cast<unsigned long long>(file_handle.bits),
                                   static_cast<long long>(requested_timeout), timeout_read_ok ? 1 : 0, timeout_specified,
                                   u16_to_u8(wait_target_name).c_str(), wait_target_name_known ? 1 : 0);

                const auto esp = c.emu.reg<uint32_t>(x86_register::esp);
                const auto ebp = c.emu.reg<uint32_t>(x86_register::ebp);

                c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_WAIT registers pid=%u tid=%u eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x "
                                   "esi=0x%x edi=0x%x esp=0x%x ebp=0x%x eip=0x%x\n",
                                   c.proc.process_id, c.thread().id, c.emu.reg<uint32_t>(x86_register::eax),
                                   c.emu.reg<uint32_t>(x86_register::ebx), c.emu.reg<uint32_t>(x86_register::ecx),
                                   c.emu.reg<uint32_t>(x86_register::edx), c.emu.reg<uint32_t>(x86_register::esi),
                                   c.emu.reg<uint32_t>(x86_register::edi), esp, ebp, c.emu.reg<uint32_t>(x86_register::eip));

                uint32_t frame_ebp = ebp;
                for (int frame = 0; frame < 8; ++frame)
                {
                    uint32_t saved_ebp = 0;
                    uint32_t frame_return_address = 0;
                    const bool saved_ebp_ok = c.emu.try_read_memory(frame_ebp, &saved_ebp, sizeof(saved_ebp));
                    const bool frame_ret_ok = c.emu.try_read_memory(frame_ebp + 4, &frame_return_address, sizeof(frame_return_address));
                    const auto* frame_ret_mod_name = c.win_emu.mod_manager.find_name(frame_return_address);
                    const auto* frame_ret_mod = c.win_emu.mod_manager.find_by_address(frame_return_address);
                    const auto frame_ret_offset = frame_ret_mod ? frame_return_address - frame_ret_mod->image_base : frame_return_address;

                    c.win_emu.log.info("[pipe-io-trace] FSCTL_PIPE_WAIT ebp-chain pid=%u frame=%d ebp=0x%x (esp+0x%llx) "
                                       "saved_ebp=0x%x (ok=%d) return_address=0x%x (%s+0x%llx) (ok=%d)\n",
                                       c.proc.process_id, frame, frame_ebp, static_cast<unsigned long long>(frame_ebp - esp), saved_ebp,
                                       saved_ebp_ok ? 1 : 0, frame_return_address, frame_ret_mod_name ? frame_ret_mod_name : "<unknown>",
                                       static_cast<unsigned long long>(frame_ret_offset), frame_ret_ok ? 1 : 0);

                    if (!saved_ebp_ok || saved_ebp == 0 || saved_ebp <= frame_ebp)
                    {
                        break;
                    }
                    frame_ebp = saved_ebp;
                }
            }

            io_device_context context{c.emu};
            context.event = event;
            context.apc_routine = apc_routine;
            context.apc_context = apc_context;
            context.io_status_block = io_status_block;
            context.io_control_code = fs_control_code;
            context.input_buffer = input_buffer;
            context.input_buffer_length = input_buffer_length;
            context.output_buffer = output_buffer;
            context.output_buffer_length = output_buffer_length;
            context.vcpu = &c.vcpu;

            return device->execute_ioctl(c.win_emu, context);
        }

        NTSTATUS handle_NtFlushBuffersFile(const syscall_context& c, const handle file_handle,
                                           const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> /*io_status_block*/)
        {
            if (file_handle == STDOUT_HANDLE)
            {
                return STATUS_SUCCESS;
            }

            const auto* f = c.proc.files.get(file_handle);
            if (!f)
            {
                return STATUS_INVALID_HANDLE;
            }

            (void)fflush(f->handle);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCancelIoFile(const syscall_context& c, const handle file_handle,
                                       const emulator_object<IO_STATUS_BLOCK<EmulatorTraits<Emu64>>> io_status_block)
        {
            const auto* f = c.proc.files.get(file_handle);
            if (!f)
            {
                const auto* device = c.proc.devices.get(file_handle);
                if (!device)
                {
                    return STATUS_INVALID_HANDLE;
                }
            }

            constexpr auto status = STATUS_NOT_FOUND;
            write_lock_io_status(io_status_block, status);
            return status;
        }
    }

} // namespace sogen
