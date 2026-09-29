#pragma once

#include <string>
#include <string_view>

namespace sogen
{
    // Canonical, case-insensitive lookup key for a named kernel object. Relative names live in the
    // caller's session base-named-objects directory (session 1 in this emulator); the `Global\` prefix
    // selects the global one, `Local\` and `Session\N\` the per-session ones - matching how kernelbase
    // resolves the names passed to CreateEvent/CreateMutex/OpenFileMapping and friends.
    std::u16string canonical_object_name(std::u16string_view name);

    bool object_names_equal(std::u16string_view left, std::u16string_view right);
}
