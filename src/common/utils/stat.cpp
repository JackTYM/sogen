#define _FILE_OFFSET_BITS 64

#include "stat.hpp"
#include "time.hpp"

#include <array>

#if defined(_WIN32) && !defined(__MINGW64__)
#include <corecrt_io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef OS_MAC
#include <sys/fsgetpath.h>
#include <sys/mount.h>
#include <sys/param.h>
#endif

#include <sys/stat.h>

#undef st_atime
#undef st_mtime
#undef st_ctime

namespace sogen
{

    LARGE_INTEGER convert_timespec_to_filetime(const timespec timespec)
    {
        return {.QuadPart = (timespec.tv_sec * HUNDRED_NANOSECONDS_IN_ONE_SECOND) + (timespec.tv_nsec / 100) + WINDOWS_EPOCH_DIFFERENCE};
    }

#if defined(_WIN32) && !defined(__MINGW64__)

    static timespec convert_filetime_to_timespec(const FILETIME windows_time)
    {
        const LARGE_INTEGER time{.LowPart = windows_time.dwLowDateTime, .HighPart = static_cast<LONG>(windows_time.dwHighDateTime)};
        const auto value = time.QuadPart - WINDOWS_EPOCH_DIFFERENCE;
        return {.tv_sec = value / HUNDRED_NANOSECONDS_IN_ONE_SECOND,
                .tv_nsec = static_cast<LONG>((value % HUNDRED_NANOSECONDS_IN_ONE_SECOND) * 100)};
    }

    static bool do_stat(HANDLE handle, struct compat_stat* stat)
    {
        const auto file_type = GetFileType(handle);
        if (file_type != FILE_TYPE_DISK)
        {
            return false;
        }

        BY_HANDLE_FILE_INFORMATION file_info{};
        if (!GetFileInformationByHandle(handle, &file_info))
        {
            return false;
        }

        stat->st_dev = 0;
        stat->st_rdev = 0;
        stat->st_uid = 0;
        stat->st_gid = 0;
        stat->st_nlink = 1;

        auto st_mode = (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? _S_IFDIR | _S_IEXEC : _S_IFREG;
        st_mode |= (file_info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? _S_IREAD : _S_IREAD | _S_IWRITE;
        st_mode |= (st_mode & 0700) >> 3;
        st_mode |= (st_mode & 0700) >> 6;
        stat->st_mode = st_mode;

        stat->st_mtimespec = convert_filetime_to_timespec(file_info.ftLastWriteTime);
        stat->st_atimespec = convert_filetime_to_timespec(file_info.ftLastAccessTime);
        stat->st_ctimespec = convert_filetime_to_timespec(file_info.ftCreationTime);

        stat->st_size = static_cast<int64_t>((file_info.nFileSizeHigh * 0x100000000ULL) + file_info.nFileSizeLow);
        stat->st_ino = (file_info.nFileIndexHigh * 0x100000000ULL) + file_info.nFileIndexLow;

        return true;
    }

    bool compat_fstat(int fd, struct compat_stat* stat)
    {
        auto* const handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        return do_stat(handle, stat);
    }

    bool compat_stat(const std::filesystem::path& file_name, struct compat_stat* stat)
    {
        if (file_name.empty())
        {
            return false;
        }

        auto* const file_handle =
            CreateFileW(file_name.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

        if (file_handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const auto result = do_stat(file_handle, stat);
        CloseHandle(file_handle);

        return result;
    }

    std::optional<std::filesystem::path> compat_find_path_by_inode(const std::filesystem::path& search_root, const uint64_t inode)
    {
        auto* const volume_hint =
            CreateFileW(search_root.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (volume_hint == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        FILE_ID_DESCRIPTOR descriptor{};
        descriptor.dwSize = sizeof(descriptor);
        descriptor.Type = FileIdType;
        descriptor.FileId.QuadPart = static_cast<LONGLONG>(inode);

        auto* const file_handle = OpenFileById(volume_hint, &descriptor, FILE_READ_ATTRIBUTES,
                                               FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, FILE_FLAG_BACKUP_SEMANTICS);
        CloseHandle(volume_hint);

        if (file_handle == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        std::wstring path(MAX_PATH, L'\0');
        auto length = GetFinalPathNameByHandleW(file_handle, path.data(), static_cast<DWORD>(path.size()), VOLUME_NAME_DOS);
        if (length >= path.size())
        {
            path.resize(length);
            length = GetFinalPathNameByHandleW(file_handle, path.data(), static_cast<DWORD>(path.size()), VOLUME_NAME_DOS);
        }

        CloseHandle(file_handle);

        if (length == 0 || length >= path.size())
        {
            return std::nullopt;
        }

        path.resize(length);

        constexpr std::wstring_view extended_prefix = L"\\\\?\\";
        if (path.starts_with(extended_prefix))
        {
            path.erase(0, extended_prefix.size());
        }

        return std::filesystem::path(path);
    }

#else

#if defined(__MINGW64__)

    static constexpr timespec time64_to_timespec(int64_t time64)
    {
        return {.tv_sec = time64, .tv_nsec = 0};
    }

#endif

    static void set_stat(struct compat_stat* dst, const struct stat* src)
    {
        dst->st_dev = 0;
        dst->st_rdev = 0;
        dst->st_uid = 0;
        dst->st_gid = 0;
        dst->st_nlink = 1;
        dst->st_mode = src->st_mode & (S_IFMT | 0700);
        dst->st_mode |= (src->st_mode & 0700) >> 3;
        dst->st_mode |= (src->st_mode & 0700) >> 6;
        dst->st_ino = src->st_ino;
        dst->st_size = src->st_size;
#ifdef OS_MAC
        dst->st_atimespec = src->st_atimespec;
        dst->st_mtimespec = src->st_mtimespec;
        dst->st_ctimespec = src->st_ctimespec;
#elif defined(__MINGW64__)
        dst->st_atimespec = time64_to_timespec(src->st_atime);
        dst->st_mtimespec = time64_to_timespec(src->st_mtime);
        dst->st_ctimespec = time64_to_timespec(src->st_ctime);
#else
        dst->st_atimespec = src->st_atim;
        dst->st_mtimespec = src->st_mtim;
        dst->st_ctimespec = src->st_ctim;
#endif
    }

    bool compat_fstat(int fd, struct compat_stat* stat)
    {
        struct stat file_stat{};
        if (fstat(fd, &file_stat))
        {
            return false;
        }

        set_stat(stat, &file_stat);

        return true;
    }

    bool compat_stat(const std::filesystem::path& file_name, struct compat_stat* stat)
    {
        struct stat file_stat{};
        if (::stat(file_name.string().c_str(), &file_stat) != 0)
        {
            return false;
        }

        set_stat(stat, &file_stat);

        return true;
    }

    std::optional<std::filesystem::path> compat_find_path_by_inode(const std::filesystem::path& search_root, const uint64_t inode)
    {
#ifdef OS_MAC
        struct statfs volume{};
        if (statfs(search_root.string().c_str(), &volume) != 0)
        {
            return std::nullopt;
        }

        std::array<char, MAXPATHLEN> path{};
        if (fsgetpath(path.data(), path.size(), &volume.f_fsid, inode) <= 0)
        {
            return std::nullopt;
        }

        return std::filesystem::path(path.data());
#else
        const auto matches = [inode](const std::filesystem::path& candidate) {
            struct stat file_stat{};
            return ::stat(candidate.string().c_str(), &file_stat) == 0 && static_cast<uint64_t>(file_stat.st_ino) == inode;
        };

        if (matches(search_root))
        {
            return search_root;
        }

        std::error_code ec{};
        for (auto it =
                 std::filesystem::recursive_directory_iterator(search_root, std::filesystem::directory_options::skip_permission_denied, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (matches(it->path()))
            {
                return it->path();
            }
        }

        return std::nullopt;
#endif
    }

#endif
} // namespace sogen
