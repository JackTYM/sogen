#pragma once

#include "d3d9_shader_translator.hpp"
#include "vulkan_host.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace sogen
{
    // One D3DVERTEXELEMENT9-derived attribute, translated to what a Vulkan graphics pipeline needs.
    // binding is the element's own D3DVERTEXELEMENT9::Stream (so multi-stream vertex sources map to
    // distinct Vulkan vertex bindings, one per D3D9 stream); offset is the element's own Offset,
    // relative to the start of one vertex in that stream. See parse_vertex_decl's definition
    // (d3d9_host.cpp) for how location is derived -- it is NOT a function of usage/usage_index alone,
    // despite D3DDECLUSAGE being the natural-looking key; that finding is written up there in full.
    struct parsed_vertex_attribute
    {
        uint32_t location;
        uint32_t binding;
        uint32_t vk_format;
        uint32_t offset;
    };

    struct parsed_vertex_decl
    {
        std::vector<parsed_vertex_attribute> attributes;
        uint32_t used_binding_mask{}; // bit i set => stream i is referenced by this declaration
    };

    // Reinterprets blob as a back-to-back array of d3d9_cmd::vertex_element (8 bytes each; blob.size()
    // must be a multiple of that, any remainder is ignored) and produces one parsed_vertex_attribute
    // per element whose D3DDECLTYPE is recognized by d3d9_format.hpp's d3d9_decl_type_to_vulkan --
    // elements with an unrecognized type are skipped (not added to attributes), matching how
    // d3d9_format_to_vulkan itself signals an unsupported format rather than guessing one.
    parsed_vertex_decl parse_vertex_decl(std::span<const std::byte> blob);

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
    // can do a real clear and mark the resource's host-side shadow copy dirty; sync_backing_from_gpu
    // lazily copies the GPU image into that shadow the next time something needs it (pfnLock/pfnUnlock,
    // already wired, hand the shadow back to the app). This deliberately sidesteps needing to
    // know how the real d3d9.dll gets pixels onto an actual window (RE'd to be genuinely opaque to the
    // driver -- D3DDDIARG_PRESENT carries no HWND): whatever mechanism the runtime uses to display the
    // backbuffer, it goes through Lock to read pixels back from the driver first.
    //
    // Part 3 (the real draw path -- ensure_draw_infra/ensure_pipeline/execute_draw) is implemented: a
    // single hardcoded fixed-function shader pair (D3DFVF_XYZRHW|D3DFVF_DIFFUSE), a cached pipeline,
    // real vertex buffer upload, dynamic rendering with explicit layout barriers, and marking the
    // render target's backing store dirty, mirroring pfnClear's own pattern -- sync_backing_from_gpu
    // does the actual lazy readback. Verified end-to-end via the guest D3D9 API -- DrawPrimitive()/
    // Present() both succeed, pixel readback matches expected.
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

        // Real pfnTexBlt handler: copies src_resource's host-side pixel backing into dst_resource's.
        // This is the D3DPOOL_MANAGED fix -- see sogen_d3d9_umd.cpp's umd_TexBlt for the live-RE trail
        // showing the real d3d9.dll issues exactly this call, with these two resource ids, to sync a
        // MANAGED texture's sysmem "master" copy (what LockRect/UnlockRect write) into its lazily
        // created vidmem copy (what SetTexture binds) on first use. Both resources must already exist
        // and share the same format/dimensions (guaranteed here: both were created from the exact same
        // CreateTexture() call). ensure_texture_uploaded already re-uploads a texture's `backing` to its
        // GPU image unconditionally on every draw, so this only needs to update the CPU-side shadow.
        int32_t tex_blt(uint64_t dst_resource, uint64_t src_resource);

        // Copies up to out_capacity bytes of the resource's host-side shadow copy into out.
        // out_data_size always receives the true backing-store size.
        int32_t lock(uint64_t resource, uint32_t subresource, uint32_t offset, uint32_t size, uint32_t flags, void* out,
                    size_t out_capacity, uint32_t& out_data_size);
        int32_t unlock(uint64_t resource, uint32_t subresource, uint32_t offset, const void* data, size_t data_size);

        // Copies the resource's current host-side pixel backing (BGRA8) out for presentation. Lazily
        // syncs from the GPU image first via sync_backing_from_gpu if pfnClear/pfnDrawPrimitive left it
        // dirty. Returns false if the resource doesn't exist or has no GPU backing (not a render target).
        bool snapshot_resource(uint64_t resource, std::vector<std::byte>& out_pixels, uint32_t& out_width,
                               uint32_t& out_height);

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
            bool backing_dirty{}; // color RT: GPU image has drawn/cleared pixels not yet copied to backing
        };

        struct shader_entry
        {
            std::vector<uint32_t> tokens;
        };

        struct vertex_decl_entry
        {
            std::vector<std::byte> elements; // raw d3d9_cmd::vertex_element entries
            // Populated eagerly by create_vertex_decl right after the memcpy above -- a D3D9 vertex
            // declaration is immutable once created (no update DDI exists), so there is no staleness
            // to guard against and no reason to defer this to first draw-time use.
            std::optional<parsed_vertex_decl> parsed;
        };

        // Per-device fixed-function/DDI state. Most of this is now consumed by execute_draw and the
        // pipeline builders (render_state, bound_textures, sampler_state, index_buffer, stream_sources/
        // strides, vertex_decl, vs/ps_const_f, vs/ps_const_i, vs/ps_const_b, render_targets,
        // depth_stencil); texture_stage_state (the non-sampler TSS values, e.g. D3DTSS_COLOROP) and
        // stream_frequencies are still write-only, tracked for fixed-function texture combining and
        // instancing respectively, neither in scope yet.
        struct device_state
        {
            std::unordered_map<uint32_t, uint32_t> render_state{};
            std::unordered_map<uint64_t, uint32_t> texture_stage_state{};  // key = (stage << 32) | state
            std::unordered_map<uint64_t, uint32_t> sampler_state{};        // key = (sampler << 32) | state
            std::unordered_map<uint32_t, uint64_t> bound_textures{};       // key = stage
            std::unordered_map<uint32_t, uint64_t> stream_sources{};       // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_strides{};       // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_offsets{};       // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_frequencies{};   // key = stream_number
            uint64_t index_buffer{};
            uint32_t index_format{};
            uint64_t vertex_decl{};
            uint64_t vertex_shader{};
            uint64_t pixel_shader{};
            std::vector<float> vs_const_f{};
            std::vector<float> ps_const_f{};
            std::vector<int32_t> vs_const_i{};
            std::vector<int32_t> ps_const_i{};
            // Expanded to 4-word (16-byte) stride per register at receipt time, matching vs/ps_const_f's
            // layout, even though the wire payload itself is tightly packed (see d3d9_set_vs_const_b's
            // handler in d3d9_host.cpp) -- only element (register * 4) is ever non-zero.
            std::vector<uint32_t> vs_const_b{};
            std::vector<uint32_t> ps_const_b{};
            std::array<uint64_t, 4> render_targets{};
            uint64_t depth_stencil{};
            // Last SetScissorRect rect (RECT semantics -- exclusive right/bottom); only consulted at
            // draw time when D3DRS_SCISSORTESTENABLE is set (see execute_draw).
            int32_t scissor_left{};
            int32_t scissor_top{};
            int32_t scissor_right{};
            int32_t scissor_bottom{};
        };

        // Starts far above any value the real d3d9.dll runtime's own internal handle spaces (vertex/
        // index buffer object handles, observed live as small sequential integers under a few hundred)
        // could ever reach, so a real pfnCreateResource-allocated id can never numerically collide with
        // one of those unrelated, never-registered handles -- see g_created_resource_ids' comment in
        // sogen_d3d9_umd.cpp for the real, live-hit collision (twice, at two different numeric ranges)
        // this is fixing at its actual source instead of chasing further symptomatic guest-side patches.
        //
        // MUST stay below 2^32: every id allocated here is round-tripped through the guest UMD as a
        // HANDLE (e.g. create_shader_common's `pArgs->ShaderHandle = reinterpret_cast<HANDLE>(
        // static_cast<uintptr_t>(resp.shader))`), and HANDLE/uintptr_t are only 32 bits wide on an x86
        // (WoW64) guest. An id starting at 1ULL<<32 silently truncates to its low 32 bits there (e.g.
        // 4294967301 -> 5), which the app then echoes back via SetVertexShaderFunc/SetPixelShader --
        // looking exactly like an unrelated small runtime-internal handle and making shaders_.find()
        // miss, so ensure_programmable_pipeline silently returns nullptr and every draw silently no-ops.
        // Root-caused 2026-07-04 via d3d9-const-test-x86.exe's pixel-exact failure (the first x86 test
        // to actually verify rendered output); x64's 64-bit HANDLE never truncated so this was invisible
        // there, and d3d9-shader-test-x86.exe never caught it either since it only checks HRESULTs, not
        // pixels. 0x10000 keeps ~100-300x headroom over the documented "few hundred" runtime handles
        // while comfortably fitting in 32 bits.
        //
        // This single counter is shared across resources_/shaders_/vertex_decls_ below -- don't give
        // any one of them its own separately-seeded counter, or the 32-bit-safety guarantee above only
        // covers that one map again.
        uint64_t next_id_{0x10000};
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

        // The one hardcoded fixed-function shader pair (see execute_draw's comment), its shader modules
        // and pipeline layout -- shape-invariant (FF always uses the same hardcoded XYZRHW+DIFFUSE vertex
        // layout), so these are lazily created once and reused for every FF pipeline variant.
        uint64_t vs_module_{};
        uint64_t fs_module_{};
        uint64_t pipeline_layout_{};
        // The VkPipeline resolved by the most recent ensure_pipeline() call (looked up/inserted into
        // ff_pipelines_ below) -- execute_draw reads this right after ensure_pipeline() returns true,
        // same single-threaded, no-reentrancy pattern as ensure_programmable_pipeline's returned pointer.
        uint64_t pipeline_{};

        // Fingerprint of every input that actually varies a built VkPipeline: the bound VS/PS pair (0/0
        // for the fixed-function pipeline, which never varies these), the bound color-attachment formats
        // (baked into VkPipelineRenderingCreateInfo), the depth format (also feeds build_depth_state,
        // which bakes depthTestEnable/depthWriteEnable/depthCompareOp as STATIC pipeline state), and the
        // vertex-input shape (see vertex_shape_key()). Two draws that differ in any of these need
        // genuinely different VkPipeline objects -- caching on a subset silently reuses a stale pipeline.
        struct pipeline_cache_key
        {
            uint64_t vertex_shader{};
            uint64_t pixel_shader{};
            std::array<uint32_t, 4> color_formats{}; // slot-order, 0-padded (VK_FORMAT_UNDEFINED == 0, never a real bound format)
            uint32_t depth_format{};
            uint64_t vertex_shape{}; // see vertex_shape_key()
            friend auto operator<=>(const pipeline_cache_key&, const pipeline_cache_key&) = default;
            friend bool operator==(const pipeline_cache_key&, const pipeline_cache_key&) = default;
        };

        // Keyed by pipeline_cache_key with vertex_shader/pixel_shader/vertex_shape all 0 (FF never varies
        // these) -- only the RT/depth shape actually distinguishes one FF pipeline from another.
        std::map<pipeline_cache_key, uint64_t> ff_pipelines_{};

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

        // Keyed by pipeline_cache_key (VS/PS pair, bound RT/depth formats, and vertex-input shape --
        // see pipeline_cache_key's own comment). Translation is lazy, on first draw with both shaders
        // bound, since SM1-3 requires the VS/PS pair together to build the inter-stage varying map (see
        // d3d9_shader_translator.hpp).
        std::map<pipeline_cache_key, programmable_pipeline_entry> programmable_pipelines_{};

        uint64_t allocate_id();
        // Lazily creates a bare Vulkan instance/device on vulkan_ (first render-target-kind resource).
        // Returns 0 on failure.
        uint64_t ensure_vk_device();
        bool ensure_draw_infra();
        // depth_format is a VkFormat (0 = no depth attachment), matching create_graphics_pipeline's own
        // dynamic-rendering depth_format parameter. color_formats holds one VkFormat per currently-bound
        // render target (slot order), each getting an identical blend-attachment entry -- D3D9 has no
        // independent per-RT blend state.
        bool ensure_pipeline(std::span<const uint32_t> color_formats, uint32_t width, uint32_t height, uint32_t depth_format);
        // Builds a fresh VkSampler from the accumulated D3D9 sampler state for `sampler_index` (falling
        // back to D3D9's own documented per-state defaults for anything never explicitly set). Created
        // fresh per draw and destroyed after, mirroring execute_draw's own per-draw VS/PS UBO lifecycle --
        // no persistent sampler cache yet.
        bool build_sampler(uint64_t device, uint32_t sampler_index, uint64_t& out_sampler) const;
        // Returns the cached parsed_vertex_decl for state_.vertex_decl, or nullptr when there's no real
        // declaration to use (state_.vertex_decl == 0, or its cached parse produced no attributes --
        // e.g. a decl containing only unrecognized D3DDECLTYPEs). Shared by ensure_programmable_pipeline
        // (builds the pipeline's vertex input state) and execute_draw (uploads/binds the referenced
        // streams) so both always agree on which case -- real declaration vs. the pre-Task-8 stream-0
        // fallback -- applies to a given draw.
        const parsed_vertex_decl* find_real_vertex_decl() const;
        // Fingerprint of "what vertex-input shape will this draw's pipeline get built with", using the
        // exact same real-decl-vs-fallback-stride branch ensure_programmable_pipeline's vertex-input
        // builder uses, so a cache key computed here can never disagree with what actually gets built on
        // a miss. Real declaration handles (allocate_id(), starting at 0x10000 -- see next_id_'s comment)
        // are used directly, since vertex_decl_entry::parsed is populated once, eagerly, at
        // create_vertex_decl time and never mutated after (no update DDI exists) -- same handle always
        // implies the same shape. The no-real-declaration fallback returns one of two tags (1 or 2,
        // disjoint from every real handle, which start at 0x10000) identifying which of the two fallback
        // strides applies.
        uint64_t vertex_shape_key() const;
        // Filters decl.used_binding_mask down to only streams that ALSO have a real, nonzero stride in
        // state_.stream_strides -- i.e. streams the app has actually called SetStreamSource for. A
        // stream the declaration references but that has no (or a zero) stride is not usable: emitting
        // a Vulkan binding for it would mis-fetch, so it must be excluded from BOTH the pipeline's
        // vertex-input state (bindings AND attributes -- an attribute whose binding isn't in this
        // filtered mask must not be emitted either, or it would reference a VkVertexInputBindingDescription
        // that was never declared) and execute_draw's upload/bind loop. Both call this so they can never
        // disagree about which bindings are real.
        uint32_t usable_vertex_binding_mask(const parsed_vertex_decl& decl) const;
        // color_formats: see ensure_pipeline's own comment above.
        const programmable_pipeline_entry* ensure_programmable_pipeline(std::span<const uint32_t> color_formats, uint32_t width,
                                                                         uint32_t height, uint32_t depth_format);
        // Lazily creates ds_entry's depth image view and, on that same first use, clears it once to
        // D3D9's own default far-plane depth (1.0) -- see the .cpp definition's comment for why.
        // No-op (returns true) if ds_entry already has a view. depth_format is ds_entry's own VkFormat.
        bool ensure_depth_stencil_view(uint64_t device, resource_entry& ds_entry, uint32_t depth_format);

        // If this color RT has GPU-side pixels not yet mirrored into `backing`, copy them now (blocking)
        // and clear the flag -- the sole place this readback happens; pfnClear/pfnDrawPrimitive only
        // mark dirty, they no longer read back eagerly. No-op for buffers/plain textures (backing_dirty
        // never set for them) and for RTs already clean. Safe because readback_render_target itself
        // verifies the image is in TRANSFER_SRC_OPTIMAL layout (the resting state left by the draw/clear
        // that dirtied it) and fails closed otherwise.
        void sync_backing_from_gpu(resource_entry& rt);

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
