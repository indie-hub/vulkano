#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vulkan_app/application.hpp>

namespace vulkan_app {
namespace {
#ifdef NDEBUG
constexpr bool enable_validation {false};
#else
constexpr bool enable_validation {true};
#endif

const std::vector<const char *> validation_layers {"VK_LAYER_KHRONOS_validation"};

bool check_validation_layer_support() {
    std::uint32_t layer_count {0};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available.data());
    for (const char *name : validation_layers) {
        bool found {false};
        for (const auto &prop : available) {
            if (std::string_view {prop.layerName} == name) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

std::vector<const char *> required_extensions() {
    std::uint32_t count {0};
    const char **ext = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char *> extensions(ext, ext + count);
    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

} // namespace

Application::Application() {
    init_window();
    init_vulkan();
}

Application::~Application() noexcept {
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::run() {
    main_loop();
}

void Application::init_window() {
    if (glfwInit() == 0) {
        throw std::runtime_error {"Failed to initialise GLFW"};
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error {"Failed to create GLFW window"};
    }
}

void Application::init_vulkan() {
    if (enable_validation && !check_validation_layer_support()) {
        throw std::runtime_error {"Validation layers requested but not available"};
    }

    VkApplicationInfo app_info {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VulkanApp";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.pEngineName = "NoEngine";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    auto extensions = required_extensions();

    VkInstanceCreateInfo create_info {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (enable_validation) {
        create_info.enabledLayerCount = static_cast<std::uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
        create_info.enabledLayerCount = 0U;
    }

    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan instance"};
    }
}

void Application::main_loop() {
    while (glfwWindowShouldClose(window) == 0) {
        glfwPollEvents();
    }
}

} // namespace vulkan_app
