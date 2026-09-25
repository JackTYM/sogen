#include "emulation_test_utils.hpp"

namespace sogen::test
{
    TEST(ActivationContextBootstrap, UnmanifestedProcessLeavesActivationContextDataNull)
    {
        // test-sample.exe carries no manifest requesting any WinSxS dependency, so this must
        // stay the same as before build_activation_context_blob existed - no behavior change
        // for the common (unmanifested) case.
        auto emu = create_sample_emulator();

        const auto peb = emu.process.peb64.read();
        EXPECT_EQ(peb.ActivationContextData, 0u);
    }
} // namespace sogen::test
