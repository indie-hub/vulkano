#include <cstdlib>
#include <exception>
#include <iostream>

#include <vulkano/app_config.hpp>
#include <vulkano/glfw_context.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>
#include <vulkano/swapchain.hpp>

auto main() -> int {
    try {
        const auto config = vulkano::AppConfigBuilder {}.build();
        const vulkano::GlfwContext glfwContext {};
        auto window = vulkano::Window::create(glfwContext, config);
        auto context = vulkano::VulkanContextBuilder {}
            .with_config(config)
            .with_window(window)
            .build();

        auto swapchain = vulkano::SwapchainBuilder {}
            .with_context(context)
            .with_window(window)
            .build();
        (void)swapchain;
        return EXIT_SUCCESS;
    } catch(const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
    } catch(...) {
        std::cerr << "Fatal error: unknown exception" << '\n';
    }
    return EXIT_FAILURE;
}
