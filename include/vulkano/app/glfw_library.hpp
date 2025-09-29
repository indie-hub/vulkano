#pragma once

struct GLFWwindow;

namespace vulkano::app {
class GlfwLibrary final {
public:
    GlfwLibrary();
    ~GlfwLibrary() noexcept;

    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;
    GlfwLibrary(GlfwLibrary&&) noexcept = delete;
    GlfwLibrary& operator=(GlfwLibrary&&) noexcept = delete;

    void ensure_vulkan_support() const;

private:
    bool m_initialised {false};
};
} // namespace vulkano::app
