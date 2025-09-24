# Shadow Mapping Audit Notes

- `SceneState::lightDirection` currently stores a vector pointing *towards* the light. The runtime treats the value inconsistently: the cascaded shadow setup negates it before computing light matrices, but the fragment shader negates the stored uniform again. As soon as a user supplies the intuitive "light rays" direction (e.g. (0, -1, 0) for sun from above), the effective light direction flips to point upward. That places the virtual light below the scene, killing visible shadows.
- `update_light_indicator()` drives the helper sphere using `m_shadow.cascadeLightPositions`. Those per-frame cascade centers depend on the camera frustum, so the indicator sphere appears to chase the camera even when the light direction is constant. The fallback branch already uses `m_scene.lightDirection`; relying on cascade positions is what makes the light look as if it moves with the camera.
- The uniform upload/write path and shadow pipeline bindings look healthy; descriptors bind the shadow atlas correctly once the layout transitions to `VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL`.
- Tests cover cascade orientation, but they assume callers pass a light-to-scene vector. Aligning runtime semantics with that expectation will keep the tests valid and fix the flipped-light behaviour.

Actionable fixes:
1. Store and propagate the light **direction of travel** (`light -> scene`) in `SceneState`. Stop double-negating between CPU and shader: write this direction into the uniform buffer and remove the extra `-` in GLSL. Pass the same direction straight into `compute_cascaded_shadow_data`.
2. Update the light indicator to position itself along `-direction * distance` (a fixed radius, independent of camera-dependent cascade centers) so the visual helper remains stable when the camera moves.
