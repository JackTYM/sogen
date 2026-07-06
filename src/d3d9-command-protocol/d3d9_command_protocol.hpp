#pragma once

// Wire protocol for the D3D9 UMD <-> host d3d9_host bridge.
//
// Transport is reused verbatim from gpu_bridge_protocol.hpp: sync commands (one request/response
// pair) travel as a single NtGdiDdDDIEscape carrying an escape_command_header; the high-churn
// per-draw state stream is recorded into command_record_header{command,size} entries and flushed
// as one ioctl_record_commands escape (see gpu_bridge_protocol.hpp's own comment on that opcode).
// This header only adds the D3D9-specific payload structs; the opcode numbers themselves live in
// gpu_bridge::command's 0x900-0x9FF block so both command families dispatch through the exact same
// gpu_bridge/gdi.cpp Escape and record-replay machinery with no new transport code.

#include "../gpu-bridge-protocol/gpu_bridge_protocol.hpp"

#include <cstdint>

namespace sogen::d3d9_cmd
{
    using resource_id = gpu_bridge::object_id;
    inline constexpr resource_id null_resource = gpu_bridge::null_object;

    // ---------------------------------------------------------------------------------------------
    // Sync commands (one Escape request/response pair each)
    // ---------------------------------------------------------------------------------------------

    // ioctl_d3d9_marker: debug/bring-up breadcrumb, no payload beyond the stage id.
    enum class marker_stage : uint32_t
    {
        open_adapter = 0,
        create_device = 1,
    };

    struct marker_request
    {
        uint32_t stage; // marker_stage
        uint32_t reserved;
    };

    // What kind of D3DDDI_CREATERESOURCE payload create_resource_request describes. Render targets
    // and depth-stencil surfaces are still created through the existing kernel
    // pfnAllocateCb -> D3DKMTCreateAllocation path (so they land in dxgk_state.allocations with a
    // vk_image_id, reusing handle_NtGdiDdDDIPresent unchanged) -- this command is only for resources
    // the UMD marshals directly to the host: plain textures, vertex buffers, index buffers.
    enum class resource_kind : uint32_t
    {
        texture_2d = 0,
        texture_cube = 1,
        texture_volume = 2,
        vertex_buffer = 3,
        index_buffer = 4,
    };

    struct create_resource_request
    {
        uint32_t kind;       // resource_kind
        uint32_t format;     // D3DFORMAT (0 for buffers)
        uint32_t width;      // byte size for buffers
        uint32_t height;     // volume/array depth for texture_volume; unused for buffers
        uint32_t depth;
        uint32_t mip_levels;
        uint32_t usage; // D3DUSAGE_* bits
        uint32_t pool;  // D3DPOOL_*
    };

    struct create_resource_response
    {
        int32_t hr;
        uint32_t reserved;
        resource_id resource; // null_resource on failure
    };

    struct destroy_resource_request
    {
        resource_id resource;
    };

    // ioctl_d3d9_tex_blt: the real d3d9.dll issues pfnTexBlt to sync a D3DPOOL_MANAGED texture's
    // sysmem "master" copy (dst of LockRect/UnlockRect) into its lazily created vidmem copy (dst of
    // SetTexture) on first bind -- see sogen_d3d9_umd.cpp's umd_TexBlt for the live-RE trail. Only the
    // two resource ids matter for this milestone (every texture this bring-up creates is single-mip,
    // whole-image, so a full-backing copy is always correct); the real DDI's rect/point/subresource
    // fields are intentionally not modeled.
    struct tex_blt_request
    {
        resource_id dst_resource;
        resource_id src_resource;
    };

    // ioctl_d3d9_lock: out header immediately followed by data_size bytes of the locked region.
    struct lock_request
    {
        resource_id resource;
        uint32_t subresource;
        uint32_t offset;
        uint32_t size; // 0 = whole resource
        uint32_t flags; // D3DLOCK_* bits
        uint32_t reserved;
    };

    struct lock_response
    {
        int32_t hr;
        uint32_t data_size;
        // uint8_t data[data_size];
    };

    // ioctl_d3d9_unlock: in header immediately followed by data_size bytes to write back. data_size
    // is 0 for a resource that was locked read-only and never written.
    struct unlock_request
    {
        resource_id resource;
        uint32_t subresource;
        uint32_t offset;
        uint32_t data_size;
        // uint8_t data[data_size];
    };

    // ioctl_d3d9_create_vertex_shader / ioctl_d3d9_create_pixel_shader: in header immediately
    // followed by token_size_bytes bytes of raw SM1-3 shader tokens (the DDI's pFunction blob).
    struct create_shader_request
    {
        uint32_t token_size_bytes;
        uint32_t reserved;
        // uint32_t tokens[token_size_bytes / 4];
    };

    struct create_shader_response
    {
        int32_t hr;
        uint32_t reserved;
        resource_id shader;
    };

    // Mirrors D3DVERTEXELEMENT9's real 8-byte layout exactly, so the guest can memcpy its own
    // D3DVERTEXELEMENT9 array as the trailing payload.
    struct vertex_element
    {
        uint16_t stream;
        uint16_t offset;
        uint8_t type;   // D3DDECLTYPE
        uint8_t method;  // D3DDECLMETHOD
        uint8_t usage;   // D3DDECLUSAGE
        uint8_t usage_index;
    };

    static_assert(sizeof(vertex_element) == 8, "must match D3DVERTEXELEMENT9 wire layout");

    // ioctl_d3d9_create_vertex_decl: in header immediately followed by element_count vertex_element
    // entries (the D3DDECL_END sentinel is not sent).
    struct create_vertex_decl_request
    {
        uint32_t element_count;
        uint32_t reserved;
        // vertex_element elements[element_count];
    };

    struct create_vertex_decl_response
    {
        int32_t hr;
        uint32_t reserved;
        resource_id decl;
    };

    // ioctl_d3d9_present: the real D3DDDIARG_PRESENT carries no HWND (RE-confirmed; the runtime
    // presents through a separate, driver-opaque kernel path), so the host resolves which window to
    // show this in on its own (see gpu_bridge.cpp's handler) -- this request only identifies which
    // resource's current pixels to display.
    struct present_request
    {
        resource_id resource;
    };

    struct present_response
    {
        int32_t hr;
        uint32_t reserved;
    };

    // ---------------------------------------------------------------------------------------------
    // Streamed commands: recorded per-device via gpu_bridge::command_record_header{command,size} and
    // flushed as one ioctl_record_commands escape on pfnFlush/pfnPresent. Each struct below is that
    // record's payload; `command` in the header is the matching gpu_bridge::command 0x900-block value.
    // ---------------------------------------------------------------------------------------------

    struct set_render_state_record
    {
        uint32_t state; // D3DRENDERSTATETYPE
        uint32_t value;
    };

    struct set_texture_stage_state_record
    {
        uint32_t stage; // 0-7
        uint32_t state; // D3DTEXTURESTAGESTATETYPE
        uint32_t value;
        uint32_t reserved;
    };

    struct set_sampler_state_record
    {
        uint32_t sampler; // 0-15
        uint32_t state;   // D3DSAMPLERSTATETYPE
        uint32_t value;
        uint32_t reserved;
    };

    struct set_texture_record
    {
        uint32_t stage;
        uint32_t reserved;
        resource_id texture; // null_resource unbinds
    };

    struct set_stream_source_record
    {
        uint32_t stream_number;
        uint32_t offset_bytes;
        uint32_t stride_bytes;
        uint32_t reserved;
        resource_id vertex_buffer; // null_resource unbinds
    };

    struct set_stream_source_freq_record
    {
        uint32_t stream_number;
        uint32_t frequency;
    };

    struct set_indices_record
    {
        resource_id index_buffer; // null_resource unbinds
        uint32_t format;          // 0 = 16-bit indices, 1 = 32-bit indices
        uint32_t reserved;
    };

    struct set_vertex_decl_record
    {
        resource_id decl;
    };

    struct set_vertex_shader_record
    {
        resource_id shader; // null_resource selects the fixed-function pipeline
    };

    struct set_pixel_shader_record
    {
        resource_id shader; // null_resource selects the fixed-function pipeline
    };

    // set_vs_const_f / set_ps_const_f: header immediately followed by vector4_count*4 floats.
    struct set_const_f_record
    {
        uint32_t start_register;
        uint32_t vector4_count;
        // float values[vector4_count * 4];
    };

    // set_vs_const_i / set_ps_const_i: header immediately followed by vector4_count*4 int32s.
    struct set_const_i_record
    {
        uint32_t start_register;
        uint32_t vector4_count;
        // int32_t values[vector4_count * 4];
    };

    // set_vs_const_b / set_ps_const_b: header immediately followed by count BOOLs (4 bytes each,
    // matching the D3D9 DDI's own BOOL width).
    struct set_const_b_record
    {
        uint32_t start_register;
        uint32_t count;
        // uint32_t values[count];
    };

    struct set_render_target_record
    {
        uint32_t render_target_index; // 0-3
        uint32_t reserved;
        resource_id surface; // null_resource unbinds
    };

    struct set_depth_stencil_record
    {
        resource_id surface; // null_resource = no depth-stencil
    };

    // Matches D3DVIEWPORT9's field order/widths.
    struct set_viewport_record
    {
        float x;
        float y;
        float width;
        float height;
        float min_z;
        float max_z;
    };

    // Matches RECT's field order/widths.
    struct set_scissor_record
    {
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    };

    // header immediately followed by rect_count set_scissor_record-shaped rects (RECT layout);
    // rect_count == 0 clears the whole bound render target/depth-stencil.
    struct clear_record
    {
        uint32_t flags; // D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL
        uint32_t color_argb;
        float z;
        uint32_t stencil;
        uint32_t rect_count;
        // set_scissor_record rects[rect_count];
    };

    struct draw_primitive_record
    {
        uint32_t primitive_type; // D3DPRIMITIVETYPE
        uint32_t start_vertex;
        uint32_t primitive_count;
        uint32_t reserved;
    };

    struct draw_indexed_primitive_record
    {
        uint32_t primitive_type;
        int32_t base_vertex_index;
        uint32_t min_vertex_index;
        uint32_t num_vertices;
        uint32_t start_index;
        uint32_t primitive_count;
    };

    // set_stream_source_um: the DrawPrimitiveUP/DrawIndexedPrimitiveUP user-memory vertex source. Real
    // d3d9.dll does NOT use a dedicated "UP draw" DDI; it binds the user vertex array via
    // pfnSetStreamSourceUm (device-func-table slot 7) and then reuses the ordinary pfnDrawPrimitive/
    // pfnDrawIndexedPrimitive slot -- so this only transports the stream binding, with the user vertex
    // bytes copied inline (they live in app memory, not a buffer). The header is immediately followed by
    // vertex_data_size bytes of inline vertex data. offset_bytes is always 0 on the UP path but kept for
    // symmetry with set_stream_source_record. Receipt marks the stream UM-backed (see d3d9_host.cpp).
    struct set_stream_source_um_record
    {
        uint32_t stream_number;
        uint32_t stride_bytes;
        uint32_t offset_bytes;
        uint32_t vertex_data_size;
        // uint8_t vertex_data[vertex_data_size];
    };

    // set_indices_um: the DrawIndexedPrimitiveUP user-memory index source, bound by real d3d9.dll via
    // pfnSetIndicesUm (device-func-table slot 9). index_element_size is the raw byte width (2 = 16-bit,
    // 4 = 32-bit indices). The header is immediately followed by index_data_size bytes of inline index
    // data. Receipt marks the index source UM-backed (see d3d9_host.cpp).
    struct set_indices_um_record
    {
        uint32_t index_element_size;
        uint32_t index_data_size;
        // uint8_t index_data[index_data_size];
    };

    // color_fill: IDirect3DDevice9::ColorFill -> real d3d9.dll pfnColorFill (device-func-table slot 56).
    // Fills DstRect of the resolved resource's subresource with a single D3DCOLOR (ARGB). The rect is in
    // D3D9 RECT convention (right/bottom exclusive); see sogen_d3d9_umd.cpp's umd_ColorFill and
    // D3DDDIARG_COLORFILL in d3d9_ddi.hpp for the RE trail.
    struct color_fill_record
    {
        resource_id resource;
        uint32_t subresource;
        uint32_t color_argb; // D3DCOLOR (0xAARRGGBB)
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    };

    // blt: IDirect3DDevice9::StretchRect -> real d3d9.dll pfnBlt (device-func-table slot 55). Blits
    // SrcRect of src_resource into DstRect of dst_resource; when the two rects differ in size the host's
    // vkCmdBlitImage scales (filter selects nearest/linear). D3D9 RECT convention (right/bottom
    // exclusive). See sogen_d3d9_umd.cpp's umd_Blt and D3DDDIARG_BLT in d3d9_ddi.hpp for the RE trail.
    struct blt_record
    {
        resource_id dst_resource;
        resource_id src_resource;
        uint32_t dst_subresource;
        uint32_t src_subresource;
        int32_t dst_left;
        int32_t dst_top;
        int32_t dst_right;
        int32_t dst_bottom;
        int32_t src_left;
        int32_t src_top;
        int32_t src_right;
        int32_t src_bottom;
        uint32_t filter; // D3DTEXTUREFILTERTYPE (0 = NONE/POINT, 2 = LINEAR)
        uint32_t reserved;
    };

    // Portability guard, same rationale as gpu_bridge_protocol.hpp's own asserts: every struct here
    // uses only fixed-width integers/floats and resource_id (a uint64), never size_t/pointers, so a
    // 32-bit WoW64 guest and a 64-bit host agree on layout byte-for-byte.
    static_assert(sizeof(marker_request) == 8, "wire layout drift");
    static_assert(sizeof(create_resource_request) == 32, "wire layout drift");
    static_assert(sizeof(create_resource_response) == 16, "wire layout drift");
    static_assert(sizeof(tex_blt_request) == 16, "wire layout drift");
    static_assert(sizeof(lock_request) == 32, "wire layout drift");
    static_assert(sizeof(lock_response) == 8, "wire layout drift");
    static_assert(sizeof(unlock_request) == 24, "wire layout drift");
    static_assert(sizeof(create_shader_request) == 8, "wire layout drift");
    static_assert(sizeof(create_shader_response) == 16, "wire layout drift");
    static_assert(sizeof(create_vertex_decl_request) == 8, "wire layout drift");
    static_assert(sizeof(create_vertex_decl_response) == 16, "wire layout drift");
    static_assert(sizeof(set_render_state_record) == 8, "wire layout drift");
    static_assert(sizeof(set_texture_stage_state_record) == 16, "wire layout drift");
    static_assert(sizeof(set_sampler_state_record) == 16, "wire layout drift");
    static_assert(sizeof(set_texture_record) == 16, "wire layout drift");
    static_assert(sizeof(set_stream_source_record) == 24, "wire layout drift");
    static_assert(sizeof(set_stream_source_freq_record) == 8, "wire layout drift");
    static_assert(sizeof(set_indices_record) == 16, "wire layout drift");
    static_assert(sizeof(set_vertex_decl_record) == 8, "wire layout drift");
    static_assert(sizeof(set_vertex_shader_record) == 8, "wire layout drift");
    static_assert(sizeof(set_pixel_shader_record) == 8, "wire layout drift");
    static_assert(sizeof(set_const_f_record) == 8, "wire layout drift");
    static_assert(sizeof(set_const_i_record) == 8, "wire layout drift");
    static_assert(sizeof(set_const_b_record) == 8, "wire layout drift");
    static_assert(sizeof(set_render_target_record) == 16, "wire layout drift");
    static_assert(sizeof(set_depth_stencil_record) == 8, "wire layout drift");
    static_assert(sizeof(set_viewport_record) == 24, "wire layout drift");
    static_assert(sizeof(set_scissor_record) == 16, "wire layout drift");
    static_assert(sizeof(clear_record) == 20, "wire layout drift");
    static_assert(sizeof(draw_primitive_record) == 16, "wire layout drift");
    static_assert(sizeof(draw_indexed_primitive_record) == 24, "wire layout drift");
    static_assert(sizeof(set_stream_source_um_record) == 16, "wire layout drift");
    static_assert(sizeof(set_indices_um_record) == 8, "wire layout drift");
    static_assert(sizeof(color_fill_record) == 32, "wire layout drift");
    static_assert(sizeof(blt_record) == 64, "wire layout drift");

} // namespace sogen::d3d9_cmd
