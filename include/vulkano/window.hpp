#pragma once

#include <vulkano/app_config.hpp>

#include <memory>
#include <string>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace vulkano {

class GlfwContext;

class Window final {
public:
    Window() = delete;
    Window(const Window&) = delete;
    Window(Window&&) noexcept = default;
    auto operator=(const Window&) -> Window& = delete;
    auto operator=(Window&&) noexcept -> Window& = default;
    ~Window() = default;

    [[nodiscard]] static auto create(const GlfwContext& context, const AppConfig& config) -> Window;

    [[nodiscard]] auto handle() const noexcept -> GLFWwindow*;
    [[nodiscard]] auto extent() const noexcept -> WindowExtent;
    [[nodiscard]] auto should_close() const noexcept -> bool;
    [[nodiscard]] auto create_surface(VkInstance instance) const -> VkSurfaceKHR;

private:
    struct WindowDeleter final {
        void operator()(GLFWwindow* window) const noexcept;
    };

    Window(std::unique_ptr<GLFWwindow, WindowDeleter> window, WindowExtent extent) noexcept;

    std::unique_ptr<GLFWwindow, WindowDeleter> m_window;
    WindowExtent m_extent;
};

} // namespace vulkano
