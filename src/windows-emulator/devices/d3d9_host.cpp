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
#include <cassert>
#include <cstring>
#include <span>

namespace sogen
{
    namespace
    {
        constexpr int32_t d3derr_invalidcall = -2005530516; // D3DERR_INVALIDCALL
        constexpr int32_t d3d_ok = 0;

        // Public, ABI-stable D3D9 API constants (d3d9types.h), not RE'd DDI internals.
        constexpr uint32_t d3dusage_rendertarget = 0x00000001;
        constexpr uint32_t d3dusage_depthstencil = 0x00000002;

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
        constexpr std::array<uint32_t, 351> k_ff_vertex_shader_spirv = {
            0x07230203u, 0x00010000u, 0x0008000bu, 0x00000030u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
            0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
            0x0009000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000cu, 0x00000022u, 0x0000002du,
            0x0000002eu, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
            0x00030005u, 0x00000009u, 0x0063646eu, 0x00060005u, 0x0000000cu, 0x6f506e69u, 0x69746973u, 0x68526e6fu,
            0x00000077u, 0x00060005u, 0x0000000fu, 0x68737550u, 0x736e6f43u, 0x746e6174u, 0x00000073u, 0x00070006u,
            0x0000000fu, 0x00000000u, 0x77656976u, 0x74726f70u, 0x657a6953u, 0x00000000u, 0x00030005u, 0x00000011u,
            0x00006370u, 0x00060005u, 0x00000020u, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u,
            0x00000020u, 0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x00000020u, 0x00000001u,
            0x505f6c67u, 0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x00000020u, 0x00000002u, 0x435f6c67u,
            0x4470696cu, 0x61747369u, 0x0065636eu, 0x00070006u, 0x00000020u, 0x00000003u, 0x435f6c67u, 0x446c6c75u,
            0x61747369u, 0x0065636eu, 0x00030005u, 0x00000022u, 0x00000000u, 0x00050005u, 0x0000002du, 0x67617266u,
            0x6f6c6f43u, 0x00000072u, 0x00040005u, 0x0000002eu, 0x6f436e69u, 0x00726f6cu, 0x00040047u, 0x0000000cu,
            0x0000001eu, 0x00000000u, 0x00030047u, 0x0000000fu, 0x00000002u, 0x00050048u, 0x0000000fu, 0x00000000u,
            0x00000023u, 0x00000000u, 0x00030047u, 0x00000020u, 0x00000002u, 0x00050048u, 0x00000020u, 0x00000000u,
            0x0000000bu, 0x00000000u, 0x00050048u, 0x00000020u, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u,
            0x00000020u, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x00000020u, 0x00000003u, 0x0000000bu,
            0x00000004u, 0x00040047u, 0x0000002du, 0x0000001eu, 0x00000000u, 0x00040047u, 0x0000002eu, 0x0000001eu,
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
            0x00000024u, 0x00000002u, 0x00040020u, 0x00000025u, 0x00000001u, 0x00000006u, 0x00040020u, 0x0000002bu,
            0x00000003u, 0x0000000au, 0x0004003bu, 0x0000002bu, 0x0000002du, 0x00000003u, 0x0004003bu, 0x0000000bu,
            0x0000002eu, 0x00000001u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
            0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000007u, 0x0004003du, 0x0000000au, 0x0000000du,
            0x0000000cu, 0x0007004fu, 0x00000007u, 0x0000000eu, 0x0000000du, 0x0000000du, 0x00000000u, 0x00000001u,
            0x00050041u, 0x00000014u, 0x00000015u, 0x00000011u, 0x00000013u, 0x0004003du, 0x00000007u, 0x00000016u,
            0x00000015u, 0x00050088u, 0x00000007u, 0x00000017u, 0x0000000eu, 0x00000016u, 0x0005008eu, 0x00000007u,
            0x00000019u, 0x00000017u, 0x00000018u, 0x00050050u, 0x00000007u, 0x0000001bu, 0x0000001au, 0x0000001au,
            0x00050083u, 0x00000007u, 0x0000001cu, 0x00000019u, 0x0000001bu, 0x0003003eu, 0x00000009u, 0x0000001cu,
            0x0004003du, 0x00000007u, 0x00000023u, 0x00000009u, 0x00050041u, 0x00000025u, 0x00000026u, 0x0000000cu,
            0x00000024u, 0x0004003du, 0x00000006u, 0x00000027u, 0x00000026u, 0x00050051u, 0x00000006u, 0x00000028u,
            0x00000023u, 0x00000000u, 0x00050051u, 0x00000006u, 0x00000029u, 0x00000023u, 0x00000001u, 0x00070050u,
            0x0000000au, 0x0000002au, 0x00000028u, 0x00000029u, 0x00000027u, 0x0000001au, 0x00050041u, 0x0000002bu,
            0x0000002cu, 0x00000022u, 0x00000013u, 0x0003003eu, 0x0000002cu, 0x0000002au, 0x0004003du, 0x0000000au,
            0x0000002fu, 0x0000002eu, 0x0003003eu, 0x0000002du, 0x0000002fu, 0x000100fdu, 0x00010038u,
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
            case d3dcmp_never: return VK_COMPARE_OP_NEVER;
            case d3dcmp_less: return VK_COMPARE_OP_LESS;
            case d3dcmp_equal: return VK_COMPARE_OP_EQUAL;
            case d3dcmp_lessequal: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case d3dcmp_greater: return VK_COMPARE_OP_GREATER;
            case d3dcmp_notequal: return VK_COMPARE_OP_NOT_EQUAL;
            case d3dcmp_greaterequal: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case d3dcmp_always: return VK_COMPARE_OP_ALWAYS;
            default: return VK_COMPARE_OP_LESS_OR_EQUAL; // D3D9's own documented default for D3DRS_ZFUNC
            }
        }

        // Builds the pipeline's depth-test state from the app's accumulated render state. depth_format ==
        // 0 (no bound depth-stencil with real GPU backing) always yields test_enable == 0, matching this
        // function's pre-depth-testing behavior exactly for every draw that never binds one.
        vulkan_host::depth_state build_depth_state(const std::unordered_map<uint32_t, uint32_t>& render_state,
                                                    const uint32_t depth_format)
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
            case d3dblend_zero: return VK_BLEND_FACTOR_ZERO;
            case d3dblend_one: return VK_BLEND_FACTOR_ONE;
            case d3dblend_srccolor: return VK_BLEND_FACTOR_SRC_COLOR;
            case d3dblend_invsrccolor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case d3dblend_srcalpha: return VK_BLEND_FACTOR_SRC_ALPHA;
            case d3dblend_invsrcalpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case d3dblend_destalpha: return VK_BLEND_FACTOR_DST_ALPHA;
            case d3dblend_invdestalpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case d3dblend_destcolor: return VK_BLEND_FACTOR_DST_COLOR;
            case d3dblend_invdestcolor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case d3dblend_srcalphasat: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case d3dblend_blendfactor: return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case d3dblend_invblendfactor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            default: return VK_BLEND_FACTOR_ONE; // D3D9's own documented default for D3DRS_SRCBLEND
            }
        }

        // D3DBLENDOP -> VkBlendOp (direct 1:1 correspondence; D3DBLENDOP_* is 1-based, VK_BLEND_OP_* is
        // 0-based, hence ADD != ADD).
        uint32_t d3dblendop_to_vk_blend_op(const uint32_t d3dblendop)
        {
            switch (d3dblendop)
            {
            case d3dblendop_add: return VK_BLEND_OP_ADD;
            case d3dblendop_subtract: return VK_BLEND_OP_SUBTRACT;
            case d3dblendop_revsubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
            case d3dblendop_min: return VK_BLEND_OP_MIN;
            case d3dblendop_max: return VK_BLEND_OP_MAX;
            default: return VK_BLEND_OP_ADD; // D3D9's own documented default for D3DRS_BLENDOP
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
            const uint32_t dst_factor =
                d3dblend_to_vk_blend_factor(render_state_or(render_state, d3drs_destblend, d3dblend_zero));
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
        if (this->vulkan_.allocate_command_buffer(device, this->command_pool_, 0, this->command_buffer_) != 0 ||
            this->command_buffer_ == 0)
        {
            return false;
        }
        if (this->vulkan_.create_fence(device, 0, this->fence_) != 0 || this->fence_ == 0)
        {
            return false;
        }

        this->draw_infra_ready_ = true;
        return true;
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
            (this->vulkan_.create_shader_module(device, k_ff_vertex_shader_spirv.data(),
                                                k_ff_vertex_shader_spirv.size() * sizeof(uint32_t), this->vs_module_) != 0 ||
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
            (this->vulkan_.create_pipeline_layout(device, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, {},
                                                  this->pipeline_layout_) != 0 ||
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
            device, /*render_pass=*/0, this->pipeline_layout_, this->vs_module_, this->fs_module_, width, height, bindings,
            attributes, key.depth, color_formats, depth_format, /*stencil_format=*/0, /*rasterization_samples=*/1,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, /*primitive_restart_enable=*/0, dynamic_states, empty_spec, empty_spec, blend,
            pipeline);
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
                shape.strides[stream] =
                    stride_it != this->state_.stream_strides.end() ? stride_it->second : 0u;
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

    const d3d9_host::programmable_pipeline_entry* d3d9_host::ensure_programmable_pipeline(
        const std::span<const uint32_t> color_formats, const uint32_t width, const uint32_t height, const uint32_t depth_format)
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

        const auto cached = this->programmable_pipelines_.find(key);
        if (cached != this->programmable_pipelines_.end())
        {
            return &cached->second;
        }

        const auto vs_it = this->shaders_.find(this->state_.vertex_shader);
        const auto ps_it = this->shaders_.find(this->state_.pixel_shader);
        if (vs_it == this->shaders_.end() || ps_it == this->shaders_.end())
        {
            return nullptr;
        }

        shader_pair_spirv spirv{};
        if (!translate_d3d9_shader_pair(vs_it->second.tokens.data(), vs_it->second.tokens.size() * sizeof(uint32_t),
                                        ps_it->second.tokens.data(), ps_it->second.tokens.size() * sizeof(uint32_t), spirv))
        {
            return nullptr;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0)
        {
            return nullptr;
        }

        programmable_pipeline_entry entry{};
        if (this->vulkan_.create_shader_module(device, spirv.vertex_spirv.data(),
                                               spirv.vertex_spirv.size() * sizeof(uint32_t), entry.vs_module) != 0 ||
            entry.vs_module == 0)
        {
            return nullptr;
        }
        if (this->vulkan_.create_shader_module(device, spirv.pixel_spirv.data(),
                                               spirv.pixel_spirv.size() * sizeof(uint32_t), entry.fs_module) != 0 ||
            entry.fs_module == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            return nullptr;
        }

        // Matches the CBV bindings d3d9_shader_translator.cpp pins into the SPIR-V: VS float-const UBO
        // at set 0 binding 0, PS float-const UBO at set 1 binding 0. Bindings 2/3 (int-const/bool-const
        // UBOs) are declared here unconditionally too, same rationale as binding 1 below -- vkd3d only
        // emits an actual SPIR-V descriptor for a register file a shader statically references, but
        // Vulkan permits a pipeline layout to declare bindings a shader doesn't use. The actual per-draw
        // UBO creation happens in execute_draw; the descriptor sets themselves are cached on this
        // pipeline's programmable_pipeline_entry (see its own comment) and only their contents are
        // rewritten per draw, not the sets or their pool.
        // Combined-image-sampler bindings for the vertex stage's texture registers s0..s3 sit at
        // bindings 1, 4, 5, 6 within set 0 (bindings 2/3 are the int/bool-const UBOs) -- the same
        // per-set scheme the PS uses in set 1, generated from the shared
        // max_vs_sampler_stages/vs_sampler_binding_for_stage (d3d9_shader_translator.hpp) that
        // d3d9_shader_translator.cpp pins into the VS SPIR-V. Declared unconditionally for the same
        // reason as PS: Vulkan permits a layout to declare more bindings than a shader statically uses,
        // so a VS that never samples (the common case) is unaffected; execute_draw only writes a
        // descriptor when a vertex texture is actually bound to that stage.
        std::array<vulkan_host::descriptor_binding, 3 + max_vs_sampler_stages> vs_bindings{{
            {.binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
            {.binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
            {.binding = 3, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
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
            {.binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
            {.binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
            {.binding = 3, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
        }};
        for (uint32_t stage = 0; stage < max_ps_sampler_stages; ++stage)
        {
            ps_bindings[3 + stage] = {.binding = ps_sampler_binding_for_stage(stage),
                                      .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .descriptor_count = 1,
                                      .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT};
        }
        if (this->vulkan_.create_descriptor_set_layout(device, vs_bindings, entry.vs_set_layout) != 0 ||
            entry.vs_set_layout == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            return nullptr;
        }
        if (this->vulkan_.create_descriptor_set_layout(device, ps_bindings, entry.ps_set_layout) != 0 ||
            entry.ps_set_layout == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            return nullptr;
        }

        const std::array<uint64_t, 2> set_layouts{entry.vs_set_layout, entry.ps_set_layout};
        if (this->vulkan_.create_pipeline_layout(device, 0, 0, set_layouts, entry.pipeline_layout) != 0 ||
            entry.pipeline_layout == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.ps_set_layout);
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
                const uint32_t input_rate = (instance_mask & (1u << stream)) != 0 ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                                                  : VK_VERTEX_INPUT_RATE_VERTEX;
                bindings.push_back({.binding = stream, .stride = stride_it->second, .input_rate = input_rate});
            }
            attributes.reserve(real_decl->attributes.size());
            for (const auto& attr : real_decl->attributes)
            {
                if (attr.binding >= 32 || (usable_mask & (1u << attr.binding)) == 0)
                {
                    continue; // references a binding that didn't make it into `bindings` above
                }
                attributes.push_back(
                    {.location = attr.location, .binding = attr.binding, .format = attr.vk_format, .offset = attr.offset});
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
            bindings.push_back(
                {.binding = 0, .stride = textured_layout ? 20u : 16u, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX});
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
            device, /*render_pass=*/0, entry.pipeline_layout, entry.vs_module, entry.fs_module, width, height, bindings,
            attributes, key.depth, color_formats, depth_format, /*stencil_format=*/0, /*rasterization_samples=*/1,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, /*primitive_restart_enable=*/0, dynamic_states, empty_spec, empty_spec,
            blend, entry.pipeline);
        if (result != 0 || entry.pipeline == 0)
        {
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_pipeline_layout(device, entry.pipeline_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.ps_set_layout);
            return nullptr;
        }

        // Per-pipeline descriptor pool + its two sets, allocated ONCE here on cache miss rather than
        // reset/reallocated every draw from a shared pool. maxSets=2, 6 UBOs (VS+PS float/int/bool const
        // registers) and max_vs_sampler_stages + max_ps_sampler_stages combined-image-samplers (both the
        // VS set's and the PS set's bindings 1/4/5/6 -- the pool must cover the samplers declared across
        // BOTH allocated sets, not just one). execute_draw rewrites the sets' contents each draw but
        // reuses these same set objects -- safe because each draw blocks on a fence before returning (no
        // in-flight GPU read of a set about to be rewritten).
        const std::array<vulkan_host::descriptor_pool_size, 2> pool_sizes{{
            {.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 6},
            {.descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .descriptor_count = max_vs_sampler_stages + max_ps_sampler_stages},
        }};
        if (this->vulkan_.create_descriptor_pool(device, 2, pool_sizes, entry.descriptor_pool) != 0 ||
            entry.descriptor_pool == 0)
        {
            this->vulkan_.destroy_pipeline(device, entry.pipeline);
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_pipeline_layout(device, entry.pipeline_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.ps_set_layout);
            return nullptr;
        }
        std::array<uint64_t, 2> descriptor_sets{};
        uint32_t set_count = 0;
        if (this->vulkan_.allocate_descriptor_sets(device, entry.descriptor_pool, set_layouts, descriptor_sets, set_count) !=
                0 ||
            set_count != descriptor_sets.size())
        {
            this->vulkan_.destroy_descriptor_pool(device, entry.descriptor_pool);
            this->vulkan_.destroy_pipeline(device, entry.pipeline);
            this->vulkan_.destroy_shader_module(device, entry.vs_module);
            this->vulkan_.destroy_shader_module(device, entry.fs_module);
            this->vulkan_.destroy_pipeline_layout(device, entry.pipeline_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.vs_set_layout);
            this->vulkan_.destroy_descriptor_set_layout(device, entry.ps_set_layout);
            return nullptr;
        }
        entry.vs_descriptor_set = descriptor_sets[0];
        entry.ps_descriptor_set = descriptor_sets[1];

        // The set layouts and pipeline layout survive here (unlike the old destroy-after-use pattern)
        // so execute_draw's cmd_bind_descriptor_sets has stable ids to bind into on every draw.
        return &this->programmable_pipelines_.emplace(key, entry).first->second;
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
            case VK_FORMAT_R8G8_SNORM:
                return static_cast<size_t>(width) * height * 2;
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
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
                const uint16_t packed =
                    static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
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
            default:
                return false;
            }
        }

        // Ensures `pool` holds a host-visible buffer of at least `size` bytes with `usage`, then uploads
        // `size` bytes from `data` into it. If the pool's existing buffer is already large enough, it's
        // reused in place (just a mapped re-upload); otherwise any existing buffer/memory is destroyed and
        // a new one of exactly `size` bytes is created. Pooled buffers persist across draws -- reuse is
        // safe because execute_draw blocks on a fence before returning, so a prior draw's GPU read of this
        // buffer has completed before a later draw rewrites it. On failure the pool is left empty (any
        // partially created buffer freed) so the next draw retries cleanly. Replaces the former
        // create_and_upload_gpu_buffer, which created and freed a fresh buffer every draw.
        // Known, accepted tradeoff: growth is exact-fit (no headroom), and a later draw whose `size` is
        // SMALLER than the pool's already-grown capacity only overwrites its own `size` bytes -- any
        // bytes beyond that (but still within capacity) retain the PRIOR draw's content. This is benign
        // for VB/IB (a correct draw only ever reads its own resource's real backing size, never past it)
        // and irrelevant for UBOs (upload_pooled_ubo below always re-uploads the full fixed size).
        bool ensure_pooled_buffer(vulkan_host& vulkan, const uint64_t device, const uint64_t physical_device,
                                  pooled_buffer& pool, const void* data, const size_t size, const uint32_t usage)
        {
            if (pool.buffer == 0 || pool.capacity < size)
            {
                if (pool.buffer != 0)
                {
                    vulkan.destroy_buffer(device, pool.buffer);
                    vulkan.free_memory(device, pool.memory);
                    pool = {};
                }
                if (vulkan.create_buffer(device, size, usage, pool.buffer) != 0 || pool.buffer == 0)
                {
                    pool = {};
                    return false;
                }
                uint64_t mem_size = 0;
                uint64_t mem_align = 0;
                uint32_t mem_type_bits = 0;
                vulkan.get_buffer_memory_requirements(device, pool.buffer, mem_size, mem_align, mem_type_bits);
                const uint32_t memory_type = find_memory_type_index(
                    vulkan, physical_device, mem_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                if (memory_type == UINT32_MAX || vulkan.allocate_memory(device, mem_size, memory_type, pool.memory) != 0 ||
                    pool.memory == 0)
                {
                    vulkan.destroy_buffer(device, pool.buffer);
                    pool = {};
                    return false;
                }
                vulkan.bind_buffer_memory(device, pool.buffer, pool.memory, 0);
                pool.capacity = size;
            }
            vulkan.upload_memory(device, pool.memory, 0, size, data, size);
            return true;
        }

        // Re-uploads `data` (zero-padded to `size`) into `pool`'s UBO, growing/creating it if needed via
        // ensure_pooled_buffer. `size` is the fixed D3D9 constant-register cap (float: VS=4096B/256 float4
        // regs, PS=512B/32 float4 regs; int/bool: 256B/16 regs, both stages) regardless of how many
        // registers the app actually set, matching real D3D9 semantics where unset registers read as 0 --
        // the full zero-padded buffer is re-uploaded every draw so no stale tail can survive across draws.
        // Since the caps never change, the pool creates its buffer once and reuses it forever after.
        // Generic over the constant-register element type (float for c#, int32_t for i#, uint32_t for b#).
        template <typename T>
        bool upload_pooled_ubo(vulkan_host& vulkan, const uint64_t device, const uint64_t physical_device, pooled_buffer& pool,
                               const size_t size, const std::vector<T>& data)
        {
            std::vector<std::byte> staging(size, std::byte{0});
            const size_t bytes = std::min(data.size() * sizeof(T), size);
            if (bytes > 0)
            {
                std::memcpy(staging.data(), data.data(), bytes);
            }
            return ensure_pooled_buffer(vulkan, device, physical_device, pool, staging.data(), size,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
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
            case 2: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case 3: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case 4: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case 5: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            default: return VK_SAMPLER_ADDRESS_MODE_REPEAT; // WRAP (1) and any unrecognized value
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

    bool d3d9_host::build_sampler(const uint64_t device, const uint32_t sampler_index, const uint32_t mip_levels,
                                  uint64_t& out_sampler)
    {
        out_sampler = 0;
        const auto& ss = this->state_.sampler_state;
        // Defaults match real D3D9's own documented per-D3DSAMPLERSTATETYPE defaults (D3DSAMP_MAGFILTER/
        // MINFILTER default to D3DTEXF_POINT, MIPFILTER to D3DTEXF_NONE, ADDRESSU/V/W to D3DTADDRESS_WRAP,
        // MAXANISOTROPY to 1), confirmed live in this task's own default-init capture (HANDOFF_MACBOOK.md).
        const uint32_t mag_filter = d3d9_filter_to_vk_filter(sampler_state_or(ss, sampler_index, d3dsamp_magfilter, 1));
        const uint32_t min_filter = d3d9_filter_to_vk_filter(sampler_state_or(ss, sampler_index, d3dsamp_minfilter, 1));
        const uint32_t mipmap_mode =
            d3d9_mip_filter_to_vk_mipmap_mode(sampler_state_or(ss, sampler_index, d3dsamp_mipfilter, 0));
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
        const sampler_cache_key key{mag_filter,       min_filter, mipmap_mode, address_u,
                                    address_v,        address_w,  anisotropy_enable,
                                    static_cast<float>(max_anisotropy), min_lod, max_lod};
        if (const auto it = this->sampler_cache_.find(key); it != this->sampler_cache_.end())
        {
            out_sampler = it->second;
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
                                         /*mip_lod_bias=*/0.0f, static_cast<float>(max_anisotropy), min_lod, max_lod,
                                         out_sampler) != 0 ||
            out_sampler == 0)
        {
            out_sampler = 0;
            return false;
        }

        this->sampler_cache_.emplace(key, out_sampler);
        return true;
    }

    bool d3d9_host::ensure_depth_stencil_view(const uint64_t device, resource_entry& ds_entry, const uint32_t depth_format)
    {
        if (ds_entry.vk_image_view_id != 0)
        {
            return true;
        }

        const uint32_t depth_aspect = depth_aspect_mask(depth_format);
        if (this->vulkan_.create_image_view(device, ds_entry.vk_image_id, depth_format, depth_aspect, VK_IMAGE_VIEW_TYPE_2D, 0,
                                            1, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                            ds_entry.vk_image_view_id) != 0 ||
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
        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, ds_entry.vk_image_id, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth_range);
        this->vulkan_.cmd_clear_depth_stencil_image(this->command_buffer_, ds_entry.vk_image_id,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1.0f, 0, depth_range);
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

    int32_t d3d9_host::execute_draw(const uint32_t vertex_count, const uint32_t first_vertex, const indexed_draw* const indexed)
    {
        const auto rt_it = this->resources_.find(this->state_.render_targets[0]);
        if (rt_it == this->resources_.end() || rt_it->second.vk_image_id == 0)
        {
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
        };
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
            rt_slots[slot] = {&bound_it->second, bound_vk_format};
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
                return d3d_ok; // no real vertex data bound
            }
        }

        const resource_entry* ib_entry = nullptr;
        const std::vector<std::byte>* ib_um_bytes = nullptr;
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
                    return d3d_ok; // no real index data bound
                }
                ib_entry = &ib_it->second;
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
                return d3d_ok; // translation/pipeline failure; degrade silently
            }
        }
        else if (!this->ensure_pipeline(color_formats, rt.width, rt.height, depth_vk_format))
        {
            return d3d_ok;
        }

        for (const auto& brt : bound_rts)
        {
            if (brt.entry == nullptr || brt.entry->vk_image_view_id != 0)
            {
                continue;
            }
            if (this->vulkan_.create_image_view(device, brt.entry->vk_image_id, brt.vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY, brt.entry->vk_image_view_id) != 0 ||
                brt.entry->vk_image_view_id == 0)
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

        // Upload the current contents of every referenced stream into its per-stream pooled vertex buffer
        // (stream_buffer_pool_[stream]) -- reused/regrown across draws rather than reallocated every draw.
        // Streams the declaration doesn't reference, or that lack a real stride (both already filtered out
        // of used_binding_mask above), or a referenced+usable stream the app never called SetStreamSource
        // for, are left at buffer id 0 -- cmd_bind_vertex_buffers already maps that to VK_NULL_HANDLE.
        std::vector<uint64_t> stream_buffers(highest_binding + 1, 0);
        std::vector<uint64_t> stream_bind_offsets(highest_binding + 1, 0);
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
                src_bytes = &res_it->second.backing;
            }
            if (!ensure_pooled_buffer(this->vulkan_, device, this->vk_physical_device_, this->stream_buffer_pool_[stream],
                                      src_bytes->data(), src_bytes->size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
            {
                return d3d_ok;
            }
            stream_buffers[stream] = this->stream_buffer_pool_[stream].buffer;
            const auto off_it = this->state_.stream_offsets.find(stream);
            stream_bind_offsets[stream] = off_it != this->state_.stream_offsets.end() ? off_it->second : 0;
        }

        uint64_t index_buffer_vk = 0;
        // UM-backed (DrawIndexedPrimitiveUP) inline index bytes take precedence over a resource-backed
        // index buffer; both upload identically into the pooled index buffer.
        const std::vector<std::byte>* ib_bytes =
            ib_um_bytes != nullptr ? ib_um_bytes : (ib_entry != nullptr ? &ib_entry->backing : nullptr);
        if (ib_bytes != nullptr)
        {
            if (!ensure_pooled_buffer(this->vulkan_, device, this->vk_physical_device_, this->index_buffer_pool_,
                                      ib_bytes->data(), ib_bytes->size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
            {
                return d3d_ok;
            }
            index_buffer_vk = this->index_buffer_pool_.buffer;
        }

        // D3D9 SM2/3 float constant-register caps (MaxVertexShaderConst = 256, fill_d3d9caps).
        constexpr size_t vs_ubo_size = 256 * 4 * sizeof(float);
        constexpr size_t ps_ubo_size = 32 * 4 * sizeof(float);
        // D3D9 SM3 int/bool constant-register caps (16 registers each, both stages -- fill_d3d9caps),
        // each register expanded to a 16-byte slot (see vs/ps_const_i/b's own comments in d3d9_host.hpp).
        constexpr size_t int_bool_ubo_size = 16 * 4 * sizeof(uint32_t);
        // Indices into ubo_pool_ (d3d9_host.hpp). Order is fixed -- the descriptor writes below read each
        // pool by these names.
        enum ubo_index : size_t
        {
            ubo_vs_f = 0,
            ubo_ps_f = 1,
            ubo_vs_i = 2,
            ubo_ps_i = 3,
            ubo_vs_b = 4,
            ubo_ps_b = 5,
        };
        std::array<uint64_t, max_ps_sampler_stages> tex_samplers{};
        std::array<uint64_t, max_ps_sampler_stages> tex_image_views{};
        // Separate from the PS arrays above so both stages' per-draw sampler/view selection is tracked
        // independently (a vertex texture and a pixel texture can be bound to the same shader-register
        // index k at once -- they live in different descriptor sets and different bound_textures keys).
        std::array<uint64_t, max_vs_sampler_stages> vs_tex_samplers{};
        std::array<uint64_t, max_vs_sampler_stages> vs_tex_image_views{};
        std::array<uint64_t, 2> descriptor_sets{};
        if (use_programmable)
        {
            // Re-upload the six constant buffers into their persistent pools (created once, on the first
            // programmable draw, and reused every draw after -- their sizes are fixed caps that never
            // change). Contents genuinely change per draw, so the full zero-padded buffer is rewritten
            // each time; the pool objects are not reallocated.
            if (!upload_pooled_ubo(this->vulkan_, device, this->vk_physical_device_, this->ubo_pool_[ubo_vs_f], vs_ubo_size,
                                   this->state_.vs_const_f) ||
                !upload_pooled_ubo(this->vulkan_, device, this->vk_physical_device_, this->ubo_pool_[ubo_ps_f], ps_ubo_size,
                                   this->state_.ps_const_f) ||
                !upload_pooled_ubo(this->vulkan_, device, this->vk_physical_device_, this->ubo_pool_[ubo_vs_i], int_bool_ubo_size,
                                   this->state_.vs_const_i) ||
                !upload_pooled_ubo(this->vulkan_, device, this->vk_physical_device_, this->ubo_pool_[ubo_ps_i], int_bool_ubo_size,
                                   this->state_.ps_const_i) ||
                !upload_pooled_ubo(this->vulkan_, device, this->vk_physical_device_, this->ubo_pool_[ubo_vs_b], int_bool_ubo_size,
                                   this->state_.vs_const_b) ||
                !upload_pooled_ubo(this->vulkan_, device, this->vk_physical_device_, this->ubo_pool_[ubo_ps_b], int_bool_ubo_size,
                                   this->state_.ps_const_b))
            {
                return d3d_ok;
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
                if (tex_it == this->state_.bound_textures.end() || tex_it->second == 0 ||
                    !this->ensure_texture_uploaded(tex_it->second))
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
                        // Sampling view spans the texture's full mip chain (levelCount = mip_levels) so the
                        // sampler can select any level; single-mip textures still get levelCount 1.
                        const uint32_t view_levels = std::max(1u, tex.mip_levels);
                        this->vulkan_.create_image_view(device, tex.vk_image_id, tex_vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                        VK_IMAGE_VIEW_TYPE_2D, 0, view_levels, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY, tex.vk_image_view_id);
                    }
                }
                if (tex.vk_image_view_id != 0 && this->build_sampler(device, stage, std::max(1u, tex.mip_levels), tex_samplers[stage]))
                {
                    tex_image_views[stage] = tex.vk_image_view_id;
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
                if (tex_it == this->state_.bound_textures.end() || tex_it->second == 0 ||
                    !this->ensure_texture_uploaded(tex_it->second))
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
                        this->vulkan_.create_image_view(device, tex.vk_image_id, tex_vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                        VK_IMAGE_VIEW_TYPE_2D, 0, view_levels, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY, tex.vk_image_view_id);
                    }
                }
                if (tex.vk_image_view_id != 0 &&
                    this->build_sampler(device, vs_stage, std::max(1u, tex.mip_levels), vs_tex_samplers[k]))
                {
                    vs_tex_image_views[k] = tex.vk_image_view_id;
                }
            }

            // Reuse the pipeline entry's cached descriptor sets (allocated once in
            // ensure_programmable_pipeline) rather than resetting/reallocating a shared pool every draw.
            // Only the CONTENTS are rewritten below via update_descriptor_sets, since the bound
            // UBOs/textures/samplers can differ draw-to-draw even for the same pipeline.
            descriptor_sets = {programmable->vs_descriptor_set, programmable->ps_descriptor_set};

            std::vector<vulkan_host::descriptor_write> writes{
                {.dst_set = descriptor_sets[0],
                 .dst_binding = 0,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = this->ubo_pool_[ubo_vs_f].buffer,
                 .offset = 0,
                 .range = vs_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = descriptor_sets[1],
                 .dst_binding = 0,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = this->ubo_pool_[ubo_ps_f].buffer,
                 .offset = 0,
                 .range = ps_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = descriptor_sets[0],
                 .dst_binding = 2,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = this->ubo_pool_[ubo_vs_i].buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = descriptor_sets[0],
                 .dst_binding = 3,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = this->ubo_pool_[ubo_vs_b].buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = descriptor_sets[1],
                 .dst_binding = 2,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = this->ubo_pool_[ubo_ps_i].buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = descriptor_sets[1],
                 .dst_binding = 3,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = this->ubo_pool_[ubo_ps_b].buffer,
                 .offset = 0,
                 .range = int_bool_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
            };
            for (uint32_t stage = 0; stage < max_ps_sampler_stages; ++stage)
            {
                if (tex_image_views[stage] == 0 || tex_samplers[stage] == 0)
                {
                    continue;
                }
                writes.push_back({.dst_set = descriptor_sets[1],
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
                writes.push_back({.dst_set = descriptor_sets[0],
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
            this->vulkan_.update_descriptor_sets(device, writes);
        }

        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);

        // Assumes Clear always runs before the first Draw (true for this test's flow), which leaves
        // the image in TRANSFER_SRC_OPTIMAL (submit_clear's own documented post-state) -- transition to
        // COLOR_ATTACHMENT_OPTIMAL for rendering, then back for the readback below.
        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        for (const auto& brt : bound_rts)
        {
            if (brt.entry == nullptr)
            {
                continue; // gap slot -- no real image to transition
            }
            this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, brt.entry->vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, color_range);
        }

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
                .image_view = brt.entry->vk_image_view_id,
                .resolve_image_view = 0,
                .image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolve_image_layout = 0,
                .resolve_mode = 0,
                .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
                .store_op = VK_ATTACHMENT_STORE_OP_STORE,
            });
        }
        // The one-time init above already left the depth image in DEPTH_STENCIL_ATTACHMENT_OPTIMAL, and
        // every later draw finds it already there (LOAD_OP_LOAD/STORE_OP_STORE keep it there across
        // draws) -- no barrier needed here, unlike the color attachment's transfer-src round trip.
        const vulkan_host::rendering_attachment depth_attachment{
            .image_view = ds_entry != nullptr ? ds_entry->vk_image_view_id : 0,
            .resolve_image_view = 0,
            .image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolve_image_layout = 0,
            .resolve_mode = 0,
            .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
            .store_op = VK_ATTACHMENT_STORE_OP_STORE,
        };
        this->vulkan_.cmd_begin_rendering(this->command_buffer_, 0, 0, rt.width, rt.height, 1, 0, 0, color_attachments,
                                          ds_entry != nullptr ? &depth_attachment : nullptr, nullptr);

        this->vulkan_.cmd_bind_pipeline(this->command_buffer_, use_programmable ? programmable->pipeline : this->pipeline_,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS);

        if (use_programmable)
        {
            this->vulkan_.cmd_bind_descriptor_sets(this->command_buffer_, programmable->pipeline_layout, 0, descriptor_sets,
                                                   VK_PIPELINE_BIND_POINT_GRAPHICS, {});
        }

        const std::array<vulkan_host::viewport_entry, 1> viewports{
            {{.x = 0, .y = 0, .width = static_cast<float>(rt.width), .height = static_cast<float>(rt.height), .min_depth = 0.0f,
              .max_depth = 1.0f}}};
        this->vulkan_.cmd_set_viewport(this->command_buffer_, 0, false, viewports);
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
        this->vulkan_.cmd_set_scissor(this->command_buffer_, 0, false, scissors);

        if (!use_programmable)
        {
            const std::array<float, 2> viewport_size{static_cast<float>(rt.width), static_cast<float>(rt.height)};
            this->vulkan_.cmd_push_constants(this->command_buffer_, this->pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                             sizeof(viewport_size), viewport_size.data());
        }

        this->vulkan_.cmd_bind_vertex_buffers(this->command_buffer_, 0, static_cast<uint32_t>(stream_buffers.size()),
                                              stream_buffers.data(), stream_bind_offsets.data());
        // D3D9 hardware instancing carries no explicit instance-count draw parameter: the count is the low
        // 30 bits of the D3DSTREAMSOURCE_INDEXEDDATA stream's SetStreamSourceFreq divider (default 1, i.e.
        // an ordinary single-instance draw). Same helper the pipeline's per-binding inputRate was built
        // from, so the fetched instance data matches the instance count issued here.
        const uint32_t instance_count = this->resolve_instancing().instance_count;
        if (indexed != nullptr)
        {
            const uint32_t index_type = indexed->index_format != 0 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            this->vulkan_.cmd_bind_index_buffer(this->command_buffer_, index_buffer_vk, 0, index_type);
            this->vulkan_.cmd_draw_indexed(this->command_buffer_, vertex_count, instance_count, indexed->first_index,
                                           indexed->base_vertex_index, 0);
        }
        else
        {
            // Real D3D9 hardware instancing requires an indexed draw (see the comment above), so
            // instance_count is 1 here in every valid usage; passed through anyway for uniformity rather
            // than special-casing the non-indexed path back to a literal 1.
            this->vulkan_.cmd_draw(this->command_buffer_, vertex_count, instance_count, first_vertex, 0);
        }

        this->vulkan_.cmd_end_rendering(this->command_buffer_);

        for (const auto& brt : bound_rts)
        {
            if (brt.entry == nullptr)
            {
                continue; // gap slot -- no real image to transition
            }
            this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, brt.entry->vk_image_id, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                               VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);
        }

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);

        this->vulkan_.wait_for_fence(this->fence_, UINT64_MAX);

        // Nothing to free here: vertex/index/constant buffers are pooled (retained in
        // stream_buffer_pool_/index_buffer_pool_/ubo_pool_ for reuse next draw), and samplers are now
        // content-cached in sampler_cache_ (an immutable VkSampler reused by any later draw with the same
        // state), both retained for the device's lifetime.

        // Mark each bound render target's backing store stale; sync_backing_from_gpu reads it back
        // lazily on the next pfnLock/Present that actually needs the pixels.
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
        // real GPU backing (see the class comment); other texture kinds still get a plain host-side
        // shadow, no GPU backing yet.
        const bool is_buffer = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) ||
                               kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer);
        const bool is_render_target = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) &&
                                      (usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0;
        // Plain sampled texture_2d: real GPU backing with a real mip chain (see below, extra_mips);
        // cube/volume are still M3. Unrecognized formats fall through with no GPU image, matching this
        // function's existing "unrecognized -> no backing" behavior rather than crashing.
        const bool is_texture = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) && !is_render_target;
        uint32_t texture_vk_format = 0;
        const bool texture_format_ok = is_texture && d3d9_format_to_vulkan(format, texture_vk_format);
        const size_t texture_backing_size = texture_format_ok ? vk_texture_data_size(texture_vk_format, width, height) : 0;

        // A render target's host-side shadow (backing) must be sized at its real per-format stride, not a
        // hardcoded 4 bytes/texel BGRA8, so a non-BGRA8 RT (R5G6B5, A16B16G16R16F) reads back the correct
        // tight byte count via sync_backing_from_gpu. Depth-stencil RTs map to a depth VkFormat here too;
        // their backing is never locked, so vk_format_bytes_per_texel's upper-bound size is harmless.
        uint32_t render_target_vk_format = 0;
        const size_t render_target_backing_size =
            is_render_target && d3d9_format_to_vulkan(format, render_target_vk_format)
                ? static_cast<size_t>(width) * height * vk_format_bytes_per_texel(render_target_vk_format)
                : 0;

        // A sampled texture can carry a real mip chain (mip_levels levels). Level 0 lives in `backing`
        // (below); levels 1..N-1 each get their own byte vector in `extra_mips`, sized for that level's
        // halved dimensions (min 1) -- a flat single vector can't hold them since each level differs in
        // size. Non-texture resources and single-mip textures leave extra_mips empty.
        const uint32_t texture_mip_levels = std::max(1u, mip_levels);
        std::vector<std::vector<std::byte>> extra_mips;
        if (texture_format_ok)
        {
            for (uint32_t level = 1; level < texture_mip_levels; ++level)
            {
                const uint32_t level_width = std::max(1u, width >> level);
                const uint32_t level_height = std::max(1u, height >> level);
                extra_mips.emplace_back(vk_texture_data_size(texture_vk_format, level_width, level_height));
            }
        }

        const size_t backing_size = is_buffer ? width
                                    : is_render_target ? render_target_backing_size
                                                        : texture_backing_size;

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

        if (is_render_target)
        {
            const uint64_t device = this->ensure_vk_device();
            if (device != 0)
            {
                uint64_t vk_image = 0;
                if (this->vulkan_.create_render_target(device, width, height, format, vk_image) == 0 && vk_image != 0)
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
                if (this->vulkan_.create_image(device, texture_vk_format, width, height, sampled_usage, VK_IMAGE_TILING_OPTIMAL,
                                               /*samples=*/1, VK_IMAGE_TYPE_2D, /*depth=*/1, texture_mip_levels, /*array_layers=*/1,
                                               /*flags=*/0, vk_image) == 0 &&
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
        return d3d_ok;
    }

    bool d3d9_host::ensure_texture_uploaded(const uint64_t resource)
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return false;
        }
        resource_entry& tex = it->second;
        if (tex.vk_image_id == 0 || tex.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) ||
            (tex.usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0)
        {
            return false; // not a plain sampled texture with real GPU backing
        }

        uint32_t vk_format = 0;
        if (!d3d9_format_to_vulkan(tex.format, vk_format))
        {
            return false;
        }

        // Gather every mip level's tightly-packed size and source backing. Level 0 lives in `backing`,
        // levels 1..N-1 in `extra_mips` (see create_resource) -- subresource_backing() abstracts that.
        // Each level is concatenated into one staging buffer at its own offset. Every level's backing is
        // pre-sized to its exact tight size at create_resource time, so `src.size() < level_size` below
        // is really just a degenerate-format guard (level_size == 0), not a "was this level ever locked"
        // check -- an app that creates N mip levels but only ever writes level 0 still uploads levels
        // 1..N-1, as their zero-initialized (black) backing. That's an intentional, safe default: no
        // bail, no partially-garbage image, just unwritten levels rendering black under minification
        // until the app writes them.
        const uint32_t mip_levels = std::max(1u, tex.mip_levels);
        struct level_upload
        {
            uint32_t width;
            uint32_t height;
            size_t size;
            uint64_t staging_offset;
            const std::byte* src;
        };
        std::vector<level_upload> levels;
        uint64_t total_size = 0;
        for (uint32_t level = 0; level < mip_levels; ++level)
        {
            const uint32_t level_width = std::max(1u, tex.width >> level);
            const uint32_t level_height = std::max(1u, tex.height >> level);
            const size_t level_size = vk_texture_data_size(vk_format, level_width, level_height);
            const std::vector<std::byte>& src = tex.subresource_backing(level);
            if (level_size == 0 || src.size() < level_size)
            {
                return false; // degenerate format/size only -- see the comment above this loop
            }
            levels.push_back({level_width, level_height, level_size, total_size, src.data()});
            total_size += level_size;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return false;
        }

        // No dirty tracking: this always re-uploads the full image, even if nothing changed since the
        // last call. Tracking staleness would mean setting a flag from unlock() (touching the
        // already-correct, resource-kind-agnostic Lock/Unlock path this task must leave alone), so this
        // is the deliberately simpler alternative -- correct in every case, just not the cheapest one.
        // execute_draw (the only caller so far) can add its own caching if re-uploading every draw turns
        // out to matter.
        uint64_t staging_buffer = 0;
        if (this->vulkan_.create_buffer(device, total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer) != 0 ||
            staging_buffer == 0)
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
        for (const auto& level : levels)
        {
            this->vulkan_.upload_memory(device, staging_memory, level.staging_offset, level.size, level.src, level.size);
        }

        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);

        // One barrier over the whole mip chain, then one buffer->image copy per level into its own mip.
        const vulkan_host::subresource_range range{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                   .base_mip_level = 0,
                                                   .level_count = mip_levels,
                                                   .base_array_layer = 0,
                                                   .layer_count = 1};
        // VK_IMAGE_LAYOUT_UNDEFINED is a valid old_layout regardless of the image's actual current
        // layout (Vulkan's "discard previous contents" transition) -- correct here since every upload
        // fully overwrites every level anyway, whether this is the first upload or a later re-upload.
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, tex.vk_image_id, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

        for (uint32_t level = 0; level < mip_levels; ++level)
        {
            const vulkan_host::buffer_image_copy_region region{
                .buffer_offset = levels[level].staging_offset,
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .image_offset_x = 0,
                .image_offset_y = 0,
                .image_offset_z = 0,
                .width = levels[level].width,
                .height = levels[level].height,
                .depth = 1,
                .mip_level = level,
                .base_array_layer = 0,
                .layer_count = 1,
                .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            };
            this->vulkan_.cmd_copy_buffer_to_image(this->command_buffer_, staging_buffer, tex.vk_image_id,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region);
        }

        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, tex.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                           VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);
        this->vulkan_.wait_for_fence(this->fence_, UINT64_MAX);

        this->vulkan_.destroy_buffer(device, staging_buffer);
        this->vulkan_.free_memory(device, staging_memory);
        return true;
    }

    void d3d9_host::destroy_resource(const uint64_t resource)
    {
        this->resources_.erase(resource);
    }

    int32_t d3d9_host::tex_blt(const uint64_t dst_resource, const uint64_t src_resource)
    {
        const auto dst_it = this->resources_.find(dst_resource);
        const auto src_it = this->resources_.find(src_resource);
        if (dst_it == this->resources_.end() || src_it == this->resources_.end())
        {
            return d3derr_invalidcall;
        }

        dst_it->second.backing = src_it->second.backing;
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

        out_pixels = it->second.backing;
        out_width = it->second.width;
        out_height = it->second.height;
        return true;
    }

    void d3d9_host::sync_backing_from_gpu(resource_entry& rt)
    {
        if (!rt.backing_dirty || rt.vk_image_id == 0)
        {
            return;
        }

        std::vector<std::byte> pixels;
        uint32_t readback_width = 0;
        uint32_t readback_height = 0;
        if (this->vulkan_.readback_render_target(rt.vk_image_id, pixels, readback_width, readback_height) == 0)
        {
            rt.backing = std::move(pixels);
            rt.backing_dirty = false;
        }
    }

    int32_t d3d9_host::color_fill(const uint64_t resource, const uint32_t /*subresource*/, const int32_t left, const int32_t top,
                                  const int32_t right, const int32_t bottom, const uint32_t color_argb)
    {
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
        if (this->vulkan_.create_buffer(device, required, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer) != 0 ||
            staging_buffer == 0)
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

        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);

        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // Resting layout is TRANSFER_SRC_OPTIMAL (submit_clear / execute_draw leave it there); go through
        // cmd_pipeline_barrier so render_targets[image].current_layout stays authoritative.
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, rt.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color_range);

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
        this->vulkan_.cmd_copy_buffer_to_image(this->command_buffer_, staging_buffer, rt.vk_image_id,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region);

        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, rt.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);
        this->vulkan_.wait_for_fence(this->fence_, UINT64_MAX);

        this->vulkan_.destroy_buffer(device, staging_buffer);
        this->vulkan_.free_memory(device, staging_memory);

        rt.backing_dirty = true;
        return d3d_ok;
    }

    int32_t d3d9_host::blt(const uint64_t dst_resource, const uint32_t /*dst_subresource*/, const int32_t dst_left,
                           const int32_t dst_top, const int32_t dst_right, const int32_t dst_bottom, const uint64_t src_resource,
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

        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);

        const vulkan_host::subresource_range color_range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // Both RTs rest in TRANSFER_SRC_OPTIMAL. The source is already blit-ready there; only the
        // destination needs a TRANSFER_DST_OPTIMAL round trip. Route through cmd_pipeline_barrier so
        // render_targets[dst].current_layout stays authoritative.
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, dst.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, color_range);

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
        this->vulkan_.cmd_blit_image(this->command_buffer_, src.vk_image_id, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.vk_image_id,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blit);

        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, dst.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);
        this->vulkan_.wait_for_fence(this->fence_, UINT64_MAX);

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
        return d3d_ok;
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
            auto& target = static_cast<gpu_bridge::command>(opcode) == gpu_bridge::command::d3d9_set_vs_const_f
                              ? this->state_.vs_const_f
                              : this->state_.ps_const_f;
            const size_t required = static_cast<size_t>(req.start_register) * 4 + float_count;
            if (target.size() < required)
            {
                target.resize(required);
            }
            std::memcpy(target.data() + static_cast<size_t>(req.start_register) * 4, payload + sizeof(req), bytes);
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
            auto& target = static_cast<gpu_bridge::command>(opcode) == gpu_bridge::command::d3d9_set_vs_const_i
                              ? this->state_.vs_const_i
                              : this->state_.ps_const_i;
            const size_t required = static_cast<size_t>(req.start_register) * 4 + int_count;
            if (target.size() < required)
            {
                target.resize(required);
            }
            std::memcpy(target.data() + static_cast<size_t>(req.start_register) * 4, payload + sizeof(req), bytes);
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
            auto& target = static_cast<gpu_bridge::command>(opcode) == gpu_bridge::command::d3d9_set_vs_const_b
                              ? this->state_.vs_const_b
                              : this->state_.ps_const_b;
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
            this->state_.render_targets[req.render_target_index] = req.surface;
            return d3d_ok;
        }
        case gpu_bridge::command::d3d9_set_depth_stencil: {
            d3d9_cmd::set_depth_stencil_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }
            this->state_.depth_stencil = req.surface;
            return d3d_ok;
        }
        // set_viewport still parse-validates (catching wire-format bugs early) and no-ops until
        // execute_draw's hardcoded full-render-target-extent viewport is replaced with real viewport
        // state; clear/draw are fully implemented (see class comment), and set_scissor below now feeds
        // execute_draw's D3DRS_SCISSORTESTENABLE-gated scissor.
        case gpu_bridge::command::d3d9_set_viewport: {
            d3d9_cmd::set_viewport_record req{};
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
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

            // Real GPU clear, marking every bound render target's backing store stale; sync_backing_from_gpu
            // reads it back lazily on the next pfnLock/Present -- see the class comment for why this
            // sidesteps needing to know how the real d3d9.dll gets pixels onto an actual window.
            // D3DCLEAR_TARGET clears ALL currently-bound render targets (D3D9's SetRenderTarget slots
            // 0-3) to the single supplied color, not just slot 0 -- same slot resolution as
            // execute_draw's rt_slots loop, just without needing a Vulkan format for a pipeline.
            if ((req.flags & 1) != 0 /* D3DCLEAR_TARGET */)
            {
                // D3DCOLOR is 0xAARRGGBB.
                const std::array<float, 4> color{
                    static_cast<float>((req.color_argb >> 16) & 0xFF) / 255.0f,
                    static_cast<float>((req.color_argb >> 8) & 0xFF) / 255.0f,
                    static_cast<float>(req.color_argb & 0xFF) / 255.0f,
                    static_cast<float>((req.color_argb >> 24) & 0xFF) / 255.0f,
                };
                for (const uint64_t rt_handle : this->state_.render_targets)
                {
                    const auto it = this->resources_.find(rt_handle);
                    if (it == this->resources_.end() || it->second.vk_image_id == 0)
                    {
                        continue;
                    }
                    this->vulkan_.submit_clear(it->second.vk_image_id, color.data());

                    it->second.backing_dirty = true;
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
                                       .base_vertex_index = req.base_vertex_index};
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
                             req.src_resource, req.src_subresource, req.src_left, req.src_top, req.src_right, req.src_bottom,
                             req.filter);
        }
        default:
            return d3derr_invalidcall;
        }
    }
} // namespace sogen
