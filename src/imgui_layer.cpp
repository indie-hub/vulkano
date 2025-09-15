#include <vulkano/imgui_layer.hpp>
#include <vulkano/window.hpp>
#include <vulkano/vulkan_context.hpp>

#if VULKANO_HAS_VULKAN
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <stdexcept>

namespace vulkano {

struct ImGuiLayer::Impl final {
    VkInstance instance {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
    uint32_t graphics_queue_family {0U};
    VkRenderPass render_pass {VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool {VK_NULL_HANDLE};
    GLFWwindow* glfw_window {nullptr};
    VulkanContext* ctx {nullptr};
};

// Forward accessor is provided by vulkan_context.hpp (VulkanContextAccess)

ImGuiLayer::ImGuiLayer(const Window& window, VulkanContext& context)
    : impl {std::make_unique<Impl>()} {
    impl->glfw_window = static_cast<GLFWwindow*>(window.native_handle());
    impl->ctx = &context;
    impl->instance = static_cast<VkInstance>(VulkanContextAccess::instance(context));
    impl->physical_device = static_cast<VkPhysicalDevice>(VulkanContextAccess::physical(context));
    impl->device = static_cast<VkDevice>(VulkanContextAccess::device(context));
    impl->graphics_queue = static_cast<VkQueue>(VulkanContextAccess::queue(context));
    impl->graphics_queue_family = VulkanContextAccess::queue_family(context);
    impl->render_pass = static_cast<VkRenderPass>(VulkanContextAccess::render_pass(context));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(impl->glfw_window, true);

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
    VkDescriptorPoolCreateInfo pool_info {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * static_cast<uint32_t>(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
    pool_info.poolSizeCount = static_cast<uint32_t>(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
    pool_info.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(impl->device, &pool_info, nullptr, &impl->descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create ImGui descriptor pool"};
    }

    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = impl->instance;
    init_info.PhysicalDevice = impl->physical_device;
    init_info.Device = impl->device;
    init_info.QueueFamily = impl->graphics_queue_family;
    init_info.Queue = impl->graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = impl->descriptor_pool;
    init_info.Allocator = nullptr;
    const uint32_t img_count = VulkanContextAccess::image_count(context);
    init_info.MinImageCount = img_count;
    init_info.ImageCount = img_count;
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = impl->render_pass;

    ImGui_ImplVulkan_Init(&init_info);
}

ImGuiLayer::~ImGuiLayer() {
    if (impl != nullptr) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (impl->descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(impl->device, impl->descriptor_pool, nullptr);
        }
    }
}

ImGuiLayer::ImGuiLayer(ImGuiLayer&&) noexcept = default;
ImGuiLayer& ImGuiLayer::operator=(ImGuiLayer&&) noexcept = default;

void ImGuiLayer::begin_frame() const noexcept {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::build_ui(std::uint32_t& subdivisions) const noexcept {
    ImGui::Begin("Vulkano Controls");
    ImGui::Text("Icosphere");
    int sub = static_cast<int>(subdivisions);
    if (ImGui::SliderInt("Subdivisions", &sub, 0, 6)) {
        if (sub < 0) { sub = 0; }
        if (sub > 6) { sub = 6; }
        subdivisions = static_cast<std::uint32_t>(sub);
    }

    if (impl->ctx != nullptr) {
        auto s = impl->ctx->get_settings();
        ImGui::Separator();
        ImGui::Text("Material");
        ImGui::SliderFloat("Normal Strength", &s.normal_strength, 0.0F, 2.0F);
        ImGui::Separator();
        ImGui::Text("Light");
        ImGui::SliderFloat3("Position", s.light_pos, -10.0F, 10.0F);
        ImGui::ColorEdit3("Color", s.light_color);
        ImGui::SliderFloat("Intensity", &s.light_intensity, 0.0F, 5.0F);
        ImGui::SliderFloat("Shininess", &s.shininess, 4.0F, 256.0F);

        ImGui::Separator();
        ImGui::Text("SSAO");
        ImGui::Checkbox("Enable SSAO", &s.ssao_enabled);
        ImGui::SliderInt("Kernel Size", &s.ssao_kernel, 8, 64);
        ImGui::SliderFloat("Radius", &s.ssao_radius, 0.05F, 2.0F);
        ImGui::SliderFloat("Bias", &s.ssao_bias, 0.0F, 0.1F);
        ImGui::SliderFloat("Power", &s.ssao_power, 0.0F, 2.0F);
        ImGui::Checkbox("Blur AO", &s.ssao_blur);
        ImGui::SliderInt("Blur Radius", &s.ssao_blur_radius, 1, 5);
        impl->ctx->set_settings(s);
    }
    ImGui::End();
}

void ImGuiLayer::render(void* cmd_buffer) const noexcept {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd_buffer));
}

} // namespace vulkano

#else

namespace vulkano {

class Window;
class VulkanContext;

ImGuiLayer::ImGuiLayer(const Window&, VulkanContext&) : impl {nullptr} {
}
ImGuiLayer::~ImGuiLayer() = default;
ImGuiLayer::ImGuiLayer(ImGuiLayer&&) noexcept = default;
ImGuiLayer& ImGuiLayer::operator=(ImGuiLayer&&) noexcept = default;
void ImGuiLayer::begin_frame() const noexcept {
}
void ImGuiLayer::build_ui(std::uint32_t&) const noexcept {
}
void ImGuiLayer::render(void*) const noexcept {
}

} // namespace vulkano

#endif
