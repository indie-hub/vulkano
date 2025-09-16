#include <vulkano_codex/utils.hpp>

namespace vulkano_codex {

void FpsAverager::reset() noexcept {
    last_ = clock::now();
    avg_frame_ms_ = 0.0;
    initialized_ = false;
}

void FpsAverager::tick() noexcept {
    const auto now = clock::now();
    const auto dt = std::chrono::duration<double, std::milli>(now - last_).count();
    last_ = now;
    if (!initialized_) {
        avg_frame_ms_ = dt;
        initialized_ = true;
    } else {
        avg_frame_ms_ = smoothing_ * dt + (1.0 - smoothing_) * avg_frame_ms_;
    }
}

double FpsAverager::frame_ms() const noexcept {
    return avg_frame_ms_;
}

double FpsAverager::fps() const noexcept {
    return (avg_frame_ms_ > 0.0) ? (1000.0 / avg_frame_ms_) : 0.0;
}

} // namespace vulkano_codex
