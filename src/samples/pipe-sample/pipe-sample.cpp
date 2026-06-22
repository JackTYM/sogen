#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_anonymous_pipe()
    {
        HANDLE read_end = nullptr, write_end = nullptr;
        if (!CreatePipe(&read_end, &write_end, nullptr, 0))
            return false;

        const char msg[] = "pipe test";
        DWORD written = 0;
        WriteFile(write_end, msg, sizeof(msg) - 1, &written, nullptr);
        CloseHandle(write_end);

        char buf[64]{};
        DWORD read = 0;
        ReadFile(read_end, buf, sizeof(buf), &read, nullptr);
        CloseHandle(read_end);

        return read == sizeof(msg) - 1 && std::memcmp(buf, msg, read) == 0;
    }

    bool test_named_pipe_roundtrip()
    {
        const char* name = "\\\\.\\pipe\\sogen-test-pipe";

        HANDLE server =
            CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, 1, 256, 256, 0, nullptr);
        if (server == INVALID_HANDLE_VALUE)
            return false;

        HANDLE client = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (client == INVALID_HANDLE_VALUE)
        {
            CloseHandle(server);
            return false;
        }

        const char msg[] = "named pipe";
        DWORD written = 0;
        WriteFile(server, msg, sizeof(msg) - 1, &written, nullptr);

        char buf[64]{};
        DWORD read = 0;
        ReadFile(client, buf, sizeof(buf), &read, nullptr);

        CloseHandle(client);
        CloseHandle(server);

        return read == sizeof(msg) - 1 && std::memcmp(buf, msg, read) == 0;
    }
}

int main()
{
    bool ok = true;
    ok &= report("Anonymous pipe read/write", test_anonymous_pipe());
    ok &= report("Named pipe roundtrip", test_named_pipe_roundtrip());
    return ok ? 0 : 1;
}
