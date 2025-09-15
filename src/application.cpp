#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vulkan_app/application.hpp>
#include <vulkan_app/icosphere.hpp>

namespace vulkan_app {
namespace {
} // namespace

Application::Application() {
    init_window();
    renderer = std::make_unique<Renderer>(window);
    // Create ImGui layer and attach to renderer for overlay rendering
    imgui = std::make_unique<ImGuiLayer>(window,
                                         renderer->instance(),
                                         renderer->device(),
                                         renderer->physical_device(),
                                         renderer->graphics_queue(),
                                         renderer->graphics_queue_family(),
                                         renderer->render_pass());
    renderer->attach_imgui(imgui.get());
}

Application::~Application() noexcept {
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::run() {
    main_loop();
}

void Application::init_window() {
    if (glfwInit() == 0) {
        throw std::runtime_error {"Failed to initialise GLFW"};
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error {"Failed to create GLFW window"};
    }
}

void Application::main_loop() {
    auto mesh = make_icosphere(subdivisions);
    renderer->update_mesh(mesh);
    while (glfwWindowShouldClose(window) == 0) {
        glfwPollEvents();
        rebuild_mesh_if_needed();

        imgui->new_frame();
        imgui->draw_ui(pending_subdivisions, light, material, ssao);

        // Prepare camera UBO
        int w{0}, h{0};
        glfwGetFramebufferSize(window, &w, &h);
        const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0F;
        const auto view = camera.view();
        const auto proj = camera.proj(aspect);
        CameraUBO ubo{};
        ubo.view = view;
        ubo.proj = proj;
        ubo.viewProj = proj * view;
        ubo.cameraPos = camera.position();

        renderer->draw_frame(ubo, light, material, ssao);
    }
}

void Application::rebuild_mesh_if_needed() {
    if (pending_subdivisions != subdivisions) {
        subdivisions = pending_subdivisions;
        const auto mesh = make_icosphere(subdivisions);
        renderer->update_mesh(mesh);
    }
}

} // namespace vulkan_app
