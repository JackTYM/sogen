#include <gtest/gtest.h>

#include <snapshot_manifest.hpp>
#include <utils/serialization.hpp>

#include <stdexcept>

namespace sogen::test
{
    namespace
    {
        snapshot::child_process_manifest round_trip(const snapshot::child_process_manifest& manifest)
        {
            utils::buffer_serializer serializer{};
            serializer.write_vector(manifest);

            utils::buffer_deserializer deserializer{serializer.get_buffer()};
            snapshot::child_process_manifest result{};
            deserializer.read_vector(result);
            return result;
        }

        snapshot::child_process_snapshot running_child()
        {
            return {.process = {.process = managed_process{1}, .process_id = 0x1234, .thread_id = 0x1235}, .file = "app.child-4660.snap"};
        }

        snapshot::child_process_snapshot exited_child()
        {
            return {.process = {.process = managed_process{2},
                                .process_id = 0x2000,
                                .thread_id = 0x2001,
                                .exit = process_exit{.kind = process_exit_kind::terminated, .code = 5}}};
        }
    }

    TEST(SnapshotManifestTest, RunningAndExitedChildrenSurviveSerialization)
    {
        const auto children = round_trip({running_child(), exited_child()});

        ASSERT_EQ(children.size(), 2U);
        EXPECT_EQ(children[0].process.process, managed_process{1});
        EXPECT_EQ(children[0].process.process_id, 0x1234U);
        EXPECT_EQ(children[0].process.thread_id, 0x1235U);
        EXPECT_FALSE(children[0].process.exit.has_value());
        EXPECT_EQ(children[0].file, "app.child-4660.snap");

        EXPECT_EQ(children[1].process.process, managed_process{2});
        ASSERT_TRUE(children[1].process.exit.has_value());
        EXPECT_EQ(children[1].process.exit->kind, process_exit_kind::terminated);
        EXPECT_EQ(children[1].process.exit->code, 5U);
        EXPECT_TRUE(children[1].file.empty());
    }

    TEST(SnapshotManifestTest, RejectsARunningChildWithoutAFileAndAnExitedChildWithOne)
    {
        auto running = running_child();
        running.file.clear();
        EXPECT_THROW(round_trip({running}), std::runtime_error);

        auto exited = exited_child();
        exited.file = "app.child-8192.snap";
        EXPECT_THROW(round_trip({exited}), std::runtime_error);
    }

    TEST(SnapshotManifestTest, AChildSnapshotPathResolvesBackFromItsFileName)
    {
        const std::filesystem::path parent = std::filesystem::path("snapshots") / "app.snap";
        const auto child = snapshot::get_child_snapshot_path(parent, 4660);

        EXPECT_EQ(child.parent_path(), parent.parent_path());
        EXPECT_EQ(snapshot::resolve_child_snapshot(parent, child.filename().string()), child);
    }

    TEST(SnapshotManifestTest, ChildFilesOutsideTheParentsDirectoryAreRejected)
    {
        const std::filesystem::path parent = std::filesystem::path("snapshots") / "app.snap";

        for (const auto* file : {"", ".", "..", "../app.child-1.snap", "nested/app.child-1.snap", "/tmp/app.child-1.snap"})
        {
            EXPECT_THROW(snapshot::resolve_child_snapshot(parent, file), std::runtime_error) << file;
        }
    }
}
