#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace vulkano {

class Window;

struct AppConfig {
    std::string title;
    std::uint32_t width {1280};
    std::uint32_t height {720};
    bool enable_vulkan {true};
};

class Application final {
public:
    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) noexcept;
    Application& operator=(Application&&) noexcept;

    void run();
    void run_for_ms(std::uint32_t milliseconds) noexcept;

private:
    void init_window(const AppConfig& config);
    void main_loop();
    void timed_loop(std::uint32_t milliseconds) noexcept;

private:
    std::unique_ptr<Window> window;
    bool enable_vulkan {false};
};

} // namespace vulkano
