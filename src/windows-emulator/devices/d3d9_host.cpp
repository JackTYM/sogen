#include "d3d9_host.hpp"

#include "d3d9_format.hpp"
#include "d3d9_shader_translator.hpp"

#include <d3d9_command_protocol.hpp>

// Real Vulkan enum/type values (VK_FORMAT_*, VK_PRIMITIVE_TOPOLOGY_*, ...), used only for their
// stable, public numeric constants -- vulkan_host's own API surface stays plain-integer (see its
// header comment); this .cpp mirrors that same "real Vulkan types stay out of the header" rule.
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>

namespace sogen
{
    namespace
    {
        constexpr int32_t d3derr_invalidcall = -2005530516; // D3DERR_INVALIDCALL
        constexpr int32_t d3d_ok = 0;

        // Public, ABI-stable D3D9 API constants (d3d9types.h), not RE'd DDI internals.
        constexpr uint32_t d3dusage_rendertarget = 0x00000001;
        constexpr uint32_t d3dusage_depthstencil = 0x00000002;
        constexpr uint32_t d3dusage_dynamic = 0x00000200;
        constexpr uint32_t d3dpool_default = 0;

        // Whether D3DUSAGE_DEPTHSTENCIL resources get memoryless-attachment (MoltenVK "transient") GPU
        // backing -- see create_resource's own comment for how this is used, and vulkan_host::
        // create_render_target's `transient` parameter for what it does at the Vulkan level.
        //
        // Left DISABLED (false): a real, likely-frequent correctness hazard was found while wiring this
        // up. execute_draw's batch-management step force-submits and reopens the current render-pass
        // instance roughly every 256 programmable draws targeting the SAME render target/depth-stencil,
        // for the entire life of the process (ensure_frame_descriptor_pool is always called with
        // needed_draws=1, so frame_desc_capacity_draws_ never grows past frame_desc_initial_draws=256;
        // see that call site) -- a trigger with no accompanying D3D9 Clear() call. A transient depth-
        // stencil's store_op is unconditionally VK_ATTACHMENT_STORE_OP_DONT_CARE (the whole point of the
        // optimization -- see execute_draw's depth_attachment), so the depth content accumulated by every
        // draw before that reopen is discarded, and the reopened instance's VK_ATTACHMENT_LOAD_OP_LOAD
        // then reads undefined content instead of it -- not a hypothetical edge case but the FIRST scene
        // in any real workload (this was written for MW2) that draws more than 256 depth-tested
        // primitives into one target in a frame, which is unremarkable for a real 3D game. The rest of
        // this feature (vulkan_host::create_render_target's transient path, the DONT_CARE store_op, the
        // Clear()-fast-path-is-mandatory-for-transient handling) is implemented and exercised by the
        // d3d9-depthclip-test/d3d9-mrt-test correctness tests with this flag flipped true, but is kept
        // off by default until the reopen-without-Clear hazard above has a real fix (e.g. growing
        // frame_desc_capacity_draws_ so the reopen becomes rare instead of recurring, and/or reasoning
        // about whether that's sufficient rather than just less likely).
        constexpr bool depth_stencil_transient_enabled = false;

        // Temporary diagnostic (EMULATOR_D3D9_PRESENT_TIMING=<frames-per-report>, default 60): the
        // native-UMD Present path is a full synchronous GPU drain followed by a CPU readback of the
        // whole backbuffer, where DXVK instead hands the backbuffer to a real asynchronous swapchain
        // and only ever blocks the calling thread on a fence three frames old. This splits one
        // Present into the three costs that model would remove -- draining the open batch, the
        // readback submit plus its fence wait, and the backing-vector copy handed to the UI backend
        // -- against the wall-clock interval between Presents, so the drain's real share of a frame
        // is measured rather than assumed.
        struct present_timing_totals
        {
            double flush_ms;
            double readback_ms;
            double copy_ms;
            double interval_ms;
            uint64_t frames;
            std::chrono::steady_clock::time_point last_present;
        };

        bool present_timing_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_PRESENT_TIMING") != nullptr;
            return enabled;
        }

        present_timing_totals& present_timing()
        {
            static present_timing_totals totals{};
            return totals;
        }

        double elapsed_ms(const std::chrono::steady_clock::time_point from, const std::chrono::steady_clock::time_point to)
        {
            return std::chrono::duration<double, std::milli>(to - from).count();
        }

        // Raw-bytes encoding rendering_attachment::clear_value expects (see vulkan_host::cmd_begin_
        // rendering's build() -- a memcpy straight into VkClearValue's union). Defined this early
        // because submit_batch_async (below) needs depth_stencil_clear_value for its pending-clear
        // safety net.
        std::array<uint32_t, 4> color_clear_value(const std::array<float, 4>& color)
        {
            return {std::bit_cast<uint32_t>(color[0]), std::bit_cast<uint32_t>(color[1]), std::bit_cast<uint32_t>(color[2]),
                    std::bit_cast<uint32_t>(color[3])};
        }

        std::array<uint32_t, 4> depth_stencil_clear_value(const float depth, const uint32_t stencil)
        {
            return {std::bit_cast<uint32_t>(depth), stencil, 0, 0};
        }

        // Public D3DSAMPLERSTATETYPE values (d3d9types.h) -- the wire protocol's set_sampler_state_record
        // carries these directly (see the UMD's sampler_state_for_ddi_tss_state, which translates the real
        // DDI-level D3DDDITEXTURESTAGESTATETYPE encoding into these before it ever reaches the host).
        constexpr uint32_t d3dsamp_addressu = 1;
        constexpr uint32_t d3dsamp_addressv = 2;
        constexpr uint32_t d3dsamp_addressw = 3;
        constexpr uint32_t d3dsamp_magfilter = 5;
        constexpr uint32_t d3dsamp_minfilter = 6;
        constexpr uint32_t d3dsamp_mipfilter = 7;
        constexpr uint32_t d3dsamp_maxmiplevel = 9;
        constexpr uint32_t d3dsamp_maxanisotropy = 10;

        // Public D3DSAMPLER_TEXTURE_TYPE base (d3d9types.h): D3D9 folds the vertex-stage texture samplers
        // into the SAME SetTexture/bound_textures stage-key space as the pixel samplers, starting at
        // D3DVERTEXTEXTURESAMPLER0 == 257 (D3DDMAPSAMPLER + 1). Real d3d9.dll passes these stage numbers
        // through the DDI unmodified, so bound_textures[257 + k] is the texture the guest bound to vertex
        // sampler s{k}. execute_draw maps that back to the VS's own shader register k (0..3). Confirmed by
        // umd_SetTexture's RE-verified direct-value-argument signature (sogen_d3d9_umd.cpp -- Stage is
        // forwarded verbatim with no special-casing by value) and empirically by
        // d3d9_vertex_texture_test.cpp's passing SetTexture(D3DVERTEXTEXTURESAMPLER0) result.
        constexpr uint32_t d3dvertextexturesampler0 = 257;

        // Public D3DRENDERSTATETYPE values (d3d9types.h) needed for real depth-test wiring.
        constexpr uint32_t d3drs_zenable = 7;
        constexpr uint32_t d3drs_zwriteenable = 14;
        constexpr uint32_t d3drs_zfunc = 23;

        // Public D3DCMPFUNC values (d3d9types.h), D3DRS_ZFUNC's value space.
        constexpr uint32_t d3dcmp_never = 1;
        constexpr uint32_t d3dcmp_less = 2;
        constexpr uint32_t d3dcmp_equal = 3;
        constexpr uint32_t d3dcmp_lessequal = 4;
        constexpr uint32_t d3dcmp_greater = 5;
        constexpr uint32_t d3dcmp_notequal = 6;
        constexpr uint32_t d3dcmp_greaterequal = 7;
        constexpr uint32_t d3dcmp_always = 8;

        // Public D3DRENDERSTATETYPE values (d3d9types.h) needed for real alpha-blend wiring.
        constexpr uint32_t d3drs_srcblend = 19;
        constexpr uint32_t d3drs_destblend = 20;
        constexpr uint32_t d3drs_alphablendenable = 27;
        constexpr uint32_t d3drs_blendop = 171;

        // Public D3DRENDERSTATETYPE value (d3d9types.h) needed for real scissor-test wiring.
        constexpr uint32_t d3drs_scissortestenable = 174;
        constexpr uint32_t d3drs_srgbwriteenable = 194;

        // Public D3DRENDERSTATETYPE value (d3d9types.h) needed for real backface-cull wiring.
        constexpr uint32_t d3drs_cullmode = 22;

        // Public D3DCULL values (d3d9types.h), D3DRS_CULLMODE's value space.
        constexpr uint32_t d3dcull_none = 1;
        constexpr uint32_t d3dcull_cw = 2;
        constexpr uint32_t d3dcull_ccw = 3;

        // Public D3DRENDERSTATETYPE value (d3d9types.h). Default TRUE (clipping on); FALSE tells the driver
        // the geometry is inside the guard band and skips clipping, incl. near/far depth clipping -- which
        // maps to VkPipelineRasterizationStateCreateInfo::depthClampEnable (clamp instead of clip in Z).
        constexpr uint32_t d3drs_clipping = 136;

        // Public D3DBLEND values (d3d9types.h), D3DRS_SRCBLEND/DESTBLEND's value space.
        constexpr uint32_t d3dblend_zero = 1;
        constexpr uint32_t d3dblend_one = 2;
        constexpr uint32_t d3dblend_srccolor = 3;
        constexpr uint32_t d3dblend_invsrccolor = 4;
        constexpr uint32_t d3dblend_srcalpha = 5;
        constexpr uint32_t d3dblend_invsrcalpha = 6;
        constexpr uint32_t d3dblend_destalpha = 7;
        constexpr uint32_t d3dblend_invdestalpha = 8;
        constexpr uint32_t d3dblend_destcolor = 9;
        constexpr uint32_t d3dblend_invdestcolor = 10;
        constexpr uint32_t d3dblend_srcalphasat = 11;
        constexpr uint32_t d3dblend_blendfactor = 14;
        constexpr uint32_t d3dblend_invblendfactor = 15;

        // D3DSTREAMSOURCE_* flags (d3d9types.h): the high 2 bits of a SetStreamSourceFreq divider select
        // the stream's instancing role; the low 30 bits carry the instance count (INDEXEDDATA) or the
        // per-instance advance divider (INSTANCEDATA). A divider with neither flag is an ordinary
        // per-vertex stream. These are the raw values the guest UMD forwards untouched (see
        // umd_SetStreamSourceFreq) and this host stores in state_.stream_frequencies.
        constexpr uint32_t d3dstreamsource_indexeddata = 1u << 30;
        constexpr uint32_t d3dstreamsource_instancedata = 2u << 30;
        constexpr uint32_t d3dstreamsource_freq_mask = (1u << 30) - 1u; // low 30 bits: count/divider

        // Public D3DBLENDOP values (d3d9types.h), D3DRS_BLENDOP's value space.
        constexpr uint32_t d3dblendop_add = 1;
        constexpr uint32_t d3dblendop_subtract = 2;
        constexpr uint32_t d3dblendop_revsubtract = 3;
        constexpr uint32_t d3dblendop_min = 4;
        constexpr uint32_t d3dblendop_max = 5;

        // Minimal fixed-function passthrough shader pair for D3DFVF_XYZRHW|D3DFVF_DIFFUSE (see
        // devices/shaders/ff_triangle.{vert,frag} for the GLSL source this was compiled from with
        // glslangValidator). Not a placeholder for missing vkd3d-shader translation -- Vulkan has no
        // true fixed-function pipeline either way, so this hardcoded pair is the correct, permanent
        // implementation for this one case; general FVF/render-state shader synthesis is the separate,
        // future M4 milestone.
        // clang-format off
        constexpr std::array<uint32_t, 367> k_ff_vertex_shader_spirv = {
            0x07230203u, 0x00010000u, 0x0008000bu, 0x00000034u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
            0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
            0x0009000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000cu, 0x00000022u, 0x00000031u,
            0x00000032u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
            0x00030005u, 0x00000009u, 0x0063646eu, 0x00060005u, 0x0000000cu, 0x6f506e69u, 0x69746973u, 0x68526e6fu,
            0x00000077u, 0x00060005u, 0x0000000fu, 0x68737550u, 0x736e6f43u, 0x746e6174u, 0x00000073u, 0x00070006u,
            0x0000000fu, 0x00000000u, 0x77656976u, 0x74726f70u, 0x657a6953u, 0x00000000u, 0x00030005u, 0x00000011u,
            0x00006370u, 0x00060005u, 0x00000020u, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u,
            0x00000020u, 0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000020u, 0x00000001u,
            0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x00000020u, 0x00000002u, 0x435f6c67u,
            0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u, 0x00000020u, 0x00000003u, 0x435f6c67u, 0x446c6c75u,
            0x61747369u, 0x0065636eu, 0x00030005u, 0x00000022u, 0x00000000u, 0x00050005u, 0x00000031u, 0x67617266u,
            0x6f6c6f43u, 0x00000072u, 0x00040005u, 0x00000032u, 0x6f436e69u, 0x00726f6cu, 0x00040047u, 0x0000000cu,
            0x0000001eu, 0x00000000u, 0x00030047u, 0x0000000fu, 0x00000002u, 0x00050048u, 0x0000000fu, 0x00000000u,
            0x00000023u, 0x00000000u, 0x00030047u, 0x00000020u, 0x00000002u, 0x00050048u, 0x00000020u, 0x00000000u,
            0x0000000bu, 0x00000000u, 0x00050048u, 0x00000020u, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u,
            0x00000020u, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x00000020u, 0x00000003u, 0x0000000bu,
            0x00000004u, 0x00040047u, 0x00000031u, 0x0000001eu, 0x00000000u, 0x00040047u, 0x00000032u, 0x0000001eu,
            0x00000001u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u,
            0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000002u, 0x00040020u, 0x00000008u, 0x00000007u,
            0x00000007u, 0x00040017u, 0x0000000au, 0x00000006u, 0x00000004u, 0x00040020u, 0x0000000bu, 0x00000001u,
            0x0000000au, 0x0004003bu, 0x0000000bu, 0x0000000cu, 0x00000001u, 0x0003001eu, 0x0000000fu, 0x00000007u,
            0x00040020u, 0x00000010u, 0x00000009u, 0x0000000fu, 0x0004003bu, 0x00000010u, 0x00000011u, 0x00000009u,
            0x00040015u, 0x00000012u, 0x00000020u, 0x00000001u, 0x0004002bu, 0x00000012u, 0x00000013u, 0x00000000u,
            0x00040020u, 0x00000014u, 0x00000009u, 0x00000007u, 0x0004002bu, 0x00000006u, 0x00000018u, 0x40000000u,
            0x0004002bu, 0x00000006u, 0x0000001au, 0x3f800000u, 0x00040015u, 0x0000001du, 0x00000020u, 0x00000000u,
            0x0004002bu, 0x0000001du, 0x0000001eu, 0x00000001u, 0x0004001cu, 0x0000001fu, 0x00000006u, 0x0000001eu,
            0x0006001eu, 0x00000020u, 0x0000000au, 0x00000006u, 0x0000001fu, 0x0000001fu, 0x00040020u, 0x00000021u,
            0x00000003u, 0x00000020u, 0x0004003bu, 0x00000021u, 0x00000022u, 0x00000003u, 0x0004002bu, 0x0000001du,
            0x00000023u, 0x00000000u, 0x00040020u, 0x00000024u, 0x00000007u, 0x00000006u, 0x0004002bu, 0x0000001du,
            0x0000002au, 0x00000002u, 0x00040020u, 0x0000002bu, 0x00000001u, 0x00000006u, 0x00040020u, 0x0000002fu,
            0x00000003u, 0x0000000au, 0x0004003bu, 0x0000002fu, 0x00000031u, 0x00000003u, 0x0004003bu, 0x0000000bu,
            0x00000032u, 0x00000001u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
            0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000007u, 0x0004003du, 0x0000000au, 0x0000000du,
            0x0000000cu, 0x0007004fu, 0x00000007u, 0x0000000eu, 0x0000000du, 0x0000000du, 0x00000000u, 0x00000001u,
            0x00050041u, 0x00000014u, 0x00000015u, 0x00000011u, 0x00000013u, 0x0004003du, 0x00000007u, 0x00000016u,
            0x00000015u, 0x00050088u, 0x00000007u, 0x00000017u, 0x0000000eu, 0x00000016u, 0x0005008eu, 0x00000007u,
            0x00000019u, 0x00000017u, 0x00000018u, 0x00050050u, 0x00000007u, 0x0000001bu, 0x0000001au, 0x0000001au,
            0x00050083u, 0x00000007u, 0x0000001cu, 0x00000019u, 0x0000001bu, 0x0003003eu, 0x00000009u, 0x0000001cu,
            0x00050041u, 0x00000024u, 0x00000025u, 0x00000009u, 0x00000023u, 0x0004003du, 0x00000006u, 0x00000026u,
            0x00000025u, 0x00050041u, 0x00000024u, 0x00000027u, 0x00000009u, 0x0000001eu, 0x0004003du, 0x00000006u,
            0x00000028u, 0x00000027u, 0x0004007fu, 0x00000006u, 0x00000029u, 0x00000028u, 0x00050041u, 0x0000002bu,
            0x0000002cu, 0x0000000cu, 0x0000002au, 0x0004003du, 0x00000006u, 0x0000002du, 0x0000002cu, 0x00070050u,
            0x0000000au, 0x0000002eu, 0x00000026u, 0x00000029u, 0x0000002du, 0x0000001au, 0x00050041u, 0x0000002fu,
            0x00000030u, 0x00000022u, 0x00000013u, 0x0003003eu, 0x00000030u, 0x0000002eu, 0x0004003du, 0x0000000au,
            0x00000033u, 0x00000032u, 0x0003003eu, 0x00000031u, 0x00000033u, 0x000100fdu, 0x00010038u,
        };

        constexpr std::array<uint32_t, 95> k_ff_fragment_shader_spirv = {
            0x07230203u, 0x00010000u, 0x0008000bu, 0x0000000du, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
            0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
            0x0007000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x0000000bu, 0x00030010u,
            0x00000004u, 0x00000007u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du,
            0x00000000u, 0x00050005u, 0x00000009u, 0x4374756fu, 0x726f6c6fu, 0x00000000u, 0x00050005u, 0x0000000bu,
            0x67617266u, 0x6f6c6f43u, 0x00000072u, 0x00040047u, 0x00000009u, 0x0000001eu, 0x00000000u, 0x00040047u,
            0x0000000bu, 0x0000001eu, 0x00000000u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u,
            0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u,
            0x00000008u, 0x00000003u, 0x00000007u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000003u, 0x00040020u,
            0x0000000au, 0x00000001u, 0x00000007u, 0x0004003bu, 0x0000000au, 0x0000000bu, 0x00000001u, 0x00050036u,
            0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003du, 0x00000007u,
            0x0000000cu, 0x0000000bu, 0x0003003eu, 0x00000009u, 0x0000000cu, 0x000100fdu, 0x00010038u,
        };
        // clang-format on

        uint64_t tss_key(const uint32_t stage, const uint32_t state)
        {
            return (uint64_t{stage} << 32) | state;
        }

        // Reads a fixed-size request struct from the front of payload/size, false if too short.
        template <typename Request>
        bool read_record(const std::byte* payload, const size_t size, Request& out)
        {
            if (size < sizeof(Request))
            {
                return false;
            }
            std::memcpy(&out, payload, sizeof(Request));
            return true;
        }

        uint32_t render_state_or(const std::unordered_map<uint32_t, uint32_t>& render_state, const uint32_t state,
                                 const uint32_t default_value)
        {
            const auto it = render_state.find(state);
            return it != render_state.end() ? it->second : default_value;
        }

        // D3DCMPFUNC -> VkCompareOp (direct 1:1 correspondence; D3DCMP_* is 1-based, VK_COMPARE_OP_* is
        // 0-based, hence NEVER != NEVER).
        uint32_t d3dcmp_to_vk_compare_op(const uint32_t d3dcmp)
        {
            switch (d3dcmp)
            {
            case d3dcmp_never:
                return VK_COMPARE_OP_NEVER;
            case d3dcmp_less:
                return VK_COMPARE_OP_LESS;
            case d3dcmp_equal:
                return VK_COMPARE_OP_EQUAL;
            case d3dcmp_lessequal:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case d3dcmp_greater:
                return VK_COMPARE_OP_GREATER;
            case d3dcmp_notequal:
                return VK_COMPARE_OP_NOT_EQUAL;
            case d3dcmp_greaterequal:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case d3dcmp_always:
                return VK_COMPARE_OP_ALWAYS;
            default:
                return VK_COMPARE_OP_LESS_OR_EQUAL; // D3D9's own documented default for D3DRS_ZFUNC
            }
        }

        // Builds the pipeline's depth-test state from the app's accumulated render state. depth_format ==
        // 0 (no bound depth-stencil with real GPU backing) always yields test_enable == 0, matching this
        // function's pre-depth-testing behavior exactly for every draw that never binds one.
        vulkan_host::depth_state build_depth_state(const std::unordered_map<uint32_t, uint32_t>& render_state, const uint32_t depth_format)
        {
            if (depth_format == 0 || render_state_or(render_state, d3drs_zenable, 0) == 0)
            {
                return {.test_enable = 0, .write_enable = 0, .compare_op = 0};
            }
            return {.test_enable = 1,
                    .write_enable = render_state_or(render_state, d3drs_zwriteenable, 1) != 0 ? 1u : 0u,
                    .compare_op = d3dcmp_to_vk_compare_op(render_state_or(render_state, d3drs_zfunc, d3dcmp_lessequal))};
        }

        // D3DBLEND -> VkBlendFactor. D3DBLEND_BOTHSRCALPHA/BOTHINVSRCALPHA (legacy D3D3-era modes that
        // imply an asymmetric src/dst pair from a single value) and the dual-source D3DBLEND_SRCCOLOR2/
        // INVSRCCOLOR2 have no single-factor Vulkan equivalent -- out of scope for this minimum-viable
        // slice, so they fall through to the same default as any other unrecognized value.
        uint32_t d3dblend_to_vk_blend_factor(const uint32_t d3dblend)
        {
            switch (d3dblend)
            {
            case d3dblend_zero:
                return VK_BLEND_FACTOR_ZERO;
            case d3dblend_one:
                return VK_BLEND_FACTOR_ONE;
            case d3dblend_srccolor:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case d3dblend_invsrccolor:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case d3dblend_srcalpha:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case d3dblend_invsrcalpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case d3dblend_destalpha:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case d3dblend_invdestalpha:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case d3dblend_destcolor:
                return VK_BLEND_FACTOR_DST_COLOR;
            case d3dblend_invdestcolor:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case d3dblend_srcalphasat:
                return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case d3dblend_blendfactor:
                return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case d3dblend_invblendfactor:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            default:
                return VK_BLEND_FACTOR_ONE; // D3D9's own documented default for D3DRS_SRCBLEND
            }
        }

        // D3DBLENDOP -> VkBlendOp (direct 1:1 correspondence; D3DBLENDOP_* is 1-based, VK_BLEND_OP_* is
        // 0-based, hence ADD != ADD).
        uint32_t d3dblendop_to_vk_blend_op(const uint32_t d3dblendop)
        {
            switch (d3dblendop)
            {
            case d3dblendop_add:
                return VK_BLEND_OP_ADD;
            case d3dblendop_subtract:
                return VK_BLEND_OP_SUBTRACT;
            case d3dblendop_revsubtract:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case d3dblendop_min:
                return VK_BLEND_OP_MIN;
            case d3dblendop_max:
                return VK_BLEND_OP_MAX;
            default:
                return VK_BLEND_OP_ADD; // D3D9's own documented default for D3DRS_BLENDOP
            }
        }

        // Every pipeline this UMD builds bakes frontFace to this fixed value regardless of D3DRS_CULLMODE.
        // execute_draw's viewport is {y = H, height = -H} (commit 7215d2d2) to match D3D9's clip-space Y
        // convention; a negative-height viewport is a pure Y-reflection, and reflecting one axis negates
        // the signed area every triangle's winding is computed from, universally. That un-mirroring is
        // exactly what makes Vulkan's window-space coordinates now equal D3D9's own screen-space
        // coordinates for the same draw, so "visually clockwise" means the same thing to both rasterizers
        // with no further correction -- D3D9's documented default (D3DCULL_CCW) culls back faces with CCW
        // vertices, i.e. its front faces are CW, hence CLOCKWISE here.
        constexpr uint32_t k_umd_front_face = VK_FRONT_FACE_CLOCKWISE;

        // D3DCULL -> VkCullModeFlags, paired with the fixed frontFace above. D3DCULL_CW/CCW name which
        // winding D3D9 treats as the back face to cull, not which winding Vulkan should cull directly --
        // CCW (the default, also this function's fallback) is back under CLOCKWISE-front, so it maps to
        // BACK_BIT; CW inverts that, so it culls the front-designated winding instead (FRONT_BIT).
        uint32_t d3dcull_to_vk_cull_mode(const uint32_t d3dcull)
        {
            switch (d3dcull)
            {
            case d3dcull_none:
                return VK_CULL_MODE_NONE;
            case d3dcull_cw:
                return VK_CULL_MODE_FRONT_BIT;
            case d3dcull_ccw:
                return VK_CULL_MODE_BACK_BIT;
            default:
                return VK_CULL_MODE_BACK_BIT; // D3D9's own documented default for D3DRS_CULLMODE
            }
        }

        // Builds the pipeline's color-blend state from the app's accumulated render state.
        // D3DRS_ALPHABLENDENABLE unset/0 always yields the pre-blending all-zero state (blend_enable=0,
        // every factor/op 0), matching this function's pre-blending behavior exactly for every draw that
        // never enables blending -- this is every existing test. The same (non-separate) color factors
        // are reused for the alpha channel, matching D3D9's own default when D3DRS_SEPARATEALPHABLENDENABLE
        // isn't set.
        vulkan_host::color_blend_attachment build_blend_state(const std::unordered_map<uint32_t, uint32_t>& render_state)
        {
            if (render_state_or(render_state, d3drs_alphablendenable, 0) == 0)
            {
                return {.blend_enable = 0,
                        .src_color_blend_factor = 0,
                        .dst_color_blend_factor = 0,
                        .color_blend_op = 0,
                        .src_alpha_blend_factor = 0,
                        .dst_alpha_blend_factor = 0,
                        .alpha_blend_op = 0,
                        .color_write_mask = 0xF};
            }
            const uint32_t src_factor = d3dblend_to_vk_blend_factor(render_state_or(render_state, d3drs_srcblend, d3dblend_one));
            const uint32_t dst_factor = d3dblend_to_vk_blend_factor(render_state_or(render_state, d3drs_destblend, d3dblend_zero));
            const uint32_t blend_op = d3dblendop_to_vk_blend_op(render_state_or(render_state, d3drs_blendop, d3dblendop_add));
            return {.blend_enable = 1,
                    .src_color_blend_factor = src_factor,
                    .dst_color_blend_factor = dst_factor,
                    .color_blend_op = blend_op,
                    .src_alpha_blend_factor = src_factor,
                    .dst_alpha_blend_factor = dst_factor,
                    .alpha_blend_op = blend_op,
                    .color_write_mask = 0xF};
        }
    } // namespace

    uint64_t d3d9_host::allocate_id()
    {
        // See next_id_'s own comment (d3d9_host.hpp) for why this must never reach 2^32 -- it's
        // round-tripped through a 32-bit HANDLE on x86 (WoW64) guests.
        assert(this->next_id_ < (1ULL << 32));
        return this->next_id_++;
    }

    uint64_t d3d9_host::ensure_vk_device()
    {
        if (this->vk_device_ != 0)
        {
            return this->vk_device_;
        }
        if (!this->vulkan_.available())
        {
            return 0;
        }

        uint64_t instance = 0;
        if (this->vulkan_.create_instance(instance) != 0 || instance == 0)
        {
            return 0;
        }

        std::array<uint64_t, 4> phys_ids{};
        uint32_t phys_count = 0;
        this->vulkan_.enumerate_physical_devices(instance, std::span{phys_ids}, phys_count);
        if (phys_count == 0)
        {
            return 0;
        }

        uint64_t device = 0;
        if (this->vulkan_.create_device(phys_ids[0], nullptr, 0, nullptr, 0, 0, nullptr, 0, 0, device) != 0 || device == 0)
        {
            return 0;
        }

        this->vk_instance_ = instance;
        this->vk_physical_device_ = phys_ids[0];
        this->vk_device_ = device;
        return device;
    }

    bool d3d9_host::ensure_draw_infra()
    {
        if (this->draw_infra_ready_)
        {
            return true;
        }
        const uint64_t device = this->ensure_vk_device();
        if (device == 0)
        {
            return false;
        }

        // A single queue family (index 0) is the same assumption vulkan_host::create_device and
        // create_render_target's own internal command pool already make for this simple, single-GPU-
        // queue setup.
        if (this->vulkan_.get_device_queue(device, 0, 0, this->queue_) != 0 || this->queue_ == 0)
        {
            return false;
        }
        if (this->vulkan_.create_command_pool(device, 0, 0, this->command_pool_) != 0 || this->command_pool_ == 0)
        {
            return false;
        }
        if (this->vulkan_.allocate_command_buffer(device, this->command_pool_, 0, this->command_buffer_) != 0 || this->command_buffer_ == 0)
        {
            return false;
        }
        if (this->vulkan_.create_fence(device, 0, this->fence_) != 0 || this->fence_ == 0)
        {
            return false;
        }
        for (uint32_t slot = 0; slot < batch_slot_count; ++slot)
        {
            if (this->vulkan_.allocate_command_buffer(device, this->command_pool_, 0, this->batch_command_buffer_[slot]) != 0 ||
                this->batch_command_buffer_[slot] == 0)
            {
                return false;
            }
            if (this->vulkan_.create_fence(device, 0, this->batch_fence_[slot]) != 0 || this->batch_fence_[slot] == 0)
            {
                return false;
            }
        }

        this->draw_infra_ready_ = true;
        return true;
    }

    void d3d9_host::close_render_pass(const uint32_t slot)
    {
        open_render_pass_state& rp = this->open_render_pass_[slot];
        if (!rp.open)
        {
            return;
        }
        const uint64_t cmd = this->batch_command_buffer_[slot];
        this->vulkan_.cmd_end_rendering(cmd);
        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        for (size_t i = 0; i < rp.color_count; ++i)
        {
            if (rp.color_image_ids[i] == 0)
            {
                continue;
            }
            this->vulkan_.cmd_pipeline_barrier(cmd, rp.color_image_ids[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                               VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);
        }
        // Mirror of the transition execute_draw made when this instance opened: every render target its
        // draws sampled goes back to the TRANSFER_SRC_OPTIMAL resting layout the rest of this host
        // (readback, clear, blt, the next draw's own attachment barriers) relies on.
        for (const uint64_t sampled_image : rp.sampled_image_ids)
        {
            this->vulkan_.cmd_pipeline_barrier(cmd, sampled_image, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);
        }
        rp = {};
    }

    void d3d9_host::realize_pending_color_clear(pending_batch_clear& pending)
    {
        if (!pending.color_pending)
        {
            return;
        }
        for (const uint64_t rt_handle : pending.color_targets)
        {
            const auto it = this->resources_.find(rt_handle);
            if (it == this->resources_.end() || it->second.vk_image_id == 0)
            {
                continue;
            }
            this->batch_clear_color_image(it->second.vk_image_id, pending.color_value);
            it->second.backing_dirty = true;
        }
        pending.color_pending = false;
    }

    void d3d9_host::submit_batch_async()
    {
        if (!this->batch_open_)
        {
            return;
        }
        // A Clear() may have deferred its value onto pending_clear_ (see the fast path in the
        // d3d9_clear handler) expecting a later draw to fold it into a render-pass LOAD_OP_CLEAR --
        // but this batch is closing without one ever having opened a render pass to consume it
        // (open_render_pass_[slot].open is false whenever anything is still pending; see that
        // struct's comment). Realize it now via the ordinary explicit-clear path so the Clear() this
        // batch recorded is never silently dropped.
        pending_batch_clear& pending = this->pending_clear_[this->batch_slot_];
        this->realize_pending_color_clear(pending);
        if (pending.depth_pending)
        {
            const auto ds_it = this->resources_.find(this->batch_ds_);
            uint32_t depth_vk_format = 0;
            if (ds_it != this->resources_.end() && ds_it->second.vk_image_id != 0 &&
                d3d9_format_to_vulkan(ds_it->second.format, depth_vk_format))
            {
                if (depth_stencil_transient_enabled && (ds_it->second.usage & d3dusage_depthstencil) != 0)
                {
                    // A transient depth-stencil has no TRANSFER_DST_BIT usage, so
                    // batch_clear_depth_stencil_image's vkCmdClearDepthStencilImage is invalid here --
                    // realize the clear the only way a transient image supports: a self-contained
                    // render-pass instance that does nothing but LOAD_OP_CLEAR then immediately end.
                    const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];
                    const vulkan_host::rendering_attachment depth_attachment{
                        .image_view = ds_it->second.vk_image_view_id,
                        .resolve_image_view = 0,
                        .image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        .resolve_image_layout = 0,
                        .resolve_mode = 0,
                        .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .clear_value = depth_stencil_clear_value(pending.depth_value, pending.stencil_value),
                    };
                    this->vulkan_.cmd_begin_rendering(batch_cmd, 0, 0, ds_it->second.width, ds_it->second.height, 1, 0, 0, {},
                                                      &depth_attachment, nullptr);
                    this->vulkan_.cmd_end_rendering(batch_cmd);
                }
                else
                {
                    this->batch_clear_depth_stencil_image(ds_it->second, depth_vk_format, VK_IMAGE_ASPECT_DEPTH_BIT, pending.depth_value,
                                                          pending.stencil_value);
                }
            }
            pending.depth_pending = false;
        }
        this->close_render_pass(this->batch_slot_);
        this->vulkan_.end_command_buffer(this->batch_command_buffer_[this->batch_slot_]);
        this->vulkan_.queue_submit(this->queue_, this->batch_command_buffer_[this->batch_slot_], this->batch_fence_[this->batch_slot_]);
        this->batch_slot_pending_[this->batch_slot_] = true;
        this->batch_open_ = false;
        ++this->batch_submit_count_;
    }

    void d3d9_host::retire_batch_slot(const uint32_t slot)
    {
        this->batch_slot_pending_[slot] = false;
        for (const pending_staging_buffer& staging : this->pending_staging_cleanup_[slot])
        {
            this->vulkan_.destroy_buffer(staging.device, staging.buffer);
            this->vulkan_.free_memory(staging.device, staging.memory);
        }
        this->pending_staging_cleanup_[slot].clear();
    }

    void d3d9_host::wait_for_batch_slot(const uint32_t slot)
    {
        if (!this->batch_slot_pending_[slot])
        {
            return;
        }
        this->vulkan_.wait_for_fence(this->batch_fence_[slot], UINT64_MAX);
        this->retire_batch_slot(slot);
    }

    void d3d9_host::flush_batch()
    {
        this->submit_batch_async();
        for (uint32_t slot = 0; slot < batch_slot_count; ++slot)
        {
            this->wait_for_batch_slot(slot);
        }
    }

    void d3d9_host::ensure_batch_open(const uint64_t device, const uint64_t target_rt, const uint64_t target_ds)
    {
        bool rotate_batch_slot = false;
        if (this->batch_open_ && (target_rt != this->batch_rt_ || target_ds != this->batch_ds_))
        {
            this->submit_batch_async();
            rotate_batch_slot = true;
        }
        if (this->batch_open_)
        {
            return;
        }
        if (rotate_batch_slot)
        {
            this->batch_slot_ = (this->batch_slot_ + 1) % batch_slot_count;
            this->wait_for_batch_slot(this->batch_slot_);
        }
        this->vertex_index_uniform_arena_[this->batch_slot_].offset = 0;
        if (this->frame_descriptor_pool_[this->batch_slot_] != 0)
        {
            this->vulkan_.reset_descriptor_pool(device, this->frame_descriptor_pool_[this->batch_slot_], 0);
        }
        this->batch_draw_count_ = 0;
        this->open_render_pass_[this->batch_slot_] = {};
        this->pending_clear_[this->batch_slot_] = {};
        this->vulkan_.reset_fence(device, this->batch_fence_[this->batch_slot_]);
        this->vulkan_.begin_command_buffer(this->batch_command_buffer_[this->batch_slot_], 0, false, 0, {}, 0, 0, 1, 0);
        this->batch_open_ = true;
        this->batch_rt_ = target_rt;
        this->batch_ds_ = target_ds;
        ++this->batch_generation_;
    }

    void d3d9_host::batch_clear_color_image(const uint64_t image, const std::array<float, 4>& color)
    {
        const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];
        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // A clear fully overwrites the image, so VK_IMAGE_LAYOUT_UNDEFINED is always a legal old layout
        // for this transition regardless of the image's real current layout -- lets this skip tracking or
        // querying it just for this, unlike every other consumer in this file.
        this->vulkan_.cmd_pipeline_barrier(batch_cmd, image, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color_range);
        this->vulkan_.cmd_clear_color_image(batch_cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color[0], color[1], color[2], color[3],
                                            color_range);
        this->vulkan_.cmd_pipeline_barrier(batch_cmd, image, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);
    }

    bool d3d9_host::ensure_pipeline(const std::span<const uint32_t> color_formats, const uint32_t width, const uint32_t height,
                                    const uint32_t depth_format)
    {
        pipeline_cache_key key{};
        for (size_t i = 0; i < color_formats.size() && i < key.color_formats.size(); ++i)
        {
            key.color_formats[i] = color_formats[i];
        }
        key.depth_format = depth_format;
        // Resolved static depth/blend state are part of the pipeline identity -- compute them ONCE here so
        // the exact values that key the cache are the same ones fed to create_graphics_pipeline on a miss.
        key.depth = build_depth_state(this->state_.render_state, depth_format);
        key.blend = build_blend_state(this->state_.render_state);
        // D3DRS_CLIPPING (default TRUE) drives depthClampEnable in the built pipeline, so it is part of the
        // pipeline identity -- normalized to 0/1 so distinct truthy values don't fragment the cache.
        key.depth_clip_enable = render_state_or(this->state_.render_state, d3drs_clipping, 1) != 0 ? 1u : 0u;
        key.cull_mode = d3dcull_to_vk_cull_mode(render_state_or(this->state_.render_state, d3drs_cullmode, d3dcull_ccw));

        const auto cached = this->ff_pipelines_.find(key);
        if (cached != this->ff_pipelines_.end())
        {
            this->pipeline_ = cached->second;
            return true;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0)
        {
            return false;
        }

        // vs_module_/fs_module_/pipeline_layout_ are shape-invariant (the fixed-function vertex/fragment
        // shaders and layout never depend on bound RT/depth format) -- created once and reused by every
        // cache miss below, unlike ff_pipelines_'s entries which vary per pipeline_cache_key.
        if (this->vs_module_ == 0 &&
            (this->vulkan_.create_shader_module(device, k_ff_vertex_shader_spirv.data(), k_ff_vertex_shader_spirv.size() * sizeof(uint32_t),
                                                this->vs_module_) != 0 ||
             this->vs_module_ == 0))
        {
            return false;
        }
        if (this->fs_module_ == 0 &&
            (this->vulkan_.create_shader_module(device, k_ff_fragment_shader_spirv.data(),
                                                k_ff_fragment_shader_spirv.size() * sizeof(uint32_t), this->fs_module_) != 0 ||
             this->fs_module_ == 0))
        {
            return false;
        }

        // One push-constant range (vec2 viewportSize) in the vertex stage, no descriptor sets -- this
        // minimal shader pair needs neither textures nor uniform buffers.
        if (this->pipeline_layout_ == 0 &&
            (this->vulkan_.create_pipeline_layout(device, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, {}, this->pipeline_layout_) != 0 ||
             this->pipeline_layout_ == 0))
        {
            return false;
        }

        // D3DFVF_XYZRHW|D3DFVF_DIFFUSE: 16-byte {x,y,z,rhw} position + 4-byte D3DCOLOR diffuse, stride 20.
        // Always per-vertex: the FF path is a single hardcoded, non-instanced stream (instancing requires
        // a programmable pipeline with a real multi-stream declaration), so it never sees INSTANCEDATA.
        const std::array<vulkan_host::vertex_binding, 1> bindings{
            {{.binding = 0, .stride = 20, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX}}};
        const std::array<vulkan_host::vertex_attribute, 2> attributes{{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 0, .format = VK_FORMAT_B8G8R8A8_UNORM, .offset = 16},
        }};
        const std::array<uint32_t, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        // Reuse the exact depth/blend values already baked into `key` above -- the cache key can never
        // disagree with what actually gets built. D3D9 has no independent per-render-target blend state --
        // every bound RT gets the same config.
        const std::vector<vulkan_host::color_blend_attachment> blend(color_formats.size(), key.blend);
        const vulkan_host::specialization empty_spec{};

        uint64_t pipeline = 0;
        const int32_t result = this->vulkan_.create_graphics_pipeline(
            device, /*render_pass=*/0, this->pipeline_layout_, this->vs_module_, this->fs_module_, width, height, bindings, attributes,
            key.depth, color_formats, depth_format, /*stencil_format=*/0, /*rasterization_samples=*/1, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            /*primitive_restart_enable=*/0, dynamic_states, empty_spec, empty_spec, blend, key.depth_clip_enable, key.cull_mode,
            k_umd_front_face, pipeline);
        if (result != 0 || pipeline == 0)
        {
            return false;
        }

        this->pipeline_ = pipeline;
        this->ff_pipelines_.emplace(key, pipeline);
        return true;
    }

    const parsed_vertex_decl* d3d9_host::find_real_vertex_decl() const
    {
        const auto vdecl_it = this->vertex_decls_.find(this->state_.vertex_decl);
        if (this->state_.vertex_decl == 0 || vdecl_it == this->vertex_decls_.end() || !vdecl_it->second.parsed.has_value() ||
            vdecl_it->second.parsed->attributes.empty())
        {
            return nullptr;
        }
        return &*vdecl_it->second.parsed;
    }

    d3d9_host::vertex_input_shape d3d9_host::vertex_shape_key() const
    {
        vertex_input_shape shape{};
        // Same shared helper the binding builder (ensure_programmable_pipeline) and draw (execute_draw)
        // consult, so the per-instance inputRate baked into a built pipeline can never disagree with the
        // mask this cache key was computed from. 0 for every non-instanced draw -- keeps existing keys.
        shape.instance_binding_mask = this->resolve_instancing().instance_binding_mask;
        const auto* real_decl = this->find_real_vertex_decl();
        if (real_decl != nullptr)
        {
            // vertex_decl_entry::parsed is populated once, eagerly, at CreateVertexDeclaration time
            // (create_vertex_decl) and never mutated after (no update DDI exists) -- same handle always
            // implies the same element types/offsets/usages. Real decl ids come from allocate_id()
            // (starts at 0x10000, see next_id_'s comment), so they can never collide with the two
            // fallback tags below.
            shape.id = this->state_.vertex_decl;
            // The handle alone is NOT sufficient: ensure_programmable_pipeline's real-decl branch bakes
            // each binding's VkVertexInputBindingDescription::stride from state_.stream_strides[stream],
            // which SetStreamSource(stream, buffer, offset, stride) can change without touching the
            // handle. Snapshot the current stride of every stream this declaration references so two draws
            // with the same handle but a different bound stride get distinct pipelines instead of reusing
            // a stale one built for the first stride. Iterate the exact same used_binding_mask the builder
            // consults; a referenced stream with no SetStreamSource yet contributes 0 (and is excluded
            // from the built bindings anyway -- usable_vertex_binding_mask), so the two can never disagree.
            for (uint32_t stream = 0; stream < max_vertex_streams; ++stream)
            {
                if ((real_decl->used_binding_mask & (1u << stream)) == 0)
                {
                    continue;
                }
                const auto stride_it = this->state_.stream_strides.find(stream);
                shape.strides[stream] = stride_it != this->state_.stream_strides.end() ? stride_it->second : 0u;
            }
            return shape;
        }
        // No real declaration bound (state_.vertex_decl == 0): the fallback stride heuristic (mirrors
        // ensure_programmable_pipeline's own `textured_layout = stride == 20` exactly) is what actually
        // decides the pipeline's vertex-input shape in this branch, NOT state_.vertex_decl (which is
        // always 0 here regardless of which fallback shape applies) -- so a key built from
        // state_.vertex_decl alone would silently collide the two fallback shapes whenever the same
        // VS/PS pair is drawn with both. The `strides` array stays all-zero here: this branch hardcodes
        // its binding stride to 16 or 20 (never the raw bound stride) and only ever reads stream 0, so
        // the tag fully captures its stride-dependence -- there is no per-stream stride to fold in.
        const auto stride_it = this->state_.stream_strides.find(0);
        const uint32_t stride = stride_it != this->state_.stream_strides.end() ? stride_it->second : 16;
        shape.id = stride == 20 ? 2u : 1u; // tags 1/2, disjoint from real decl handles (start at 0x10000)
        return shape;
    }

    uint32_t d3d9_host::usable_vertex_binding_mask(const parsed_vertex_decl& decl) const
    {
        uint32_t mask = 0;
        for (uint32_t stream = 0; stream < 32; ++stream)
        {
            if ((decl.used_binding_mask & (1u << stream)) == 0)
            {
                continue;
            }
            const auto stride_it = this->state_.stream_strides.find(stream);
            if (stride_it != this->state_.stream_strides.end() && stride_it->second != 0)
            {
                mask |= (1u << stream);
            }
        }
        return mask;
    }

    d3d9_host::instancing_state d3d9_host::resolve_instancing() const
    {
        // KNOWN LIMITATION: real D3D9 usage sets INDEXEDDATA on exactly one stream (the per-vertex
        // geometry stream driving the draw). If an app somehow set it on more than one, whichever one
        // this unordered_map happens to iterate last wins -- an arbitrary, nondeterministic choice, but
        // harmless in practice since that usage pattern is itself invalid D3D9 to begin with.
        instancing_state result{};
        for (const auto& [stream, freq] : this->state_.stream_frequencies)
        {
            if (stream >= max_vertex_streams)
            {
                continue; // beyond the mask width -- no Vulkan binding is ever emitted for it anyway
            }
            if ((freq & d3dstreamsource_indexeddata) != 0)
            {
                // The INDEXEDDATA stream's low 30 bits are the instance count for the whole draw. A count
                // of 0 is meaningless (a draw of no instances); ignore it and keep the default of 1.
                const uint32_t count = freq & d3dstreamsource_freq_mask;
                if (count != 0)
                {
                    result.instance_count = count;
                }
            }
            else if ((freq & d3dstreamsource_instancedata) != 0)
            {
                // Only divider == 1 (advance the stream once per instance) is representable without
                // VK_EXT_vertex_attribute_divisor. KNOWN LIMITATION: a non-1 divider is left per-vertex
                // (mask bit unset) rather than silently rendering wrong per-instance data -- the extension
                // is not enabled on the D3D9 path's Vulkan device (see this file's device creation).
                if ((freq & d3dstreamsource_freq_mask) == 1)
                {
                    result.instance_binding_mask |= (1u << stream);
                }
            }
        }
        return result;
    }

    const d3d9_host::programmable_pipeline_entry* d3d9_host::remember_pipeline_failure(const pipeline_cache_key& key,
                                                                                       const int32_t vk_result)
    {
        // Out-of-memory is the one failure that is a property of the moment, not of the key: the exact
        // same build can succeed once the driver has room again (a texture/pipeline freed, a smaller
        // scene). Blacklisting it would turn a temporary shortage into geometry that never comes back for
        // the rest of the process. Everything else -- a SPIR-V construct MoltenVK cannot express in MSL,
        // an unsupported vertex format, an invalid state combination -- is fully determined by the key
        // and will fail identically forever, so it is worth remembering. Failures with no VkResult to
        // inspect (module/layout creation, which report only success/failure) pass vk_result 0 and are
        // remembered, matching the "determined by the key" reasoning above.
        if (vk_result != VK_ERROR_OUT_OF_HOST_MEMORY && vk_result != VK_ERROR_OUT_OF_DEVICE_MEMORY)
        {
            this->failed_pipelines_.insert(key);
        }
        return nullptr;
    }

    const d3d9_host::programmable_pipeline_entry* d3d9_host::ensure_programmable_pipeline(const std::span<const uint32_t> color_formats,
                                                                                          const uint32_t width, const uint32_t height,
                                                                                          const uint32_t depth_format)
    {
        pipeline_cache_key key{};
        key.vertex_shader = this->state_.vertex_shader;
        key.pixel_shader = this->state_.pixel_shader;
        for (size_t i = 0; i < color_formats.size() && i < key.color_formats.size(); ++i)
        {
            key.color_formats[i] = color_formats[i];
        }
        key.depth_format = depth_format;
        key.vertex_shape = this->vertex_shape_key();
        // Resolved static depth/blend state are part of the pipeline identity -- compute them ONCE here so
        // the exact values that key the cache are the same ones fed to create_graphics_pipeline on a miss.
        key.depth = build_depth_state(this->state_.render_state, depth_format);
        key.blend = build_blend_state(this->state_.render_state);
        // D3DRS_CLIPPING (default TRUE) drives depthClampEnable in the built pipeline, so it is part of the
        // pipeline identity -- normalized to 0/1 so distinct truthy values don't fragment the cache.
        key.depth_clip_enable = render_state_or(this->state_.render_state, d3drs_clipping, 1) != 0 ? 1u : 0u;
        key.cull_mode = d3dcull_to_vk_cull_mode(render_state_or(this->state_.render_state, d3drs_cullmode, d3dcull_ccw));

        const auto cached = this->programmable_pipelines_.find(key);
        if (cached != this->programmable_pipelines_.end())
        {
            ++this->stats_.pipeline_cache_hit;
            return &cached->second;
        }
        // Already known unbuildable: bail before paying for the translation and Vulkan objects that are
        // only going to be thrown away again. See failed_pipelines_ (d3d9_host.hpp).
        if (this->failed_pipelines_.contains(key))
        {
            ++this->stats_.pipeline_negative_hit;
            return nullptr;
        }
        ++this->stats_.pipeline_cache_miss;

        const auto vs_it = this->shaders_.find(this->state_.vertex_shader);
        const auto ps_it = this->shaders_.find(this->state_.pixel_shader);
        if (vs_it == this->shaders_.end() || ps_it == this->shaders_.end())
        {
            // Deliberately NOT remembered: unlike every other failure here this one is about live state
            // (which ids state_ currently holds), not about the key's buildability.
            ++this->stats_.drop_shader_missing;
            return nullptr;
        }

        shader_pair_spirv spirv{};
        if (!translate_d3d9_shader_pair(vs_it->second.tokens.data(), vs_it->second.tokens.size() * sizeof(uint32_t),
                                        ps_it->second.tokens.data(), ps_it->second.tokens.size() * sizeof(uint32_t), spirv))
        {
            ++this->stats_.drop_translate_failed;
            return this->remember_pipeline_failure(key, 0);
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0)
        {
            ++this->stats_.drop_vk_object_failed;
            return nullptr;
        }

        programmable_pipeline_entry entry{};
        const int32_t vs_module_result = this->vulkan_.create_shader_module(device, spirv.vertex_spirv.data(),
                                                                            spirv.vertex_spirv.size() * sizeof(uint32_t), entry.vs_module);
        if (vs_module_result != 0 || entry.vs_module == 0)
        {
            ++this->stats_.drop_vk_object_failed;
            return this->remember_pipeline_failure(key, vs_module_result);
        }
        const int32_t ps_module_result = this->vulkan_.create_shader_module(device, spirv.pixel_spirv.data(),
                                                                            spirv.pixel_spirv.size() * sizeof(uint32_t), entry.fs_module);
        if (ps_module_result != 0 || entry.fs_module == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            ++this->stats_.drop_vk_object_failed;
            return this->remember_pipeline_failure(key, ps_module_result);
        }

        // Matches the CBV bindings d3d9_shader_translator.cpp pins into the SPIR-V: VS float-const UBO
        // at set 0 binding 0, PS float-const UBO at set 1 binding 0. Bindings 2/3 (int-const/bool-const
        // UBOs) are declared here unconditionally too, same rationale as binding 1 below -- vkd3d only
        // emits an actual SPIR-V descriptor for a register file a shader statically references, but
        // Vulkan permits a pipeline layout to declare bindings a shader doesn't use. The actual per-draw
        // UBO creation happens in execute_draw, which also allocates a fresh descriptor-set pair per
        // draw from the shared frame pool against these cached layouts.
        // Combined-image-sampler bindings for the vertex stage's texture registers s0..s3 sit at
        // bindings 1, 4, 5, 6 within set 0 (bindings 2/3 are the int/bool-const UBOs) -- the same
        // per-set scheme the PS uses in set 1, generated from the shared
        // max_vs_sampler_stages/vs_sampler_binding_for_stage (d3d9_shader_translator.hpp) that
        // d3d9_shader_translator.cpp pins into the VS SPIR-V. Declared unconditionally for the same
        // reason as PS: Vulkan permits a layout to declare more bindings than a shader statically uses,
        // so a VS that never samples (the common case) is unaffected; execute_draw only writes a
        // descriptor when a vertex texture is actually bound to that stage.
        // The six constant UBOs are DYNAMIC rather than plain: every draw sub-allocates its constants at a
        // fresh arena offset, and a plain uniform buffer can only express that offset inside the descriptor
        // write itself, which forces a fresh descriptor-set allocation + write per draw even when nothing
        // else about the bindings changed. As dynamic buffers the descriptor names the whole slice range at
        // offset 0 and the per-draw offset travels in vkCmdBindDescriptorSets' pDynamicOffsets instead, so
        // draws that differ only in where their constants landed reuse execute_draw's descriptor_set_memo.
        // Both stages use 3 dynamic uniform buffers, well inside the 8 Vulkan guarantees per stage and per
        // set (maxPerStageDescriptorUniformBuffersDynamic / maxDescriptorSetUniformBuffersDynamic).
        std::array<vulkan_host::descriptor_binding, 3 + max_vs_sampler_stages> vs_bindings{{
            {.binding = 0,
             .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
             .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
            {.binding = 2,
             .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
             .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
            {.binding = 3,
             .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
             .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
        }};
        for (uint32_t stage = 0; stage < max_vs_sampler_stages; ++stage)
        {
            vs_bindings[3 + stage] = {.binding = vs_sampler_binding_for_stage(stage),
                                      .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .descriptor_count = 1,
                                      .stage_flags = VK_SHADER_STAGE_VERTEX_BIT};
        }
        // Combined-image-sampler bindings for texture stages s0..s3 sit at bindings 1, 4, 5, 6 (bindings 2/3
        // are the int/bool-const UBOs) -- the exact scheme d3d9_shader_translator.cpp pins into the PS SPIR-V.
        // The sampler entries below are generated from max_ps_sampler_stages/ps_sampler_binding_for_stage
        // (d3d9_shader_translator.hpp) rather than hand-typed, so the two sides can never independently
        // drift on the s(k) -> binding mapping. All four are declared here unconditionally: Vulkan allows a
        // pipeline layout to declare more bindings than a given shader module statically uses, so this is
        // safe both for a PS that never samples and for one that samples fewer than four stages
        // (d3d9_shader_translator.cpp only emits a SPIR-V sampler variable for a stage the PS actually
        // reads). execute_draw only writes each descriptor when a texture is bound to that stage. Raising
        // the cap toward D3D9's 16-sampler max is a mechanical change to max_ps_sampler_stages + the pool
        // sizing (this constant is now the only place that needs to change for the bindings themselves).
        std::array<vulkan_host::descriptor_binding, 3 + max_ps_sampler_stages> ps_bindings{{
            {.binding = 0,
             .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
             .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
            {.binding = 2,
             .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
             .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
            {.binding = 3,
             .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
             .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
        }};
        for (uint32_t stage = 0; stage < max_ps_sampler_stages; ++stage)
        {
            ps_bindings[3 + stage] = {.binding = ps_sampler_binding_for_stage(stage),
                                      .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .descriptor_count = 1,
                                      .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT};
        }
        if (this->vulkan_.create_descriptor_set_layout(device, vs_bindings, entry.vs_set_layout) != 0 || entry.vs_set_layout == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            ++this->stats_.drop_vk_object_failed;
            return nullptr;
        }
        if (this->vulkan_.create_descriptor_set_layout(device, ps_bindings, entry.ps_set_layout) != 0 || entry.ps_set_layout == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            ++this->stats_.drop_vk_object_failed;
            return nullptr;
        }

        const std::array<uint64_t, 2> set_layouts{entry.vs_set_layout, entry.ps_set_layout};
        if (this->vulkan_.create_pipeline_layout(device, 0, 0, set_layouts, entry.pipeline_layout) != 0 || entry.pipeline_layout == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.ps_set_layout);
            ++this->stats_.drop_vk_object_failed;
            return nullptr;
        }

        // Vertex layout selection: prefer the real parsed D3DVERTEXELEMENT9 declaration (Task 7's
        // parse_vertex_decl, cached in vertex_decls_ at CreateVertexDeclaration time) when the app
        // actually bound one -- see parse_vertex_decl's comment (this file) for why its
        // parsed_vertex_attribute::location/binding/offset can be consumed directly with no further
        // usage-keyed remapping. Falls back to the old stride heuristic below when state_.vertex_decl
        // is unset or its cached parse produced no attributes, so every existing programmable-pipeline
        // guest test (none of which call CreateVertexDeclaration/SetVertexDeclaration) keeps working
        // byte-for-byte unchanged.
        const parsed_vertex_decl* real_decl = this->find_real_vertex_decl();

        std::vector<vulkan_host::vertex_binding> bindings;
        std::vector<vulkan_host::vertex_attribute> attributes;
        if (real_decl != nullptr)
        {
            // Only streams with a real, nonzero stride (usable_vertex_binding_mask) get a Vulkan
            // binding -- a stream the declaration references but the app never called SetStreamSource
            // for (or set a zero stride) is excluded rather than emitting a zero-stride binding that
            // would mis-fetch. Every attribute referencing a binding NOT in that filtered mask is
            // dropped too (not just skipped from `bindings`): a VkVertexInputAttributeDescription whose
            // binding has no corresponding VkVertexInputBindingDescription is a Vulkan spec violation
            // (VUID-VkPipelineVertexInputStateCreateInfo-binding-00615). execute_draw's own per-stream
            // upload/bind loop uses this exact same filtered mask, so the two can never disagree.
            const uint32_t usable_mask = this->usable_vertex_binding_mask(*real_decl);
            // Same shared helper vertex_shape_key() folded into this pipeline's cache key -- a stream
            // flagged D3DSTREAMSOURCE_INSTANCEDATA gets VK_VERTEX_INPUT_RATE_INSTANCE so Vulkan fetches it
            // by instance index; every other stream stays per-vertex. Because both the key and this build
            // read the identical mask, they can never disagree about which bindings are instance-rate.
            const uint32_t instance_mask = this->resolve_instancing().instance_binding_mask;
            for (uint32_t stream = 0; stream < 32; ++stream)
            {
                if ((usable_mask & (1u << stream)) == 0)
                {
                    continue;
                }
                const auto stride_it = this->state_.stream_strides.find(stream);
                const uint32_t input_rate =
                    (instance_mask & (1u << stream)) != 0 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
                bindings.push_back({.binding = stream, .stride = stride_it->second, .input_rate = input_rate});
            }
            attributes.reserve(real_decl->attributes.size());
            for (const auto& attr : real_decl->attributes)
            {
                if (attr.binding >= 32 || (usable_mask & (1u << attr.binding)) == 0)
                {
                    continue; // references a binding that didn't make it into `bindings` above
                }
                attributes.push_back({.location = attr.location, .binding = attr.binding, .format = attr.vk_format, .offset = attr.offset});
            }
        }
        else
        {
            // Fallback: this host has no D3DDDIARG_CREATEVERTEXSHADERDECL wiring yet
            // (pfnCreateVertexShaderDecl is still an unwired device_stub -- the real D3DVERTEXELEMENT9
            // array never reaches here), so the two vertex shapes this milestone's guest tests actually
            // use are told apart by the one piece of real per-vertex-layout information the wire protocol
            // already carries end-to-end: SetStreamSource's Stride (see d3d9_set_stream_source's handler --
            // stream_strides was previously received and silently discarded here). D3DFVF_XYZ|D3DFVF_TEX1
            // (position + one float2 texcoord) is 20 bytes; D3DFVF_XYZ|D3DFVF_DIFFUSE (position + one
            // D3DCOLOR) is 16 bytes -- distinct stride values for this milestone's fixed set of shapes,
            // same spirit as classify_resource_usage's format-based heuristic on the guest side.
            const auto stride_it = this->state_.stream_strides.find(0);
            const uint32_t stride = stride_it != this->state_.stream_strides.end() ? stride_it->second : 16;
            const bool textured_layout = stride == 20;
            // Always per-vertex: this fallback is a single stream-0-only shape with no vertex declaration,
            // so there is no second stream to carry per-instance data (instancing needs a real multi-stream
            // declaration, which takes the real-decl branch above).
            bindings.push_back({.binding = 0, .stride = textured_layout ? 20u : 16u, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX});
            attributes.push_back({.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0});
            attributes.push_back({.location = 1,
                                  .binding = 0,
                                  .format = textured_layout ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_B8G8R8A8_UNORM,
                                  .offset = 12});
        }
        const std::array<uint32_t, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        // Reuse the exact depth/blend values already baked into `key` above -- the cache key can never
        // disagree with what actually gets built. D3D9 has no independent per-render-target blend state --
        // every bound RT gets the same config.
        const std::vector<vulkan_host::color_blend_attachment> blend(color_formats.size(), key.blend);
        const vulkan_host::specialization empty_spec{};

        const int32_t result = this->vulkan_.create_graphics_pipeline(
            device, /*render_pass=*/0, entry.pipeline_layout, entry.vs_module, entry.fs_module, width, height, bindings, attributes,
            key.depth, color_formats, depth_format, /*stencil_format=*/0, /*rasterization_samples=*/1, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            /*primitive_restart_enable=*/0, dynamic_states, empty_spec, empty_spec, blend, key.depth_clip_enable, key.cull_mode,
            k_umd_front_face, entry.pipeline);
        if (result != 0 || entry.pipeline == 0)
        {
            // EMULATOR_D3D9_PIPEDIAG dumps the full description of a pipeline the driver refused. Without
            // it the failure is completely silent -- ensure_programmable_pipeline returns nullptr,
            // execute_draw drops the draw with a D3D_OK, and the only symptom is geometry that never
            // appears, indistinguishable from geometry the app never submitted. Nothing else recovers the
            // rejected configuration: it is assembled from live render state and thrown away on failure.
            // Same fprintf-to-stderr shape d3d9_shader_translator.cpp uses for its vkd3d diagnostics, and
            // capped, because a title with many distinct unbuildable pipelines would otherwise flood
            // stderr on the first frame that draws them all (failed_pipelines_ means each one is only
            // attempted, and so only reported, once).
            static int pipe_diag_left = getenv("EMULATOR_D3D9_PIPEDIAG") != nullptr ? 24 : 0;
            if (pipe_diag_left > 0)
            {
                --pipe_diag_left;
                fprintf(stderr,
                        "[d3d9-pipediag] create_graphics_pipeline rc=%d vs=%llu ps=%llu %ux%u depth_fmt=%u depth{test=%u write=%u op=%u} "
                        "clip=%u colors=%zu bindings=%zu attrs=%zu\n",
                        result, static_cast<unsigned long long>(this->state_.vertex_shader),
                        static_cast<unsigned long long>(this->state_.pixel_shader), width, height, depth_format, key.depth.test_enable,
                        key.depth.write_enable, key.depth.compare_op, key.depth_clip_enable, color_formats.size(), bindings.size(),
                        attributes.size());
                for (size_t i = 0; i < color_formats.size(); ++i)
                {
                    fprintf(stderr, "[d3d9-pipediag]   color[%zu] fmt=%u\n", i, color_formats[i]);
                }
                for (const auto& b : bindings)
                {
                    fprintf(stderr, "[d3d9-pipediag]   binding %u stride=%u rate=%u\n", b.binding, b.stride, b.input_rate);
                }
                for (const auto& a : attributes)
                {
                    fprintf(stderr, "[d3d9-pipediag]   attr loc=%u binding=%u fmt=%u offset=%u\n", a.location, a.binding, a.format,
                            a.offset);
                }
            }
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_pipeline_layout(device, entry.pipeline_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.ps_set_layout);
            ++this->stats_.drop_vk_object_failed;
            return this->remember_pipeline_failure(key, result);
        }

        // The set layouts and pipeline layout survive here (unlike the old destroy-after-use pattern) so
        // execute_draw can allocate a fresh per-draw descriptor-set pair against these layouts (from the
        // shared frame_descriptor_pool_) and bind it every draw.
        return &this->programmable_pipelines_.emplace(key, entry).first->second;
    }

    bool d3d9_host::ensure_frame_descriptor_pool(const uint64_t device, const uint32_t slot, const uint32_t needed_draws)
    {
        if (this->frame_descriptor_pool_[slot] != 0 && this->frame_desc_capacity_draws_[slot] >= needed_draws)
        {
            return true;
        }

        uint32_t new_capacity =
            this->frame_desc_capacity_draws_[slot] != 0 ? this->frame_desc_capacity_draws_[slot] : frame_desc_initial_draws;
        while (new_capacity < needed_draws)
        {
            new_capacity *= 2;
        }

        // Drop the undersized pool before creating the larger one -- this is only ever reached on the slot
        // execute_draw has already settled on for THIS draw (see the header comment on
        // ensure_frame_descriptor_pool), so every EARLIER draw that allocated from this same slot's pool
        // has already fenced.
        if (this->frame_descriptor_pool_[slot] != 0)
        {
            this->vulkan_.destroy_descriptor_pool(device, this->frame_descriptor_pool_[slot]);
            this->frame_descriptor_pool_[slot] = 0;
            this->frame_desc_capacity_draws_[slot] = 0;
        }

        // Same per-draw accounting as the removed per-pipeline pool (maxSets=2, 6 UBOs, max_vs_sampler_stages
        // + max_ps_sampler_stages combined-image-samplers), scaled by the number of draws the pool covers.
        const std::array<vulkan_host::descriptor_pool_size, 2> pool_sizes{{
            {.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptor_count = 6 * new_capacity},
            {.descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .descriptor_count = (max_vs_sampler_stages + max_ps_sampler_stages) * new_capacity},
        }};
        uint64_t pool = 0;
        if (this->vulkan_.create_descriptor_pool(device, 2 * new_capacity, pool_sizes, pool) != 0 || pool == 0)
        {
            return false;
        }
        this->frame_descriptor_pool_[slot] = pool;
        this->frame_desc_capacity_draws_[slot] = new_capacity;
        return true;
    }

    namespace
    {
        // VkPhysicalDeviceMemoryProperties parsing helper for execute_draw's vertex buffer upload --
        // vulkan_host keeps its own equivalent private (find_memory_type in vulkan_host.cpp), so this
        // mirrors it locally using the real Vulkan struct (already included above for the enum values).
        uint32_t find_memory_type_index(vulkan_host& vulkan, const uint64_t physical_device, const uint32_t type_bits,
                                        const VkMemoryPropertyFlags required)
        {
            VkPhysicalDeviceMemoryProperties props{};
            if (vulkan.get_physical_device_memory_properties(physical_device, &props, sizeof(props)) != 0)
            {
                return UINT32_MAX;
            }
            for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
            {
                if ((type_bits & (1u << i)) != 0 && (props.memoryTypes[i].propertyFlags & required) == required)
                {
                    return i;
                }
            }
            return UINT32_MAX;
        }

        // Bytes needed for one tightly-packed mip-0, layer-0 image at `vk_format`/`width`x`height` --
        // covers exactly the VkFormat constants d3d9_format_to_vulkan can produce. Returns 0 for
        // anything else, so callers can fail cleanly instead of guessing a size.
        size_t vk_texture_data_size(const uint32_t vk_format, const uint32_t width, const uint32_t height)
        {
            switch (vk_format)
            {
            case VK_FORMAT_R8_UNORM:
                return static_cast<size_t>(width) * height;
            case VK_FORMAT_R5G6B5_UNORM_PACK16:
            case VK_FORMAT_R8G8_UNORM:
            case VK_FORMAT_R8G8_SNORM:
                return static_cast<size_t>(width) * height * 2;
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R32_SFLOAT:
                return static_cast<size_t>(width) * height * 4;
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return static_cast<size_t>(width) * height * 8;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
                return ((static_cast<size_t>(width) + 3) / 4) * ((static_cast<size_t>(height) + 3) / 4) * 8;
            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
                return ((static_cast<size_t>(width) + 3) / 4) * ((static_cast<size_t>(height) + 3) / 4) * 16;
            default:
                return 0;
            }
        }

        // One subresource of a sampled texture: its mip level, its extent, its array layer (a cube
        // face; 0 for 2D/volume), and its tightly-packed byte size.
        struct texture_subresource
        {
            uint32_t level;
            uint32_t width;
            uint32_t height;
            uint32_t depth;            // 1 for 2D/cube; the level's depth extent for a volume
            uint32_t base_array_layer; // cube face index; 0 for 2D/volume
            size_t size;
        };

        constexpr uint32_t cube_face_count = 6;

        // Ordered per-subresource layout (index -> level/face/extent/byte-size) of a sampled texture of
        // `kind`. Cube: 6*mip_levels entries, subresource index == face*mip_levels + level. Volume:
        // mip_levels entries, index == level, each spanning the level's whole depth extent. 2D:
        // mip_levels entries, index == level. This ONE mapping both sizes the per-subresource backing
        // store (create_resource) AND drives the staging upload (ensure_texture_uploaded), so the two can
        // never disagree about which subresource index holds which (level, face).
        std::vector<texture_subresource> texture_subresource_layout(const uint32_t kind, const uint32_t vk_format, const uint32_t width,
                                                                    const uint32_t height, const uint32_t depth, const uint32_t mip_levels)
        {
            const uint32_t levels = std::max(1u, mip_levels);
            const bool is_cube = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_cube);
            const bool is_volume = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_volume);
            const uint32_t faces = is_cube ? cube_face_count : 1u;

            std::vector<texture_subresource> out;
            out.reserve(static_cast<size_t>(faces) * levels);
            for (uint32_t face = 0; face < faces; ++face)
            {
                for (uint32_t level = 0; level < levels; ++level)
                {
                    const uint32_t level_width = std::max(1u, width >> level);
                    const uint32_t level_height = std::max(1u, height >> level);
                    const uint32_t level_depth = is_volume ? std::max(1u, depth >> level) : 1u;
                    const size_t size = vk_texture_data_size(vk_format, level_width, level_height) * level_depth;
                    out.push_back({level, level_width, level_height, level_depth, face, size});
                }
            }
            return out;
        }

        // View dimensionality for sampling a resource of `kind`: cube -> CUBE / 6 layers, volume ->
        // 3D / 1 layer, everything else -> 2D / 1 layer. The PS and VS sampler-binding sites both call
        // this so a cube/volume view can never be built 2D at one site and cube/3D at the other.
        struct sampled_view_shape
        {
            uint32_t view_type;
            uint32_t layer_count;
        };

        sampled_view_shape sampled_view_shape_for_kind(const uint32_t kind)
        {
            if (kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_cube))
            {
                return {VK_IMAGE_VIEW_TYPE_CUBE, cube_face_count};
            }
            if (kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_volume))
            {
                return {VK_IMAGE_VIEW_TYPE_3D, 1};
            }
            return {VK_IMAGE_VIEW_TYPE_2D, 1};
        }

        // True for a colour render target that a draw may legally sample as a texture. D3D9 lets an app
        // SetTexture() a surface it previously rendered into (render-to-texture), which is how every
        // shader-era title composites its scene: the world is drawn into one or more off-screen targets
        // and a later full-screen pass samples them onto the back buffer. Such a resource is
        // render-target-kind, so it has real GPU backing but NO CPU-side pixels to upload -- the exact
        // case ensure_texture_uploaded is documented to refuse.
        //
        // Depth-stencil-usage resources are deliberately excluded (a shadow-map fetch needs a
        // single-aspect sampled view, while this host's cached per-resource view for a combined D24S8
        // depth attachment necessarily carries both aspects -- see ensure_depth_stencil_view -- so the
        // one cached view cannot serve both roles). Those stages stay unbound, as before.
        // Takes the two resource_entry fields it needs rather than the entry itself: resource_entry is a
        // private nested type of d3d9_host and cannot be named from this anonymous namespace.
        bool is_samplable_render_target(const uint64_t vk_image_id, const uint32_t usage)
        {
            return vk_image_id != 0 && (usage & d3dusage_rendertarget) != 0 && (usage & d3dusage_depthstencil) == 0;
        }

        // Converts an IEEE-754 single-precision float to a half-precision (binary16) bit pattern, with
        // round-to-nearest-even and correct handling of zero/subnormal/overflow -- used to encode a
        // ColorFill's normalized channel value for R16G16B16A16_SFLOAT render targets.
        uint16_t float_to_half(const float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            const uint32_t sign = (bits >> 16) & 0x8000u;
            const int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFF) - 127 + 15;
            const uint32_t mantissa = bits & 0x7FFFFFu;

            if (((bits >> 23) & 0xFF) == 0xFF)
            {
                // Inf/NaN: preserve a NaN payload bit so a NaN never collapses to Inf.
                return static_cast<uint16_t>(sign | 0x7C00u | (mantissa != 0 ? 0x0200u : 0u));
            }
            if (exponent >= 0x1F)
            {
                return static_cast<uint16_t>(sign | 0x7C00u); // overflow -> Inf
            }
            if (exponent <= 0)
            {
                if (exponent < -10)
                {
                    return static_cast<uint16_t>(sign); // too small -> signed zero
                }
                // Subnormal half: add the implicit leading 1, then round-to-nearest-even.
                const uint32_t full_mantissa = mantissa | 0x800000u;
                const int32_t shift = 14 - exponent;
                const uint32_t half_mantissa = full_mantissa >> shift;
                const uint32_t remainder = full_mantissa & ((1u << shift) - 1);
                const uint32_t halfway = 1u << (shift - 1);
                uint32_t rounded = half_mantissa;
                if (remainder > halfway || (remainder == halfway && (half_mantissa & 1u) != 0))
                {
                    ++rounded;
                }
                return static_cast<uint16_t>(sign | rounded);
            }
            // Normal half: round the 23-bit mantissa down to 10 bits, round-to-nearest-even.
            uint32_t half = (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
            const uint32_t remainder = mantissa & 0x1FFFu;
            if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1u) != 0))
            {
                ++half; // carry ripples correctly into the exponent field if the mantissa overflows
            }
            return static_cast<uint16_t>(sign | half);
        }

        // Encodes a single D3DCOLOR (0xAARRGGBB) fill value into `out` for `vk_format`, returning false for
        // a format with no encoder. The caller sizes `out` via the shared vk_format_bytes_per_texel, so this
        // and the RT backing/readback sizing can never disagree on a format's texel size.
        bool encode_fill_texel(const uint32_t vk_format, const uint32_t color_argb, std::array<std::byte, 8>& out)
        {
            const uint32_t a = (color_argb >> 24) & 0xFF;
            const uint32_t r = (color_argb >> 16) & 0xFF;
            const uint32_t g = (color_argb >> 8) & 0xFF;
            const uint32_t b = color_argb & 0xFF;

            switch (vk_format)
            {
            case VK_FORMAT_B8G8R8A8_UNORM: {
                // In-memory byte order B,G,R,A is exactly a little-endian D3DCOLOR dword -- passthrough.
                std::memcpy(out.data(), &color_argb, sizeof(color_argb));
                return true;
            }
            case VK_FORMAT_R5G6B5_UNORM_PACK16: {
                // VK_FORMAT_R5G6B5_UNORM_PACK16 packs R in bits 11..15, G in 5..10, B in 0..4 -- the exact
                // D3DFMT_R5G6B5 layout (see d3d9_format_to_vulkan).
                const uint16_t packed = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                std::memcpy(out.data(), &packed, sizeof(packed));
                return true;
            }
            case VK_FORMAT_R16G16B16A16_SFLOAT: {
                // Interpret the 8-bit D3DCOLOR channels as normalized [0,1] floats and store one half per
                // channel in R,G,B,A memory order (VK_FORMAT_R16G16B16A16_SFLOAT).
                const uint16_t halves[4] = {
                    float_to_half(static_cast<float>(r) / 255.0f),
                    float_to_half(static_cast<float>(g) / 255.0f),
                    float_to_half(static_cast<float>(b) / 255.0f),
                    float_to_half(static_cast<float>(a) / 255.0f),
                };
                std::memcpy(out.data(), halves, sizeof(halves));
                return true;
            }
            case VK_FORMAT_R32_SFLOAT: {
                // Single-channel float RT: store the red channel as a normalized [0,1] float32 (matches the
                // per-channel normalization the half-float RT path above uses).
                const float value = static_cast<float>(r) / 255.0f;
                std::memcpy(out.data(), &value, sizeof(value));
                return true;
            }
            default:
                return false;
            }
        }

        // D3DTEXTUREFILTERTYPE (D3DTEXF_NONE=0, POINT=1, LINEAR=2, ANISOTROPIC=3, ...) -> VkFilter.
        // Anisotropic/pyramidal/gaussian quad filters all sample linearly in Vulkan; anisotropy itself is
        // the sampler's separate anisotropyEnable/maxAnisotropy fields (see build_sampler below).
        uint32_t d3d9_filter_to_vk_filter(const uint32_t d3d9_filter)
        {
            return d3d9_filter >= 2 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        }

        uint32_t d3d9_mip_filter_to_vk_mipmap_mode(const uint32_t d3d9_mip_filter)
        {
            return d3d9_mip_filter >= 2 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }

        // D3DTEXTUREADDRESS (WRAP=1, MIRROR=2, CLAMP=3, BORDER=4, MIRRORONCE=5) -> VkSamplerAddressMode.
        uint32_t d3d9_address_to_vk(const uint32_t d3d9_address)
        {
            switch (d3d9_address)
            {
            case 2:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case 3:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case 4:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case 5:
                return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            default:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT; // WRAP (1) and any unrecognized value
            }
        }

        uint32_t sampler_state_or(const std::unordered_map<uint64_t, uint32_t>& sampler_state, const uint32_t sampler,
                                  const uint32_t d3dsamp_type, const uint32_t default_value)
        {
            const auto it = sampler_state.find(tss_key(sampler, d3dsamp_type));
            return it != sampler_state.end() ? it->second : default_value;
        }

        // VkFormat's contiguous depth/depth-stencil range covers exactly the two depth formats
        // d3d9_format_to_vulkan can produce (D32_SFLOAT_S8_UINT for D3DFMT_D24S8, D32_SFLOAT for
        // D3DFMT_D24X8); the aspect mask includes STENCIL only for formats that actually carry it.
        uint32_t depth_aspect_mask(const uint32_t vk_format)
        {
            switch (vk_format)
            {
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            }
        }

    } // namespace

    namespace
    {
        // Temporary diagnostic (EMULATOR_D3D9_DRAWPROFILE=1, shared with execute_draw's own profiling
        // block below): self-contained wall-clock totals for build_sampler/ensure_texture_uploaded's
        // own full bodies (cache-hit path included), to find which specific call inside execute_draw's
        // texdesc phase is actually expensive rather than continuing to guess from code reading.
        uint64_t g_build_sampler_ns{};
        uint64_t g_ensure_texture_uploaded_ns{};
        uint64_t g_ensure_texture_uploaded_max_ns{};
        uint64_t g_texture_real_upload_count{};

        bool drawprofile_enabled_flag()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_DRAWPROFILE") != nullptr;
            return enabled;
        }

        // glibc/libc getenv takes a process-wide lock and linearly scans environ, so a per-draw or
        // per-Unlock check is not free: a live MW2 profile measured the unhoisted checks below at ~1.5%
        // of both vCPU threads' wall time, all of it inside execute_draw. The guest cannot change the
        // host environment, so every diagnostic flag on a hot path resolves exactly once -- the same
        // idiom drawprofile_enabled_flag/pass_diag_enabled/present_timing_enabled already use.
        bool resetpool_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_RESETPOOL_DIAG") != nullptr;
            return enabled;
        }

        bool allocset_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_ALLOCSET_DIAG") != nullptr;
            return enabled;
        }

        bool uploadvol_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_UPLOADVOL_DIAG") != nullptr;
            return enabled;
        }

        bool uploadid_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_UPLOADID_DIAG") != nullptr;
            return enabled;
        }

        bool texupload_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_TEXUPLOAD_DIAG") != nullptr;
            return enabled;
        }

        bool texblt_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_TEXBLT_DIAG") != nullptr;
            return enabled;
        }

        bool direct_miss_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_DIRECT_MISS_DIAG") != nullptr;
            return enabled;
        }

        // Temporary diagnostic (EMULATOR_D3D9_RESETPOOL_DIAG=1): direct measurement to test the
        // hypothesis from the 2026-08-24 frame_desc_initial_draws regression (see that constant's own
        // comment) -- does vkResetDescriptorPool's real cost scale with the pool's total capacity, and
        // which of execute_draw's three batch-reopen triggers (RT/depth-stencil change, descriptor-pool
        // exhaustion, arena growth) actually dominates reopen frequency in real gameplay.
        void resetpool_diag_report(const uint64_t elapsed_ns, const uint64_t rt_change, const uint64_t desc_exhaustion,
                                   const uint64_t arena_growth)
        {
            static std::atomic<uint64_t> total_ns{};
            static std::atomic<uint64_t> max_ns{};
            static std::atomic<uint64_t> call_count{};
            static std::atomic<uint64_t> reopen_rt_change{};
            static std::atomic<uint64_t> reopen_desc_exhaustion{};
            static std::atomic<uint64_t> reopen_arena_growth{};
            static auto window_start = std::chrono::steady_clock::now();

            total_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
            call_count.fetch_add(1, std::memory_order_relaxed);
            reopen_rt_change.fetch_add(rt_change, std::memory_order_relaxed);
            reopen_desc_exhaustion.fetch_add(desc_exhaustion, std::memory_order_relaxed);
            reopen_arena_growth.fetch_add(arena_growth, std::memory_order_relaxed);
            uint64_t prev_max = max_ns.load(std::memory_order_relaxed);
            while (elapsed_ns > prev_max && !max_ns.compare_exchange_weak(prev_max, elapsed_ns, std::memory_order_relaxed))
            {
            }

            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - window_start).count() < 5.0)
            {
                return;
            }
            window_start = now;

            const uint64_t calls = call_count.exchange(0, std::memory_order_relaxed);
            const uint64_t total = total_ns.exchange(0, std::memory_order_relaxed);
            const uint64_t peak = max_ns.exchange(0, std::memory_order_relaxed);
            const uint64_t rt = reopen_rt_change.exchange(0, std::memory_order_relaxed);
            const uint64_t desc = reopen_desc_exhaustion.exchange(0, std::memory_order_relaxed);
            const uint64_t arena = reopen_arena_growth.exchange(0, std::memory_order_relaxed);
            const double avg_us = calls != 0 ? (static_cast<double>(total) / static_cast<double>(calls)) / 1000.0 : 0.0;
            fprintf(stderr,
                    "[d3d9-resetpool-diag] reset_calls=%llu avg_us=%.2f max_us=%.2f total_ms=%.2f | reopen_rt_change=%llu "
                    "reopen_desc_exhaustion=%llu reopen_arena_growth=%llu\n",
                    static_cast<unsigned long long>(calls), avg_us, static_cast<double>(peak) / 1000.0,
                    static_cast<double>(total) / 1'000'000.0, static_cast<unsigned long long>(rt), static_cast<unsigned long long>(desc),
                    static_cast<unsigned long long>(arena));
        }

        // Temporary diagnostic (EMULATOR_D3D9_ALLOCSET_DIAG=1): same style as resetpool_diag_report,
        // measuring vkAllocateDescriptorSets instead -- called once per programmable draw (not once per
        // batch reopen like reset_descriptor_pool), so its cumulative per-frame cost is a much larger
        // multiple of any per-call regression than the reset call's. Built to test whether THIS is the
        // real mechanism behind the frame_desc_initial_draws=4096 regression that resetpool_diag_report's
        // own measurement ruled out as too small to explain.
        void allocset_diag_report(const uint64_t elapsed_ns)
        {
            static std::atomic<uint64_t> total_ns{};
            static std::atomic<uint64_t> max_ns{};
            static std::atomic<uint64_t> call_count{};
            static auto window_start = std::chrono::steady_clock::now();

            total_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
            call_count.fetch_add(1, std::memory_order_relaxed);
            uint64_t prev_max = max_ns.load(std::memory_order_relaxed);
            while (elapsed_ns > prev_max && !max_ns.compare_exchange_weak(prev_max, elapsed_ns, std::memory_order_relaxed))
            {
            }

            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - window_start).count() < 5.0)
            {
                return;
            }
            window_start = now;

            const uint64_t calls = call_count.exchange(0, std::memory_order_relaxed);
            const uint64_t total = total_ns.exchange(0, std::memory_order_relaxed);
            const uint64_t peak = max_ns.exchange(0, std::memory_order_relaxed);
            const double avg_us = calls != 0 ? (static_cast<double>(total) / static_cast<double>(calls)) / 1000.0 : 0.0;
            fprintf(stderr, "[d3d9-allocset-diag] alloc_calls=%llu avg_us=%.2f max_us=%.2f total_ms=%.2f\n",
                    static_cast<unsigned long long>(calls), avg_us, static_cast<double>(peak) / 1000.0,
                    static_cast<double>(total) / 1'000'000.0);
        }

        // Accumulates elapsed wall-clock time into `target` on destruction regardless of which of a
        // function's return statements actually fires -- avoids having to touch every exit path.
        class scoped_ns_accumulator
        {
          public:
            scoped_ns_accumulator(uint64_t& target, const bool active, uint64_t* max_target = nullptr)
                : target_(target),
                  max_target_(max_target),
                  active_(active),
                  start_(active ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{})
            {
            }

            ~scoped_ns_accumulator()
            {
                if (this->active_)
                {
                    const auto elapsed = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - this->start_).count());
                    this->target_ += elapsed;
                    if (this->max_target_ != nullptr && elapsed > *this->max_target_)
                    {
                        *this->max_target_ = elapsed;
                    }
                }
            }

            scoped_ns_accumulator(const scoped_ns_accumulator&) = delete;
            scoped_ns_accumulator& operator=(const scoped_ns_accumulator&) = delete;

          private:
            uint64_t& target_;
            uint64_t* max_target_;
            bool active_;
            std::chrono::steady_clock::time_point start_;
        };
    } // namespace

    bool d3d9_host::build_sampler(const uint64_t device, const uint32_t sampler_index, const uint32_t mip_levels, uint64_t& out_sampler)
    {
        const scoped_ns_accumulator _prof(g_build_sampler_ns, drawprofile_enabled_flag());
        out_sampler = 0;
        // Both call sites bound sampler_index to a real stage (0..max_ps_sampler_stages-1 for the pixel
        // loop, d3dvertextexturesampler0 + 0..max_vs_sampler_stages-1 for the vertex one).
        sampler_memo_entry& memo =
            this->sampler_memo_[sampler_index < max_ps_sampler_stages ? sampler_index
                                                                      : max_ps_sampler_stages + (sampler_index - d3dvertextexturesampler0)];
        // A zero sampler is the "never resolved" state -- a real VkSampler is never 0 (see below) -- which
        // is what makes a zero starting sampler_state_version safe.
        if (memo.sampler != 0 && memo.sampler_state_version == this->state_.sampler_state_version && memo.mip_levels == mip_levels)
        {
            out_sampler = memo.sampler;
            return true;
        }

        const auto& ss = this->state_.sampler_state;
        // Defaults match real D3D9's own documented per-D3DSAMPLERSTATETYPE defaults (D3DSAMP_MAGFILTER/
        // MINFILTER default to D3DTEXF_POINT, MIPFILTER to D3DTEXF_NONE, ADDRESSU/V/W to D3DTADDRESS_WRAP,
        // MAXANISOTROPY to 1), confirmed live in this task's own default-init capture (HANDOFF_MACBOOK.md).
        const uint32_t mag_filter = d3d9_filter_to_vk_filter(sampler_state_or(ss, sampler_index, d3dsamp_magfilter, 1));
        const uint32_t min_filter = d3d9_filter_to_vk_filter(sampler_state_or(ss, sampler_index, d3dsamp_minfilter, 1));
        const uint32_t mipmap_mode = d3d9_mip_filter_to_vk_mipmap_mode(sampler_state_or(ss, sampler_index, d3dsamp_mipfilter, 0));
        const uint32_t address_u = d3d9_address_to_vk(sampler_state_or(ss, sampler_index, d3dsamp_addressu, 1));
        const uint32_t address_v = d3d9_address_to_vk(sampler_state_or(ss, sampler_index, d3dsamp_addressv, 1));
        const uint32_t address_w = d3d9_address_to_vk(sampler_state_or(ss, sampler_index, d3dsamp_addressw, 1));
        const uint32_t max_anisotropy = sampler_state_or(ss, sampler_index, d3dsamp_maxanisotropy, 1);

        // Real LOD range from the bound texture's actual mip count and the app's sampler state. The mip
        // chain has mip_levels levels (0..last_level); D3DSAMP_MAXMIPLEVEL (default 0) is the most-detailed
        // level the sampler may use, i.e. it clamps min_lod. D3DSAMP_MIPFILTER == D3DTEXF_NONE (0) disables
        // mip selection entirely, so the sampler is pinned to that single MAXMIPLEVEL level (min==max_lod);
        // POINT/LINEAR let the GPU pick across [MAXMIPLEVEL, last_level] by screen-space derivative. For a
        // single-mip texture (mip_levels<=1) this collapses to min==max_lod==0, identical to the old
        // hardcoded pinning -- so existing single-mip resources are byte-for-byte unaffected. mipmap_mode
        // (above) already carries the app's D3DSAMP_MIPFILTER POINT-vs-LINEAR choice for the >NONE cases.
        const uint32_t mip_filter = sampler_state_or(ss, sampler_index, d3dsamp_mipfilter, 0);
        const uint32_t max_mip_level = sampler_state_or(ss, sampler_index, d3dsamp_maxmiplevel, 0);
        const uint32_t last_level = mip_levels > 0 ? mip_levels - 1 : 0;
        const float min_lod = static_cast<float>(std::min(max_mip_level, last_level));
        const float max_lod = mip_filter == 0 /*D3DTEXF_NONE*/ ? min_lod : static_cast<float>(last_level);

        const uint32_t anisotropy_enable = max_anisotropy > 1 ? 1 : 0;

        // Every argument below that create_sampler varies the resulting VkSampler on is folded into the
        // cache key; the rest (compare_enable/compare_op/border_color/mip_lod_bias) are hardcoded
        // constants here, so they never distinguish two states and are left out of the key.
        const sampler_cache_key key{mag_filter, min_filter, mipmap_mode,       address_u,
                                    address_v,  address_w,  anisotropy_enable, static_cast<float>(max_anisotropy),
                                    min_lod,    max_lod};
        if (const auto it = this->sampler_cache_.find(key); it != this->sampler_cache_.end())
        {
            out_sampler = it->second;
            memo = {.sampler_state_version = this->state_.sampler_state_version, .mip_levels = mip_levels, .sampler = out_sampler};
            return true;
        }

        // D3DSAMP_BORDERCOLOR is captured into sampler_state (see the UMD's tss_key table) but never read
        // here -- border_color is hardcoded to transparent-black, matching D3D9's own default. vulkan_host
        // ::create_sampler only accepts discrete VkBorderColor buckets (transparent/opaque black/white; the
        // bridge doesn't forward VK_EXT_custom_border_color), so an arbitrary ARGB border color can't be
        // represented faithfully with the current wrapper regardless. Only matters once D3DTADDRESS_BORDER
        // is actually used with a non-default border color.
        if (this->vulkan_.create_sampler(device, mag_filter, min_filter, address_u, address_v, address_w, mipmap_mode,
                                         /*compare_enable=*/0, /*compare_op=*/0, anisotropy_enable, /*border_color=*/0,
                                         /*mip_lod_bias=*/0.0f, static_cast<float>(max_anisotropy), min_lod, max_lod, out_sampler) != 0 ||
            out_sampler == 0)
        {
            out_sampler = 0;
            return false;
        }

        this->sampler_cache_.emplace(key, out_sampler);
        memo = {.sampler_state_version = this->state_.sampler_state_version, .mip_levels = mip_levels, .sampler = out_sampler};
        return true;
    }

    bool d3d9_host::ensure_depth_stencil_view(const uint64_t device, resource_entry& ds_entry, const uint32_t depth_format)
    {
        if (ds_entry.vk_image_view_id != 0)
        {
            return true;
        }

        const uint32_t depth_aspect = depth_aspect_mask(depth_format);
        if (this->vulkan_.create_image_view(device, ds_entry.vk_image_id, depth_format, depth_aspect, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                            VK_COMPONENT_SWIZZLE_IDENTITY, ds_entry.vk_image_view_id) != 0 ||
            ds_entry.vk_image_view_id == 0)
        {
            return false;
        }

        // First use of this depth-stencil resource: it starts VK_IMAGE_LAYOUT_UNDEFINED with
        // undefined contents (D3DCLEAR_ZBUFFER isn't wired to a real clear yet). Clear once to D3D9's
        // own default far-plane depth (1.0) so the first draw's comparisons are well-defined, then
        // leave it in DEPTH_STENCIL_ATTACHMENT_OPTIMAL for cmd_begin_rendering's LOAD_OP_LOAD below to
        // pick up -- mirrors the color attachment's own accumulate-across-draws convention, so depth
        // persists across every draw until this resource is destroyed.
        const vulkan_host::subresource_range depth_range{
            .aspect_mask = depth_aspect, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // Flush any open batch first: the clear below goes out on the SHARED command_buffer_ with its own
        // immediate submit+wait, which carries no ordering against a batch that is recorded but not yet
        // submitted -- it would race (and, being a full-image clear, could land after) that batch's draws.
        // Once per depth-stencil resource, since the view check above short-circuits every later call.
        this->flush_batch();
        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, ds_entry.vk_image_id, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth_range);
        this->vulkan_.cmd_clear_depth_stencil_image(this->command_buffer_, ds_entry.vk_image_id, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1.0f,
                                                    0, depth_range);
        this->vulkan_.cmd_pipeline_barrier(
            this->command_buffer_, ds_entry.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depth_range);
        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);
        this->vulkan_.wait_for_fence(this->fence_, UINT64_MAX);
        return true;
    }

    void d3d9_host::batch_clear_depth_stencil_image(resource_entry& ds_entry, const uint32_t depth_format, const uint32_t clear_aspects,
                                                    const float depth, const uint32_t stencil)
    {
        const uint32_t full_aspect = depth_aspect_mask(depth_format);
        const uint32_t aspects = clear_aspects & full_aspect;
        if (aspects == 0)
        {
            return; // e.g. D3DCLEAR_STENCIL against a depth-only format -- nothing this image can clear
        }
        const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];
        const vulkan_host::subresource_range barrier_range{
            .aspect_mask = full_aspect, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        const vulkan_host::subresource_range clear_range{
            .aspect_mask = aspects, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        this->vulkan_.cmd_pipeline_barrier(
            batch_cmd, ds_entry.vk_image_id, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, barrier_range);
        this->vulkan_.cmd_clear_depth_stencil_image(batch_cmd, ds_entry.vk_image_id, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth, stencil,
                                                    clear_range);
        this->vulkan_.cmd_pipeline_barrier(
            batch_cmd, ds_entry.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, barrier_range);
    }

    // The single alignment used for every arena slice (vertex/index/UBO), because it is simultaneously
    // >= every per-usage requirement this one buffer serves: >= the real minUniformBufferOffsetAlignment
    // reported by every target device (so a UBO slice is a legal VkDescriptorBufferInfo offset), >= any
    // index-element size 2/4 (so an index slice is a legal cmd_bind_index_buffer offset), and core Vulkan
    // imposes no vertex-buffer bind-offset alignment at all. One constant that satisfies all three makes
    // every slice trivially correct, at negligible waste for realistic per-draw range counts. execute_draw's
    // own draw_arena_bytes pre-count must round with this exact same constant -- see its own comment.
    constexpr size_t arena_alignment = 256;

    bool d3d9_host::arena_suballoc(frame_arena& arena, const size_t size, size_t& out_offset)
    {
        const size_t aligned_size = (size + arena_alignment - 1) & ~(arena_alignment - 1);
        if (arena.offset + aligned_size > arena.capacity)
        {
            // Grow-only, high-water-mark growth (new capacity = max(needed, 2 * old)). This is
            // DELIBERATELY different from ensure_pooled_buffer's former exact-fit, no-headroom growth: a
            // pool held exactly one range per slot, so exact-fit never re-grew once a slot reached its
            // steady-state size. This arena instead packs MANY ranges whose combined size varies per
            // draw, so doubling amortizes the destroy/recreate cost to O(log) reallocations over a run
            // instead of one reallocation per size increase. Growth destroys the current buffer, so every
            // slice a draw needs must be reserved (this called for all of them) BEFORE any bytes are
            // uploaded into the arena -- execute_draw does exactly that (reserve phase, then upload phase).
            const size_t needed = arena.offset + aligned_size;
            if (!this->grow_arena(arena, std::max(needed, arena.capacity * 2)))
            {
                return false;
            }
        }
        out_offset = arena.offset;
        arena.offset += aligned_size;
        return true;
    }

    bool d3d9_host::grow_arena(frame_arena& arena, const size_t new_capacity)
    {
        uint64_t new_buffer = 0;
        if (this->vulkan_.create_buffer(this->vk_device_, new_capacity,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        new_buffer) != 0 ||
            new_buffer == 0)
        {
            return false;
        }
        uint64_t mem_size = 0;
        uint64_t mem_align = 0;
        uint32_t mem_type_bits = 0;
        this->vulkan_.get_buffer_memory_requirements(this->vk_device_, new_buffer, mem_size, mem_align, mem_type_bits);
        const uint32_t memory_type = find_memory_type_index(this->vulkan_, this->vk_physical_device_, mem_type_bits,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uint64_t new_memory = 0;
        if (memory_type == UINT32_MAX || this->vulkan_.allocate_memory(this->vk_device_, mem_size, memory_type, new_memory) != 0 ||
            new_memory == 0)
        {
            this->vulkan_.destroy_buffer(this->vk_device_, new_buffer);
            return false;
        }
        this->vulkan_.bind_buffer_memory(this->vk_device_, new_buffer, new_memory, 0);

        // Host-coherent memory is mapped once, persistently, instead of via upload_memory's
        // per-call vkMapMemory/vkUnmapMemory round trip -- that round trip (not the memcpy itself)
        // dominated execute_draw's vertex/index/UBO upload cost in live profiling. No explicit
        // flush is needed on unmap/write since HOST_COHERENT guarantees visibility to the GPU as
        // of the next queue submission, and every upload here happens-before submit_batch_async().
        void* mapped_ptr = nullptr;
        uint64_t mapped_size = 0;
        if (this->vulkan_.map_memory(this->vk_device_, new_memory, mapped_ptr, mapped_size) != 0 || !mapped_ptr)
        {
            this->vulkan_.destroy_buffer(this->vk_device_, new_buffer);
            this->vulkan_.free_memory(this->vk_device_, new_memory);
            return false;
        }

        if (arena.buffer != 0)
        {
            this->vulkan_.unmap_memory(this->vk_device_, arena.memory);
            this->vulkan_.destroy_buffer(this->vk_device_, arena.buffer);
            this->vulkan_.free_memory(this->vk_device_, arena.memory);
        }
        arena.buffer = new_buffer;
        arena.memory = new_memory;
        arena.capacity = new_capacity;
        arena.mapped = mapped_ptr;
        return true;
    }

    namespace
    {
        // Temporary diagnostic (EMULATOR_D3D9_DRAWPROFILE=1): wall-clock breakdown of execute_draw's own
        // phases, to measure where real per-draw CPU cost actually goes instead of reasoning about it
        // from code reading -- five independent Lock/Unlock/flush round-trip-reduction fixes in a row
        // measured zero FPS impact on real MW2 gameplay, so this profiles the other major candidate
        // (execute_draw's own per-draw pipeline/descriptor-set/upload work) directly.
        struct draw_profile_totals
        {
            uint64_t calls{};
            uint64_t setup_ns{};   // entry through pipeline/RT/depth-stencil-view setup
            uint64_t reserve_ns{}; // Phase A: resolve+reserve arena slices
            // Sub-phases of reserve_ns, in execution order; they sum to it bar the timestamp overhead.
            uint64_t resolve_ns{};     // stream/index byte-source resolution + per-draw byte-range narrowing
            uint64_t ubo_build_ns{};   // build_ubo_staging: zero-fill + memcpy + equality compare, x6
            uint64_t precount_ns{};    // draw_arena_bytes pre-count
            uint64_t batch_ns{};       // batch flush/grow/reopen decision and the reopen work itself
            uint64_t suballoc_ns{};    // upload-cache checks + arena_suballoc for every slice
            uint64_t upload_ns{};      // Phase B: upload each reserved range
            uint64_t texdesc_ns{};     // texture upload/view/sampler setup + descriptor set writes
            uint64_t update_desc_ns{}; // subset of texdesc_ns: just the vulkan_.update_descriptor_sets call
            uint64_t record_ns{};      // barriers, begin-rendering, binds, the draw call itself
        };

        draw_profile_totals g_draw_profile{};

        bool draw_profile_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_DRAWPROFILE") != nullptr;
            return enabled;
        }

        // Temporary diagnostic (EMULATOR_D3D9_PASSDIAG=1): how often execute_draw's recording step has to
        // open a fresh dynamic-rendering instance, and which of the two reasons forced it -- a colour/depth
        // attachment change, or a difference in the set of render targets the draw samples as textures.
        // Each open costs a full tile store-and-reload of every attachment on a tile-based GPU, so the
        // rate is what sizes any change to the instance-merging rule.
        struct pass_diag_totals
        {
            uint64_t draws{};
            uint64_t open_attachment{};
            uint64_t open_sampled_rt{};
            uint64_t reuse{};
        };

        pass_diag_totals g_pass_diag{};

        bool pass_diag_enabled()
        {
            static const bool enabled = getenv("EMULATOR_D3D9_PASSDIAG") != nullptr;
            return enabled;
        }

        void pass_diag_maybe_report()
        {
            if (g_pass_diag.draws == 0 || g_pass_diag.draws % 2000 != 0)
            {
                return;
            }
            const auto pct = [](const uint64_t part) { return 100.0 * static_cast<double>(part) / static_cast<double>(g_pass_diag.draws); };
            fprintf(stderr, "[d3d9-passdiag] draws=%llu open_attachment=%llu (%.1f%%) open_sampled_rt=%llu (%.1f%%) reuse=%llu (%.1f%%)\n",
                    static_cast<unsigned long long>(g_pass_diag.draws), static_cast<unsigned long long>(g_pass_diag.open_attachment),
                    pct(g_pass_diag.open_attachment), static_cast<unsigned long long>(g_pass_diag.open_sampled_rt),
                    pct(g_pass_diag.open_sampled_rt), static_cast<unsigned long long>(g_pass_diag.reuse), pct(g_pass_diag.reuse));
        }

        void draw_profile_maybe_report()
        {
            if (g_draw_profile.calls != 0 && g_draw_profile.calls % 2000 == 0)
            {
                fprintf(stderr,
                        "[d3d9-drawprofile] calls=%llu setup=%.2fus reserve=%.2fus "
                        "(resolve=%.2fus ubo_build=%.2fus precount=%.2fus batch=%.2fus suballoc=%.2fus) "
                        "upload=%.2fus texdesc=%.2fus "
                        "(update_desc=%.2fus) record=%.2fus (avg/draw)\n",
                        static_cast<unsigned long long>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.setup_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.reserve_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.resolve_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.ubo_build_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.precount_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.batch_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.suballoc_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.upload_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.texdesc_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.update_desc_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls),
                        static_cast<double>(g_draw_profile.record_ns) / 1000.0 / static_cast<double>(g_draw_profile.calls));
            }
        }
    } // namespace

    int32_t d3d9_host::execute_draw(const uint32_t vertex_count, const uint32_t first_vertex, const indexed_draw* const indexed)
    {
        const bool profile = draw_profile_enabled();
        const bool pass_diag = pass_diag_enabled();
        const auto t_entry = std::chrono::steady_clock::now();
        ++this->draw_count_;

        const auto rt_it = this->resources_.find(this->state_.render_targets[0]);
        if (rt_it == this->resources_.end() || rt_it->second.vk_image_id == 0)
        {
            ++this->stats_.drop_no_render_target;
            return d3d_ok; // no bound render target with GPU backing -- nothing to draw into yet
        }
        auto& rt = rt_it->second;

        // Per-D3D9-slot render target list across slots 0-3 (D3D9's simultaneous-render-target slots),
        // indexed by slot -- NOT compacted. vkd3d-shader compiles the PS's oC0..oC3 outputs to FIXED
        // SPIR-V locations taken from the D3D9 output signature, independent of which slots happen to
        // be bound at draw time (e.g. SetRenderTarget(0, A); SetRenderTarget(2, B) is legal D3D9 and
        // already supported by the wire handler with no contiguity check). Attachment array index N
        // must always be D3D9 RT slot N, so a gap (an unbound slot below the highest bound one) gets a
        // real entry in the array with entry == nullptr / vk_format == 0 (VK_FORMAT_UNDEFINED), which
        // both create_graphics_pipeline's color_formats and cmd_begin_rendering's color attachments
        // already treat as "this attachment slot is unused" (VkFormat(0) / VkImageView(VK_NULL_HANDLE)
        // are the documented Vulkan placeholders for exactly this). bound_rts is sized to one past the
        // highest bound+resolvable slot -- slot 0 (rt, checked above) is always index 0 when present.
        // Render-area dimensions still come from slot 0 alone (real D3D9 requires every simultaneously-
        // bound RT to share dimensions).
        struct slot_render_target
        {
            resource_entry* entry{}; // nullptr = this D3D9 RT slot isn't bound/resolvable (a gap)
            uint32_t vk_format{};    // 0 (VK_FORMAT_UNDEFINED) when entry == nullptr
            bool srgb{};             // attach through the format's _SRGB view (D3DRS_SRGBWRITEENABLE)
        };

        // D3DRS_SRGBWRITEENABLE: the pixel shader's linear output is encoded to sRGB on the way into the
        // render target. A title that does its own gamma decode in the shader (squaring every sampled
        // albedo, which is what makes its lighting maths linear) relies on this render state alone to
        // get back to display space -- ignoring it leaves the whole 3D scene stored at roughly the
        // square of its intended brightness, i.e. near-black everywhere but the brightest highlights,
        // while 2D/HUD draws (which set the state to 0) come out perfectly correct.
        const bool srgb_write = render_state_or(this->state_.render_state, d3drs_srgbwriteenable, 0) != 0;

        // Matches device_state::render_targets's own std::array<uint64_t, 4> size (d3d9_host.hpp).
        std::array<slot_render_target, 4> rt_slots{};
        size_t bound_rt_count = 0; // one past the highest bound+resolvable slot index
        for (size_t slot = 0; slot < this->state_.render_targets.size(); ++slot)
        {
            const uint64_t rt_handle = this->state_.render_targets[slot];
            if (rt_handle == 0)
            {
                continue;
            }
            const auto bound_it = this->resources_.find(rt_handle);
            uint32_t bound_vk_format = 0;
            if (bound_it == this->resources_.end() || bound_it->second.vk_image_id == 0 ||
                !d3d9_format_to_vulkan(bound_it->second.format, bound_vk_format))
            {
                continue;
            }
            // An sRGB counterpart only exists for the 8-bit RGB(A) formats; every other render target
            // keeps its linear format, matching real D3D9 ignoring the render state for those.
            uint32_t srgb_vk_format = 0;
            const bool use_srgb = srgb_write && d3d9_format_to_vulkan_srgb(bound_it->second.format, srgb_vk_format);
            rt_slots[slot] = {.entry = &bound_it->second, .vk_format = use_srgb ? srgb_vk_format : bound_vk_format, .srgb = use_srgb};
            bound_rt_count = slot + 1;
        }
        const std::span<const slot_render_target> bound_rts(rt_slots.data(), bound_rt_count);
        std::vector<uint32_t> color_formats;
        color_formats.reserve(bound_rts.size());
        for (const auto& brt : bound_rts)
        {
            color_formats.push_back(brt.vk_format);
        }

        // A bound depth-stencil resource only participates once it has real GPU backing (create_resource
        // gives one to every render-target/depth-stencil-usage resource) and a format this host actually
        // understands. depth_vk_format stays 0 -- and every depth-testing code path below is skipped --
        // for every draw that never calls SetDepthStencil, which is every existing test.
        resource_entry* ds_entry = nullptr;
        uint32_t depth_vk_format = 0;
        if (this->state_.depth_stencil != 0)
        {
            const auto ds_it = this->resources_.find(this->state_.depth_stencil);
            if (ds_it != this->resources_.end() && ds_it->second.vk_image_id != 0 &&
                d3d9_format_to_vulkan(ds_it->second.format, depth_vk_format))
            {
                ds_entry = &ds_it->second;
            }
            else
            {
                depth_vk_format = 0;
            }
        }

        // Only trust a resource that was actually created as a vertex/index buffer. Runtime-assigned
        // DDI handles for D3DPOOL_DEFAULT vertex buffers (see class comment) are small sequential values
        // from a handle space independent of our own resource ids, and can coincidentally collide with
        // an unrelated resource id -- without this guard that collision would silently draw garbage from
        // whatever resource happens to share the number.
        // A DrawPrimitiveUP/DrawIndexedPrimitiveUP stream 0 carries its vertex bytes inline in
        // stream_um_data (see the d3d9_set_stream_source_um handler) rather than a resource id, so the
        // resource-id guard below only applies when stream 0 is not UM-backed.
        const auto stream0_um_it = this->state_.stream_um_data.find(0);
        const bool stream0_um = stream0_um_it != this->state_.stream_um_data.end() && !stream0_um_it->second.empty();
        if (!stream0_um)
        {
            const auto vb_it = this->resources_.find(this->state_.stream_sources[0]);
            if (vb_it == this->resources_.end() || vb_it->second.backing.empty() ||
                (vb_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) &&
                 vb_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer)))
            {
                ++this->stats_.drop_no_vertex_data;
                return d3d_ok; // no real vertex data bound
            }
        }

        const resource_entry* ib_entry = nullptr;
        const std::vector<std::byte>* ib_um_bytes = nullptr;
        // 0 = not resource-backed (DrawIndexedPrimitiveUP's inline ib_um_bytes) -- never cacheable, same
        // sentinel convention as reserved_range::resource_id below.
        uint64_t ib_resource_id = 0;
        if (indexed != nullptr)
        {
            if (!this->state_.index_um_data.empty())
            {
                ib_um_bytes = &this->state_.index_um_data; // DrawIndexedPrimitiveUP: inline index bytes
            }
            else
            {
                const auto ib_it = this->resources_.find(indexed->index_buffer);
                // Same vertex_buffer/index_buffer kind ambiguity as the stream_sources[0] guard above --
                // the UMD's resolve_buffer_resource_id resolves every buffer (vertex or index) with kind
                // vertex_buffer, so index buffers are only ever seen with that kind in practice.
                if (ib_it == this->resources_.end() || ib_it->second.backing.empty() ||
                    (ib_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) &&
                     ib_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer)))
                {
                    ++this->stats_.drop_no_vertex_data;
                    return d3d_ok; // no real index data bound
                }
                ib_entry = &ib_it->second;
                ib_resource_id = indexed->index_buffer;
            }
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return d3d_ok; // GPU unavailable; degrade silently like the rest of this host does
        }

        const bool use_programmable = this->state_.vertex_shader != 0 && this->state_.pixel_shader != 0;
        const programmable_pipeline_entry* programmable = nullptr;
        if (use_programmable)
        {
            programmable = this->ensure_programmable_pipeline(color_formats, rt.width, rt.height, depth_vk_format);
            if (programmable == nullptr)
            {
                ++this->stats_.drop_no_pipeline;
                return d3d_ok; // translation/pipeline failure; degrade silently
            }
        }
        else if (!this->ensure_pipeline(color_formats, rt.width, rt.height, depth_vk_format))
        {
            ++this->stats_.drop_no_pipeline;
            return d3d_ok;
        }

        // The attachment view for a slot: the resource's linear view normally, its sRGB view while
        // D3DRS_SRGBWRITEENABLE is on. Both are cached on the resource and created on first use.
        const auto attachment_view = [](const slot_render_target& brt) -> uint64_t& {
            return brt.srgb ? brt.entry->vk_image_view_srgb_id : brt.entry->vk_image_view_id;
        };
        for (auto& brt : rt_slots)
        {
            if (brt.entry == nullptr || attachment_view(brt) != 0)
            {
                continue;
            }
            if (this->vulkan_.create_image_view(device, brt.entry->vk_image_id, brt.vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                attachment_view(brt)) != 0 ||
                attachment_view(brt) == 0)
            {
                return d3d_ok;
            }
        }

        if (ds_entry != nullptr && !this->ensure_depth_stencil_view(device, *ds_entry, depth_vk_format))
        {
            return d3d_ok;
        }

        // Which vertex streams this draw actually needs to bind: the real parsed declaration's
        // usable_vertex_binding_mask (streams it references AND that have a real, nonzero stride) when
        // state_.vertex_decl is a real, non-empty declaration, else the pre-Task-8 stream-0-only
        // fallback. This is the exact same filtered mask ensure_programmable_pipeline just used to build
        // this pipeline's vertex input state, so the two can never disagree about which bindings the
        // pipeline actually declared.
        const parsed_vertex_decl* real_decl = this->find_real_vertex_decl();
        const uint32_t used_binding_mask = real_decl != nullptr ? this->usable_vertex_binding_mask(*real_decl) : 1u;
        uint32_t highest_binding = 0;
        for (uint32_t bit = 0; bit < 32; ++bit)
        {
            if ((used_binding_mask & (1u << bit)) != 0)
            {
                highest_binding = bit;
            }
        }

        const auto t_setup_done = profile ? std::chrono::steady_clock::now() : t_entry;

        // Phase A -- RESOLVE every arena slice this draw needs (vertex streams, index buffer, the six
        // UBOs), then (after the batch-management decision below) RESERVE them all WITHOUT uploading
        // anything. arena_suballoc grows the arena by destroy+recreate, which invalidates the buffer id
        // and any bytes already written, so all reservations must complete before the first upload
        // (phase B) reads the now-final arena buffer. Resolving the byte sources up front also lets the
        // batch-management step size this draw's total arena need and flush (never grow the arena while a
        // batch still holds recorded commands referencing the old buffer). Streams the declaration
        // doesn't reference, or that lack a real stride (both already filtered out of used_binding_mask
        // above), or a referenced+usable stream the app never called SetStreamSource for, are left at
        // buffer id 0 -- cmd_bind_vertex_buffers already maps that to VK_NULL_HANDLE.
        //
        // `arena` itself is bound further down, once the batch-management step below has settled which
        // slot (see batch_slot_count's header comment) this draw's batch is (re)opened on.

        struct reserved_range
        {
            uint32_t stream;
            const std::vector<std::byte>* bytes;
            size_t offset;
            // 0 = not resource-backed (DrawPrimitiveUP's inline stream_um_data) -- never cacheable, same
            // sentinel convention as ib_resource_id above (real ids from allocate_id() start at 0x10000).
            uint64_t resource_id{};
            uint64_t content_version{};
            bool cache_hit{false};
            // Byte sub-range of *bytes this draw actually needs (see upload_cache_entry's comment) --
            // defaults to the whole buffer; narrowed below for resource-backed streams where the app's
            // declared vertex range is known, so a draw referencing a handful of vertices out of a large
            // shared buffer doesn't pay to upload the whole thing.
            size_t range_start{};
            size_t range_end{};
        };

        // Live MW2 profiling found resource-backed vertex/index uploads routinely copying tens of MB per
        // draw while the draw itself only ever reads a few hundred vertices out of it (a large shared
        // buffer with many small sub-range draws) -- up to ~600x more bytes than needed. Bounding the
        // copy (and the cache-hit check) to the app's own declared vertex range fixes this without
        // touching bind offsets or the draw call itself: the arena reservation still starts logically at
        // resource byte 0, we simply don't bother writing (or reserving arena space for) bytes the GPU
        // will never fetch for this draw.
        const auto vertex_byte_range = [&](const uint32_t stream, const size_t total_bytes) -> std::pair<size_t, size_t> {
            const auto stride_it = this->state_.stream_strides.find(stream);
            const uint32_t stride = stride_it != this->state_.stream_strides.end() ? stride_it->second : 0;
            if (stride == 0)
            {
                return {0, total_bytes}; // unknown stride -- can't bound safely, upload everything
            }
            uint64_t vtx_lo = 0;
            uint64_t vtx_hi = 0;
            if (indexed != nullptr)
            {
                const int64_t base = static_cast<int64_t>(indexed->base_vertex_index) + indexed->min_vertex_index;
                vtx_lo = base > 0 ? static_cast<uint64_t>(base) : 0;
                vtx_hi = vtx_lo + indexed->num_vertices;
            }
            else
            {
                vtx_lo = first_vertex;
                vtx_hi = static_cast<uint64_t>(first_vertex) + vertex_count;
            }
            // The GPU fetches at bind_offset + i*stride, and bind_offset already folds in this stream's
            // own SetStreamSource byte offset (see stream_bind_offsets below) -- the actual resource-byte
            // range read is offset by that same amount, not just the vertex-index math above.
            const auto off_it = this->state_.stream_offsets.find(stream);
            const uint64_t d3d9_stream_offset = off_it != this->state_.stream_offsets.end() ? off_it->second : 0;
            const size_t start = std::min(static_cast<size_t>(d3d9_stream_offset + vtx_lo * stride), total_bytes);
            const size_t end = std::min(static_cast<size_t>(d3d9_stream_offset + vtx_hi * stride), total_bytes);
            return {start, std::max(start, end)};
        };

        // Streams bound to a resource with a real direct GPU buffer (see create_resource's eligibility
        // check) skip the arena entirely: no reservation, no upload memmove, no cache-hit bookkeeping --
        // the draw binds the resource's own buffer straight, at its own D3D9-level stream byte offset.
        // Vulkan's cmd_bind_vertex_buffers already takes one buffer+offset PER binding slot, so a draw
        // mixing direct-bound and arena-bound streams in the same call needs no special-casing beyond
        // filling stream_buffers/stream_bind_offsets from each source per slot (done below).
        std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> direct_streams; // stream -> {vk_buffer, byte_offset}

        std::vector<reserved_range> reserved_streams;
        for (uint32_t stream = 0; stream <= highest_binding; ++stream)
        {
            if ((used_binding_mask & (1u << stream)) == 0)
            {
                continue;
            }
            // Prefer a UM-backed (DrawPrimitiveUP) inline byte source for this stream; otherwise resolve
            // the resource-id-backed vertex buffer exactly as before. The two are mutually exclusive per
            // stream (each bind path clears the other -- see the set_stream_source[_um] handlers).
            const std::vector<std::byte>* src_bytes = nullptr;
            uint64_t src_resource_id = 0;
            uint64_t src_content_version = 0;
            const auto um_it = this->state_.stream_um_data.find(stream);
            if (um_it != this->state_.stream_um_data.end() && !um_it->second.empty())
            {
                src_bytes = &um_it->second;
            }
            else
            {
                const auto src_it = this->state_.stream_sources.find(stream);
                if (src_it == this->state_.stream_sources.end())
                {
                    continue;
                }
                const auto res_it = this->resources_.find(src_it->second);
                if (res_it == this->resources_.end() || res_it->second.backing.empty() ||
                    (res_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) &&
                     res_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer)))
                {
                    continue;
                }
                if (res_it->second.vk_direct_buffer_id != 0)
                {
                    const auto off_it = this->state_.stream_offsets.find(stream);
                    const uint64_t d3d9_stream_offset = off_it != this->state_.stream_offsets.end() ? off_it->second : 0;
                    direct_streams[stream] = {res_it->second.vk_direct_buffer_id, res_it->second.direct_slice_offset + d3d9_stream_offset};
                    continue;
                }
                src_bytes = &res_it->second.backing;
                src_resource_id = src_it->second;
                src_content_version = res_it->second.content_version;
            }
            const auto [range_start, range_end] =
                src_resource_id != 0 ? vertex_byte_range(stream, src_bytes->size()) : std::pair<size_t, size_t>{0, src_bytes->size()};
            reserved_streams.push_back({stream, src_bytes, 0, src_resource_id, src_content_version, false, range_start, range_end});
        }

        // A direct-buffer index resource (see create_resource's eligibility check and the matching vertex
        // stream handling above) skips the arena the same way: no reservation, no upload memmove, the
        // draw binds the resource's own buffer at offset 0 (D3D9's index buffer has no SetStreamSource-
        // style byte offset of its own -- SetIndices carries only the resource handle).
        const bool ib_direct = ib_um_bytes == nullptr && ib_entry != nullptr && ib_entry->vk_direct_buffer_id != 0;

        // UM-backed (DrawIndexedPrimitiveUP) inline index bytes take precedence over a resource-backed
        // index buffer; both upload identically into the arena. A direct-buffer resource never goes
        // through this path at all (see ib_direct above), so ib_bytes stays null for it.
        const std::vector<std::byte>* ib_bytes =
            ib_um_bytes != nullptr ? ib_um_bytes : (ib_entry != nullptr && !ib_direct ? &ib_entry->backing : nullptr);
        const uint64_t ib_content_version = ib_entry != nullptr ? ib_entry->content_version : 0;
        size_t ib_arena_offset = 0;
        bool ib_cache_hit = false;
        // Same range-bounding as vertex streams above, but along the index axis: this draw only ever
        // reads indices [first_index, first_index+index_count) (index_count is `vertex_count` here --
        // see this function's indexed-draw call site, which passes the index count through that
        // parameter). Only meaningful for a resource-backed index buffer (ib_resource_id != 0); UM-backed
        // inline index bytes are already exactly sized for this one draw.
        size_t ib_range_start = 0;
        size_t ib_range_end = ib_bytes != nullptr ? ib_bytes->size() : 0;
        if (ib_bytes != nullptr && ib_resource_id != 0 && indexed != nullptr)
        {
            const size_t index_size = indexed->index_format != 0 ? 4 : 2;
            const size_t start = std::min(static_cast<size_t>(indexed->first_index) * index_size, ib_bytes->size());
            const size_t end = std::min((static_cast<size_t>(indexed->first_index) + vertex_count) * index_size, ib_bytes->size());
            ib_range_start = start;
            ib_range_end = std::max(start, end);
        }

        const auto t_resolve_done = profile ? std::chrono::steady_clock::now() : t_setup_done;

        // D3D9 SM2/3 float constant-register caps (MaxVertexShaderConst = 256, fill_d3d9caps).
        constexpr size_t vs_ubo_size = 256 * 4 * sizeof(float);
        // The pixel stage has no D3DCAPS9 field of its own -- its float-constant count follows
        // PixelShaderVersion, which fill_d3d9caps reports as ps_3_0. ps_3_0 has 224 float constant
        // registers (c0..c223); 32 is the ps_2_0 number and would silently truncate everything above c31
        // in build_ubo_staging while also binding a descriptor range smaller than the block vkd3d-shader
        // declares for such a shader, so the out-of-range reads land in the neighbouring arena slices.
        constexpr size_t ps_ubo_size = 224 * 4 * sizeof(float);
        // D3D9 SM3 int/bool constant-register caps (16 registers each, both stages -- fill_d3d9caps),
        // each register expanded to a 16-byte slot (see vs/ps_const_i/b's own comments in d3d9_host.hpp).
        constexpr size_t int_bool_ubo_size = 16 * 4 * sizeof(uint32_t);

        // Indexed by ubo_index (d3d9_host.hpp) -- both the reservation/upload here and the descriptor
        // writes below read each UBO by those names.
        const std::array<size_t, 6> ubo_sizes{vs_ubo_size,       ps_ubo_size,       int_bool_ubo_size,
                                              int_bool_ubo_size, int_bool_ubo_size, int_bool_ubo_size};
        std::array<size_t, 6> ubo_offsets{};
        // Reused across draws (ubo_staging_ on the host object) rather than freshly allocated here -- each
        // of the six sizes above is a fixed D3D9 constant-register cap, never varying per draw or per
        // shader, so a fresh std::vector would never actually need a different size from call to call.
        std::array<std::vector<std::byte>, 6>& ubo_staging = this->ubo_staging_;
        // ubo_staging_[slot] holds the LAST-UPLOADED content for that slot, not necessarily this draw's
        // content: the candidate is built into ubo_scratch_[slot] first, and only copied over ubo_staging_
        // (replacing the last-uploaded snapshot) when it's actually different. A live MW2 profile found the
        // int/bool constant UBOs byte-identical across ~100% of consecutive draws and the pixel-shader float
        // UBO ~95% identical, so most draws can skip the arena reservation + upload_memory + descriptor
        // write below entirely and just reuse the previous draw's binding (see ubo_upload_cache_ and its
        // batch_generation_-gated reuse check in phase A) -- same shape of win as Task #161's vertex/index
        // upload cache, just keyed on content equality instead of a resource's content_version.
        //
        // Returns true if the content changed (staging was updated, so phase A must reserve a fresh arena
        // slice and phase B must re-upload), false if byte-identical to what's already at the cached offset.
        auto build_ubo_staging = [](std::vector<std::byte>& staging, std::vector<std::byte>& scratch, const size_t size,
                                    const auto& consts) {
            if (scratch.size() != size)
            {
                scratch.assign(size, std::byte{0});
            }
            else
            {
                std::ranges::fill(scratch, std::byte{0});
            }
            const size_t bytes = std::min(consts.size() * sizeof(*consts.data()), size);
            if (bytes > 0)
            {
                std::memcpy(scratch.data(), consts.data(), bytes);
            }
            if (staging.size() == size && staging == scratch)
            {
                return false;
            }
            staging.swap(scratch);
            return true;
        };
        std::array<std::vector<std::byte>, 6>& ubo_scratch = this->ubo_scratch_;
        // True for a slot this draw actually changed (build_ubo_staging updated ubo_staging_[slot]) -- see
        // the cache-hit check in phase A below, which reuses the previous draw's arena offset/upload for any
        // slot that's both unchanged AND still within the same batch_generation_.
        std::array<bool, 6> ubo_changed{};
        // Skips build_ubo_staging outright for a slot whose source registers haven't been written since the
        // build ubo_staging_[slot] currently holds -- untouched registers cannot produce different bytes, so
        // the zero-fill/memcpy/compare would always conclude "unchanged". An empty ubo_staging_[slot] (first
        // draw, or a size change) is never skipped, which is what makes a zero starting version safe.
        const auto build_slot = [&](const size_t slot, const size_t size, const auto& consts) {
            if (ubo_staging[slot].size() == size && this->ubo_built_version_[slot] == this->state_.const_versions[slot])
            {
                return false;
            }
            this->ubo_built_version_[slot] = this->state_.const_versions[slot];
            return build_ubo_staging(ubo_staging[slot], ubo_scratch[slot], size, consts);
        };
        if (use_programmable)
        {
            ubo_changed[ubo_vs_f] = build_slot(ubo_vs_f, vs_ubo_size, this->state_.vs_const_f);
            ubo_changed[ubo_ps_f] = build_slot(ubo_ps_f, ps_ubo_size, this->state_.ps_const_f);
            ubo_changed[ubo_vs_i] = build_slot(ubo_vs_i, int_bool_ubo_size, this->state_.vs_const_i);
            ubo_changed[ubo_ps_i] = build_slot(ubo_ps_i, int_bool_ubo_size, this->state_.ps_const_i);
            ubo_changed[ubo_vs_b] = build_slot(ubo_vs_b, int_bool_ubo_size, this->state_.vs_const_b);
            ubo_changed[ubo_ps_b] = build_slot(ubo_ps_b, int_bool_ubo_size, this->state_.ps_const_b);
        }

        const auto t_ubo_build_done = profile ? std::chrono::steady_clock::now() : t_setup_done;

        // Total arena bytes this draw's reservations will consume, rounded per slice with the exact same
        // arena_alignment arena_suballoc itself rounds with (a mismatch here would let this pre-count
        // underestimate the real requirement, letting arena_suballoc trigger a mid-batch growth that
        // destroys the buffer prior recorded draws reference). Lets the batch-management step below decide
        // whether this draw still fits in the open batch's arena without that growth.
        auto aligned = [](const size_t n) { return (n + arena_alignment - 1) & ~(arena_alignment - 1); };
        size_t draw_arena_bytes = 0;
        for (const auto& rs : reserved_streams)
        {
            draw_arena_bytes += aligned(rs.bytes->size());
        }
        if (ib_bytes != nullptr)
        {
            draw_arena_bytes += aligned(ib_bytes->size());
        }
        if (use_programmable)
        {
            for (const size_t s : ubo_sizes)
            {
                draw_arena_bytes += aligned(s);
            }
        }

        const auto t_precount_done = profile ? std::chrono::steady_clock::now() : t_setup_done;

        // Batch management -- decide whether to keep accumulating into the currently-open batch or close
        // it first, then (re)open a batch this draw records into. Depth-stencil draws batch on exactly the
        // same terms as colour ones; the only extra requirement they carry is the inter-draw depth
        // dependency the recording step below emits (see its comment).
        //   * A draw whose slot-0 render target differs from the open batch's closes it first, so a batch
        //     never mixes render targets. This is the ONLY thing enforcing that: the
        //     d3d9_set_render_target DDI handler deliberately does not drain the batch itself (see its
        //     own comment). The reopen below round-robins to the other slot (see batch_slot_count's
        //     header comment) exactly like the descriptor-pool trigger below, so an RT change costs a
        //     submit plus a one-slot-back wait rather than a full GPU drain.
        //   * A draw whose bound depth-stencil differs from the open batch's closes it first, for the same
        //     reason: the recording step's depth barrier only synchronizes the ONE depth image the batch
        //     accumulates into. Same round-robin-on-reopen behavior as the render-target check above.
        //   * A change to a non-slot-0 MRT slot leaves the batch open (batch_rt_ tracks slot 0 only) and
        //     is instead caught by the recording step's own render-pass attachment comparison, which
        //     opens a fresh dynamic-rendering instance inside the same batch -- cheaper still, and legal
        //     because the two instances are ordered by program order within one command buffer.
        //   * A programmable draw that would exceed the descriptor pool's per-batch capacity closes it
        //     first, so the pool can be reset (reset only happens on batch open, when it is idle). This is
        //     the trigger that actually fires in ordinary gameplay -- once every frame_desc_initial_draws
        //     draws into the same render target -- so it's the one the round-robin buys real CPU/GPU
        //     overlap for; see batch_slot_count's header comment for the full reasoning.
        //   * A draw whose arena slices would overflow the current arena capacity closes it first, then
        //     grows THIS SLOT's arena (amortized doubling) before reopening on the SAME slot rather than
        //     rotating. Growing destroys and recreates the buffer, which is only safe once this slot's own
        //     submission is provably complete -- so unlike the two triggers above, this one waits for that
        //     slot's own fence right here instead of deferring the wait to a later reopen.
        const uint64_t target_rt = this->state_.render_targets[0];
        const uint64_t target_ds = ds_entry != nullptr ? this->state_.depth_stencil : 0;
        bool rotate_batch_slot = false;
        const bool resetpool_diag = resetpool_diag_enabled();
        bool reopen_reason_rt_change = false;
        bool reopen_reason_desc_exhaustion = false;
        bool reopen_reason_arena_growth = false;
        if (this->batch_open_ && (target_rt != this->batch_rt_ || target_ds != this->batch_ds_))
        {
            this->submit_batch_async();
            rotate_batch_slot = true;
            reopen_reason_rt_change = true;
        }
        if (this->batch_open_ && use_programmable && this->frame_desc_capacity_draws_[this->batch_slot_] != 0 &&
            this->batch_draw_count_ + 1 > this->frame_desc_capacity_draws_[this->batch_slot_])
        {
            this->submit_batch_async();
            rotate_batch_slot = true;
            reopen_reason_desc_exhaustion = true;
        }
        if (this->batch_open_ && this->vertex_index_uniform_arena_[this->batch_slot_].offset + draw_arena_bytes >
                                     this->vertex_index_uniform_arena_[this->batch_slot_].capacity)
        {
            this->submit_batch_async();
            reopen_reason_arena_growth = true;
            this->wait_for_batch_slot(this->batch_slot_);
            frame_arena& growing_arena = this->vertex_index_uniform_arena_[this->batch_slot_];
            if (!this->grow_arena(growing_arena, std::max(draw_arena_bytes, growing_arena.capacity * 2)))
            {
                return d3d_ok;
            }
            growing_arena.offset = 0;
        }
        if (!this->batch_open_)
        {
            if (rotate_batch_slot)
            {
                this->batch_slot_ = (this->batch_slot_ + 1) % batch_slot_count;
                // Deferred wait: whatever this slot was last used for may still be finishing on the GPU --
                // wait it out now, before this call reuses its command buffer/descriptor pool/arena.
                // Usually already signaled by the time control gets back here, which is the entire point
                // of the round-robin (see batch_slot_count's header comment).
                this->wait_for_batch_slot(this->batch_slot_);
            }
            this->vertex_index_uniform_arena_[this->batch_slot_].offset = 0;
            if (this->frame_descriptor_pool_[this->batch_slot_] != 0)
            {
                if (resetpool_diag)
                {
                    const auto reset_start = std::chrono::steady_clock::now();
                    this->vulkan_.reset_descriptor_pool(device, this->frame_descriptor_pool_[this->batch_slot_], 0);
                    const auto elapsed_ns = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - reset_start).count());
                    resetpool_diag_report(elapsed_ns, reopen_reason_rt_change ? 1 : 0, reopen_reason_desc_exhaustion ? 1 : 0,
                                          reopen_reason_arena_growth ? 1 : 0);
                }
                else
                {
                    this->vulkan_.reset_descriptor_pool(device, this->frame_descriptor_pool_[this->batch_slot_], 0);
                }
            }
            this->batch_draw_count_ = 0;
            // Whatever render-pass instance this slot last had open was already closed by
            // submit_batch_async (close_render_pass) before its batch could be submitted, or never existed
            // (first use) -- reasserted here defensively rather than trusted, same belt-and-suspenders
            // convention as the RT/depth-stencil mismatch check above. Same for any deferred Clear() value
            // still sitting on pending_clear_ -- submit_batch_async already realizes it before this point.
            this->open_render_pass_[this->batch_slot_] = {};
            this->pending_clear_[this->batch_slot_] = {};
            this->vulkan_.reset_fence(device, this->batch_fence_[this->batch_slot_]);
            this->vulkan_.begin_command_buffer(this->batch_command_buffer_[this->batch_slot_], 0, false, 0, {}, 0, 0, 1, 0);
            this->batch_open_ = true;
            this->batch_rt_ = target_rt;
            this->batch_ds_ = target_ds;
            // Invalidates every vertex/index upload cache entry against this new batch (see
            // batch_generation_'s own comment) -- covers a plain close+reopen, a close+reopen that rotated
            // slots, and the close+grow_arena path above, which destroys and recreates
            // vertex_index_uniform_arena_[slot].buffer/memory entirely.
            ++this->batch_generation_;
        }

        const auto t_batch_done = profile ? std::chrono::steady_clock::now() : t_setup_done;

        // Bound here, now that batch_slot_ is settled for this draw -- every arena_suballoc/upload_memory
        // call below (phase A/B) and every batch_command_buffer_ use in the recording step further down
        // goes through these two, never the raw slot-indexed members.
        frame_arena& arena = this->vertex_index_uniform_arena_[this->batch_slot_];
        const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];

        // Reserve every arena slice now that the batch is open and the arena is guaranteed large enough
        // (either it already fit, or the overflow path above flushed and grew it while the GPU was idle).
        // A freshly-opened batch is empty, so any growth arena_suballoc still performs here is safe.
        //
        // Cache check (Task #161) happens here, not in the precount above: whether this draw's reservations
        // land in a continuing batch or a freshly (re)opened one -- and therefore what batch_generation_ is
        // -- is only settled by the flush/grow decisions just above. A cache hit reuses the exact offset the
        // matching resource was already uploaded to earlier in this same batch_generation_, so it neither
        // consumes new arena space nor performs a new arena_suballoc call.
        for (auto& rs : reserved_streams)
        {
            if (rs.resource_id != 0)
            {
                const upload_cache_entry& cached = this->stream_upload_cache_[rs.stream];
                if (cached.valid && cached.resource_id == rs.resource_id && cached.content_version == rs.content_version &&
                    cached.batch_generation == this->batch_generation_ && cached.range_start <= rs.range_start &&
                    cached.range_end >= rs.range_end)
                {
                    rs.offset = cached.arena_offset;
                    rs.cache_hit = true;
                    continue;
                }
            }
            if (!this->arena_suballoc(arena, rs.range_end, rs.offset))
            {
                return d3d_ok;
            }
        }
        if (ib_bytes != nullptr)
        {
            if (ib_resource_id != 0 && this->index_upload_cache_.valid && this->index_upload_cache_.resource_id == ib_resource_id &&
                this->index_upload_cache_.content_version == ib_content_version &&
                this->index_upload_cache_.batch_generation == this->batch_generation_ &&
                this->index_upload_cache_.range_start <= ib_range_start && this->index_upload_cache_.range_end >= ib_range_end)
            {
                ib_arena_offset = this->index_upload_cache_.arena_offset;
                ib_cache_hit = true;
            }
            else if (!this->arena_suballoc(arena, ib_range_end, ib_arena_offset))
            {
                return d3d_ok;
            }
        }
        std::array<bool, 6> ubo_cache_hit{};
        if (use_programmable)
        {
            for (size_t i = 0; i < ubo_offsets.size(); ++i)
            {
                const ubo_upload_cache_entry& cached = this->ubo_upload_cache_[i];
                if (!ubo_changed[i] && cached.valid && cached.batch_generation == this->batch_generation_)
                {
                    ubo_offsets[i] = cached.arena_offset;
                    ubo_cache_hit[i] = true;
                    continue;
                }
                if (!this->arena_suballoc(arena, ubo_sizes[i], ubo_offsets[i]))
                {
                    return d3d_ok;
                }
            }
        }

        const auto t_reserve_done = profile ? std::chrono::steady_clock::now() : t_entry;
        // Only advanced on the use_programmable path below; stays at t_reserve_done for fixed-function
        // draws, which have no separate upload checkpoint (see the final report's own comment).
        auto t_upload_done = t_reserve_done;
        auto t_texdesc_done = t_reserve_done;

        // Phase B -- every slice is reserved, so arena.buffer/memory are final. Upload each range at its
        // recorded offset and bind against the single shared arena buffer. A vertex stream's bind offset
        // is its arena slice offset plus any D3D9-level SetStreamSource start offset (into the resource),
        // preserving the pre-arena behavior of fetching the first vertex at that D3D9 offset.
        std::vector<uint64_t> stream_buffers(highest_binding + 1, 0);
        std::vector<uint64_t> stream_bind_offsets(highest_binding + 1, 0);
        for (const auto& rs : reserved_streams)
        {
            if (rs.cache_hit)
            {
                ++this->stats_.vertex_upload_skipped;
            }
            else
            {
                std::memcpy(static_cast<std::byte*>(arena.mapped) + rs.offset + rs.range_start, rs.bytes->data() + rs.range_start,
                            rs.range_end - rs.range_start);
                ++this->stats_.vertex_upload_done;
                if (uploadvol_diag_enabled())
                {
                    fprintf(stderr, "[d3d9-uploadvol-diag] kind=vertex bytes=%zu referenced_verts=%u\n", rs.range_end - rs.range_start,
                            vertex_count);
                }
                if (rs.resource_id != 0)
                {
                    this->stream_upload_cache_[rs.stream] = {.resource_id = rs.resource_id,
                                                             .content_version = rs.content_version,
                                                             .batch_generation = this->batch_generation_,
                                                             .arena_offset = rs.offset,
                                                             .range_start = rs.range_start,
                                                             .range_end = rs.range_end,
                                                             .valid = true};
                }
            }
            stream_buffers[rs.stream] = arena.buffer;
            const auto off_it = this->state_.stream_offsets.find(rs.stream);
            const uint32_t d3d9_stream_offset = off_it != this->state_.stream_offsets.end() ? off_it->second : 0;
            stream_bind_offsets[rs.stream] = rs.offset + d3d9_stream_offset;
        }
        // Direct-bound streams (see the reservation loop above) never went into reserved_streams, so
        // they never got a slot filled by the loop above either -- fill them here from the resource's
        // own buffer instead of the arena. No upload, no cache bookkeeping: the data is already exactly
        // where the guest's own Lock/Unlock wrote it.
        for (const auto& [stream, buffer_and_offset] : direct_streams)
        {
            stream_buffers[stream] = buffer_and_offset.first;
            stream_bind_offsets[stream] = buffer_and_offset.second;
        }

        uint64_t index_buffer_vk = 0;
        uint64_t index_buffer_bind_offset = 0;
        if (ib_direct)
        {
            // D3D9's SetIndices carries only the resource handle, no byte offset of its own (unlike
            // SetStreamSource) -- the whole resource is the index buffer, so the only offset that applies
            // is which ring slice is currently live.
            index_buffer_vk = ib_entry->vk_direct_buffer_id;
            index_buffer_bind_offset = ib_entry->direct_slice_offset;
        }
        else if (ib_bytes != nullptr)
        {
            if (ib_cache_hit)
            {
                ++this->stats_.index_upload_skipped;
            }
            else
            {
                std::memcpy(static_cast<std::byte*>(arena.mapped) + ib_arena_offset + ib_range_start, ib_bytes->data() + ib_range_start,
                            ib_range_end - ib_range_start);
                ++this->stats_.index_upload_done;
                if (uploadvol_diag_enabled())
                {
                    fprintf(stderr, "[d3d9-uploadvol-diag] kind=index bytes=%zu\n", ib_range_end - ib_range_start);
                }
                if (ib_resource_id != 0)
                {
                    this->index_upload_cache_ = {.resource_id = ib_resource_id,
                                                 .content_version = ib_content_version,
                                                 .batch_generation = this->batch_generation_,
                                                 .arena_offset = ib_arena_offset,
                                                 .range_start = ib_range_start,
                                                 .range_end = ib_range_end,
                                                 .valid = true};
                }
            }
            index_buffer_vk = arena.buffer;
        }

        std::array<uint64_t, max_ps_sampler_stages> tex_samplers{};
        std::array<uint64_t, max_ps_sampler_stages> tex_image_views{};
        // Separate from the PS arrays above so both stages' per-draw sampler/view selection is tracked
        // independently (a vertex texture and a pixel texture can be bound to the same shader-register
        // index k at once -- they live in different descriptor sets and different bound_textures keys).
        std::array<uint64_t, max_vs_sampler_stages> vs_tex_samplers{};
        std::array<uint64_t, max_vs_sampler_stages> vs_tex_image_views{};
        std::array<uint64_t, 2> descriptor_sets{};
        // Colour render targets this draw samples as textures (render-to-texture). They rest in
        // TRANSFER_SRC_OPTIMAL like every other render target here, so each needs a barrier into
        // SHADER_READ_ONLY_OPTIMAL before the render pass and back out after it -- the read-side mirror
        // of the write-side round trip the colour attachments themselves already do below.
        std::vector<resource_entry*> sampled_render_targets;
        if (use_programmable)
        {
            // Upload the six constant buffers into their reserved arena slices -- skipped for any slot
            // phase A found unchanged-and-cached (ubo_cache_hit), which already points ubo_offsets[i] at
            // the still-valid arena slice from an earlier draw in this same batch_generation_.
            for (size_t i = 0; i < ubo_offsets.size(); ++i)
            {
                if (ubo_cache_hit[i])
                {
                    continue;
                }
                std::memcpy(static_cast<std::byte*>(arena.mapped) + ubo_offsets[i], ubo_staging[i].data(), ubo_sizes[i]);
                if (uploadvol_diag_enabled())
                {
                    fprintf(stderr, "[d3d9-uploadvol-diag] kind=ubo slot=%zu bytes=%zu\n", i, ubo_sizes[i]);
                }
                this->ubo_upload_cache_[i] = {.batch_generation = this->batch_generation_, .arena_offset = ubo_offsets[i], .valid = true};
            }

            if (profile)
            {
                t_upload_done = std::chrono::steady_clock::now();
                g_draw_profile.upload_ns +=
                    static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t_upload_done - t_reserve_done).count());
            }

            // Combined-image-sampler bindings for texture stages s0..s3 (see ensure_programmable_pipeline's
            // ps_bindings comment). Each descriptor is only written when a real, GPU-backed texture is
            // actually bound to that stage; Vulkan permits an allocated descriptor set to leave a binding
            // unwritten as long as no bound pipeline's shader statically accesses it -- true for every stage
            // a PS doesn't sample (d3d9_shader_translator.cpp only emits a SPIR-V sampler variable for a stage
            // the PS actually reads). A PS that samples a stage with no texture bound remains a real gap, same
            // as the prior single-texture code, and is not exercised by any guest test yet.
            for (uint32_t stage = 0; stage < max_ps_sampler_stages; ++stage)
            {
                const auto tex_it = this->state_.bound_textures.find(stage);
                if (tex_it == this->state_.bound_textures.end() || tex_it->second == 0)
                {
                    continue;
                }
                const auto tex_res_it = this->resources_.find(tex_it->second);
                if (tex_res_it == this->resources_.end())
                {
                    continue;
                }
                resource_entry& tex = tex_res_it->second;
                // A depth-stencil surface bound as a texture -- a shadow map. Neither of the two paths
                // below can service it (is_samplable_render_target excludes DEPTHSTENCIL usage, and
                // ensure_texture_uploaded refuses it too), so it is counted here rather than being lost
                // in the generic upload-refused bucket: a shader that samples one gets an unwritten
                // descriptor, and every surface it lights comes out unlit.
                if ((tex.usage & d3dusage_depthstencil) != 0)
                {
                    ++this->stats_.sampler_skip_depth_stencil;
                    this->stats_.sampler_skip_formats[tex.format] += 1;
                    this->stats_.depth_stencil_skip_at_stage[stage] += 1;
                    continue;
                }
                // Render-to-texture: a colour render target bound as a texture. There is nothing to
                // upload -- its GPU image already holds the pixels a previous draw rendered into it --
                // so ensure_texture_uploaded (which refuses render-target-usage resources outright) must
                // NOT gate it. What it does need is a layout round trip, because a render target rests
                // in TRANSFER_SRC_OPTIMAL; it is collected into sampled_render_targets for that below.
                const bool rt_as_texture = is_samplable_render_target(tex.vk_image_id, tex.usage);
                if (rt_as_texture)
                {
                    // Sampling an image this same draw also renders into is a feedback loop -- undefined
                    // in D3D9 as much as in Vulkan. Skip the stage rather than record an illegal draw.
                    bool is_own_attachment = false;
                    for (const auto& brt : bound_rts)
                    {
                        is_own_attachment = is_own_attachment || brt.entry == &tex;
                    }
                    if (is_own_attachment)
                    {
                        continue;
                    }
                }
                else if (!this->ensure_texture_uploaded(tex_it->second))
                {
                    ++this->stats_.sampler_skip_upload_refused;
                    this->stats_.sampler_skip_formats[tex.format] += 1;
                    continue;
                }
                if (tex.vk_image_view_id == 0)
                {
                    uint32_t tex_vk_format = 0;
                    if (d3d9_format_to_vulkan(tex.format, tex_vk_format))
                    {
                        // Sampling view spans the texture's full mip chain (levelCount = mip_levels) so the
                        // sampler can select any level; single-mip textures still get levelCount 1. View
                        // type/layer-count follow the resource kind (cube/volume/2D) via the shared helper.
                        // A render target's image is always created single-mip, single-layer, plain 2D by
                        // vulkan_host::create_render_target no matter what the D3D9 resource claims, so its
                        // view must describe that image, not the D3D9 declaration.
                        const uint32_t view_levels = rt_as_texture ? 1u : std::max(1u, tex.mip_levels);
                        const sampled_view_shape view_shape =
                            rt_as_texture ? sampled_view_shape{VK_IMAGE_VIEW_TYPE_2D, 1} : sampled_view_shape_for_kind(tex.kind);
                        const auto swizzle = d3d9_format_to_vulkan_swizzle(tex.format);
                        this->vulkan_.create_image_view(device, tex.vk_image_id, tex_vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                        view_shape.view_type, 0, view_levels, 0, view_shape.layer_count, swizzle.r,
                                                        swizzle.g, swizzle.b, swizzle.a, tex.vk_image_view_id);
                    }
                }
                const uint32_t sampler_mips = rt_as_texture ? 1u : std::max(1u, tex.mip_levels);
                if (tex.vk_image_view_id == 0)
                {
                    ++this->stats_.sampler_skip_no_view;
                    this->stats_.sampler_skip_formats[tex.format] += 1;
                }
                if (tex.vk_image_view_id != 0 && this->build_sampler(device, stage, sampler_mips, tex_samplers[stage]))
                {
                    tex_image_views[stage] = tex.vk_image_view_id;
                    if (rt_as_texture)
                    {
                        sampled_render_targets.push_back(&tex);
                        this->stats_.rt_sampled_by_shader[{stage, tex_it->second, this->state_.pixel_shader}] += 1;
                    }
                }
            }

            // Vertex-stage texture fetch (SM3.0 tex2Dlod etc.): the guest binds these via
            // SetTexture(D3DVERTEXTEXTURESAMPLER0 + k, ...), which real d3d9.dll forwards through the DDI
            // unmodified, so the texture for VS sampler register k lives at bound_textures[257 + k]. This
            // mirrors the PS loop above exactly (same upload/view-creation/sampler-build), just keyed off
            // the vertex-sampler stage numbers and tracked in the separate vs_* arrays. build_sampler is
            // called with the raw stage (257 + k) so any D3DSAMP_* state the app set on that vertex
            // sampler is honored; with none set it defaults to POINT/no-mipmap, matching real D3D9's
            // vertex-texture-fetch filtering restrictions.
            for (uint32_t k = 0; k < max_vs_sampler_stages; ++k)
            {
                const uint32_t vs_stage = d3dvertextexturesampler0 + k;
                const auto tex_it = this->state_.bound_textures.find(vs_stage);
                if (tex_it == this->state_.bound_textures.end() || tex_it->second == 0 || !this->ensure_texture_uploaded(tex_it->second))
                {
                    continue;
                }
                const auto tex_res_it = this->resources_.find(tex_it->second);
                if (tex_res_it == this->resources_.end())
                {
                    continue;
                }
                resource_entry& tex = tex_res_it->second;
                if (tex.vk_image_view_id == 0)
                {
                    uint32_t tex_vk_format = 0;
                    if (d3d9_format_to_vulkan(tex.format, tex_vk_format))
                    {
                        const uint32_t view_levels = std::max(1u, tex.mip_levels);
                        const sampled_view_shape view_shape = sampled_view_shape_for_kind(tex.kind);
                        const auto swizzle = d3d9_format_to_vulkan_swizzle(tex.format);
                        this->vulkan_.create_image_view(device, tex.vk_image_id, tex_vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                        view_shape.view_type, 0, view_levels, 0, view_shape.layer_count, swizzle.r,
                                                        swizzle.g, swizzle.b, swizzle.a, tex.vk_image_view_id);
                    }
                }
                if (tex.vk_image_view_id != 0 && this->build_sampler(device, vs_stage, std::max(1u, tex.mip_levels), vs_tex_samplers[k]))
                {
                    vs_tex_image_views[k] = tex.vk_image_view_id;
                }
            }

            // Build the write list first, with dst_set holding the set INDEX rather than a set id, so it
            // can be compared against the previous draw's list before anything is allocated (see
            // descriptor_set_memo). Reused scratch: no per-draw heap allocation.
            // The UBO descriptors are dynamic (see ensure_programmable_pipeline's vs_bindings comment), so
            // each names its slice range from offset 0 and this draw's actual arena offset is supplied at
            // bind time in ubo_dynamic_offsets below. Keeping the offsets out of the writes is what lets
            // draws whose constants merely landed elsewhere in the arena still hit descriptor_set_memo.
            std::vector<vulkan_host::descriptor_write>& writes = this->draw_writes_;
            writes.assign({
                {.dst_set = 0,
                 .dst_binding = 0,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                 .buffer = arena.buffer,
                 .offset = 0,
                 .range = vs_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = 1,
                 .dst_binding = 0,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                 .buffer = arena.buffer,
                 .offset = 0,
                 .range = ps_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = 0,
                 .dst_binding = 2,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                 .buffer = arena.buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = 0,
                 .dst_binding = 3,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                 .buffer = arena.buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = 1,
                 .dst_binding = 2,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                 .buffer = arena.buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = 1,
                 .dst_binding = 3,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                 .buffer = arena.buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
            });
            for (uint32_t stage = 0; stage < max_ps_sampler_stages; ++stage)
            {
                if (tex_image_views[stage] == 0 || tex_samplers[stage] == 0)
                {
                    continue;
                }
                writes.push_back({.dst_set = 1,
                                  .dst_binding = ps_sampler_binding_for_stage(stage),
                                  .dst_array_element = 0,
                                  .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .buffer = 0,
                                  .offset = 0,
                                  .range = 0,
                                  .sampler = tex_samplers[stage],
                                  .image_view = tex_image_views[stage],
                                  .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            }
            // Vertex-stage samplers go into the VS descriptor set (set 0) at the same per-set binding
            // formula as PS -- see the vs_bindings layout and the vertex-texture upload loop above.
            for (uint32_t k = 0; k < max_vs_sampler_stages; ++k)
            {
                if (vs_tex_image_views[k] == 0 || vs_tex_samplers[k] == 0)
                {
                    continue;
                }
                writes.push_back({.dst_set = 0,
                                  .dst_binding = vs_sampler_binding_for_stage(k),
                                  .dst_array_element = 0,
                                  .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .buffer = 0,
                                  .offset = 0,
                                  .range = 0,
                                  .sampler = vs_tex_samplers[k],
                                  .image_view = vs_tex_image_views[k],
                                  .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            }
            descriptor_set_memo& memo = this->descriptor_memo_;
            const bool memo_hit = memo.valid && memo.batch_generation == this->batch_generation_ &&
                                  memo.vs_set_layout == programmable->vs_set_layout && memo.ps_set_layout == programmable->ps_set_layout &&
                                  memo.writes == writes;
            if (memo_hit)
            {
                descriptor_sets = memo.sets;
                ++this->stats_.descriptor_set_reused;
            }
            else
            {
                // Allocate a fresh per-draw VS/PS descriptor-set pair from the shared frame pool (reset on
                // batch open), against this pipeline's cached set layouts. The pool is reset only when a
                // batch opens, so each batched draw allocates 2 more sets against it without a reset; the
                // batch is flushed (closing it, so the next draw reopens and resets the pool) before
                // batch_draw_count_ could exceed frame_desc_capacity_draws_ -- see the batch-management
                // overflow guard above -- so allocation here always fits and ensure_frame_descriptor_pool's
                // growth path stays dormant.
                // The growth path in here destroys the slot's old pool, freeing every set the memo could
                // still be naming, so the memo must not survive a failure to build the replacement.
                if (!this->ensure_frame_descriptor_pool(device, this->batch_slot_, 1))
                {
                    memo.valid = false;
                    return d3d_ok; // GPU allocation failure; degrade silently like the rest of this host does
                }
                const std::array<uint64_t, 2> set_layouts{programmable->vs_set_layout, programmable->ps_set_layout};
                uint32_t set_count = 0;
                const bool allocset_diag = allocset_diag_enabled();
                int32_t allocset_result = 0;
                if (allocset_diag)
                {
                    const auto alloc_start = std::chrono::steady_clock::now();
                    allocset_result = this->vulkan_.allocate_descriptor_sets(device, this->frame_descriptor_pool_[this->batch_slot_],
                                                                             set_layouts, descriptor_sets, set_count);
                    const auto elapsed_ns = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - alloc_start).count());
                    allocset_diag_report(elapsed_ns);
                }
                else
                {
                    allocset_result = this->vulkan_.allocate_descriptor_sets(device, this->frame_descriptor_pool_[this->batch_slot_],
                                                                             set_layouts, descriptor_sets, set_count);
                }
                if (allocset_result != 0 || set_count != descriptor_sets.size())
                {
                    memo.valid = false;
                    return d3d_ok;
                }
                ++this->batch_draw_count_;
                ++this->stats_.descriptor_set_allocated;

                memo.writes = writes;
                for (vulkan_host::descriptor_write& w : writes)
                {
                    w.dst_set = descriptor_sets[w.dst_set];
                }

                const auto t_before_update_desc = profile ? std::chrono::steady_clock::now() : t_upload_done;
                this->vulkan_.update_descriptor_sets(device, writes);
                if (profile)
                {
                    g_draw_profile.update_desc_ns += static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t_before_update_desc)
                            .count());
                }

                memo.batch_generation = this->batch_generation_;
                memo.vs_set_layout = programmable->vs_set_layout;
                memo.ps_set_layout = programmable->ps_set_layout;
                memo.sets = descriptor_sets;
                memo.valid = true;
            }
        }

        if (profile)
        {
            t_texdesc_done = std::chrono::steady_clock::now();
            g_draw_profile.texdesc_ns +=
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t_texdesc_done - t_upload_done).count());
        }

        // Record into the open batch command buffer (begun in batch management above); the batch stays
        // open across draws and is submitted only by flush_batch.
        //
        // Assumes Clear always runs before the first Draw (true for this test's flow), which leaves
        // the image in TRANSFER_SRC_OPTIMAL (submit_clear's own documented post-state) -- transition to
        // COLOR_ATTACHMENT_OPTIMAL for rendering, then back for the readback below.
        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};

        // Consecutive draws into the exact same color attachments (same images AND same views -- an
        // sRGB-vs-linear view swap on an otherwise-unchanged image counts as a change) share ONE dynamic-
        // rendering instance instead of paying their own cmd_begin_rendering/cmd_end_rendering plus the
        // TRANSFER_SRC<->COLOR_ATTACHMENT round trip each. On a tile-based GPU (Apple Silicon) ending a
        // rendering instance forces its attachments' tile memory to be stored to VRAM, and beginning one
        // with LOAD_OP_LOAD forces a reload from VRAM -- paying that full-attachment round trip on every
        // single draw, as opposed to once per run of same-target draws, is the same shape of cost Task
        // #161's vertex/index upload cache eliminated for CPU-side uploads (see stream_upload_cache_'s
        // comment), just on the GPU side and per render-pass-instance rather than per byte range.
        //
        // A render-to-texture draw (sampled_render_targets non-empty) can share an instance too, as long as
        // the previous draw put exactly the same render targets into SHADER_READ_ONLY_OPTIMAL: the only
        // extra work such a draw needs is that set of layout transitions, and Vulkan only allows them
        // outside a rendering instance. MW2's gameplay draws are ~50% render-to-texture with the sampled
        // set almost always unchanged from the immediately preceding draw (measured via
        // EMULATOR_D3D9_PASSDIAG), so excluding them outright cost ~1070 instance close/reopen pairs per
        // frame -- the tile store-and-reload round trip this merge exists to avoid, at its worst.
        open_render_pass_state& rp = this->open_render_pass_[this->batch_slot_];
        std::vector<uint64_t> sampled_image_ids;
        sampled_image_ids.reserve(sampled_render_targets.size());
        for (const resource_entry* srt : sampled_render_targets)
        {
            sampled_image_ids.push_back(srt->vk_image_id);
        }
        bool attachments_match =
            rp.open && rp.color_count == bound_rts.size() && rp.depth_image_id == (ds_entry != nullptr ? ds_entry->vk_image_id : 0);
        for (size_t i = 0; attachments_match && i < bound_rts.size(); ++i)
        {
            const uint64_t want_image = bound_rts[i].entry != nullptr ? bound_rts[i].entry->vk_image_id : 0;
            const uint64_t want_view = bound_rts[i].entry != nullptr ? attachment_view(bound_rts[i]) : 0;
            attachments_match = rp.color_image_ids[i] == want_image && rp.color_view_ids[i] == want_view;
        }
        const bool can_continue_pass = attachments_match && rp.sampled_image_ids == sampled_image_ids;

        if (pass_diag)
        {
            ++g_pass_diag.draws;
            if (can_continue_pass)
            {
                ++g_pass_diag.reuse;
            }
            else if (!sampled_render_targets.empty())
            {
                ++g_pass_diag.open_sampled_rt;
            }
            else
            {
                ++g_pass_diag.open_attachment;
            }
        }

        if (!can_continue_pass)
        {
            this->close_render_pass(this->batch_slot_);
            // A Clear() deferred against a render-target set the app has since rebound cannot be folded
            // into this instance's load op -- doing so would clear whatever is bound NOW rather than what
            // the Clear() named. Realize it explicitly against its own snapshot instead, while the
            // instance is still closed (vkCmdClearColorImage is illegal inside one) and before the
            // attachment transitions below.
            pending_batch_clear& stale = this->pending_clear_[this->batch_slot_];
            if (stale.color_pending && stale.color_targets != this->state_.render_targets)
            {
                this->realize_pending_color_clear(stale);
            }
            for (const auto& brt : bound_rts)
            {
                if (brt.entry == nullptr)
                {
                    continue; // gap slot -- no real image to transition
                }
                this->vulkan_.cmd_pipeline_barrier(batch_cmd, brt.entry->vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, color_range);
            }
        }
        if (!can_continue_pass)
        {
            for (const uint64_t sampled_image : sampled_image_ids)
            {
                this->vulkan_.cmd_pipeline_barrier(batch_cmd, sampled_image, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                   VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, color_range);
            }
        }

        if (!can_continue_pass)
        {
            // A Clear() that ran since this slot's last render-pass instance closed (or since the batch
            // opened) may have deferred its value here instead of an explicit vkCmdClear*Image -- see
            // pending_clear_'s own comment. Folding it into this fresh instance's load op is free: the
            // attachment's tile memory is initialized to the clear value as part of beginning to render
            // into it regardless.
            pending_batch_clear& pending = this->pending_clear_[this->batch_slot_];
            const uint32_t color_load_op = pending.color_pending ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            const std::array<uint32_t, 4> color_clear = color_clear_value(pending.color_value);

            std::vector<vulkan_host::rendering_attachment> color_attachments;
            color_attachments.reserve(bound_rts.size());
            for (const auto& brt : bound_rts)
            {
                if (brt.entry == nullptr)
                {
                    // Gap slot: image_view == 0 (VK_NULL_HANDLE) marks this attachment index unused per
                    // VkRenderingAttachmentInfo's own documented semantics -- writes to this location are
                    // discarded, matching a PS that never writes this oC# in the first place.
                    color_attachments.push_back({});
                    continue;
                }
                color_attachments.push_back({
                    .image_view = brt.srgb ? brt.entry->vk_image_view_srgb_id : brt.entry->vk_image_view_id,
                    .resolve_image_view = 0,
                    .image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolve_image_layout = 0,
                    .resolve_mode = 0,
                    .load_op = color_load_op,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                    .clear_value = color_clear,
                });
            }
            // The one-time init above already left the depth image in DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            // and every later draw finds it already there (LOAD_OP_LOAD/STORE_OP_STORE keep it there
            // across draws) -- so no LAYOUT transition is needed here, unlike the color attachment's
            // transfer-src round trip. A pure execution+memory dependency still is, though, but only once
            // per fresh rendering instance: two draws inside the SAME instance (the merge case above) are
            // in the same subpass, where Vulkan's own implicit ordering already makes an earlier draw's
            // depth writes visible to a later one's depth test -- exactly how every ordinary Vulkan
            // renderer relies on it without an explicit barrier between draws. That guarantee does not
            // reach across two separate instances, so opening a fresh one here still needs it, to make
            // whatever a PRIOR instance (a previous batch, or a previous same-slot run before an
            // attachment change) wrote visible to this one.
            if (ds_entry != nullptr)
            {
                const vulkan_host::subresource_range depth_range{.aspect_mask = depth_aspect_mask(depth_vk_format),
                                                                 .base_mip_level = 0,
                                                                 .level_count = 1,
                                                                 .base_array_layer = 0,
                                                                 .layer_count = 1};
                this->vulkan_.cmd_pipeline_barrier(
                    batch_cmd, ds_entry->vk_image_id,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depth_range);
            }
            // A transient depth-stencil (see depth_stencil_transient_enabled's own comment for why this
            // is currently always false, i.e. depth_transient below is currently dead-but-exercised code)
            // only ever exists in tile memory for the render-pass instance that produced it: DONT_CARE is
            // what actually realizes MoltenVK's memoryless-attachment bandwidth win (a STORE would force
            // real VRAM backing to materialize, defeating the optimization) -- at the cost of a same-
            // batch render-pass reopen against the same target that ISN'T preceded by a fresh D3D9
            // Clear() (see depth_stencil_transient_enabled's comment for why that's a real, frequent case,
            // not a hypothetical one) reading undefined content via VK_ATTACHMENT_LOAD_OP_LOAD instead of
            // the real accumulated depth buffer -- the reason this stays disabled for now.
            const bool depth_transient =
                depth_stencil_transient_enabled && ds_entry != nullptr && (ds_entry->usage & d3dusage_depthstencil) != 0;
            const vulkan_host::rendering_attachment depth_attachment{
                .image_view = ds_entry != nullptr ? ds_entry->vk_image_view_id : 0,
                .resolve_image_view = 0,
                .image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .resolve_image_layout = 0,
                .resolve_mode = 0,
                .load_op = pending.depth_pending ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .store_op = depth_transient ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
                .clear_value = depth_stencil_clear_value(pending.depth_value, pending.stencil_value),
            };
            this->vulkan_.cmd_begin_rendering(batch_cmd, 0, 0, rt.width, rt.height, 1, 0, 0, color_attachments,
                                              ds_entry != nullptr ? &depth_attachment : nullptr, nullptr);

            rp.open = true;
            rp.color_count = bound_rts.size();
            for (size_t i = 0; i < bound_rts.size(); ++i)
            {
                rp.color_image_ids[i] = bound_rts[i].entry != nullptr ? bound_rts[i].entry->vk_image_id : 0;
                rp.color_view_ids[i] = bound_rts[i].entry != nullptr ? attachment_view(bound_rts[i]) : 0;
            }
            rp.depth_image_id = ds_entry != nullptr ? ds_entry->vk_image_id : 0;
            rp.sampled_image_ids = std::move(sampled_image_ids);
            pending = {};
        }

        this->vulkan_.cmd_bind_pipeline(batch_cmd, use_programmable ? programmable->pipeline : this->pipeline_,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS);

        if (use_programmable)
        {
            // Vulkan consumes pDynamicOffsets in ascending set order, then ascending binding order within a
            // set, counting only the dynamic descriptors -- so this is set 0's bindings 0/2/3 followed by
            // set 1's, and the combined-image-sampler bindings interleaved among them contribute nothing.
            const std::array<uint32_t, 6> ubo_dynamic_offsets{
                static_cast<uint32_t>(ubo_offsets[ubo_vs_f]), static_cast<uint32_t>(ubo_offsets[ubo_vs_i]),
                static_cast<uint32_t>(ubo_offsets[ubo_vs_b]), static_cast<uint32_t>(ubo_offsets[ubo_ps_f]),
                static_cast<uint32_t>(ubo_offsets[ubo_ps_i]), static_cast<uint32_t>(ubo_offsets[ubo_ps_b])};
            this->vulkan_.cmd_bind_descriptor_sets(batch_cmd, programmable->pipeline_layout, 0, descriptor_sets,
                                                   VK_PIPELINE_BIND_POINT_GRAPHICS, ubo_dynamic_offsets);
        }

        // Negative height (VK_KHR_maintenance1, core since Vulkan 1.1): D3D9 puts clip-space y = +1 at the
        // TOP of the screen, Vulkan puts y = -1 there. vkd3d-shader translates a D3D9 vertex shader's oPos
        // to gl_Position verbatim, in D3D9 clip space, so a plain {y = Y, height = +H} viewport rasterizes
        // every translated draw upside down. Flipping the viewport transform instead of rewriting each
        // shader also realigns D3D9 screen space with Vulkan framebuffer space, which is what the scissor
        // rect below is expressed in. The fixed-function shader's input is already in D3D9 screen space
        // rather than clip space and compensates for this itself (ff_triangle.vert).
        //
        // General case: D3D9's viewport transform is Px = X + (ndcX+1)*Width/2, Py = Y + (1-ndcY)*Height/2
        // (D3D9 docs); Vulkan's is framebufferX = x + (ndcX+1)*width/2, framebufferY = y + (ndcY+1)*height/2
        // (VkViewport spec). X/width need no correction -- matching coefficients gives x = X, width = Width
        // directly. For Y, matching the ndcY coefficient forces height = -Height, and matching the constant
        // term then forces y = Y + Height. This reduces to commit 7215d2d2's original hardcoded
        // {y = H, height = -H} exactly when X = Y = 0 and Width/Height equal the render target's extent --
        // the only case exercised before SetViewport's parsed state was wired up here.
        //
        // Winding order: a negative-height viewport reverses the effective face orientation the rasterizer
        // sees. Paid back once, permanently, in the pipeline's baked frontFace rather than here -- see
        // d3dcull_to_vk_cull_mode's comment for the reasoning.
        const bool has_explicit_viewport = this->state_.viewport_width > 0.0f && this->state_.viewport_height > 0.0f;
        const float vp_x = has_explicit_viewport ? this->state_.viewport_x : 0.0f;
        const float vp_y = has_explicit_viewport ? this->state_.viewport_y : 0.0f;
        const float vp_width = has_explicit_viewport ? this->state_.viewport_width : static_cast<float>(rt.width);
        const float vp_height = has_explicit_viewport ? this->state_.viewport_height : static_cast<float>(rt.height);
        const float vp_min_z = has_explicit_viewport ? this->state_.viewport_min_z : 0.0f;
        const float vp_max_z = has_explicit_viewport ? this->state_.viewport_max_z : 1.0f;
        const std::array<vulkan_host::viewport_entry, 1> viewports{
            {{.x = vp_x, .y = vp_y + vp_height, .width = vp_width, .height = -vp_height, .min_depth = vp_min_z, .max_depth = vp_max_z}}};
        this->vulkan_.cmd_set_viewport(batch_cmd, 0, false, viewports);
        vulkan_host::scissor_entry scissor{.offset_x = 0, .offset_y = 0, .width = rt.width, .height = rt.height};
        if (render_state_or(this->state_.render_state, d3drs_scissortestenable, 0) != 0)
        {
            const int32_t clamped_left = std::clamp(this->state_.scissor_left, 0, static_cast<int32_t>(rt.width));
            const int32_t clamped_top = std::clamp(this->state_.scissor_top, 0, static_cast<int32_t>(rt.height));
            const int32_t clamped_right = std::clamp(this->state_.scissor_right, clamped_left, static_cast<int32_t>(rt.width));
            const int32_t clamped_bottom = std::clamp(this->state_.scissor_bottom, clamped_top, static_cast<int32_t>(rt.height));
            scissor = {.offset_x = clamped_left,
                       .offset_y = clamped_top,
                       .width = static_cast<uint32_t>(clamped_right - clamped_left),
                       .height = static_cast<uint32_t>(clamped_bottom - clamped_top)};
        }
        const std::array<vulkan_host::scissor_entry, 1> scissors{scissor};
        this->vulkan_.cmd_set_scissor(batch_cmd, 0, false, scissors);

        if (!use_programmable)
        {
            const std::array<float, 2> viewport_size{static_cast<float>(rt.width), static_cast<float>(rt.height)};
            this->vulkan_.cmd_push_constants(batch_cmd, this->pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(viewport_size),
                                             viewport_size.data());
        }

        this->vulkan_.cmd_bind_vertex_buffers(batch_cmd, 0, static_cast<uint32_t>(stream_buffers.size()), stream_buffers.data(),
                                              stream_bind_offsets.data());
        // D3D9 hardware instancing carries no explicit instance-count draw parameter: the count is the low
        // 30 bits of the D3DSTREAMSOURCE_INDEXEDDATA stream's SetStreamSourceFreq divider (default 1, i.e.
        // an ordinary single-instance draw). Same helper the pipeline's per-binding inputRate was built
        // from, so the fetched instance data matches the instance count issued here.
        const uint32_t instance_count = this->resolve_instancing().instance_count;
        if (indexed != nullptr)
        {
            const uint32_t index_type = indexed->index_format != 0 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            this->vulkan_.cmd_bind_index_buffer(batch_cmd, index_buffer_vk, ib_direct ? index_buffer_bind_offset : ib_arena_offset,
                                                index_type);
            this->vulkan_.cmd_draw_indexed(batch_cmd, vertex_count, instance_count, indexed->first_index, indexed->base_vertex_index, 0);
        }
        else
        {
            // Real D3D9 hardware instancing requires an indexed draw (see the comment above), so
            // instance_count is 1 here in every valid usage; passed through anyway for uniformity rather
            // than special-casing the non-indexed path back to a literal 1.
            this->vulkan_.cmd_draw(batch_cmd, vertex_count, instance_count, first_vertex, 0);
        }

        if (pass_diag)
        {
            pass_diag_maybe_report();
        }

        if (profile)
        {
            const auto t_done = std::chrono::steady_clock::now();
            const auto ns = [](const auto duration) {
                return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
            };
            ++g_draw_profile.calls;
            g_draw_profile.setup_ns += ns(t_setup_done - t_entry);
            g_draw_profile.reserve_ns += ns(t_reserve_done - t_setup_done);
            g_draw_profile.resolve_ns += ns(t_resolve_done - t_setup_done);
            g_draw_profile.ubo_build_ns += ns(t_ubo_build_done - t_resolve_done);
            g_draw_profile.precount_ns += ns(t_precount_done - t_ubo_build_done);
            g_draw_profile.batch_ns += ns(t_batch_done - t_precount_done);
            g_draw_profile.suballoc_ns += ns(t_reserve_done - t_batch_done);
            // t_upload_done/t_texdesc_done default to t_reserve_done for a fixed-function draw (no
            // separate upload/texture-descriptor checkpoints on that path), so its whole reserve-to-done
            // span is attributed to record_ns instead. Acceptable imprecision for a first-pass profile:
            // MW2's real gameplay draws are overwhelmingly programmable-shader.
            g_draw_profile.record_ns += ns(t_done - t_texdesc_done);
            draw_profile_maybe_report();
            if (g_draw_profile.calls % 2000 == 0)
            {
                fprintf(stderr,
                        "[d3d9-drawprofile] texture_upload_skipped=%llu draw_count=%llu build_sampler_total=%.2fus "
                        "ensure_texture_uploaded_total=%.2fus ensure_texture_uploaded_max=%.2fus real_upload_count=%llu "
                        "descset_allocated=%llu descset_reused=%llu\n",
                        static_cast<unsigned long long>(this->stats_.texture_upload_skipped),
                        static_cast<unsigned long long>(this->draw_count_), static_cast<double>(g_build_sampler_ns) / 1000.0,
                        static_cast<double>(g_ensure_texture_uploaded_ns) / 1000.0,
                        static_cast<double>(g_ensure_texture_uploaded_max_ns) / 1000.0,
                        static_cast<unsigned long long>(g_texture_real_upload_count),
                        static_cast<unsigned long long>(this->stats_.descriptor_set_allocated),
                        static_cast<unsigned long long>(this->stats_.descriptor_set_reused));
            }
        }

        // Counted here, past every early return, so these are draws that genuinely reached the GPU.
        ++(use_programmable ? this->stats_.recorded_programmable : this->stats_.recorded_fixed);
        ++this->stats_.draws_per_render_target[target_rt];
        ++this->stats_.draws_per_shader_pair[{target_rt, this->state_.vertex_shader, this->state_.pixel_shader}];
        if (!srgb_write && render_state_or(this->state_.render_state, d3drs_alphablendenable, 0) != 0)
        {
            for (const auto& brt : bound_rts)
            {
                uint32_t probe_srgb_format = 0;
                if (brt.entry != nullptr && d3d9_format_to_vulkan_srgb(brt.entry->format, probe_srgb_format))
                {
                    ++this->stats_.blend_srgb_mismatch;
                    ++this->stats_.blend_srgb_mismatch_shader_pair[{target_rt, this->state_.vertex_shader, this->state_.pixel_shader}];
                    break;
                }
            }
        }
        if (build_depth_state(this->state_.render_state, depth_vk_format).test_enable != 0)
        {
            ++this->stats_.recorded_depth_tested;
        }

        // The batch is NOT submitted here -- it stays open, accumulating subsequent same-render-target
        // color draws, and is submitted at the next boundary: flush_batch (readback, clear, blt,
        // color_fill, resource teardown, render-target/depth-stencil change) or, for the descriptor-pool/
        // arena-overflow triggers in the batch-management step above, submit_batch_async, which does not
        // wait -- see batch_slot_count's header comment.
        // Nothing to free here: vertex/index/constant buffers are all sub-allocated slices of this batch
        // slot's own arena (vertex_index_uniform_arena_[batch_slot_]), reset to offset 0 when a batch opens
        // on that slot, and samplers are content-cached in sampler_cache_ (an immutable VkSampler reused by
        // any later draw with the same state), both retained for the device's lifetime.

        // Mark each bound render target's backing store stale; sync_backing_from_gpu reads it back
        // lazily on the next pfnLock/Present that actually needs the pixels (flushing the batch first).
        for (const auto& brt : bound_rts)
        {
            if (brt.entry == nullptr)
            {
                continue; // gap slot -- nothing was rendered here
            }
            brt.entry->backing_dirty = true;
        }

        return d3d_ok;
    }

    int32_t d3d9_host::create_resource(const uint32_t kind, const uint32_t format, const uint32_t width, const uint32_t height,
                                       const uint32_t depth, const uint32_t mip_levels, const uint32_t usage, const uint32_t pool,
                                       uint64_t& out_resource)
    {
        out_resource = 0;

        // Buffers (vertex/index) size their backing store directly from `width` (the byte count, per
        // d3d9_cmd::create_resource_request's convention). Render-target/depth-stencil 2D textures get
        // real GPU backing (see the class comment); sampled 2D/cube/volume textures get a real sampled
        // GPU image (below).
        const bool is_buffer = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) ||
                               kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer);
        const bool is_render_target = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) &&
                                      (usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0;
        const bool is_cube = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_cube);
        const bool is_volume = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_volume);
        // Every sampled texture kind (2D, cube, volume) that isn't a render target gets a real sampled
        // GPU image. This one predicate decides both "does this resource get a GPU image" and (via the
        // shared texture_subresource_layout below) how its per-subresource backing is sized, so the two
        // can never disagree. Unrecognized formats fall through with no GPU image, matching this
        // function's existing "unrecognized -> no backing" behavior rather than crashing.
        const bool is_texture =
            (kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) || is_cube || is_volume) && !is_render_target;
        uint32_t texture_vk_format = 0;
        const bool texture_format_ok = is_texture && d3d9_format_to_vulkan(format, texture_vk_format);
        const uint32_t texture_mip_levels = std::max(1u, mip_levels);

        // The one shared subresource layout: index 0 -> `backing`, indices 1..N-1 -> extra_mips. The same
        // helper drives the staging upload in ensure_texture_uploaded, so both agree on the index->(level,
        // face) mapping. Empty (no GPU image) for buffers/RTs/unrecognized formats.
        const std::vector<texture_subresource> subresources =
            texture_format_ok ? texture_subresource_layout(kind, texture_vk_format, width, height, depth, texture_mip_levels)
                              : std::vector<texture_subresource>{};
        const size_t texture_backing_size = subresources.empty() ? 0 : subresources.front().size;

        // A render target's host-side shadow (backing) must be sized at its real per-format stride, not a
        // hardcoded 4 bytes/texel BGRA8, so a non-BGRA8 RT (R5G6B5, A16B16G16R16F) reads back the correct
        // tight byte count via sync_backing_from_gpu. Depth-stencil RTs map to a depth VkFormat here too;
        // their backing is never locked, so vk_format_bytes_per_texel's upper-bound size is harmless.
        uint32_t render_target_vk_format = 0;
        const size_t render_target_backing_size =
            is_render_target && d3d9_format_to_vulkan(format, render_target_vk_format)
                ? static_cast<size_t>(width) * height * vk_format_bytes_per_texel(render_target_vk_format)
                : 0;

        // Subresources 1..N-1 each get their own byte vector in `extra_mips` (subresource 0 lives in
        // `backing`). Each cube face's mip chain and each volume level is a different byte size, so a flat
        // single vector can't hold them. Non-texture resources and single-subresource textures leave
        // extra_mips empty.
        std::vector<std::vector<std::byte>> extra_mips;
        for (size_t s = 1; s < subresources.size(); ++s)
        {
            extra_mips.emplace_back(subresources[s].size);
        }

        const size_t backing_size = is_buffer ? width : is_render_target ? render_target_backing_size : texture_backing_size;

        resource_entry entry{
            .kind = kind,
            .format = format,
            .width = width,
            .height = height,
            .depth = depth,
            .mip_levels = mip_levels,
            .usage = usage,
            .pool = pool,
            .backing = std::vector<std::byte>(backing_size),
            .extra_mips = std::move(extra_mips),
        };

        // Real DXVK source (D3D9CommonBuffer::DetermineMapMode, doitsujin/dxvk) gates a persistently
        // host-visible-mapped GPU buffer on exactly D3DPOOL_DEFAULT + D3DUSAGE_DYNAMIC. WriteOnly is
        // deliberately NOT part of the condition there -- it only steers which memory type gets picked --
        // and must not be part of it here either: a non-dynamic buffer has no renaming contract, so
        // handing the guest a direct pointer to it would let a plain Lock overwrite bytes a recorded draw
        // still reads. MW2's own vertex/index streaming buffers set Dynamic on ~22 of ~34 real buffer
        // creates (live-traced), which is what this path exists to accelerate.
        const bool eligible_for_direct_buffer =
            is_buffer && pool == d3dpool_default && (usage & d3dusage_dynamic) != 0 && backing_size != 0;
        if (eligible_for_direct_buffer)
        {
            // The ring is what makes a D3DLOCK_DISCARD free: the guest moves to the next slice rather
            // than waiting for the GPU to release the current one. Slices are page-aligned so each one's
            // guest-visible base lands on a page boundary too, and the ring is capped by total bytes
            // rather than a fixed count so a large buffer can't multiply into a huge 32-bit-guest VA
            // reservation. Two slices is the floor at which renaming still means anything at all.
            constexpr uint64_t slice_alignment = 0x1000;
            // 2026-08-27: measured on settled MW2 gameplay, a wrap is by far the dominant cost of this
            // whole mechanism -- 99.5% of the guest's GPU syncs are ring wraps, ~1.17ms of vCPU thread
            // time per frame. At a 4MB budget MW2's two most heavily renamed streaming buffers (2MB and
            // 4.5MB) got the 2-slice floor and so wrapped on every other Discard, together accounting for
            // 77% of all wraps; the buffers that got the full 8 slices wrapped at a twelfth the rate.
            // 16MB buys those two 8 and 3 slices respectively, and is deliberately still a byte budget
            // rather than a slice count: a 32-bit guest's address space is the real constraint, and MW2's
            // largest dynamic buffer (12MB, never renamed at all in the measured window) must not be
            // allowed to multiply into a ring the guest cannot reserve.
            constexpr uint64_t max_ring_bytes = 16u << 20;
            const uint64_t slice_stride = (backing_size + slice_alignment - 1) & ~(slice_alignment - 1);
            const uint64_t slice_count = std::clamp<uint64_t>(max_ring_bytes / slice_stride, 2, 8);
            const uint64_t ring_size = slice_stride * slice_count;

            const uint64_t device = this->ensure_vk_device();
            uint64_t vk_buffer = 0;
            if (device != 0 &&
                this->vulkan_.create_buffer(device, ring_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                            vk_buffer) == 0 &&
                vk_buffer != 0)
            {
                uint64_t mem_size = 0;
                uint64_t mem_align = 0;
                uint32_t mem_type_bits = 0;
                this->vulkan_.get_buffer_memory_requirements(device, vk_buffer, mem_size, mem_align, mem_type_bits);
                const uint32_t memory_type =
                    find_memory_type_index(this->vulkan_, this->vk_physical_device_, mem_type_bits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                uint64_t vk_memory = 0;
                void* mapped_ptr = nullptr;
                if (memory_type != UINT32_MAX && this->vulkan_.allocate_memory(device, mem_size, memory_type, vk_memory) == 0 &&
                    vk_memory != 0)
                {
                    this->vulkan_.bind_buffer_memory(device, vk_buffer, vk_memory, 0);
                    uint64_t mapped_size = 0;
                    if (this->vulkan_.map_memory(device, vk_memory, mapped_ptr, mapped_size) == 0 && mapped_ptr != nullptr)
                    {
                        entry.vk_direct_buffer_id = vk_buffer;
                        entry.vk_direct_memory_id = vk_memory;
                        entry.direct_mapped_ptr = mapped_ptr;
                        entry.direct_slice_stride = static_cast<uint32_t>(slice_stride);
                        entry.direct_slice_count = static_cast<uint32_t>(slice_count);
                    }
                    else
                    {
                        this->vulkan_.free_memory(device, vk_memory);
                        this->vulkan_.destroy_buffer(device, vk_buffer);
                    }
                }
                else
                {
                    this->vulkan_.destroy_buffer(device, vk_buffer);
                }
            }
            // A failed direct-buffer creation is not fatal -- entry.vk_direct_buffer_id stays 0 and every
            // consumer (prepare_unlock_target, lock, execute_draw) falls back to the ordinary
            // `backing`-based path exactly as if this resource had never been eligible.
        }

        if (is_render_target)
        {
            const uint64_t device = this->ensure_vk_device();
            if (device != 0)
            {
                // Every D3DUSAGE_DEPTHSTENCIL resource is structurally never sampled (is_samplable_
                // render_target excludes DEPTHSTENCIL usage outright -- shadow-map sampling isn't
                // implemented) or read back (readback_render_target's resting-layout guard already
                // rejects it; sync_backing_from_gpu never reaches a depth-stencil resource in practice),
                // so eligibility here is unconditional -- see depth_stencil_transient_enabled's own
                // comment for why the feature itself is currently disabled regardless (a separate,
                // real hazard unrelated to sampling/readback).
                const bool transient = depth_stencil_transient_enabled && (usage & d3dusage_depthstencil) != 0;
                uint64_t vk_image = 0;
                if (this->vulkan_.create_render_target(device, width, height, format, transient, vk_image) == 0 && vk_image != 0)
                {
                    entry.vk_image_id = vk_image;
                }
            }
        }
        else if (texture_backing_size != 0)
        {
            const uint64_t device = this->ensure_vk_device();
            if (device != 0)
            {
                uint64_t vk_image = 0;
                constexpr uint32_t sampled_usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                uint32_t image_type = VK_IMAGE_TYPE_2D;
                uint32_t image_depth = 1;
                uint32_t array_layers = 1;
                uint32_t image_flags = 0;
                if (is_cube)
                {
                    array_layers = cube_face_count;
                    image_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                }
                else if (is_volume)
                {
                    image_type = VK_IMAGE_TYPE_3D;
                    image_depth = std::max(1u, depth);
                }
                if (this->vulkan_.create_image(device, texture_vk_format, width, height, sampled_usage, VK_IMAGE_TILING_OPTIMAL,
                                               /*samples=*/1, image_type, image_depth, texture_mip_levels, array_layers, image_flags,
                                               vk_image) == 0 &&
                    vk_image != 0)
                {
                    uint64_t image_mem_size = 0;
                    uint64_t image_mem_align = 0;
                    uint32_t image_mem_type_bits = 0;
                    this->vulkan_.get_image_memory_requirements(device, vk_image, image_mem_size, image_mem_align, image_mem_type_bits);
                    const uint32_t memory_type = find_memory_type_index(this->vulkan_, this->vk_physical_device_, image_mem_type_bits,
                                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                    uint64_t image_memory = 0;
                    if (memory_type != UINT32_MAX &&
                        this->vulkan_.allocate_memory(device, image_mem_size, memory_type, image_memory) == 0 && image_memory != 0 &&
                        this->vulkan_.bind_image_memory(device, vk_image, image_memory, 0) == 0)
                    {
                        entry.vk_image_id = vk_image;
                        entry.vk_image_memory_id = image_memory;
                    }
                    else
                    {
                        this->vulkan_.destroy_image(device, vk_image);
                        if (image_memory != 0)
                        {
                            this->vulkan_.free_memory(device, image_memory);
                        }
                    }
                }
            }
        }

        const uint64_t id = this->allocate_id();
        this->resources_.emplace(id, std::move(entry));
        out_resource = id;

        if (getenv("EMULATOR_D3D9_TEXBLT_DIAG"))
        {
            fprintf(stderr,
                    "[d3d9-rescreate-diag] resource=%llu kind=%u format=%u %ux%ux%u mips=%u usage=%u pool=%u "
                    "backing_size=%zu\n",
                    static_cast<unsigned long long>(id), kind, format, width, height, depth, mip_levels, usage, pool, backing_size);
        }
        return d3d_ok;
    }

    bool d3d9_host::ensure_texture_uploaded(const uint64_t resource)
    {
        const scoped_ns_accumulator _prof(g_ensure_texture_uploaded_ns, drawprofile_enabled_flag(), &g_ensure_texture_uploaded_max_ns);

        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return false;
        }
        resource_entry& tex = it->second;
        const bool is_sampled_kind = tex.kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) ||
                                     tex.kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_cube) ||
                                     tex.kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_volume);
        if (tex.vk_image_id == 0 || !is_sampled_kind || (tex.usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0)
        {
            return false; // not a sampled texture with real GPU backing
        }

        // Dirty check. upload_dirty == false means a previous call already ran the full upload below to
        // completion AND nothing has written this texture's backing store since (see resource_entry's
        // comment for the complete writer list), so the GPU image is bit-identical to what re-uploading
        // would produce. Short-circuiting here is what makes a repeatedly-sampled texture cost nothing
        // after its first use, instead of a staging allocate + copy + submit + fence-wait + free per
        // draw. Placed after the guards above (not before) so a "clean" verdict can only ever be reached
        // by a resource that really is an uploadable sampled texture -- upload_dirty is cleared solely at
        // the successful end of this function, so a refused or failed upload always stays dirty and gets
        // retried, never silently reported as uploaded.
        if (!tex.upload_dirty)
        {
            ++this->stats_.texture_upload_skipped;
            return true;
        }
        ++g_texture_real_upload_count;
        if (uploadid_diag_enabled())
        {
            fprintf(stderr, "[d3d9-uploadid-diag] resource=%llu width=%u height=%u\n", static_cast<unsigned long long>(resource), tex.width,
                    tex.height);
        }

        uint32_t vk_format = 0;
        if (!d3d9_format_to_vulkan(tex.format, vk_format))
        {
            return false;
        }

        // Gather every subresource's tightly-packed size and source backing via the SAME shared layout
        // create_resource used to size the backing store, so the index->(level, face) mapping can never
        // disagree. Subresource 0 lives in `backing`, 1..N-1 in `extra_mips` (see create_resource) --
        // subresource_backing() abstracts that. Each is concatenated into one staging buffer at its own
        // offset. Every subresource's backing is pre-sized to its exact tight size at create_resource
        // time, so `src.size() < sub.size` below is really just a degenerate-format guard (size == 0),
        // not a "was this ever locked" check -- an app that creates N subresources but only ever writes
        // one still uploads the rest, as their zero-initialized (black) backing. That's an intentional,
        // safe default: no bail, no partially-garbage image, just unwritten faces/levels rendering black
        // until the app writes them.
        const uint32_t mip_levels = std::max(1u, tex.mip_levels);
        const bool is_cube = tex.kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_cube);
        const std::vector<texture_subresource> subresources =
            texture_subresource_layout(tex.kind, vk_format, tex.width, tex.height, tex.depth, mip_levels);

        struct subresource_upload
        {
            uint32_t width;
            uint32_t height;
            uint32_t depth;
            uint32_t mip_level;
            uint32_t base_array_layer;
            size_t size;
            uint64_t staging_offset;
            const std::byte* src;
        };

        std::vector<subresource_upload> uploads;
        uint64_t total_size = 0;
        for (uint32_t index = 0; index < subresources.size(); ++index)
        {
            const texture_subresource& sub = subresources[index];
            const std::vector<std::byte>& src = tex.subresource_backing(index);
            if (sub.size == 0 || src.size() < sub.size)
            {
                return false; // degenerate format/size only -- see the comment above this loop
            }
            uploads.push_back({sub.width, sub.height, sub.depth, sub.level, sub.base_array_layer, sub.size, total_size, src.data()});
            total_size += sub.size;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return false;
        }

        // Past the dirty check, so this really is a needed upload: always the FULL image, every
        // subresource. Uploading only the subresources unlock() actually touched would need per-mip
        // tracking for no real gain -- an app that writes one level of a texture almost always writes
        // the whole chain in one Lock/Unlock burst, and this whole path now runs once per change rather
        // than once per draw.
        uint64_t staging_buffer = 0;
        if (this->vulkan_.create_buffer(device, total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer) != 0 || staging_buffer == 0)
        {
            return false;
        }
        uint64_t mem_size = 0;
        uint64_t mem_align = 0;
        uint32_t mem_type_bits = 0;
        this->vulkan_.get_buffer_memory_requirements(device, staging_buffer, mem_size, mem_align, mem_type_bits);
        const uint32_t memory_type = find_memory_type_index(this->vulkan_, this->vk_physical_device_, mem_type_bits,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uint64_t staging_memory = 0;
        if (memory_type == UINT32_MAX || this->vulkan_.allocate_memory(device, mem_size, memory_type, staging_memory) != 0 ||
            staging_memory == 0)
        {
            this->vulkan_.destroy_buffer(device, staging_buffer);
            return false;
        }
        this->vulkan_.bind_buffer_memory(device, staging_buffer, staging_memory, 0);
        for (const auto& up : uploads)
        {
            this->vulkan_.upload_memory(device, staging_memory, up.staging_offset, up.size, up.src, up.size);
        }

        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);

        // One barrier over the whole image, then one buffer->image copy per subresource. A cube's six
        // faces are array layers 0..5 of a single image, so the barrier must span all six; a 2D texture
        // and a 3D volume (which has a single array layer) span one.
        const uint32_t barrier_layer_count = is_cube ? cube_face_count : 1u;
        const vulkan_host::subresource_range range{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                   .base_mip_level = 0,
                                                   .level_count = mip_levels,
                                                   .base_array_layer = 0,
                                                   .layer_count = barrier_layer_count};
        // VK_IMAGE_LAYOUT_UNDEFINED is a valid old_layout regardless of the image's actual current
        // layout (Vulkan's "discard previous contents" transition) -- correct here since every upload
        // fully overwrites every level anyway, whether this is the first upload or a later re-upload.
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, tex.vk_image_id, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

        for (const auto& up : uploads)
        {
            const vulkan_host::buffer_image_copy_region region{
                .buffer_offset = up.staging_offset,
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .image_offset_x = 0,
                .image_offset_y = 0,
                .image_offset_z = 0,
                .width = up.width,
                .height = up.height,
                .depth = up.depth,
                .mip_level = up.mip_level,
                .base_array_layer = up.base_array_layer,
                .layer_count = 1,
                .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            };
            this->vulkan_.cmd_copy_buffer_to_image(this->command_buffer_, staging_buffer, tex.vk_image_id,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region);
        }

        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, tex.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);
        this->vulkan_.wait_for_fence(this->fence_, UINT64_MAX);

        this->vulkan_.destroy_buffer(device, staging_buffer);
        this->vulkan_.free_memory(device, staging_memory);

        // Only here, after the fence proved every copy landed, is the GPU image known to match `backing`.
        tex.upload_dirty = false;
        ++this->stats_.texture_upload_done;

        if (texupload_diag_enabled())
        {
            uint64_t hash = 14695981039346656037ull; // FNV-1a, offset basis
            for (const auto byte : tex.backing)
            {
                hash ^= static_cast<uint8_t>(byte);
                hash *= 1099511628211ull; // FNV-1a prime
            }
            fprintf(stderr, "[d3d9-texupload-diag] resource=%llu %ux%u format=%u usage=0x%x backing_hash=0x%llx\n",
                    static_cast<unsigned long long>(resource), tex.width, tex.height, tex.format, tex.usage,
                    static_cast<unsigned long long>(hash));
        }

        return true;
    }

    void d3d9_host::destroy_resource(const uint64_t resource)
    {
        // Flush any open batch before erasing the resource: a deferred batch may hold recorded commands
        // referencing this resource's VkImage/VkImageView (as a render target or a sampled texture), and
        // the erase destroys those Vulkan objects. Flushing waits for the GPU, so nothing in flight or
        // recorded-but-unsubmitted still references them. The same wait covers a direct-mapped buffer's
        // own recorded draws below (see create_resource's eligibility check) -- flush_batch() already
        // guarantees the GPU is idle with respect to anything referencing it before this point.
        this->flush_batch();
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return;
        }

        resource_entry& entry = it->second;
        // A resource with no Vulkan object of its own (a plain, non-direct-mapped buffer) must not drag a
        // device into existence just to be freed -- ensure_vk_device() creates one on first call.
        if (entry.vk_direct_buffer_id != 0 || entry.vk_image_id != 0 || entry.vk_image_view_id != 0 || entry.vk_image_view_srgb_id != 0)
        {
            const uint64_t device = this->ensure_vk_device();
            if (entry.vk_direct_buffer_id != 0)
            {
                this->vulkan_.destroy_buffer(device, entry.vk_direct_buffer_id);
                this->vulkan_.free_memory(device, entry.vk_direct_memory_id);
            }
            if (entry.vk_image_view_id != 0)
            {
                this->vulkan_.destroy_image_view(device, entry.vk_image_view_id);
            }
            if (entry.vk_image_view_srgb_id != 0)
            {
                this->vulkan_.destroy_image_view(device, entry.vk_image_view_srgb_id);
            }
            if (entry.vk_image_id != 0)
            {
                // A render target's image, its memory, and the readback buffer/command pool/fence attached
                // to it are all owned by vulkan_host's render_target_data, so it needs the paired teardown;
                // a sampled texture's image and memory are this entry's own (see vk_image_memory_id).
                if (entry.vk_image_memory_id != 0)
                {
                    this->vulkan_.destroy_image(device, entry.vk_image_id);
                    this->vulkan_.free_memory(device, entry.vk_image_memory_id);
                }
                else
                {
                    this->vulkan_.destroy_render_target(device, entry.vk_image_id);
                }
            }
        }

        this->resources_.erase(it);
    }

    int32_t d3d9_host::tex_blt(const uint64_t dst_resource, const uint64_t src_resource)
    {
        const auto dst_it = this->resources_.find(dst_resource);
        const auto src_it = this->resources_.find(src_resource);
        if (dst_it == this->resources_.end() || src_it == this->resources_.end())
        {
            if (texblt_diag_enabled())
            {
                fprintf(stderr, "[d3d9-texblt-diag] dst=%llu(found=%d) src=%llu(found=%d) -> invalidcall\n",
                        static_cast<unsigned long long>(dst_resource), dst_it != this->resources_.end(),
                        static_cast<unsigned long long>(src_resource), src_it != this->resources_.end());
            }
            return d3derr_invalidcall;
        }

        // Flush any open batch first: ensure_texture_uploaded re-uploads dst's backing into one
        // persistent GPU image with no per-draw snapshot, so an already-recorded-but-unsubmitted
        // batched draw that samples dst would otherwise see THIS write's contents once the batch
        // finally executes, not what it sampled at record time. Also required for the GPU fast path
        // below: it needs src's render-target content (if any) already resting in its
        // TRANSFER_SRC_OPTIMAL layout, which only holds once every batched draw into it has executed.
        this->flush_batch();
        resource_entry& dst = dst_it->second;
        resource_entry& src = src_it->second;
        if (texblt_diag_enabled())
        {
            const auto hash_of = [](const std::vector<std::byte>& bytes) {
                uint64_t hash = 14695981039346656037ull;
                for (const auto byte : bytes)
                {
                    hash ^= static_cast<uint8_t>(byte);
                    hash *= 1099511628211ull;
                }
                return hash;
            };
            fprintf(stderr,
                    "[d3d9-texblt-diag] dst=%llu %ux%u fmt=%u bytes=%zu src=%llu %ux%u fmt=%u bytes=%zu "
                    "src_hash_before=0x%llx\n",
                    static_cast<unsigned long long>(dst_resource), dst.width, dst.height, dst.format, dst.backing.size(),
                    static_cast<unsigned long long>(src_resource), src.width, src.height, src.format, src.backing.size(),
                    static_cast<unsigned long long>(hash_of(src.backing)));
        }

        // Fast path: sync directly on the GPU with a single cheap image copy instead of round-tripping
        // the whole resource through CPU memory (a full backing-vector copy, then a full re-upload on
        // the next sample) on every single call regardless of whether the content actually changed.
        // Live MW2 profiling (EMULATOR_D3D9_DRAWPROFILE) found this exact CPU-mediated path costing
        // 500us-2.8ms per call, hundreds of times in a single play session, dwarfing every other
        // per-draw cost combined -- this is what real hardware (and DXVK) do as a near-free GPU-only
        // operation instead. Two src cases, both left resting at their own steady-state layout by
        // whatever produced them: a render target's GPU image is authoritative and (per flush_batch
        // above) guaranteed resting in TRANSFER_SRC_OPTIMAL, the same invariant d3d9_host::blt already
        // relies on for its own src parameter; a plain sampled texture (the D3DPOOL_MANAGED master/
        // vidmem sync this function was originally written for -- live-confirmed as the case MW2
        // itself actually exercises) rests in SHADER_READ_ONLY_OPTIMAL once ensure_texture_uploaded
        // has it current, needing an extra pair of barriers around the copy to and from that layout.
        const uint64_t device = this->ensure_vk_device();
        const bool src_is_render_target = (src.usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0;
        const bool src_ready_as_texture = !src_is_render_target && this->ensure_texture_uploaded(src_resource);
        if (device != 0 && (src_is_render_target || src_ready_as_texture) && dst.vk_image_id != 0 && src.vk_image_id != 0 &&
            dst.width == src.width && dst.height == src.height && dst.format == src.format && this->ensure_draw_infra())
        {
            const uint32_t src_resting_layout =
                src_is_render_target ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Record into the currently-open batch (the flush_batch() above just drained and closed
            // whatever was open, so this reopens fresh on the same slot) instead of a standalone
            // submit+wait_for_fence(UINT64_MAX) -- same fix as the Clear() fast path: this copy is left
            // recorded but unsubmitted, to be picked up by whatever draw or flush_batch() (sync_backing_
            // from_gpu, the next SetRenderTarget, ...) comes next, all of which already flush before
            // relying on dst's GPU content.
            this->ensure_batch_open(device, this->state_.render_targets[0], this->state_.depth_stencil);
            this->close_render_pass(this->batch_slot_); // vkCmdCopyImage is illegal inside a render pass instance
            const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];

            const vulkan_host::subresource_range color_range{
                .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
            if (!src_is_render_target)
            {
                this->vulkan_.cmd_pipeline_barrier(batch_cmd, src.vk_image_id, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                   src_resting_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);
            }
            this->vulkan_.cmd_pipeline_barrier(batch_cmd, dst.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color_range);

            const vulkan_host::image_copy_region region{
                .src_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                .src_mip_level = 0,
                .src_base_array_layer = 0,
                .src_layer_count = 1,
                .src_offset_x = 0,
                .src_offset_y = 0,
                .src_offset_z = 0,
                .dst_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                .dst_mip_level = 0,
                .dst_base_array_layer = 0,
                .dst_layer_count = 1,
                .dst_offset_x = 0,
                .dst_offset_y = 0,
                .dst_offset_z = 0,
                .width = dst.width,
                .height = dst.height,
                .depth = 1,
            };
            this->vulkan_.cmd_copy_image(batch_cmd, src.vk_image_id, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.vk_image_id,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region);

            if (!src_is_render_target)
            {
                // Restore src to the layout every other sampler of it (a draw's descriptor set) expects.
                this->vulkan_.cmd_pipeline_barrier(batch_cmd, src.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                   VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_resting_layout,
                                                   color_range);
            }
            this->vulkan_.cmd_pipeline_barrier(batch_cmd, dst.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);

            dst.backing_dirty = true; // CPU-side backing now stale relative to the GPU image
            dst.upload_dirty = false; // ...but the GPU image itself already matches; no re-upload needed
            return d3d_ok;
        }

        dst.backing = src.backing;
        dst.upload_dirty = true; // dst's GPU image no longer matches its (just replaced) backing
        return d3d_ok;
    }

    bool d3d9_host::snapshot_resource(const uint64_t resource, std::vector<std::byte>& out_pixels, uint32_t& out_width,
                                      uint32_t& out_height)
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end() || it->second.vk_image_id == 0 || it->second.backing.empty())
        {
            return false;
        }

        this->sync_backing_from_gpu(it->second);

        const bool timing = present_timing_enabled();
        const auto t0 = timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};

        out_pixels = it->second.backing;
        out_width = it->second.width;
        out_height = it->second.height;

        if (timing)
        {
            const auto now = std::chrono::steady_clock::now();
            present_timing_totals& totals = present_timing();
            totals.copy_ms += elapsed_ms(t0, now);
            if (totals.last_present.time_since_epoch().count() != 0)
            {
                totals.interval_ms += elapsed_ms(totals.last_present, now);
            }
            totals.last_present = now;

            const int parsed = atoi(getenv("EMULATOR_D3D9_PRESENT_TIMING"));
            const auto period = static_cast<uint64_t>(parsed > 0 ? parsed : 60);
            if (++totals.frames >= period)
            {
                const auto n = static_cast<double>(totals.frames);
                fprintf(stderr,
                        "[d3d9-present-timing] %ux%u over %llu presents: interval=%.2fms (%.1f FPS) | flush=%.2fms "
                        "readback=%.2fms copy=%.2fms | blocking=%.1f%% of frame\n",
                        out_width, out_height, static_cast<unsigned long long>(totals.frames), totals.interval_ms / n,
                        1000.0 * n / std::max(totals.interval_ms, 1e-9), totals.flush_ms / n, totals.readback_ms / n, totals.copy_ms / n,
                        100.0 * (totals.flush_ms + totals.readback_ms + totals.copy_ms) / std::max(totals.interval_ms, 1e-9));
                const auto last = totals.last_present;
                totals = {};
                totals.last_present = last;
            }
        }
        return true;
    }

    bool d3d9_host::is_render_target(const uint64_t resource) const
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return false;
        }
        // Same predicate create_resource uses to decide a resource gets render-target GPU backing.
        return it->second.kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) &&
               (it->second.usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0;
    }

    std::vector<std::string> d3d9_host::describe_render_targets()
    {
        // Decodes an IEEE-754 binary16 bit pattern (the inverse of float_to_half above), so a
        // A16B16G16R16F target's texels can be summarized on the same scale as every other format.
        const auto half_to_float = [](const uint16_t h) {
            const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
            const uint32_t exponent = (h >> 10) & 0x1Fu;
            const uint32_t mantissa = h & 0x3FFu;
            uint32_t bits = 0;
            if (exponent == 0)
            {
                bits = sign; // zero/subnormal -- summarized as zero, which is all this diagnostic needs
            }
            else if (exponent == 0x1F)
            {
                bits = sign | 0x7F800000u | (mantissa << 13);
            }
            else
            {
                bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
            }
            float value = 0.0f;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        };

        std::vector<std::string> lines;
        for (auto& [id, entry] : this->resources_)
        {
            if (entry.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) ||
                (entry.usage & (d3dusage_rendertarget | d3dusage_depthstencil)) == 0 || entry.vk_image_id == 0)
            {
                continue;
            }

            uint32_t vk_format = 0;
            d3d9_format_to_vulkan(entry.format, vk_format);
            const auto draws_it = this->stats_.draws_per_render_target.find(id);
            std::array<char, 192> header{};
            std::snprintf(header.data(), header.size(), "rt=%llu d3dfmt=%u vkfmt=%u %ux%u usage=0x%X draws=%llu",
                          static_cast<unsigned long long>(id), static_cast<unsigned>(entry.format), static_cast<unsigned>(vk_format),
                          static_cast<unsigned>(entry.width), static_cast<unsigned>(entry.height), static_cast<unsigned>(entry.usage),
                          static_cast<unsigned long long>(draws_it == this->stats_.draws_per_render_target.end() ? 0 : draws_it->second));
            std::string line = header.data();

            // A depth-stencil image's readback path (colour aspect, colour layouts) does not apply, and
            // nothing ever populates its `backing`; report the declaration alone rather than a fake zero.
            if ((entry.usage & d3dusage_depthstencil) != 0)
            {
                lines.push_back(line + " contents=<depth-stencil, not read back>");
                continue;
            }

            this->sync_backing_from_gpu(entry);
            const uint32_t bytes_per_texel = vk_format_bytes_per_texel(vk_format);
            const size_t texels = bytes_per_texel != 0 ? entry.backing.size() / bytes_per_texel : 0;
            if (texels == 0)
            {
                lines.push_back(line + " contents=<no backing>");
                continue;
            }

            // One representative scalar per texel, on a comparable scale across formats: the stored value
            // for a single-channel float target (a shadow map / depth prepass, where the value IS the
            // datum), and the brightest colour channel otherwise.
            const auto texel_value = [&](const size_t index) -> float {
                const std::byte* texel = entry.backing.data() + index * bytes_per_texel;
                switch (vk_format)
                {
                case VK_FORMAT_R32_SFLOAT: {
                    float value = 0.0f;
                    std::memcpy(&value, texel, sizeof(value));
                    return value;
                }
                case VK_FORMAT_R16G16B16A16_SFLOAT: {
                    uint16_t bits = 0;
                    std::memcpy(&bits, texel, sizeof(bits));
                    return half_to_float(bits);
                }
                case VK_FORMAT_R5G6B5_UNORM_PACK16: {
                    uint16_t bits = 0;
                    std::memcpy(&bits, texel, sizeof(bits));
                    return static_cast<float>(std::max({(bits >> 11) & 0x1F, (bits >> 5) & 0x3F, bits & 0x1F})) / 63.0f;
                }
                default: {
                    uint32_t brightest = 0;
                    for (uint32_t byte = 0; byte < std::min(bytes_per_texel, 3u); ++byte)
                    {
                        brightest = std::max(brightest, static_cast<uint32_t>(texel[byte]));
                    }
                    return static_cast<float>(brightest) / 255.0f;
                }
                }
            };

            float min_value = std::numeric_limits<float>::infinity();
            float max_value = -std::numeric_limits<float>::infinity();
            double sum = 0.0;
            size_t nan_count = 0;
            size_t zero_count = 0;
            // Fraction of texels equal to the most common value, computed over a coarse bucketing: the
            // single number that separates "this target holds a real, varying image" from "this target
            // holds one constant everywhere", which is the whole question a degenerate pass raises.
            std::array<size_t, 16> histogram{};
            for (size_t i = 0; i < texels; ++i)
            {
                const float value = texel_value(i);
                if (std::isnan(value))
                {
                    ++nan_count;
                    continue;
                }
                min_value = std::min(min_value, value);
                max_value = std::max(max_value, value);
                sum += value;
                zero_count += value == 0.0f ? 1 : 0;
                const float clamped = std::clamp(value, 0.0f, 1.0f);
                histogram[static_cast<size_t>(std::min(15.0f, clamped * 16.0f))] += 1;
            }
            const size_t finite = texels - nan_count;
            std::array<char, 320> summary{};
            std::snprintf(summary.data(), summary.size(), " texels=%zu min=%g max=%g mean=%g nan=%zu zero=%zu", texels,
                          finite != 0 ? min_value : 0.0f, finite != 0 ? max_value : 0.0f,
                          finite != 0 ? sum / static_cast<double>(finite) : 0.0, nan_count, zero_count);
            line += summary.data();
            line += " hist=";
            for (const size_t bucket : histogram)
            {
                line += std::to_string(finite != 0 ? bucket * 100 / finite : 0) + ",";
            }
            lines.push_back(line);

            // EMULATOR_D3D9_RTDUMPDIR=<dir> additionally writes the raw texels to <dir>/rt_<id>.bin.
            // Summary statistics say whether a target's contents are plausible; only the image itself
            // says whether the *structure* in it is the scene it should be, which is the difference
            // between "these numbers look odd" and a diagnosis.
            if (const char* dump_dir = getenv("EMULATOR_D3D9_RTDUMPDIR"))
            {
                std::array<char, 512> path{};
                std::snprintf(path.data(), path.size(), "%s/rt_%llu.bin", dump_dir, static_cast<unsigned long long>(id));
                if (FILE* file = std::fopen(path.data(), "wb"))
                {
                    std::fwrite(entry.backing.data(), 1, entry.backing.size(), file);
                    std::fclose(file);
                }
            }
        }
        return lines;
    }

    std::string d3d9_host::describe_pipeline_state(const std::map<uint32_t, std::map<uint32_t, uint64_t>>& render_state_values) const
    {
        std::string out = "rs:";
        for (const auto& [state, values] : render_state_values)
        {
            out += " " + std::to_string(state) + "={";
            for (const auto& [value, count] : values)
            {
                out += std::to_string(value) + "x" + std::to_string(count) + ",";
            }
            out += "}";
        }
        out += " | samp:";
        std::map<uint64_t, uint32_t> ordered_ss(this->state_.sampler_state.begin(), this->state_.sampler_state.end());
        for (const auto& [key, value] : ordered_ss)
        {
            out += " s" + std::to_string(key >> 32) + "." + std::to_string(key & 0xFFFFFFFF) + "=" + std::to_string(value);
        }
        return out;
    }

    std::string d3d9_host::describe_ps_fog_constants() const
    {
        const auto& cf = this->state_.ps_const_f;
        auto reg = [&cf](const uint32_t r) -> std::array<float, 4> {
            const size_t base = static_cast<size_t>(r) * 4;
            if (cf.size() < base + 4)
            {
                return {0.0f, 0.0f, 0.0f, 0.0f};
            }
            return {cf[base], cf[base + 1], cf[base + 2], cf[base + 3]};
        };
        const auto c0 = reg(0);
        const auto c32 = reg(32);
        const auto c34 = reg(34);
        std::array<char, 256> buf{};
        std::snprintf(buf.data(), buf.size(), "c0=(%.4f,%.4f,%.4f,%.4f) c32=(%.4f,%.4f,%.4f,%.4f) c34=(%.4f,%.4f,%.4f,%.4f)", c0[0], c0[1],
                      c0[2], c0[3], c32[0], c32[1], c32[2], c32[3], c34[0], c34[1], c34[2], c34[3]);
        return {buf.data()};
    }

    void d3d9_host::sync_backing_from_gpu(resource_entry& rt)
    {
        if (!rt.backing_dirty || rt.vk_image_id == 0)
        {
            return;
        }

        const bool timing = present_timing_enabled();
        const auto t0 = timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};

        // Flush any open batch so its draws have executed (and left the render target in
        // TRANSFER_SRC_OPTIMAL) before the readback reads the image. Covers both pfnLock and Present.
        this->flush_batch();

        const auto t1 = timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};

        std::vector<std::byte> pixels;
        uint32_t readback_width = 0;
        uint32_t readback_height = 0;
        if (this->vulkan_.readback_render_target(rt.vk_image_id, pixels, readback_width, readback_height) == 0)
        {
            rt.backing = std::move(pixels);
            rt.backing_dirty = false;
        }

        if (timing)
        {
            const auto t2 = std::chrono::steady_clock::now();
            present_timing().flush_ms += elapsed_ms(t0, t1);
            present_timing().readback_ms += elapsed_ms(t1, t2);
        }
    }

    int32_t d3d9_host::color_fill(const uint64_t resource, const uint32_t /*subresource*/, const int32_t left, const int32_t top,
                                  const int32_t right, const int32_t bottom, const uint32_t color_argb)
    {
        // Flush any open batch first: ColorFill overwrites the resource on the GPU, and any batched draw
        // targeting it must have executed before that overwrite (and the resource must rest in
        // TRANSFER_SRC_OPTIMAL, which the batch's closing barrier guarantees).
        this->flush_batch();
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end() || it->second.vk_image_id == 0)
        {
            return d3derr_invalidcall;
        }
        resource_entry& rt = it->second;

        // Clamp to the image extent (D3D9 RECT right/bottom are exclusive). An empty rect is a no-op,
        // matching real ColorFill on a degenerate rect.
        const int32_t x0 = std::clamp(left, 0, static_cast<int32_t>(rt.width));
        const int32_t y0 = std::clamp(top, 0, static_cast<int32_t>(rt.height));
        const int32_t x1 = std::clamp(right, x0, static_cast<int32_t>(rt.width));
        const int32_t y1 = std::clamp(bottom, y0, static_cast<int32_t>(rt.height));
        const uint32_t fill_w = static_cast<uint32_t>(x1 - x0);
        const uint32_t fill_h = static_cast<uint32_t>(y1 - y0);
        if (fill_w == 0 || fill_h == 0)
        {
            return d3d_ok;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return d3derr_invalidcall;
        }

        // Encode the fill color into one texel of the render target's real VkFormat, then replicate it
        // across the fill region, so a non-BGRA8 render target is filled with the correct byte pattern
        // rather than a raw D3DCOLOR dword. An unsupported format fails cleanly instead of corrupting the
        // staging copy.
        uint32_t rt_vk_format = 0;
        std::array<std::byte, 8> texel{};
        if (!d3d9_format_to_vulkan(rt.format, rt_vk_format))
        {
            return d3derr_invalidcall;
        }
        const uint32_t bytes_per_texel = vk_format_bytes_per_texel(rt_vk_format);
        if (bytes_per_texel == 0 || !encode_fill_texel(rt_vk_format, color_argb, texel))
        {
            return d3derr_invalidcall;
        }
        const size_t pixel_count = static_cast<size_t>(fill_w) * fill_h;
        const size_t required = pixel_count * bytes_per_texel;
        std::vector<std::byte> fill_bytes(required);
        for (size_t i = 0; i < pixel_count; ++i)
        {
            std::memcpy(fill_bytes.data() + i * bytes_per_texel, texel.data(), bytes_per_texel);
        }

        uint64_t staging_buffer = 0;
        if (this->vulkan_.create_buffer(device, required, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer) != 0 || staging_buffer == 0)
        {
            return d3derr_invalidcall;
        }
        uint64_t mem_size = 0;
        uint64_t mem_align = 0;
        uint32_t mem_type_bits = 0;
        this->vulkan_.get_buffer_memory_requirements(device, staging_buffer, mem_size, mem_align, mem_type_bits);
        const uint32_t memory_type = find_memory_type_index(this->vulkan_, this->vk_physical_device_, mem_type_bits,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uint64_t staging_memory = 0;
        if (memory_type == UINT32_MAX || this->vulkan_.allocate_memory(device, mem_size, memory_type, staging_memory) != 0 ||
            staging_memory == 0)
        {
            this->vulkan_.destroy_buffer(device, staging_buffer);
            return d3derr_invalidcall;
        }
        this->vulkan_.bind_buffer_memory(device, staging_buffer, staging_memory, 0);
        this->vulkan_.upload_memory(device, staging_memory, 0, required, fill_bytes.data(), required);

        // Record into the currently-open batch (the flush_batch() above just drained and closed whatever
        // was open, so this reopens fresh on the same slot) instead of a standalone
        // submit+wait_for_fence(UINT64_MAX) -- same fix as the Clear() fast path. D3D9's ColorFill can
        // target a sub-rect, which vkCmdClearColorImage has no way to express (it always clears whole
        // subresources), so this keeps the buffer-copy approach rather than switching to a native clear.
        this->ensure_batch_open(device, this->state_.render_targets[0], this->state_.depth_stencil);
        this->close_render_pass(this->batch_slot_); // vkCmdCopyBufferToImage is illegal inside a render pass instance
        const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];

        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // Resting layout is TRANSFER_SRC_OPTIMAL (submit_clear / execute_draw leave it there); go through
        // cmd_pipeline_barrier so render_targets[image].current_layout stays authoritative.
        this->vulkan_.cmd_pipeline_barrier(batch_cmd, rt.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color_range);

        const vulkan_host::buffer_image_copy_region region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .image_offset_x = x0,
            .image_offset_y = y0,
            .image_offset_z = 0,
            .width = fill_w,
            .height = fill_h,
            .depth = 1,
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        };
        this->vulkan_.cmd_copy_buffer_to_image(batch_cmd, staging_buffer, rt.vk_image_id, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region);

        this->vulkan_.cmd_pipeline_barrier(batch_cmd, rt.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);

        // Freed once this batch slot's submission actually completes (wait_for_batch_slot) -- the staging
        // buffer must stay alive on the GPU until then, not right after this call returns.
        this->pending_staging_cleanup_[this->batch_slot_].push_back({.device = device, .buffer = staging_buffer, .memory = staging_memory});

        rt.backing_dirty = true;
        return d3d_ok;
    }

    int32_t d3d9_host::blt(const uint64_t dst_resource, const uint32_t /*dst_subresource*/, const int32_t dst_left, const int32_t dst_top,
                           const int32_t dst_right, const int32_t dst_bottom, const uint64_t src_resource,
                           const uint32_t /*src_subresource*/, const int32_t src_left, const int32_t src_top, const int32_t src_right,
                           const int32_t src_bottom, const uint32_t filter)
    {
        const auto dst_it = this->resources_.find(dst_resource);
        const auto src_it = this->resources_.find(src_resource);
        if (dst_it == this->resources_.end() || src_it == this->resources_.end() || dst_it->second.vk_image_id == 0 ||
            src_it->second.vk_image_id == 0)
        {
            return d3derr_invalidcall;
        }
        resource_entry& dst = dst_it->second;
        resource_entry& src = src_it->second;

        const int32_t sx0 = std::clamp(src_left, 0, static_cast<int32_t>(src.width));
        const int32_t sy0 = std::clamp(src_top, 0, static_cast<int32_t>(src.height));
        const int32_t sx1 = std::clamp(src_right, sx0, static_cast<int32_t>(src.width));
        const int32_t sy1 = std::clamp(src_bottom, sy0, static_cast<int32_t>(src.height));
        const int32_t dx0 = std::clamp(dst_left, 0, static_cast<int32_t>(dst.width));
        const int32_t dy0 = std::clamp(dst_top, 0, static_cast<int32_t>(dst.height));
        const int32_t dx1 = std::clamp(dst_right, dx0, static_cast<int32_t>(dst.width));
        const int32_t dy1 = std::clamp(dst_bottom, dy0, static_cast<int32_t>(dst.height));
        if (sx1 - sx0 == 0 || sy1 - sy0 == 0 || dx1 - dx0 == 0 || dy1 - dy0 == 0)
        {
            return d3d_ok;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return d3derr_invalidcall;
        }

        // Record into the currently-open batch instead of flush_batch() + a standalone
        // submit+wait_for_fence(UINT64_MAX), the same way tex_blt's and color_fill's GPU paths do. What
        // the old drain bought -- every batched draw into src/dst already executed, and both images back
        // at their TRANSFER_SRC_OPTIMAL resting layout -- is delivered without any GPU wait by recording
        // after close_render_pass(): program order inside one command buffer orders the blit after those
        // draws, and close_render_pass emits exactly the attachment/sampled-image barriers that restore
        // the resting layout. Every consumer that must OBSERVE the blit (Lock/Present via
        // sync_backing_from_gpu, resource teardown) already flushes for itself.
        this->ensure_batch_open(device, this->state_.render_targets[0], this->state_.depth_stencil);
        this->close_render_pass(this->batch_slot_); // vkCmdBlitImage is illegal inside a render pass instance
        // A Clear() whose colour is still parked on pending_clear_ was issued BEFORE this blit, but would
        // otherwise be realized after it -- folded into a later draw's LOAD_OP_CLEAR, wiping a dst the
        // Clear() named, or leaving the blit reading a not-yet-cleared src. The drain used to force it out
        // (via submit_batch_async); do the same explicitly, while the render pass is still closed.
        // pending_clear_'s depth half needs no such treatment: this blit only ever touches the colour
        // aspect, so it cannot alias the depth-stencil the deferred depth clear names.
        this->realize_pending_color_clear(this->pending_clear_[this->batch_slot_]);
        const uint64_t batch_cmd = this->batch_command_buffer_[this->batch_slot_];

        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // Both RTs rest in TRANSFER_SRC_OPTIMAL. The source is already blit-ready there; only the
        // destination needs a TRANSFER_DST_OPTIMAL round trip. Route through cmd_pipeline_barrier so
        // render_targets[dst].current_layout stays authoritative.
        this->vulkan_.cmd_pipeline_barrier(batch_cmd, dst.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color_range);

        // D3DTEXF_LINEAR (2) -> VK_FILTER_LINEAR; every other value (incl. NONE/POINT) -> NEAREST, which
        // is an exact copy for same-size blits.
        const uint32_t vk_filter = filter == 2 /*D3DTEXF_LINEAR*/ ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        const vulkan_host::image_blit_region blit{
            .src_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            .src_mip_level = 0,
            .src_base_array_layer = 0,
            .src_layer_count = 1,
            .src_offset_x0 = sx0,
            .src_offset_y0 = sy0,
            .src_offset_z0 = 0,
            .src_offset_x1 = sx1,
            .src_offset_y1 = sy1,
            .src_offset_z1 = 1,
            .dst_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            .dst_mip_level = 0,
            .dst_base_array_layer = 0,
            .dst_layer_count = 1,
            .dst_offset_x0 = dx0,
            .dst_offset_y0 = dy0,
            .dst_offset_z0 = 0,
            .dst_offset_x1 = dx1,
            .dst_offset_y1 = dy1,
            .dst_offset_z1 = 1,
            .filter = vk_filter,
        };
        this->vulkan_.cmd_blit_image(batch_cmd, src.vk_image_id, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.vk_image_id,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blit);

        this->vulkan_.cmd_pipeline_barrier(batch_cmd, dst.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);

        dst.backing_dirty = true;
        return d3d_ok;
    }

    int32_t d3d9_host::lock(const uint64_t resource, const uint32_t subresource, const uint32_t offset, const uint32_t size,
                            const uint32_t /*flags*/, void* out, const size_t out_capacity, uint32_t& out_data_size)
    {
        out_data_size = 0;

        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return d3derr_invalidcall;
        }
        // subresource != 0 addresses a mip level in extra_mips (see resource_entry); reject an index the
        // resource doesn't have. Buffers/render targets only ever use subresource 0.
        if (subresource != 0 && (subresource - 1) >= it->second.extra_mips.size())
        {
            return d3derr_invalidcall;
        }

        this->sync_backing_from_gpu(it->second);

        // A resource with a direct buffer (see create_resource's eligibility check) never has current
        // data in `backing` -- reads must come from the real, always-current mapped GPU buffer instead.
        // Only ever set for subresource 0 (buffers have no mips), matching the check below.
        if (it->second.vk_direct_buffer_id != 0 && subresource == 0)
        {
            const auto* mapped = static_cast<const std::byte*>(it->second.direct_mapped_ptr) + it->second.direct_slice_offset;
            const size_t buffer_size = it->second.width;
            if (offset > buffer_size)
            {
                return d3derr_invalidcall;
            }
            const size_t available = buffer_size - offset;
            const size_t requested = size != 0 ? size : available;
            const size_t to_copy = std::min({requested, available, out_capacity});
            if (to_copy > 0 && out != nullptr && mapped != nullptr)
            {
                std::memcpy(out, mapped + offset, to_copy);
            }
            out_data_size = static_cast<uint32_t>(available);
            return d3d_ok;
        }

        auto& backing = it->second.subresource_backing(subresource);
        const size_t requested = size != 0 ? size : backing.size();
        if (offset > backing.size())
        {
            return d3derr_invalidcall;
        }

        const size_t available = backing.size() - offset;
        const size_t to_copy = std::min({requested, available, out_capacity});

        if (to_copy > 0 && out != nullptr)
        {
            std::memcpy(out, backing.data() + offset, to_copy);
        }
        out_data_size = static_cast<uint32_t>(available);
        return d3d_ok;
    }

    int32_t d3d9_host::unlock(const uint64_t resource, const uint32_t subresource, const uint32_t offset, const void* data,
                              const size_t data_size)
    {
        if (data == nullptr || data_size == 0)
        {
            return d3d_ok; // read-only lock, nothing to write back
        }

        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return d3derr_invalidcall;
        }
        if (subresource != 0 && (subresource - 1) >= it->second.extra_mips.size())
        {
            return d3derr_invalidcall;
        }

        auto& backing = it->second.subresource_backing(subresource);
        const size_t required_size = static_cast<size_t>(offset) + data_size;
        if (backing.size() < required_size)
        {
            backing.resize(required_size);
        }
        std::memcpy(backing.data() + offset, data, data_size);
        if (texblt_diag_enabled())
        {
            uint64_t hash = 14695981039346656037ull;
            for (size_t i = 0; i < data_size; ++i)
            {
                hash ^= static_cast<const uint8_t*>(data)[i];
                hash *= 1099511628211ull;
            }
            fprintf(stderr, "[d3d9-unlock-diag] resource=%llu subresource=%u offset=%u data_size=%zu data_hash=0x%llx\n",
                    static_cast<unsigned long long>(resource), subresource, offset, data_size, static_cast<unsigned long long>(hash));
        }
        // The one write-back path for a sampled texture's pixels, so this is where staleness originates:
        // the GPU image (if this resource has one) is now out of date and ensure_texture_uploaded must
        // re-upload before the next draw samples it. Unconditional -- cheap, and deliberately not
        // narrowed to "is this a texture kind", since a mislabelled resource that later turns out to be
        // sampled would then silently render stale pixels.
        it->second.upload_dirty = true;
        // Same unconditional treatment for execute_draw's vertex/index upload cache (Task #161) -- this is
        // the one site that mutates `backing` without the batch already having been flushed first (every
        // other writer -- tex_blt, sync_backing_from_gpu -- calls flush_batch() before touching backing),
        // so it is the one site that needs the version bumped rather than just relying on a flush having
        // already invalidated any in-flight cached offset.
        ++it->second.content_version;
        return d3d_ok;
    }

    std::byte* d3d9_host::prepare_unlock_target(const uint64_t resource, const uint32_t subresource, const uint32_t offset,
                                                const size_t data_size)
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return nullptr;
        }
        if (subresource != 0 && (subresource - 1) >= it->second.extra_mips.size())
        {
            return nullptr;
        }

        // A direct-buffer resource (see create_resource's eligibility check) writes straight into its
        // real, always-current GPU buffer instead of `backing` -- unlike `backing`, this buffer is
        // fixed-size (allocated once at resource creation), so an out-of-range write is rejected rather
        // than silently growing it (growing would mean reallocating live GPU memory a draw might already
        // be bound to).
        if (direct_miss_diag_enabled())
        {
            static std::atomic<uint64_t> direct_hits{};
            static std::atomic<uint64_t> non_direct{};
            static auto window_start = std::chrono::steady_clock::now();
            if (it->second.vk_direct_buffer_id != 0)
            {
                direct_hits.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                non_direct.fetch_add(1, std::memory_order_relaxed);
            }
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - window_start).count() >= 5.0)
            {
                window_start = now;
                fprintf(stderr, "[d3d9-direct-miss-diag] unlock: direct_but_still_crossed=%llu non_direct=%llu\n",
                        static_cast<unsigned long long>(direct_hits.exchange(0)), static_cast<unsigned long long>(non_direct.exchange(0)));
            }

            // One-time dump of every distinct non-direct resource ever unlocked, to characterize WHY
            // they missed eligibility (kind/pool/usage/width) -- logged once per resource id, not per
            // call, to stay readable across a whole run.
            if (it->second.vk_direct_buffer_id == 0)
            {
                static std::unordered_set<uint64_t> logged_misses;
                if (logged_misses.insert(resource).second)
                {
                    fprintf(stderr, "[d3d9-direct-miss-diag] non-direct resource=%llu kind=%u pool=%u usage=0x%X width=%u\n",
                            static_cast<unsigned long long>(resource), it->second.kind, it->second.pool, it->second.usage,
                            it->second.width);
                }
            }
        }

        if (it->second.vk_direct_buffer_id != 0 && subresource == 0)
        {
            const size_t required_size = static_cast<size_t>(offset) + data_size;
            if (required_size > it->second.width)
            {
                return nullptr;
            }
            return static_cast<std::byte*>(it->second.direct_mapped_ptr) + it->second.direct_slice_offset + offset;
        }

        auto& backing = it->second.subresource_backing(subresource);
        const size_t required_size = static_cast<size_t>(offset) + data_size;
        if (backing.size() < required_size)
        {
            backing.resize(required_size);
        }
        return backing.data() + offset;
    }

    void d3d9_host::finish_unlock(const uint64_t resource, const uint32_t subresource)
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return;
        }
        if (subresource != 0 && (subresource - 1) >= it->second.extra_mips.size())
        {
            return;
        }

        // Mirrors unlock()'s own bookkeeping above -- see its comments for why both flags are
        // unconditional.
        it->second.upload_dirty = true;
        ++it->second.content_version;
    }

    bool d3d9_host::get_direct_mapping(const uint64_t resource, void*& out_ptr, size_t& out_size, uint32_t& out_slice_stride,
                                       uint32_t& out_slice_count) const
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end() || it->second.vk_direct_buffer_id == 0)
        {
            return false;
        }

        out_ptr = it->second.direct_mapped_ptr;
        out_size = static_cast<size_t>(it->second.direct_slice_stride) * it->second.direct_slice_count;
        out_slice_stride = it->second.direct_slice_stride;
        out_slice_count = it->second.direct_slice_count;
        return true;
    }

    void d3d9_host::begin_flush_pending()
    {
        this->submit_batch_async();
    }

    bool d3d9_host::poll_flush_complete()
    {
        bool all_complete = true;
        for (uint32_t slot = 0; slot < batch_slot_count; ++slot)
        {
            if (!this->batch_slot_pending_[slot])
            {
                continue;
            }

            // Only VK_NOT_READY means "still executing". Anything else -- completion or a lost device --
            // retires the slot, so a device loss can never leave the guest thread parked forever.
            if (this->vulkan_.get_fence_status(this->batch_fence_[slot]) == VK_NOT_READY)
            {
                all_complete = false;
                continue;
            }

            this->retire_batch_slot(slot);
        }

        return all_complete;
    }

    int32_t d3d9_host::create_vertex_shader(const void* tokens, const size_t token_size_bytes, uint64_t& out_shader)
    {
        out_shader = 0;
        if (tokens == nullptr || token_size_bytes == 0 || (token_size_bytes % sizeof(uint32_t)) != 0)
        {
            return d3derr_invalidcall;
        }

        shader_entry entry{};
        entry.tokens.resize(token_size_bytes / sizeof(uint32_t));
        std::memcpy(entry.tokens.data(), tokens, token_size_bytes);

        const uint64_t id = this->allocate_id();

        // EMULATOR_D3D9_SHADERDUMP=<dir> writes every shader the guest creates, as D3D assembly, to
        // <dir>/sh_<id>.asm. Read alongside the draws_per_shader_pair counter (which names the ids that
        // actually matter) this turns "the pixels are wrong and every counter is clean" into a readable
        // program. Files are named by the same resource id the counters print, so the two join directly.
        if (const char* dump_dir = getenv("EMULATOR_D3D9_SHADERDUMP"))
        {
            std::string text;
            if (disassemble_d3d9_shader(tokens, token_size_bytes, text))
            {
                std::array<char, 512> path{};
                std::snprintf(path.data(), path.size(), "%s/sh_%llu.asm", dump_dir, static_cast<unsigned long long>(id));
                if (FILE* file = std::fopen(path.data(), "wb"))
                {
                    std::fwrite(text.data(), 1, text.size(), file);
                    std::fclose(file);
                }
            }
        }

        this->shaders_.emplace(id, std::move(entry));
        out_shader = id;
        return d3d_ok;
    }

    int32_t d3d9_host::create_pixel_shader(const void* tokens, const size_t token_size_bytes, uint64_t& out_shader)
    {
        // Vertex and pixel shaders share the same token-blob storage model at this stage; they only
        // diverge once Part 4 feeds their tokens through vkd3d-shader with stage-specific interface info.
        return this->create_vertex_shader(tokens, token_size_bytes, out_shader);
    }

    // --- Vertex declaration parsing -------------------------------------------------------------
    //
    // Location-assignment investigation (Task 7 of the multi-stream-vertex-source plan):
    //
    // The open question was how vkd3d-shader assigns SPIR-V `Location` decorations to a translated
    // D3D9 (SM1-3 "D3DBC") vertex shader's input registers (v0, v1, ...), since that is exactly what
    // a Vulkan VkVertexInputAttributeDescription's own `location` field must match.
    //
    // Read directly out of this project's vendored deps/vkd3d/libs/vkd3d-shader sources:
    //   - d3dbc.c's add_signature_element (~line 707) sets `element->target_location = register_index`
    //     unconditionally for every D3DBC (SM1-3) input-signature element -- register_index being the
    //     literal v# the shader's own `dcl_<usage><index> vN` token declared.
    //   - spirv.c's spirv_compiler_emit_input_register (~lines 5725/5735) reads that same
    //     `target_location` straight into `OpDecorate %v<N> Location <target_location>` for every
    //     vertex-shader input, with no further remapping.
    //   - The one place vkd3d-shader *does* rewrite `target_location` post-hoc is
    //     vsir_program_remap_output_signature (ir.c ~line 4076), but that only ever touches
    //     `program->output_signature` -- i.e. a VS's *output* varyings feeding a PS's input, via the
    //     varying_map machinery d3d9_shader_translator.cpp's compile_stage() builds. It is never
    //     consulted for a VS's own *input* signature, which is what a vertex declaration feeds.
    //
    // Conclusion: a compiled D3D9 vertex shader's input Location is simply its declared v# register
    // index, decided entirely by where its HLSL input struct/D3DBC dcl instructions place that
    // semantic -- NOT by the semantic (D3DDECLUSAGE) itself.
    //
    // Empirically confirmed (not just read) via this repo's own deps/vkd3d/programs/vkd3d-compiler,
    // built ad hoc against the already-built libvkd3d-shader.dylib and run against hand-written HLSL
    // vs_2_0 shaders, then inspected with spirv-dis:
    //   struct { POSITION, TEXCOORD0, COLOR0 } -> dcl_position v0, dcl_texcoord0 v1, dcl_color v2
    //                                          -> OpDecorate %v0/%v1/%v2 Location 0/1/2
    //   struct { POSITION, COLOR0, TEXCOORD0 } -> dcl_position v0, dcl_color v1, dcl_texcoord0 v2
    //                                          -> OpDecorate %v0/%v1/%v2 Location 0/1/2
    //   struct { COLOR0, POSITION, TEXCOORD0 } -> dcl_color v0, dcl_position v1, dcl_texcoord0 v2
    //                                          -> OpDecorate %v0/%v1/%v2 Location 0/1/2
    // All three assign Location purely by struct/declaration order; POSITION gets no special-casing
    // (the third case puts it at v1/Location 1, not v0/Location 0). This directly rules out a static
    // D3DDECLUSAGE(+usage_index) -> location table as originally planned: usage does not determine
    // location, declaration position does.
    //
    // Consequence for parse_vertex_decl below: since this parser deliberately has no visibility into
    // the paired vertex shader (that pairing is Task 8's concern, not this standalone task's), the
    // only location it can produce is each element's own ordinal position within the D3DVERTEXELEMENT9
    // array `blob` encodes -- under the (documented, currently-true-for-every-shader-in-this-repo)
    // convention that a vertex declaration's element order matches its paired vertex shader's HLSL
    // input-struct order. Every existing shader in this codebase (d3d9_const_test.cpp,
    // d3d9_shader_test.cpp, d3d9_texcoord_test.cpp, etc.) declares POSITION first, matching this.
    // Task 8/9 must preserve that convention for new multi-stream shapes; the fully general fix (cross-
    // referencing the actually-bound VS's own scanned input signature instead of assuming declaration
    // order) is future work if that convention is ever violated.
    parsed_vertex_decl parse_vertex_decl(const std::span<const std::byte> blob)
    {
        parsed_vertex_decl out{};

        const size_t element_count = blob.size() / sizeof(d3d9_cmd::vertex_element);
        std::vector<d3d9_cmd::vertex_element> elements(element_count);
        if (element_count != 0)
        {
            std::memcpy(elements.data(), blob.data(), element_count * sizeof(d3d9_cmd::vertex_element));
        }

        for (uint32_t location = 0; location < element_count; ++location)
        {
            const d3d9_cmd::vertex_element& element = elements[location];
            uint32_t vk_format = 0;
            if (!d3d9_decl_type_to_vulkan(element.type, vk_format))
            {
                continue; // Unrecognized D3DDECLTYPE -- skip, don't guess a format (see the .hpp comment).
            }

            out.attributes.push_back({
                .location = location,
                .binding = element.stream,
                .vk_format = vk_format,
                .offset = element.offset,
            });
            // element.stream is an untrusted guest-supplied uint16_t (see d3d9_host.hpp's
            // bound_textures/render_state comment for this same class of guest-value concern) --
            // shifting a uint32_t by >= 32 is UB, so bound the shift instead of trusting the wire
            // value. This project's own UMD caps MaxStreams at 16 (sogen_d3d9_umd.cpp), so every
            // legitimate declaration is unaffected.
            if (element.stream < 32)
            {
                out.used_binding_mask |= (1u << element.stream);
            }
        }

        return out;
    }

    int32_t d3d9_host::create_vertex_decl(const void* elements, const size_t element_count, const size_t element_size_bytes,
                                          uint64_t& out_decl)
    {
        out_decl = 0;
        if (elements == nullptr && element_count != 0)
        {
            return d3derr_invalidcall;
        }

        vertex_decl_entry entry{};
        entry.elements.resize(element_count * element_size_bytes);
        if (element_count != 0)
        {
            std::memcpy(entry.elements.data(), elements, entry.elements.size());
        }
        entry.parsed = parse_vertex_decl(entry.elements);

        const uint64_t id = this->allocate_id();
        this->vertex_decls_.emplace(id, std::move(entry));
        out_decl = id;
        return d3d_ok;
    }

    int32_t d3d9_host::execute_recorded(const uint32_t opcode, const std::byte* payload, const size_t size)
    {
        switch (static_cast<gpu_bridge::command>(opcode))
        {
        case gpu_bridge::command::d3d9_set_render_state: {
            d3d9_cmd::set_render_state_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.render_state[req.state] = req.value;
            this->stats_.render_state_values[req.state][req.value] += 1;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_texture_stage_state: {
            d3d9_cmd::set_texture_stage_state_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.texture_stage_state[tss_key(req.stage, req.state)] = req.value;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_sampler_state: {
            d3d9_cmd::set_sampler_state_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.sampler_state[tss_key(req.sampler, req.state)] = req.value;
            ++this->state_.sampler_state_version;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_texture: {
            d3d9_cmd::set_texture_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.bound_textures[req.stage] = req.texture;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_stream_source: {
            d3d9_cmd::set_stream_source_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.stream_sources[req.stream_number] = req.vertex_buffer;
            this->state_.stream_strides[req.stream_number] = req.stride_bytes;
            this->state_.stream_offsets[req.stream_number] = req.offset_bytes;
            this->state_.stream_um_data.erase(req.stream_number); // a real buffer bind supersedes UM
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_stream_source_freq: {
            d3d9_cmd::set_stream_source_freq_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.stream_frequencies[req.stream_number] = req.frequency;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_indices: {
            d3d9_cmd::set_indices_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.index_buffer = req.index_buffer;
            this->state_.index_format = req.format;
            this->state_.index_um_data.clear(); // a real index buffer bind supersedes UM
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_vertex_decl: {
            d3d9_cmd::set_vertex_decl_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.vertex_decl = req.decl;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_vertex_shader: {
            d3d9_cmd::set_vertex_shader_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.vertex_shader = req.shader;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_pixel_shader: {
            d3d9_cmd::set_pixel_shader_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.pixel_shader = req.shader;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_vs_const_f:
        case gpu_bridge::command::d3d9_set_ps_const_f: {
            d3d9_cmd::set_const_f_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            const size_t float_count = static_cast<size_t>(req.vector4_count) * 4;
            const size_t bytes = float_count * sizeof(float);
            if (size - sizeof(req) < bytes)
            {
                return d3derr_invalidcall;
            }
            const bool is_vs = static_cast<gpu_bridge::command>(opcode) == gpu_bridge::command::d3d9_set_vs_const_f;
            auto& target = is_vs ? this->state_.vs_const_f : this->state_.ps_const_f;
            if (req.vector4_count > 0)
            {
                auto& high = is_vs ? this->stats_.max_vs_const_f_register : this->stats_.max_ps_const_f_register;
                high = std::max(high, req.start_register + req.vector4_count - 1);
            }
            const size_t required = static_cast<size_t>(req.start_register) * 4 + float_count;
            if (target.size() < required)
            {
                target.resize(required);
            }
            std::memcpy(target.data() + static_cast<size_t>(req.start_register) * 4, payload + sizeof(req), bytes);
            ++this->state_.const_versions[is_vs ? ubo_vs_f : ubo_ps_f];
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_vs_const_i:
        case gpu_bridge::command::d3d9_set_ps_const_i: {
            d3d9_cmd::set_const_i_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            const size_t int_count = static_cast<size_t>(req.vector4_count) * 4;
            const size_t bytes = int_count * sizeof(int32_t);
            if (size - sizeof(req) < bytes)
            {
                return d3derr_invalidcall;
            }
            const bool is_vs = static_cast<gpu_bridge::command>(opcode) == gpu_bridge::command::d3d9_set_vs_const_i;
            auto& target = is_vs ? this->state_.vs_const_i : this->state_.ps_const_i;
            const size_t required = static_cast<size_t>(req.start_register) * 4 + int_count;
            if (target.size() < required)
            {
                target.resize(required);
            }
            std::memcpy(target.data() + static_cast<size_t>(req.start_register) * 4, payload + sizeof(req), bytes);
            ++this->state_.const_versions[is_vs ? ubo_vs_i : ubo_ps_i];
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_vs_const_b:
        case gpu_bridge::command::d3d9_set_ps_const_b: {
            d3d9_cmd::set_const_b_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            const size_t bytes = static_cast<size_t>(req.count) * sizeof(uint32_t);
            if (size - sizeof(req) < bytes)
            {
                return d3derr_invalidcall;
            }
            const bool is_vs = static_cast<gpu_bridge::command>(opcode) == gpu_bridge::command::d3d9_set_vs_const_b;
            auto& target = is_vs ? this->state_.vs_const_b : this->state_.ps_const_b;
            const size_t required = (static_cast<size_t>(req.start_register) + req.count) * 4;
            if (target.size() < required)
            {
                target.resize(required);
            }
            for (uint32_t i = 0; i < req.count; ++i)
            {
                uint32_t value{};
                std::memcpy(&value, payload + sizeof(req) + static_cast<size_t>(i) * sizeof(uint32_t), sizeof(value));
                target[(static_cast<size_t>(req.start_register) + i) * 4] = value;
            }
            ++this->state_.const_versions[is_vs ? ubo_vs_b : ubo_ps_b];
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_render_target: {
            d3d9_cmd::set_render_target_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            if (req.render_target_index >= std::size(this->state_.render_targets))
            {
                return d3derr_invalidcall;
            }
            // Deliberately does NOT flush. A rebind is pure CPU-side state: execute_draw's own slot-0
            // batch_rt_ guard closes and ROTATES the batch (submit + wait one slot back) on the next
            // draw, which is the same ordering a full drain gave but pipelined, and a change to a
            // non-slot-0 MRT slot is caught by the render-pass attachment comparison there instead. The
            // one thing a drain also did implicitly -- forcing a still-deferred Clear() to be realized
            // before its target could be rebound -- is now handled by pending_batch_clear::color_targets.
            this->state_.render_targets[req.render_target_index] = req.surface;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_depth_stencil: {
            d3d9_cmd::set_depth_stencil_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            // Same no-flush reasoning as set_render_target above: execute_draw's batch_ds_ guard closes
            // and rotates the batch on the next draw, which is what actually keeps a batch's single
            // inter-draw depth barrier covering only the one image that batch accumulates into. A
            // deferred depth Clear() is already immune to a rebind -- it resolves against batch_ds_, a
            // per-batch snapshot, and any depth-stencil change rotates the batch before a new one opens.
            this->state_.depth_stencil = req.surface;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_viewport: {
            d3d9_cmd::set_viewport_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.viewport_x = req.x;
            this->state_.viewport_y = req.y;
            this->state_.viewport_width = req.width;
            this->state_.viewport_height = req.height;
            this->state_.viewport_min_z = req.min_z;
            this->state_.viewport_max_z = req.max_z;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_scissor: {
            d3d9_cmd::set_scissor_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.scissor_left = req.left;
            this->state_.scissor_top = req.top;
            this->state_.scissor_right = req.right;
            this->state_.scissor_bottom = req.bottom;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_clear: {
            d3d9_cmd::clear_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }

            // D3DCLEAR_TARGET clears ALL currently-bound render targets (D3D9's SetRenderTarget slots
            // 0-3) to the single supplied color, not just slot 0 -- same slot resolution as
            // execute_draw's rt_slots loop, just without needing a Vulkan format for a pipeline.
            constexpr uint32_t d3dclear_target = 0x1;
            constexpr uint32_t d3dclear_zbuffer = 0x2;
            constexpr uint32_t d3dclear_stencil = 0x4;
            this->stats_.clear_target += (req.flags & d3dclear_target) != 0 ? 1 : 0;
            this->stats_.clear_zbuffer += (req.flags & d3dclear_zbuffer) != 0 ? 1 : 0;
            this->stats_.clear_stencil += (req.flags & d3dclear_stencil) != 0 ? 1 : 0;

            const uint64_t device = this->ensure_vk_device();
            if (device == 0 || !this->ensure_draw_infra())
            {
                return d3d_ok; // no GPU backing yet -- nothing to clear
            }

            // D3DCLEAR_ZBUFFER/D3DCLEAR_STENCIL against the currently-bound depth-stencil. Before this
            // batching was added, both bits were silently dropped and the depth buffer was only ever
            // cleared ONCE, by ensure_depth_stencil_view's first-use initialization -- so every frame
            // after the first depth-tested one ran against the accumulated minimum depth of every frame
            // before it. With a moving camera that stale buffer rejects essentially every world fragment
            // (the classic symptom: a correct HUD, drawn with D3DRS_ZENABLE off, over a completely black
            // 3D scene), while a purely 2D app never notices because it never binds a depth-stencil at
            // all. Resolved up front, before the batch below is (re)opened, because
            // ensure_depth_stencil_view's first-use initialization does its own flush_batch() plus a
            // synchronous submit+wait -- that has to happen before, not interleaved with, the batched
            // recording further down.
            resource_entry* ds_entry = nullptr;
            uint32_t depth_vk_format = 0;
            if ((req.flags & (d3dclear_zbuffer | d3dclear_stencil)) != 0 && this->state_.depth_stencil != 0)
            {
                const auto ds_it = this->resources_.find(this->state_.depth_stencil);
                if (ds_it != this->resources_.end() && ds_it->second.vk_image_id != 0 &&
                    d3d9_format_to_vulkan(ds_it->second.format, depth_vk_format) &&
                    // Establishes the DEPTH_STENCIL_ATTACHMENT_OPTIMAL resting layout
                    // batch_clear_depth_stencil_image transitions out of; on a resource whose view already
                    // exists this is a no-op.
                    this->ensure_depth_stencil_view(device, ds_it->second, depth_vk_format))
                {
                    ds_entry = &ds_it->second;
                }
            }

            const bool clear_color = (req.flags & d3dclear_target) != 0;
            if (!clear_color && ds_entry == nullptr)
            {
                return d3d_ok;
            }

            // Record the clear(s) into the currently open batch (opening/continuing one on the same
            // render-target/depth-stencil identity execute_draw itself uses) instead of the old standalone
            // synchronous submit_clear/clear_depth_stencil -- lets the deferred, round-robin wait the
            // batch system already uses for draws cover Clear too, instead of a hard CPU/GPU stall on
            // every single Clear() call (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER together used to be up to 4
            // separate wait_for_fence(UINT64_MAX) calls). Any draw or readback ordered after this Clear
            // still observes its result correctly: it either continues recording into this SAME command
            // buffer (ordinary program-order execution plus the barriers below), or -- for a readback --
            // goes through sync_backing_from_gpu, which already flushes the batch (draining this Clear's
            // fence) before touching GPU memory.
            const uint64_t target_rt = this->state_.render_targets[0];
            const uint64_t target_ds = ds_entry != nullptr ? this->state_.depth_stencil : 0;
            this->ensure_batch_open(device, target_rt, target_ds);

            // LOAD_OP_CLEAR fast path: when nothing has drawn into this batch's render-pass instance
            // yet (open_render_pass_[slot].open == false), the cheapest possible clear defers its value
            // to whichever render-pass-begin comes next -- see pending_clear_'s own comment -- instead
            // of an explicit vkCmdClear*Image. D3DCLEAR_STENCIL against a format that actually carries a
            // stencil aspect always falls back: cmd_begin_rendering never wires up a separate stencil
            // attachment (see execute_draw's depth_attachment), so a stencil load op has nowhere to go.
            //
            // A transient depth-stencil (see vulkan_host::create_render_target's transient parameter)
            // has no TRANSFER_DST_BIT usage at all, so the explicit vkCmdClearDepthStencilImage fallback
            // is not just slower but outright invalid Vulkan usage for it -- the fast path is therefore
            // the ONLY path for a transient depth-stencil, unconditionally, even mid-instance (rp.open):
            // its store_op is already VK_ATTACHMENT_STORE_OP_DONT_CARE (execute_draw), so whatever the
            // currently-open instance wrote to it was never going to survive past this Clear() anyway --
            // closing it now to make way for a fresh LOAD_OP_CLEAR instance costs nothing extra.
            open_render_pass_state& rp = this->open_render_pass_[this->batch_slot_];
            pending_batch_clear& pending = this->pending_clear_[this->batch_slot_];
            // An earlier Clear() still deferred against a render-target set the app has since rebound
            // would be silently dropped by the overwrite below. Realize it against its own snapshot first.
            if (pending.color_pending && pending.color_targets != this->state_.render_targets)
            {
                this->realize_pending_color_clear(pending);
            }
            const bool depth_transient =
                depth_stencil_transient_enabled && ds_entry != nullptr && (ds_entry->usage & d3dusage_depthstencil) != 0;
            const bool color_fast_path = clear_color && !rp.open;
            const bool depth_fast_path =
                ds_entry != nullptr &&
                (depth_transient || (!rp.open && ((req.flags & d3dclear_stencil) == 0 ||
                                                  (depth_aspect_mask(depth_vk_format) & VK_IMAGE_ASPECT_STENCIL_BIT) == 0)));

            if ((clear_color && !color_fast_path) || (ds_entry != nullptr && (!depth_fast_path || rp.open)))
            {
                this->close_render_pass(this->batch_slot_); // vkCmdClear*Image is illegal inside a render pass instance
            }

            if (clear_color)
            {
                // D3DCOLOR is 0xAARRGGBB.
                const std::array<float, 4> color{
                    static_cast<float>((req.color_argb >> 16) & 0xFF) / 255.0f,
                    static_cast<float>((req.color_argb >> 8) & 0xFF) / 255.0f,
                    static_cast<float>(req.color_argb & 0xFF) / 255.0f,
                    static_cast<float>((req.color_argb >> 24) & 0xFF) / 255.0f,
                };
                if (color_fast_path)
                {
                    pending.color_pending = true;
                    pending.color_value = color;
                    pending.color_targets = this->state_.render_targets;
                }
                else
                {
                    pending.color_pending = false;
                }
                for (const uint64_t rt_handle : this->state_.render_targets)
                {
                    const auto it = this->resources_.find(rt_handle);
                    if (it == this->resources_.end() || it->second.vk_image_id == 0)
                    {
                        continue;
                    }
                    if (!color_fast_path)
                    {
                        this->batch_clear_color_image(it->second.vk_image_id, color);
                    }

                    // Marks every bound render target's backing store stale; sync_backing_from_gpu reads
                    // it back lazily on the next pfnLock/Present -- see the class comment for why this
                    // sidesteps needing to know how the real d3d9.dll gets pixels onto an actual window.
                    it->second.backing_dirty = true;
                }
            }

            if (ds_entry != nullptr)
            {
                if (depth_fast_path)
                {
                    pending.depth_pending = true;
                    pending.depth_value = req.z;
                    pending.stencil_value = req.stencil;
                }
                else
                {
                    pending.depth_pending = false;
                    uint32_t aspects = 0;
                    aspects |= (req.flags & d3dclear_zbuffer) != 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u;
                    aspects |= (req.flags & d3dclear_stencil) != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u;
                    this->batch_clear_depth_stencil_image(*ds_entry, depth_vk_format, aspects, req.z, req.stencil);
                }
            }
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_draw_primitive: {
            d3d9_cmd::draw_primitive_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            // The pipeline is hardcoded to VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST (see ensure_pipeline);
            // primitive_type is not yet consulted -- D3DPT_TRIANGLELIST only for this milestone.
            return this->execute_draw(req.primitive_count * 3, req.start_vertex);
        }
        case gpu_bridge::command::d3d9_draw_indexed_primitive: {
            d3d9_cmd::draw_indexed_primitive_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            // Same D3DPT_TRIANGLELIST-only assumption as d3d9_draw_primitive.
            const indexed_draw indexed{.index_buffer = this->state_.index_buffer,
                                       .index_format = this->state_.index_format,
                                       .first_index = req.start_index,
                                       .base_vertex_index = req.base_vertex_index,
                                       .min_vertex_index = req.min_vertex_index,
                                       .num_vertices = req.num_vertices};
            return this->execute_draw(req.primitive_count * 3, 0, &indexed);
        }
        case gpu_bridge::command::d3d9_set_stream_source_um: {
            d3d9_cmd::set_stream_source_um_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            if (size - sizeof(req) < req.vertex_data_size)
            {
                return d3derr_invalidcall;
            }
            // Bind this stream as a UM (DrawPrimitiveUP) source: stash the inline vertex bytes and clear
            // any real buffer binding for the slot. stream_strides is populated exactly as the real
            // buffer path does, so vertex_shape_key()'s per-stream stride keying covers UM streams too.
            const std::byte* bytes = payload + sizeof(req);
            this->state_.stream_um_data[req.stream_number].assign(bytes, bytes + req.vertex_data_size);
            this->state_.stream_strides[req.stream_number] = req.stride_bytes;
            this->state_.stream_offsets[req.stream_number] = req.offset_bytes;
            this->state_.stream_sources[req.stream_number] = 0;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_indices_um: {
            d3d9_cmd::set_indices_um_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            if (size - sizeof(req) < req.index_data_size)
            {
                return d3derr_invalidcall;
            }
            // Bind the index source as UM (DrawIndexedPrimitiveUP): stash the inline index bytes and
            // clear any real index buffer binding. index_format mirrors set_indices_record (0/1).
            const std::byte* bytes = payload + sizeof(req);
            this->state_.index_um_data.assign(bytes, bytes + req.index_data_size);
            this->state_.index_format = req.index_element_size == 4 ? 1u : 0u;
            this->state_.index_buffer = 0;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_direct_slice: {
            d3d9_cmd::set_direct_slice_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            const auto it = this->resources_.find(req.resource);
            if (it == this->resources_.end() || it->second.vk_direct_buffer_id == 0)
            {
                return d3derr_invalidcall;
            }
            // Replaying in stream order is the whole mechanism: draws recorded before this record have
            // already been through execute_draw with the previous slice baked into their bind offset, so
            // moving the live slice here cannot retroactively change what they read.
            const uint64_t ring_size = static_cast<uint64_t>(it->second.direct_slice_stride) * it->second.direct_slice_count;
            if (req.slice_offset + static_cast<uint64_t>(it->second.width) > ring_size)
            {
                return d3derr_invalidcall;
            }
            it->second.direct_slice_offset = req.slice_offset;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_color_fill: {
            d3d9_cmd::color_fill_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            return this->color_fill(req.resource, req.subresource, req.left, req.top, req.right, req.bottom, req.color_argb);
        }
        case gpu_bridge::command::d3d9_blt: {
            d3d9_cmd::blt_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            return this->blt(req.dst_resource, req.dst_subresource, req.dst_left, req.dst_top, req.dst_right, req.dst_bottom,
                             req.src_resource, req.src_subresource, req.src_left, req.src_top, req.src_right, req.src_bottom, req.filter);
        }
        default:
            return d3derr_invalidcall;
        }
    }
} // namespace sogen
