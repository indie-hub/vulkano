#include <cstdint>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include <vulkan_app/imgui_layer.hpp>
#include <vulkan_app/types.hpp>

namespace vulkan_app {

struct ImGuiLayer::Impl final {
    GLFWwindow* window{};
    VkInstance instance{};
    VkDevice device{};
    VkPhysicalDevice gpu{};
    VkQueue queue{};
    std::uint32_t queueFamily{0U};
    VkRenderPass renderPass{};
};

ImGuiLayer::ImGuiLayer(GLFWwindow* window,
                       VkInstance instance,
                       VkDevice device,
                       VkPhysicalDevice gpu,
                       VkQueue graphicsQueue,
                       std::uint32_t graphicsQueueFamily,
                       VkRenderPass renderPass) {
    impl_ = new Impl{};
    impl_->window = window;
    impl_->instance = instance;
    impl_->device = device;
    impl_->gpu = gpu;
    impl_->queue = graphicsQueue;
    impl_->queueFamily = graphicsQueueFamily;
    impl_->renderPass = renderPass;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);
    // The actual Vulkan init (descriptor pool etc.) will be done when render pass and pipeline are ready.
}

ImGuiLayer::~ImGuiLayer() noexcept {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete impl_;
}

void ImGuiLayer::new_frame() {
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::draw_ui(int& subdivisions, Light& light, Material& material, SsaoParams& ssao) {
    ImGui::Begin("Vulkan App");
    ImGui::Text("Stats");
    ImGui::Separator();
    ImGui::SliderInt("Subdivisions", &subdivisions, 0, 6);

    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &light.position.x, 0.05F);
        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.05F, 0.0F, 100.0F);
        ImGui::DragFloat("Shininess", &light.shininess, 1.0F, 1.0F, 512.0F);
    }

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Albedo", &material.albedo.x);
        ImGui::DragFloat("Normal Strength", &material.normalStrength, 0.01F, 0.0F, 2.0F);
    }

    if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", reinterpret_cast<bool*>(&ssao.enabled));
        ImGui::SliderInt("Kernel Size", &ssao.kernelSize, 16, 64);
        ImGui::DragFloat("Radius", &ssao.radius, 0.01F, 0.01F, 2.0F);
        ImGui::DragFloat("Bias", &ssao.bias, 0.001F, 0.001F, 0.1F, "%.3f");
        ImGui::DragFloat("Power", &ssao.power, 0.01F, 0.0F, 3.0F);
        ImGui::Checkbox("Blur", reinterpret_cast<bool*>(&ssao.enableBlur));
        ImGui::SliderInt("Blur Radius", &ssao.blurRadius, 1, 5);
    }

    ImGui::End();
}

void ImGuiLayer::render(VkCommandBuffer /*cmd*/) {
    ImGui::Render();
}

} // namespace vulkan_app

