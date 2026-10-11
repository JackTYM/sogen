#pragma once

#include "../std_include.hpp"

#include <array>
#include <string_view>

namespace sogen::syscalls
{
    struct dxgk_adapter
    {
        uint32_t handle;
        LUID luid;
        uint32_t source_count;
        uint32_t device_id;
        uint32_t adapter_type;
        uint64_t dedicated_video_memory_size;
        GUID guid;
        std::u16string_view description;
    };

    constexpr uint32_t k_dxgk_adapter_vendor_id = 0x1414;
    constexpr uint32_t k_dxgk_adapter_type_render_supported = 0x1;
    constexpr uint32_t k_dxgk_adapter_type_software_device = 0x4;

    // DXGI only treats an adapter as the WARP adapter (DXGI_ADAPTER_FLAG_SOFTWARE, picked by
    // D3D_DRIVER_TYPE_WARP / IDXGIFactory::CreateSoftwareAdapter) when it reports vendor 0x1414, device 0x8C and a
    // render-only adapter type, exactly like the Microsoft Basic Render Driver present on every Windows system.
    constexpr std::array<dxgk_adapter, 2> k_dxgk_adapters{{
        {
            .handle = 0x4000,
            .luid = {.LowPart = 0x1000, .HighPart = 0},
            .source_count = 1,
            .device_id = 0x02C1,
            .adapter_type = k_dxgk_adapter_type_render_supported,
            .dedicated_video_memory_size = 4ull * 1024 * 1024 * 1024,
            .guid = {0x5b45201d, 0xf2f2, 0x4f3b, {0x85, 0xbb, 0x30, 0xff, 0x1f, 0x95, 0x35, 0x99}},
            .description = u"Microsoft RemoteFX Graphics Device",
        },
        {
            .handle = 0x4004,
            .luid = {.LowPart = 0x1001, .HighPart = 0},
            .source_count = 0,
            .device_id = 0x008C,
            .adapter_type = k_dxgk_adapter_type_render_supported | k_dxgk_adapter_type_software_device,
            .dedicated_video_memory_size = 0,
            .guid = {0x5b45201d, 0xf2f2, 0x4f3b, {0x85, 0xbb, 0x30, 0xff, 0x1f, 0x95, 0x35, 0x9a}},
            .description = u"Microsoft Basic Render Driver",
        },
    }};

    constexpr const dxgk_adapter& primary_dxgk_adapter()
    {
        return k_dxgk_adapters.front();
    }

    inline const dxgk_adapter* find_dxgk_adapter_by_handle(const uint32_t handle)
    {
        for (const auto& adapter : k_dxgk_adapters)
        {
            if (adapter.handle == handle)
            {
                return &adapter;
            }
        }

        return nullptr;
    }

    inline const dxgk_adapter* find_dxgk_adapter_by_luid(const LUID& luid)
    {
        for (const auto& adapter : k_dxgk_adapters)
        {
            if (adapter.luid.LowPart == luid.LowPart && adapter.luid.HighPart == luid.HighPart)
            {
                return &adapter;
            }
        }

        return nullptr;
    }
}
