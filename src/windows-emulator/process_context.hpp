#pragma once

#include "emulator_utils.hpp"
#include "handles.hpp"
#include "registry/registry_manager.hpp"

#include "module/module_manager.hpp"
#include <utils/nt_handle.hpp>

#include <arch_emulator.hpp>

#include "io_device.hpp"
#include "kusd_mmio.hpp"
#include "windows_objects.hpp"
#include "emulator_thread.hpp"
#include "port.hpp"
#include "user_handle_table.hpp"

#include "apiset/apiset.hpp"

namespace sogen
{

    struct fake_environment_config;

#define PEB_SEGMENT_SIZE (20 << 20) // 20 MB
#define GS_SEGMENT_SIZE  (1 << 20)  // 1 MB

#define STACK_SIZE       0x40000ULL // 256KB

#ifdef __APPLE__
// Darwin refuses MAP_FIXED anywhere in the low ~4GB regardless of ASLR (the standard 64-bit
// Mach-O __PAGEZERO convention, enforced at the mmap syscall level) - a backend sharing the guest
// address space with the host process (guest VA == host VA, e.g. FEX) can never place anything
// there. GDT_ADDR is a fixed constant sogen always uses directly (not chosen dynamically via
// find_free_allocation_base, so the reserved-host-ranges mechanism can't route around it), so it
// has to live well above that floor here. Chosen far from typical host dyld/heap/stack placement
// (which stays within a few GB above 4GB) to also avoid the *dynamic*, ASLR-dependent collisions
// that reserved-host-ranges handles for everything else.
#define GDT_ADDR 0x7ffff0000000ULL
#else
#define GDT_ADDR 0x35000
#endif
#define GDT_LIMIT      0x1000
#define GDT_ENTRY_SIZE 0x8

    // Each vCPU gets its own GDT page. Most descriptors are identical, but the WOW64 FS descriptor
    // (selector 0x53) holds a per-thread 32-bit TEB base that the guest reloads on every 64<->32
    // transition, so a shared GDT would let a WOW64 thread on one vCPU read another vCPU's TEB base.
    // base is normally GDT_ADDR, but see memory_manager::gdt_base's doc comment for why it isn't
    // always that fixed value.
    constexpr uint64_t gdt_base_for_vcpu(const uint64_t base, const size_t vcpu_index) noexcept
    {
        return base + vcpu_index * GDT_LIMIT;
    }

// TODO: Get rid of that
#define WOW64_NATIVE_STACK_SIZE      0x40000ULL
#define WOW64_32BIT_STACK_SIZE       (1 << 20)

// A WoW64 thread's *native* 64-bit stack must live in the low 4GB: wow64win.dll's win32k
// callback-marshaling thunks (e.g. fnINLPCREATESTRUCT for WM_NCCREATE) build the 32-bit call
// frame on the native stack and truncate the 64-bit stack pointer to 32 bits before handing it
// to the 32-bit window proc. Search from a base above the 32-bit module/heap region to avoid
// low-address collisions.
#define WOW64_NATIVE_STACK_BASE_HINT 0x70000000ULL

    struct emulator_settings;
    struct application_settings;
    class windows_version_manager;

    using knowndlls_map = std::map<std::u16string, section>;

    struct file_lock_range
    {
        uint64_t offset{};
        uint64_t length{};
        ULONG key{};
        bool exclusive{};
        handle owner{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->offset);
            buffer.write(this->length);
            buffer.write(this->key);
            buffer.write(this->exclusive);
            buffer.write(this->owner);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->offset);
            buffer.read(this->length);
            buffer.read(this->key);
            buffer.read(this->exclusive);
            buffer.read(this->owner);
        }
    };

    struct file_lock_ranges
    {
        std::vector<file_lock_range> locks{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write_vector(this->locks);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read_vector(this->locks);
        }
    };

    struct gdi_bitmap_surface
    {
        uint32_t width{};
        uint32_t height{};
        std::vector<uint32_t> pixels{};

        emulator_pointer guest_bits{};
        uint32_t guest_stride{};
        uint32_t guest_bpp{32};
        bool guest_top_down{};
        bool guest_owns_memory{};

        int32_t dimension_cx{};
        int32_t dimension_cy{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->width);
            buffer.write(this->height);
            buffer.write_vector(this->pixels);
            buffer.write(this->guest_bits);
            buffer.write(this->guest_stride);
            buffer.write(this->guest_bpp);
            buffer.write(this->guest_top_down);
            buffer.write(this->guest_owns_memory);
            buffer.write(this->dimension_cx);
            buffer.write(this->dimension_cy);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->width);
            buffer.read(this->height);
            buffer.read_vector(this->pixels);
            buffer.read(this->guest_bits);
            buffer.read(this->guest_stride);
            buffer.read(this->guest_bpp);
            buffer.read(this->guest_top_down);
            buffer.read(this->guest_owns_memory);
            buffer.read(this->dimension_cx);
            buffer.read(this->dimension_cy);
        }
    };

    struct gdi_dc_state
    {
        uint32_t selected_bitmap{};
        hwnd target_window{};
        int32_t current_x{};
        int32_t current_y{};
        bool is_memory_dc{};
        uint32_t map_mode{1}; // MM_TEXT, the real GDI default for a freshly created DC

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->selected_bitmap);
            buffer.write(this->target_window);
            buffer.write(this->current_x);
            buffer.write(this->current_y);
            buffer.write(this->is_memory_dc);
            buffer.write(this->map_mode);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->selected_bitmap);
            buffer.read(this->target_window);
            buffer.read(this->current_x);
            buffer.read(this->current_y);
            buffer.read(this->is_memory_dc);
            buffer.read(this->map_mode);
        }
    };

    struct process_context
    {
        struct callbacks
        {
            utils::optional_function<void(handle h, emulator_thread& thr)> on_thread_create{};
            utils::optional_function<void(handle h, emulator_thread& thr)> on_thread_terminated{};
            utils::optional_function<void(emulator_thread& current_thread, emulator_thread& new_thread)> on_thread_switch{};
            utils::optional_function<void(emulator_thread& current_thread)> on_thread_set_name{};
        };

        struct atom_entry
        {
            std::u16string name{};
            uint32_t ref_count = 0;

            void serialize(utils::buffer_serializer& buffer) const
            {
                buffer.write(this->name);
                buffer.write(this->ref_count);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                buffer.read(this->name);
                buffer.read(this->ref_count);
            }
        };

        // A child process created via NtCreateUserProcess, running as a separate, independent host OS
        // process (see windows_emulator's create_child_process callback) - exit_status is STATUS_PENDING
        // for the lifetime of this record, since nothing observes a still-running child's real exit
        // status yet (NtWaitForSingleObject/NtQueryInformationProcess support for the minted process
        // handle remains unimplemented).
        struct child_process_record
        {
            windows_path image_path{};
            uint32_t pid = 0;
            NTSTATUS exit_status = STATUS_SUCCESS;

            void serialize(utils::buffer_serializer& buffer) const
            {
                buffer.write(this->image_path);
                buffer.write(this->pid);
                buffer.write(this->exit_status);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                buffer.read(this->image_path);
                buffer.read(this->pid);
                buffer.read(this->exit_status);
            }
        };

        // A thread-specific hook installed via NtUserSetWindowsHookEx. MFC's AfxHookWindowCreate
        // relies on a WH_CBT hook receiving a real HCBT_CREATEWND notification (dispatched by
        // NtUserCreateWindowEx) to bind CWnd::m_hWnd to a newly created window - see
        // handle_NtUserSetWindowsHookEx/handle_NtUserCreateWindowEx.
        struct windows_hook_entry
        {
            int32_t id_hook{};
            uint32_t thread_id{};
            emulator_pointer proc{};
        };

        struct class_entry
        {
            emulator_pointer guest_obj_addr{};
            EMU_WNDCLASSEX wnd_class{};
            CLSMENUNAME<EmulatorTraits<Emu64>> menu_name{};

            class_entry() = default;

            class_entry(const emulator_pointer guest_obj, const EMU_WNDCLASSEX& wnd_class,
                        const CLSMENUNAME<EmulatorTraits<Emu64>>& menu_name)
                : guest_obj_addr(guest_obj),
                  wnd_class(wnd_class),
                  menu_name(menu_name)
            {
            }
        };

        struct dxgk_state
        {
            struct dxgk_allocation
            {
                uint32_t resource_handle{};
                uint64_t backing_memory{};
                uint64_t backing_size{};

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write(this->resource_handle);
                    buffer.write(this->backing_memory);
                    buffer.write(this->backing_size);
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    buffer.read(this->resource_handle);
                    buffer.read(this->backing_memory);
                    buffer.read(this->backing_size);
                }
            };

            struct dxgk_buffer
            {
                uint64_t address{};
                uint32_t size{};

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write(this->address);
                    buffer.write(this->size);
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    buffer.read(this->address);
                    buffer.read(this->size);
                }
            };

            uint32_t next_resource_handle{0x8000};
            uint32_t next_allocation_handle{0x9000};
            std::map<uint32_t, dxgk_allocation> allocations{};

            dxgk_buffer command_buffer{};
            dxgk_buffer allocation_list{};
            dxgk_buffer patch_location_list{};

            static void reserve_buffer(memory_manager& memory, dxgk_buffer& buffer, const uint32_t size)
            {
                if (buffer.address != 0 && buffer.size >= size)
                {
                    return;
                }

                if (buffer.address != 0)
                {
                    memory.release_memory(buffer.address, 0);
                }

                const auto aligned_size = static_cast<uint32_t>(page_align_up(size));

                buffer.address = memory.allocate_memory(aligned_size, memory_permission::read_write);
                buffer.size = aligned_size;

                const std::vector<uint8_t> zeros(aligned_size, 0);
                memory.write_memory(buffer.address, zeros.data(), zeros.size());
            }

            uint32_t create_resource()
            {
                return ++this->next_resource_handle;
            }

            uint32_t create_allocation(memory_manager& memory, const uint32_t resource_handle, uint64_t backing_size)
            {
                if (backing_size == 0)
                {
                    backing_size = 0x1000; // fallback size
                }

                const auto aligned_size = static_cast<uint64_t>(page_align_up(backing_size));
                const auto backing_memory = memory.allocate_memory(static_cast<size_t>(aligned_size), memory_permission::read_write);

                const uint32_t handle = ++this->next_allocation_handle;

                this->allocations[handle] = {
                    .resource_handle = resource_handle,
                    .backing_memory = backing_memory,
                    .backing_size = aligned_size,
                };

                return handle;
            }

            const dxgk_allocation* get_allocation(const uint32_t handle) const
            {
                const auto it = this->allocations.find(handle);
                if (it == this->allocations.end())
                {
                    return nullptr;
                }

                return &it->second;
            }

            bool destroy_allocation(memory_manager& memory, const uint32_t handle)
            {
                const auto it = this->allocations.find(handle);
                if (it == this->allocations.end())
                {
                    return false;
                }

                if (it->second.backing_memory != 0)
                {
                    memory.release_memory(it->second.backing_memory, 0);
                }

                this->allocations.erase(it);
                return true;
            }

            size_t destroy_resource_allocations(memory_manager& memory, const uint32_t resource_handle)
            {
                size_t destroy_count = 0;

                for (auto it = this->allocations.begin(); it != this->allocations.end();)
                {
                    if (it->second.resource_handle != resource_handle)
                    {
                        ++it;
                        continue;
                    }

                    if (it->second.backing_memory != 0)
                    {
                        memory.release_memory(it->second.backing_memory, 0);
                    }

                    it = this->allocations.erase(it);
                    ++destroy_count;
                }

                return destroy_count;
            }

            void serialize(utils::buffer_serializer& buffer) const
            {
                buffer.write(this->next_resource_handle);
                buffer.write(this->next_allocation_handle);
                buffer.write_map(this->allocations);
                buffer.write(this->command_buffer);
                buffer.write(this->allocation_list);
                buffer.write(this->patch_location_list);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                buffer.read(this->next_resource_handle);
                buffer.read(this->next_allocation_handle);
                buffer.read_map(this->allocations);
                buffer.read(this->command_buffer);
                buffer.read(this->allocation_list);
                buffer.read(this->patch_location_list);
            }
        };

        process_context(x86_64_emulator& emu, memory_manager& memory, utils::clock& clock, callbacks& cb)
            : callbacks_(&cb),
              base_allocator(emu),
              peb64(emu),
              process_params64(emu),
              kusd(memory, clock),
              user_handles(memory)
        {
        }

        void setup(windows_emulator& win_emu, const application_settings& app_settings, const mapped_module& executable,
                   const mapped_module& ntdll, const apiset::container& apiset_container, const mapped_module* ntdll32 = nullptr);

        static emulator_pointer allocate_user_class(memory_manager& memory, std::u16string_view class_name);

        handle create_thread(memory_manager& memory, uint64_t start_address, uint64_t argument, uint64_t stack_size, uint32_t create_flags,
                             bool initial_thread = false);
        void terminate_thread(emulator_thread& thread, NTSTATUS thread_exit_status);

        void set_foreground_window(hwnd handle);

        const windows_hook_entry* find_windows_hook(int32_t id_hook, uint32_t thread_id) const;

        std::optional<uint16_t> find_atom(std::u16string_view name);
        uint16_t add_or_find_atom(std::u16string name);
        bool delete_atom(const std::u16string& name);
        bool delete_atom(uint16_t atom_id);
        std::optional<std::u16string> get_atom_name(uint16_t atom_id) const;

        template <typename T>
        void build_knowndlls_section_table(registry_manager& registry, const file_system& file_system, const apiset_map& apiset,
                                           const windows_path& system_root, bool is_32bit);

        std::optional<section> get_knowndll_section_by_name(const std::u16string& name, bool is_32bit) const;
        void add_knowndll_section(const std::u16string& name, const section& section, bool is_32bit);
        bool has_knowndll_section(const std::u16string& name, bool is_32bit) const;

        void serialize(utils::buffer_serializer& buffer, const emulator_thread* active_thread) const;
        void deserialize(utils::buffer_deserializer& buffer, emulator_thread*& active_thread);

        generic_handle_store* get_handle_store(handle handle);
        emulator_thread* find_thread_by_id(uint32_t thread_id);
        const emulator_thread* find_thread_by_id(uint32_t thread_id) const;
        bool is_current_process_handle(handle handle) const;
        bool is_current_thread_handle(handle handle, const emulator_thread* active_thread) const;
        bool is_object_pseudo_handle(handle handle) const;
        handle resolve_object_pseudo_handle(handle handle, const emulator_thread* active_thread) const;

        size_t get_live_thread_count() const;

        // WOW64 support flag - set during process setup based on executable architecture
        bool is_wow64_process{false};

        callbacks* callbacks_{};

        std::vector<uint8_t> sid{};

        uint64_t shared_section_address{0};
        uint64_t shared_section_size{0};
        uint64_t dbwin_buffer{0};
        uint64_t dbwin_buffer_size{0};

        std::optional<NTSTATUS> exit_status{};

        emulator_allocator base_allocator;

        emulator_object<PEB64> peb64;
        emulator_object<RTL_USER_PROCESS_PARAMETERS64> process_params64;
        kusd_mmio kusd;

        uint64_t ntdll_image_base{};
        // nullopt = ensure_nls_lead_byte_info_table (syscall_dispatcher.cpp) hasn't run to completion
        // yet, e.g. because ntdll's own codepage init hasn't happened. Not serialized; reset on
        // deserialize because the patch it gates lives in guest memory and reverts with it.
        std::optional<bool> nls_lead_byte_info_table_resolved{};
        // Guest address of kernelbase.dll's private gNlsProcessLocalCache global, resolved once
        // kernelbase.dll is mapped (see setup()). Real Windows points every thread's TEB.NlsCache at
        // this shared, process-wide fallback structure until that thread does its own locale/NLS API
        // work; BaseNlsThreadCleanup (kernelbase.dll, DLL_THREAD_DETACH) skips freeing TEB.NlsCache
        // whenever it still points here. 0 if kernelbase.dll wasn't found (emulator_thread falls back
        // to a zeroed placeholder in that case - see nls_cache_placeholder's doc comment).
        uint64_t kernelbase_nls_process_local_cache{};
        // Set once kernelbase.dll's real GetUserDefaultLCID export has been run synchronously via
        // guest_function_call.hpp, so its own real code populates kernelbase_nls_process_local_cache
        // the same way real Windows does. Deliberately not serialized and always re-derived to false
        // alongside kernelbase_nls_process_local_cache (module_manager.cpp's
        // ensure_kernelbase_nls_cache_hook) - this only matters again after restoring a snapshot
        // taken before kernelbase.dll loaded, where it must be false to let the real mapping event
        // warm the cache again.
        bool kernelbase_nls_cache_warmed{false};
        // True while the GetUserDefaultLCID call above is in flight. kernelbase.dll's own real code
        // (GetUserDefaultLCID -> an internal per-thread-cache resolver) only actually reaches the
        // registry-backed population path it's being invoked to run if the calling thread's own
        // TEB.NlsCache does not already equal kernelbase_nls_process_local_cache - real Windows
        // threads only ever reach that state after ntdll's own LdrpInitializeProcess has performed a
        // real per-thread heap allocation there, which sogen does not implement; emulator_thread.cpp's
        // placeholder (pointing every thread's TEB.NlsCache at kernelbase_nls_process_local_cache
        // itself, added for a different, narrower null-deref/BaseNlsThreadCleanup-safety reason) makes
        // kernelbase's own code believe every thread has already been through this exactly once,
        // permanently skipping the real population logic. Zeroing TEB.NlsCache for the warming
        // thread right before this call - see try_warm_kernelbase_nls_cache - lets kernelbase's real
        // code take its own genuine allocate-and-populate path instead, exactly as it would for an
        // actually-fresh real Windows thread. If the call completes without kernelbase itself writing
        // a new, non-zero value there (see on_instruction_execution), this flag's own completion
        // handler restores the placeholder, preserving the original null-deref/heap-corruption
        // safety net this mechanism was built for.
        bool kernelbase_nls_cache_warming{false};
        // kernelbase_entry_point/kernelbase_get_user_default_lcid: resolved alongside
        // kernelbase_nls_process_local_cache above. GetUserDefaultLCID must not be invoked until
        // kernelbase.dll's own DllMain (DLL_PROCESS_ATTACH) has actually finished - it depends on
        // internal state DllMain itself sets up (confirmed directly: invoking it right when
        // kernelbase.dll maps, before DllMain runs, reliably crashes inside kernelbase's own code).
        // kernelbase_dllmain_return_address captures the loader's own real return address the first
        // time kernelbase_entry_point is observed to execute (read directly off the stack at that
        // exact moment - see windows_emulator::on_instruction_execution); once execution naturally
        // reaches that address again, DllMain has genuinely completed and it is safe to warm the
        // cache. Also not serialized, for the same reason as kernelbase_nls_cache_warmed.
        uint64_t kernelbase_entry_point{};
        uint64_t kernelbase_get_user_default_lcid{};
        uint64_t kernelbase_dllmain_return_address{};
        uint64_t ldr_initialize_thunk{};
        uint64_t rtl_user_thread_start{};
        uint64_t ki_user_apc_dispatcher{};
        uint64_t ki_user_exception_dispatcher{};
        uint64_t ki_user_callback_dispatcher{};
        uint64_t instrumentation_callback{};
        uint64_t zw_callback_return{};
        uint64_t dispatch_client_message{};
        uint32_t gdi_default_dc_handle{};
        std::map<uint32_t, gdi_dc_state> gdi_dc_states{};
        // Per-DC stack of states pushed by NtGdiSaveDC and popped by NtGdiRestoreDC.
        std::map<uint32_t, std::vector<gdi_dc_state>> gdi_dc_save_states{};
        std::map<uint32_t, gdi_bitmap_surface> gdi_bitmap_surfaces{};
        // Persistent per-top-level-window paint surface; child controls composite into it at their offset.
        std::map<uint32_t, gdi_bitmap_surface> gdi_window_surfaces{};
        dxgk_state dxgk{};
        std::vector<handle> etw_notification_events{};
        hwnd mouse_capture_window{};
        // The window that currently holds keyboard focus / is the foreground window, and the last known
        // cursor position in screen coordinates. Games poll these via GetForegroundWindow/GetActiveWindow
        // and GetCursorPos to drive menu cursors and gate their input loop on the window being active.
        hwnd foreground_window{};
        int32_t cursor_x{};
        int32_t cursor_y{};
        hcursor current_cursor{};
        int32_t cursor_show_count{};
        // Whether the current cursor has a visible shape. SetCursor(NULL) clears it to hide the pointer
        // without touching the show count (some games hide the cursor that way). Defaults to true since a
        // window without an explicit SetCursor still shows its class cursor. The host pointer is shown only
        // when cursor_show_count >= 0 and this is set.
        bool cursor_shape_visible{true};
        // Per-virtual-key pressed state (0x80 = down), updated from key/mouse-button events and reported by
        // GetKeyState; games poll this for in-game input (movement, etc.) rather than window messages.
        std::array<uint8_t, 256> key_state{};
        // Per-virtual-key transition state for GetAsyncKeyState's low bit. A value of 1 means the key or
        // mouse button was pressed since the last GetAsyncKeyState query for that virtual key.
        std::array<uint8_t, 256> async_key_state{};

        // Raw mouse input registration (NtUserRegisterRawInputDevices). When registered, mouse motion
        // synthesizes relative-mouse RAWINPUT delivered as WM_INPUT, so in-game mouse-look works.
        // raw_mouse_target is the explicit hwndTarget; 0 means "follow focus" (deliver to the foreground
        // window), resolved at delivery time so a registration done before any window is foreground still works.
        bool raw_mouse_registered{};
        hwnd raw_mouse_target{};
        // Keyboard raw input registration (HID usage page 0x01, usage 0x06), mirroring the mouse fields above.
        bool raw_keyboard_registered{};
        hwnd raw_keyboard_target{};

        // One pending raw-input payload (mouse motion + buttons, or a keyboard make/break) keyed by the
        // HRAWINPUT token posted in WM_INPUT's lParam; consumed by NtUserGetRawInputData.
        struct raw_input_payload
        {
            bool keyboard{};              // false = mouse, true = keyboard
            int32_t dx{};                 // mouse relative motion
            int32_t dy{};                 //
            uint16_t mouse_buttons{};     // RI_MOUSE_* button transition flags
            uint16_t mouse_button_data{}; // wheel delta for RI_MOUSE_WHEEL/RI_MOUSE_HWHEEL
            uint16_t vkey{};              // keyboard virtual key
            uint16_t scan_code{};         // keyboard scan code
            uint32_t key_message{};       // corresponding WM_KEY* or WM_SYSKEY* message
            bool key_extended{};          // true when the key's scan code has an E0 prefix (lParam bit 24)
        };

        std::map<uint32_t, raw_input_payload> raw_inputs{};
        uint32_t next_raw_input_token{1};

        // For WOW64 processes
        std::optional<emulator_object<PEB32>> peb32;
        std::optional<emulator_object<RTL_USER_PROCESS_PARAMETERS32>> process_params32;
        std::optional<uint64_t> rtl_user_thread_start32{};
        std::optional<uint64_t> wow64_syscall_reentry_addr{};

        user_handle_table user_handles;
        handle default_monitor_handle{};
        handle default_desktop_window_handle{};
        handle_store<handle_types::event, event> events{};
        handle_store<handle_types::job, job_object> jobs{};
        handle_store<handle_types::file, file> files{};
        utils::insensitive_u16string_map<file_lock_ranges> file_locks{};
        handle_store<handle_types::section, section, 2> sections{};
        // Keeps a pagefile-backed section's section_object alive for as long as a guest-visible view of
        // it is mapped, independent of the handle-table entry above (see handle_NtMapViewOfSection /
        // handle_NtUnmapViewOfSection). Real Windows' documented view/handle lifetime independence (MSDN:
        // "the file mapping object may be used until all references to it, including the memory-mapped
        // view, are released") falls out for free from this being an ordinary shared_ptr: the object is
        // only ever actually destroyed (freeing section_object::backing_storage) once neither this map
        // nor any `sections` entry references it anymore. Keyed by the view's own guest VA. Not
        // serialized - runtime-only, like the host aliasing it tracks.
        std::unordered_map<uint64_t, std::shared_ptr<section_object>> section_views{};
        handle_store<handle_types::device, io_device_container> devices{};
        handle console_handle{};
        handle_store<handle_types::semaphore, semaphore> semaphores{};
        handle_store<handle_types::io_completion, io_completion> io_completions{};
        handle_store<handle_types::wait_completion_packet, wait_completion_packet> wait_completion_packets{};
        handle_store<handle_types::worker_factory, worker_factory> worker_factories{};
        handle_store<handle_types::port, port_container> ports{};
        handle_store<handle_types::mutant, mutant> mutants{};
        handle_store<handle_types::private_namespace, private_namespace> private_namespaces{};
        handle default_desktop{};
        handle_store<handle_types::desktop, desktop> desktops{};
        user_handle_store<handle_types::window, window> windows{user_handles};
        user_handle_store<handle_types::type::menu, menu> menus{user_handles};
        handle_store<handle_types::timer, timer> timers{};
        handle_store<handle_types::token, process_token> tokens{};
        user_handle_store<handle_types::accelerator_table, accelerator_table> accelerator_tables{user_handles};
        user_handle_store<handle_types::cursor_icon, cursor_icon> icons{user_handles};
        handle_store<handle_types::registry, registry_key, 2> registry_keys{};
        std::map<uint32_t, handle> thread_handles_by_id{};
        std::map<uint32_t, child_process_record> child_processes{};
        std::map<uint32_t, windows_hook_entry> windows_hooks{};
        uint32_t next_windows_hook_id{0x300};
        // Starts at 2: pseudo process handle id 1 is already taken by STEAM_PROCESS_HANDLE
        // (handles.hpp), and process/thread pseudo handles share this id per record.
        uint32_t next_child_record_id{2};
        std::map<uint16_t, atom_entry> atoms{};
        utils::insensitive_u16string_map<class_entry> classes{};

        apiset_map apiset;
        knowndlls_map knowndlls32_sections;
        knowndlls_map knowndlls64_sections;

        std::vector<std::byte> default_register_set{};

        // Process and thread ids mimic Windows' PspCidTable: a single space of distinct multiples of 4.
        // The process keeps id 4; threads take 8, 12, 16, ... Real Windows never hands out tiny or
        // non-4-aligned ids, and some code (e.g. CEG-style anti-tamper) relies on that.
        static constexpr uint32_t process_id = 4;
        uint32_t spawned_thread_count{0};
        handle_store<handle_types::thread, emulator_thread> threads{};

        // Handles delivered with the most recent ALPC reply message (NtAlpcSendWaitReceivePort). rpcrt4's
        // system-handle import retrieves them via NtAlpcQueryInformationMessage(AlpcMessageHandleInformation)
        // rather than reading the handle attribute directly. Transient (valid only until the next reply).
        std::vector<alpc_reply_handle> pending_alpc_message_handles{};

        // Transient (not serialized) WASAPI render-engine simulation. sogen has no host audio engine draining
        // the shared ring, so dsound opens its stream event-driven (AUDCLNT_STREAMFLAGS_EVENTCALLBACK) and its
        // render thread would block forever on the buffer-ready event a real engine signals every period. We
        // model that engine on the per-context-switch tick: signal the auto-reset events dsound registered via
        // IAudioClient::SetEventHandle so it produces, and advance the shared read cursor at real time so its
        // DirectSound play cursor moves and MSS's "non-moving playback cursor" watchdog stops resetting.
        struct audio_render_stream
        {
            // The render section's own backing buffer, read/written directly (host-side, no guest VA
            // needed) so the tick works whether or not the guest currently has a view mapped. weak_ptr,
            // not shared_ptr: this must not itself keep the section alive - dsound churns stream setup,
            // and a stream whose section_object has genuinely been destroyed (last handle and view both
            // gone) is exactly the "no longer accessible" signal drive_audio_render_engine prunes on.
            std::weak_ptr<section_object> section;
            uint64_t start_time_ns{}; // steady-clock ns anchor for the read cursor (0 = anchor on first tick)
        };

        std::vector<audio_render_stream> audio_render_streams{};
        std::vector<handle> audio_render_events{};

        // Extended parameters from last NtMapViewOfSectionEx call
        // These can be used by other syscalls like NtAllocateVirtualMemoryEx
        uint64_t last_extended_params_numa_node{0};
        uint32_t last_extended_params_attributes{0};
        uint16_t last_extended_params_image_machine{IMAGE_FILE_MACHINE_UNKNOWN};

        uint64_t next_luid{0x1001};
    };

} // namespace sogen
