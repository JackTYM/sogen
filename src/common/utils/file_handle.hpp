#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <type_traits>

#if defined(OS_WINDOWS) && !defined(__MINGW64__)
#include <corecrt_io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#endif

namespace sogen
{

    namespace utils
    {
        class file_handle
        {
          public:
            struct rename_information
            {
                std::filesystem::path old_filepath;
                std::filesystem::path new_filepath;

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write(this->old_filepath.u16string());
                    buffer.write(this->new_filepath.u16string());
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    this->old_filepath = buffer.read<std::u16string>();
                    this->new_filepath = buffer.read<std::u16string>();
                }
            };

            struct delete_information
            {
                std::filesystem::path filepath;

                void serialize(utils::buffer_serializer& buffer) const
                {
                    buffer.write(this->filepath.u16string());
                }

                void deserialize(utils::buffer_deserializer& buffer)
                {
                    this->filepath = buffer.read<std::u16string>();
                }
            };

            file_handle() = default;

            file_handle(FILE* file)
            {
                this->operator=(file);
            }

            file_handle(const file_handle&) = delete;
            file_handle& operator=(const file_handle&) = delete;

            file_handle(file_handle&&) noexcept = default;
            file_handle& operator=(file_handle&&) noexcept = default;

            ~file_handle() = default;

            file_handle& operator=(FILE* file)
            {
                this->state_ = std::make_shared<open_file>();
                this->state_->file = file;

                return *this;
            }

            // A duplicated NT handle refers to the same file object: position and pending rename/delete
            // are shared, and the host file is closed with the last reference.
            [[nodiscard]] file_handle share() const
            {
                file_handle shared{};
                shared.state_ = this->state_;
                return shared;
            }

            [[nodiscard]] explicit operator bool() const
            {
                return this->get() != nullptr;
            }

            [[nodiscard]] operator FILE*() const
            {
                return this->get();
            }

            [[nodiscard]] int file_descriptor() const
            {
                return fileno(this->get());
            }

            [[nodiscard]] int64_t size() const
            {
                const auto current_position = this->tell();

                this->seek_to(0, SEEK_END);
                const auto size = this->tell();
                this->seek_to(current_position);

                return size;
            }

            bool seek_to(const int64_t position, const int origin = SEEK_SET) const
            {
                return _fseeki64(this->get(), position, origin) == 0;
            }

            [[nodiscard]] int64_t tell() const
            {
                return _ftelli64(this->get());
            }

            bool resize(uint64_t size) const
            {
                const auto fd = this->file_descriptor();
                if (fd == -1)
                {
                    return false;
                }

#ifdef OS_WINDOWS
                // NOLINTNEXTLINE(google-runtime-int)
                return _chsize_s(fd, static_cast<long long>(size)) == 0;
#else
                return ftruncate(fd, static_cast<off_t>(size)) == 0;
#endif
            }

            void defer_rename(std::filesystem::path oldname, std::filesystem::path newname)
            {
                this->state().deferred_rename = rename_information{.old_filepath = std::move(oldname), .new_filepath = std::move(newname)};
            }

            void defer_delete(std::filesystem::path name)
            {
                if (name == std::filesystem::path{})
                {
                    this->state().deferred_delete = {};
                    return;
                }

                this->state().deferred_delete = delete_information{.filepath = std::move(name)};
            }

            std::optional<std::filesystem::path> take_deferred_delete()
            {
                if (!this->state_ || !this->state_->deferred_delete)
                {
                    return std::nullopt;
                }

                auto path = std::move(this->state_->deferred_delete->filepath);
                this->state_->deferred_delete = {};
                return path;
            }

            // Holds an advisory shared lock on `path` for the remaining lifetime of this host
            // process (the fd is intentionally never closed - the kernel releases the lock on
            // process exit). Used by a freshly-spawned child process (NtCreateUserProcess) to
            // protect its own main image file, on the real host filesystem, against a concurrently
            // running sibling/parent process's deferred-delete (see is_pinned_by_another_process) -
            // real Windows only actually removes a delete-pending file once every handle across
            // every process has closed; sogen's own per-process file_handle has no cross-process
            // visibility of that on its own, so real advisory file locks fill that gap.
            static void pin_for_process_lifetime(const std::filesystem::path& path)
            {
#ifndef OS_WINDOWS
                const int fd = ::open(path.c_str(), O_RDONLY);
                if (fd < 0)
                {
                    return;
                }

                if (::flock(fd, LOCK_SH | LOCK_NB) != 0)
                {
                    ::close(fd);
                }
#else
                (void)path;
#endif
            }

            // Real Windows' DLL search order always checks the directory a process was launched
            // from before anything else, so a freshly-spawned process's own dependencies typically
            // sit right next to its main image - pin_for_process_lifetime alone only protects that
            // main image itself, leaving those sibling dependencies unprotected against the same
            // concurrently running sibling/parent's deferred-delete. Real hardware gets away with
            // this because a native process reaches its own early LoadLibrary calls long before a
            // disk-bound cleanup loop gets around to deleting dozens of files; under emulation the
            // relative cost of interpreting a process's startup instructions versus running a
            // cleanup loop's syscalls is reversed, so the same race that never resolves badly on
            // real hardware reliably does here without this.
            static void pin_image_directory_for_process_lifetime(const std::filesystem::path& image_path)
            {
                std::error_code ec{};
                const auto directory = image_path.parent_path();

                for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
                {
                    if (entry.is_regular_file(ec))
                    {
                        pin_for_process_lifetime(entry.path());
                    }
                }
            }

            void serialize(utils::buffer_serializer& buffer) const
            {
                buffer.write(this->tell());
                buffer.write_optional(this->state_ ? this->state_->deferred_rename : std::nullopt);
                buffer.write_optional(this->state_ ? this->state_->deferred_delete : std::nullopt);
            }

            void deserialize(utils::buffer_deserializer& buffer)
            {
                int64_t position = 0;
                buffer.read(position);

                if (!this->seek_to(position))
                {
                    throw std::runtime_error("Failed to seek to serialized file position");
                }

                buffer.read_optional(this->state().deferred_rename);
                buffer.read_optional(this->state().deferred_delete);
            }

          private:
            struct open_file
            {
                FILE* file{};
                std::optional<rename_information> deferred_rename;
                std::optional<delete_information> deferred_delete;

                open_file() = default;
                open_file(const open_file&) = delete;
                open_file& operator=(const open_file&) = delete;
                open_file(open_file&&) = delete;
                open_file& operator=(open_file&&) = delete;

                ~open_file()
                {
                    if (this->file)
                    {
                        (void)fclose(this->file);
                    }

                    if (this->deferred_rename && !this->deferred_delete)
                    {
                        std::error_code ec{};
                        std::filesystem::rename(this->deferred_rename->old_filepath, this->deferred_rename->new_filepath, ec);
                    }

                    if (this->deferred_delete && !is_pinned_by_another_process(this->deferred_delete->filepath))
                    {
                        std::error_code ec{};
                        std::filesystem::remove(this->deferred_delete->filepath, ec);
                    }
                }
            };

            std::shared_ptr<open_file> state_{};

            FILE* get() const
            {
                return this->state_ ? this->state_->file : nullptr;
            }

            open_file& state()
            {
                if (!this->state_)
                {
                    this->state_ = std::make_shared<open_file>();
                }

                return *this->state_;
            }

            static bool is_pinned_by_another_process(const std::filesystem::path& path)
            {
#ifndef OS_WINDOWS
                const int fd = ::open(path.c_str(), O_RDONLY);
                if (fd < 0)
                {
                    return false;
                }

                const bool acquired = ::flock(fd, LOCK_EX | LOCK_NB) == 0;
                if (acquired)
                {
                    ::flock(fd, LOCK_UN);
                }

                ::close(fd);
                return !acquired;
#else
                (void)path;
                return false;
#endif
            }
        };
    }
} // namespace sogen
