#include <vulkano/app/camera_controller.hpp>

#include <vulkano/app/camera.hpp>
#include <vulkano/app/window.hpp>

#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

namespace vulkano::app {
CameraController::CameraController(Camera& camera, Window& window) noexcept
    : m_camera {camera}
    , m_window {window} {
}

void CameraController::set_move_speed(float speed) noexcept {
    m_moveSpeed = speed;
}

void CameraController::set_mouse_sensitivity(float sensitivity) noexcept {
    m_mouseSensitivity = sensitivity;
}

void CameraController::set_input_enabled(bool enabled) noexcept {
    if (!enabled) {
        disable_capture();
        m_window.reset_cursor_delta();
    }
    m_inputEnabled = enabled;
}

void CameraController::update(float deltaSeconds) {
    if (!m_inputEnabled) {
        disable_capture();
        m_window.reset_cursor_delta();
        return;
    }

    const bool rightButtonDown = m_window.is_mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (rightButtonDown && !m_capturing) {
        enable_capture();
    } else if (!rightButtonDown && m_capturing) {
        disable_capture();
        return;
    }

    if (!m_capturing) {
        m_window.reset_cursor_delta();
        return;
    }

    if (m_window.is_key_pressed(GLFW_KEY_W)) {
        m_camera.move(Camera::Movement::Forward, deltaSeconds, m_moveSpeed);
    }
    if (m_window.is_key_pressed(GLFW_KEY_S)) {
        m_camera.move(Camera::Movement::Backward, deltaSeconds, m_moveSpeed);
    }
    if (m_window.is_key_pressed(GLFW_KEY_A)) {
        m_camera.move(Camera::Movement::Left, deltaSeconds, m_moveSpeed);
    }
    if (m_window.is_key_pressed(GLFW_KEY_D)) {
        m_camera.move(Camera::Movement::Right, deltaSeconds, m_moveSpeed);
    }
    if (m_window.is_key_pressed(GLFW_KEY_E)) {
        m_camera.move(Camera::Movement::Up, deltaSeconds, m_moveSpeed);
    }
    if (m_window.is_key_pressed(GLFW_KEY_Q)) {
        m_camera.move(Camera::Movement::Down, deltaSeconds, m_moveSpeed);
    }

    const glm::vec2 cursorDelta = m_window.consume_cursor_delta();
    if (cursorDelta.x != 0.0F || cursorDelta.y != 0.0F) {
        m_camera.rotate(cursorDelta.x, cursorDelta.y, m_mouseSensitivity);
    }
}

void CameraController::reset() noexcept {
    disable_capture();
    m_window.reset_cursor_delta();
}

void CameraController::enable_capture() noexcept {
    m_window.set_cursor_capture(true);
    m_window.reset_cursor_delta();
    m_capturing = true;
}

void CameraController::disable_capture() noexcept {
    if (m_capturing) {
        m_window.set_cursor_capture(false);
        m_capturing = false;
    }
}
} // namespace vulkano::app
