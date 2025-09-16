#include <vulkano/window.hpp>

#include <GLFW/glfw3.h>

#include <cassert>
#include <stdexcept>

namespace vulkano {

class WindowImpl final {
public:
    WindowImpl(const std::string& title, std::uint32_t width, std::uint32_t height)
        : handle {nullptr} {
        const int init = glfwInit();
        if (init != GLFW_TRUE) {
            throw std::runtime_error {"Failed to initialize GLFW"};
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#if defined(__APPLE__)
        // Required for macOS compatibility
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif

        handle = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
        if (handle == nullptr) {
            glfwTerminate();
            throw std::runtime_error {"Failed to create GLFW window"};
        }
    }

    ~WindowImpl() noexcept {
        if (handle != nullptr) {
            glfwDestroyWindow(handle);
            handle = nullptr;
        }
        glfwTerminate();
    }

    void poll_events() const noexcept {
        glfwPollEvents();
    }

    bool should_close() const noexcept {
        return glfwWindowShouldClose(handle) == GLFW_TRUE;
    }

    void set_title(const std::string& title) noexcept {
        glfwSetWindowTitle(handle, title.c_str());
    }

    void* native_handle() const noexcept {
        return static_cast<void*>(handle);
    }

    void framebuffer_size(std::uint32_t& out_width, std::uint32_t& out_height) const noexcept {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(handle, &w, &h);
        if (w < 0) { w = 0; }
        if (h < 0) { h = 0; }
        out_width = static_cast<std::uint32_t>(w);
        out_height = static_cast<std::uint32_t>(h);
    }

private:
    GLFWwindow* handle;
};

Window::Window(const std::string& title, std::uint32_t width, std::uint32_t height)
    : impl {std::make_unique<WindowImpl>(title, width, height)} {
}

Window::~Window() = default;

Window::Window(Window&& other) noexcept = default;
Window& Window::operator=(Window&& other) noexcept = default;

void Window::poll_events() const noexcept {
    impl->poll_events();
}

bool Window::should_close() const noexcept {
    return impl->should_close();
}

void Window::set_title(const std::string& title) noexcept {
    impl->set_title(title);
}

void* Window::native_handle() const noexcept {
    return impl->native_handle();
}

void Window::framebuffer_size(std::uint32_t& out_width, std::uint32_t& out_height) const noexcept {
    impl->framebuffer_size(out_width, out_height);
}

} // namespace vulkano
