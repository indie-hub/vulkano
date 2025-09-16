#include <catch2/catch_all.hpp>
#include <chrono>

namespace {

// Simple moving average for testing purposes (will mirror app's smoothing)
class FpsAverager final {
public:
    explicit FpsAverager(const int window)
        : windowSize {window} {
    }

    void addSample(const double dt) {
        if (samples.size() == static_cast<std::size_t>(windowSize)) {
            sum -= samples.front();
            samples.erase(samples.begin());
        }
        samples.push_back(dt);
        sum += dt;
    }

    [[nodiscard]] double avgMs() const {
        if (samples.empty()) {
            return 0.0;
        }
        return (sum / static_cast<double>(samples.size())) * 1000.0;
    }

    [[nodiscard]] double fps() const {
        const double ms = avgMs();
        if (ms <= 0.0) {
            return 0.0;
        }
        return 1000.0 / ms;
    }

private:
    int windowSize {60};
    std::vector<double> samples {};
    double sum {0.0};
};

} // namespace

TEST_CASE("FpsAverager computes average ms and fps") {
    FpsAverager avg {4};
    avg.addSample(0.010); // 100 FPS
    avg.addSample(0.020); // 50 FPS
    avg.addSample(0.040); // 25 FPS
    avg.addSample(0.020); // 50 FPS

    const double ms = avg.avgMs();
    REQUIRE(ms == Approx(22.5));
    const double fps = avg.fps();
    REQUIRE(fps == Approx(44.444).margin(0.01));
}

