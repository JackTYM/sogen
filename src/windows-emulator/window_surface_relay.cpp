#include "std_include.hpp"
#include "window_surface_relay.hpp"

namespace sogen
{
    window_surface_exporter::~window_surface_exporter()
    {
        this->release_ring();
    }

    void window_surface_exporter::release_ring()
    {
        if (!this->ring_name_.empty())
        {
            shared_backing::unlink_named(this->ring_name_);
        }

        this->ring_.reset();
        this->ring_name_.clear();
        this->slot_size_ = 0;
        this->next_slot_ = 0;
    }

    bool window_surface_exporter::ensure_ring(const size_t frame_size)
    {
        if (this->ring_ && frame_size <= this->slot_size_)
        {
            return true;
        }

        this->release_ring();

        std::string name{};
        auto ring = shared_backing::create_named(frame_size * slot_count, name);
        if (!ring)
        {
            return false;
        }

        this->ring_ = std::move(ring);
        this->ring_name_ = std::move(name);
        this->slot_size_ = frame_size;
        return true;
    }

    std::optional<std::string> window_surface_exporter::publish(window_surface_update& update, const ui_surface_desc& surface)
    {
        update.width = surface.width;
        update.height = surface.height;
        update.stride = surface.stride;

        const auto frame_size = static_cast<size_t>(std::max(surface.stride, 0)) * static_cast<size_t>(std::max(surface.height, 0));
        if (!surface.pixels || frame_size == 0)
        {
            return std::string{};
        }

        if (!this->ensure_ring(frame_size))
        {
            return std::string(static_cast<const char*>(surface.pixels), frame_size);
        }

        const auto offset = static_cast<size_t>(this->next_slot_) * this->slot_size_;
        this->next_slot_ = (this->next_slot_ + 1) % slot_count;
        std::memcpy(this->ring_->data() + offset, surface.pixels, frame_size);

        update.shm_name = this->ring_name_;
        update.shm_size = this->ring_->size();
        update.offset = offset;
        return std::nullopt;
    }

    std::optional<ui_surface_desc> window_surface_importer::resolve(const window_surface_update& update, const std::string& inline_pixels)
    {
        if (update.width <= 0 || update.height <= 0 || update.stride <= 0)
        {
            return std::nullopt;
        }

        const auto frame_size = static_cast<size_t>(update.stride) * static_cast<size_t>(update.height);
        ui_surface_desc surface{
            .width = update.width, .height = update.height, .stride = update.stride, .format = ui_surface_format::bgra8};

        if (update.shm_name.empty())
        {
            if (inline_pixels.size() < frame_size)
            {
                return std::nullopt;
            }

            surface.pixels = inline_pixels.data();
            return surface;
        }

        auto mapping = this->mappings_.find(update.shm_name);
        if (mapping == this->mappings_.end())
        {
            if (this->mappings_.size() >= max_mappings)
            {
                this->mappings_.clear();
            }

            auto backing = shared_backing::open_named(update.shm_name, static_cast<size_t>(update.shm_size));
            if (!backing)
            {
                return std::nullopt;
            }

            mapping = this->mappings_.emplace(update.shm_name, std::move(backing)).first;
        }

        if (update.offset + frame_size > mapping->second->size())
        {
            return std::nullopt;
        }

        surface.pixels = mapping->second->data() + update.offset;
        return surface;
    }
}
