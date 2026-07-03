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
                           const vkd3d_shader_varying_map_info* varying_map_info, std::vector<uint32_t>& out_spirv)
        {
            vkd3d_shader_spirv_target_info spirv_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
            spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_0;

            vkd3d_shader_compile_info compile_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
            compile_info.source.code = tokens;
            compile_info.source.size = token_size_bytes;
            compile_info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
            compile_info.target_type = VKD3D_SHADER_TARGET_SPIRV_BINARY;
            compile_info.log_level = VKD3D_SHADER_LOG_NONE;

            if (varying_map_info != nullptr)
            {
                compile_info.next = varying_map_info;
                // varying_map_info->next is set by the caller to chain onward to spirv_info.
            }
            else
            {
                compile_info.next = &spirv_info;
            }

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

        vkd3d_shader_spirv_target_info vs_spirv_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
        vs_spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_0;

        vkd3d_shader_varying_map_info varying_map_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_VARYING_MAP_INFO};
        varying_map_info.next = &vs_spirv_info;
        varying_map_info.varying_map = varying_map.data();
        varying_map_info.varying_count = varying_count;

        if (!compile_stage(vs_tokens, vs_token_size_bytes, &varying_map_info, out.vertex_spirv))
        {
            return false;
        }
        if (!compile_stage(ps_tokens, ps_token_size_bytes, nullptr, out.pixel_spirv))
        {
            out.vertex_spirv.clear();
            return false;
        }
        return true;
    }
} // namespace sogen
