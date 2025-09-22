#include <cstdlib>
#include <exception>
#include <cstdint>
#include <iostream>

#include <vulkano/app_config.hpp>
#include <vulkano/glfw_context.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>
#include <vulkano/swapchain.hpp>
#include <vulkano/render_pass.hpp>
#include <vulkano/framebuffers.hpp>
#include <vulkano/command_allocator.hpp>
#include <vulkano/synchronization.hpp>

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
        auto renderPass = vulkano::RenderPass::create(context, swapchain.image_format());
        auto framebuffers = vulkano::FramebufferCollection::create(context, swapchain, renderPass);
        const std::uint32_t commandBufferCount = static_cast<std::uint32_t>(framebuffers.size());
        auto commandAllocator = vulkano::CommandAllocator::create(context, commandBufferCount);
        constexpr std::uint32_t framesInFlight {2U};
        auto syncManager = vulkano::SyncManager::create(context, framesInFlight);
        (void)renderPass;
        (void)framebuffers;
        (void)commandAllocator;
        (void)syncManager;
        return EXIT_SUCCESS;
    } catch(const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
    } catch(...) {
        std::cerr << "Fatal error: unknown exception" << '\n';
    }
    return EXIT_FAILURE;
}
