# Shadow Mapping Implementation Plan

## Goal
Introduce shadow rendering so scene geometry casts shadows under existing lights (starting with one directional light), while maintaining real-time performance, clean Vulkan validation, and simple ImGui controls.

## Step-by-Step Plan

1. **Research Baseline & Constraints**
   - Confirm current renderer uses forward shading with multi-light PBR pass.
   - Decide on shadow approach: start with single directional light using cascade-less shadow map (orthographic depth pass).
   - Acceptance: Document chosen approach and limitations in `plan_shadows.md` (done).

2. **Shadow Map Resources**
   - Create `vk::DepthImage` derivative for shadow map (e.g., 2048×2048 depth texture) with sampler configured for comparison filtering.
   - Manage per-frame shadow framebuffer; ensure swapchain recreation handles it.
   - Acceptance: Resource creation wrapped in RAII; tests cover basic allocation (unit tests or assertions).

3. **Shadow Pass Pipeline**
   - Add dedicated render pass + pipeline for depth-only rendering from light POV.
   - Reuse existing mesh vertex buffers; record command buffer that renders scene into shadow map.
   - Acceptance: Shadow pass builds without validation errors; pipeline compiled.

4. **Light View/Projection Setup**
   - Compute light view-projection matrix for primary directional light (fit bounding box around scene or camera frustum).
   - Store matrix in uniform/push constant accessible by shadow pass and scene pass.
   - Acceptance: Matrix generation tested (unit tests for BBox transforms) or verified via instrumentation.

5. **Descriptor Updates & Sampling**
   - Add shadow map sampler to material/light descriptor set (likely new set binding for shadow textures).
   - Update `SceneRenderer::set_light_resources` to provide light MVP and shadow descriptor to shader.
   - Acceptance: Descriptor sets updated safely; build succeeds.

6. **Lighting Shader Integration**
   - Modify `scene.frag` to sample shadow map using light space coords (with bias) and attenuate light contribution when in shadow.
   - Implement PCF (3×3) for soft edges initially; configurable bias to avoid acne.
   - Acceptance: Shader compiles; fallback path when no shadow map is bound.

7. **ImGui Controls**
   - Add controls for shadow enable toggle, map resolution (predefined options), depth bias, PCF radius, and show shadow map debug.
   - Acceptance: UI updates registry/config; renderer refreshes resources when parameters change.

8. **Runtime Wiring**
   - Update frame loop: render shadow pass before main pass each frame (respecting fences), bind resulting depth texture to lighting shader.
   - Ensure synchronization via barriers (shadow map layout transitions).
   - Acceptance: Runtime executes without validation warnings; FPS acceptable.

9. **Testing & Validation**
   - Add unit tests for light MVP calculations and PCF sampling utility functions.
   - Rebuild, run `ctest`; manual verification with shadow debug view + screenshot for QA.
   - Acceptance: Tests pass; manual inspection shows shadows under directional light.

10. **Documentation**
   - Update `assets/README.md` (if new resources), add `docs/shadows.md` describing usage, controls, and limitations.
   - Acceptance: Docs merged alongside code change.

## Acceptance Criteria Summary
- Shadow map depth resources managed with RAII, recreated on resize without leaks.
- Shadow pass renders scene from light POV and feeds main shader via descriptor.
- Shaders attenuate lighting based on shadow lookup with configurable bias/PCF.
- ImGui exposes shadow controls; runtime toggle works.
- All tests + runtime validation clean.

## Future Work
- Cascade shadow maps for wide scenes.
- Support spot/point light shadows via cube maps.
- Temporal filtering for softness.
