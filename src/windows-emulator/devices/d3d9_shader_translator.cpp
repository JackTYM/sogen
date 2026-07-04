#include "d3d9_shader_translator.hpp"

#include <vkd3d_shader.h>

#include <cstring>

namespace sogen
{
    namespace
    {
        // out_output_or_input is a shallow copy aliasing info's heap-owned element array; info must stay
        // alive (not be freed via vkd3d_shader_free_scan_signature_info) until the signature is done being used.
        bool scan_signature(const void* tokens, const size_t token_size_bytes, vkd3d_shader_scan_signature_info& info,
                            vkd3d_shader_signature& out_output_or_input, const bool want_output)
        {
            info = vkd3d_shader_scan_signature_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_SIGNATURE_INFO};

            vkd3d_shader_compile_info compile_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
            compile_info.next = &info;
            compile_info.source.code = tokens;
            compile_info.source.size = token_size_bytes;
            compile_info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
            compile_info.log_level = VKD3D_SHADER_LOG_NONE;

            char* messages = nullptr;
            const int result = vkd3d_shader_scan(&compile_info, &messages);
            if (messages != nullptr)
            {
                vkd3d_shader_free_messages(messages);
            }
            if (result < 0)
            {
                return false;
            }
            out_output_or_input = want_output ? info.output : info.input;
            return true;
        }

        bool compile_stage(const void* tokens, const size_t token_size_bytes,
                           const vkd3d_shader_varying_map_info* varying_map_info,
                           const vkd3d_shader_visibility shader_visibility, const unsigned int descriptor_set,
                           const vkd3d_shader_combined_resource_sampler* combined_samplers,
                           const unsigned int combined_sampler_count, std::vector<uint32_t>& out_spirv)
        {
            vkd3d_shader_spirv_target_info spirv_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
            spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_0;
            spirv_info.next = varying_map_info;

            const vkd3d_shader_resource_binding const_buffer_binding{
                .type = VKD3D_SHADER_DESCRIPTOR_TYPE_CBV,
                .register_space = 0,
                .register_index = VKD3D_SHADER_D3DBC_FLOAT_CONSTANT_REGISTER,
                .shader_visibility = shader_visibility,
                .flags = VKD3D_SHADER_BINDING_FLAG_BUFFER,
                .binding = {.set = descriptor_set, .binding = 0, .count = 1},
            };

            vkd3d_shader_interface_info interface_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_INTERFACE_INFO};
            interface_info.next = &spirv_info;
            interface_info.bindings = &const_buffer_binding;
            interface_info.binding_count = 1;
            interface_info.combined_samplers = combined_samplers;
            interface_info.combined_sampler_count = combined_sampler_count;

            vkd3d_shader_compile_info compile_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
            compile_info.source.code = tokens;
            compile_info.source.size = token_size_bytes;
            compile_info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
            compile_info.target_type = VKD3D_SHADER_TARGET_SPIRV_BINARY;
            compile_info.log_level = VKD3D_SHADER_LOG_NONE;
            compile_info.next = &interface_info;

            vkd3d_shader_code out{};
            char* messages = nullptr;
            const int result = vkd3d_shader_compile(&compile_info, &out, &messages);
            if (messages != nullptr)
            {
                vkd3d_shader_free_messages(messages);
            }
            if (result < 0)
            {
                return false;
            }

            out_spirv.resize(out.size / sizeof(uint32_t));
            std::memcpy(out_spirv.data(), out.code, out.size);
            vkd3d_shader_free_shader_code(&out);
            return true;
        }
    } // namespace

    bool translate_d3d9_shader_pair(const void* vs_tokens, const size_t vs_token_size_bytes, const void* ps_tokens,
                                    const size_t ps_token_size_bytes, shader_pair_spirv& out)
    {
        out.vertex_spirv.clear();
        out.pixel_spirv.clear();

        if (vs_tokens == nullptr || vs_token_size_bytes == 0 || ps_tokens == nullptr || ps_token_size_bytes == 0)
        {
            return false;
        }

        vkd3d_shader_scan_signature_info vs_scan{};
        vkd3d_shader_signature vs_output{};
        if (!scan_signature(vs_tokens, vs_token_size_bytes, vs_scan, vs_output, /*want_output=*/true))
        {
            return false;
        }

        vkd3d_shader_scan_signature_info ps_scan{};
        vkd3d_shader_signature ps_input{};
        if (!scan_signature(ps_tokens, ps_token_size_bytes, ps_scan, ps_input, /*want_output=*/false))
        {
            vkd3d_shader_free_scan_signature_info(&vs_scan);
            return false;
        }

        std::vector<vkd3d_shader_varying_map> varying_map(ps_input.element_count);
        unsigned int varying_count = 0;
        vkd3d_shader_build_varying_map(&vs_output, &ps_input, &varying_count, varying_map.data());
        varying_map.resize(varying_count);

        vkd3d_shader_free_scan_signature_info(&vs_scan);
        vkd3d_shader_free_scan_signature_info(&ps_scan);

        vkd3d_shader_varying_map_info varying_map_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_VARYING_MAP_INFO};
        varying_map_info.varying_map = varying_map.data();
        varying_map_info.varying_count = varying_count;

        // No combined-sampler binding for VS in this milestone -- d3d9_host.cpp's vs_bindings (set 0) has no
        // sampler slot at all; vertex-shader texture fetch (SM3.0) is out of scope, not merely unwired here.
        if (!compile_stage(vs_tokens, vs_token_size_bytes, &varying_map_info, VKD3D_SHADER_VISIBILITY_VERTEX, 0,
                            nullptr, 0, out.vertex_spirv))
        {
            return false;
        }

        // Matches d3d9_host.cpp's ensure_programmable_pipeline PS descriptor set (set 1): binding 0 is the
        // float-const UBO, binding 1 is the combined-image-sampler for texture stage 0. vkd3d's own D3DBC
        // frontend addresses combined samplers by plain sampler-stage number (resource_index == sampler_index
        // == the D3D9 s# register), matching SetTexture(stage, ...) directly.
        const vkd3d_shader_combined_resource_sampler ps_sampler_binding{
            .resource_space = 0,
            .resource_index = 0,
            .sampler_space = 0,
            .sampler_index = 0,
            .shader_visibility = VKD3D_SHADER_VISIBILITY_PIXEL,
            // Must be IMAGE, not BUFFER (unlike the CBV binding above): vkd3d matches this bitwise against
            // the shader's own declared resource dimension and silently drops the binding on a mismatch --
            // the sampler variable then never resolves and vkd3d-shader crashes when the shader references it.
            .flags = VKD3D_SHADER_BINDING_FLAG_IMAGE,
            .binding = {.set = 1, .binding = 1, .count = 1},
        };
        if (!compile_stage(ps_tokens, ps_token_size_bytes, nullptr, VKD3D_SHADER_VISIBILITY_PIXEL, 1,
                            &ps_sampler_binding, 1, out.pixel_spirv))
        {
            out.vertex_spirv.clear();
            return false;
        }
        return true;
    }
} // namespace sogen
