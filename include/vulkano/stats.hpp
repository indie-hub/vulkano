#pragma once

#include <chrono>

namespace vulkano {

class Stats final {
public:
    using clock = std::chrono::steady_clock;

    explicit Stats(float smoothingAlpha = 0.1F) noexcept;
    Stats(const Stats&) = delete;
    Stats& operator=(const Stats&) = delete;
    Stats(Stats&&) = delete;
    Stats& operator=(Stats&&) = delete;
    ~Stats() = default;

    void reset() noexcept;
    void on_frame(clock::time_point now) noexcept;

    [[nodiscard]] float fps() const noexcept;
    [[nodiscard]] float frame_ms() const noexcept;

private:
    float alpha_ {0.1F};
    clock::time_point last_ {};
    float smoothed_dt_ms_ {16.6667F};
    bool initialized_ {false};
};

} // namespace vulkano

