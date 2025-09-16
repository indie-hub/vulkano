#include <vulkano/imgui_overlay.hpp>

#include <cstdint>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

namespace vulkano {

ImGuiOverlay::~ImGuiOverlay() noexcept {
    shutdown();
}

void ImGuiOverlay::init(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    std::uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue,
    VkRenderPass renderPass,
    std::uint32_t minImageCount) noexcept {
    if (initialized_) {
        return;
    }
    window_ = window;
    instance_ = instance;
    physical_device_ = physicalDevice;
    device_ = device;
    graphics_queue_family_ = graphicsQueueFamily;
    graphics_queue_ = graphicsQueue;
    render_pass_ = renderPass;
    min_image_count_ = minImageCount > 0U ? minImageCount : 2U;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io {ImGui::GetIO()};
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window_, true);

    // Create descriptor pool for ImGui
    const VkDescriptorPoolSize pool_sizes[] = {
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

    VkDescriptorPoolCreateInfo pool_info {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    const std::uint32_t pool_count {static_cast<std::uint32_t>(sizeof(pool_sizes) / sizeof(pool_sizes[0]))};
    pool_info.maxSets = 1000U * pool_count;
    pool_info.poolSizeCount = pool_count;
    pool_info.pPoolSizes = pool_sizes;
    (void)vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_);

    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physical_device_;
    init_info.Device = device_;
    init_info.QueueFamily = graphics_queue_family_;
    init_info.Queue = graphics_queue_;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.Subpass = 0;
    init_info.MinImageCount = min_image_count_;
    init_info.ImageCount = min_image_count_;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = render_pass_;
    init_info.UseDynamicRendering = false;

    ImGui_ImplVulkan_Init(&init_info);

    upload_fonts();

    initialized_ = true;
}

void ImGuiOverlay::new_frame() noexcept {
    if (!initialized_) {
        return;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::end_frame_build() noexcept {
    if (!initialized_) {
        return;
    }
    ImGui::Render();
}

void ImGuiOverlay::render(VkCommandBuffer cmd) noexcept {
    if (!initialized_) {
        return;
    }
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGuiOverlay::on_render_pass_changed(VkRenderPass renderPass, std::uint32_t minImageCount) noexcept {
    if (!initialized_) {
        return;
    }
    // Re-init backend for new render pass
    ImGui_ImplVulkan_Shutdown();
    render_pass_ = renderPass;
    min_image_count_ = minImageCount > 0U ? minImageCount : min_image_count_;

    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physical_device_;
    init_info.Device = device_;
    init_info.QueueFamily = graphics_queue_family_;
    init_info.Queue = graphics_queue_;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.Subpass = 0;
    init_info.MinImageCount = min_image_count_;
    init_info.ImageCount = min_image_count_;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = render_pass_;
    init_info.UseDynamicRendering = false;

    ImGui_ImplVulkan_Init(&init_info);
}

void ImGuiOverlay::shutdown() noexcept {
    if (!initialized_) {
        return;
    }
    vkDeviceWaitIdle(device_);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    initialized_ = false;
}

void ImGuiOverlay::upload_fonts() noexcept {
    // Backend will create and use its own command buffer internally.
    ImGui_ImplVulkan_CreateFontsTexture();
}

} // namespace vulkano
