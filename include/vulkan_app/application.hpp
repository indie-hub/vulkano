#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace vulkan_app {

class Application final {
public:
    Application();
    ~Application() noexcept;
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void run();

private:
    void init_window();
    void init_vulkan();
    void main_loop();

    GLFWwindow *window{};
    VkInstance instance{};
};

} // namespace vulkan_app
