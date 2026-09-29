#include "emulation_test_utils.hpp"

#include <shared_backing.hpp>

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
#include <child_process_spawn.hpp>

#include <atomic>
#include <cstring>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>
#endif

namespace sogen::test
{
    TEST(SharedBackingTest, HeapBackingIsZeroedAndNotShared)
    {
        const auto backing = shared_backing::create_heap(100);
        ASSERT_EQ(backing->size(), 100u);
        ASSERT_FALSE(backing->is_shared());
        ASSERT_EQ(backing->native_fd(), -1);
        for (size_t i = 0; i < backing->size(); ++i)
        {
            ASSERT_EQ(backing->data()[i], std::byte{0});
        }
    }

    TEST(SharedBackingTest, CreateFromContentRoundTrips)
    {
        const std::vector<std::byte> content{std::byte{1}, std::byte{2}, std::byte{3}};
        const auto backing = shared_backing::create_from_content(content);
        ASSERT_EQ(backing->content(), content);
    }

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN)
    TEST(SharedBackingTest, SharedBackingIsBackedByADescriptor)
    {
        const auto backing = shared_backing::create(0x5000);
        ASSERT_TRUE(backing->is_shared());
        ASSERT_GE(backing->native_fd(), 0);
        ASSERT_EQ(backing->size(), 0x5000u);

        const auto second_mapping = shared_backing::adopt_fd(::dup(backing->native_fd()), backing->size());
        ASSERT_NE(second_mapping, nullptr);
        ASSERT_NE(second_mapping->data(), backing->data());

        backing->data()[0x4fff] = std::byte{0x5a};
        ASSERT_EQ(second_mapping->data()[0x4fff], std::byte{0x5a});
        second_mapping->data()[3] = std::byte{0xa5};
        ASSERT_EQ(backing->data()[3], std::byte{0xa5});
    }

    namespace
    {
        struct socket_pair
        {
            std::unique_ptr<process_control_channel> client{};
            std::unique_ptr<process_control_channel> server{};

            socket_pair()
            {
                int fds[2]{-1, -1};
                EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
                this->client = create_fd_process_control_channel(fds[0]);
                this->server = create_fd_process_control_channel(fds[1]);
            }
        };

        std::optional<process_control_request> wait_for_request(process_control_channel& server)
        {
            for (int i = 0; i < 5000; ++i)
            {
                if (auto request = server.try_receive())
                {
                    return request;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return std::nullopt;
        }
    }

    TEST(SharedBackingTest, ControlChannelSharesSectionBackingBothWays)
    {
        socket_pair channels{};

        const auto backing = shared_backing::create(0x2000);
        std::memset(backing->data(), 0x11, backing->size());

        process_control_request request{};
        request.op = process_control_op::adopt_object;
        request.maximum_size = backing->size();
        request.backing = backing;

        std::optional<process_control_response> response{};
        std::thread client_thread([&] { response = channels.client->request(request, 10000); });

        const auto received = wait_for_request(*channels.server);
        ASSERT_TRUE(received.has_value());
        ASSERT_NE(received->backing, nullptr);
        ASSERT_TRUE(received->backing->is_shared());
        ASSERT_EQ(received->backing->size(), backing->size());
        ASSERT_NE(received->backing->data(), backing->data());
        ASSERT_EQ(received->backing->data()[0x1fff], std::byte{0x11});

        received->backing->data()[0x10] = std::byte{0x77};

        process_control_response reply{};
        reply.request_id = received->request_id;
        reply.status = STATUS_SUCCESS;
        reply.backing = received->backing;
        channels.server->respond(reply);

        client_thread.join();
        ASSERT_TRUE(response.has_value());
        ASSERT_EQ(response->status, STATUS_SUCCESS);
        ASSERT_NE(response->backing, nullptr);
        ASSERT_EQ(backing->data()[0x10], std::byte{0x77});

        backing->data()[0x20] = std::byte{0x33};
        ASSERT_EQ(received->backing->data()[0x20], std::byte{0x33});
        ASSERT_EQ(response->backing->data()[0x20], std::byte{0x33});
    }

    TEST(SharedBackingTest, ControlChannelFallsBackToPayloadForHeapBacking)
    {
        socket_pair channels{};

        process_control_request request{};
        request.op = process_control_op::adopt_object;
        request.backing = shared_backing::create_heap(8);
        request.payload = {std::byte{9}, std::byte{8}};

        std::optional<process_control_response> response{};
        std::thread client_thread([&] { response = channels.client->request(request, 10000); });

        const auto received = wait_for_request(*channels.server);
        ASSERT_TRUE(received.has_value());
        ASSERT_EQ(received->backing, nullptr);
        ASSERT_EQ(received->payload, request.payload);

        process_control_response reply{};
        reply.request_id = received->request_id;
        channels.server->respond(reply);
        client_thread.join();
        ASSERT_TRUE(response.has_value());
    }

    TEST(SharedBackingTest, BootstrapPassesInheritedSectionsAsSharedMappings)
    {
        int fds[2]{-1, -1};
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

        const auto shared = shared_backing::create(0x1000);
        shared->data()[5] = std::byte{0x42};
        const auto heap = shared_backing::create_from_content({std::byte{7}, std::byte{6}});

        std::vector<inherited_section_handle> sections{};
        sections.push_back({.maximum_size = 0x1000, .granted_access = 1, .backing = shared});
        sections.push_back({.maximum_size = 2, .granted_access = 2, .backing = heap});

        ASSERT_TRUE(send_child_bootstrap_data(fds[0], {}, {}, sections, {}));
        const auto received = receive_child_bootstrap_data(fds[1]);
        ASSERT_TRUE(received.has_value());
        ASSERT_EQ(received->inherited_sections.size(), 2u);

        const auto& first = received->inherited_sections[0];
        ASSERT_NE(first.backing, nullptr);
        ASSERT_TRUE(first.backing->is_shared());
        ASSERT_EQ(first.backing->data()[5], std::byte{0x42});
        shared->data()[6] = std::byte{0x43};
        ASSERT_EQ(first.backing->data()[6], std::byte{0x43});

        const auto& second = received->inherited_sections[1];
        ASSERT_NE(second.backing, nullptr);
        ASSERT_FALSE(second.backing->is_shared());
        ASSERT_EQ(second.backing->content(), heap->content());

        ::close(fds[0]);
        ::close(fds[1]);
    }
#endif
}
