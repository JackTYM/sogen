#include "std_include.hpp"
#include "steam_drm_ipc.hpp"

namespace sogen::steam_drm_ipc
{
    namespace
    {
        // Spelling matches Proton's steam_helper and the games that open these objects.
        constexpr std::u16string_view consume_semaphore = u"STEAM_DIPC_CONSUME";
        constexpr std::u16string_view produce_semaphore = u"SREAM_DIPC_PRODUCE";
        constexpr std::u16string_view start_ack_event_prefix = u"STEAM_START_ACK_EVENT";

        std::u16string_view leaf_name(const std::u16string_view name)
        {
            const auto separator = name.find_last_of(u'\\');
            return separator == std::u16string_view::npos ? name : name.substr(separator + 1);
        }

        bool launched_by_steam(const windows_emulator& win_emu)
        {
            return win_emu.get_application_settings().environment.contains(u"SteamGameId");
        }
    }

    void on_semaphore_released(const syscall_context& c, semaphore& released)
    {
        if (leaf_name(released.name) != consume_semaphore || !launched_by_steam(c.win_emu) || !released.try_lock())
        {
            return;
        }

        for (auto& entry : c.proc.events)
        {
            if (leaf_name(entry.second.name).starts_with(start_ack_event_prefix))
            {
                entry.second.signaled = true;
                break;
            }
        }

        for (auto& entry : c.proc.semaphores)
        {
            if (leaf_name(entry.second.name) == produce_semaphore)
            {
                entry.second.release(1);
                break;
            }
        }

        c.win_emu.notify_thread_readiness_change();
    }
}
