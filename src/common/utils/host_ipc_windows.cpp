#include "host_ipc.hpp"

// Windows transport for the analyzer's multi-process machinery; see host_ipc.hpp for the design.

#ifdef _WIN32

#include "win.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace sogen::host_ipc
{
    namespace
    {
        // Set on the length prefix when handle values follow it. Frames are capped at 64 MiB, so a real length
        // never has this bit.
        constexpr uint64_t handles_follow_flag = 1ULL << 63;

        // Pipe buffer quota per direction. Writes larger than this block (blocking channels) or are chunked
        // (non-blocking channel) until the peer reads, like a full socket buffer.
        constexpr DWORD pipe_buffer_size = 1u << 20;

        // Largest single write on a non-blocking channel, so a write can always fit the free quota eventually.
        constexpr size_t max_nonblocking_write = 64u << 10;

        HANDLE to_handle(const int fd)
        {
            return LongToHandle(fd);
        }

        int from_handle(const HANDLE handle)
        {
            return HandleToLong(handle);
        }

        struct handle_closer
        {
            void operator()(void* handle) const
            {
                if (handle && handle != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(handle);
                }
            }
        };

        using shared_process_handle = std::shared_ptr<void>;

        struct state
        {
            std::mutex mutex{};
            // Channel fd -> process at its other end (for DuplicateHandle into it).
            std::unordered_map<int, shared_process_handle> channel_peers{};
            // Host pid -> process handle of children this process created (kept open so the pid stays valid).
            std::unordered_map<int, shared_process_handle> children{};
            shared_process_handle parent{};
            bool parent_resolved{false};
            HANDLE job{};
        };

        state& get_state()
        {
            static state s{};
            return s;
        }

        // The process that created this one (PROCESS_BASIC_INFORMATION::InheritedFromUniqueProcessId), opened for
        // handle duplication. A child's channels to its parent are never bound explicitly (the parent has no way to
        // name its own handle inside the child), so this is the peer of any unbound channel. The parent is alive
        // while the child uses those channels: it is blocked in the bootstrap handshake when the child first sends,
        // and the child dies with it (job object), so the pid cannot have been reused.
        shared_process_handle open_parent_process()
        {
            struct basic_information
            {
                LONG exit_status;
                void* peb_base_address;
                ULONG_PTR affinity_mask;
                LONG base_priority;
                ULONG_PTR unique_process_id;
                ULONG_PTR inherited_from_unique_process_id;
            };

            using query_function = LONG(WINAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
            auto* const ntdll = ::GetModuleHandleW(L"ntdll.dll");
            const auto query = ntdll ? reinterpret_cast<query_function>(::GetProcAddress(ntdll, "NtQueryInformationProcess")) : nullptr;
            if (!query)
            {
                return nullptr;
            }

            basic_information info{};
            ULONG returned{};
            if (query(::GetCurrentProcess(), 0 /* ProcessBasicInformation */, &info, sizeof(info), &returned) < 0)
            {
                return nullptr;
            }

            auto* const process =
                ::OpenProcess(PROCESS_DUP_HANDLE | SYNCHRONIZE, FALSE, static_cast<DWORD>(info.inherited_from_unique_process_id));
            if (!process)
            {
                return nullptr;
            }

            return shared_process_handle{process, handle_closer{}};
        }

        shared_process_handle peer_process(const int fd)
        {
            auto& s = get_state();
            const std::lock_guard lock{s.mutex};

            if (const auto entry = s.channel_peers.find(fd); entry != s.channel_peers.end())
            {
                return entry->second;
            }

            if (!s.parent_resolved)
            {
                s.parent_resolved = true;
                s.parent = open_parent_process();
            }

            return s.parent;
        }

        HANDLE child_process_handle(const int pid)
        {
            auto& s = get_state();
            const std::lock_guard lock{s.mutex};
            const auto entry = s.children.find(pid);
            return entry == s.children.end() ? nullptr : entry->second.get();
        }

        // Every child is put in this job; closing its last handle (this process ending, however it ends) kills
        // them all, the way a POSIX child loses its parent's socket and ends. Children's own children are in the
        // children's jobs, nested inside this one.
        HANDLE get_kill_on_close_job()
        {
            auto& s = get_state();
            const std::lock_guard lock{s.mutex};
            if (!s.job)
            {
                auto* const job = ::CreateJobObjectW(nullptr, nullptr);
                if (job)
                {
                    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                    if (!::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                    {
                        ::CloseHandle(job);
                    }
                    else
                    {
                        s.job = job;
                    }
                }
            }

            return s.job;
        }

        std::wstring utf8_to_wide(const std::string& value)
        {
            if (value.empty())
            {
                return {};
            }

            const auto length = ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
            std::wstring result(static_cast<size_t>(std::max(length, 0)), L'\0');
            if (length > 0)
            {
                ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
            }
            return result;
        }

        // Quotes one argument so CommandLineToArgvW (which CLI11's ensure_utf8 uses in the child) yields it back
        // unchanged.
        void append_quoted_argument(std::wstring& command_line, const std::wstring& argument)
        {
            if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
            {
                command_line += argument;
                return;
            }

            command_line += L'"';
            for (auto it = argument.begin();; ++it)
            {
                size_t backslashes = 0;
                while (it != argument.end() && *it == L'\\')
                {
                    ++it;
                    ++backslashes;
                }

                if (it == argument.end())
                {
                    command_line.append(backslashes * 2, L'\\');
                    break;
                }

                if (*it == L'"')
                {
                    command_line.append(backslashes * 2 + 1, L'\\');
                    command_line += L'"';
                }
                else
                {
                    command_line.append(backslashes, L'\\');
                    command_line += *it;
                }
            }
            command_line += L'"';
        }

        void wait_briefly(const uint32_t iteration)
        {
            if (iteration < 1000)
            {
                ::SwitchToThread();
                return;
            }

            // Sleep(1) lasts a whole timer tick (up to 15.6 ms) unless something raised the system timer resolution;
            // a high-resolution waitable timer keeps a control round trip waiting on a busy peer near 0.5 ms.
            thread_local HANDLE timer = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if (!timer)
            {
                ::Sleep(1);
                return;
            }

            LARGE_INTEGER due{};
            due.QuadPart = -5000; // 0.5 ms, relative, in 100 ns units
            if (!::SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) || ::WaitForSingleObject(timer, 100) != WAIT_OBJECT_0)
            {
                ::Sleep(1);
            }
        }
    }

    bool supported()
    {
        return true;
    }

    bool write_all(const int fd, const void* data, size_t size)
    {
        const auto* p = static_cast<const std::byte*>(data);
        while (size > 0)
        {
            const auto chunk = static_cast<DWORD>(std::min<size_t>(size, 0x40000000));
            DWORD written = 0;
            if (!::WriteFile(to_handle(fd), p, chunk, &written, nullptr) || written == 0)
            {
                return false;
            }

            p += written;
            size -= written;
        }

        return true;
    }

    bool read_all(const int fd, void* data, size_t size)
    {
        auto* p = static_cast<std::byte*>(data);
        while (size > 0)
        {
            const auto chunk = static_cast<DWORD>(std::min<size_t>(size, 0x40000000));
            DWORD read = 0;
            if (!::ReadFile(to_handle(fd), p, chunk, &read, nullptr) || read == 0)
            {
                return false;
            }

            p += read;
            size -= read;
        }

        return true;
    }

    bool write_length_with_fds(const int fd, const uint64_t length, const std::span<const int> fds)
    {
        if (fds.size() > max_fds_per_frame || (length & handles_follow_flag) != 0)
        {
            return false;
        }

        const auto peer = peer_process(fd);
        if (!peer)
        {
            return false;
        }

        std::vector<uint64_t> remote_handles{};
        remote_handles.reserve(fds.size());
        const auto close_remote = [&] {
            for (const auto remote : remote_handles)
            {
                ::DuplicateHandle(peer.get(), reinterpret_cast<HANDLE>(remote), nullptr, nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
            }
        };

        for (const auto local : fds)
        {
            HANDLE remote{};
            if (!::DuplicateHandle(::GetCurrentProcess(), to_handle(local), peer.get(), &remote, 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                close_remote();
                return false;
            }
            remote_handles.push_back(reinterpret_cast<uint64_t>(remote));
        }

        std::vector<std::byte> header(sizeof(uint64_t) + 2 * sizeof(uint32_t) + remote_handles.size() * sizeof(uint64_t));
        const uint64_t flagged_length = length | handles_follow_flag;
        const auto count = static_cast<uint32_t>(remote_handles.size());
        std::memcpy(header.data(), &flagged_length, sizeof(flagged_length));
        std::memcpy(header.data() + sizeof(uint64_t), &count, sizeof(count));
        if (!remote_handles.empty())
        {
            std::memcpy(header.data() + sizeof(uint64_t) + 2 * sizeof(uint32_t), remote_handles.data(),
                        remote_handles.size() * sizeof(uint64_t));
        }

        if (!write_all(fd, header.data(), header.size()))
        {
            close_remote();
            return false;
        }

        return true;
    }

    bool read_length_with_fds(const int fd, uint64_t& length, std::vector<int>& received_fds)
    {
        if (!read_all(fd, &length, sizeof(length)))
        {
            return false;
        }

        if ((length & handles_follow_flag) == 0)
        {
            return true;
        }

        length &= ~handles_follow_flag;

        uint32_t counts[2]{};
        if (!read_all(fd, counts, sizeof(counts)) || counts[0] > max_fds_per_frame)
        {
            return false;
        }

        std::vector<uint64_t> handles(counts[0]);
        if (!handles.empty() && !read_all(fd, handles.data(), handles.size() * sizeof(uint64_t)))
        {
            return false;
        }

        for (const auto value : handles)
        {
            received_fds.push_back(from_handle(reinterpret_cast<HANDLE>(value)));
        }

        return true;
    }

    void close_fd(const int fd)
    {
        if (fd < 0)
        {
            return;
        }

        {
            auto& s = get_state();
            const std::lock_guard lock{s.mutex};
            s.channel_peers.erase(fd);
        }

        ::CloseHandle(to_handle(fd));
    }

    bool wait_readable(const int fd, const int timeout_ms)
    {
        if (timeout_ms < 0)
        {
            return true;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        for (uint32_t iteration = 0;; ++iteration)
        {
            DWORD available = 0;
            if (!::PeekNamedPipe(to_handle(fd), nullptr, 0, nullptr, &available, nullptr))
            {
                return true; // peer gone (or not a pipe): the read that follows reports it
            }

            if (available > 0)
            {
                return true;
            }

            if (timeout_ms == 0 || std::chrono::steady_clock::now() >= deadline)
            {
                return false;
            }

            wait_briefly(iteration);
        }
    }

    void set_nonblocking(const int fd)
    {
        DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
        ::SetNamedPipeHandleState(to_handle(fd), &mode, nullptr, nullptr);
    }

    io_result write_some(const int fd, const void* data, const size_t size)
    {
        DWORD written = 0;
        const auto chunk = static_cast<DWORD>(std::min(size, max_nonblocking_write));
        if (::WriteFile(to_handle(fd), data, chunk, &written, nullptr))
        {
            if (written > 0)
            {
                return {.status = io_status::done, .bytes = written, .raw = static_cast<ptrdiff_t>(written)};
            }

            return {.status = io_status::would_block};
        }

        return {.status = io_status::error, .raw = -1, .error_code = static_cast<int>(::GetLastError())};
    }

    io_result read_some(const int fd, void* data, const size_t size)
    {
        // Peek first: in PIPE_NOWAIT mode an empty pipe and a closing one both fail ReadFile with ERROR_NO_DATA.
        DWORD available = 0;
        if (!::PeekNamedPipe(to_handle(fd), nullptr, 0, nullptr, &available, nullptr))
        {
            const auto error = ::GetLastError();
            const auto closed = error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_NO_DATA;
            return {.status = closed ? io_status::eof : io_status::error, .raw = closed ? 0 : -1, .error_code = static_cast<int>(error)};
        }

        if (available == 0)
        {
            return {.status = io_status::would_block};
        }

        DWORD read = 0;
        const auto chunk = static_cast<DWORD>(std::min<size_t>(size, available));
        if (!::ReadFile(to_handle(fd), data, chunk, &read, nullptr))
        {
            const auto error = ::GetLastError();
            const auto closed = error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED;
            return {.status = closed ? io_status::eof : io_status::error, .raw = closed ? 0 : -1, .error_code = static_cast<int>(error)};
        }

        if (read == 0)
        {
            return {.status = io_status::would_block};
        }

        return {.status = io_status::done, .bytes = read, .raw = static_cast<ptrdiff_t>(read)};
    }

    std::string describe_error(const int error_code)
    {
        char* message = nullptr;
        const auto length = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                             nullptr, static_cast<DWORD>(error_code), 0, reinterpret_cast<char*>(&message), 0, nullptr);
        std::string result = length && message ? std::string(message, length) : "Win32 error " + std::to_string(error_code);
        if (message)
        {
            ::LocalFree(message);
        }

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == '.'))
        {
            result.pop_back();
        }
        return result;
    }

    void prepare_child_spawning()
    {
    }

    std::optional<channel_pair> create_channel_pair(std::string& error)
    {
        static std::atomic<uint64_t> counter{0};
        wchar_t name[128]{};
        std::swprintf(name, std::size(name), L"\\\\.\\pipe\\sogen-ipc-%lu-%llu-%llx", ::GetCurrentProcessId(),
                      static_cast<unsigned long long>(counter.fetch_add(1)), static_cast<unsigned long long>(::GetTickCount64()));

        auto* const server = ::CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                                pipe_buffer_size, pipe_buffer_size, 0, nullptr);
        if (server == INVALID_HANDLE_VALUE)
        {
            error = "CreateNamedPipe() failed: " + describe_error(static_cast<int>(::GetLastError()));
            return std::nullopt;
        }

        auto* const client = ::CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (client == INVALID_HANDLE_VALUE)
        {
            error = "CreateFile(pipe) failed: " + describe_error(static_cast<int>(::GetLastError()));
            ::CloseHandle(server);
            return std::nullopt;
        }

        if (!::ConnectNamedPipe(server, nullptr) && ::GetLastError() != ERROR_PIPE_CONNECTED)
        {
            error = "ConnectNamedPipe() failed: " + describe_error(static_cast<int>(::GetLastError()));
            ::CloseHandle(client);
            ::CloseHandle(server);
            return std::nullopt;
        }

        return channel_pair{.parent_end = from_handle(server), .child_end = from_handle(client)};
    }

    spawn_result spawn_process(const std::filesystem::path& executable, const std::vector<std::string>& argv,
                               const std::span<const int> child_fds, const std::span<const int> parent_only_fds)
    {
        (void)parent_only_fds; // handles are not inheritable unless listed below

        std::wstring command_line{};
        for (const auto& argument : argv)
        {
            if (!command_line.empty())
            {
                command_line += L' ';
            }
            append_quoted_argument(command_line, utf8_to_wide(argument));
        }

        std::vector<HANDLE> inherited{};
        for (const auto fd : child_fds)
        {
            auto* const handle = to_handle(fd);
            if (std::ranges::find(inherited, handle) == inherited.end())
            {
                ::SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
                inherited.push_back(handle);
            }
        }

        // The child writes its log to the same stdout/stderr as this process (hidden-run forwards them).
        HANDLE standard_handles[3]{};
        constexpr DWORD standard_ids[3]{STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
        for (size_t i = 0; i < 3; ++i)
        {
            auto* const source = ::GetStdHandle(standard_ids[i]);
            if (source && source != INVALID_HANDLE_VALUE &&
                ::DuplicateHandle(::GetCurrentProcess(), source, ::GetCurrentProcess(), &standard_handles[i], 0, TRUE,
                                  DUPLICATE_SAME_ACCESS))
            {
                inherited.push_back(standard_handles[i]);
            }
            else
            {
                standard_handles[i] = nullptr;
            }
        }

        const auto cleanup = [&] {
            for (const auto fd : child_fds)
            {
                ::SetHandleInformation(to_handle(fd), HANDLE_FLAG_INHERIT, 0);
            }
            for (auto* const handle : standard_handles)
            {
                if (handle)
                {
                    ::CloseHandle(handle);
                }
            }
        };

        SIZE_T attribute_size = 0;
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
        std::vector<std::byte> attribute_storage(attribute_size);
        auto* const attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
        if (!::InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size))
        {
            const auto error = "InitializeProcThreadAttributeList() failed: " + describe_error(static_cast<int>(::GetLastError()));
            cleanup();
            return {.error = error};
        }

        const auto delete_attributes = [&] { ::DeleteProcThreadAttributeList(attributes); };

        if (!::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(),
                                         inherited.size() * sizeof(HANDLE), nullptr, nullptr))
        {
            const auto error = "UpdateProcThreadAttribute() failed: " + describe_error(static_cast<int>(::GetLastError()));
            delete_attributes();
            cleanup();
            return {.error = error};
        }

        // lpDesktop stays null: the child inherits this process's window station and desktop.
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = standard_handles[0];
        startup.StartupInfo.hStdOutput = standard_handles[1];
        startup.StartupInfo.hStdError = standard_handles[2];
        startup.lpAttributeList = attributes;

        PROCESS_INFORMATION process{};
        const auto created = ::CreateProcessW(executable.wstring().c_str(), command_line.data(), nullptr, nullptr, TRUE,
                                              EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
                                              &startup.StartupInfo, &process);
        const auto create_error = ::GetLastError();
        delete_attributes();
        cleanup();

        if (!created)
        {
            return {.error = "CreateProcess() failed: " + describe_error(static_cast<int>(create_error))};
        }

        auto* const job = get_kill_on_close_job();
        if (!job || !::AssignProcessToJobObject(job, process.hProcess))
        {
            static std::once_flag warned{};
            const auto error = ::GetLastError();
            std::call_once(warned, [&] {
                std::fprintf(stderr, "host_ipc: cannot put child processes in a kill-on-close job (%s); they rely on the enclosing job\n",
                             describe_error(static_cast<int>(error)).c_str());
            });
        }

        ::ResumeThread(process.hThread);
        ::CloseHandle(process.hThread);

        const auto pid = static_cast<int>(process.dwProcessId);
        {
            auto& s = get_state();
            const std::lock_guard lock{s.mutex};
            s.children[pid] = shared_process_handle{process.hProcess, handle_closer{}};
        }

        return {.pid = pid};
    }

    void bind_channel_peer(const int fd, const int pid)
    {
        auto& s = get_state();
        const std::lock_guard lock{s.mutex};
        if (const auto child = s.children.find(pid); child != s.children.end())
        {
            s.channel_peers[fd] = child->second;
        }
    }

    bool process_alive(const int pid)
    {
        if (auto* const handle = child_process_handle(pid))
        {
            return ::WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
        }

        auto* const handle = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        if (!handle)
        {
            return false;
        }

        const auto alive = ::WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
        ::CloseHandle(handle);
        return alive;
    }

    void kill_process(const int pid)
    {
        if (auto* const handle = child_process_handle(pid))
        {
            ::TerminateProcess(handle, 1);
            return;
        }

        if (auto* const handle = ::OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid)))
        {
            ::TerminateProcess(handle, 1);
            ::CloseHandle(handle);
        }
    }

    int current_process_id()
    {
        return static_cast<int>(::GetCurrentProcessId());
    }
}

#endif
