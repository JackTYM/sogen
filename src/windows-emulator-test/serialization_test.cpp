#include "emulation_test_utils.hpp"

#include <utils/io.hpp>

namespace sogen::test
{
    namespace
    {
        void dump_and_expect_equal(const char* label, const std::vector<std::byte>& a, const std::vector<std::byte>& b)
        {
            if (a == b)
            {
                return;
            }

            utils::io::write_file(std::string(label) + ".a.bin", a);
            utils::io::write_file(std::string(label) + ".b.bin", b);

            ADD_FAILURE() << label << ": serialized buffers differ (dumps written to the working directory)";
        }
    }

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

        dump_and_expect_equal("ResettingEmulatorWorks", end_state1.get_buffer(), end_state2.get_buffer());
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

        dump_and_expect_equal("SerializedDataIsReproducible", serializer1.get_buffer(), serializer2.get_buffer());
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

        dump_and_expect_equal("EmulationIsReproducible", serializer1.get_buffer(), serializer2.get_buffer());
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

        dump_and_expect_equal("DeserializedEmulatorBehavesLikeSource", serializer1.get_buffer(), serializer2.get_buffer());
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

    TEST(SerializationTest, InheritedProcessHandleRoundTrips)
    {
        inherited_process_handle inherited{};
        inherited.target_handle = make_pseudo_handle(9, handle_types::process);
        inherited.pid = 0x1234;
        inherited.exit_status = STATUS_PENDING;
        inherited.granted_access = 0x1410;
        inherited.image_path = windows_path{u"C:\\browser.exe"};

        utils::buffer_serializer serializer{};
        serializer.write(inherited);

        utils::buffer_deserializer deserializer{serializer};
        inherited_process_handle restored{};
        deserializer.read(restored);

        ASSERT_EQ(restored.target_handle.bits, inherited.target_handle.bits);
        ASSERT_EQ(restored.pid, inherited.pid);
        ASSERT_EQ(restored.exit_status, inherited.exit_status);
        ASSERT_EQ(restored.granted_access, inherited.granted_access);
        ASSERT_EQ(restored.image_path, inherited.image_path);
    }
} // namespace sogen::test
