#include "guest_file.hpp"

#include "../emulator_utils.hpp"
#include "../file_system.hpp"

#include <utils/string.hpp>

namespace sogen
{

    namespace
    {
        std::u16string get_reopen_mode(const std::u16string& mode)
        {
            if (mode.starts_with(u'w'))
            {
                return u"r+b";
            }

            return mode;
        }

        void validate_writable_after_restore(const file_system& file_sys, const windows_path& path, const std::string_view saved_state)
        {
            if (!file_sys.is_writable(path))
            {
                throw std::runtime_error(path.string() + " was saved " + std::string(saved_state) + ", but its mount is not writable now");
            }
        }
    }

    guest_file::guest_file(file_system& file_sys, const windows_path& path, std::u16string mode, std::unique_ptr<i_file> io)
        : state_(std::make_shared<open_file>())
    {
        this->state_->file_sys = &file_sys;
        this->state_->path = path.lexically_normal();
        this->state_->mode = std::move(mode);
        this->state_->io = std::move(io);
    }

    guest_file guest_file::share() const
    {
        guest_file shared{};
        shared.state_ = this->state_;
        return shared;
    }

    const windows_path& guest_file::path() const
    {
        static const windows_path no_path{};
        return this->state_ ? this->state_->path : no_path;
    }

    size_t guest_file::read(const std::span<std::byte> buffer) const
    {
        return this->state_->io->read(buffer);
    }

    size_t guest_file::write(const std::span<const std::byte> buffer) const
    {
        return this->state_->io->write(buffer);
    }

    bool guest_file::flush() const
    {
        return this->state_->io->flush();
    }

    bool guest_file::seek_to(const int64_t position, const int origin) const
    {
        return this->state_->io->seek(position, origin);
    }

    int64_t guest_file::tell() const
    {
        return this->state_->io->tell();
    }

    int64_t guest_file::size() const
    {
        return this->state_->io->size();
    }

    bool guest_file::resize(const uint64_t size) const
    {
        return this->state_->io->resize(size);
    }

    std::optional<struct compat_stat> guest_file::stat() const
    {
        return this->state_->io->stat();
    }

    void guest_file::defer_rename(const windows_path& new_path) const
    {
        this->state_->rename_target = new_path.lexically_normal();
    }

    void guest_file::set_delete_on_close(const bool delete_on_close) const
    {
        this->state_->delete_on_close = delete_on_close;
    }

    bool guest_file::take_delete_on_close() const
    {
        return this->state_ && std::exchange(this->state_->delete_on_close, false);
    }

    void guest_file::serialize(utils::buffer_serializer& buffer) const
    {
        const open_file no_file{};
        const auto& state = this->state_ ? *this->state_ : no_file;

        buffer.write(state.path);
        buffer.write(state.mode);
        buffer.write_optional(state.rename_target);
        buffer.write(state.delete_on_close);

        const auto has_io = state.io != nullptr;
        buffer.write(has_io);

        if (has_io)
        {
            buffer.write(state.io->tell());
        }
    }

    void guest_file::deserialize(utils::buffer_deserializer& buffer)
    {
        if (this->state_)
        {
            this->state_->rename_target.reset();
            this->state_->delete_on_close = false;
        }

        auto state = std::make_shared<open_file>();
        state->file_sys = &buffer.read<file_system_wrapper>().get();
        this->state_ = state;

        buffer.read(state->path);
        buffer.read(state->mode);
        buffer.read_optional(state->rename_target);
        buffer.read(state->delete_on_close);

        if (state->delete_on_close)
        {
            validate_writable_after_restore(*state->file_sys, state->path, "with delete-on-close set");
        }

        if (state->rename_target)
        {
            validate_writable_after_restore(*state->file_sys, state->path, "with a pending rename");
            validate_writable_after_restore(*state->file_sys, *state->rename_target, "as a pending rename target");
        }

        if (!buffer.read<bool>())
        {
            return;
        }

        const auto position = buffer.read<int64_t>();

        if (is_write_open_mode(state->mode))
        {
            validate_writable_after_restore(*state->file_sys, state->path, "with a writable handle");
        }

        auto result = state->file_sys->open(state->path, get_reopen_mode(state->mode));
        if (!result.file)
        {
            throw std::runtime_error("Failed to reopen file " + state->path.string() + ": status 0x" +
                                     utils::string::to_hex_number(static_cast<uint32_t>(result.status)));
        }

        if (!result.file->seek(position, SEEK_SET))
        {
            throw std::runtime_error("Failed to seek to serialized file position");
        }

        state->io = std::move(result.file);
    }

    guest_file::open_file::~open_file()
    {
        this->io.reset();

        if (!this->file_sys)
        {
            return;
        }

        if (this->delete_on_close)
        {
            if (!this->file_sys->is_pinned_by_another_process(this->path))
            {
                (void)this->file_sys->remove(this->path);
            }
        }
        else if (this->rename_target)
        {
            (void)this->file_sys->rename(this->path, *this->rename_target);
        }
    }

} // namespace sogen
