#include <vulkano/stats.hpp>

namespace vulkano {

Stats::Stats(float smoothingAlpha) noexcept : alpha_ {smoothingAlpha} {
    if (alpha_ < 0.0F) {
        alpha_ = 0.0F;
    }
    if (alpha_ > 1.0F) {
        alpha_ = 1.0F;
    }
}

void Stats::reset() noexcept {
    initialized_ = false;
    smoothed_dt_ms_ = 16.6667F;
}

void Stats::on_frame(clock::time_point now) noexcept {
    if (!initialized_) {
        last_ = now;
        initialized_ = true;
        return;
    }
    const auto dt = now - last_;
    last_ = now;
    const float dt_ms = std::chrono::duration<float, std::milli>(dt).count();
    smoothed_dt_ms_ = (alpha_ * dt_ms) + ((1.0F - alpha_) * smoothed_dt_ms_);
}

float Stats::fps() const noexcept {
    const float dt = smoothed_dt_ms_ <= 0.0F ? 16.6667F : smoothed_dt_ms_;
    return 1000.0F / dt;
}

float Stats::frame_ms() const noexcept {
    return smoothed_dt_ms_;
}

} // namespace vulkano

