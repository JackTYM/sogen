#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "serialization.hpp"
#include "shared_backing.hpp"

#include <platform/ui_backend.hpp>

namespace sogen
{
    // A frame presented into a window whose on-screen top-level belongs to another host process. Each hop
    // re-addresses it to the next foreign ancestor; destination and clip are in the client coordinates of the
    // window the carrying pipe_ipc_message is addressed to.
    struct window_surface_update
    {
        uint64_t layer{};
        RECT destination{};
        RECT clip{};
        bool visible{};
        int32_t width{};
        int32_t height{};
        int32_t stride{};
        std::string shm_name{};
        uint64_t shm_size{};
        uint64_t offset{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->layer);
            buffer.write(this->destination);
            buffer.write(this->clip);
            buffer.write(this->visible);
            buffer.write(this->width);
            buffer.write(this->height);
            buffer.write(this->stride);
            buffer.write(this->shm_name);
            buffer.write(this->shm_size);
            buffer.write(this->offset);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->layer);
            buffer.read(this->destination);
            buffer.read(this->clip);
            buffer.read(this->visible);
            buffer.read(this->width);
            buffer.read(this->height);
            buffer.read(this->stride);
            buffer.read(this->shm_name);
            buffer.read(this->shm_size);
            buffer.read(this->offset);
        }
    };

    // Presenter side. Frames go into a ring of slots in one named shared-memory object, so the message that
    // announces a frame only carries a slot reference; the receiver copies the pixels out when it handles the
    // message, long before the ring wraps around to that slot again.
    class window_surface_exporter
    {
      public:
        static constexpr uint32_t slot_count = 4;

        window_surface_exporter() = default;
        ~window_surface_exporter();

        window_surface_exporter(const window_surface_exporter&) = delete;
        window_surface_exporter& operator=(const window_surface_exporter&) = delete;

        // Fills the frame fields of `update`. Without shared memory the pixels are returned for the message to
        // carry inline instead.
        std::optional<std::string> publish(window_surface_update& update, const ui_surface_desc& surface);

      private:
        bool ensure_ring(size_t frame_size);
        void release_ring();

        std::shared_ptr<shared_backing> ring_{};
        std::string ring_name_{};
        size_t slot_size_{};
        uint32_t next_slot_{};
    };

    // Receiver side: maps the presenters' rings by name and resolves an update to its pixels.
    class window_surface_importer
    {
      public:
        std::optional<ui_surface_desc> resolve(const window_surface_update& update, const std::string& inline_pixels);

      private:
        static constexpr size_t max_mappings = 8;

        std::unordered_map<std::string, std::shared_ptr<shared_backing>> mappings_{};
    };
}
