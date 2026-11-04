#include <gtest/gtest.h>

#include <windows-emulator/process_context.hpp>

namespace sogen::test
{
    TEST(GdiBoundsTest, FreshStateIsDisabledAndEmpty)
    {
        const gdi_bounds_state state{};
        EXPECT_EQ(state.flags(), gdi_bounds_state::disable_flag | gdi_bounds_state::reset_flag);
    }

    TEST(GdiBoundsTest, AccumulateUnionsRectangles)
    {
        gdi_bounds_state state{};
        const RECT first{10, 10, 20, 20};
        const RECT second{5, 15, 12, 40};

        state.update(gdi_bounds_state::accumulate_flag, &first);
        state.update(gdi_bounds_state::accumulate_flag, &second);

        EXPECT_EQ(state.bounds.left, 5);
        EXPECT_EQ(state.bounds.top, 10);
        EXPECT_EQ(state.bounds.right, 20);
        EXPECT_EQ(state.bounds.bottom, 40);
        EXPECT_EQ(state.flags() & gdi_bounds_state::accumulate_flag, gdi_bounds_state::accumulate_flag);
    }

    TEST(GdiBoundsTest, SetReplacesAndResetClears)
    {
        gdi_bounds_state state{};
        const RECT first{0, 0, 100, 100};
        const RECT second{10, 10, 20, 20};

        state.update(gdi_bounds_state::accumulate_flag, &first);
        state.update(gdi_bounds_state::reset_flag | gdi_bounds_state::accumulate_flag, &second);
        EXPECT_EQ(state.bounds.left, 10);
        EXPECT_EQ(state.bounds.right, 20);

        state.update(gdi_bounds_state::reset_flag, nullptr);
        EXPECT_EQ(state.flags() & gdi_bounds_state::reset_flag, gdi_bounds_state::reset_flag);
    }

    TEST(GdiBoundsTest, EnableAndDisableToggleTracking)
    {
        gdi_bounds_state state{};
        state.update(gdi_bounds_state::enable_flag, nullptr);
        EXPECT_EQ(state.flags() & gdi_bounds_state::enable_flag, gdi_bounds_state::enable_flag);
        state.update(gdi_bounds_state::disable_flag, nullptr);
        EXPECT_EQ(state.flags() & gdi_bounds_state::disable_flag, gdi_bounds_state::disable_flag);
    }

    TEST(GdiBoundsTest, UnorderedRectangleIsNormalized)
    {
        gdi_bounds_state state{};
        const RECT flipped{20, 20, 10, 10};
        state.update(gdi_bounds_state::accumulate_flag, &flipped);
        EXPECT_EQ(state.bounds.left, 10);
        EXPECT_EQ(state.bounds.bottom, 20);
    }
}
