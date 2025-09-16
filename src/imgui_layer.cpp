#include "imgui_layer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <stdexcept>

ImGuiLayer::~ImGuiLayer() {
    if (initialized_) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void ImGuiLayer::init(GLFWwindow* window,
                      VkInstance instance,
                      VkPhysicalDevice physical_device,
                      VkDevice device,
                      uint32_t graphics_queue_family,
                      VkQueue graphics_queue,
                      VkCommandPool command_pool,
                      VkDescriptorPool descriptor_pool,
                      VkRenderPass render_pass,
                      uint32_t image_count) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        throw std::runtime_error("Failed to init ImGui GLFW");
    }

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = graphics_queue_family;
    init_info.Queue = graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool;
    init_info.Allocator = nullptr;
    init_info.CommandPool = command_pool;
    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = nullptr;

    init_info.RenderPass = render_pass;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("Failed to init ImGui Vulkan");
    }

    // Upload fonts using internal helper
    ImGui_ImplVulkan_CreateFontsTexture();
    ImGui_ImplVulkan_DestroyFontsTexture();

    initialized_ = true;
}

void ImGuiLayer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
