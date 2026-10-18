#include "rpc_test_utils.hpp"

#include <chrono>
#include <thread>

namespace sogen::test
{
    namespace
    {
        constexpr std::array<uint8_t, 16> audio_client_interface{0x11, 0xd1, 0x74, 0xd5, 0x26, 0x61, 0xd7, 0x49,
                                                                 0x9b, 0x86, 0x4d, 0xe6, 0xb6, 0x50, 0xd4, 0xfc};
        constexpr std::array<uint8_t, 16> mmdevice_interface{0xb3, 0x85, 0x3f, 0x92, 0xee, 0xbb, 0xdf, 0x4e,
                                                             0x80, 0x59, 0xf5, 0x69, 0xfa, 0x64, 0xa0, 0x27};

        constexpr uint32_t device_state_active = 1;

        constexpr uint32_t opnum_open_stream = 4;
        constexpr uint32_t opnum_create_stream = 7;
        constexpr uint32_t opnum_get_default_endpoint = 25;

        constexpr size_t rpc_reply_header_size = 0x18;
        constexpr size_t open_stream_format_offset = 152;
        constexpr size_t create_stream_duration_offset = 24;
        constexpr uint64_t write_cursor_offset = 0x10;
        constexpr uint64_t play_cursor_offset = 0x18;

        class AudioServiceTest : public RpcProtocolTest
        {
          protected:
            void SetUp() override
            {
                RpcProtocolTest::SetUp();
                this->emu->registry = registry_manager{get_emulator_root() / "registry"};
            }

            void add_endpoint(const std::string_view name, const uint32_t device_state)
            {
                const auto key = this->emu->registry.create_key(
                    std::filesystem::path{R"(\Registry\Machine\Software\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\)"} /
                    std::string{name});
                ASSERT_TRUE(key.has_value());
                this->emu->registry.set_value(*key, "DeviceState", REG_DWORD, std::as_bytes(std::span{&device_state, 1}));
            }
        };

        std::vector<uint8_t> float_stereo_open_stream_request()
        {
            struct wave_format_extensible
            {
                uint16_t tag;
                uint16_t channels;
                uint32_t sample_rate;
                uint32_t average_bytes_per_second;
                uint16_t block_align;
                uint16_t bits_per_sample;
                uint16_t extra_size;
                uint16_t valid_bits_per_sample;
                uint32_t channel_mask;
                uint32_t subtype;
                std::array<uint8_t, 12> subtype_tail;
            };

            const wave_format_extensible format{.tag = 0xFFFE,
                                                .channels = 2,
                                                .sample_rate = 44100,
                                                .average_bytes_per_second = 44100 * 8,
                                                .block_align = 8,
                                                .bits_per_sample = 32,
                                                .extra_size = 22,
                                                .valid_bits_per_sample = 32,
                                                .channel_mask = 3,
                                                .subtype = 3,
                                                .subtype_tail = {0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

            std::vector<uint8_t> request(open_stream_format_offset + sizeof(format));
            std::memcpy(request.data() + open_stream_format_offset, &format, sizeof(format));
            return request;
        }
    }

    TEST_F(AudioServiceTest, DefaultEndpointReportsActiveDeviceState)
    {
        this->add_endpoint("{0.0.0.00000000}.{11111111-1111-1111-1111-111111111111}", device_state_active);
        this->bind(u"\\RPC Control\\Audiosrv", mmdevice_interface, 8);

        const auto reply = this->call(opnum_get_default_endpoint, std::array<uint8_t, 4>{});
        ASSERT_EQ(STATUS_SUCCESS, reply.status);
        ASSERT_TRUE(reply.payload.has_value());
        ASSERT_GE(reply.payload->size(), rpc_reply_header_size + 2 * sizeof(uint32_t));

        const auto reply_end = reply.payload->size();
        EXPECT_EQ(device_state_active, read<uint32_t>(*reply.payload, reply_end - 2 * sizeof(uint32_t)));
    }

    TEST_F(AudioServiceTest, RenderStreamPlayCursorFollowsTheWriteCursor)
    {
        this->bind(u"\\RPC Control\\Audiosrv", audio_client_interface, 8);

        const auto open_request = float_stereo_open_stream_request();
        ASSERT_EQ(STATUS_SUCCESS, this->call(opnum_open_stream, open_request).status);

        std::array<uint8_t, create_stream_duration_offset + sizeof(uint64_t)> create_request{};
        const uint64_t duration_hns = 1'000'000;
        std::memcpy(create_request.data() + create_stream_duration_offset, &duration_hns, sizeof(duration_hns));
        const auto created = this->call(opnum_create_stream, create_request);
        ASSERT_EQ(STATUS_SUCCESS, created.status);
        ASSERT_EQ(1u, created.handles.size());

        handle section_handle{};
        section_handle.bits = created.handles.front().handle;
        const auto* section = this->emu->process.sections.get(section_handle);
        ASSERT_NE(nullptr, section);
        const auto buffer = section->backing_address;
        ASSERT_NE(0u, buffer);

        constexpr uint64_t bytes_written = 0x400;
        this->emu->memory.write_memory(buffer + write_cursor_offset, bytes_written);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        uint64_t play_cursor{};
        while (play_cursor != bytes_written && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            this->emu->memory.read_memory(buffer + play_cursor_offset, &play_cursor, sizeof(play_cursor));
        }

        EXPECT_EQ(bytes_written, play_cursor);
    }
}
