#pragma once

#include "mount_provider.hpp"
#include "../windows_path.hpp"

namespace sogen
{

    class file_system;

    class guest_file
    {
      public:
        guest_file() = default;
        guest_file(file_system& file_sys, const windows_path& path, std::u16string mode = {}, std::unique_ptr<i_file> io = {});

        guest_file(const guest_file&) = delete;
        guest_file& operator=(const guest_file&) = delete;

        guest_file(guest_file&& obj) noexcept = default;
        guest_file& operator=(guest_file&& obj) noexcept = default;

        ~guest_file() = default;

        // A duplicated NT handle refers to the same file object: position and pending rename/delete
        // are shared, and the file is closed with the last reference.
        [[nodiscard]] guest_file share() const;

        [[nodiscard]] explicit operator bool() const
        {
            return this->state_ && this->state_->io != nullptr;
        }

        const windows_path& path() const;

        bool is_writable() const
        {
            return static_cast<bool>(*this) && is_write_open_mode(this->state_->mode);
        }

        size_t read(std::span<std::byte> buffer) const;
        size_t write(std::span<const std::byte> buffer) const;
        bool flush() const;

        bool seek_to(int64_t position, int origin = SEEK_SET) const;
        [[nodiscard]] int64_t tell() const;
        [[nodiscard]] int64_t size() const;
        bool resize(uint64_t size) const;
        [[nodiscard]] std::optional<struct compat_stat> stat() const;

        void defer_rename(const windows_path& new_path) const;
        // posix: FILE_DISPOSITION_POSIX_SEMANTICS. The name goes away when the marking handle closes, even while other
        // handles to the file stay open (classic semantics wait for the last handle).
        void set_delete_on_close(bool delete_on_close, bool posix = false) const;
        bool take_delete_on_close() const;
        bool has_posix_delete() const;

        void serialize(utils::buffer_serializer& buffer) const;
        void deserialize(utils::buffer_deserializer& buffer);

      private:
        struct open_file
        {
            file_system* file_sys{};
            windows_path path{};
            std::u16string mode{};
            std::unique_ptr<i_file> io{};
            std::optional<windows_path> rename_target{};
            bool delete_on_close{false};
            bool posix_delete{false}; // not serialized: a restored handle falls back to classic semantics

            open_file() = default;
            open_file(const open_file&) = delete;
            open_file& operator=(const open_file&) = delete;
            open_file(open_file&&) = delete;
            open_file& operator=(open_file&&) = delete;

            ~open_file();
        };

        std::shared_ptr<open_file> state_{};
    };

} // namespace sogen
