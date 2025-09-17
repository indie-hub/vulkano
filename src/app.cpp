#include <vulkano/app.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/geometry.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>
#include <glm/vec3.hpp>
#include <imgui.h>

namespace vulkano {

namespace {
    constexpr int glfw_context_version_major {3};
    constexpr int glfw_context_version_minor {3};
    // Input sensitivities (no magic numbers in code paths)
    constexpr float kLookSensitivity {0.0025F}; // radians per pixel
    constexpr float kFovScrollStep {0.05F};     // radians per scroll step (~3 deg)
    constexpr float kMoveSpeed {3.0F};          // world units per second
    constexpr float kSprintMultiplier {2.0F};
    constexpr float kMaxDeltaTime {0.1F};       // seconds, to avoid big steps on stall
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
        glfwSetKeyCallback(window_, &App::key_callback);
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
    // Optional automated resize stress test (for CI): set VK_TEST_RESIZE to any non-zero/non-empty value.
    const char* envTestResize {std::getenv("VK_TEST_RESIZE")};
    const bool enableResizeTest {envTestResize != nullptr && envTestResize[0] != '\0' && std::strcmp(envTestResize, "0") != 0};
    // Interval between resizes (ms), configurable via VK_TEST_RESIZE_INTERVAL_MS; default 250ms
    const char* envResizeInterval {std::getenv("VK_TEST_RESIZE_INTERVAL_MS")};
    const std::chrono::milliseconds resizeIntervalMs {
        (envResizeInterval != nullptr && envResizeInterval[0] != '\0')
            ? std::chrono::milliseconds {std::strtol(envResizeInterval, nullptr, 10)}
            : std::chrono::milliseconds {250}
    };
    struct ResizeStep final { int w {0}; int h {0}; };
    // Named sequence to avoid magic numbers; includes current default window size
    constexpr ResizeStep kResizeSeq[] {
        ResizeStep {800, 600},
        ResizeStep {1280, 720},
        ResizeStep {960, 540},
    };
    std::size_t resizeIndex {0U};
    auto lastResizeTime = Stats::clock::now();
    const auto startTime {std::chrono::steady_clock::now()};
    stats_.reset();
    last_frame_time_ = Stats::clock::now();

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        const auto now {Stats::clock::now()};
        stats_.on_frame(now);
        const std::chrono::duration<float> dtDur {now - last_frame_time_};
        float dt {dtDur.count()};
        if (dt > kMaxDeltaTime) {
            dt = kMaxDeltaTime;
        }
        if (vk_ != nullptr) {
            // Begin ImGui frame and build minimal overlay
            vk_->imgui_new_frame();
            build_ui();
            // Keyboard movement (FPS-style), gated by ImGui capture rules
            ImGuiIO& io {ImGui::GetIO()};
            const bool canKeys {(!lock_camera_) && (look_active_ || !io.WantCaptureKeyboard)};
            if (canKeys) {
                float speed {kMoveSpeed};
                if (key_shift_) {
                    speed *= kSprintMultiplier;
                }
                glm::vec3 move {0.0F, 0.0F, 0.0F};
                if (key_w_) { move.z += 1.0F; }
                if (key_s_) { move.z -= 1.0F; }
                if (key_d_) { move.x += 1.0F; }
                if (key_a_) { move.x -= 1.0F; }
                if (key_space_) { move.y += 1.0F; }
                if (key_ctrl_) { move.y -= 1.0F; }
                const float lenSq {move.x * move.x + move.y * move.y + move.z * move.z};
                if (lenSq > 0.0F) {
                    const float invLen {1.0F / std::sqrt(lenSq)};
                    move.x *= invLen;
                    move.y *= invLen;
                    move.z *= invLen;
                    move.x *= speed * dt;
                    move.y *= speed * dt;
                    move.z *= speed * dt;
                    vk_->camera_move_local(move);
                }
            }
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
                if (!ok) {
                    // End ImGui frame build to keep ImGui state consistent when frame is skipped
                    vk_->imgui_end_frame_build();
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds {16});
        }
        last_frame_time_ = now;
        // Drive automated resize steps on a fixed interval for stability testing
        if (enableResizeTest) {
            const auto nowR {Stats::clock::now()};
            if (nowR - lastResizeTime >= resizeIntervalMs) {
                const ResizeStep step {kResizeSeq[resizeIndex % (sizeof(kResizeSeq) / sizeof(kResizeSeq[0]))]};
                glfwSetWindowSize(window_, step.w, step.h);
                lastResizeTime = nowR;
                resizeIndex = (resizeIndex + 1U);
            }
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
    ImGuiIO& io {ImGui::GetIO()};
    const bool canLook {appPtr->look_active_ && !appPtr->lock_camera_ && !io.WantCaptureMouse};
    if (!canLook) {
        appPtr->last_cursor_x_ = xpos;
        appPtr->last_cursor_y_ = ypos;
        appPtr->first_mouse_ = true;
        return;
    }
    if (appPtr->first_mouse_) {
        appPtr->last_cursor_x_ = xpos;
        appPtr->last_cursor_y_ = ypos;
        appPtr->first_mouse_ = false;
        return;
    }
    const double dx {xpos - appPtr->last_cursor_x_};
    const double dy {ypos - appPtr->last_cursor_y_};
    appPtr->last_cursor_x_ = xpos;
    appPtr->last_cursor_y_ = ypos;
    if (appPtr->vk_ != nullptr) {
        const float dYaw {static_cast<float>(dx) * kLookSensitivity};
        const float dPitch {static_cast<float>(-dy) * kLookSensitivity};
        appPtr->vk_->camera_look_delta(dYaw, dPitch);
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
    ImGuiIO& io {ImGui::GetIO()};
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            if (!io.WantCaptureMouse && !appPtr->lock_camera_) {
                appPtr->look_active_ = true;
                appPtr->first_mouse_ = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                double x {0.0};
                double y {0.0};
                glfwGetCursorPos(window, &x, &y);
                appPtr->last_cursor_x_ = x;
                appPtr->last_cursor_y_ = y;
            }
        } else if (action == GLFW_RELEASE) {
            appPtr->look_active_ = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
    ImGuiIO& io {ImGui::GetIO()};
    if (appPtr->vk_ != nullptr && !io.WantCaptureMouse) {
        const float dFov {static_cast<float>(-yoffset) * kFovScrollStep};
        appPtr->vk_->camera_fov_delta(dFov);
    }
}

void App::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept {
    (void)scancode;
    (void)mods;
    if (window == nullptr) {
        return;
    }
    auto* appPtr {static_cast<App*>(glfwGetWindowUserPointer(window))};
    if (appPtr == nullptr) {
        return;
    }
    const bool pressed {action != GLFW_RELEASE};
    switch (key) {
        case GLFW_KEY_W: { appPtr->key_w_ = pressed; break; }
        case GLFW_KEY_A: { appPtr->key_a_ = pressed; break; }
        case GLFW_KEY_S: { appPtr->key_s_ = pressed; break; }
        case GLFW_KEY_D: { appPtr->key_d_ = pressed; break; }
        case GLFW_KEY_SPACE: { appPtr->key_space_ = pressed; break; }
        case GLFW_KEY_LEFT_CONTROL: { appPtr->key_ctrl_ = pressed; break; }
        case GLFW_KEY_RIGHT_CONTROL: { appPtr->key_ctrl_ = pressed; break; }
        case GLFW_KEY_LEFT_SHIFT: { appPtr->key_shift_ = pressed; break; }
        case GLFW_KEY_RIGHT_SHIFT: { appPtr->key_shift_ = pressed; break; }
        default: { break; }
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
        const std::uint32_t amips {vk_->albedo_mip_levels()};
        const std::uint32_t nmips {vk_->normal_mip_levels()};
        ImGui::Text("Albedo: %u x %u, mips=%u", aw, ah, amips);
        ImGui::Text("  Format: %s", vk_->albedo_format_string().c_str());
        ImGui::Text("  Source: %s", alLabel.c_str());
        ImGui::Text("Normal: %u x %u, mips=%u", nw, nh, nmips);
        ImGui::Text("  Format: %s", vk_->normal_format_string().c_str());
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

    // Camera panel
    ImGui::Begin("Camera");
    ImGui::Text("Hold RMB to look. WASD to move. Shift to sprint.");
    bool lockCam {lock_camera_};
    if (ImGui::Checkbox("Lock camera", &lockCam)) {
        lock_camera_ = lockCam;
        if (lock_camera_) {
            look_active_ = false;
            if (window_ != nullptr) {
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }
    if (vk_ != nullptr) {
        const glm::vec3 pos {vk_->camera_position()};
        const glm::vec2 ang {vk_->camera_angles()};
        const float fov {vk_->camera_fov()};
        ImGui::SeparatorText("Pose");
        ImGui::Text("Pos: (%.3f, %.3f, %.3f)", static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z));
        ImGui::Text("Yaw: %.3f rad", static_cast<double>(ang.x));
        ImGui::Text("Pitch: %.3f rad", static_cast<double>(ang.y));
        ImGui::Text("FOV Y: %.3f rad", static_cast<double>(fov));
    }
    ImGui::End();
}

} // namespace vulkano
