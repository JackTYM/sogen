#include "std_include.hpp"

#include "snapshot_manifest.hpp"

#include <utils/serialization.hpp>

#include <stdexcept>

namespace sogen::snapshot
{
    namespace
    {
        bool is_plain_file_name(const std::filesystem::path& file)
        {
            return !file.empty() && file == file.filename() && file != "." && file != "..";
        }

        void validate(const child_process_snapshot& child)
        {
            const auto& exit = child.process.exit;
            const auto running = !exit.has_value();
            if (!child.process.process || running == child.file.empty() || (!running && exit->kind > process_exit_kind::runtime_failure))
            {
                throw std::runtime_error("The snapshot holds an invalid child process");
            }
        }
    }

    void child_process_snapshot::serialize(utils::buffer_serializer& buffer) const
    {
        buffer.write(this->process.process.value);
        buffer.write(this->process.process_id);
        buffer.write(this->process.thread_id);
        buffer.write(this->process.exit.has_value());
        if (this->process.exit)
        {
            buffer.write(this->process.exit->kind);
            buffer.write(this->process.exit->code);
        }
        buffer.write_string(this->file);
    }

    void child_process_snapshot::deserialize(utils::buffer_deserializer& buffer)
    {
        buffer.read(this->process.process.value);
        buffer.read(this->process.process_id);
        buffer.read(this->process.thread_id);
        this->process.exit.reset();
        if (buffer.read<bool>())
        {
            auto& exit = this->process.exit.emplace();
            buffer.read(exit.kind);
            buffer.read(exit.code);
        }
        this->file = buffer.read_string<char>();
        validate(*this);
    }

    std::filesystem::path get_child_snapshot_path(const std::filesystem::path& parent_snapshot, const uint32_t process_id)
    {
        auto file = parent_snapshot.stem();
        file += ".child-" + std::to_string(process_id);
        file += parent_snapshot.extension();
        return parent_snapshot.parent_path() / file;
    }

    std::filesystem::path resolve_child_snapshot(const std::filesystem::path& parent_snapshot, const std::string_view file)
    {
        const std::filesystem::path child{file};
        if (!is_plain_file_name(child))
        {
            throw std::runtime_error("The snapshot names an invalid child process file: " + std::string{file});
        }
        return parent_snapshot.parent_path() / child;
    }
}
