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

    // One host-visible GPU buffer that a draw sub-allocates many distinct ranges from (vertex streams,
    // the index buffer, and the constant UBOs all live in this single buffer). `capacity` is the byte
    // size the buffer was last (re)created with; `offset` is the bump-allocator cursor -- arena_suballoc
    // (d3d9_host.cpp) hands out a 256-byte-aligned slice at `offset` and advances it, growing (destroy +
    // recreate, high-water mark) only when a draw needs more than `capacity`. execute_draw resets
    // `offset` to 0 at the start of every draw (see its comment there).
    struct frame_arena
    {
        uint64_t buffer{};
        uint64_t memory{};
        size_t capacity{};
        size_t offset{};
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

        // Process-lifetime diagnostic counters exposed for gpu_bridge's per-frame draw/submit logging
        // (see the members' own comment). draw_count() is every execute_draw call; batch_submit_count()
        // is every real batch submit -- together they make the batching win (draws >> submits) directly
        // observable rather than only inferable from wall-clock timing.
        uint64_t draw_count() const { return this->draw_count_; }
        uint64_t batch_submit_count() const { return this->batch_submit_count_; }
        // True if `resource` is a render-target-kind resource -- the frame-output image whose pixels a
        // Lock/Present reads back. Lets gpu_bridge fire its draw/submit summary only at a real frame
        // completion (a render-target Lock), not on every vertex/index-buffer Lock.
        bool is_render_target(uint64_t resource) const;

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
            std::vector<std::byte> backing; // subresource 0 (mip level 0); buffers/RTs use only this
            // backing (not extra_mips[0]) stays the storage for level 0 specifically -- not folded
            // uniformly into extra_mips[0..N-1] -- so every render-target/buffer/pre-mip-mapping call
            // site that already addresses `.backing` directly (sync_backing_from_gpu, StretchRect's
            // dst-copy, tex_blt, etc.) needed zero changes for this feature to land.
            //
            // Per-mip-level backing for subresources 1..mip_levels-1 (sampled textures only): each mip is
            // a different byte size, so a single flat vector can't address them. Empty for buffers, render
            // targets, and single-mip textures. Indexed as extra_mips[subresource - 1].
            std::vector<std::vector<std::byte>> extra_mips;
            uint64_t vk_image_id{}; // 0 = no GPU backing (plain buffer); set for render targets and textures
            uint64_t vk_image_view_id{};    // 0 until first drawn to; lazily created, cached per resource
            bool backing_dirty{}; // color RT: GPU image has drawn/cleared pixels not yet copied to backing

            // Selects subresource `index`'s backing store (index 0 == `backing`; higher == a mip level).
            // Callers must bounds-check index against extra_mips.size() + 1 before calling.
            std::vector<std::byte>& subresource_backing(const uint32_t index)
            {
                return index == 0 ? backing : extra_mips[index - 1];
            }
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
        // depth_stencil, stream_frequencies -- the last now decoded by resolve_instancing() into a draw's
        // instance count and per-instance binding mask for D3D9 hardware instancing); texture_stage_state
        // (the non-sampler TSS values, e.g. D3DTSS_COLOROP) is still write-only, tracked for fixed-function
        // texture combining, not in scope yet.
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
            // DrawPrimitiveUP/DrawIndexedPrimitiveUP user-memory sources: a non-empty entry means that
            // stream (or the index source) is UM-backed -- execute_draw uploads these raw bytes as a
            // transient buffer instead of looking up a resource id. Mutually exclusive with a real
            // stream_sources/index_buffer binding for the same slot (each binding path clears the other).
            std::unordered_map<uint32_t, std::vector<std::byte>> stream_um_data{}; // key = stream_number
            std::vector<std::byte> index_um_data{};
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

        // Lazily created once per device: a single command pool/queue, plus a command_buffer_/fence_
        // pair reused (submitted and waited on synchronously) by the prep helpers
        // (ensure_texture_uploaded, ensure_depth_stencil_view), color_fill, and blt -- draws themselves
        // record into the separate batch_command_buffer_/batch_fence_ below.
        uint64_t queue_{};
        uint64_t command_pool_{};
        uint64_t command_buffer_{};
        uint64_t fence_{};
        bool draw_infra_ready_{false};

        // A separate, dedicated command buffer/fence for the batched-draw recording, so it never
        // collides with the shared command_buffer_/fence_ that the prep helpers, color_fill, and blt
        // submit+wait on synchronously above.
        uint64_t batch_command_buffer_{};
        uint64_t batch_fence_{};
        bool batch_open_{false};
        // Render target (slot-0 handle) the currently-open batch records into; 0 = none. A draw whose
        // slot-0 render target differs flushes the batch first (execute_draw), so a batch never mixes
        // render targets.
        uint64_t batch_rt_{};
        // Draws recorded into the currently-open batch that consumed a descriptor-set pair (programmable
        // draws). Reset to 0 on batch open; when the next draw would exceed frame_desc_capacity_draws_ the
        // batch is flushed first so the pool can be reset (see execute_draw's overflow guard).
        uint32_t batch_draw_count_{};

        // Process-lifetime instrumentation, never reset: draw_count_ increments once per execute_draw
        // call, batch_submit_count_ once per real flush_batch() submit (an open batch actually
        // ended+submitted+waited, not a no-op flush). gpu_bridge logs both at each frame-completion point
        // so batching's draws-per-submit ratio is directly observable. Distinct from batch_draw_count_
        // above, which is a per-batch descriptor-pool bookkeeping counter that resets every batch.
        uint64_t draw_count_{};
        uint64_t batch_submit_count_{};

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
        // which bakes depthTestEnable/depthWriteEnable/depthCompareOp as STATIC pipeline state), the
        // vertex-input shape (declaration identity AND the per-stream vertex-buffer strides the build
        // reads -- see vertex_input_shape/vertex_shape_key()), and the depth/blend render-state that
        // build_depth_state/build_blend_state bake into the pipeline STATICALLY (not dynamic state):
        // depth is the resolved depthTestEnable/depthWriteEnable/depthCompareOp, blend is the resolved
        // blendEnable and src/dst/op blend factors + write mask. Depth-compare op and depth_format are
        // distinct axes: two draws with the same depth_format but different D3DRS_ZFUNC/ZWRITEENABLE, or
        // toggling D3DRS_ALPHABLENDENABLE / changing D3DRS_SRCBLEND/DESTBLEND between draws, produce
        // genuinely different pipelines. Two draws that differ in any of these need genuinely different
        // VkPipeline objects -- caching on a subset silently reuses a stale pipeline (e.g. omitting blend
        // makes a blend-enabled draw incorrectly reuse an earlier blend-disabled pipeline and render
        // transparent geometry opaque). Cull mode, fill mode, stencil, and D3DRS_COLORWRITEENABLE are
        // currently hardcoded constants in create_graphics_pipeline/build_blend_state, not yet driven by
        // render_state -- if any of those are ever made render-state-driven, they need the same
        // treatment: fold them into this key, exactly like depth/blend were just added here.
        // D3D9 vertex streams are addressed by a 32-bit mask (bit i => stream i) throughout this file
        // (parsed_vertex_decl::used_binding_mask, usable_vertex_binding_mask, ensure_programmable_pipeline's
        // 0..31 stream loops), so 32 slots cover every stream a declaration can reference. Well above
        // D3D9's real MaxStreams cap (16); sized to the mask width so a stride snapshot can never truncate.
        static constexpr uint32_t max_vertex_streams = 32;

        // Resolved D3D9 hardware-instancing state for the current draw, decoded once from
        // state_.stream_frequencies (the raw SetStreamSourceFreq dividers, INCLUDING their
        // D3DSTREAMSOURCE_* flag bits). instance_count is the vkCmdDraw*/instanceCount to issue;
        // instance_binding_mask has bit i set iff stream i must be bound VK_VERTEX_INPUT_RATE_INSTANCE.
        // The pipeline-cache key (via vertex_shape_key), the vertex-binding builder
        // (ensure_programmable_pipeline), and execute_draw's draw calls all derive their decisions from
        // this ONE helper (resolve_instancing) so they can never disagree.
        struct instancing_state
        {
            uint32_t instance_count{1};       // from the D3DSTREAMSOURCE_INDEXEDDATA stream's low 30 bits
            uint32_t instance_binding_mask{}; // bit i => stream i is D3DSTREAMSOURCE_INSTANCEDATA (divider 1)
        };

        // Full identity of the vertex-input state ensure_programmable_pipeline will BUILD for a draw, so a
        // cache key computed from it can never disagree with what actually gets built on a miss. `id` is the
        // real declaration handle (allocate_id(), >= 0x10000) or one of two fallback tags (1/2); see
        // vertex_shape_key(). `strides` snapshots state_.stream_strides for exactly the streams the build
        // consumes -- the built VkVertexInputBindingDescription::stride for each binding is read straight
        // from state_.stream_strides[stream], which SetStreamSource(stream, buffer, offset, stride) can
        // change WITHOUT changing the declaration handle. Two draws with the same declaration/VS/PS/RT-shape
        // but a different bound stride are genuinely different pipelines; keying on `id` alone silently
        // reuses a pipeline built for the first stride and mis-fetches every vertex past index 0.
        //
        // instance_binding_mask (bit i => stream i is VK_VERTEX_INPUT_RATE_INSTANCE, from a
        // D3DSTREAMSOURCE_INSTANCEDATA SetStreamSourceFreq -- see resolve_instancing()) is folded in for
        // the same reason as the per-stream strides above: ensure_programmable_pipeline bakes each
        // binding's inputRate STATICALLY into the pipeline, and SetStreamSourceFreq can flip a stream
        // between per-vertex and per-instance WITHOUT changing the declaration handle. Two draws with the
        // same declaration/strides but a different instance mask are genuinely different pipelines; keying
        // on `id`/`strides` alone would reuse a per-vertex pipeline for a per-instance draw (or vice
        // versa) and fetch the instanced stream by the wrong index. Defaults to 0 (all streams per-vertex),
        // so every non-instanced draw -- which is every draw that never calls SetStreamSourceFreq with an
        // INSTANCEDATA flag, i.e. every existing test -- keys exactly as it did before instancing existed.
        struct vertex_input_shape
        {
            uint64_t id{};
            std::array<uint32_t, max_vertex_streams> strides{};
            uint32_t instance_binding_mask{};
            auto operator<=>(const vertex_input_shape&) const = default;
        };

        struct pipeline_cache_key
        {
            uint64_t vertex_shader{};
            uint64_t pixel_shader{};
            std::array<uint32_t, 4> color_formats{}; // slot-order, 0-padded (VK_FORMAT_UNDEFINED == 0, never a real bound format)
            uint32_t depth_format{};
            vertex_input_shape vertex_shape{};   // decl identity + per-stream strides the build reads (see vertex_shape_key())
            vulkan_host::depth_state depth{};    // resolved static depth test/write/compare (build_depth_state)
            vulkan_host::color_blend_attachment blend{}; // resolved static blend enable/factors/write-mask (build_blend_state)
            auto operator<=>(const pipeline_cache_key&) const = default;
        };

        // Keyed by pipeline_cache_key with vertex_shader/pixel_shader/vertex_shape all 0 (FF never varies
        // these) -- RT/depth format and the resolved depth/blend render state (see pipeline_cache_key's
        // own comment) are what distinguish one FF pipeline from another.
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

        // Keyed by pipeline_cache_key (VS/PS pair, bound RT/depth formats, vertex-input shape, and
        // resolved depth/blend render state -- see pipeline_cache_key's own comment). Translation is
        // lazy, on first draw with both shaders
        // bound, since SM1-3 requires the VS/PS pair together to build the inter-stage varying map (see
        // d3d9_shader_translator.hpp).
        std::map<pipeline_cache_key, programmable_pipeline_entry> programmable_pipelines_{};

        // Content-addressed VkSampler cache. Unlike the VB/IB/UBO arena (one buffer whose per-draw
        // slices are re-sub-allocated and rewritten every draw), a VkSampler is immutable once created --
        // differing filter/address/aniso/LOD state genuinely needs a different object. So this is a cache keyed by
        // the resolved sampler-state tuple (every field build_sampler varies the VkSampler on), created
        // lazily on first use of a given state and retained for the device's lifetime, exactly like
        // programmable_pipelines_/ff_pipelines_. There is no synchronization hazard: nothing ever mutates
        // a cached sampler after creation, so draws that reuse it across frames only ever read it.
        //
        // compare_enable/compare_op/border_color/mip_lod_bias are deliberately NOT here: build_sampler
        // passes hardcoded constants for all four (see its own implementation), never derived from any
        // D3D9 state, so they can never distinguish two real requests and are safe to omit from the key.
        struct sampler_cache_key
        {
            uint32_t mag_filter{};
            uint32_t min_filter{};
            uint32_t mipmap_mode{};
            uint32_t address_u{};
            uint32_t address_v{};
            uint32_t address_w{};
            // Folded in even when disabled (Vulkan then ignores max_anisotropy) -- two D3D9 states
            // differing only in MAXANISOTROPY while aniso is off miss the cache unnecessarily. An
            // accepted, minor cache-effectiveness gap, not a correctness issue.
            uint32_t anisotropy_enable{};
            float max_anisotropy{};
            float min_lod{};
            float max_lod{};
            auto operator<=>(const sampler_cache_key&) const = default;
        };
        std::map<sampler_cache_key, uint64_t> sampler_cache_{};

        // Single per-device-lifetime GPU buffer backing every vertex/index/uniform range a draw needs.
        // Each range is a distinct 256-byte-aligned slice handed out by arena_suballoc; the buffer is
        // created lazily and grown grow-only across draws. execute_draw resets its offset to 0 at the
        // start of every draw -- reuse is safe because execute_draw submits and blocks on a fence before
        // returning, so a prior draw's GPU read of the arena has completed before a later draw rewrites
        // it. Combined VERTEX|INDEX|UNIFORM usage so one buffer serves all three binding points.
        frame_arena vertex_index_uniform_arena_{};

        // Shared descriptor pool that every programmable draw allocates its per-draw VS/PS descriptor-set
        // pair from, replacing the old per-pipeline pool that pre-allocated exactly two reused sets. Sized
        // for frame_desc_capacity_draws_ draws (2 sets each), created lazily and grown grow-only. Like the
        // arena, execute_draw resets it (reset_descriptor_pool) at the start of every draw -- safe because
        // every draw submits and blocks on a fence before returning, so a prior draw's GPU read of a set
        // has completed before reset frees it. Per-draw descriptor-type counts (6 UBOs + max_vs_sampler_stages
        // + max_ps_sampler_stages combined-image-samplers) are scaled by the capacity; see
        // ensure_frame_descriptor_pool.
        uint64_t frame_descriptor_pool_{};
        uint32_t frame_desc_capacity_draws_{}; // how many draws' worth of sets the pool is currently sized for
        // Initial per-frame descriptor-pool capacity, in draws. One draw needs only 2 sets, so 256 is
        // ample headroom -- the growth path in ensure_frame_descriptor_pool exists for correctness, not
        // because a single draw is expected to exceed it.
        static constexpr uint32_t frame_desc_initial_draws = 256;

        uint64_t allocate_id();
        // Hands out a 256-byte-aligned `size`-byte slice of `arena`, returning its byte offset in
        // out_offset and advancing the arena's bump cursor. The single place any arena offset is
        // computed, so every consumer (vertex-buffer bind offset, index-buffer bind offset, UBO
        // descriptor offset) agrees with the buffer the bytes were actually uploaded into. Grows the
        // arena buffer (destroy + recreate, high-water mark) when a slice won't fit; returns false only
        // on a Vulkan allocation failure. See the .cpp definition for the 256-byte and growth rationale.
        bool arena_suballoc(frame_arena& arena, size_t size, size_t& out_offset);
        // Grows the arena buffer (destroy + recreate) to at least new_capacity, preserving nothing (the
        // caller resets/repopulates the arena). Destroying the buffer is only safe when no in-flight or
        // recorded-but-unsubmitted command references it, so callers must flush any open batch first.
        // Returns false only on a Vulkan allocation failure.
        bool grow_arena(frame_arena& arena, size_t new_capacity);
        // Ensures frame_descriptor_pool_ exists and is sized for at least needed_draws draws' worth of
        // descriptor sets (2 per draw). Creates it lazily at frame_desc_initial_draws capacity, or doubles
        // and recreates it (dropping the old pool -- every prior draw already fenced) when needed_draws
        // exceeds the current capacity. Returns false only on a Vulkan allocation failure.
        bool ensure_frame_descriptor_pool(uint64_t device, uint32_t needed_draws);
        // Lazily creates a bare Vulkan instance/device on vulkan_ (first render-target-kind resource).
        // Returns 0 on failure.
        uint64_t ensure_vk_device();
        bool ensure_draw_infra();
        // Ends, submits and waits on the open batch command buffer, closing the batch (batch_open_ =
        // false). A no-op when no batch is open. Called at every boundary that must observe the batch's
        // GPU work before proceeding (readback, clear, color_fill, blt, resource teardown, render-target
        // change) and at each batching scope boundary inside execute_draw.
        void flush_batch();
        // depth_format is a VkFormat (0 = no depth attachment), matching create_graphics_pipeline's own
        // dynamic-rendering depth_format parameter. color_formats holds one VkFormat per currently-bound
        // render target (slot order), each getting an identical blend-attachment entry -- D3D9 has no
        // independent per-RT blend state.
        bool ensure_pipeline(std::span<const uint32_t> color_formats, uint32_t width, uint32_t height, uint32_t depth_format);
        // Returns a VkSampler for the accumulated D3D9 sampler state for `sampler_index` (falling back to
        // D3D9's own documented per-state defaults for anything never explicitly set). Resolves the state
        // into a sampler_cache_key and looks it up in sampler_cache_: on a hit the existing (immutable)
        // sampler is reused, on a miss vulkan_host::create_sampler builds a new one that is then cached
        // for the device's lifetime. Not destroyed per draw -- a later draw with the same state reuses it.
        bool build_sampler(uint64_t device, uint32_t sampler_index, uint32_t mip_levels, uint64_t& out_sampler);
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
        // a miss. In the real-declaration branch `id` is the declaration handle (allocate_id(), starting
        // at 0x10000 -- see next_id_'s comment), since vertex_decl_entry::parsed is populated once,
        // eagerly, at create_vertex_decl time and never mutated after (no update DDI exists) -- the same
        // handle always implies the same element types/offsets/usages. But the handle does NOT pin the
        // per-binding strides: the build reads state_.stream_strides[stream] for each stream the
        // declaration references (usable_vertex_binding_mask), and SetStreamSource can change a stride
        // without touching the handle -- so `strides` additionally snapshots the current stride of every
        // stream in the declaration's used_binding_mask (others left 0). The no-real-declaration fallback
        // sets `id` to one of two tags (1 or 2, disjoint from every real handle) identifying which of the
        // two fallback shapes applies and leaves `strides` all-zero: that branch hardcodes its binding
        // stride to 16 or 20 (never the raw bound stride) and reads only stream 0, so the tag already
        // captures its entire stride-dependence -- there is no per-stream stride to fold in there.
        vertex_input_shape vertex_shape_key() const;
        // Decodes state_.stream_frequencies into the effective instance count and per-instance binding
        // mask (see instancing_state). The stream carrying D3DSTREAMSOURCE_INDEXEDDATA supplies the
        // instance count in its low 30 bits (default 1 when no stream sets that flag); each stream
        // carrying D3DSTREAMSOURCE_INSTANCEDATA sets its mask bit. KNOWN LIMITATION: only an INSTANCEDATA
        // divider of exactly 1 is honored -- a non-1 divider needs VK_EXT_vertex_attribute_divisor, which
        // is not enabled on this path, so such a stream is left per-vertex (mask bit unset) rather than
        // silently rendering wrong per-instance data. Called by vertex_shape_key (cache key),
        // ensure_programmable_pipeline (binding inputRate), and execute_draw (draw instanceCount).
        instancing_state resolve_instancing() const;
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

        // pfnColorFill: fills a rect of a color render target's Vulkan image with a single D3DCOLOR.
        // Implemented as a scoped buffer->image transfer copy on the shared draw command buffer, going
        // through the same cmd_pipeline_barrier choke point (which keeps render_targets[image]
        // current_layout authoritative) execute_draw uses; leaves the RT in TRANSFER_SRC_OPTIMAL and
        // marks backing_dirty so sync_backing_from_gpu picks it up on the next Lock/Present. Assumes the
        // RT is at its resting TRANSFER_SRC_OPTIMAL layout on entry (post clear/draw), matching
        // execute_draw's own documented assumption. `subresource` is always 0 (single-mip, single-layer
        // resources only, matching this codebase's current scope) and unused -- not yet plumbed to a
        // real mip/array level.
        int32_t color_fill(uint64_t resource, uint32_t subresource, int32_t left, int32_t top, int32_t right, int32_t bottom,
                           uint32_t color_argb);

        // pfnBlt (StretchRect): blits src_rect of src_resource's image into dst_rect of dst_resource's
        // image via vkCmdBlitImage (which scales natively when the rects differ in size). Same shared
        // command buffer / cmd_pipeline_barrier choke point as color_fill; both RTs assumed at their
        // resting TRANSFER_SRC_OPTIMAL layout on entry. Marks the destination backing_dirty.
        // `dst_subresource`/`src_subresource` are always 0 (single-mip, single-layer resources only) and
        // unused -- not yet plumbed to a real mip/array level; a future mip-generation consumer reusing
        // this as a blit-between-mip-levels primitive would need to thread these through into the
        // underlying image_blit_region's mip_level/base_array_layer, which are currently hardcoded to 0.
        int32_t blt(uint64_t dst_resource, uint32_t dst_subresource, int32_t dst_left, int32_t dst_top, int32_t dst_right,
                    int32_t dst_bottom, uint64_t src_resource, uint32_t src_subresource, int32_t src_left, int32_t src_top,
                    int32_t src_right, int32_t src_bottom, uint32_t filter);

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
