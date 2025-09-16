// imgui_layer.cpp
#include <vulkano/imgui_layer.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <vulkan/vulkan.h>

namespace vulkano {

namespace {
    VkDescriptorPool create_descriptor_pool(VkDevice device) {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        };
        const std::uint32_t pool_sizes_count {static_cast<std::uint32_t>(sizeof(pool_sizes) / sizeof(pool_sizes[0]))};
        VkDescriptorPoolCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = 1000;
        info.poolSizeCount = pool_sizes_count;
        info.pPoolSizes = pool_sizes;
        VkDescriptorPool pool {nullptr};
        if (vkCreateDescriptorPool(device, &info, nullptr, &pool) != VK_SUCCESS) {
            return nullptr;
        }
        return pool;
    }
}

ImGuiLayer::ImGuiLayer(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, std::uint32_t queue_family_index, VkQueue graphics_queue, VkCommandPool command_pool, VkRenderPass render_pass)
    : m_window {window}, m_instance {instance}, m_device {device}, m_physical_device {physical_device}, m_graphics_queue {graphics_queue}, m_render_pass {render_pass}, m_descriptor_pool {nullptr}, m_command_pool {command_pool}, m_queue_family_index {queue_family_index} {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);
    m_descriptor_pool = create_descriptor_pool(device);

    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = m_queue_family_index;
    init_info.Queue = graphics_queue;
    init_info.DescriptorPool = m_descriptor_pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.RenderPass = render_pass;
    ImGui_ImplVulkan_Init(&init_info);
    // Upload fonts
    VkCommandBufferAllocateInfo alloc {};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd {};
    vkAllocateCommandBuffers(m_device, &alloc, &cmd);
    VkCommandBufferBeginInfo begin {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    ImGui_ImplVulkan_CreateFontsTexture();
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphics_queue);
    ImGui_ImplVulkan_DestroyFontsTexture();
    vkFreeCommandBuffers(m_device, m_command_pool, 1, &cmd);
}

ImGuiLayer::ImGuiLayer(ImGuiLayer&& other) noexcept {
    m_window = other.m_window;
    m_instance = other.m_instance;
    m_device = other.m_device;
    m_graphics_queue = other.m_graphics_queue;
    m_render_pass = other.m_render_pass;
    m_descriptor_pool = other.m_descriptor_pool;
    other.m_window = nullptr;
    other.m_descriptor_pool = nullptr;
}

ImGuiLayer& ImGuiLayer::operator=(ImGuiLayer&& other) noexcept {
    if (this != &other) {
        this->~ImGuiLayer();
        m_window = other.m_window;
        m_instance = other.m_instance;
        m_device = other.m_device;
        m_graphics_queue = other.m_graphics_queue;
        m_render_pass = other.m_render_pass;
        m_descriptor_pool = other.m_descriptor_pool;
        other.m_window = nullptr;
        other.m_descriptor_pool = nullptr;
    }
    return *this;
}

ImGuiLayer::~ImGuiLayer() {
    if (m_device != nullptr) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (m_descriptor_pool != nullptr) {
            vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
            m_descriptor_pool = nullptr;
        }
    }
}

void ImGuiLayer::new_frame() noexcept {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(void* command_buffer) noexcept {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(command_buffer));
}

} // namespace vulkano
