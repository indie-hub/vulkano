# Shadow Mapping Audit Notes

- `SceneState::lightDirection` currently stores a vector pointing *towards* the light. The runtime treats the value inconsistently: the cascaded shadow setup negates it before computing light matrices, but the fragment shader negates the stored uniform again. As soon as a user supplies the intuitive "light rays" direction (e.g. (0, -1, 0) for sun from above), the effective light direction flips to point upward. That places the virtual light below the scene, killing visible shadows.
- `update_light_indicator()` drives the helper sphere using `m_shadow.cascadeLightPositions`. Those per-frame cascade centers depend on the camera frustum, so the indicator sphere appears to chase the camera even when the light direction is constant. The fallback branch already uses `m_scene.lightDirection`; relying on cascade positions is what makes the light look as if it moves with the camera.
- The uniform upload/write path and shadow pipeline bindings look healthy; descriptors bind the shadow atlas correctly once the layout transitions to `VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL`.
- Tests cover cascade orientation, but they assume callers pass a light-to-scene vector. Aligning runtime semantics with that expectation will keep the tests valid and fix the flipped-light behaviour.

Actionable fixes:
1. Store and propagate the light **direction of travel** (`light -> scene`) in `SceneState`. Stop double-negating between CPU and shader: write this direction into the uniform buffer and remove the extra `-` in GLSL. Pass the same direction straight into `compute_cascaded_shadow_data`.
2. Update the light indicator to position itself along `-direction * distance` (a fixed radius, independent of camera-dependent cascade centers) so the visual helper remains stable when the camera moves.

## 2025-09-24 Camera-dependent lighting investigation
- Current shading pipeline feeds the light-to-scene vector directly to GLSL after recent refactor.
- The fragment shader now uses that vector without negating it, so Lambertian dot(n, l) is evaluated with l pointing *away* from the light.
- Result: upward-facing surfaces see negative cosine (clamped to zero), so diffuse + shadow terms drop out; only specular noise tied to view survives, giving the impression that lighting follows the camera and hides shadows.
- Restoring the original convention (store light-to-scene on CPU, negate in GLSL to obtain surface-to-light) should recover camera-independent lighting and shadowing.
- Updated fragment shader to restore surface-to-light vector by negating the stored light ray direction before Lambert/PCF calculations.
