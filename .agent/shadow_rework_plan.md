# Cascaded Shadow Map Plan

## Goals
- Maintain crisp shadows near the camera while reducing shimmering and aliasing across the scene.
- Support up to four cascades with tunable split distribution, bias, and PCF radius per cascade.
- Keep per-frame overhead low and avoid validation warnings.

## Tasks
1. **Frustum Splits & Matrices**
   - Compute split depths each frame using configurable lambda blend of logarithmic and linear distribution.
   - Construct light frusta for each split, snap projections to texel increments for stability.
   - Store light view-projection matrices and split depths in uniform buffer.
2. **Resources**
   - Allocate layered depth image (`VK_IMAGE_VIEW_TYPE_2D_ARRAY`) sized `resolution x resolution x cascadeCount`.
   - Update render pass/framebuffer to render each layer sequentially; adjust descriptor to sample specific layer.
3. **Rendering**
   - Record shadow pass per cascade: set viewport/scissor, depth bias, push matrix, render primitives.
   - Insert image barriers per layer.
4. **Lighting**
   - Fragment shader selects cascade by view-space depth, samples appropriate layer with PCF.
   - Use per-cascade bias parameters from uniform.
5. **UX & Docs**
   - ImGui controls for cascade count (1–4), lambda, per-cascade PCF/bias, resolution toggle.
   - Update README with cascade description.
6. **Validation**
   - Unit tests for split computation and cascade selection logic.
   - Integration test ensures resources initialise and cascades render without validation errors.

## Cascade Debug Visualisation (New)
- Use `ImGui_ImplVulkan_AddTexture` to register per-cascade depth layer views with the ImGui renderer.
- Maintain a vector of `ImTextureID` handles matching the current cascade count; recreate when resolution or layer count changes.
- Display each cascade in the Shadows panel with `ImGui::Image`, providing hover tooltips for exact depth values.
- Ensure layout transitions leave shadow layers in `VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL` before ImGui sampling.
- Remove textures via `ImGui_ImplVulkan_RemoveTexture` when recreating shadow resources or on shutdown to avoid leaks.

## Shadows Toggle Plan
- Add `bool shadowsEnabled` to `ShadowSettings`, defaulting to true; expose a checkbox in the Shadows panel.
- When disabled:
  - Skip the shadow pass entirely.
  - Force shader visibility to 1.0 without sampling the array texture.
  - Pause cascade preview updates and hide the preview tree.
- When (re)enabled, mark shadow resources dirty so layered maps regenerate.
- Update uniforms to carry the toggle flag; fragment shader uses it to bypass visibility attenuation.

## Acceptance Checklist
- ✔ Cascades render without seams and no significant shimmering.
- ✔ Configurable cascade count, split lambda, bias, and PCF radius.
- ✔ Performance impact within agreed budget (≤15% relative to single-pass baseline).
- ✔ Vulkan validation clean, no descriptor or barrier warnings.
- ✔ Tests cover split math, cascade selection; README documents controls.
