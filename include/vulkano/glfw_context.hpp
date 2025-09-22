#pragma once

#include <vulkano/app_config.hpp>

#include <GLFW/glfw3.h>

namespace vulkano {

class GlfwContext final {
public:
    GlfwContext();
    GlfwContext(const GlfwContext&) = delete;
    GlfwContext(GlfwContext&&) noexcept = delete;
    auto operator=(const GlfwContext&) -> GlfwContext& = delete;
    auto operator=(GlfwContext&&) noexcept -> GlfwContext& = delete;
    ~GlfwContext() noexcept;

    [[nodiscard]] auto create_window(const AppConfig& config) const -> GLFWwindow*;

private:
    void configure_hints(const AppConfig& config) const noexcept;
};

} // namespace vulkano
