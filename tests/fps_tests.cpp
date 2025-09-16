#include <catch2/catch_all.hpp>
#include <vulkano_codex/utils.hpp>

#include <thread>

TEST_CASE("FpsAverager computes reasonable fps") {
    vulkano_codex::FpsAverager fps {};
    fps.reset();
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        fps.tick();
    }
    REQUIRE(fps.frame_ms() > 0.0);
    REQUIRE(fps.fps() > 0.0);
}

