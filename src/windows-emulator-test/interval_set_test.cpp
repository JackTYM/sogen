#include <gtest/gtest.h>

#include <utils/interval_set.hpp>

#include <utility>
#include <vector>

namespace sogen::test
{
    namespace
    {
        std::vector<std::pair<uint64_t, uint64_t>> runs_of(const utils::interval_set& set)
        {
            return {set.runs().begin(), set.runs().end()};
        }

        using runs = std::vector<std::pair<uint64_t, uint64_t>>;
    }

    TEST(IntervalSetTest, InsertMergesOverlappingAndAdjacentRuns)
    {
        utils::interval_set set{};
        set.insert(0x10, 0x20);
        set.insert(0x30, 0x40);
        EXPECT_EQ(runs_of(set), (runs{{0x10, 0x20}, {0x30, 0x40}}));

        set.insert(0x20, 0x30);
        EXPECT_EQ(runs_of(set), (runs{{0x10, 0x40}}));

        set.insert(0x0, 0x18);
        set.insert(0x38, 0x50);
        EXPECT_EQ(runs_of(set), (runs{{0x0, 0x50}}));

        set.insert(0x60, 0x60);
        EXPECT_EQ(runs_of(set), (runs{{0x0, 0x50}}));
    }

    TEST(IntervalSetTest, EraseSplitsAndTrimsRuns)
    {
        utils::interval_set set{};
        set.insert(0x0, 0x100);

        set.erase(0x40, 0x80);
        EXPECT_EQ(runs_of(set), (runs{{0x0, 0x40}, {0x80, 0x100}}));

        set.erase(0x0, 0x10);
        EXPECT_EQ(runs_of(set), (runs{{0x10, 0x40}, {0x80, 0x100}}));

        set.erase(0x30, 0x90);
        EXPECT_EQ(runs_of(set), (runs{{0x10, 0x30}, {0x90, 0x100}}));

        set.erase(0x0, 0x200);
        EXPECT_TRUE(set.empty());
    }

    TEST(IntervalSetTest, LookupsReportContainingAndNextRuns)
    {
        utils::interval_set set{};
        set.insert(0x10, 0x20);
        set.insert(0x40, 0x50);

        EXPECT_FALSE(set.contains(0x0F));
        EXPECT_TRUE(set.contains(0x10));
        EXPECT_TRUE(set.contains(0x1F));
        EXPECT_FALSE(set.contains(0x20));

        EXPECT_EQ(set.run_containing(0x45), (std::pair<uint64_t, uint64_t>{0x40, 0x50}));
        EXPECT_FALSE(set.run_containing(0x30).has_value());

        EXPECT_EQ(set.next_run_start(0x20), 0x40u);
        EXPECT_EQ(set.next_run_start(0x0), 0x10u);
        EXPECT_FALSE(set.next_run_start(0x40).has_value());

        runs overlaps{};
        set.for_each_overlap(0x18, 0x48, [&](const uint64_t start, const uint64_t end) { overlaps.emplace_back(start, end); });
        EXPECT_EQ(overlaps, (runs{{0x18, 0x20}, {0x40, 0x48}}));
    }
}
