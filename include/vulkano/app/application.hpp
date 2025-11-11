#pragma once

namespace vulkano::app {
class Application final {
public:
    Application() noexcept = default;
    ~Application() noexcept = default;

    [[nodiscard]] int run() noexcept;
};
} // namespace vulkano::app
