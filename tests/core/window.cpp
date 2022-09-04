// Disable some warnings for this file, because we test if the code is resilient against them
#pragma clang diagnostic push
#pragma ide diagnostic   ignored "bugprone-use-after-move"
#pragma ide diagnostic   ignored "performance-unnecessary-copy-initialization"

#include <test_framework/test_framework.hpp>
#include <vr_engine/core/global.h>
#include <vr_engine/core/window.h>

TEST
{
    vre::Window window;

    vre::Settings settings {
        vre::ApplicationInfo {
            "Test Application",
            {0, 1, 0},
        },
        {
            .enabled = true,
        },
    };

    ASSERT_NO_THROWS(window = vre::Window(settings));

    vre::Window window_copy = window;

    // It is a shared pointer, so both are valid
    EXPECT_TRUE(window.is_valid());
    EXPECT_TRUE(window_copy.is_valid());

    {
        vre::Window window_copy2 = window_copy;
        EXPECT_TRUE(window_copy2.is_valid());
        EXPECT_TRUE(window_copy.is_valid());
        EXPECT_TRUE(window.is_valid());
    }

    EXPECT_TRUE(window.is_valid());
    EXPECT_TRUE(window_copy.is_valid());

    vre::Window new_window = std::move(window);

    EXPECT_FALSE(window.is_valid());
    EXPECT_TRUE(new_window.is_valid());
    EXPECT_TRUE(window_copy.is_valid());

    // Destroy one of them
    window_copy.~Window();
    EXPECT_FALSE(window_copy.is_valid());
    EXPECT_TRUE(new_window.is_valid());

    // Destroy the other one
    new_window = {};
    EXPECT_FALSE(new_window.is_valid());
}
#pragma clang diagnostic pop