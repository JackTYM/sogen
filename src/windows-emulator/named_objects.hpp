#pragma once

#include "std_include.hpp"
#include "object_name.hpp"
#include "process_context.hpp"

namespace sogen
{
    // Cross-process lookup and publication of named synchronization objects. The per-process handle
    // tables only know the objects their own process created; the kernel arena's registry (see
    // kernel_state.hpp) is what lets `OpenEvent(L"Global\\Foo")` find an object another process of the
    // tree created, the way the single shared object manager namespace does on Windows.
    class named_objects
    {
      public:
        using kind = kernel_arena::named_object_kind;

        // Looks `name` up in the shared registry. On a hit the found slot is bound to `state` (which then
        // shares its words with every other holder). STATUS_OBJECT_NAME_NOT_FOUND when nothing is
        // registered, STATUS_OBJECT_TYPE_MISMATCH when the name belongs to another kind of object.
        static NTSTATUS open(process_context& process, kind expected, std::u16string_view name, kernel_state& state);

        // Publishes a freshly created object's `state` under `name`. If another process published the
        // name first, `state` is rebound to that object instead and STATUS_OBJECT_NAME_EXISTS is returned.
        static NTSTATUS publish(process_context& process, kind object_kind, std::u16string_view name, kernel_state& state);

        // Named pagefile-backed sections. Their pages are a named shm object (so processes that never
        // received a descriptor can still map them) whose name, size and protection live in the section's
        // arena slot, and which is unlinked once the last section object in the tree referencing it is gone.
        //
        // publish_section gives a freshly created section named backing and registers it (returning
        // STATUS_OBJECT_NAME_EXISTS after rebinding to the winner when another process registered the name
        // first); open_section fills `section` from a registered one.
        static bool is_shareable_named_section(const section_object& section);
        static NTSTATUS publish_section(process_context& process, section_object& section);
        static NTSTATUS open_section(process_context& process, std::u16string_view name, section_object& section);
    };
}
