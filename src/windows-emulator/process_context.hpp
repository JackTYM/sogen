#pragma once
#include "section_view_manager.hpp"
#include "arch/windows_abi.hpp"

#include "emulator_utils.hpp"
#include "handles.hpp"
#include "registry/registry_manager.hpp"

#include "module/module_manager.hpp"
#include <utils/nt_handle.hpp>

#include <arch_emulator.hpp>
#include <platform/ui_backend.hpp>

#include "io_device.hpp"
#include "kusd_mmio.hpp"
#include "windows_objects.hpp"
#include "emulator_thread.hpp"
#include "port.hpp"
#include "user_handle_table.hpp"

#include "apiset/apiset.hpp"

#include <array>

namespace sogen
{

    struct fake_environment_config;

#define PEB_SEGMENT_SIZE             (20 << 20) // 20 MB
#define GS_SEGMENT_SIZE              (1 << 20)  // 1 MB

#define STACK_SIZE                   0x40000ULL // 256KB

#define GDT_LIMIT                    0x1000
#define GDT_ENTRY_SIZE               0x8

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

    // Every path that presents a window surface goes through this, so none of them can drop the layered-window
    // alpha and have the compositor show a layered window opaque.
    inline ui_surface_desc make_window_surface_desc(const gdi_bitmap_surface& surface, const window* win)
    {
        return ui_surface_desc{
            .width = static_cast<int>(surface.width),
            .height = static_cast<int>(surface.height),
            .stride = static_cast<int>(surface.width * sizeof(uint32_t)),
            .format = ui_surface_format::bgra8,
            .pixels = surface.pixels.data(),
            .layered = win && win->layered_blend,
            .premultiplied_alpha = win && win->layered_premultiplied_alpha,
            .constant_alpha = win ? win->layered_constant_alpha : uint8_t{255},
        };
    }

    struct gdi_bounds_state
    {
        static constexpr uint32_t reset_flag = 0x1;
        static constexpr uint32_t accumulate_flag = 0x2;
        static constexpr uint32_t enable_flag = 0x4;
        static constexpr uint32_t disable_flag = 0x8;

        RECT bounds{};
        bool enabled{};

        bool empty() const
        {
            return this->bounds.left >= this->bounds.right || this->bounds.top >= this->bounds.bottom;
        }

        uint32_t flags() const
        {
            return (this->enabled ? enable_flag : disable_flag) | (this->empty() ? reset_flag : accumulate_flag);
        }

        void update(const uint32_t update_flags, const RECT* const rect)
        {
            if (update_flags & reset_flag)
            {
                this->bounds = {};
            }

            if ((update_flags & accumulate_flag) && rect)
            {
                const RECT normalized{std::min(rect->left, rect->right), std::min(rect->top, rect->bottom),
                                      std::max(rect->left, rect->right), std::max(rect->top, rect->bottom)};

                if (normalized.left < normalized.right && normalized.top < normalized.bottom)
                {
                    if (this->empty())
                    {
                        this->bounds = normalized;
                    }
                    else
                    {
                        this->bounds = {std::min(this->bounds.left, normalized.left), std::min(this->bounds.top, normalized.top),
                                        std::max(this->bounds.right, normalized.right), std::max(this->bounds.bottom, normalized.bottom)};
                    }
                }
            }

            if (update_flags & enable_flag)
            {
                this->enabled = true;
            }

            if (update_flags & disable_flag)
            {
                this->enabled = false;
            }
        }

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->bounds);
            buffer.write(this->enabled);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->bounds);
            buffer.read(this->enabled);
        }
    };

    struct gdi_dc_state
    {
        uint32_t selected_bitmap{};
        uint32_t selected_font{};
        hwnd target_window{};
        int32_t current_x{};
        int32_t current_y{};
        bool is_memory_dc{};
        uint32_t map_mode{1}; // MM_TEXT, the real GDI default for a freshly created DC
        // Retrieved with GetDC and friends, which hand out the desktop window's DC for no window.
        bool is_window_dc{};
        gdi_bounds_state bounds{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->selected_bitmap);
            buffer.write(this->selected_font);
            buffer.write(this->target_window);
            buffer.write(this->current_x);
            buffer.write(this->current_y);
            buffer.write(this->is_memory_dc);
            buffer.write(this->map_mode);
            buffer.write(this->is_window_dc);
            this->bounds.serialize(buffer);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->selected_bitmap);
            buffer.read(this->selected_font);
            buffer.read(this->target_window);
            buffer.read(this->current_x);
            buffer.read(this->current_y);
            buffer.read(this->is_memory_dc);
            buffer.read(this->map_mode);
            buffer.read(this->is_window_dc);
            this->bounds.deserialize(buffer);
        }
    };

    // A resource of a DirectComposition channel (visual, surface proxy, hwnd target, ...). Handles released by
    // the channel stay alive while another resource still references them, as in the compositor.
    struct dcomp_resource
    {
        uint64_t shared_handle{};
        std::vector<std::shared_ptr<dcomp_resource>> children{};
        std::map<uint32_t, std::shared_ptr<dcomp_resource>> references{};
    };

    struct dcomp_channel
    {
        uint64_t command_buffer{};
        std::unordered_map<uint32_t, std::shared_ptr<dcomp_resource>> resources{};
    };

    struct pending_pipe_connection
    {
        std::u16string name{};
        uint64_t connection_id{};
        std::optional<uint32_t> client_process_id{};
        std::deque<std::string> client_bytes{};
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
        // process (see windows_emulator's create_child_process callback) - exit_status starts as
        // STATUS_PENDING and stays that way until either a successful remote NtTerminateProcess
        // (handle_NtTerminateProcess, process.cpp) updates it to the requested exit code, or the child
        // reports its own exit over its process_control_channel (see windows_emulator::
        // notify_own_exit/pump_child_exit_notifications) once it exits or crashes on its own. Either
        // path is what waiting on (NtWaitForSingleObject/NtAssociateWaitCompletionPacket,
        // validate_wait_handle in object.cpp and is_wait_completion_target_signaled in
        // io_completion_wait.cpp) or querying (NtQueryInformationProcess's ProcessBasicInformation
        // branch, also process.cpp) the minted process handle for such a child observes. granted_access
        // is the caller's process_desired_access from NtCreateUserProcess (see
        // handle_NtCreateUserProcess), checked by resolve_child_target/resolve_child_record
        // (cross_process.hpp) against the required right for a given cross-process operation.
        struct child_process_record
        {
            windows_path image_path{};
            uint32_t pid = 0;
            NTSTATUS exit_status = STATUS_SUCCESS;
            ACCESS_MASK granted_access{};

            void serialize(utils::buffer_serializer& buffer) const
            {
                buffer.write(this->image_path);
                buffer.write(this->pid);
                buffer.write(this->exit_status);
                buffer.write(this->granted_access);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                buffer.read(this->image_path);
                buffer.read(this->pid);
                buffer.read(this->exit_status);
                buffer.read(this->granted_access);
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
            bool ansi_proc{};

            class_entry() = default;

            class_entry(const emulator_pointer guest_obj, const EMU_WNDCLASSEX& wnd_class,
                        const CLSMENUNAME<EmulatorTraits<Emu64>>& menu_name, const bool ansi_proc = false)
                : guest_obj_addr(guest_obj),
                  wnd_class(wnd_class),
                  menu_name(menu_name),
                  ansi_proc(ansi_proc)
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
                uint32_t width{};
                uint32_t height{};
                uint32_t pitch{};

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write(this->resource_handle);
                    buffer.write(this->backing_memory);
                    buffer.write(this->backing_size);
                    buffer.write(this->width);
                    buffer.write(this->height);
                    buffer.write(this->pitch);
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    buffer.read(this->resource_handle);
                    buffer.read(this->backing_memory);
                    buffer.read(this->backing_size);
                    buffer.read(this->width);
                    buffer.read(this->height);
                    buffer.read(this->pitch);
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

            struct dxgk_sync_object
            {
                uint32_t type{};
                uint64_t fence_address{};

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write(this->type);
                    buffer.write(this->fence_address);
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    buffer.read(this->type);
                    buffer.read(this->fence_address);
                }
            };

            struct dxgk_fence_event_wait
            {
                std::vector<uint32_t> sync_objects{};
                std::vector<uint64_t> fence_values{};
                bool wait_any{};
                uint64_t event{};

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write_vector(this->sync_objects);
                    buffer.write_vector(this->fence_values);
                    buffer.write(this->wait_any);
                    buffer.write(this->event);
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    buffer.read_vector(this->sync_objects);
                    buffer.read_vector(this->fence_values);
                    buffer.read(this->wait_any);
                    buffer.read(this->event);
                }
            };

            uint32_t next_resource_handle{0x8000};
            uint32_t next_allocation_handle{0x9000};
            uint32_t next_sync_object_handle{0x7100};
            std::map<uint32_t, dxgk_allocation> allocations{};
            std::map<uint32_t, dxgk_sync_object> sync_objects{};
            std::vector<dxgk_fence_event_wait> fence_event_waits{};

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

            uint32_t create_allocation(memory_manager& memory, const uint32_t resource_handle, uint64_t backing_size,
                                       const uint32_t width = 0, const uint32_t height = 0, const uint32_t pitch = 0)
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
                    .width = width,
                    .height = height,
                    .pitch = pitch,
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
                buffer.write(this->next_sync_object_handle);
                buffer.write_map(this->allocations);
                buffer.write_map(this->sync_objects);
                buffer.write_vector(this->fence_event_waits);
                buffer.write(this->command_buffer);
                buffer.write(this->allocation_list);
                buffer.write(this->patch_location_list);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                buffer.read(this->next_resource_handle);
                buffer.read(this->next_allocation_handle);
                buffer.read(this->next_sync_object_handle);
                buffer.read_map(this->allocations);
                buffer.read_map(this->sync_objects);
                buffer.read_vector(this->fence_event_waits);
                buffer.read(this->command_buffer);
                buffer.read(this->allocation_list);
                buffer.read(this->patch_location_list);
            }
        };

        process_context(memory_interface& emu, memory_manager& memory, utils::clock& clock, callbacks& cb, const bool intercept_kusd,
                        const windows_guest_architecture architecture = windows_guest_architecture::x86_64)
            : callbacks_(&cb),
              clock_(&clock),
              base_allocator(emu),
              peb64(emu),
              process_params64(emu),
              kusd(memory, clock, intercept_kusd),
              user_handles(memory)
        {
            (void)windows_abi_for(architecture);
            this->native_architecture = architecture;
            emulator_process current_process{};
            current_process.id = process_id;
            [[maybe_unused]] const auto process_handle = this->processes.store(std::move(current_process));
            assert(process_handle == GUEST_PROCESS_HANDLE);
        }

        void setup(windows_emulator& win_emu, const application_settings& app_settings, const mapped_module& executable,
                   const mapped_module& ntdll, const apiset::container& apiset_container, const mapped_module* ntdll32 = nullptr);

        static emulator_pointer allocate_user_class(memory_manager& memory, std::u16string_view class_name);

        handle create_thread(memory_manager& memory, uint64_t start_address, uint64_t argument, uint64_t stack_size, uint32_t create_flags,
                             bool initial_thread = false);
        void terminate_thread(emulator_thread& thread, NTSTATUS thread_exit_status);

        void update_message_key_state(const msg& message);
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

        std::optional<hwnd> find_composition_surface_window(uint64_t surface_handle) const;

        void serialize(utils::buffer_serializer& buffer, const emulator_thread* active_thread) const;
        void deserialize(utils::buffer_deserializer& buffer, emulator_thread*& active_thread);
        void prepare_for_state_restore(windows_emulator& win_emu);
        void restore_after_state_restore(windows_emulator& win_emu);
        void restore_windows_after_state_restore(windows_emulator& win_emu);

        generic_handle_store* get_handle_store(handle handle);
        emulator_thread* find_thread_by_id(uint32_t thread_id);
        const emulator_thread* find_thread_by_id(uint32_t thread_id) const;
        bool is_window_effectively_visible(hwnd window) const;
        bool is_current_process_handle(handle handle) const;
        handle get_process_handle_by_pid(uint32_t pid, ACCESS_MASK granted_access);
        bool is_current_thread_handle(handle handle, const emulator_thread* active_thread) const;
        bool is_object_pseudo_handle(handle handle) const;
        handle resolve_object_pseudo_handle(handle handle, const emulator_thread* active_thread) const;

        size_t get_live_thread_count() const;

        // WOW64 support flag - set during process setup based on executable architecture
        bool is_wow64_process{false};

        bool break_on_termination{false};

        uint32_t default_locale_id{};
        // Each vCPU gets its own GDT page. Most descriptors are identical, but the WOW64 FS descriptor
        // (selector 0x53) holds a per-thread 32-bit TEB base that the guest reloads on every 64<->32
        // transition, so a shared GDT would let a WOW64 thread on one vCPU read another vCPU's TEB base.
        uint64_t gdt_base{};

        windows_guest_architecture native_architecture{windows_guest_architecture::x86_64};

        const windows_arch_abi& abi() const
        {
            return windows_abi_for(this->native_architecture);
        }

        uint32_t hard_error_mode{0};

        callbacks* callbacks_{};
        utils::clock* clock_{};

        std::chrono::steady_clock::time_point steady_now() const
        {
            return this->clock_->steady_now();
        }

        std::vector<uint8_t> sid{};
        std::vector<token_privilege> privileges{};
        LUID authentication_id{};

        // Windows draws a token's ModifiedId from the system-wide LUID counter, and combase keys its DCOM
        // access-check cache by it, so a zero or shared id hits stale cache slots. Each emulated process owns
        // one token that is never modified, and process ids are unique across emulated processes.
        static LUID token_modified_id(const uint32_t pid)
        {
            return LUID{.LowPart = pid, .HighPart = 1};
        }

        uint64_t shared_section_address{0};
        uint64_t shared_section_size{0};
        uint64_t dbwin_buffer{0};
        uint64_t dbwin_buffer_size{0};
        std::map<uint64_t, uint32_t> orphaned_section_backings{};
        section_view_manager section_views{};

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
        // Guest address currently holding a temporary 0xCC byte, used by
        // windows_emulator::try_warm_kernelbase_nls_cache_breakpoint to reach the same three points
        // (DllMain entry, DllMain return, GetUserDefaultLCID-call completion) as
        // try_warm_kernelbase_nls_cache above, but via a real guest int3 instead of an
        // instruction-precision host hook: uses_instruction_precision() is only ever true for
        // Unicorn (see supports_instruction_counting) - FEX/KVM/WHP run guest code natively and
        // expose no per-instruction or per-address execution hook at all (their hook_memory_execution
        // overloads are no-ops, kept only for API compatibility), so on those backends
        // try_warm_kernelbase_nls_cache's own host hook never fires and 5b3fd9d5's warm-up never
        // runs. A software breakpoint uses only real x86 semantics (STATUS_BREAKPOINT delivery,
        // already proven to work identically on every backend) instead. 0 = no patch currently
        // installed. Not serialized: host-side bookkeeping for an in-flight guest-code patch,
        // meaningless across a snapshot boundary.
        uint64_t kernelbase_nls_cache_breakpoint_address{};
        std::byte kernelbase_nls_cache_breakpoint_original_byte{};
        // Guest address of ntdll64's exported RtlQueryPerformanceCounter, permanently patched with
        // 0xCC once resolved (see windows_emulator::arm_rtl_query_performance_counter_trap). Its
        // real user-mode "bypass" fast path reads several KUSER_SHARED_DATA fields at their raw,
        // unrebased address - always unbacked by real memory on a Mach-O host, see
        // fex_x86_64_emulator's mmio_region doc comment - before ever reaching an RDTSCP this
        // backend doesn't support either (see that backend's SupportsCPUIndexInTPIDRRO comment), so
        // every call was paying multiple guaranteed hardware faults. Unlike
        // kernelbase_nls_cache_breakpoint_address, this one is never disarmed: the function is
        // called repeatedly for the whole process lifetime, and every hit is answered directly
        // from kusd_mmio's own clock (windows_emulator::try_service_rtl_query_performance_counter)
        // instead of ever letting the real body run. 0 = not armed. Not serialized, same reasoning
        // as kernelbase_nls_cache_breakpoint_address - re-armed on module load, and FEX snapshots
        // don't persist guest memory contents yet anyway (see that backend's serialize_state).
        uint64_t rtl_query_performance_counter_trap{};
        uint64_t ldr_initialize_thunk{};
        uint64_t rtl_user_thread_start{};
        uint64_t ki_user_apc_dispatcher{};
        uint64_t ki_user_exception_dispatcher{};
        uint64_t ki_user_callback_dispatcher{};
        uint64_t instrumentation_callback{};
        uint64_t zw_callback_return{};
        uint64_t dispatch_client_message{};
        uint32_t gdi_default_dc_handle{};
        uint32_t gdi_memory_dc_default_bitmap_handle{};
        uint32_t gdi_objects_peak{};
        uint32_t user_objects_peak{};
        std::map<uint32_t, gdi_dc_state> gdi_dc_states{};
        // Per-DC stack of states pushed by NtGdiSaveDC and popped by NtGdiRestoreDC.
        std::map<uint32_t, std::vector<gdi_dc_state>> gdi_dc_save_states{};
        std::map<uint32_t, gdi_bitmap_surface> gdi_bitmap_surfaces{};
        std::map<uint32_t, EMU_LOGFONTW> gdi_fonts{};
        // Rectangles (YX-banded, non-overlapping) of every complex region; simple and null regions live in RGNATTR only.
        std::map<uint32_t, std::vector<RECT>> gdi_regions{};
        // Persistent per-top-level-window paint surface; child controls composite into it at their offset.
        std::map<uint32_t, gdi_bitmap_surface> gdi_window_surfaces{};
        dxgk_state dxgk{};
        std::vector<handle> etw_notification_events{};
        hwnd mouse_capture_window{};
        std::optional<ULONG> timer_resolution_request{};
        // The window that currently holds keyboard focus / is the foreground window, and the last known
        // cursor position in screen coordinates. Games poll these via GetForegroundWindow/GetActiveWindow
        // and GetCursorPos to drive menu cursors and gate their input loop on the window being active.
        hwnd foreground_window{};
        // Keyboard focus of the input queue. A cross-process child window (e.g. an embedded browser) shares
        // the queue of the window it is attached to, so this can be a window owned by another process.
        hwnd focus_window{};
        int32_t cursor_x{};
        int32_t cursor_y{};
        hcursor current_cursor{};
        // The last position set via SetCaretPos, in the caret owner's client coordinates. GetCaretPos reads
        // this back; real Win32 semantics report it relative to whichever window currently owns the caret,
        // but sogen does not track caret ownership per-window, so this is process-wide like cursor_x/cursor_y.
        int32_t caret_x{};
        int32_t caret_y{};
        int32_t cursor_show_count{};
        // Whether the current cursor has a visible shape. SetCursor(NULL) clears it to hide the pointer
        // without touching the show count (some games hide the cursor that way). Defaults to true since a
        // window without an explicit SetCursor still shows its class cursor. The host pointer is shown only
        // when cursor_show_count >= 0 and this is set.
        bool cursor_shape_visible{true};
        std::optional<RECT> cursor_clip{};
        // Per-virtual-key pressed state (0x80 = down), updated from key/mouse-button events and reported by
        // GetKeyState; games poll this for in-game input (movement, etc.) rather than window messages.
        std::array<uint8_t, 256> key_state{};
        // Per-virtual-key transition state for GetAsyncKeyState's low bit. A value of 1 means the key or
        // mouse button was pressed since the last GetAsyncKeyState query for that virtual key.
        std::array<uint8_t, 256> async_key_state{};
        // Key state as of the keyboard messages already retrieved from the queue (Windows' synchronous key
        // state). TranslateMessage reads it: key_state may already reflect key-ups that are still queued.
        std::array<uint8_t, 256> message_key_state{};

        // Raw mouse input registration (NtUserRegisterRawInputDevices). When registered, mouse motion
        // synthesizes relative-mouse RAWINPUT delivered as WM_INPUT, so in-game mouse-look works.
        // raw_mouse_target is the explicit hwndTarget; 0 means "follow focus" (deliver to the foreground
        // window), resolved at delivery time so a registration done before any window is foreground still works.
        bool raw_mouse_registered{};
        hwnd raw_mouse_target{};
        // Keyboard raw input registration (HID usage page 0x01, usage 0x06), mirroring the mouse fields above.
        bool raw_keyboard_registered{};
        hwnd raw_keyboard_target{};

        // Keyboard and mouse input held back until every low-level hook it is due for has seen it, in arrival order.
        struct low_level_input
        {
            uint64_t id{};
            ui_event event{};
            int32_t screen_x{};
            int32_t screen_y{};
            DWORD time{};
            std::vector<uint64_t> hooks{};
            utils::clock::steady_time_point waiting_since{};
            bool in_progress{};

            void serialize(utils::buffer_serializer& buffer) const
            {
                buffer.write(this->id);
                buffer.write(this->event.window);
                buffer.write(this->event.message);
                buffer.write(this->event.wParam);
                buffer.write(this->event.lParam);
                buffer.write(this->event.relative_motion.has_value());
                if (this->event.relative_motion)
                {
                    buffer.write(this->event.relative_motion->first);
                    buffer.write(this->event.relative_motion->second);
                }
                buffer.write(this->event.cursor_target.has_value());
                if (this->event.cursor_target)
                {
                    buffer.write(this->event.cursor_target->first);
                    buffer.write(this->event.cursor_target->second);
                }
                buffer.write(this->event.injected);
                buffer.write(this->event.extra_info);
                buffer.write(this->event.injected_character);
                buffer.write(this->screen_x);
                buffer.write(this->screen_y);
                buffer.write(this->time);
                buffer.write_vector(this->hooks);
                buffer.write(this->waiting_since.time_since_epoch().count());
                buffer.write(this->in_progress);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                buffer.read(this->id);
                buffer.read(this->event.window);
                buffer.read(this->event.message);
                buffer.read(this->event.wParam);
                buffer.read(this->event.lParam);
                bool has_relative_motion{};
                buffer.read(has_relative_motion);
                if (has_relative_motion)
                {
                    std::pair<int32_t, int32_t> relative_motion{};
                    buffer.read(relative_motion.first);
                    buffer.read(relative_motion.second);
                    this->event.relative_motion = relative_motion;
                }
                else
                {
                    this->event.relative_motion.reset();
                }
                bool has_cursor_target{};
                buffer.read(has_cursor_target);
                if (has_cursor_target)
                {
                    std::pair<int32_t, int32_t> cursor_target{};
                    buffer.read(cursor_target.first);
                    buffer.read(cursor_target.second);
                    this->event.cursor_target = cursor_target;
                }
                else
                {
                    this->event.cursor_target.reset();
                }
                buffer.read(this->event.injected);
                buffer.read(this->event.extra_info);
                buffer.read(this->event.injected_character);
                buffer.read(this->screen_x);
                buffer.read(this->screen_y);
                buffer.read(this->time);
                buffer.read_vector(this->hooks);
                utils::clock::steady_duration::rep waiting_since_ticks{};
                buffer.read(waiting_since_ticks);
                this->waiting_since = utils::clock::steady_time_point{utils::clock::steady_duration{waiting_since_ticks}};
                buffer.read(this->in_progress);
            }
        };

        std::vector<low_level_input> pending_low_level_input{};
        uint64_t next_low_level_input_id{1};
        uint64_t next_hook_handle{1};
        // The host sends the character of a key as its own event, so it has to be dropped with a swallowed key press.
        bool drop_low_level_characters{};

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

        static constexpr uint32_t default_display_width = 1920;
        static constexpr uint32_t default_display_height = 1080;
        uint32_t display_width{default_display_width};
        uint32_t display_height{default_display_height};
        uint32_t registry_display_width{default_display_width};
        uint32_t registry_display_height{default_display_height};

        void set_display_mode(windows_emulator& win_emu, uint32_t width, uint32_t height);
        void set_mouse_capture(hwnd window);
        void release_mouse_capture(hwnd window);
        handle_store<handle_types::event, event> events{};
        handle_store<handle_types::job, job_object> jobs{};
        job_limits assigned_job_limits{};
        handle_store<handle_types::file, file> files{};
        utils::insensitive_u16string_map<file_lock_ranges> file_locks{};
        handle_store<handle_types::section, section> sections{};
        handle_store<handle_types::device, io_device_container> devices{};
        handle console_handle{};
        handle_store<handle_types::semaphore, semaphore> semaphores{};
        handle_store<handle_types::io_completion, io_completion> io_completions{};
        handle_store<handle_types::wait_completion_packet, wait_completion_packet> wait_completion_packets{};
        handle_store<handle_types::worker_factory, worker_factory> worker_factories{};
        handle_store<handle_types::thread_state_change, thread_state_change> thread_state_changes{};
        handle_store<handle_types::port, port_container> ports{};
        alpc::state alpc{};
        handle_store<handle_types::mutant, mutant> mutants{};
        handle_store<handle_types::private_namespace, private_namespace> private_namespaces{};
        handle default_desktop{};
        handle_store<handle_types::desktop, desktop> desktops{};
        handle_store<handle_types::window_station, window_station> window_stations{};
        // WinSta0, the interactive window station every process starts in, is the fixed handle 1.
        uint64_t process_window_station{1};
        user_handle_store<handle_types::window, window> windows{user_handles};
        user_handle_store<handle_types::type::menu, menu> menus{user_handles};
        handle_store<handle_types::timer, timer> timers{};
        std::vector<std::unique_ptr<timer>> orphaned_timers{};

        void queue_due_timer_apcs(utils::clock& clock);
        void drop_timer_apcs(uint64_t timer_source);
        void retain_armed_timer_after_close(handle timer_handle);

        // Client connections to a pipe name this process serves while every server instance of that name is busy;
        // the next instance to listen takes the oldest one, with any bytes the client already wrote.
        std::deque<pending_pipe_connection> pending_pipe_connections{};
        uint32_t next_pipe_connection_serial{};

        std::map<uint64_t, dcomp_channel> dcomp_channels{};
        std::map<uint64_t, hwnd> dcomp_hwnd_targets{};
        // Windows owned by a child host process that were reparented under one of this process's windows,
        // mapped to that parent. Pointer input hitting the parent is offered to them (see window_control).
        std::map<hwnd, hwnd> foreign_child_windows{};

        struct foreign_window_owner
        {
            uint32_t process_id{};
            uint32_t thread_id{};
        };

        // Windows of the other processes of the tree (announced via pipe_ipc_message_type::window_created), so
        // queries about them can be answered here the way the shared win32k state answers them on Windows.
        std::map<hwnd, foreign_window_owner> foreign_windows{};
        handle_store<handle_types::token, process_token> tokens{};
        user_handle_store<handle_types::accelerator_table, accelerator_table> accelerator_tables{user_handles};
        user_handle_store<handle_types::cursor_icon, cursor_icon> icons{user_handles};
        handle_store<handle_types::deferred_window_positions, deferred_window_positions> deferred_window_position_batches{};
        handle_store<handle_types::registry, registry_key> registry_keys{};
        handle_store<handle_types::process, emulator_process> processes{};
        handle_store<handle_types::managed_thread, managed_process_thread> managed_threads{};
        handle_store<handle_types::debug_object, debug_object> debug_objects{};
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

        // Process and thread ids share Windows' 4-aligned client ID space.
        uint32_t process_id{default_guest_process_id};
        uint32_t initial_thread_id{8};
        uint32_t spawned_thread_count{0};

        // The kernel-object arena shared with every other sogen host process of this process tree (see
        // kernel_state.hpp). Absent until the first object is shared or a child is spawned; children
        // receive their parent's arena at bootstrap. Not serialized: it is host-side plumbing.
        std::shared_ptr<kernel_arena> shared_arena{};

        kernel_arena* ensure_shared_arena()
        {
            if (!this->shared_arena)
            {
                this->shared_arena = kernel_arena::create();
                this->user_handles.share_indices(this->shared_arena);
            }

            return this->shared_arena.get();
        }

        handle_store<handle_types::thread, emulator_thread> threads{};

        // Handles delivered with the most recent ALPC reply message (NtAlpcSendWaitReceivePort). rpcrt4's
        // system-handle import retrieves them via NtAlpcQueryInformationMessage(AlpcMessageHandleInformation)
        // rather than reading the handle attribute directly. Transient (valid only until the next reply).
        std::vector<alpc_reply_handle> pending_alpc_message_handles{};
        std::map<uint32_t, std::vector<std::array<uint64_t, 2>>> pending_alpc_reply_views{};

        // Guest events WASAPI EVENTCALLBACK clients registered via SetEventHandle, keyed by the stream whose endpoint
        // port received them. The audio service hands each to its stream when the stream starts.
        std::map<uint32_t, uint64_t> audio_render_events_by_stream{};

        // Events registered through an unnamed endpoint port, keyed by the registering thread instead, which also
        // starts the stream. An entry whose stream is never started stays until that thread registers again.
        std::map<uint32_t, uint64_t> pending_audio_render_events{};

        // Audio streams are numbered per process, so an endpoint port name identifies one stream across every
        // audio-service connection.
        uint32_t next_audio_stream_id{1};

        // Extended parameters from last NtMapViewOfSectionEx call
        // These can be used by other syscalls like NtAllocateVirtualMemoryEx
        uint64_t last_extended_params_numa_node{0};
        uint32_t last_extended_params_attributes{0};
        uint16_t last_extended_params_image_machine{IMAGE_FILE_MACHINE_UNKNOWN};

        uint64_t next_luid{0x1001};
        std::array<uint8_t, 6> uuid_seed{};
        uint64_t next_uuid_time{};
        uint32_t uuid_sequence{};
    };

} // namespace sogen
