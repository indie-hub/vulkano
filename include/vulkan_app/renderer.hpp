#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkan_app {

struct Vertex;
struct Mesh;
struct CameraUBO;
struct Light;
struct Material;
struct SsaoParams;

class Renderer final {
public:
    explicit Renderer(GLFWwindow* window);
    ~Renderer() noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void resize();
    void update_mesh(const Mesh& mesh);
    void draw_frame(const CameraUBO& camera, const Light& light, const Material& material, const SsaoParams& ssao);
    [[nodiscard]] const char* device_name() const noexcept { return device_name_; }

private:
    void init_instance();
    void init_surface(GLFWwindow* window);
    void pick_physical_device();
    void create_device();
    void create_swapchain();
    void destroy_swapchain();

    VkInstance instance_{};
    VkDebugUtilsMessengerEXT debug_messenger_{};
    VkSurfaceKHR surface_{};
    VkPhysicalDevice physical_device_{};
    VkDevice device_{};
    VkQueue graphics_queue_{};
    VkQueue present_queue_{};
    std::uint32_t graphics_family_{0U};
    std::uint32_t present_family_{0U};
    VkSwapchainKHR swapchain_{};
    VkFormat swapchain_format_{};
    VkExtent2D swapchain_extent_{};
    const char* device_name_{""};
};

} // namespace vulkan_app

