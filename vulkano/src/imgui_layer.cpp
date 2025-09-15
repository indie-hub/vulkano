#include <vulkano/imgui_layer.hpp>

namespace vulkano {

class ImGuiLayer::Impl {
public:
    Impl() = default;
};

ImGuiLayer::ImGuiLayer(VulkanContext&, VkRenderPass, uint32_t)
    : m_impl {new Impl()}
{
}

ImGuiLayer::~ImGuiLayer()
{
    delete m_impl;
}

void ImGuiLayer::new_frame()
{
}

void ImGuiLayer::render(VkCommandBuffer)
{
}

void ImGuiLayer::shutdown()
{
}

} // namespace vulkano

