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
        constexpr uint32_t d3dsamp_maxanisotropy = 10;

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
    } // namespace

    uint64_t d3d9_host::allocate_id()
    {
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

        const std::array<vulkan_host::descriptor_pool_size, 2> pool_sizes{{
            {.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 2},
            // One combined-image-sampler slot: the PS set's binding 1 (see ensure_programmable_pipeline),
            // for texture stage/sampler 0 -- this slice's minimum-viable single-texture binding.
            {.descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptor_count = 1},
        }};
        if (this->vulkan_.create_descriptor_pool(device, 2, pool_sizes, this->descriptor_pool_) != 0 ||
            this->descriptor_pool_ == 0)
        {
            return false;
        }

        this->draw_infra_ready_ = true;
        return true;
    }

    bool d3d9_host::ensure_pipeline(const uint32_t color_format, const uint32_t width, const uint32_t height)
    {
        if (this->pipeline_ready_)
        {
            return true;
        }
        const uint64_t device = this->ensure_vk_device();
        if (device == 0)
        {
            return false;
        }

        if (this->vulkan_.create_shader_module(device, k_ff_vertex_shader_spirv.data(),
                                               k_ff_vertex_shader_spirv.size() * sizeof(uint32_t), this->vs_module_) != 0 ||
            this->vs_module_ == 0)
        {
            return false;
        }
        if (this->vulkan_.create_shader_module(device, k_ff_fragment_shader_spirv.data(),
                                               k_ff_fragment_shader_spirv.size() * sizeof(uint32_t), this->fs_module_) != 0 ||
            this->fs_module_ == 0)
        {
            return false;
        }

        // One push-constant range (vec2 viewportSize) in the vertex stage, no descriptor sets -- this
        // minimal shader pair needs neither textures nor uniform buffers.
        if (this->vulkan_.create_pipeline_layout(device, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, {},
                                                 this->pipeline_layout_) != 0 ||
            this->pipeline_layout_ == 0)
        {
            return false;
        }

        // D3DFVF_XYZRHW|D3DFVF_DIFFUSE: 16-byte {x,y,z,rhw} position + 4-byte D3DCOLOR diffuse, stride 20.
        const std::array<vulkan_host::vertex_binding, 1> bindings{
            {{.binding = 0, .stride = 20, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX}}};
        const std::array<vulkan_host::vertex_attribute, 2> attributes{{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 0, .format = VK_FORMAT_B8G8R8A8_UNORM, .offset = 16},
        }};
        const std::array<uint32_t, 1> color_formats{color_format};
        const std::array<uint32_t, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const vulkan_host::depth_state depth{.test_enable = 0, .write_enable = 0, .compare_op = 0};
        const std::array<vulkan_host::color_blend_attachment, 1> blend{{{
            .blend_enable = 0,
            .src_color_blend_factor = 0,
            .dst_color_blend_factor = 0,
            .color_blend_op = 0,
            .src_alpha_blend_factor = 0,
            .dst_alpha_blend_factor = 0,
            .alpha_blend_op = 0,
            .color_write_mask = 0xF,
        }}};
        const vulkan_host::specialization empty_spec{};

        const int32_t result = this->vulkan_.create_graphics_pipeline(
            device, /*render_pass=*/0, this->pipeline_layout_, this->vs_module_, this->fs_module_, width, height, bindings,
            attributes, depth, color_formats, /*depth_format=*/0, /*stencil_format=*/0, /*rasterization_samples=*/1,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, /*primitive_restart_enable=*/0, dynamic_states, empty_spec, empty_spec, blend,
            this->pipeline_);
        if (result != 0 || this->pipeline_ == 0)
        {
            return false;
        }

        this->pipeline_ready_ = true;
        return true;
    }

    const d3d9_host::programmable_pipeline_entry* d3d9_host::ensure_programmable_pipeline(const uint32_t color_format,
                                                                                           const uint32_t width,
                                                                                           const uint32_t height)
    {
        const uint64_t key = (this->state_.vertex_shader << 32) | this->state_.pixel_shader;
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
        // at set 0 binding 0, PS float-const UBO at set 1 binding 0. The actual per-draw UBO creation
        // and descriptor-set binding happens in execute_draw, using descriptor_pool_.
        const std::array<vulkan_host::descriptor_binding, 1> vs_bindings{{
            {.binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
        }};
        // Binding 1 (combined image sampler, texture stage/sampler 0) is declared here unconditionally --
        // Vulkan allows a pipeline layout to declare more bindings than a given shader module statically
        // uses, so this is safe even before the shader translator (Task 7) emits a SPIR-V sampler that
        // references it. execute_draw only writes this descriptor when a texture is actually bound.
        const std::array<vulkan_host::descriptor_binding, 2> ps_bindings{{
            {.binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
            {.binding = 1, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT},
        }};
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

        // D3DFVF_XYZ|D3DFVF_DIFFUSE: 12-byte {x,y,z} object-space (pre-transform) position + 4-byte
        // D3DCOLOR diffuse, stride 16 -- this slice's one supported programmable vertex format
        // (position+color passthrough only, see the design spec's Non-Goals).
        const std::array<vulkan_host::vertex_binding, 1> bindings{
            {{.binding = 0, .stride = 16, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX}}};
        const std::array<vulkan_host::vertex_attribute, 2> attributes{{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 0, .format = VK_FORMAT_B8G8R8A8_UNORM, .offset = 12},
        }};
        const std::array<uint32_t, 1> color_formats{color_format};
        const std::array<uint32_t, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const vulkan_host::depth_state depth{.test_enable = 0, .write_enable = 0, .compare_op = 0};
        const std::array<vulkan_host::color_blend_attachment, 1> blend{{{
            .blend_enable = 0,
            .src_color_blend_factor = 0,
            .dst_color_blend_factor = 0,
            .color_blend_op = 0,
            .src_alpha_blend_factor = 0,
            .dst_alpha_blend_factor = 0,
            .alpha_blend_op = 0,
            .color_write_mask = 0xF,
        }}};
        const vulkan_host::specialization empty_spec{};

        const int32_t result = this->vulkan_.create_graphics_pipeline(
            device, /*render_pass=*/0, entry.pipeline_layout, entry.vs_module, entry.fs_module, width, height, bindings,
            attributes, depth, color_formats, /*depth_format=*/0, /*stencil_format=*/0, /*rasterization_samples=*/1,
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

        // Creates a host-visible buffer sized exactly for `data` and uploads it -- shared by
        // execute_draw's per-draw vertex and index buffer uploads (simplest correct model for a first
        // triangle; no persistent GPU buffer / dirty tracking yet, same as the rest of this file).
        bool create_and_upload_gpu_buffer(vulkan_host& vulkan, const uint64_t device, const uint64_t physical_device,
                                          const uint32_t usage, const std::vector<std::byte>& data, uint64_t& out_buffer,
                                          uint64_t& out_memory)
        {
            if (vulkan.create_buffer(device, data.size(), usage, out_buffer) != 0 || out_buffer == 0)
            {
                return false;
            }
            uint64_t mem_size = 0;
            uint64_t mem_align = 0;
            uint32_t mem_type_bits = 0;
            vulkan.get_buffer_memory_requirements(device, out_buffer, mem_size, mem_align, mem_type_bits);
            const uint32_t memory_type = find_memory_type_index(
                vulkan, physical_device, mem_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (memory_type == UINT32_MAX || vulkan.allocate_memory(device, mem_size, memory_type, out_memory) != 0 ||
                out_memory == 0)
            {
                vulkan.destroy_buffer(device, out_buffer);
                out_buffer = 0;
                return false;
            }
            vulkan.bind_buffer_memory(device, out_buffer, out_memory, 0);
            vulkan.upload_memory(device, out_memory, 0, data.size(), data.data(), data.size());
            return true;
        }

        // Creates a host-visible UBO of exactly `size` bytes and uploads `data` zero-padded to that
        // size -- the fixed D3D9 constant-register cap (VS=4096B/256 float4 regs, PS=512B/32 float4
        // regs) regardless of how many registers the app actually set, matching real D3D9 semantics
        // where unset registers read as 0. Mirrors execute_draw's own per-draw vertex-buffer upload.
        bool create_and_upload_ubo(vulkan_host& vulkan, const uint64_t device, const uint64_t physical_device, const size_t size,
                                   const std::vector<float>& data, uint64_t& out_buffer, uint64_t& out_memory)
        {
            if (vulkan.create_buffer(device, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, out_buffer) != 0 || out_buffer == 0)
            {
                return false;
            }
            uint64_t mem_size = 0;
            uint64_t mem_align = 0;
            uint32_t mem_type_bits = 0;
            vulkan.get_buffer_memory_requirements(device, out_buffer, mem_size, mem_align, mem_type_bits);
            const uint32_t memory_type = find_memory_type_index(
                vulkan, physical_device, mem_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (memory_type == UINT32_MAX || vulkan.allocate_memory(device, mem_size, memory_type, out_memory) != 0 ||
                out_memory == 0)
            {
                vulkan.destroy_buffer(device, out_buffer);
                out_buffer = 0;
                return false;
            }
            vulkan.bind_buffer_memory(device, out_buffer, out_memory, 0);

            std::vector<std::byte> staging(size, std::byte{0});
            const size_t bytes = std::min(data.size() * sizeof(float), size);
            if (bytes > 0)
            {
                std::memcpy(staging.data(), data.data(), bytes);
            }
            vulkan.upload_memory(device, out_memory, 0, size, staging.data(), size);
            return true;
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
    } // namespace

    bool d3d9_host::build_sampler(const uint64_t device, const uint32_t sampler_index, uint64_t& out_sampler) const
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

        // Every resource created by this slice has exactly one mip level (see create_resource), so LOD is
        // pinned to 0 regardless of the app's own MIPFILTER/MAXMIPLEVEL/MIPMAPLODBIAS state.
        //
        // D3DSAMP_BORDERCOLOR is captured into sampler_state (see the UMD's tss_key table) but never read
        // here -- border_color is hardcoded to transparent-black, matching D3D9's own default. vulkan_host
        // ::create_sampler only accepts discrete VkBorderColor buckets (transparent/opaque black/white; the
        // bridge doesn't forward VK_EXT_custom_border_color), so an arbitrary ARGB border color can't be
        // represented faithfully with the current wrapper regardless. Only matters once D3DTADDRESS_BORDER
        // is actually used with a non-default border color.
        return this->vulkan_.create_sampler(device, mag_filter, min_filter, address_u, address_v, address_w, mipmap_mode,
                                            /*compare_enable=*/0, /*compare_op=*/0,
                                            /*anisotropy_enable=*/max_anisotropy > 1 ? 1 : 0, /*border_color=*/0,
                                            /*mip_lod_bias=*/0.0f, static_cast<float>(max_anisotropy), /*min_lod=*/0.0f,
                                            /*max_lod=*/0.0f, out_sampler) == 0 &&
               out_sampler != 0;
    }

    int32_t d3d9_host::execute_draw(const uint32_t vertex_count, const uint32_t first_vertex, const indexed_draw* const indexed)
    {
        const auto rt_it = this->resources_.find(this->state_.render_targets[0]);
        if (rt_it == this->resources_.end() || rt_it->second.vk_image_id == 0)
        {
            return d3d_ok; // no bound render target with GPU backing -- nothing to draw into yet
        }
        auto& rt = rt_it->second;

        // Only trust a resource that was actually created as a vertex/index buffer. Runtime-assigned
        // DDI handles for D3DPOOL_DEFAULT vertex buffers (see class comment) are small sequential values
        // from a handle space independent of our own resource ids, and can coincidentally collide with
        // an unrelated resource id -- without this guard that collision would silently draw garbage from
        // whatever resource happens to share the number.
        const auto vb_it = this->resources_.find(this->state_.stream_sources[0]);
        if (vb_it == this->resources_.end() || vb_it->second.backing.empty() ||
            (vb_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) &&
             vb_it->second.kind != static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer)))
        {
            return d3d_ok; // no real vertex data bound
        }
        const auto& vb_backing = vb_it->second.backing;

        const resource_entry* ib_entry = nullptr;
        if (indexed != nullptr)
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

        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return d3d_ok; // GPU unavailable; degrade silently like the rest of this host does
        }

        const bool use_programmable = this->state_.vertex_shader != 0 && this->state_.pixel_shader != 0;
        const programmable_pipeline_entry* programmable = nullptr;
        if (use_programmable)
        {
            programmable = this->ensure_programmable_pipeline(VK_FORMAT_B8G8R8A8_UNORM, rt.width, rt.height);
            if (programmable == nullptr)
            {
                return d3d_ok; // translation/pipeline failure; degrade silently
            }
        }
        else if (!this->ensure_pipeline(VK_FORMAT_B8G8R8A8_UNORM, rt.width, rt.height))
        {
            return d3d_ok;
        }

        if (rt.vk_image_view_id == 0)
        {
            if (this->vulkan_.create_image_view(device, rt.vk_image_id, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
                                                VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY, rt.vk_image_view_id) != 0 ||
                rt.vk_image_view_id == 0)
            {
                return d3d_ok;
            }
        }

        // Upload the current vertex buffer contents fresh every draw -- simplest correct model for a
        // first triangle; no persistent GPU vertex buffer / dirty tracking yet.
        uint64_t vertex_buffer = 0;
        uint64_t vertex_memory = 0;
        if (!create_and_upload_gpu_buffer(this->vulkan_, device, this->vk_physical_device_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                          vb_backing, vertex_buffer, vertex_memory))
        {
            return d3d_ok;
        }

        uint64_t index_buffer_vk = 0;
        uint64_t index_memory = 0;
        if (ib_entry != nullptr &&
            !create_and_upload_gpu_buffer(this->vulkan_, device, this->vk_physical_device_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                          ib_entry->backing, index_buffer_vk, index_memory))
        {
            this->vulkan_.destroy_buffer(device, vertex_buffer);
            this->vulkan_.free_memory(device, vertex_memory);
            return d3d_ok;
        }

        // D3D9 SM2/3 float constant-register caps (MaxVertexShaderConst = 256, fill_d3d9caps).
        constexpr size_t vs_ubo_size = 256 * 4 * sizeof(float);
        constexpr size_t ps_ubo_size = 32 * 4 * sizeof(float);
        uint64_t vs_ubo = 0;
        uint64_t vs_ubo_memory = 0;
        uint64_t ps_ubo = 0;
        uint64_t ps_ubo_memory = 0;
        uint64_t tex_sampler = 0;
        uint64_t tex_image_view = 0;
        std::array<uint64_t, 2> descriptor_sets{};
        if (use_programmable)
        {
            if (!create_and_upload_ubo(this->vulkan_, device, this->vk_physical_device_, vs_ubo_size, this->state_.vs_const_f,
                                       vs_ubo, vs_ubo_memory) ||
                !create_and_upload_ubo(this->vulkan_, device, this->vk_physical_device_, ps_ubo_size, this->state_.ps_const_f,
                                       ps_ubo, ps_ubo_memory))
            {
                this->vulkan_.destroy_buffer(device, vertex_buffer);
                this->vulkan_.free_memory(device, vertex_memory);
                if (index_buffer_vk != 0)
                {
                    this->vulkan_.destroy_buffer(device, index_buffer_vk);
                    this->vulkan_.free_memory(device, index_memory);
                }
                if (vs_ubo != 0)
                {
                    this->vulkan_.destroy_buffer(device, vs_ubo);
                    this->vulkan_.free_memory(device, vs_ubo_memory);
                }
                return d3d_ok;
            }

            // Combined-image-sampler binding for texture stage/sampler 0 (this slice's minimum-viable
            // single-texture scope -- see ensure_programmable_pipeline's ps_bindings comment). Only
            // written into the descriptor set when a real, GPU-backed texture is actually bound; Vulkan
            // permits an allocated descriptor set to leave a binding unwritten as long as no bound
            // pipeline's shader statically accesses it (still true until Task 7 wires the SPIR-V side).
            const auto tex_it = this->state_.bound_textures.find(0);
            if (tex_it != this->state_.bound_textures.end() && tex_it->second != 0 &&
                this->ensure_texture_uploaded(tex_it->second))
            {
                const auto tex_res_it = this->resources_.find(tex_it->second);
                if (tex_res_it != this->resources_.end())
                {
                    resource_entry& tex = tex_res_it->second;
                    if (tex.vk_image_view_id == 0)
                    {
                        uint32_t tex_vk_format = 0;
                        if (d3d9_format_to_vulkan(tex.format, tex_vk_format))
                        {
                            this->vulkan_.create_image_view(device, tex.vk_image_id, tex_vk_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                                            VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY, tex.vk_image_view_id);
                        }
                    }
                    if (tex.vk_image_view_id != 0 && this->build_sampler(device, 0, tex_sampler))
                    {
                        tex_image_view = tex.vk_image_view_id;
                    }
                }
            }

            this->vulkan_.reset_descriptor_pool(device, this->descriptor_pool_, 0);
            const std::array<uint64_t, 2> set_layouts{programmable->vs_set_layout, programmable->ps_set_layout};
            uint32_t set_count = 0;
            if (this->vulkan_.allocate_descriptor_sets(device, this->descriptor_pool_, set_layouts, descriptor_sets, set_count) !=
                    0 ||
                set_count != descriptor_sets.size())
            {
                this->vulkan_.destroy_buffer(device, vertex_buffer);
                this->vulkan_.free_memory(device, vertex_memory);
                if (index_buffer_vk != 0)
                {
                    this->vulkan_.destroy_buffer(device, index_buffer_vk);
                    this->vulkan_.free_memory(device, index_memory);
                }
                this->vulkan_.destroy_buffer(device, vs_ubo);
                this->vulkan_.free_memory(device, vs_ubo_memory);
                this->vulkan_.destroy_buffer(device, ps_ubo);
                this->vulkan_.free_memory(device, ps_ubo_memory);
                if (tex_sampler != 0)
                {
                    this->vulkan_.destroy_sampler(device, tex_sampler);
                }
                return d3d_ok;
            }

            std::vector<vulkan_host::descriptor_write> writes{
                {.dst_set = descriptor_sets[0],
                 .dst_binding = 0,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = vs_ubo,
                 .offset = 0,
                 .range = vs_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
                {.dst_set = descriptor_sets[1],
                 .dst_binding = 0,
                 .dst_array_element = 0,
                 .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .buffer = ps_ubo,
                 .offset = 0,
                 .range = ps_ubo_size,
                 .sampler = 0,
                 .image_view = 0,
                 .image_layout = 0},
            };
            if (tex_image_view != 0 && tex_sampler != 0)
            {
                writes.push_back({.dst_set = descriptor_sets[1],
                                  .dst_binding = 1,
                                  .dst_array_element = 0,
                                  .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .buffer = 0,
                                  .offset = 0,
                                  .range = 0,
                                  .sampler = tex_sampler,
                                  .image_view = tex_image_view,
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
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, rt.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, color_range);

        const vulkan_host::rendering_attachment color_attachment{
            .image_view = rt.vk_image_view_id,
            .resolve_image_view = 0,
            .image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolve_image_layout = 0,
            .resolve_mode = 0,
            .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
            .store_op = VK_ATTACHMENT_STORE_OP_STORE,
        };
        const std::array<vulkan_host::rendering_attachment, 1> color_attachments{color_attachment};
        this->vulkan_.cmd_begin_rendering(this->command_buffer_, 0, 0, rt.width, rt.height, 1, 0, 0, color_attachments, nullptr,
                                          nullptr);

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
        const std::array<vulkan_host::scissor_entry, 1> scissors{
            {{.offset_x = 0, .offset_y = 0, .width = rt.width, .height = rt.height}}};
        this->vulkan_.cmd_set_scissor(this->command_buffer_, 0, false, scissors);

        if (!use_programmable)
        {
            const std::array<float, 2> viewport_size{static_cast<float>(rt.width), static_cast<float>(rt.height)};
            this->vulkan_.cmd_push_constants(this->command_buffer_, this->pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                             sizeof(viewport_size), viewport_size.data());
        }

        const uint64_t vb_offset = 0;
        this->vulkan_.cmd_bind_vertex_buffers(this->command_buffer_, 0, 1, &vertex_buffer, &vb_offset);
        if (indexed != nullptr)
        {
            const uint32_t index_type = indexed->index_format != 0 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            this->vulkan_.cmd_bind_index_buffer(this->command_buffer_, index_buffer_vk, 0, index_type);
            this->vulkan_.cmd_draw_indexed(this->command_buffer_, vertex_count, 1, indexed->first_index,
                                           indexed->base_vertex_index, 0);
        }
        else
        {
            this->vulkan_.cmd_draw(this->command_buffer_, vertex_count, 1, first_vertex, 0);
        }

        this->vulkan_.cmd_end_rendering(this->command_buffer_);

        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, rt.vk_image_id, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_range);

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);

        while (this->vulkan_.get_fence_status(this->fence_) != 0 /*VK_SUCCESS*/)
        {
            // Synchronous wait, matching create_render_target's own submit_clear/readback pattern.
        }

        this->vulkan_.destroy_buffer(device, vertex_buffer);
        this->vulkan_.free_memory(device, vertex_memory);
        if (index_buffer_vk != 0)
        {
            this->vulkan_.destroy_buffer(device, index_buffer_vk);
            this->vulkan_.free_memory(device, index_memory);
        }
        if (use_programmable)
        {
            this->vulkan_.destroy_buffer(device, vs_ubo);
            this->vulkan_.free_memory(device, vs_ubo_memory);
            this->vulkan_.destroy_buffer(device, ps_ubo);
            this->vulkan_.free_memory(device, ps_ubo_memory);
            if (tex_sampler != 0)
            {
                this->vulkan_.destroy_sampler(device, tex_sampler);
            }
        }

        // Read the drawn frame back into the resource's backing store, same as pfnClear -- so pfnLock
        // sees the real drawn pixels.
        std::vector<std::byte> pixels;
        uint32_t readback_width = 0;
        uint32_t readback_height = 0;
        if (this->vulkan_.readback_render_target(rt.vk_image_id, pixels, readback_width, readback_height) == 0)
        {
            rt.backing = std::move(pixels);
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
        // shadow sized for one mip's worth, no GPU backing yet.
        const bool is_buffer = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::vertex_buffer) ||
                               kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::index_buffer);
        const bool is_render_target = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) &&
                                      (usage & (d3dusage_rendertarget | d3dusage_depthstencil)) != 0;
        // Plain sampled texture_2d: real GPU backing too, single mip/layer, 2D only (mip-mapping and
        // cube/volume are M3). Unrecognized formats fall through with no GPU image, matching this
        // function's existing "unrecognized -> no backing" behavior rather than crashing.
        const bool is_texture = kind == static_cast<uint32_t>(d3d9_cmd::resource_kind::texture_2d) && !is_render_target;
        uint32_t texture_vk_format = 0;
        const bool texture_format_ok = is_texture && d3d9_format_to_vulkan(format, texture_vk_format);
        const size_t texture_backing_size = texture_format_ok ? vk_texture_data_size(texture_vk_format, width, height) : 0;

        const size_t backing_size = is_buffer ? width
                                    : is_render_target ? static_cast<size_t>(width) * height * 4
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
                                               /*samples=*/1, VK_IMAGE_TYPE_2D, /*depth=*/1, /*mip_levels=*/1, /*array_layers=*/1,
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
        const size_t required = vk_texture_data_size(vk_format, tex.width, tex.height);
        if (required == 0 || tex.backing.size() < required)
        {
            return false; // no (or incomplete) pixel data written yet
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
        // Whichever draw path first calls this (Task 3/7) can add its own caching if re-uploading every
        // draw turns out to matter.
        uint64_t staging_buffer = 0;
        if (this->vulkan_.create_buffer(device, required, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer) != 0 ||
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
        this->vulkan_.upload_memory(device, staging_memory, 0, required, tex.backing.data(), required);

        this->vulkan_.reset_fence(device, this->fence_);
        this->vulkan_.begin_command_buffer(this->command_buffer_, 0, false, 0, {}, 0, 0, 1, 0);

        const vulkan_host::subresource_range range{
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1};
        // VK_IMAGE_LAYOUT_UNDEFINED is a valid old_layout regardless of the image's actual current
        // layout (Vulkan's "discard previous contents" transition) -- correct here since every upload
        // fully overwrites mip 0 anyway, whether this is the first upload or a later re-upload.
        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, tex.vk_image_id, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

        const vulkan_host::buffer_image_copy_region region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .width = tex.width,
            .height = tex.height,
            .depth = 1,
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        };
        this->vulkan_.cmd_copy_buffer_to_image(this->command_buffer_, staging_buffer, tex.vk_image_id,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region);

        this->vulkan_.cmd_pipeline_barrier(this->command_buffer_, tex.vk_image_id, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                           VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

        this->vulkan_.end_command_buffer(this->command_buffer_);
        this->vulkan_.queue_submit(this->queue_, this->command_buffer_, this->fence_);
        while (this->vulkan_.get_fence_status(this->fence_) != 0 /*VK_SUCCESS*/)
        {
            // Synchronous wait, matching execute_draw's own submit pattern.
        }

        this->vulkan_.destroy_buffer(device, staging_buffer);
        this->vulkan_.free_memory(device, staging_memory);
        return true;
    }

    void d3d9_host::destroy_resource(const uint64_t resource)
    {
        this->resources_.erase(resource);
    }

    bool d3d9_host::snapshot_resource(const uint64_t resource, std::vector<std::byte>& out_pixels, uint32_t& out_width,
                                      uint32_t& out_height) const
    {
        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end() || it->second.vk_image_id == 0 || it->second.backing.empty())
        {
            return false;
        }

        out_pixels = it->second.backing;
        out_width = it->second.width;
        out_height = it->second.height;
        return true;
    }

    int32_t d3d9_host::lock(const uint64_t resource, const uint32_t /*subresource*/, const uint32_t offset, const uint32_t size,
                            const uint32_t /*flags*/, void* out, const size_t out_capacity, uint32_t& out_data_size)
    {
        out_data_size = 0;

        const auto it = this->resources_.find(resource);
        if (it == this->resources_.end())
        {
            return d3derr_invalidcall;
        }

        auto& backing = it->second.backing;
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

    int32_t d3d9_host::unlock(const uint64_t resource, const uint32_t /*subresource*/, const uint32_t offset, const void* data,
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

        auto& backing = it->second.backing;
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
        // set_viewport / set_scissor / clear / draw_* are consumed once Part 3's pipeline builder and
        // vulkan_host integration land; for now they parse-validate (catching wire-format bugs early)
        // and no-op.
        case gpu_bridge::command::d3d9_set_viewport: {
            d3d9_cmd::set_viewport_record req{};
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
        }
        case gpu_bridge::command::d3d9_set_scissor: {
            d3d9_cmd::set_scissor_record req{};
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
        }
        case gpu_bridge::command::d3d9_clear: {
            d3d9_cmd::clear_record req{};
            if (!read_record(payload, size, req))
            {
                return d3derr_invalidcall;
            }

            // Real GPU clear + readback into the bound render target's backing store, so pfnLock
            // (already wired) hands the app real pixels -- see the class comment for why this
            // sidesteps needing to know how the real d3d9.dll gets pixels onto an actual window.
            const auto it = this->resources_.find(this->state_.render_targets[0]);
            if (it != this->resources_.end() && it->second.vk_image_id != 0 && (req.flags & 1) != 0 /* D3DCLEAR_TARGET */)
            {
                // D3DCOLOR is 0xAARRGGBB.
                const std::array<float, 4> color{
                    static_cast<float>((req.color_argb >> 16) & 0xFF) / 255.0f,
                    static_cast<float>((req.color_argb >> 8) & 0xFF) / 255.0f,
                    static_cast<float>(req.color_argb & 0xFF) / 255.0f,
                    static_cast<float>((req.color_argb >> 24) & 0xFF) / 255.0f,
                };
                this->vulkan_.submit_clear(it->second.vk_image_id, color.data());

                std::vector<std::byte> pixels;
                uint32_t readback_width = 0;
                uint32_t readback_height = 0;
                if (this->vulkan_.readback_render_target(it->second.vk_image_id, pixels, readback_width, readback_height) == 0)
                {
                    it->second.backing = std::move(pixels);
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
        case gpu_bridge::command::d3d9_draw_primitive_up: {
            d3d9_cmd::draw_primitive_up_record req{};
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
        }
        case gpu_bridge::command::d3d9_draw_indexed_primitive_up: {
            d3d9_cmd::draw_indexed_primitive_up_record req{};
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
        }
        default:
            return d3derr_invalidcall;
        }
    }
} // namespace sogen
