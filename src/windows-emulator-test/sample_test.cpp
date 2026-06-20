#include "emulation_test_utils.hpp"

namespace sogen::test
{
    TEST(SampleTest, ProcessInfoSamplePasses)
    {
        auto emu = create_app_emulator("C:\\process-info-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, ThreadInfoSamplePasses)
    {
        auto emu = create_app_emulator("C:\\thread-info-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, SectionSamplePasses)
    {
        auto emu = create_app_emulator("C:\\section-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, RegistryInfoSamplePasses)
    {
        auto emu = create_app_emulator("C:\\registry-info-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, SyncSamplePasses)
    {
        auto emu = create_app_emulator("C:\\sync-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, FileIoSamplePasses)
    {
        auto emu = create_app_emulator("C:\\file-io-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, SocketSamplePasses)
    {
        auto emu = create_app_emulator("C:\\socket-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, PipeSamplePasses)
    {
        auto emu = create_app_emulator("C:\\pipe-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, SecuritySamplePasses)
    {
        auto emu = create_app_emulator("C:\\security-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, TimerSamplePasses)
    {
        auto emu = create_app_emulator("C:\\timer-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, IoOpsSamplePasses)
    {
        auto emu = create_app_emulator("C:\\io-ops-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }

    TEST(SampleTest, MiscOpsSamplePasses)
    {
        auto emu = create_app_emulator("C:\\misc-ops-sample.exe");
        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);
    }
} // namespace sogen::test
