#pragma once

#include <glm/glm.hpp>

namespace vulkan_app {

class Camera final {
public:
    Camera() = default;
    [[nodiscard]] glm::mat4 view() const noexcept;
    [[nodiscard]] glm::mat4 proj(float aspect) const noexcept;
    [[nodiscard]] glm::vec3 position() const noexcept { return position_; }

    void set_position(const glm::vec3 &p) noexcept { position_ = p; }
    void set_yaw_pitch(float yaw, float pitch) noexcept { yaw_ = yaw; pitch_ = pitch; }
    void set_fov(float fov_radians) noexcept { fov_ = fov_radians; }
    void set_near_far(float n, float f) noexcept { near_ = n; far_ = f; }

private:
    glm::vec3 position_{0.0F, 0.0F, 3.0F};
    float yaw_{0.0F};
    float pitch_{0.0F};
    float fov_{glm::radians(60.0F)};
    float near_{0.1F};
    float far_{100.0F};
};

} // namespace vulkan_app

