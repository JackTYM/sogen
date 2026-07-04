#include "d3d9_host.hpp"

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

        const std::array<vulkan_host::descriptor_pool_size, 1> pool_sizes{
            {{.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 2}}};
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
        // at set 0 binding 0, PS float-const UBO at set 1 binding 0. Only the SHAPE is wired up here --
        // no buffers are created/bound yet (that lands with the descriptor pool + per-draw binding).
        const std::array<vulkan_host::descriptor_binding, 1> vs_bindings{{
            {.binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
             .stage_flags = VK_SHADER_STAGE_VERTEX_BIT},
        }};
        const std::array<vulkan_host::descriptor_binding, 1> ps_bindings{{
            {.binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptor_count = 1,
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
        // so execute_draw's future cmd_bind_descriptor_sets has stable ids to bind into on every draw.
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
    } // namespace

    int32_t d3d9_host::execute_draw(const uint32_t vertex_count, const uint32_t first_vertex)
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
        if (this->vulkan_.create_buffer(device, vb_backing.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_buffer) != 0 ||
            vertex_buffer == 0)
        {
            return d3d_ok;
        }
        uint64_t vb_mem_size = 0;
        uint64_t vb_mem_align = 0;
        uint32_t vb_mem_type_bits = 0;
        this->vulkan_.get_buffer_memory_requirements(device, vertex_buffer, vb_mem_size, vb_mem_align, vb_mem_type_bits);
        const uint32_t memory_type = find_memory_type_index(this->vulkan_, this->vk_physical_device_, vb_mem_type_bits,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memory_type == UINT32_MAX)
        {
            this->vulkan_.destroy_buffer(device, vertex_buffer);
            return d3d_ok;
        }
        uint64_t vertex_memory = 0;
        if (this->vulkan_.allocate_memory(device, vb_mem_size, memory_type, vertex_memory) != 0 || vertex_memory == 0)
        {
            this->vulkan_.destroy_buffer(device, vertex_buffer);
            return d3d_ok;
        }
        this->vulkan_.bind_buffer_memory(device, vertex_buffer, vertex_memory, 0);
        this->vulkan_.upload_memory(device, vertex_memory, 0, vb_backing.size(), vb_backing.data(), vb_backing.size());

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
        this->vulkan_.cmd_draw(this->command_buffer_, vertex_count, 1, first_vertex, 0);

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
        const size_t backing_size = is_buffer ? width : (is_render_target ? static_cast<size_t>(width) * height * 4 : 0);

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

        const uint64_t id = this->allocate_id();
        this->resources_.emplace(id, std::move(entry));
        out_resource = id;
        return d3d_ok;
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
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
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
