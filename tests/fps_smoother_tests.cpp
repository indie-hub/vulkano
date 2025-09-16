// fps_smoother_tests.cpp
#include <catch2/catch_all.hpp>
#include <vulkano/utils/fps.hpp>

TEST_CASE("FpsSmoother smooths towards samples") {
    vulkano::utils::FpsSmoother fps {0.5};
    fps.add_sample(10.0); // ms
    REQUIRE(fps.smoothed_ms() > 0.0);
    const double first = fps.smoothed_ms();
    fps.add_sample(20.0);
    REQUIRE(fps.smoothed_ms() > first);
    REQUIRE(fps.fps() > 0.0);
}

