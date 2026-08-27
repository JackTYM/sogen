#pragma once

#include "d3d9_shader_translator.hpp"
#include "vulkan_host.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
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
        void* mapped{};
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
        explicit d3d9_host(vulkan_host& vulkan)
            : vulkan_(vulkan)
        {
        }

        // ---------------------------------------------------------------------------------------
        // Sync commands
        // ---------------------------------------------------------------------------------------

        int32_t create_resource(uint32_t kind, uint32_t format, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels,
                                uint32_t usage, uint32_t pool, uint64_t& out_resource);
        void destroy_resource(uint64_t resource);

        // Real pfnTexBlt handler: copies src_resource's host-side pixel backing into dst_resource's.
        // This is the D3DPOOL_MANAGED fix -- see sogen_d3d9_umd.cpp's umd_TexBlt for the live-RE trail
        // showing the real d3d9.dll issues exactly this call, with these two resource ids, to sync a
        // MANAGED texture's sysmem "master" copy (what LockRect/UnlockRect write) into its lazily
        // created vidmem copy (what SetTexture binds) on first use. Both resources must already exist
        // and share the same format/dimensions (guaranteed here: both were created from the exact same
        // CreateTexture() call). This only needs to update the CPU-side shadow and flag it: it marks dst
        // resource_entry::upload_dirty, and ensure_texture_uploaded re-uploads `backing` to dst's GPU
        // image on the next draw that samples it.
        int32_t tex_blt(uint64_t dst_resource, uint64_t src_resource);

        // Copies up to out_capacity bytes of the resource's host-side shadow copy into out.
        // out_data_size always receives the true backing-store size.
        int32_t lock(uint64_t resource, uint32_t subresource, uint32_t offset, uint32_t size, uint32_t flags, void* out,
                     size_t out_capacity, uint32_t& out_data_size);
        int32_t unlock(uint64_t resource, uint32_t subresource, uint32_t offset, const void* data, size_t data_size);

        // Resizes the resource's backing store as needed and returns a writable pointer directly into it
        // for the caller to fill (typically via a single guest-memory read), avoiding the extra host-side
        // copy `unlock` above requires when the caller already has the bytes in a separate buffer. Returns
        // nullptr on an invalid resource/subresource; the caller must then skip both the write and the
        // matching finish_unlock call. Must be followed by finish_unlock once the pointer's been filled.
        std::byte* prepare_unlock_target(uint64_t resource, uint32_t subresource, uint32_t offset, size_t data_size);
        void finish_unlock(uint64_t resource, uint32_t subresource);

        // Returns the resource's real, persistent HOST_VISIBLE|HOST_COHERENT buffer mapping (see
        // resource_entry::direct_mapped_ptr) for the caller to alias directly into the guest's address
        // space, letting Lock/Unlock skip the host round-trip entirely for eligible resources. out_size is
        // the whole ring's extent; out_slice_stride/out_slice_count describe how the guest carves it into
        // renameable slices. Returns false (leaving the outputs untouched) for any resource without a
        // direct buffer.
        bool get_direct_mapping(uint64_t resource, void*& out_ptr, size_t& out_size, uint32_t& out_slice_stride,
                                uint32_t& out_slice_count) const;

        // Submits whatever the open batch has recorded and waits for the GPU to finish it, so nothing the
        // guest wrote before this call can still be in flight afterwards. The guest UMD needs this before
        // recycling a direct buffer's oldest ring slice, the one point where renaming alone can't prove
        // the GPU is done with the bytes about to be overwritten.
        void flush_pending();

        // Copies the resource's current host-side pixel backing (BGRA8) out for presentation. Lazily
        // syncs from the GPU image first via sync_backing_from_gpu if pfnClear/pfnDrawPrimitive left it
        // dirty. Returns false if the resource doesn't exist or has no GPU backing (not a render target).
        bool snapshot_resource(uint64_t resource, std::vector<std::byte>& out_pixels, uint32_t& out_width, uint32_t& out_height);

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
        uint64_t draw_count() const
        {
            return this->draw_count_;
        }

        uint64_t batch_submit_count() const
        {
            return this->batch_submit_count_;
        }

        // Per-outcome classification of every draw and clear this host has processed -- see stats_'s own
        // comment (below, with the members) for why the raw draw/submit totals aren't enough.
        struct draw_stats
        {
            uint64_t drop_no_render_target{}; // execute_draw returned early: slot-0 RT missing/unbacked
            uint64_t drop_no_vertex_data{};   // ... no usable stream-0 vertex (or index) source bound
            uint64_t drop_no_pipeline{};      // ... shader translation or pipeline creation failed
            // drop_no_pipeline broken down by which stage of ensure_programmable_pipeline gave up.
            uint64_t drop_shader_missing{};   // the bound VS or PS id isn't in shaders_ at all
            uint64_t drop_translate_failed{}; // vkd3d-shader could not translate the pair to SPIR-V
            uint64_t drop_vk_object_failed{}; // a Vulkan module/layout/pipeline object failed to create
            uint64_t recorded_fixed{};        // draw recorded against the fixed-function pipeline
            uint64_t recorded_programmable{}; // draw recorded against a translated VS+PS pipeline
            uint64_t recorded_depth_tested{}; // subset of the two above whose pipeline has depth test on
            uint64_t clear_target{};          // pfnClear calls carrying D3DCLEAR_TARGET
            uint64_t clear_zbuffer{};         // ... D3DCLEAR_ZBUFFER
            uint64_t clear_stencil{};         // ... D3DCLEAR_STENCIL
            // Sampled-texture staging uploads actually run vs. skipped by the resource_entry::upload_dirty
            // check. A title whose textures are written once and sampled forever should show `skipped`
            // climbing per draw and `done` flat after load; `done` still climbing per frame means
            // something is re-writing texture backings every frame and the dirty check cannot help.
            uint64_t texture_upload_done{};
            uint64_t texture_upload_skipped{};
            // Same idea as the texture pair above, but for execute_draw's per-draw vertex-stream and
            // index-buffer arena uploads (Task #161) -- `skipped` means a resource's content_version and
            // the open batch's batch_generation both matched a still-valid cache entry, so the prior
            // upload's arena offset was reused as-is with no new memcpy.
            uint64_t vertex_upload_done{};
            uint64_t vertex_upload_skipped{};
            uint64_t index_upload_done{};
            uint64_t index_upload_skipped{};
            // programmable_pipelines_ lookups. A steady-state frame should be all hits: the cache is never
            // evicted, so a per-frame `miss` count means the key is genuinely varying (new shader pair,
            // new RT format, new vertex shape, or new depth/blend combination) and every one of those
            // misses pays a full vkd3d translate + VkShaderModule + vkCreateGraphicsPipelines.
            uint64_t pipeline_cache_hit{};
            uint64_t pipeline_cache_miss{};
            // Draws short-circuited by failed_pipelines_ -- a build this key already proved unbuildable,
            // skipped without re-running translation/module/pipeline creation. Counted separately from
            // pipeline_cache_miss so a "misses collapsed to zero" claim can be checked against the drops
            // that replaced them (this counter and drop_no_pipeline should climb together).
            uint64_t pipeline_negative_hit{};
            // Recorded draws bucketed by the slot-0 render target they targeted. A title that renders its
            // 3D scene into an off-screen target and composites it separately shows up here as a second,
            // heavily-drawn resource id that is NOT the one pfnPresent hands back -- the one shape of
            // "everything works but nothing is visible" that no other counter can distinguish.
            std::map<uint64_t, uint64_t> draws_per_render_target{};
            // Highest float constant register (c#) the guest has ever written, per stage. Compared against
            // the UBO caps in execute_draw: anything above them is silently truncated by build_ubo_staging
            // and reads back as zero in the shader, which is invisible in every other counter here.
            uint32_t max_vs_const_f_register{};
            uint32_t max_ps_const_f_register{};
            // Pixel-stage texture bindings the guest asked for that this host could not service, bucketed
            // by why. Every one of these leaves its combined-image-sampler descriptor unwritten, which is
            // only legal if the bound PS never samples that stage -- so a nonzero count here is the one
            // signal that a draw is reading an undefined sampler and silently rendering wrong.
            uint64_t sampler_skip_depth_stencil{};  // a depth-stencil surface bound as a texture (shadow map)
            uint64_t sampler_skip_upload_refused{}; // ensure_texture_uploaded declined (kind/format/backing)
            uint64_t sampler_skip_no_view{};        // no image view, i.e. d3d9_format_to_vulkan had no mapping
            // The D3D9 format of every skipped binding above, so an unhandled fourcc can be named rather
            // than guessed at.
            std::map<uint32_t, uint64_t> sampler_skip_formats{};
            // The sampler_skip_depth_stencil total above, split by the stage whose descriptor was left
            // unwritten: a shadow-map sampler register is identifiable this way, an aggregate count is not.
            std::map<uint32_t, uint64_t> depth_stencil_skip_at_stage{};
            // Recorded draws keyed by {slot-0 render target, vertex shader, pixel shader}. draws_per_
            // render_target says a target got N draws; this says which *program* produced them, which is
            // what identifies the one shader pair responsible for a title's dominant surface -- the only
            // way to know which of a hundred dumped shaders is worth reading.
            std::map<std::array<uint64_t, 3>, uint64_t> draws_per_shader_pair{};
            // Render-to-texture bindings that DID succeed: {pixel-sampler stage, render target, pixel
            // shader} -> draws. draws_per_render_target says which target a draw WROTE; this says which
            // previously-rendered target a draw READ, at which sampler register, and by which program.
            // Together they reconstruct a title's real multi-pass frame graph, which is what tells a
            // broken intermediate target apart from a broken consumer of one -- and names the shader
            // whose disassembly answers which of the two it is.
            std::map<std::array<uint64_t, 3>, uint64_t> rt_sampled_by_shader{};
            // Every distinct value the guest has ever set each D3D9 render state to, with a count.
            // describe_pipeline_state prints only the CURRENT value, which is whatever the last draw of
            // the frame (typically 2D HUD) left behind -- a state the title toggles per draw, which is
            // most of them, is therefore invisible there. This says what the state's full value set is.
            std::map<uint32_t, std::map<uint32_t, uint64_t>> render_state_values{};
            // Diagnostic (Task #144, sRGB-haze investigation): draws with D3DRS_ALPHABLENDENABLE set
            // while D3DRS_SRGBWRITEENABLE is OFF, targeting a render target whose format DOES have an
            // sRGB counterpart (d3d9_format_to_vulkan_srgb). Such a draw blends through the target's
            // LINEAR view -- reading whatever bytes are currently stored as-is, with no sRGB decode --
            // even though other draws (typically the opaque 3D scene) may have written real sRGB-encoded
            // bytes into that same target through its _SRGB view. If so, this draw's blend treats
            // gamma-bright stored bytes as linear, which is a genuine colour-space corruption distinct
            // from the D3DRS_SRGBWRITEENABLE gap already fixed. A nonzero count here does not by itself
            // prove a visible bug (the target might not yet hold real sRGB-encoded bytes at this point),
            // but it identifies exactly which shader pair to inspect via blend_srgb_mismatch_shader_pair.
            uint64_t blend_srgb_mismatch{};
            std::map<std::array<uint64_t, 3>, uint64_t> blend_srgb_mismatch_shader_pair{};
        };

        const draw_stats& stats() const
        {
            return this->stats_;
        }

        // How many distinct programmable pipelines are live. Read alongside pipeline_cache_miss: if the
        // two climb together the app keeps producing genuinely new state combinations; if misses climb
        // while this stays flat, something is evicting or re-keying entries that already exist.
        size_t programmable_pipeline_count() const
        {
            return this->programmable_pipelines_.size();
        }

        // Diagnostic (EMULATOR_D3D9_RESLIFE_DIAG): how many resources are alive right now. A leak in the
        // create/destroy path shows up here as a count that only ever climbs.
        size_t live_resource_count() const
        {
            return this->resources_.size();
        }

        // True if `resource` is a render-target-kind resource -- the frame-output image whose pixels a
        // Lock/Present reads back. Lets gpu_bridge fire its draw/submit summary only at a real frame
        // completion (a render-target Lock), not on every vertex/index-buffer Lock.
        bool is_render_target(uint64_t resource) const;

        // Diagnostic (EMULATOR_D3D9_RTDIAG): one human-readable line per render-target resource,
        // describing its declaration AND a format-aware summary of the pixels currently in it, read
        // back from the GPU. Every other counter this host exposes says how many draws reached a
        // target; none says whether what landed there is usable data. An intermediate render target
        // (shadow map, depth prepass, downsampled bloom chain) that is fully drawn but holds a
        // degenerate constant -- all far-plane, all zero, all NaN -- looks identical to a correct one
        // in draws_per_render_target, and produces a plausible-but-wrong final image rather than an
        // obviously broken one. This is the only way to tell those apart.
        //
        // Deliberately expensive: it forces a full GPU->host readback of every render target, so it is
        // called only under the env var, never on a normal frame.
        std::vector<std::string> describe_render_targets();

        // Diagnostic: the full census of D3D9 pipeline state the guest has set -- every value each
        // render state has ever been given (from draw_stats::render_state_values, passed in) plus the
        // current value of every sampler state. The counters above can only report what this host DID;
        // a title rendering wrong pixels with every one of them clean is usually asking for a piece of
        // pipeline state this host silently drops on the floor, and a state this host never reads is
        // invisible everywhere else. Printing the value SET rather than the current value matters: most
        // states are toggled per draw, so the current value is just whatever the frame's last (usually
        // 2D/HUD) draw left behind, and a state a title turns on for exactly its 3D geometry -- which is
        // the interesting case -- reads as permanently off there.
        std::string describe_pipeline_state(const std::map<uint32_t, std::map<uint32_t, uint64_t>>& render_state_values) const;

        // Diagnostic (Task #144, sRGB-haze investigation): the CURRENT value of PS float constant
        // registers c0 and c34 -- MW2's dominant world-material shader (ps68508) reads these as a fog
        // colour pair (see the shader dump), and the open question is whether the game uploads them
        // already linear (consistent with its own manual sRGB-decode-via-squaring of every sampled
        // texture) or in raw display/gamma space (which would blend wrong against the shader's linear
        // lighting math). Values only, no interpretation -- read the actual numbers to tell.
        std::string describe_ps_fog_constants() const;

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
            // Real, persistent, HOST_VISIBLE|HOST_COHERENT GPU buffer backing this resource directly --
            // only ever set for eligible D3DUSAGE_DYNAMIC/D3DPOOL_DEFAULT vertex/index buffers (see
            // create_resource's eligibility check). When set, `backing` above is still correctly sized
            // (existing `.empty()` validity guards elsewhere never inspect its bytes for buffer-kind
            // resources, only its size) but is never written to -- Lock/Unlock and execute_draw's
            // reservation logic all read/write this buffer's mapped memory directly instead, eliminating
            // both the CPU-side backing copy AND, when execute_draw binds it directly, the per-draw arena
            // upload memmove entirely. 0 = no direct buffer; falls back to the ordinary `backing`-based
            // path used by every other resource kind.
            uint64_t vk_direct_buffer_id{};
            uint64_t vk_direct_memory_id{};
            void* direct_mapped_ptr{};
            // The direct buffer is a ring of direct_slice_count slices of direct_slice_stride bytes each,
            // of which direct_slice_offset names the live one. A guest D3DLOCK_DISCARD renames to the next
            // slice (DXVK's D3D9CommonBuffer::DiscardMapSlice) instead of overwriting bytes an already-
            // recorded draw may still read, and announces it with a batched d3d9_set_direct_slice so the
            // switch replays in order between the draws around it. Stays 0/1/1 for a resource whose guest
            // VA alias failed, where every access still funnels through the host Lock/Unlock path.
            uint32_t direct_slice_stride{};
            uint32_t direct_slice_count{};
            uint32_t direct_slice_offset{};
            uint64_t vk_image_id{}; // 0 = no GPU backing (plain buffer); set for render targets and textures
            // Device memory bound to vk_image_id, owned by this entry -- set only for the sampled-texture
            // path, which allocates the image's memory itself. A render target's image and memory are both
            // owned by vulkan_host's own render_target_data (create_render_target), released through
            // destroy_render_target instead, so this stays 0 for those.
            uint64_t vk_image_memory_id{};
            uint64_t vk_image_view_id{}; // 0 until first drawn to; lazily created, cached per resource
            // Second colour-attachment view of the SAME image, through the format's _SRGB counterpart,
            // used only while D3DRS_SRGBWRITEENABLE is set (see execute_draw). Vulkan performs the
            // linear->sRGB encode on writes through an sRGB-formatted attachment view, so this one extra
            // view is the whole mechanism. Stays 0 for formats with no sRGB counterpart, and is never
            // used for SAMPLING -- a render target's stored bytes are read back exactly as they were
            // written, which is what the guest's own later passes expect.
            uint64_t vk_image_view_srgb_id{};
            bool backing_dirty{}; // color RT: GPU image has drawn/cleared pixels not yet copied to backing

            // Sampled texture: the CPU-side backing has bytes the GPU image does not have yet, so the next
            // ensure_texture_uploaded must do a real staging upload. The inverse of backing_dirty above,
            // which tracks the render-target direction (GPU -> CPU). Defaults to true so a texture that
            // was never uploaded still gets its first upload; cleared only by a fully successful upload,
            // so a failed/refused upload can never leave a stale image marked clean. Set by every writer
            // of a sampled texture's backing store -- unlock() (the Lock/Unlock DDI write-back) and
            // tex_blt() (UpdateTexture's whole-surface copy). One flag covers all subresources because
            // ensure_texture_uploaded is all-or-nothing: it re-uploads every mip level/cube face in a
            // single staging buffer, so per-subresource granularity would buy nothing.
            bool upload_dirty{true};

            // Monotonic counter bumped every time this resource's backing bytes are mutated (currently the
            // sole site is unlock()'s write-back memcpy -- see its own comment). execute_draw's vertex/index
            // upload cache keys on {resource id, content_version, batch_generation} so a cached arena offset
            // is only ever reused when the bytes it was uploaded from are still exactly what `backing` holds
            // now. Starts at 0 and is never reset, including across destroy_resource -- irrelevant, since
            // resource ids (allocate_id()) are never reused either, so a stale cache entry can never alias a
            // different, newer resource.
            uint64_t content_version{};

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

        // Fixed slot order for the six D3D9 constant-register UBOs. device_state::const_versions,
        // ubo_staging_, ubo_scratch_, ubo_built_version_ and ubo_upload_cache_ are all indexed by it, as
        // are execute_draw's own per-draw arena offsets and descriptor writes.
        enum ubo_index : size_t
        {
            ubo_vs_f = 0,
            ubo_ps_f = 1,
            ubo_vs_i = 2,
            ubo_ps_i = 3,
            ubo_vs_b = 4,
            ubo_ps_b = 5,
            ubo_slot_count = 6,
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
            std::unordered_map<uint64_t, uint32_t> texture_stage_state{}; // key = (stage << 32) | state
            std::unordered_map<uint64_t, uint32_t> sampler_state{};       // key = (sampler << 32) | state
            std::unordered_map<uint32_t, uint64_t> bound_textures{};      // key = stage
            std::unordered_map<uint32_t, uint64_t> stream_sources{};      // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_strides{};      // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_offsets{};      // key = stream_number
            std::unordered_map<uint32_t, uint32_t> stream_frequencies{};  // key = stream_number
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
            // Bumped by every SetVertex/PixelShaderConstant* handler that writes the matching vector
            // above. A live MW2 gameplay profile measured the six build_ubo_staging calls at 1.59us of
            // execute_draw's 2.52us reserve phase -- ~26KB of zero-fill, memcpy and byte-compare per
            // draw -- while MW2 rewrites at most one or two of the six between consecutive draws, so
            // execute_draw uses these to skip rebuilding a slot whose source is untouched.
            std::array<uint64_t, ubo_slot_count> const_versions{};
            // Same version-stamp-and-skip shape as const_versions above, bumped by every SetSamplerState
            // handler write. build_sampler otherwise re-reads ten separate sampler_state entries and
            // re-hashes a ten-field cache key on every draw, for every bound texture stage, to reach the
            // same VkSampler it reached last time -- MW2 changes sampler state far more rarely than it
            // draws. One counter covering every stage, since a stage-granular version would cost the same
            // map lookups it exists to avoid.
            uint64_t sampler_state_version{};
            std::array<uint64_t, 4> render_targets{};
            uint64_t depth_stencil{};
            // Last SetScissorRect rect (RECT semantics -- exclusive right/bottom); only consulted at
            // draw time when D3DRS_SCISSORTESTENABLE is set (see execute_draw).
            int32_t scissor_left{};
            int32_t scissor_top{};
            int32_t scissor_right{};
            int32_t scissor_bottom{};
            // Last SetViewport/SetZRange values (D3DVIEWPORT9 semantics: X/Y/Width/Height in the same
            // top-left-origin screen space as the scissor rect above). width/height default to 0, which
            // execute_draw reads as "no explicit viewport yet" and falls back to the full render-target
            // extent -- see its viewport-transform derivation.
            float viewport_x{};
            float viewport_y{};
            float viewport_width{};
            float viewport_height{};
            float viewport_min_z{0.0f};
            float viewport_max_z{1.0f};
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

        // batch_command_buffer_/batch_fence_/frame_descriptor_pool_ (below) and vertex_index_uniform_arena_
        // (further below) are each double-buffered, batch_slot_count-wide, rather than a single reused
        // object. The old single-object design forced flush_batch() to submit AND block on
        // vkWaitForFences before the next batch could safely reopen -- CPU-writing new vertex/index/UBO
        // bytes into vertex_index_uniform_arena_.memory, or vkBeginCommandBuffer-ing
        // batch_command_buffer_ again, while the GPU might still be reading the PREVIOUS batch's use of
        // that exact same buffer/command-buffer/descriptor-pool is a real WAW/use-after-free hazard, not
        // just a validation nicety -- so the wait could never be skipped. With batch_slot_count copies,
        // reopening a batch can round-robin to the OTHER slot and defer that wait until this slot is
        // about to be reused again (wait_for_batch_slot, called from execute_draw's batch-management
        // step) -- by then the GPU has typically already finished it, so the wait is usually free. Live
        // profiling (2026-08-22 session) found the batch's descriptor-pool-exhaustion reopen (every
        // frame_desc_initial_draws draws into the same render target -- the common case for a single
        // scene render) as the dominant real-world trigger of this reopen path. Render-target and
        // depth-stencil changes take the same round-robin path: their DDI handlers only update state,
        // leaving execute_draw's own batch_rt_/batch_ds_ mismatch guard to close and rotate the batch.
        //
        // Every OTHER caller of flush_batch() (destroy_resource, tex_blt, sync_backing_from_gpu,
        // color_fill, blt, ensure_depth_stencil_view's first-use init, ...) needs the batch's GPU
        // work OBSERVABLY FINISHED before it proceeds -- it reads back pixels, destroys the Vulkan objects
        // the batch referenced, or must be ordered relative to a batch that might still be sitting
        // unsubmitted. flush_batch() stays a full barrier for them: it still submits whatever is
        // currently open, then waits on EVERY slot with a pending (submitted-but-unwaited) fence, not just
        // the current one -- because by the time flush_batch() runs, an EARLIER draw's batch-management
        // step may have already async-submitted a DIFFERENT slot's batch without waiting for it (that is
        // the whole point of the round-robin). Only execute_draw's own internal reopen decision is allowed
        // to defer a wait; every external call site keeps its original "GPU is fully idle after this call"
        // contract.
        static constexpr uint32_t batch_slot_count = 2;

        // A separate, dedicated command buffer/fence pair per slot for the batched-draw recording, so
        // neither collides with the shared command_buffer_/fence_ that the prep helpers, color_fill, and
        // blt submit+wait on synchronously above.
        std::array<uint64_t, batch_slot_count> batch_command_buffer_{};
        std::array<uint64_t, batch_slot_count> batch_fence_{};
        // Set by submit_batch_async() right after it submits slot i's batch (before any wait), cleared by
        // wait_for_batch_slot(i) right after that wait completes. A slot with this false has no
        // outstanding GPU work referencing its command buffer/descriptor pool/arena -- either it was never
        // used, or a prior wait already drained it -- so it's immediately safe to reset/rewrite.
        std::array<bool, batch_slot_count> batch_slot_pending_{};

        // color_fill's batched cmd_copy_buffer_to_image (see its own comment) needs its host-visible
        // staging buffer held alive on the GPU until this slot's submission actually completes --
        // destroyed by wait_for_batch_slot right after its wait, the same point every other per-slot GPU
        // resource (arena, descriptor pool) becomes safe to reuse/destroy.
        struct pending_staging_buffer
        {
            uint64_t device{};
            uint64_t buffer{};
            uint64_t memory{};
        };

        std::array<std::vector<pending_staging_buffer>, batch_slot_count> pending_staging_cleanup_{};
        // Which slot the CURRENTLY open batch (if any) is recording into; also the last slot used when no
        // batch is open. Advanced (round-robin) only by execute_draw's batch-management step, and only for
        // the reopen reasons that don't themselves require this exact slot back immediately (see that
        // function's own comment).
        uint32_t batch_slot_{0};
        bool batch_open_{false};
        // Render target (slot-0 handle) the currently-open batch records into; 0 = none. A draw whose
        // slot-0 render target differs flushes the batch first (execute_draw), so a batch never mixes
        // render targets.
        uint64_t batch_rt_{};
        // Depth-stencil resource handle the currently-open batch records into; 0 = none (colour-only
        // batch). A draw whose bound depth-stencil differs flushes the batch first (execute_draw), because
        // the inter-draw depth barrier execute_draw emits only synchronizes this one image.
        uint64_t batch_ds_{};
        // Draws recorded into the currently-open batch that consumed a descriptor-set pair (programmable
        // draws). Reset to 0 on batch open; when the next draw would exceed frame_desc_capacity_draws_ the
        // batch is flushed first so the pool can be reset (see execute_draw's overflow guard).
        uint32_t batch_draw_count_{};

        // Bumped once every time a new batch opens (execute_draw's "if (!this->batch_open_)" block) --
        // i.e. after every real flush_batch() (RT/depth-stencil change, descriptor-pool exhaustion, or
        // arena growth) as well as the very first batch. arena_.offset resets to 0 whenever a batch
        // (re)opens, so an arena offset cached from a previous batch generation would otherwise alias
        // whatever a later batch has since written at that same offset -- the vertex/index upload cache
        // below (see upload_cache_entry) includes this counter in its match key specifically to rule that
        // out, on top of the {resource id, content_version} check that already covers content changes
        // within the SAME still-open batch (e.g. a Lock/Unlock of a dynamic vertex buffer between draws).
        uint64_t batch_generation_{0};

        // One entry: {resource id, content_version, batch_generation} -> arena_ offset the resource's
        // bytes were last uploaded to. A hit on all three fields means the bytes currently in
        // resource_entry::backing are bit-identical to what's already sitting at arena_offset in the
        // still-open batch's arena, so execute_draw can bind that offset directly and skip the
        // upload_memory memcpy entirely (Task #161: this upload was measured at 19% of all wall-clock
        // time in a live MW2 profile, with zero dirty-tracking -- every draw re-uploaded every bound
        // vertex stream and the index buffer even when byte-identical to the prior draw's). resource_id
        // == 0 is the "never populated" / "not cacheable" sentinel -- real resource ids from allocate_id()
        // start at 0x10000, and a DrawPrimitiveUP/DrawIndexedPrimitiveUP's inline UM-backed bytes have no
        // resource id at all, so those are always cache misses (a fresh reservation + upload every draw,
        // exactly like today).
        struct upload_cache_entry
        {
            uint64_t resource_id{};
            uint64_t content_version{};
            uint64_t batch_generation{};
            size_t arena_offset{};
            // Byte range (relative to the resource's own backing, not the arena) that was actually
            // uploaded -- a draw needing a range outside [range_start, range_end) is a miss even if the
            // other fields match, since bytes outside that range were never written into the arena slice.
            size_t range_start{};
            size_t range_end{};
            bool valid{false};
        };

        // Indexed directly by D3D9 stream number (execute_draw's `used_binding_mask` is a 32-bit mask, so
        // every stream index it ever iterates is < 32).
        std::array<upload_cache_entry, 32> stream_upload_cache_{};
        upload_cache_entry index_upload_cache_{};

        // Same idea as upload_cache_entry above, but for the six per-draw constant-register UBOs (see
        // ubo_staging_ below): these have no D3D9 resource id/content_version to key off, so the cache key
        // is instead "did build_ubo_staging detect a byte-for-byte content change since the value currently
        // sitting in ubo_staging_ (which IS the last-uploaded content -- see its own comment), still within
        // the same batch_generation_". A live MW2 profile found the int/bool constant UBOs are ~100% byte-
        // identical across consecutive draws and the pixel-shader float UBO ~95% identical, so skipping the
        // arena reservation + upload_memory + descriptor write for those is a real, common-case win.
        struct ubo_upload_cache_entry
        {
            uint64_t batch_generation{};
            size_t arena_offset{};
            bool valid{false};
        };

        std::array<ubo_upload_cache_entry, 6> ubo_upload_cache_{};
        // Scratch buffer build_ubo_staging fills the CANDIDATE content into before comparing against
        // ubo_staging_'s current (= last-uploaded) content -- ubo_staging_ itself is only overwritten when
        // the candidate actually differs, so it doubles as the "last known uploaded content" snapshot the
        // cache-hit check compares fresh candidates against.
        std::array<std::vector<std::byte>, 6> ubo_scratch_{};
        // device_state::const_versions value ubo_staging_[slot]'s current content was built from. Equal
        // versions mean the source registers haven't been written since, so the content cannot differ and
        // the whole build+compare is skipped. A slot whose ubo_staging_ is still empty (first draw, or a
        // size change) always rebuilds regardless, so no "never built" sentinel is needed.
        std::array<uint64_t, 6> ubo_built_version_{};

        // Identity of the dynamic-rendering instance (if any) currently left open on a slot's batch
        // command buffer, spanning zero or more already-recorded draws. execute_draw reopens a fresh
        // instance (its own cmd_begin_rendering/cmd_end_rendering pair, plus the color-attachment layout
        // round trip) only when the next draw's own attachments don't match this exactly -- otherwise it
        // just keeps recording into the same instance. Every render-to-texture draw (one that samples a
        // render target as a texture) always closes any open instance first and never leaves one open
        // itself, so this never has to reason about that case; see close_render_pass's caller in
        // execute_draw for why. color_image_ids/color_view_ids are 0 for a gap slot (D3D9 RT slot with no
        // resolvable resource), matching rt_slots' own "0 = unbound" convention; depth_image_id is 0 for
        // a colour-only instance.
        struct open_render_pass_state
        {
            bool open{false};
            std::array<uint64_t, 4> color_image_ids{};
            std::array<uint64_t, 4> color_view_ids{};
            size_t color_count{0};
            uint64_t depth_image_id{0};

            // Colour render targets this instance transitioned out of the TRANSFER_SRC_OPTIMAL resting
            // layout so its draws could sample them (render-to-texture), in the order execute_draw
            // collected them. They stay in SHADER_READ_ONLY_OPTIMAL for as long as the instance lives, and
            // close_render_pass restores them; a draw whose own sampled set differs from this one cannot
            // share the instance, since the difference is exactly a set of layout transitions that Vulkan
            // only allows outside a rendering instance.
            std::vector<uint64_t> sampled_image_ids{};
        };

        std::array<open_render_pass_state, batch_slot_count> open_render_pass_{};

        // A Clear() whose color/depth-stencil target has no render-pass instance open yet in this
        // batch (open_render_pass_[slot].open == false, i.e. nothing has been drawn into it since the
        // batch's last flush) defers its value here instead of paying for a standalone
        // vkCmdClear*Image: the next render-pass-begin for this slot -- execute_draw's own fresh-
        // instance branch -- consumes it as that attachment's VK_ATTACHMENT_LOAD_OP_CLEAR value, which
        // is free (the attachment's tile memory is initialized to the clear value as part of beginning
        // to render into it regardless). color/depth are independent: one aspect can take the fast path
        // while the other falls back (e.g. a D3DCLEAR_STENCIL against a real stencil format always
        // falls back, since cmd_begin_rendering never wires up a separate stencil attachment). Reset to
        // {} whenever a fresh render-pass instance consumes it, and whenever a batch (re)opens on this
        // slot (same places open_render_pass_[slot] itself is reset) -- and realized via the ordinary
        // explicit-clear path by submit_batch_async if the batch is submitted before any draw ever
        // consumes it, so a Clear() is never silently dropped.
        struct pending_batch_clear
        {
            bool color_pending{false};
            std::array<float, 4> color_value{};
            // device_state::render_targets as it stood when the Clear() deferred its value here. The
            // realization sites (submit_batch_async's explicit-clear fallback, execute_draw's
            // LOAD_OP_CLEAR fold) run arbitrarily later, by which point the app may have rebound a render
            // target -- resolving the clear against the CURRENT bindings would land it on a surface the
            // Clear() never named. A slot-0 or depth-stencil rebind rotates the batch (submit_batch_async
            // realizes the clear against this snapshot first), so only an MRT slot-1..3 rebind can
            // actually reach a realization site with a mismatched set; that case is detected by comparing
            // this against the live bindings and realized explicitly instead of folded.
            std::array<uint64_t, 4> color_targets{};
            bool depth_pending{false};
            float depth_value{1.0f};
            uint32_t stencil_value{0};
        };

        std::array<pending_batch_clear, batch_slot_count> pending_clear_{};

        // Process-lifetime instrumentation, never reset: draw_count_ increments once per execute_draw
        // call, batch_submit_count_ once per real flush_batch() submit (an open batch actually
        // ended+submitted+waited, not a no-op flush). gpu_bridge logs both at each frame-completion point
        // so batching's draws-per-submit ratio is directly observable. Distinct from batch_draw_count_
        // above, which is a per-batch descriptor-pool bookkeeping counter that resets every batch.
        uint64_t draw_count_{};
        uint64_t batch_submit_count_{};

        // Same never-reset, process-lifetime convention as the two counters above, but classifying WHAT
        // each draw/clear actually did rather than just how many there were. draw_count_ alone cannot
        // distinguish "2856 draws produced pixels" from "2856 draws were all dropped before recording" or
        // "all recorded but every fragment failed its depth test" -- three states that look identical in
        // the frame line and are the first fork any black-output investigation has to take. Incremented
        // unconditionally (a predictable-branch increment per draw, immaterial next to the Vulkan work in
        // the same function); only the logging of them is env-gated, in gpu_bridge.
        draw_stats stats_{};

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
            vertex_input_shape vertex_shape{};           // decl identity + per-stream strides the build reads (see vertex_shape_key())
            vulkan_host::depth_state depth{};            // resolved static depth test/write/compare (build_depth_state)
            vulkan_host::color_blend_attachment blend{}; // resolved static blend enable/factors/write-mask (build_blend_state)
            uint32_t depth_clip_enable{1};               // D3DRS_CLIPPING, baked into depthClampEnable (default TRUE = clip on)
            uint32_t cull_mode{2};                       // D3DRS_CULLMODE, resolved VkCullModeFlags (default: BACK_BIT)
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

        // Negative half of programmable_pipelines_: keys whose pipeline the driver already refused to
        // build. Without it a permanently-unbuildable pipeline is retried on EVERY draw that wants it,
        // and the retry is not cheap -- it is the full vkd3d-shader SM3->SPIR-V translation of the VS/PS
        // pair, two fresh VkShaderModules (which is where MoltenVK does its SPIRV-Cross->MSL conversion
        // and Metal compile), two descriptor-set layouts, a pipeline layout, then the rejected
        // vkCreateGraphicsPipelines, then destroying all of it again. MW2 in "The Pit" hit this ~848
        // times per frame against a handful of distinct shader pairs.
        //
        // Sound because every input to the build is fixed by the key: the VS/PS token blobs are
        // immutable once created (no update DDI) and their ids are never reused, the vertex shape and
        // attachment formats are in the key, and the resolved depth/blend state is in the key. Nothing
        // outside the key can turn a refusal into an acceptance. The one exception is driver resource
        // exhaustion, which is transient rather than a property of the key -- see remember_pipeline_
        // failure() for how those are excluded so they keep retrying.
        std::set<pipeline_cache_key> failed_pipelines_{};

        // Records `key` as unbuildable (unless the failure was transient) and returns nullptr, so
        // ensure_programmable_pipeline's failure paths can `return this->remember_pipeline_failure(...)`.
        const programmable_pipeline_entry* remember_pipeline_failure(const pipeline_cache_key& key, int32_t vk_result);

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

        // Per-stage shortcut past the whole of build_sampler: with the D3D9 sampler state untouched since
        // this stage last resolved a sampler, and the bound texture's mip count unchanged, the ten
        // sampler_state lookups and the sampler_cache_ probe can only reach the same VkSampler again.
        // Indexed with the pixel stages first and the vertex ones after, since D3D9 numbers vertex
        // samplers from D3DVERTEXTEXTURESAMPLER0 (257) rather than continuing the pixel range. Nothing
        // invalidates an entry other than the version stamp: a cached VkSampler lives for the
        // device's lifetime (see sampler_cache_ above).
        struct sampler_memo_entry
        {
            uint64_t sampler_state_version{};
            uint32_t mip_levels{};
            uint64_t sampler{};
        };

        std::array<sampler_memo_entry, max_ps_sampler_stages + max_vs_sampler_stages> sampler_memo_{};

        // One GPU buffer per batch slot (see batch_slot_count's comment above), each backing every
        // vertex/index/uniform range a draw in THAT slot's batch needs. Each range is a distinct
        // 256-byte-aligned slice handed out by arena_suballoc; a slot's buffer is created lazily and grown
        // grow-only across draws, independently of the other slot's. execute_draw resets a slot's offset
        // to 0 only when (re)opening a batch on it, after wait_for_batch_slot has proven that slot's prior
        // GPU work is done -- so a later draw rewriting it can never race a still-in-flight read of the
        // same bytes by an earlier batch on the same slot. Combined VERTEX|INDEX|UNIFORM usage so one
        // buffer serves all three binding points.
        std::array<frame_arena, batch_slot_count> vertex_index_uniform_arena_{};

        // One shared descriptor pool per batch slot that every programmable draw in that slot's batch
        // allocates its per-draw VS/PS descriptor-set pair from, replacing the old per-pipeline pool that
        // pre-allocated exactly two reused sets. Sized for frame_desc_capacity_draws_[slot] draws (2 sets
        // each), created lazily per slot and grown grow-only. Like the arena, execute_draw resets a slot's
        // pool (reset_descriptor_pool) only when (re)opening a batch on it, after wait_for_batch_slot has
        // proven that slot's prior GPU work is done -- so a reset can never free a set an in-flight draw on
        // the SAME slot is still reading. Per-draw descriptor-type counts (6 UBOs + max_vs_sampler_stages +
        // max_ps_sampler_stages combined-image-samplers) are scaled by the capacity; see
        // ensure_frame_descriptor_pool.
        std::array<uint64_t, batch_slot_count> frame_descriptor_pool_{};
        // How many draws' worth of sets each slot's pool is currently sized for.
        std::array<uint32_t, batch_slot_count> frame_desc_capacity_draws_{};
        // Initial per-frame descriptor-pool capacity, in draws. One draw needs only 2 sets, so 256 is
        // ample headroom -- the growth path in ensure_frame_descriptor_pool exists for correctness, not
        // because a single draw is expected to exceed it.
        //
        // 2026-08-24: tried raising this to 4096 to reduce how often the descriptor-pool-exhaustion trigger
        // in execute_draw forces a mid-frame render-pass restart (see close_render_pass) on the same render
        // target. First live A/B found FPS MUCH WORSE (~2.8-3.4 FPS vs. a ~6.8-7 FPS baseline at 256).
        // Built direct timing diagnostics for both vkResetDescriptorPool (EMULATOR_D3D9_RESETPOOL_DIAG) and
        // vkAllocateDescriptorSets (EMULATOR_D3D9_ALLOCSET_DIAG, both left in the tree) and measured their
        // real combined cost is negligible either way (~0.4-0.5% of wall-clock time) -- nowhere near enough
        // to explain that regression's magnitude even under generous scaling assumptions, so that's NOT the
        // mechanism. A clean re-test at 4096 with both diagnostics active then showed FPS statistically
        // matching the 256 baseline (~7.0-7.7 FPS) with LOWER combined descriptor-management overhead
        // (fewer reopens needed) and reopen_desc_exhaustion=0 throughout, exactly as designed. A THIRD test
        // (meant as a second confirmation) reproduced a severe drop again (~2-4 FPS) -- but this run had
        // confirmed, real concurrent host contention at the time (the same unrelated
        // wt-solidworks-bringup worktree's analyzer process plus a `cargo` build both actively running,
        // verified via `ps`), which was also present during the original "regression" run. Both severe-drop
        // observations correlate with detected external contention; the one clean, uncontaminated run at
        // 4096 was FPS-neutral. This makes external contention the leading explanation for both drops, but
        // it was not proven with a fully controlled (guaranteed-quiet-throughout, not just checked
        // periodically) A/B, so the fix was NOT re-applied -- reverted back to 256 as the safe, extensively
        // verified default. Before retrying 4096, either get a genuinely quiet host for the entire test
        // window, or use the two diagnostics above alongside FPS to catch any real per-call cost increase
        // directly rather than inferring causation from FPS alone.
        static constexpr uint32_t frame_desc_initial_draws = 256;

        // Reused scratch buffers for the six per-draw constant-register UBOs execute_draw stages into the
        // arena (vs/ps float, vs/ps int, vs/ps bool). Each has a fixed, draw-independent size (the D3D9
        // constant-register caps), so a fresh std::vector per draw bought nothing but allocator churn --
        // these are resized once (on first use) and then just overwritten in place every draw. Plain CPU
        // scratch memory, never touched by the GPU directly: each execute_draw call fills it, then
        // synchronously upload_memory()s it into that call's arena slot before returning, so there is
        // nothing here for a later draw to race -- unlike the arena/descriptor pool below, this one is not
        // slot-indexed.
        std::array<std::vector<std::byte>, 6> ubo_staging_{};

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
        // Ensures frame_descriptor_pool_[slot] exists and is sized for at least needed_draws draws' worth
        // of descriptor sets (2 per draw). Creates it lazily at frame_desc_initial_draws capacity, or
        // doubles and recreates it (dropping the old pool) when needed_draws exceeds the current capacity.
        // Only called (with needed_draws == 1) on the batch slot execute_draw has already settled on for
        // THIS draw, past the point a reopen would have waited for that slot's own pending fence -- so
        // dropping the old pool here is safe on the same grounds as the reopen path's own reset. Returns
        // false only on a Vulkan allocation failure.
        bool ensure_frame_descriptor_pool(uint64_t device, uint32_t slot, uint32_t needed_draws);
        // Lazily creates a bare Vulkan instance/device on vulkan_ (first render-target-kind resource).
        // Returns 0 on failure.
        uint64_t ensure_vk_device();
        bool ensure_draw_infra();
        // Ends and submits the currently open batch's command buffer (batch_open_ = false), WITHOUT
        // waiting for it -- marks batch_slot_pending_[batch_slot_] so a later reuse of this exact slot
        // knows to wait first (see wait_for_batch_slot). A no-op when no batch is open. The only caller
        // that may skip the immediate wait a real flush needs is execute_draw's own batch-management step,
        // which deliberately defers it to get CPU/GPU overlap across the round-robin slots (see
        // batch_slot_count's comment) -- every other caller must use flush_batch() below instead.
        void submit_batch_async();
        // Records a deferred colour Clear() as an explicit vkCmdClearColorImage against the render-target
        // set it was queued against (pending_batch_clear::color_targets), and clears the pending flag. A
        // no-op when nothing is pending. The caller must have no dynamic-rendering instance open on the
        // current batch slot -- vkCmdClearColorImage is illegal inside one -- which holds by construction
        // wherever a colour clear can be pending (see pending_clear_'s own comment).
        void realize_pending_color_clear(pending_batch_clear& pending);
        // Ends slot `slot`'s currently open dynamic-rendering instance (if any -- a no-op otherwise) and
        // restores every colour attachment it used to the TRANSFER_SRC_OPTIMAL resting layout the rest of
        // this host relies on between draws. Must run before that slot's command buffer is ended
        // (submit_batch_async) and before execute_draw opens a differently-shaped instance on it.
        void close_render_pass(uint32_t slot);
        // Blocks until batch slot `slot`'s most recently submitted batch (if any) has completed, then
        // clears batch_slot_pending_[slot]. A no-op when that slot has no outstanding submission -- either
        // it was never used, or an earlier wait already drained it.
        void wait_for_batch_slot(uint32_t slot);
        // Full barrier: submits whatever batch is currently open (submit_batch_async), then waits on EVERY
        // slot that still has a pending (submitted-but-unwaited) fence, not just the current one -- an
        // earlier draw's batch-management step may have async-submitted a DIFFERENT slot without waiting
        // for it, so "current slot only" would not actually guarantee this host's GPU work is done. Called
        // at every boundary that must observe ALL of this host's outstanding GPU work before proceeding
        // (readback, color_fill, blt, resource teardown) -- every one of those needs this full-barrier
        // contract, unlike execute_draw's own internal reopen decision (see submit_batch_async). The
        // D3D9 clear and render-target/depth-stencil handlers deliberately do NOT use this: none of them
        // returns data to the CPU or destroys anything the batch references, so a plain
        // submit-and-rotate on the next draw gives them the ordering they need without the wait.
        void flush_batch();
        // Opens a batch recording into target_rt/target_ds's identity, or keeps the currently open one if
        // it already matches -- the same round-robin slot-selection execute_draw's own batch-management
        // step performs (submit+rotate+wait_for_batch_slot on an identity mismatch, reset the new slot's
        // arena/descriptor-pool/render-pass state on open), factored out so a Clear that arrives with no
        // batch open (or a mismatched one) can join the SAME deferred-wait mechanism as draws instead of
        // its own synchronous submit+wait. Unlike execute_draw's inline version, this has no arena-growth
        // or descriptor-pool-exhaustion trigger -- a bare clear consumes neither.
        void ensure_batch_open(uint64_t device, uint64_t target_rt, uint64_t target_ds);
        // Records a full-image color clear into the currently open batch's command buffer (batch_slot_),
        // instead of vulkan_host::submit_clear's standalone one-shot-submit-then-wait. Caller must have
        // already called ensure_batch_open and closed any open render-pass instance on that slot first
        // (vkCmdClearColorImage is illegal inside one). Leaves the image at its TRANSFER_SRC_OPTIMAL
        // resting layout, matching submit_clear's own documented post-state.
        void batch_clear_color_image(uint64_t image, const std::array<float, 4>& color);
        // Same idea as batch_clear_color_image, but for the currently-bound depth-stencil resource: records
        // a barrier/clear/barrier sequence onto the open batch's command buffer instead of a standalone
        // one-shot submit+wait. Caller must have already run ensure_depth_stencil_view (so the image is
        // resting in DEPTH_STENCIL_ATTACHMENT_OPTIMAL) and closed any open render-pass instance on
        // batch_slot_ first.
        void batch_clear_depth_stencil_image(resource_entry& ds_entry, uint32_t depth_format, uint32_t clear_aspects, float depth,
                                             uint32_t stencil);
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
            // The app-declared vertex range this draw touches (D3D9's DrawIndexedPrimitive MinIndex/
            // NumVertices) -- used to bound how much of a resource-backed vertex stream actually needs
            // uploading, independent of the index buffer's own [first_index, first_index+index_count)
            // bound above.
            uint32_t min_vertex_index;
            uint32_t num_vertices;
        };

        int32_t execute_draw(uint32_t vertex_count, uint32_t first_vertex, const indexed_draw* indexed = nullptr);
    };
} // namespace sogen
