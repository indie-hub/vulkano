#pragma once

#include <chrono>

namespace vulkano_codex {

class FpsAverager final {
public:
    FpsAverager() noexcept = default;

    void reset() noexcept {
        last_ = clock::now();
        avg_frame_ms_ = 0.0;
        initialized_ = false;
    }

    void tick() noexcept {
        const auto now = clock::now();
        const auto dt = std::chrono::duration<double, std::milli>(now - last_).count();
        last_ = now;
        if (!initialized_) {
            avg_frame_ms_ = dt;
            initialized_ = true;
        } else {
            // Exponential moving average
            avg_frame_ms_ = smoothing_ * dt + (1.0 - smoothing_) * avg_frame_ms_;
        }
    }

    [[nodiscard]] double frame_ms() const noexcept {
        return avg_frame_ms_;
    }

    [[nodiscard]] double fps() const noexcept {
        return (avg_frame_ms_ > 0.0) ? (1000.0 / avg_frame_ms_) : 0.0;
    }

private:
    using clock = std::chrono::steady_clock;
    clock::time_point last_ {clock::now()};
    double avg_frame_ms_ {0.0};
    bool initialized_ {false};
    static constexpr double smoothing_ {0.1};
};

} // namespace vulkano_codex

