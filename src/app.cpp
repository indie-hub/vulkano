// app.cpp
#include <vulkano/app.hpp>

#include <imgui.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include <vulkano/window.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/imgui_layer.hpp>
#include <vulkano/utils/fps.hpp>
#include <vulkan/vulkan.h>

namespace vulkano {

namespace {
    void imgui_render_cb(void* cmd_buf, void* user) {
        (void)cmd_buf;
        (void)user;
        ImGui::Render();
        // Actual rendering is performed by ImGuiLayer::render via the user object; kept for interface parity.
    }
}

App::App() : m_window {nullptr}, m_vk {nullptr}, m_imgui {nullptr}, m_color {1.0F, 1.0F, 1.0F, 1.0F}, m_fps {0.0}, m_frame_ms {0.0} {
    const WindowConfig cfg {std::string{"VulkanoCodex"}, 1280U, 720U, true};
    m_window = new Window{cfg};
    m_vk = new VulkanContext{m_window->handle(), cfg.width, cfg.height};
    m_imgui = new ImGuiLayer{m_window->handle(), m_vk->instance(), static_cast<VkPhysicalDevice>(m_vk->physical_device()), m_vk->device(), m_vk->graphics_family_index(), static_cast<VkQueue>(m_vk->graphics_queue()), static_cast<VkCommandPool>(m_vk->command_pool()), m_vk->render_pass()};
}

App::App(App&& other) noexcept {
    m_window = other.m_window; other.m_window = nullptr;
    m_vk = other.m_vk; other.m_vk = nullptr;
    m_imgui = other.m_imgui; other.m_imgui = nullptr;
    m_color = other.m_color;
    m_fps = other.m_fps;
    m_frame_ms = other.m_frame_ms;
}

App& App::operator=(App&& other) noexcept {
    if (this != &other) {
        this->~App();
        m_window = other.m_window; other.m_window = nullptr;
        m_vk = other.m_vk; other.m_vk = nullptr;
        m_imgui = other.m_imgui; other.m_imgui = nullptr;
        m_color = other.m_color;
        m_fps = other.m_fps;
        m_frame_ms = other.m_frame_ms;
    }
    return *this;
}

App::~App() {
    if (m_vk != nullptr) {
        m_vk->wait_idle();
    }
    delete m_imgui; m_imgui = nullptr;
    delete m_vk; m_vk = nullptr;
    delete m_window; m_window = nullptr;
}

void App::update_stats(double dt_ms) noexcept {
    static utils::FpsSmoother smoother {0.1};
    smoother.add_sample(dt_ms);
    m_frame_ms = smoother.smoothed_ms();
    m_fps = smoother.fps();
}

int App::run() {
    using clock = std::chrono::high_resolution_clock;
    auto last = clock::now();
    while (!m_window->should_close()) {
        m_window->poll_events();
        auto now = clock::now();
        const double dt_ms = std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        update_stats(dt_ms);

        m_imgui->new_frame();
        const auto extent = m_vk->current_extent();
        ImGui::Begin("Stats");
        ImGui::Text("FPS: %.1f", m_fps);
        ImGui::Text("Frame ms: %.2f", m_frame_ms);
        ImGui::Text("Device: %s", m_vk->device_name());
        ImGui::Text("Extent: %u x %u", extent.width, extent.height);
        ImGui::End();

        bool recreated {false};
        auto render_imgui = [this](void* cmd, void*) {
            m_imgui->render(cmd);
        };
        m_vk->draw_frame(m_color, recreated, [](void* cmd, void* user) {
            auto* self = static_cast<App*>(user);
            self->m_imgui->render(cmd);
        }, this);
        if (recreated) {
            m_vk->recreate_swapchain(m_window->width(), m_window->height());
        }
    }
    return 0;
}

} // namespace vulkano
