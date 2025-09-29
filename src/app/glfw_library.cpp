#include <vulkano/app/glfw_library.hpp>

#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

namespace {
void glfw_error_callback(int errorCode, const char* description) {
    std::cerr << "GLFW error [" << errorCode << "]: " << (description != nullptr ? description : "<no description>")
              << '\n';
}
} // namespace

namespace vulkano::app {
GlfwLibrary::GlfwLibrary() {
    glfwSetErrorCallback(glfw_error_callback);
    glfwInitHint(GLFW_JOYSTICK_HAT_BUTTONS, GLFW_TRUE);
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error {"Failed to initialise GLFW"};
    }
    m_initialised = true;
}

GlfwLibrary::~GlfwLibrary() noexcept {
    if (m_initialised) {
        glfwTerminate();
    }
}

void GlfwLibrary::ensure_vulkan_support() const {
    if (glfwVulkanSupported() != GLFW_TRUE) {
        throw std::runtime_error {"GLFW reports that Vulkan is not supported on this platform"};
    }
}
} // namespace vulkano::app
