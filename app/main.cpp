#include <vulkano/app.hpp>

#include <cstdlib>
#include <string>

int main() {
    vulkano::AppConfig config { };
    config.title = std::string {"Vulkano – Vulkan/GLFW Skeleton"};
    config.width = 1280U;
    config.height = 720U;
    config.enable_vulkan = true;

    vulkano::Application app {config};
    if (const char* headless = std::getenv("HEADLESS_RUN_MS"); headless != nullptr) {
        const unsigned long ms = std::strtoul(headless, nullptr, 10);
        const std::uint32_t clamped = ms > 60000UL ? 60000U : static_cast<std::uint32_t>(ms);
        app.run_for_ms(clamped);
    } else {
        app.run();
    }
    return EXIT_SUCCESS;
}
