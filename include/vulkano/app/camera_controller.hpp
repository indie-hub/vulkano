#pragma once

#include <cstdint>

namespace vulkano::app {
class Camera;
class Window;

class CameraController final {
public:
    CameraController(Camera& camera, Window& window) noexcept;

    void set_move_speed(float speed) noexcept;
    void set_mouse_sensitivity(float sensitivity) noexcept;
    void set_input_enabled(bool enabled) noexcept;

    void update(float deltaSeconds);
    void reset() noexcept;

private:
    void enable_capture() noexcept;
    void disable_capture() noexcept;

    Camera& m_camera;
    Window& m_window;

    float m_moveSpeed {4.0F};
    float m_mouseSensitivity {0.1F};
    bool m_capturing {false};
    bool m_inputEnabled {true};
};
} // namespace vulkano::app
