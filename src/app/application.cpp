#include <vulkano/app/application.hpp>

#include <vulkano/app/glfw_library.hpp>
#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>

namespace vulkano::app {
int Application::run() noexcept {
    try {
        GlfwLibrary glfw {};
        glfw.ensure_vulkan_support();

        Window window {"Vulkano Renderer", 1280U, 720U};
        VulkanContext context {window};

        while (!window.should_close()) {
            window.poll_events();
            std::this_thread::sleep_for(std::chrono::milliseconds {16});
        }

        context.wait_idle();
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "Application error: " << exception.what() << '\n';
    } catch (...) {
        std::cerr << "Application error: unknown exception" << '\n';
    }

    return EXIT_FAILURE;
}
} // namespace vulkano::app
