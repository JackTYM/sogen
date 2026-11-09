#pragma once

#ifdef _WIN32

#include <cstddef>
#include <string>

namespace sogen::utils::host_shm
{
    // Windows-host shared memory for sogen::shared_backing: pagefile-backed file-mapping objects in place of
    // POSIX shm_open/mmap. Handles are carried as `int` (kernel handle values fit in 32 bits, see
    // HandleToLong), so they travel through the same descriptor-shaped interfaces as POSIX file descriptors.
    // A returned handle is never inheritable; it reaches another sogen process only by DuplicateHandle (see
    // host_ipc). Kept out of the emulator headers so no translation unit has to mix <Windows.h> with the
    // emulator's own NT definitions.

    // Returns -1 on failure. The mapping is `size` bytes (callers round to the page size), zero filled.
    int create_anonymous(size_t size);

    // Creates a new `Local\<name>` with a fresh name of the POSIX shm shape ("sgn.<pid>.<counter>", at most 23
    // characters, which is what a kernel arena slot holds) and returns it in `name`. Returns -1 on failure.
    int create_unique_named(size_t size, std::string& name);

    // Opens an existing `Local\<name>`. Returns -1 when it does not exist.
    int open_named(const std::string& name);

    // Maps the whole object read/write. Returns nullptr on failure.
    void* map(int handle, size_t size);
    void unmap(void* data);
    void close(int handle);
}

#endif
