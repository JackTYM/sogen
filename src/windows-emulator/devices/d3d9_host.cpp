#include "d3d9_host.hpp"

#include <d3d9_command_protocol.hpp>

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

        this->vk_device_ = device;
        return device;
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
            return read_record(payload, size, req) ? d3d_ok : d3derr_invalidcall;
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
