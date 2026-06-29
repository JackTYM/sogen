#include "std_include.hpp"
#include "win32k_userconnect.hpp"

#include "process_context.hpp"
#include "windows_emulator.hpp"

namespace sogen
{

    namespace
    {
        constexpr size_t k_dispatch_client_message_index = 21;
        constexpr size_t k_ntdll_probe_size = 128;
        constexpr size_t k_expected_pfn_pointer_count = 3;
        constexpr size_t k_client_pfn_array_size = FNID_ARRAY_SIZE * sizeof(uint64_t);
        constexpr size_t k_client_worker_pfn_array_size = 0x90;

        struct client_pfn_arrays
        {
            uint64_t ansi{};
            uint64_t wide{};
            uint64_t worker{};
        };

        std::vector<uint64_t> scan_rip_relative_lea_references(const std::vector<uint8_t>& bytes, const uint64_t code_base,
                                                               const size_t max_results)
        {
            std::vector<uint64_t> results;
            results.reserve(max_results);

            for (size_t i = 0; i + 6 < bytes.size(); ++i)
            {
                if (bytes[i] != 0x48 || bytes[i + 1] != 0x8D || (bytes[i + 2] & 0xC7) != 0x05)
                {
                    continue;
                }

                int32_t disp{};
                std::memcpy(&disp, &bytes[i + 3], sizeof(disp));
                results.push_back(code_base + i + 7 + disp);

                if (results.size() >= max_results)
                {
                    break;
                }
            }

            return results;
        }

    }

    namespace win32k_userconnect
    {
        void refresh_dispatch_client_message(process_context& process)
        {
            uint64_t dispatch_client_message = 0;

            process.user_handles.get_server_info().access([&](const USER_SERVERINFO& server_info) {
                dispatch_client_message = server_info.apfnClientA[k_dispatch_client_message_index];
                if (dispatch_client_message == 0)
                {
                    dispatch_client_message = server_info.apfnClientW[k_dispatch_client_message_index];
                }
            });

            if (dispatch_client_message != 0)
            {
                process.dispatch_client_message = dispatch_client_message;
            }
        }
    }

    namespace
    {

        bool try_read_exact(memory_interface& memory, const uint64_t address, void* data, const size_t size)
        {
            return address != 0 && memory.try_read_memory(address, data, size);
        }

        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        bool try_copy_client_pfn_array(memory_interface& memory, const uint64_t source, uint64_t (&destination)[FNID_ARRAY_SIZE],
                                       const size_t source_size)
        {
            std::ranges::fill(destination, 0);
            return try_read_exact(memory, source, destination, std::min(sizeof(destination), source_size));
        }

        // user32 builds MessageBox dialogs from SERVERINFO.MBStrings: an array of
        // { WCHAR achName[16]; UINT id; UINT uStr; } (0x28 bytes) at gpsi + 0x3A4, indexed by the
        // standard order OK, Cancel, Abort, Retry, Ignore, Yes, No, Close, Help, Try Again, Continue.
        // SoftModalMessageBox reads each button's control id (+0x20) and caption (+0x00) from here, so
        // without it message-box buttons get id 0 (wrong return value) and empty captions.
        void seed_messagebox_button_strings(memory_interface& memory, const uint64_t serverinfo_base)
        {
            if (serverinfo_base == 0)
            {
                return;
            }

            struct mb_string
            {
                std::u16string_view text;
                uint32_t id;
            };
            static constexpr std::array<mb_string, 11> entries = {{
                {.text = u"OK", .id = 1},          // IDOK
                {.text = u"Cancel", .id = 2},      // IDCANCEL
                {.text = u"&Abort", .id = 3},      // IDABORT
                {.text = u"&Retry", .id = 4},      // IDRETRY
                {.text = u"&Ignore", .id = 5},     // IDIGNORE
                {.text = u"&Yes", .id = 6},        // IDYES
                {.text = u"&No", .id = 7},         // IDNO
                {.text = u"&Close", .id = 8},      // IDCLOSE
                {.text = u"Help", .id = 9},        // IDHELP
                {.text = u"&Try Again", .id = 10}, // IDTRYAGAIN
                {.text = u"&Continue", .id = 11},  // IDCONTINUE
            }};

            constexpr uint64_t k_mbstrings_offset = 0x3A4;
            constexpr uint64_t k_mbstrings_entry_size = 0x28;
            constexpr uint64_t k_mbstrings_id_offset = 0x20;
            constexpr size_t k_mbstrings_name_capacity = 16; // WCHAR achName[16]

            for (size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry_base = serverinfo_base + k_mbstrings_offset + i * k_mbstrings_entry_size;

                std::array<char16_t, k_mbstrings_name_capacity> name{};
                const auto copy_count = std::min<size_t>(entries[i].text.size(), k_mbstrings_name_capacity - 1);
                std::ranges::copy_n(entries[i].text.begin(), static_cast<std::ptrdiff_t>(copy_count), name.begin());
                memory.write_memory(entry_base, name.data(), name.size() * sizeof(char16_t));

                const auto id = entries[i].id;
                memory.write_memory(entry_base + k_mbstrings_id_offset, &id, sizeof(id));
            }
        }

        bool try_copy_client_pfn_arrays(memory_interface& memory, process_context& process, const client_pfn_arrays arrays)
        {
            if (arrays.ansi == 0 || arrays.wide == 0 || arrays.worker == 0)
            {
                return false;
            }

            bool copied = false;
            process.user_handles.get_server_info().access([&](USER_SERVERINFO& server_info) {
                copied = try_copy_client_pfn_array(memory, arrays.ansi, server_info.apfnClientA, k_client_pfn_array_size) &&
                         try_copy_client_pfn_array(memory, arrays.wide, server_info.apfnClientW, k_client_pfn_array_size) &&
                         try_copy_client_pfn_array(memory, arrays.worker, server_info.apfnClientWorker, k_client_worker_pfn_array_size);
            });

            if (!copied)
            {
                return false;
            }

            // Seed after the pfn copy: the worker-array fill above zeroes part of the MBStrings region.
            seed_messagebox_button_strings(memory, process.user_handles.get_server_info().value());

            win32k_userconnect::refresh_dispatch_client_message(process);
            return true;
        }

    }

    namespace win32k_userconnect
    {
        NTSTATUS narrow_wow64_address(const uint64_t address, uint32_t& narrowed)
        {
            narrowed = 0;

            if (address > std::numeric_limits<uint32_t>::max())
            {
                return STATUS_INVALID_PARAMETER;
            }

            narrowed = static_cast<uint32_t>(address);
            return STATUS_SUCCESS;
        }

        NTSTATUS resolve_wow64_destination(const uint64_t user_connect_ptr, const uint64_t user_connect_length, uint32_t& destination)
        {
            destination = 0;

            if (user_connect_ptr == 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            if (user_connect_length < sizeof(WIN32K_USERCONNECT32))
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            uint64_t offset = 0;
            if (user_connect_length == sizeof(WIN32K_USERCONNECT32))
            {
                offset = 0;
            }
            else if (user_connect_length == (sizeof(WIN32K_USERCONNECT32) + k_wow64_userconnect_header_size))
            {
                offset = k_wow64_userconnect_header_size;
            }
            else
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto destination64 = user_connect_ptr + offset;
            if (destination64 < user_connect_ptr)
            {
                return STATUS_INVALID_PARAMETER;
            }

            return narrow_wow64_address(destination64, destination);
        }

        NTSTATUS build_wow64_userconnect(const process_context& process, WIN32K_USERCONNECT32& connect)
        {
            connect = {};

            uint32_t psi{};
            uint32_t disp_info{};
            uint32_t ahe_list{};
            uint32_t monitor_info{};

            auto status = narrow_wow64_address(process.user_handles.get_server_info().value(), psi);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            status = narrow_wow64_address(process.user_handles.get_display_info().value(), disp_info);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            status = narrow_wow64_address(process.user_handles.get_handle_table().value(), ahe_list);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            status = narrow_wow64_address(process.user_handles.get_display_info().value(), monitor_info);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            uint32_t wndmsg_bitmap{};
            status = narrow_wow64_address(process.user_handles.get_wow64_wndmsg_bitmap(), wndmsg_bitmap);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            uint32_t ime_msg_bitmap{};
            status = narrow_wow64_address(process.user_handles.get_wow64_ime_msg_bitmap(), ime_msg_bitmap);
            if (status != STATUS_SUCCESS)
            {
                return status;
            }

            connect.psi = psi;
            connect.ahe_list = ahe_list;
            connect.he_entry_size = sizeof(USER_HANDLEENTRY);
            connect.disp_info_low = disp_info;
            connect.monitor_info_low = monitor_info;
            connect.wndmsg_count = k_wow64_wndmsg_count;
            connect.wndmsg_bits = wndmsg_bitmap;
            connect.ime_msg_count = k_wow64_ime_msg_count;
            connect.ime_msg_bits = ime_msg_bitmap;

            return STATUS_SUCCESS;
        }

        bool try_write_wow64_userconnect(memory_interface& memory, const uint64_t destination, const WIN32K_USERCONNECT32& connect)
        {
            try
            {
                const emulator_object<WIN32K_USERCONNECT32> connect_obj{memory, destination};
                connect_obj.write(connect);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        void populate_user_shared_info(USER_SHAREDINFO& shared, const process_context& process)
        {
            shared.psi = process.user_handles.get_server_info().value();
            shared.aheList = process.user_handles.get_handle_table().value();
            shared.HeEntrySize = sizeof(USER_HANDLEENTRY);
            shared.pDispInfo = process.user_handles.get_display_info().value();
            for (int i = 0; i < FNID_ARRAY_SIZE; i++)
            {
                shared.awmControl[i] = process.user_handles.get_awm_control_message(i);
            }
            shared.DefWindowMsgs = process.user_handles.get_def_window_messages();
            shared.DefWindowSpecMsgs = process.user_handles.get_def_window_spec_messages();
        }

        bool try_write_user_shared_info(memory_interface& memory, const uint64_t destination, const process_context& process)
        {
            try
            {
                const emulator_object<USER_SHAREDINFO> shared_obj{memory, destination};
                auto shared = shared_obj.read();
                populate_user_shared_info(shared, process);
                shared_obj.write(shared);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool try_write_api_port_userconnect_reply(memory_interface& memory, const uint64_t reply_base, const process_context& process)
        {
            const auto destination = reply_base + k_wow64_userconnect_reply_shared_info_offset;
            if (destination < reply_base)
            {
                return false;
            }

            return try_write_user_shared_info(memory, destination, process);
        }

        bool try_update_client_pfn_arrays_from_addresses(memory_interface& memory, process_context& process, const uint64_t apfn_client_a,
                                                         const uint64_t apfn_client_w, const uint64_t apfn_client_worker)
        {
            return try_copy_client_pfn_arrays(
                memory, process, client_pfn_arrays{.ansi = apfn_client_a, .wide = apfn_client_w, .worker = apfn_client_worker});
        }

        bool try_bootstrap_client_pfn_arrays_from_ntdll(windows_emulator& win_emu)
        {
            if (!win_emu.mod_manager.ntdll)
            {
                return false;
            }

            const auto retrieve_user_pfn = win_emu.mod_manager.ntdll->find_export("RtlRetrieveNtUserPfn");
            if (retrieve_user_pfn == 0)
            {
                return false;
            }

            std::vector<uint8_t> code_window(k_ntdll_probe_size);
            if (!win_emu.memory.try_read_memory(retrieve_user_pfn, code_window.data(), code_window.size()))
            {
                return false;
            }

            const auto pointers = scan_rip_relative_lea_references(code_window, retrieve_user_pfn, k_expected_pfn_pointer_count);
            if (pointers.size() != k_expected_pfn_pointer_count)
            {
                return false;
            }

            return try_update_client_pfn_arrays_from_addresses(win_emu.memory, win_emu.process, pointers[0], pointers[1], pointers[2]);
        }

        bool try_write_wow64_hybrid_userconnect(memory_interface& memory, const uint64_t destination, const process_context& process)
        {
            // Write USER_SHAREDINFO so awmControl[0..13] is populated for KCT window-message dispatch
            // (rendering). Then overwrite awmControl[14] — which lands at offset 0x108 where 32-bit
            // user32 expects wndmsg_count/wndmsg_bits — with the flat zero bitmap so that
            // WM_LBUTTONDOWN (bit not set) falls through to the stored wndproc instead of taking a
            // garbage KCT path.
            if (!try_write_user_shared_info(memory, destination, process))
            {
                return false;
            }

            static_assert(offsetof(USER_SHAREDINFO, awmControl) == 0x28);
            constexpr size_t k_wndmsg_awm_index = 14;
            const uint64_t awm14_addr =
                destination + offsetof(USER_SHAREDINFO, awmControl) + k_wndmsg_awm_index * sizeof(USER_WNDMSG);

            USER_WNDMSG awm14{};
            awm14.maxMsgs = k_wow64_wndmsg_count;
            awm14.abMsgs = process.user_handles.get_wow64_wndmsg_bitmap();

            try
            {
                memory.write_memory(awm14_addr, &awm14, sizeof(awm14));
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool try_write_64bit_user_shared_info(windows_emulator& win_emu)
        {
            // gSharedInfo RVA in 64-bit user32.dll (IDB base 0x180000000):
            //   gSharedInfo at IDB VA 0x1800B9030 → RVA 0xB9030
            // awmControl starts at gSharedInfo+0x28; FNID_BUTTON (index 7) lands at +0x98,
            // exactly where ButtonWndProcW reads its message bitmap gate.
            static_assert(offsetof(USER_SHAREDINFO, awmControl) == 0x28);
            constexpr uint64_t k_idb_base = 0x180000000ULL;
            constexpr uint64_t k_gsharedinfo_idb = 0x1800B9030ULL;
            constexpr uint64_t k_gsharedinfo_rva = k_gsharedinfo_idb - k_idb_base;

            // gSharedInfo is a per-process mapping accessed by both 64-bit ntdll and user32 at the
            // same RVA (0xB9030) relative to whichever module is loaded at 0x180000000. In sogen's
            // WOW64 setup, 64-bit user32 is not loaded as a separate module; ntdll occupies that
            // base address and its data section at this RVA is the live gSharedInfo the KCT stubs
            // and initialisation paths read. Write through ntdll's image_base so the destination
            // is always resolved correctly at runtime, regardless of ASLR.
            const auto* ntdll = win_emu.mod_manager.ntdll;
            if (!ntdll)
            {
                return false;
            }

            const uint64_t destination = ntdll->image_base + k_gsharedinfo_rva;
            return try_write_user_shared_info(win_emu.memory, destination, win_emu.process);
        }

        bool try_populate_wow64win_client_tables(windows_emulator& win_emu)
        {
            const auto* wow64win = win_emu.mod_manager.wow64_modules_.wow64win_dll;
            if (!wow64win)
            {
                return false;
            }

            // Global addresses from wow64win IDB (base 0x180000000):
            //   apfnClientWClient       0x180078310  (whcbfnDWORD reads DWORD at 8*i from this)
            //   apfnClientWKernel       0x180078318  (whcbfnDWORD scans for xpfnProc match)
            //   apfnClientWorkerKernel  0x180078320
            //   apfnClientAClient       0x180078328
            //   apfnClientAKernel       0x180078578
            //   apfnClientWorkerClient  0x180078588
            constexpr uint64_t k_idb_base = 0x180000000ULL;
            constexpr uint64_t k_client_w_idb = 0x180078310ULL;
            constexpr uint64_t k_kernel_w_idb = 0x180078318ULL;
            constexpr uint64_t k_kernel_worker_idb = 0x180078320ULL;
            constexpr uint64_t k_client_a_idb = 0x180078328ULL;
            constexpr uint64_t k_kernel_a_idb = 0x180078578ULL;
            constexpr uint64_t k_worker_c_idb = 0x180078588ULL;

            const uint64_t delta = wow64win->image_base - k_idb_base;
            const uint64_t client_w_addr = k_client_w_idb + delta;
            const uint64_t kernel_w_addr = k_kernel_w_idb + delta;
            const uint64_t kernel_worker_addr = k_kernel_worker_idb + delta;
            const uint64_t client_a_addr = k_client_a_idb + delta;
            const uint64_t kernel_a_addr = k_kernel_a_idb + delta;
            const uint64_t worker_c_addr = k_worker_c_idb + delta;

            // Find ntdll32's NtUserPfn slot table (the .mrdata array written by 32-bit user32 at
            // init time). Each slot is 8 bytes; the DWORD at slot[i] is the 32-bit wndproc address
            // for FNID index i.  whcbfnDWORD reads apfnClient*Client with an 8-byte stride and
            // extracts the DWORD at 8*i, so the layout matches exactly.
            const auto* ntdll32 = win_emu.mod_manager.wow64_modules_.ntdll32;
            if (!ntdll32)
            {
                return false;
            }

            // RVA 0x12b0c0 is the NtUserPfnArray table inside ntdll32's .mrdata section.
            constexpr uint32_t k_mrdata_rva = 0x12b0c0;
            const uint64_t mrdata_base = ntdll32->image_base + k_mrdata_rva;

            // apfnClient*Client: point directly at the live .mrdata table so whcbfnDWORD reads
            // the real 32-bit wndproc addresses that user32 writes at initialisation time.
            win_emu.memory.write_memory(client_w_addr, &mrdata_base, sizeof(mrdata_base));
            win_emu.memory.write_memory(client_a_addr, &mrdata_base, sizeof(mrdata_base));
            win_emu.memory.write_memory(worker_c_addr, &mrdata_base, sizeof(mrdata_base));

            // apfnClient*Kernel: 64-bit pfn tables that whcbfnDWORD scans for a match against the
            // dispatched xpfnProc. NtWow64UserConnectHook copies USER_SERVERINFO into gSharedInfo at
            // connect time and sets these pointers into gSharedInfo; however that snapshot is taken
            // before try_bootstrap_client_pfn_arrays_from_ntdll populates the pfn arrays, leaving
            // the kernel-side tables empty. Point them directly at the live USER_SERVERINFO arrays so
            // whcbfnDWORD finds its match regardless of when the bootstrap runs.
            const uint64_t server_info = win_emu.process.user_handles.get_server_info().value();
            const uint64_t pfn_a_ptr = server_info + offsetof(USER_SERVERINFO, apfnClientA);
            const uint64_t pfn_w_ptr = server_info + offsetof(USER_SERVERINFO, apfnClientW);
            const uint64_t pfn_worker_ptr = server_info + offsetof(USER_SERVERINFO, apfnClientWorker);
            win_emu.memory.write_memory(kernel_w_addr, &pfn_w_ptr, sizeof(pfn_w_ptr));
            win_emu.memory.write_memory(kernel_a_addr, &pfn_a_ptr, sizeof(pfn_a_ptr));
            win_emu.memory.write_memory(kernel_worker_addr, &pfn_worker_ptr, sizeof(pfn_worker_ptr));

            return true;
        }
    }

} // namespace sogen
