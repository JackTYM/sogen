#pragma once

#include "d3d9_shader_translator.hpp"
#include "vulkan_host.hpp"

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
    // mixing emulated-Windows definitions into this path is exactly what vulkan_host.hpp's own comment
    // warns against.
    //
    // Render-target/depth-stencil-kind resources get real GPU backing (a lazily-created Vulkan
    // instance/device on the injected vulkan_host, then vulkan_host::create_render_target) so pfnClear
    // can do a real clear+readback into the resource's host-side shadow copy -- which is what
    // pfnLock/pfnUnlock (already wired) hand back to the app. This deliberately sidesteps needing to
    // know how the real d3d9.dll gets pixels onto an actual window (RE'd to be genuinely opaque to the
    // driver -- D3DDDIARG_PRESENT carries no HWND): whatever mechanism the runtime uses to display the
    // backbuffer, it goes through Lock to read pixels back from the driver first.
    //
    // Part 3 (the real draw path -- ensure_draw_infra/ensure_pipeline/execute_draw) is implemented: a
    // single hardcoded fixed-function shader pair (D3DFVF_XYZRHW|D3DFVF_DIFFUSE), a cached pipeline,
    // real vertex buffer upload, dynamic rendering with explicit layout barriers, and readback into the
    // render target's backing store, mirroring pfnClear's own pattern. Verified end-to-end via the
    // guest D3D9 API -- DrawPrimitive()/Present() both succeed, pixel readback matches expected.
    //
    // Part 4 (programmable shaders) is also implemented: ensure_programmable_pipeline lazily
    // translates a bound vertex+pixel shader pair via vkd3d-shader (d3d9_shader_translator.hpp) into a
    // second, cached Vulkan pipeline, selected by execute_draw whenever both shaders are bound.
    // Verified end-to-end with real D3DCompile()-produced SM2 bytecode -- see HANDOFF_MACBOOK.md §15.
    //
    // M2 (textures, samplers, indexed draws, real depth/blend) is also implemented: plain sampled
    // texture_2d resources get real GPU backing with lazy staging upload (ensure_texture_uploaded), a
    // real vulkan_host::create_sampler is built from live-RE'd sampler state and bound as a
    // combined-image-sampler alongside the existing float-constant UBOs, execute_draw's indexed
    // variant issues cmd_bind_index_buffer/cmd_draw_indexed, and both pipelines build real depth
    // (ensure_depth_stencil_view, build_depth_state) and blend (build_blend_state) state from
    // render_state instead of the old always-disabled defaults. Verified end-to-end by
    // d3d9_texture_test.cpp -- see HANDOFF_MACBOOK.md §16 and docs/d3d9-roadmap.md for what's still open.
    class d3d9_host
    {
      public:
        // vulkan is the same instance the gpu_command_processor already owns (a sibling member) --
        // shared, not a second GPU connection. Its instance/device are created lazily, on first
        // render-target-kind resource creation, mirroring handle_NtGdiDdDDICreateDevice's own
        // lazy-init pattern for the DXGK path.
        explicit d3d9_host(vulkan_host& vulkan) : vulkan_(vulkan) {}

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

        // Copies the resource's current host-side pixel backing (BGRA8, kept in sync with its GPU
        // image by every pfnClear/pfnDrawPrimitive) out for presentation. Returns false if the
        // resource doesn't exist or has no GPU backing (not a render target).
        bool snapshot_resource(uint64_t resource, std::vector<std::byte>& out_pixels, uint32_t& out_width,
                               uint32_t& out_height) const;

        // Uploads a plain sampled texture_2d resource's current `backing` shadow into its real
        // vk_image_id via a staging buffer, so it's ready to be sampled by a draw. No dirty tracking --
        // every call re-uploads unconditionally (see d3d9_host.cpp's comment on this method for why).
        // Called by execute_draw whenever a real, GPU-backed texture is bound at stage 0. Returns false
        // (no-op) if the resource isn't a texture with real GPU backing, or its backing doesn't yet hold
        // a full mip-0 image's worth of pixel data.
        bool ensure_texture_uploaded(uint64_t resource);

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
            std::vector<std::byte> backing; // host-side shadow copy; kept in sync with vk_image below
            uint64_t vk_image_id{}; // 0 = no GPU backing (plain buffer); set for render targets and textures
            uint64_t vk_image_view_id{};    // 0 until first drawn to; lazily created, cached per resource
        };

        struct shader_entry
        {
            std::vector<uint32_t> tokens;
        };

        struct vertex_decl_entry
        {
            std::vector<std::byte> elements; // raw d3d9_cmd::vertex_element entries
        };

        // Per-device fixed-function/DDI state. Most of this is now consumed by execute_draw and the
        // pipeline builders (render_state, bound_textures, sampler_state, index_buffer, stream_sources/
        // strides, vertex_decl, vs/ps_const_f, render_targets, depth_stencil); texture_stage_state (the
        // non-sampler TSS values, e.g. D3DTSS_COLOROP) and stream_frequencies are still write-only,
        // tracked for fixed-function texture combining and instancing respectively, neither in scope yet.
        struct device_state
        {
            std::unordered_map<uint32_t, uint32_t> render_state{};
            std::unordered_map<uint64_t, uint32_t> texture_stage_state{};  // key = (stage << 32) | state
            std::unordered_map<uint64_t, uint32_t> sampler_state{};        // key = (sampler << 32) | state
            std::unordered_map<uint32_t, uint64_t> bound_textures{};       // key = stage
            std::unordered_map<uint32_t, uint64_t> stream_sources{};       // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_strides{};       // key = stream_number
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

        // Starts far above any value the real d3d9.dll runtime's own internal handle spaces (vertex/
        // index buffer object handles, observed live as small sequential integers under a few hundred)
        // could ever reach, so a real pfnCreateResource-allocated id can never numerically collide with
        // one of those unrelated, never-registered handles -- see g_created_resource_ids' comment in
        // sogen_d3d9_umd.cpp for the real, live-hit collision (twice, at two different numeric ranges)
        // this is fixing at its actual source instead of chasing further symptomatic guest-side patches.
        uint64_t next_id_{1ULL << 32};
        std::unordered_map<uint64_t, resource_entry> resources_{};
        std::unordered_map<uint64_t, shader_entry> shaders_{};
        std::unordered_map<uint64_t, vertex_decl_entry> vertex_decls_{};
        device_state state_{};

        vulkan_host& vulkan_;
        uint64_t vk_instance_{};        // 0 until lazily created
        uint64_t vk_physical_device_{}; // 0 until lazily created
        uint64_t vk_device_{};          // 0 until lazily created

        // Lazily created once per device: a single command pool/buffer/fence/queue reused for every
        // draw, submitted and waited on synchronously (simplest correct model for a first triangle --
        // no double-buffering/pipelining yet).
        uint64_t queue_{};
        uint64_t command_pool_{};
        uint64_t command_buffer_{};
        uint64_t fence_{};
        // Sized for the two per-draw constant-register UBOs (VS set 0 + PS set 1); reset each draw in
        // execute_draw, with descriptor sets re-allocated from it every time.
        uint64_t descriptor_pool_{};
        bool draw_infra_ready_{false};

        // The one hardcoded fixed-function shader pair (see execute_draw's comment) and its pipeline,
        // lazily created and cached -- valid for every draw in this milestone since nothing about the
        // pipeline shape varies yet.
        uint64_t vs_module_{};
        uint64_t fs_module_{};
        uint64_t pipeline_layout_{};
        uint64_t pipeline_{};
        bool pipeline_ready_{false};

        struct programmable_pipeline_entry
        {
            uint64_t vs_module{};
            uint64_t fs_module{};
            // Set 0 = VS float-const UBO (binding 0), set 1 = PS float-const UBO (binding 0) -- matches
            // the CBV bindings d3d9_shader_translator.cpp pins into the SPIR-V. Cached alongside the
            // pipeline (rather than destroyed after create_graphics_pipeline like a one-shot local) so
            // execute_draw's cmd_bind_descriptor_sets has stable layout ids to bind into on every draw.
            uint64_t vs_set_layout{};
            uint64_t ps_set_layout{};
            uint64_t pipeline_layout{};
            uint64_t pipeline{};
        };

        // Keyed by (vertex_shader_id << 32 | pixel_shader_id) -- translation is lazy, on first draw
        // with both shaders bound, since SM1-3 requires the VS/PS pair together to build the
        // inter-stage varying map (see d3d9_shader_translator.hpp).
        std::unordered_map<uint64_t, programmable_pipeline_entry> programmable_pipelines_{};

        uint64_t allocate_id();
        // Lazily creates a bare Vulkan instance/device on vulkan_ (first render-target-kind resource).
        // Returns 0 on failure.
        uint64_t ensure_vk_device();
        bool ensure_draw_infra();
        // depth_format is a VkFormat (0 = no depth attachment), matching create_graphics_pipeline's own
        // dynamic-rendering depth_format parameter.
        bool ensure_pipeline(uint32_t color_format, uint32_t width, uint32_t height, uint32_t depth_format);
        // Builds a fresh VkSampler from the accumulated D3D9 sampler state for `sampler_index` (falling
        // back to D3D9's own documented per-state defaults for anything never explicitly set). Created
        // fresh per draw and destroyed after, mirroring execute_draw's own per-draw VS/PS UBO lifecycle --
        // no persistent sampler cache yet.
        bool build_sampler(uint64_t device, uint32_t sampler_index, uint64_t& out_sampler) const;
        const programmable_pipeline_entry* ensure_programmable_pipeline(uint32_t color_format, uint32_t width, uint32_t height,
                                                                         uint32_t depth_format);
        // Lazily creates ds_entry's depth image view and, on that same first use, clears it once to
        // D3D9's own default far-plane depth (1.0) -- see the .cpp definition's comment for why.
        // No-op (returns true) if ds_entry already has a view. depth_format is ds_entry's own VkFormat.
        bool ensure_depth_stencil_view(uint64_t device, resource_entry& ds_entry, uint32_t depth_format);

        // Present only for indexed draws; execute_draw binds `index_buffer` and calls cmd_draw_indexed
        // instead of cmd_draw when passed. index_format matches set_indices_record::format (0 = 16-bit,
        // 1 = 32-bit indices).
        struct indexed_draw
        {
            uint64_t index_buffer;
            uint32_t index_format;
            uint32_t first_index;
            int32_t base_vertex_index;
        };
        int32_t execute_draw(uint32_t vertex_count, uint32_t first_vertex, const indexed_draw* indexed = nullptr);
    };
} // namespace sogen
