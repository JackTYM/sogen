#include "emulation_test_utils.hpp"
#include <syscall_utils.hpp>

namespace sogen::syscalls
{
    BOOL handle_NtUserGetCursorPos(const syscall_context& c, emulator_pointer point_ptr);
}

namespace sogen::test
{
    TEST(CursorPositionTest, CursorStartsAtCentreOfPrimaryDisplay)
    {
        auto emu = create_empty_emulator();
        const auto point = emu.memory.allocate_memory(0x1000, memory_permission::read_write);

        const syscall_context context{.win_emu = emu, .emu = emu.vcpu(0).cpu, .vcpu = emu.vcpu(0), .proc = emu.process};
        ASSERT_TRUE(syscalls::handle_NtUserGetCursorPos(context, point));

        std::array<int32_t, 2> position{};
        emu.memory.read_memory(point, position.data(), sizeof(position));
        EXPECT_EQ(static_cast<int32_t>(emu.process.display_width / 2), position[0]);
        EXPECT_EQ(static_cast<int32_t>(emu.process.display_height / 2), position[1]);
    }
}
