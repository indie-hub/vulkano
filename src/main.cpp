#include "vulkan_app.hpp"
#include "imgui_layer.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <chrono>
#include <cstdio>
#include <string>
#include <imgui.h>

namespace {
    constexpr int DEFAULT_WIDTH{1280};
    constexpr int DEFAULT_HEIGHT{720};
    constexpr char WINDOW_TITLE[] = "Vulkan Triangle with ImGui";
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    (void)width; (void)height; // Unused
    // Store a flag in user pointer if needed; here we recreate swapchain on next frame
}

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    VulkanApp app{window};

    ImGuiLayer imgui;
    imgui.init(window,
               app.instance(),
               app.physicalDevice(),
               app.device(),
               app.graphicsQueueFamily(),
               app.graphicsQueue(),
               app.imguiDescriptorPool(),
               app.renderPass(),
               app.imageCount());

    auto last_time = std::chrono::high_resolution_clock::now();
    double smooth_dt_ms = 16.0;
    constexpr double SMOOTH_FACTOR{0.1};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        auto now = std::chrono::high_resolution_clock::now();
        double dt_ms = std::chrono::duration<double, std::milli>(now - last_time).count();
        last_time = now;
        smooth_dt_ms = (1.0 - SMOOTH_FACTOR) * smooth_dt_ms + SMOOTH_FACTOR * dt_ms;
        double fps = smooth_dt_ms > 0.0 ? 1000.0 / smooth_dt_ms : 0.0;

        imgui.newFrame();

        // Build overlay
        ImGui::Begin("Stats");
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame time: %.2f ms", smooth_dt_ms);
        ImGui::Text("Device: %s", app.deviceName().c_str());
        auto extent = app.currentExtent();
        ImGui::Text("Swapchain: %ux%u", extent.width, extent.height);
        ImGui::End();

        const glm::vec4 white{1.0f, 1.0f, 1.0f, 1.0f};
        app.drawFrame(white);
    }

    app.waitIdle();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
