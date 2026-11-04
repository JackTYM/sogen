#pragma once

#include <process_manager.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sogen
{
    namespace utils
    {
        class buffer_serializer;
        class buffer_deserializer;
    }

    namespace snapshot
    {
        struct child_process_snapshot
        {
            managed_process_record process{};
            // The child's own snapshot, next to its parent's. Empty for a child that had exited.
            std::string file{};

            void serialize(utils::buffer_serializer& buffer) const;
            void deserialize(utils::buffer_deserializer& buffer);
        };

        using child_process_manifest = std::vector<child_process_snapshot>;

        std::filesystem::path get_child_snapshot_path(const std::filesystem::path& parent_snapshot, uint32_t process_id);
        std::filesystem::path resolve_child_snapshot(const std::filesystem::path& parent_snapshot, std::string_view file);
    }
}
