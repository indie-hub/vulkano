// Unit tests for FPS/frame time smoothing
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vulkano/stats.hpp>

TEST_CASE("stats_initial_values") {
    vulkano::Stats stats {0.1F};
    stats.reset();
    REQUIRE(stats.frame_ms() == Catch::Approx(16.6667F).margin(0.001F));
    REQUIRE(stats.fps() == Catch::Approx(60.0F).margin(0.1F));
}

TEST_CASE("stats_smoothing_behaviour") {
    vulkano::Stats stats {0.5F};
    stats.reset();
    auto t = vulkano::Stats::clock::now();
    // First frame initializes
    stats.on_frame(t);
    // Next frame: 20 ms later
    t += std::chrono::milliseconds {20};
    stats.on_frame(t);
    // With alpha=0.5, smoothed becomes (0.5*20 + 0.5*16.6667) ~= 18.33335
    REQUIRE(stats.frame_ms() == Catch::Approx(18.33335F).margin(0.2F));
    // Another frame: 20 ms later -> new smoothed ~= (0.5*20 + 0.5*18.33335) ~= 19.166675
    t += std::chrono::milliseconds {20};
    stats.on_frame(t);
    REQUIRE(stats.frame_ms() == Catch::Approx(19.1667F).margin(0.3F));
}
