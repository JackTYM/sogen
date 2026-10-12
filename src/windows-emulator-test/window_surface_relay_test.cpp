#include "emulation_test_utils.hpp"

#include <window_surface_relay.hpp>

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
#include <child_process_spawn.hpp>
#include <pipe_ipc_channel.hpp>

#include <chrono>
#include <thread>

#include <sys/socket.h>
#endif

namespace sogen::test
{
    namespace
    {
        std::vector<uint32_t> make_frame(const int width, const int height, const uint32_t seed)
        {
            std::vector<uint32_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
            for (size_t i = 0; i < pixels.size(); ++i)
            {
                pixels[i] = seed + static_cast<uint32_t>(i);
            }
            return pixels;
        }

        ui_surface_desc describe(const std::vector<uint32_t>& pixels, const int width, const int height)
        {
            return ui_surface_desc{.width = width,
                                   .height = height,
                                   .stride = width * static_cast<int>(sizeof(uint32_t)),
                                   .format = ui_surface_format::bgra8,
                                   .pixels = pixels.data()};
        }

        void expect_pixels(const ui_surface_desc& surface, const std::vector<uint32_t>& expected)
        {
            ASSERT_NE(surface.pixels, nullptr);
            ASSERT_EQ(std::memcmp(surface.pixels, expected.data(), expected.size() * sizeof(uint32_t)), 0);
        }
    }

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
    TEST(WindowSurfaceRelayTest, FramesTravelThroughSharedMemory)
    {
        window_surface_exporter exporter{};
        window_surface_importer importer{};

        const auto first = make_frame(16, 8, 1);
        window_surface_update first_update{.layer = 0x1234};
        const auto first_inline = exporter.publish(first_update, describe(first, 16, 8));
        ASSERT_FALSE(first_inline.has_value());
        ASSERT_FALSE(first_update.shm_name.empty());

        const auto second = make_frame(16, 8, 1000);
        window_surface_update second_update{.layer = 0x1234};
        ASSERT_FALSE(exporter.publish(second_update, describe(second, 16, 8)).has_value());
        ASSERT_EQ(second_update.shm_name, first_update.shm_name);
        ASSERT_NE(second_update.offset, first_update.offset);

        const auto first_surface = importer.resolve(first_update, {});
        ASSERT_TRUE(first_surface.has_value());
        expect_pixels(*first_surface, first);

        const auto second_surface = importer.resolve(second_update, {});
        ASSERT_TRUE(second_surface.has_value());
        ASSERT_EQ(second_surface->width, 16);
        ASSERT_EQ(second_surface->height, 8);
        expect_pixels(*second_surface, second);
    }

    TEST(WindowSurfaceRelayTest, LargerFrameMovesToANewRing)
    {
        window_surface_exporter exporter{};
        window_surface_importer importer{};

        const auto small = make_frame(4, 4, 7);
        window_surface_update small_update{};
        ASSERT_FALSE(exporter.publish(small_update, describe(small, 4, 4)).has_value());

        const auto large = make_frame(32, 32, 9);
        window_surface_update large_update{};
        ASSERT_FALSE(exporter.publish(large_update, describe(large, 32, 32)).has_value());
        ASSERT_NE(large_update.shm_name, small_update.shm_name);

        const auto surface = importer.resolve(large_update, {});
        ASSERT_TRUE(surface.has_value());
        expect_pixels(*surface, large);
    }

    TEST(WindowSurfaceRelayTest, UpdateSurvivesThePipeChannel)
    {
        int fds[2]{-1, -1};
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
        const auto sender = create_fd_pipe_ipc_channel(fds[0]);
        const auto receiver = create_fd_pipe_ipc_channel(fds[1]);

        window_surface_exporter exporter{};
        const auto frame = make_frame(8, 2, 42);
        window_surface_update update{.layer = 0x685001c,
                                     .destination = {.left = 1, .top = 2, .right = 9, .bottom = 4},
                                     .clip = {.left = 1, .top = 2, .right = 5, .bottom = 4},
                                     .visible = true};
        ASSERT_FALSE(exporter.publish(update, describe(frame, 8, 2)).has_value());

        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::window_surface;
        message.window = 0x6820034;
        message.surface = update;
        sender->send(message);

        std::optional<pipe_ipc_message> received{};
        for (int i = 0; i < 5000 && !received; ++i)
        {
            received = receiver->try_receive();
            if (!received)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        ASSERT_TRUE(received.has_value());
        ASSERT_EQ(received->type, pipe_ipc_message_type::window_surface);
        ASSERT_EQ(received->window, 0x6820034u);
        ASSERT_TRUE(received->surface.has_value());
        ASSERT_EQ(received->surface->layer, 0x685001cu);
        ASSERT_EQ(received->surface->destination.right, 9);
        ASSERT_EQ(received->surface->clip.right, 5);
        ASSERT_TRUE(received->surface->visible);

        window_surface_importer importer{};
        const auto surface = importer.resolve(*received->surface, received->data);
        ASSERT_TRUE(surface.has_value());
        expect_pixels(*surface, frame);
    }
#endif

    TEST(WindowSurfaceRelayTest, InlinePixelsResolveWithoutSharedMemory)
    {
        const auto frame = make_frame(4, 3, 5);
        const std::string inline_pixels(reinterpret_cast<const char*>(frame.data()), frame.size() * sizeof(uint32_t));

        const window_surface_update update{.width = 4, .height = 3, .stride = 16};

        window_surface_importer importer{};
        const auto surface = importer.resolve(update, inline_pixels);
        ASSERT_TRUE(surface.has_value());
        expect_pixels(*surface, frame);

        ASSERT_FALSE(importer.resolve(update, inline_pixels.substr(0, 8)).has_value());
    }
}
