#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include <vulkano/mesh.hpp>

namespace vulkano {

class VulkanContext;
class Swapchain;
class ImGuiLayer;

struct RenderSettings final {
    int subdivisions {2};
    bool ssao_enabled {true};
    int ssao_kernel_size {32};
    float ssao_radius {0.5f};
    float ssao_bias {0.025f};
    float ssao_power {1.0f};
    bool blur_enabled {true};
    int blur_radius {2};
    float normal_strength {1.0f};
    float shininess {64.0f};
};

class Renderer final {
public:
    explicit Renderer(VulkanContext& ctx);
    ~Renderer();

    void draw_frame();
    void on_resize();

    RenderSettings& settings() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl {};
};

} // namespace vulkano

