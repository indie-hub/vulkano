#pragma once

#include <chrono>

namespace vulkano_codex {

class FpsAverager final {
public:
    FpsAverager() noexcept = default;

    void reset() noexcept;
    void tick() noexcept;
    [[nodiscard]] double frame_ms() const noexcept;
    [[nodiscard]] double fps() const noexcept;

private:
    using clock = std::chrono::steady_clock;
    clock::time_point last_ {clock::now()};
    double avg_frame_ms_ {0.0};
    bool initialized_ {false};
    static constexpr double smoothing_ {0.1};
};

} // namespace vulkano_codex
