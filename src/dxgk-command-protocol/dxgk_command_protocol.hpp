#pragma once

#include <array>
#include <cstdint>

// Minimal host-native GPU command contract for the sogen native D3DKMT present path.
// The guest sample passes these structs via D3DKMT private-driver-data fields;
// the host gdi.cpp handlers and the vulkan_host backend consume them.

namespace sogen::dxgk_cmd
{
    constexpr uint32_t protocol_magic = 0x4b58'4453u; // 'SDXK'
    constexpr uint32_t version = 1;

    enum class command_type : uint32_t
    {
        clear = 1,
    };

    // Private driver data the guest passes in EMU_D3DDDI_ALLOCATIONINFO::pPrivateDriverData
    // to request a host-side Vulkan render-target image backing for the allocation.
    struct render_target_desc
    {
        uint32_t magic; // must equal protocol_magic
        uint32_t width;
        uint32_t height;
        uint32_t format; // 0 = VK_FORMAT_B8G8R8A8_UNORM (matches ui backend bgra8)
    };

    // Payload placed in EMU_D3DKMT_SUBMITCOMMAND::pPrivateDriverData for a clear operation.
    struct clear_command
    {
        uint32_t magic;             // must equal protocol_magic
        uint32_t type;              // command_type::clear
        uint32_t target_allocation; // allocation handle of the render target to clear
        uint32_t reserved;
        std::array<float, 4> color; // RGBA, each in [0, 1]
    };

} // namespace sogen::dxgk_cmd
