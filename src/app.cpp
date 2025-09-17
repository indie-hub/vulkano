#include <vulkano/app.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/geometry.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>
#include <imgui.h>

namespace vulkano {

namespace {
    constexpr int glfw_context_version_major {3};
    constexpr int glfw_context_version_minor {3};
    // Input sensitivities (no magic numbers in code paths)
    constexpr float kOrbitSensitivity {0.005F}; // radians per pixel
    constexpr float kZoomSensitivity {0.25F};   // distance units per scroll step
}

App::App(const AppConfig& config) noexcept : config_ {config} {
    init_glfw();
    init_vulkan();
}

App::~App() noexcept {
    shutdown_vulkan();
    shutdown_glfw();
}

void App::init_glfw() noexcept {
    if (glfwInit() != GLFW_TRUE) {
        // Fail fast: if GLFW cannot init, leave window_ null; run() will exit.
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glfw_context_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glfw_context_version_minor);

    window_ = glfwCreateWindow(static_cast<int>(config_.width), static_cast<int>(config_.height), config_.title, nullptr, nullptr);
    if (window_ != nullptr) {
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, &App::framebuffer_size_callback);
        glfwSetCursorPosCallback(window_, &App::cursor_pos_callback);
        glfwSetMouseButtonCallback(window_, &App::mouse_button_callback);
        glfwSetScrollCallback(window_, &App::scroll_callback);
    }
}

void App::shutdown_glfw() noexcept {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void App::init_vulkan() noexcept {
    if (window_ == nullptr) {
        return;
    }
    vk_ = std::make_unique<VulkanContext>(window_);
}

void App::shutdown_vulkan() noexcept {
    vk_.reset();
}

void App::run() noexcept {
    if (window_ == nullptr) {
        return;
    }

    const char* envAutoClose {std::getenv("VK_APP_AUTOCLOSE_MS")};
    const bool useAutoClose {envAutoClose != nullptr};
    const std::chrono::milliseconds autoCloseMs {
        useAutoClose ? std::chrono::milliseconds {std::strtol(envAutoClose, nullptr, 10)} : std::chrono::milliseconds {0}
    };
    const auto startTime {std::chrono::steady_clock::now()};
    stats_.reset();
    last_frame_time_ = Stats::clock::now();

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        const auto now {Stats::clock::now()};
        stats_.on_frame(now);
        if (vk_ != nullptr) {
            // Begin ImGui frame and build minimal overlay
            vk_->imgui_new_frame();
            build_ui();
            const bool ok {(vk_->draw_frame())};
            if (!ok || framebuffer_resized_) {
                int width {0};
                int height {0};
                do {
                    glfwGetFramebufferSize(window_, &width, &height);
                    if (width == 0 || height == 0) {
                        glfwWaitEvents();
                    }
                } while (width == 0 || height == 0);

                vk_->recreate_swapchain(window_);
                framebuffer_resized_ = false;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds {16});
        }
        if (useAutoClose) {
            const auto now {std::chrono::steady_clock::now()};
            if (now - startTime >= autoCloseMs) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
        }
    }
}

void App::framebuffer_size_callback(GLFWwindow* window, int width, int height) noexcept {
    (void)width;
    (void)height;
    if (window == nullptr) {
        return;
    }
    auto* appPtr {static_cast<App*>(glfwGetWindowUserPointer(window))};
    if (appPtr != nullptr) {
        appPtr->framebuffer_resized_ = true;
        if (width > 0 && height > 0 && appPtr->vk_ != nullptr) {
            const float aspect {static_cast<float>(width) / static_cast<float>(height)};
            appPtr->vk_->camera_set_aspect(aspect);
        }
    }
}

void App::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) noexcept {
    if (window == nullptr) {
        return;
    }
    auto* appPtr {static_cast<App*>(glfwGetWindowUserPointer(window))};
    if (appPtr == nullptr) {
        return;
    }
    if (!appPtr->mouse_left_pressed_) {
        appPtr->last_cursor_x_ = xpos;
        appPtr->last_cursor_y_ = ypos;
        return;
    }
    const double dx {xpos - appPtr->last_cursor_x_};
    const double dy {ypos - appPtr->last_cursor_y_};
    appPtr->last_cursor_x_ = xpos;
    appPtr->last_cursor_y_ = ypos;
    if (appPtr->vk_ != nullptr) {
        const float dYaw {static_cast<float>(dx) * kOrbitSensitivity};
        const float dPitch {static_cast<float>(-dy) * kOrbitSensitivity};
        appPtr->vk_->camera_orbit_delta(dYaw, dPitch);
    }
}

void App::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) noexcept {
    (void)mods;
    if (window == nullptr) {
        return;
    }
    auto* appPtr {static_cast<App*>(glfwGetWindowUserPointer(window))};
    if (appPtr == nullptr) {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            appPtr->mouse_left_pressed_ = true;
            double x {0.0};
            double y {0.0};
            glfwGetCursorPos(window, &x, &y);
            appPtr->last_cursor_x_ = x;
            appPtr->last_cursor_y_ = y;
        } else if (action == GLFW_RELEASE) {
            appPtr->mouse_left_pressed_ = false;
        }
    }
}

void App::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) noexcept {
    (void)xoffset;
    if (window == nullptr) {
        return;
    }
    auto* appPtr {static_cast<App*>(glfwGetWindowUserPointer(window))};
    if (appPtr == nullptr) {
        return;
    }
    if (appPtr->vk_ != nullptr) {
        const float dDist {static_cast<float>(-yoffset) * kZoomSensitivity};
        appPtr->vk_->camera_zoom_delta(dDist);
    }
}

void App::build_ui() noexcept {
    ImGui::SetNextWindowBgAlpha(0.4F);
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    const float fps {stats_.fps()};
    const float ms {stats_.frame_ms()};
    ImGui::Text("FPS: %.1f", static_cast<double>(fps));
    ImGui::Text("Frame: %.2f ms", static_cast<double>(ms));
    if (vk_ != nullptr) {
        const auto extent {vk_->swapchain_extent()};
        ImGui::Text("Device: %s", vk_->device_name().c_str());
        ImGui::Text("Extent: %u x %u", extent.width, extent.height);
        // Texture + sampler info (readonly)
        const std::uint32_t aw {vk_->albedo_width()};
        const std::uint32_t ah {vk_->albedo_height()};
        const std::uint32_t nw {vk_->normal_width()};
        const std::uint32_t nh {vk_->normal_height()};
        ImGui::SeparatorText("Textures");
        const std::string& alLabel {vk_->albedo_label()};
        const std::string& nmLabel {vk_->normal_label()};
        ImGui::Text("Albedo: %u x %u (sRGB RGBA8)", aw, ah);
        ImGui::Text("  Source: %s", alLabel.c_str());
        ImGui::Text("Normal: %u x %u (UNORM RGBA8)", nw, nh);
        ImGui::Text("  Source: %s", nmLabel.c_str());
        ImGui::Text("Sampler: Linear + Mipmaps");
        if (vk_->sampler_anisotropy_supported()) {
            ImGui::Text("Anisotropy: On (max %.1f)", static_cast<double>(vk_->max_sampler_anisotropy()));
        } else {
            ImGui::Text("Anisotropy: Off");
        }
    }
    ImGui::End();

    // Scene controls window
    ImGui::Begin("Scene");
    if (vk_ != nullptr) {
        // Light controls
        auto light = vk_->light();
        const float stepPos {0.1F};
        const float stepIntensity {0.01F};
        const float minIntensity {0.0F};
        const float maxIntensity {10.0F};
        const float minAmbient {0.0F};
        const float maxAmbient {1.0F};
        ImGui::SeparatorText("Light");
        bool lightChanged {false};
        if (ImGui::DragFloat3("Position", &light.position.x, stepPos)) {
            lightChanged = true;
        }
        if (ImGui::DragFloat("Intensity", &light.intensity, stepIntensity, minIntensity, maxIntensity)) {
            lightChanged = true;
        }
        if (ImGui::ColorEdit3("Color", &light.color.x)) {
            lightChanged = true;
        }
        if (ImGui::SliderFloat("Ambient", &light.ambient, minAmbient, maxAmbient)) {
            lightChanged = true;
        }
        if (lightChanged) {
            vk_->set_light(light);
        }

        // Per-primitive controls
        const std::size_t count {vk_->primitive_count()};
        for (std::size_t i {0U}; i < count; ++i) {
            auto* prim {vk_->primitive_at(i)};
            if (prim == nullptr) {
                continue;
            }
            const char* name {prim->type_name()};
            const std::string header {std::string {name != nullptr ? name : "Primitive"} + " ##" + std::to_string(i)};
            if (ImGui::CollapsingHeader(header.c_str())) {
                Transform t {prim->transform()};
                Material m {prim->material()};
                const float stepTransform {0.05F};
                const float stepRotate {0.01745329252F}; // ~1 degree in radians
                const float minScale {0.01F};
                const float maxScale {100.0F};
                ImGui::DragFloat3("Position", &t.position.x, stepTransform);
                ImGui::DragFloat3("Rotation (rad)", &t.rotationEuler.x, stepRotate);
                ImGui::DragFloat3("Scale", &t.scale.x, stepTransform, minScale, maxScale);
                ImGui::ColorEdit3("Base Color", &m.baseColor.x);
                ImGui::SliderFloat("Shininess", &m.shininess, 1.0F, 256.0F);
                // Material texture toggles and normal strength
                ImGui::Checkbox("Use Albedo Map", &m.useAlbedo);
                ImGui::Checkbox("Use Normal Map", &m.useNormal);
                ImGui::SliderFloat("Normal Strength", &m.normalStrength, 0.0F, 2.0F);
                prim->set_transform(t);
                prim->set_material(m);

                // Optional control: Icosphere subdivisions (live rebuild + reupload)
                if (std::strcmp(name, "Icosphere") == 0) {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                    auto* iso = static_cast<Icosphere*>(prim);
                    if (iso != nullptr) {
                        std::uint32_t sub {iso->subdivisions()};
                        const std::uint32_t minSub {0U};
                        const std::uint32_t maxSub {5U};
                        if (ImGui::SliderScalar("Subdivisions", ImGuiDataType_U32, &sub, &minSub, &maxSub)) {
                            iso->set_subdivisions(sub);
                            vk_->rebuild_scene_gpu_buffers();
                        }
                    }
                } else if (std::strcmp(name, "Plane") == 0) {
                    // UV tiling for plane primitive
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                    auto* plane = static_cast<Plane*>(prim);
                    if (plane != nullptr) {
                        glm::vec2 tiling {plane->uv_tiling()};
                        const float minTile {0.01F};
                        if (ImGui::DragFloat2("UV Tiling", &tiling.x, 0.1F, minTile, 1000.0F)) {
                            // Clamp to minimum to avoid zeros/negatives
                            tiling.x = std::max(tiling.x, minTile);
                            tiling.y = std::max(tiling.y, minTile);
                            plane->set_uv_tiling(tiling);
                            vk_->rebuild_scene_gpu_buffers();
                        }
                    }
                }
            }
        }
    }
    ImGui::End();
}

} // namespace vulkano
