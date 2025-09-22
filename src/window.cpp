#include <vulkano/window.hpp>

#include <stdexcept>
#include <utility>

#include <vulkano/glfw_context.hpp>

namespace vulkano {

void Window::WindowDeleter::operator()(GLFWwindow* window) const noexcept {
    if(window == nullptr) {
        return;
    }
    glfwDestroyWindow(window);
}

Window::Window(std::unique_ptr<GLFWwindow, WindowDeleter> window, WindowExtent extent) noexcept
    : m_window {std::move(window)}
    , m_extent {extent} {
}

auto Window::create(const GlfwContext& context, const AppConfig& config) -> Window {
    GLFWwindow* rawWindow = context.create_window(config);
    std::unique_ptr<GLFWwindow, WindowDeleter> managedWindow {rawWindow};
    return Window {std::move(managedWindow), config.window_extent()};
}

auto Window::handle() const noexcept -> GLFWwindow* {
    return m_window.get();
}

auto Window::extent() const noexcept -> WindowExtent {
    return m_extent;
}

auto Window::create_surface(VkInstance instance) const -> VkSurfaceKHR {
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    const VkResult result {glfwCreateWindowSurface(instance, m_window.get(), nullptr, &surface)};
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan surface"};
    }
    return surface;
}

} // namespace vulkano
