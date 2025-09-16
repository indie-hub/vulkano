// app.hpp
#pragma once
#include <glm/glm.hpp>
#include <string>

namespace vulkano {

class Window;
class VulkanContext;
class ImGuiLayer;

class App final {
public:
    App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) noexcept;
    App& operator=(App&&) noexcept;
    ~App();

    int run();

private:
    void update_stats(double dt_ms) noexcept;

private:
    Window* m_window {nullptr};
    VulkanContext* m_vk {nullptr};
    ImGuiLayer* m_imgui {nullptr};
    glm::vec4 m_color {1.0F, 1.0F, 1.0F, 1.0F};
    double m_fps {0.0};
    double m_frame_ms {0.0};
};

} // namespace vulkano

