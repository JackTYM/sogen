#include "emulation_test_utils.hpp"

#include <cross_process.hpp>
#include <syscall_utils.hpp>

namespace sogen::test
{
    namespace
    {
        class fake_control_channel final : public process_control_channel
        {
          public:
            std::optional<process_control_response> request(const process_control_request&, int /*timeout_ms*/) override
            {
                return std::nullopt;
            }

            std::optional<process_control_request> try_receive() override
            {
                return std::nullopt;
            }

            void respond(const process_control_response&) override
            {
            }
        };

        constexpr ACCESS_MASK PROCESS_VM_READ = 0x0010;
        constexpr ACCESS_MASK PROCESS_ALL_ACCESS = 0x001FFFFF;
        constexpr ACCESS_MASK MAXIMUM_ALLOWED = 0x02000000;

        syscall_context make_context(windows_emulator& emu)
        {
            return syscall_context{emu, emu.vcpu(0).cpu, emu.vcpu(0), emu.process};
        }
    }

    TEST(CrossProcessTest, NonChildHandleFallsThrough)
    {
        auto emu = create_empty_emulator();
        const auto c = make_context(emu);

        const auto result = resolve_child_target(c, handle{}, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, UnknownChildRecordIsInvalidHandle)
    {
        auto emu = create_empty_emulator();
        const auto c = make_context(emu);

        const auto h = make_pseudo_handle(42, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_INVALID_HANDLE);
    }

    TEST(CrossProcessTest, KnownChildWithoutLiveChannelIsNotSupported)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        emu.process.child_processes[7] = record;

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_NOT_SUPPORTED);
    }

    TEST(CrossProcessTest, InsufficientAccessIsDenied)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = 0;
        emu.process.child_processes[7] = record;
        emu.register_child_control_channel(7, std::make_unique<fake_control_channel>());

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<NTSTATUS>(result));
        ASSERT_EQ(std::get<NTSTATUS>(result), STATUS_ACCESS_DENIED);
    }

    TEST(CrossProcessTest, SufficientAccessResolvesProcessHandle)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        emu.process.child_processes[7] = record;
        emu.register_child_control_channel(7, std::make_unique<fake_control_channel>());
        auto* const registered = emu.find_child_control_channel(7);

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::process);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<child_target>(result));
        const auto& target = std::get<child_target>(result);
        ASSERT_EQ(target.record_id, 7u);
        ASSERT_EQ(target.channel, registered);
    }

    TEST(CrossProcessTest, SufficientAccessResolvesThreadHandleToSameRecord)
    {
        auto emu = create_empty_emulator();

        process_context::child_process_record record{};
        record.granted_access = PROCESS_ALL_ACCESS;
        emu.process.child_processes[7] = record;
        emu.register_child_control_channel(7, std::make_unique<fake_control_channel>());
        auto* const registered = emu.find_child_control_channel(7);

        const auto c = make_context(emu);
        const auto h = make_pseudo_handle(7, handle_types::thread);
        const auto result = resolve_child_target(c, h, PROCESS_VM_READ);

        ASSERT_TRUE(std::holds_alternative<child_target>(result));
        const auto& target = std::get<child_target>(result);
        ASSERT_EQ(target.record_id, 7u);
        ASSERT_EQ(target.channel, registered);
    }

    TEST(CrossProcessTest, ResolveGrantedProcessAccessPassesThroughSpecificMask)
    {
        ASSERT_EQ(resolve_granted_process_access(PROCESS_VM_READ), PROCESS_VM_READ);
    }

    TEST(CrossProcessTest, ResolveGrantedProcessAccessMapsMaximumAllowedToAllAccess)
    {
        ASSERT_EQ(resolve_granted_process_access(MAXIMUM_ALLOWED), PROCESS_ALL_ACCESS);
    }

    TEST(CrossProcessTest, ResolveGrantedProcessAccessMapsGenericAllToAllAccess)
    {
        ASSERT_EQ(resolve_granted_process_access(GENERIC_ALL), PROCESS_ALL_ACCESS);
    }
} // namespace sogen::test
