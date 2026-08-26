#include "emulation_test_utils.hpp"

namespace sogen::test
{
    TEST(SerializationTest, ResettingEmulatorWorks)
    {
        auto emu = create_sample_emulator();

        utils::buffer_serializer start_state{};
        emu.serialize(start_state);

        emu.start();

        ASSERT_TERMINATED_SUCCESSFULLY(emu);

        utils::buffer_serializer end_state1{};
        emu.serialize(end_state1);

        utils::buffer_deserializer deserializer{start_state};
        emu.deserialize(deserializer);

        emu.start();

        ASSERT_TERMINATED_SUCCESSFULLY(emu);

        utils::buffer_serializer end_state2{};
        emu.serialize(end_state2);

        ASSERT_EQ(end_state1.get_buffer(), end_state2.get_buffer());
    }

    TEST(SerializationTest, SerializedDataIsReproducible)
    {
        auto emu1 = create_sample_emulator();
        emu1.start();

        ASSERT_TERMINATED_SUCCESSFULLY(emu1);

        utils::buffer_serializer serializer1{};
        emu1.serialize(serializer1);

        utils::buffer_deserializer deserializer{serializer1};

        auto new_emu = create_empty_emulator();
        new_emu.deserialize(deserializer);

        utils::buffer_serializer serializer2{};
        new_emu.serialize(serializer2);

        auto buffer1 = serializer1.move_buffer();
        auto buffer2 = serializer2.move_buffer();

        ASSERT_EQ(serializer1.get_buffer(), serializer2.get_buffer());
    }

    TEST(SerializationTest, EmulationIsReproducible)
    {
        auto emu1 = create_sample_emulator();
        emu1.start();

        ASSERT_TERMINATED_SUCCESSFULLY(emu1);

        utils::buffer_serializer serializer1{};
        emu1.serialize(serializer1);

        auto emu2 = create_sample_emulator();
        emu2.start();

        ASSERT_TERMINATED_SUCCESSFULLY(emu2);

        utils::buffer_serializer serializer2{};
        emu2.serialize(serializer2);

        ASSERT_EQ(serializer1.get_buffer(), serializer2.get_buffer());
    }

    TEST(SerializationTest, DeserializedEmulatorBehavesLikeSource)
    {
        auto emu = create_sample_emulator();
        emu.start(100);

        utils::buffer_serializer serializer{};
        emu.serialize(serializer);

        utils::buffer_deserializer deserializer{serializer};

        auto new_emu = create_empty_emulator();
        new_emu.deserialize(deserializer);

        new_emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(new_emu);

        emu.start();
        ASSERT_TERMINATED_SUCCESSFULLY(emu);

        utils::buffer_serializer serializer1{};
        utils::buffer_serializer serializer2{};

        emu.serialize(serializer1);
        new_emu.serialize(serializer2);

        ASSERT_EQ(serializer1.get_buffer(), serializer2.get_buffer());
    }

    TEST(SerializationTest, ChildProcessRecordRoundTripsGrantedAccess)
    {
        process_context::child_process_record record{};
        record.image_path = windows_path{u"C:\\child.exe"};
        record.pid = 0x1234;
        record.exit_status = STATUS_PENDING;
        record.granted_access = 0x001FFFFF;

        utils::buffer_serializer serializer{};
        record.serialize(serializer);

        utils::buffer_deserializer deserializer{serializer};
        process_context::child_process_record restored{};
        restored.deserialize(deserializer);

        ASSERT_EQ(restored.image_path, record.image_path);
        ASSERT_EQ(restored.pid, record.pid);
        ASSERT_EQ(restored.exit_status, record.exit_status);
        ASSERT_EQ(restored.granted_access, record.granted_access);
    }
} // namespace sogen::test
