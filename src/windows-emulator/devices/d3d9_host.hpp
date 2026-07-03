#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sogen
{
    // Host-side D3D9 DDI decoder. Owned by gpu_command_processor (gpu_bridge.cpp), which forwards the
    // D3D9 opcode block (gpu_bridge::command's 0x900 range, see d3d9-command-protocol/
    // d3d9_command_protocol.hpp) here after reading guest memory into plain buffers.
    //
    // This interface is deliberately free of emulator/guest-Windows types, same rule as vulkan_host:
    // Part 3/4 will build real GPU resources and draw here, sharing the vulkan_host already created
    // for the device (see handle_NtGdiDdDDICreateDevice), and mixing emulated-Windows definitions
    // into that path is exactly what vulkan_host.hpp's own comment warns against.
    //
    // Current scope (Part 2 -- transport bring-up): resource lifetime and per-device render-state
    // tracking, enough to prove the D3D9 UMD <-> host transport end-to-end. Building a pipeline key
    // from that state and actually drawing against vulkan_host is Part 3; shader translation via
    // vkd3d-shader is Part 4.
    class d3d9_host
    {
      public:
        d3d9_host() = default;

        // ---------------------------------------------------------------------------------------
        // Sync commands
        // ---------------------------------------------------------------------------------------

        int32_t create_resource(uint32_t kind, uint32_t format, uint32_t width, uint32_t height, uint32_t depth,
                                uint32_t mip_levels, uint32_t usage, uint32_t pool, uint64_t& out_resource);
        void destroy_resource(uint64_t resource);

        // Copies up to out_capacity bytes of the resource's host-side shadow copy into out.
        // out_data_size always receives the true backing-store size.
        int32_t lock(uint64_t resource, uint32_t subresource, uint32_t offset, uint32_t size, uint32_t flags, void* out,
                    size_t out_capacity, uint32_t& out_data_size);
        int32_t unlock(uint64_t resource, uint32_t subresource, uint32_t offset, const void* data, size_t data_size);

        int32_t create_vertex_shader(const void* tokens, size_t token_size_bytes, uint64_t& out_shader);
        int32_t create_pixel_shader(const void* tokens, size_t token_size_bytes, uint64_t& out_shader);

        // element_size_bytes is d3d9_cmd::vertex_element's size (8); elements points at element_count
        // contiguous entries of that shape.
        int32_t create_vertex_decl(const void* elements, size_t element_count, size_t element_size_bytes, uint64_t& out_decl);

        // ---------------------------------------------------------------------------------------
        // Streamed (recorded) commands
        // ---------------------------------------------------------------------------------------

        // Dispatches one recorded D3D9 opcode (a gpu_bridge::command d3d9_* value) with its raw
        // payload bytes. Returns 0 on success or a negative D3DERR-shaped code, matching
        // execute_recorded_command's own int32_t convention.
        int32_t execute_recorded(uint32_t opcode, const std::byte* payload, size_t size);

      private:
        struct resource_entry
        {
            uint32_t kind;
            uint32_t format;
            uint32_t width;
            uint32_t height;
            uint32_t depth;
            uint32_t mip_levels;
            uint32_t usage;
            uint32_t pool;
            std::vector<std::byte> backing; // host-side shadow copy; real GPU backing lands in Part 3
        };

        struct shader_entry
        {
            std::vector<uint32_t> tokens;
        };

        struct vertex_decl_entry
        {
            std::vector<std::byte> elements; // raw d3d9_cmd::vertex_element entries
        };

        // Per-device fixed-function/DDI state, tracked but not yet consumed by a pipeline builder.
        struct device_state
        {
            std::unordered_map<uint32_t, uint32_t> render_state{};
            std::unordered_map<uint64_t, uint32_t> texture_stage_state{};  // key = (stage << 32) | state
            std::unordered_map<uint64_t, uint32_t> sampler_state{};        // key = (sampler << 32) | state
            std::unordered_map<uint32_t, uint64_t> bound_textures{};       // key = stage
            std::unordered_map<uint32_t, uint64_t> stream_sources{};       // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_frequencies{};   // key = stream_number
            uint64_t index_buffer{};
            uint32_t index_format{};
            uint64_t vertex_decl{};
            uint64_t vertex_shader{};
            uint64_t pixel_shader{};
            std::vector<float> vs_const_f{};
            std::vector<float> ps_const_f{};
            std::array<uint64_t, 4> render_targets{};
            uint64_t depth_stencil{};
        };

        uint64_t next_id_{1};
        std::unordered_map<uint64_t, resource_entry> resources_{};
        std::unordered_map<uint64_t, shader_entry> shaders_{};
        std::unordered_map<uint64_t, vertex_decl_entry> vertex_decls_{};
        device_state state_{};

        uint64_t allocate_id();
    };
} // namespace sogen
