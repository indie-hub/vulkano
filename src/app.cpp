#include <vulkano/app.hpp>
#include <vulkano/window.hpp>

#include <chrono>
#include <thread>

namespace vulkano {

Application::Application(const AppConfig& config)
    : window {nullptr} {
    init_window(config);
}

Application::~Application() = default;

Application::Application(Application&&) noexcept = default;
Application& Application::operator=(Application&&) noexcept = default;

void Application::init_window(const AppConfig& config) {
    window = std::make_unique<Window>(config.title, config.width, config.height);
}

void Application::run() {
    main_loop();
}

void Application::main_loop() {
    while (!window->should_close()) {
        window->poll_events();
        // Placeholder: rendering will be added in subsequent steps.
        std::this_thread::sleep_for(std::chrono::milliseconds {16});
    }
}

void Application::run_for_ms(std::uint32_t milliseconds) noexcept {
    timed_loop(milliseconds);
}

void Application::timed_loop(std::uint32_t milliseconds) noexcept {
    const auto start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::milliseconds {milliseconds};
    while (!window->should_close()) {
        window->poll_events();
        std::this_thread::sleep_for(std::chrono::milliseconds {16});
        const auto now = std::chrono::steady_clock::now();
        if ((now - start) >= duration) {
            break;
        }
    }
}

} // namespace vulkano
