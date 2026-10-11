#include "../std_include.hpp"
#include <array>
#include "../syscall_dispatcher.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

namespace sogen::syscalls
{
#define ACCEPT_DCOMP_SYSCALL(name)                                                                             \
    NTSTATUS handle_##name(const syscall_context&, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) \
    {                                                                                                          \
        return STATUS_SUCCESS;                                                                                 \
    }

    NTSTATUS handle_NtDCompositionCreateChannel(const syscall_context& c, const emulator_object<handle> channel_handle,
                                                const emulator_object<uint64_t> section_size, const emulator_object<uint64_t> section_base)
    {
        constexpr uint64_t command_buffer_size = 0x400000;

        const auto requested_size = section_size.read();
        const auto size = std::max<uint64_t>(requested_size, command_buffer_size);

        const auto base = c.win_emu.memory.allocate_memory(static_cast<size_t>(size), memory_permission::read_write);
        if (!base)
        {
            return STATUS_NO_MEMORY;
        }

        event channel{};
        channel.type = NotificationEvent;
        const auto created = c.proc.events.store(std::move(channel));
        c.proc.dcomp_channels[created.bits].command_buffer = base;
        channel_handle.write(created);
        section_size.write(size);
        section_base.write(base);
        return STATUS_SUCCESS;
    }

    BOOL handle_NtUserCreateDCompositionHwndTarget(const syscall_context& c, const hwnd window, const uint32_t /*topmost*/,
                                                   const emulator_object<handle> target_handle)
    {
        if (!c.proc.windows.get(window))
        {
            return FALSE;
        }

        event target{};
        target.type = NotificationEvent;
        const auto created = c.proc.events.store(std::move(target));
        c.proc.dcomp_hwnd_targets[created.bits] = window;
        target_handle.write(created);
        return TRUE;
    }

    BOOL handle_NtUserDestroyDCompositionHwndTarget(const syscall_context& c, const hwnd /*window*/, const handle target_handle)
    {
        c.proc.dcomp_hwnd_targets.erase(target_handle.bits);
        return TRUE;
    }

    NTSTATUS handle_NtCreateCompositionSurfaceHandle(const syscall_context& c, const uint64_t /*object_attributes*/,
                                                     const ACCESS_MASK /*desired_access*/, const emulator_object<handle> surface_handle)
    {
        event surface{};
        surface.type = NotificationEvent;
        surface_handle.write(c.proc.events.store(std::move(surface)));
        return STATUS_SUCCESS;
    }

    namespace
    {
        // Layout of the commands written by dcomp.dll's CDevice::BeginKernelCommand callers, indexed by opcode (the
        // first dword). Opcodes 11 (handle array), 12 (buffer) and 14 (reference array) carry a 16-byte header followed
        // by a payload whose element count or byte length is the fourth dword. Opcode 0 references a single command
        // stored out of line that is too large for the channel buffer; dcomp counts both.
        constexpr std::array<uint32_t, 21> command_sizes{24, 16, 24, 8, 24, 24, 16, 12, 24, 16, 24, 0, 0, 16, 0, 16, 16, 20, 72, 16, 12};

        constexpr uint32_t out_of_line_command = 0;
        constexpr uint32_t create_resource_command = 1;
        constexpr uint32_t open_shared_resource_command = 2;
        constexpr uint32_t release_resource_command = 3;
        constexpr uint32_t set_handle_property_command = 10;
        constexpr uint32_t set_reference_property_command = 13;
        constexpr uint32_t add_visual_child_command = 17;
        constexpr uint32_t remove_visual_child_command = 20;
        constexpr uint32_t handle_array_command = 11;
        constexpr uint32_t buffer_command = 12;
        constexpr uint32_t reference_array_command = 14;
        constexpr uint32_t variable_command_header_size = 16;

        uint32_t get_command_size(const syscall_context& c, const uint64_t command, const uint32_t opcode)
        {
            const auto payload_length = [&] { return c.emu.read_memory<uint32_t>(command + 12); };

            switch (opcode)
            {
            case handle_array_command:
                return variable_command_header_size + (payload_length() * 8);
            case buffer_command:
                return variable_command_header_size + ((payload_length() + 3) & ~3u);
            case reference_array_command:
                return variable_command_header_size + (payload_length() * 4);
            default:
                return command_sizes[opcode];
            }
        }

        std::shared_ptr<dcomp_resource> find_resource(const dcomp_channel& channel, const uint32_t id)
        {
            const auto entry = channel.resources.find(id);
            return entry == channel.resources.end() ? nullptr : entry->second;
        }

        // Layouts written by dcomp.dll's CDevice::ChannelCreateResource, ChannelOpenSharedResource,
        // ChannelReleaseResource, ResourceSetHandleProperty, ResourceSetReferenceProperty, VisualAddChild and
        // CResorceProxy_RemoveVisual. Only the state needed to find which hwnd target shows a composition surface is
        // kept.
        void apply_command(const syscall_context& c, dcomp_channel& channel, const uint64_t command, const uint32_t opcode)
        {
            const auto read_dword = [&](const uint64_t offset) { return c.emu.read_memory<uint32_t>(command + offset); };
            const auto resource = find_resource(channel, read_dword(4));

            switch (opcode)
            {
            case create_resource_command:
                channel.resources[read_dword(4)] = std::make_shared<dcomp_resource>();
                break;
            case open_shared_resource_command:
                channel.resources[read_dword(4)] =
                    std::make_shared<dcomp_resource>(dcomp_resource{.shared_handle = c.emu.read_memory<uint64_t>(command + 8)});
                break;
            case release_resource_command:
                channel.resources.erase(read_dword(4));
                break;
            case set_handle_property_command:
                if (resource)
                {
                    resource->shared_handle = c.emu.read_memory<uint64_t>(command + 16);
                }
                break;
            case set_reference_property_command:
                if (!resource)
                {
                    break;
                }

                if (auto referenced = find_resource(channel, read_dword(12)))
                {
                    resource->references[read_dword(8)] = std::move(referenced);
                }
                else
                {
                    resource->references.erase(read_dword(8));
                }
                break;
            case add_visual_child_command:
                if (auto child = find_resource(channel, read_dword(8)); resource && child)
                {
                    resource->children.push_back(std::move(child));
                }
                break;
            case remove_visual_child_command:
                if (const auto child = find_resource(channel, read_dword(8)); resource && child)
                {
                    std::erase(resource->children, child);
                }
                break;
            default:
                break;
            }
        }

        uint32_t process_commands(const syscall_context& c, dcomp_channel& channel, const uint32_t batch_size)
        {
            uint32_t count = 0;
            uint32_t offset = 0;
            while (offset < batch_size)
            {
                const auto command = channel.command_buffer + offset;
                const auto opcode = c.emu.read_memory<uint32_t>(command);
                if (opcode >= command_sizes.size())
                {
                    return count + 1;
                }

                apply_command(c, channel, command, opcode);
                offset += get_command_size(c, command, opcode);
                count += opcode == out_of_line_command ? 2 : 1;
            }

            return count;
        }
    }

    NTSTATUS handle_NtDCompositionProcessChannelBatchBuffer(const syscall_context& c, const handle channel_handle,
                                                            const uint32_t batch_size, const emulator_object<uint32_t> commands_processed,
                                                            const emulator_object<uint8_t> flag)
    {
        const auto channel = c.proc.dcomp_channels.find(channel_handle.bits);
        if (channel == c.proc.dcomp_channels.end())
        {
            return STATUS_INVALID_HANDLE;
        }

        commands_processed.write_if_valid(process_commands(c, channel->second, batch_size));
        flag.write_if_valid(0);
        return STATUS_SUCCESS;
    }

    NTSTATUS handle_NtDCompositionDestroyChannel(const syscall_context& c, const handle channel_handle)
    {
        c.proc.dcomp_channels.erase(channel_handle.bits);
        return STATUS_SUCCESS;
    }

    ACCEPT_DCOMP_SYSCALL(NtDCompositionCommitChannel)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionWaitForChannel)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionSetChannelCommitCompletionEvent)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionCreateAndBindSharedSection)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionCreateSharedResourceHandle)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionDuplicateHandleToProcess)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionSynchronize)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionReleaseAllResources)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionGetBatchId)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionGetChannels)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionEnableMMCSS)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionSetChannelConnectionId)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionCreateConnection)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionDestroyConnection)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionConnectPipe)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionBeginFrame)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionConfirmFrame)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionRetireFrame)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionDiscardFrame)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionGetDeletedResources)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionGetFrameSurfaceUpdates)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionGetTargetStatistics)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionCreateSynchronizationObject)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionCommitSynchronizationObject)
    ACCEPT_DCOMP_SYSCALL(NtDCompositionSetChildRootVisual)
    ACCEPT_DCOMP_SYSCALL(NtBindCompositionSurface)
    ACCEPT_DCOMP_SYSCALL(NtUnBindCompositionSurface)
    ACCEPT_DCOMP_SYSCALL(NtNotifyPresentToCompositionSurface)
    ACCEPT_DCOMP_SYSCALL(NtSetCompositionSurfaceBufferUsage)
    ACCEPT_DCOMP_SYSCALL(NtSetCompositionSurfaceDirectFlipState)
    ACCEPT_DCOMP_SYSCALL(NtSetCompositionSurfaceIndependentFlipInfo)
    ACCEPT_DCOMP_SYSCALL(NtSetCompositionSurfaceStatistics)
    ACCEPT_DCOMP_SYSCALL(NtSetCompositionSurfaceAnalogExclusive)
    ACCEPT_DCOMP_SYSCALL(NtQueryCompositionSurfaceBinding)
    ACCEPT_DCOMP_SYSCALL(NtQueryCompositionSurfaceHDRMetaData)
    ACCEPT_DCOMP_SYSCALL(NtQueryCompositionSurfaceRenderingRealization)
    ACCEPT_DCOMP_SYSCALL(NtQueryCompositionSurfaceStatistics)
    ACCEPT_DCOMP_SYSCALL(NtOpenCompositionSurfaceDirtyRegion)
    ACCEPT_DCOMP_SYSCALL(NtOpenCompositionSurfaceSectionInfo)
    ACCEPT_DCOMP_SYSCALL(NtOpenCompositionSurfaceSwapChainHandleInfo)
    ACCEPT_DCOMP_SYSCALL(NtValidateCompositionSurfaceHandle)
    ACCEPT_DCOMP_SYSCALL(NtUserGetDCompositionHwndBitmap)
    ACCEPT_DCOMP_SYSCALL(NtUserGetResizeDCompositionSynchronizationObject)
    ACCEPT_DCOMP_SYSCALL(NtModerncoreCreateDCompositionHwndTarget)
    ACCEPT_DCOMP_SYSCALL(NtModerncoreDestroyDCompositionHwndTarget)
    ACCEPT_DCOMP_SYSCALL(NtModerncoreGetResizeDCompositionSynchronizationObject)
    ACCEPT_DCOMP_SYSCALL(NtGdiDdDDIGetPostCompositionCaps)
}
