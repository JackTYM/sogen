#include "host_shared_memory.hpp"

#ifdef _WIN32

#include "win.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace sogen::utils::host_shm
{
    namespace
    {
        HANDLE to_handle(const int handle)
        {
            return LongToHandle(handle);
        }

        int from_handle(const HANDLE handle)
        {
            return handle ? HandleToLong(handle) : -1;
        }

        std::wstring local_name(const std::string& name)
        {
            std::wstring result = L"Local\\";
            result.append(name.begin(), name.end());
            return result;
        }

        HANDLE create_mapping(const size_t size, const wchar_t* name)
        {
            const auto size64 = static_cast<uint64_t>(size);
            return ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, static_cast<DWORD>(size64 >> 32),
                                        static_cast<DWORD>(size64 & 0xFFFFFFFF), name);
        }
    }

    int create_anonymous(const size_t size)
    {
        if (size == 0)
        {
            return -1;
        }

        return from_handle(create_mapping(size, nullptr));
    }

    int create_unique_named(const size_t size, std::string& name)
    {
        if (size == 0)
        {
            return -1;
        }

        static std::atomic<uint64_t> counter{0};
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            std::array<char, 24> candidate{};
            std::snprintf(candidate.data(), candidate.size(), "sgn.%lx.%llx", static_cast<unsigned long>(::GetCurrentProcessId()),
                          static_cast<unsigned long long>(counter.fetch_add(1)));

            const auto wide = local_name(candidate.data());
            const auto mapping = create_mapping(size, wide.c_str());
            if (!mapping)
            {
                return -1;
            }

            if (::GetLastError() == ERROR_ALREADY_EXISTS)
            {
                ::CloseHandle(mapping);
                continue;
            }

            name = candidate.data();
            return from_handle(mapping);
        }

        return -1;
    }

    int open_named(const std::string& name)
    {
        const auto wide = local_name(name);
        return from_handle(::OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, wide.c_str()));
    }

    void* map(const int handle, const size_t size)
    {
        if (handle < 0 || size == 0)
        {
            return nullptr;
        }

        return ::MapViewOfFile(to_handle(handle), FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size);
    }

    void unmap(void* data)
    {
        if (data)
        {
            ::UnmapViewOfFile(data);
        }
    }

    void close(const int handle)
    {
        if (handle >= 0)
        {
            ::CloseHandle(to_handle(handle));
        }
    }
}

#endif
