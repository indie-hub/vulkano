#include <vulkano/app.hpp>

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <volk.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace vulkano {

namespace {

constexpr int defaultWidth {1280};
constexpr int defaultHeight {720};

void glfwErrorCallback(const int code, const char* description) noexcept {
    (void)code;
    (void)description;
}

[[nodiscard]] std::vector<const char*> getGlfwRequiredInstanceExtensions() {
    uint32_t count {0U};
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> result {};
    result.reserve(static_cast<std::size_t>(count));
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(names[i]);
    }
    return result;
}

} // namespace

struct App::Impl {
    AppConfig config {};
    bool running {false};

    GLFWwindow* window {nullptr};

    VkInstance instance {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    uint32_t queueFamilyGraphics {0U};
    uint32_t queueFamilyPresent {0U};
    VkQueue graphicsQueue {VK_NULL_HANDLE};
    VkQueue presentQueue {VK_NULL_HANDLE};

    explicit Impl(const AppConfig& cfg)
        : config {cfg} {
    }

    void initGlfw() {
        glfwSetErrorCallback(&glfwErrorCallback);
        if (!glfwInit()) {
            throw std::runtime_error {"Failed to initialize GLFW"};
        }
        if (glfwVulkanSupported() != GLFW_TRUE) {
            throw std::runtime_error {"GLFW reports Vulkan not supported"};
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const int width = (config.initialWidth > 0) ? config.initialWidth : defaultWidth;
        const int height = (config.initialHeight > 0) ? config.initialHeight : defaultHeight;
        window = glfwCreateWindow(width, height, config.windowTitle.c_str(), nullptr, nullptr);
        if (window == nullptr) {
            throw std::runtime_error {"Failed to create GLFW window"};
        }
    }

    void initVulkan() {
        if (volkInitialize() != VK_SUCCESS) {
            throw std::runtime_error {"Failed to initialize volk"};
        }

        vkb::InstanceBuilder builder {};
        builder.require_api_version(1, 2, 0);
        builder.set_app_name(config.windowTitle.c_str());

        const bool enableValidation = config.enableValidation;
#ifdef VULKANO_CODEX_DEBUG
        if (enableValidation) {
            builder.request_validation_layers(true).use_default_debug_messenger();
        }
#endif

        const auto glfwExts = getGlfwRequiredInstanceExtensions();
        for (const char* ext : glfwExts) {
            builder.enable_extension(ext);
        }

        auto instRet = builder.build();
        if (!instRet) {
            throw std::runtime_error {"Failed to create Vulkan instance"};
        }
        vkb::Instance vkbInstance = instRet.value();
        instance = vkbInstance.instance;
        debugMessenger = vkbInstance.debug_messenger;
        volkLoadInstance(instance);

        if (!vkb::create_surface_glfw(instance, window, nullptr, &surface)) {
            throw std::runtime_error {"Failed to create window surface"};
        }

        vkb::PhysicalDeviceSelector selector {vkbInstance};
        auto physRet = selector.set_surface(surface)
                                 .set_minimum_version(1, 2)
                                 .set_preferred_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                 .select();
        if (!physRet) {
            throw std::runtime_error {"Failed to select physical device"};
        }
        vkb::PhysicalDevice vkbPhys = physRet.value();
        physicalDevice = vkbPhys.physical_device;

        vkb::DeviceBuilder devBuilder {vkbPhys};
        auto devRet = devBuilder.build();
        if (!devRet) {
            throw std::runtime_error {"Failed to create logical device"};
        }
        vkb::Device vkbDevice = devRet.value();
        device = vkbDevice.device;
        graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();
        queueFamilyGraphics = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        queueFamilyPresent = vkbDevice.get_queue_index(vkb::QueueType::present).value();
    }

    void init() {
        initGlfw();
        initVulkan();
        running = true;
    }

    void mainLoop() {
        while (running && glfwWindowShouldClose(window) == GLFW_FALSE) {
            glfwPollEvents();
        }
    }

    void shutdown() noexcept {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            if (debugMessenger != VK_NULL_HANDLE) {
                auto pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (pfnDestroyDebugUtilsMessengerEXT != nullptr) {
                    pfnDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
                }
            }
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        if (window != nullptr) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
        running = false;
    }
};

App::App(const AppConfig& config)
    : impl {std::make_unique<Impl>(config)} {
    impl->init();
}

App::~App() {
    impl->shutdown();
}

void App::run() {
    impl->mainLoop();
}

} // namespace vulkano
