#pragma once

#include <cstdint>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace vulkano {

// Simple orbit camera (right-handed, depth 0..1 for Vulkan)
class Camera final {
public:
    struct Params final {
        glm::vec3 target {0.0F, 0.0F, 0.0F};
        float distance {4.0F};
        float yawRadians {0.78539816339F};   // ~45 deg
        float pitchRadians {-0.3490658504F}; // ~-20 deg
        float fovYRadians {1.0471975512F};   // 60 deg
        float aspect {16.0F / 9.0F};
        float nearPlane {0.1F};
        float farPlane {200.0F};
    };

    Camera() noexcept;
    explicit Camera(const Params& p) noexcept;

    void set_params(const Params& p) noexcept;
    [[nodiscard]] const Params& params() const noexcept;

    void set_aspect(float aspect) noexcept;
    void orbit_delta(float dYaw, float dPitch, float dDistance) noexcept;
    void pan_delta(const glm::vec3& delta) noexcept;

    [[nodiscard]] glm::vec3 position() const noexcept;
    [[nodiscard]] glm::mat4 view() const noexcept;
    [[nodiscard]] glm::mat4 projection() const noexcept;
    [[nodiscard]] glm::mat4 view_projection() const noexcept;

private:
    Params params_ {};
};

} // namespace vulkano

