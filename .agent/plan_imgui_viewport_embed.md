# Plan: Present Scene Output Within Docked Viewport

## Goal
Render the swapchain image directly inside the ImGui `Viewport` window so the scene is no longer baked into the background and can be resized/undocked like any other panel.

## Baby-Step Plan

- [ ] **Preparation**
  - Audit current presentation path: confirm scene is rendered via the main swapchain render pass and composited behind ImGui.
  - Identify how the existing `Viewport` window stores its available size (from `DockspaceResult`).
  - *Acceptance:* Notes in this plan summarising current rendering order and data flow.

- [ ] **Offscreen Render Target**
  - Create an offscreen color image + depth buffer matching the viewport size (likely using existing color attachments or a new RGBA image per frame).
  - Allocate descriptor set (or reuse ImGui texture helper) to expose the offscreen image as `ImTextureID`.
  - *Acceptance:* Offscreen framebuffer is created and cleaned alongside swapchain, validated via build/run (no rendering yet).

- [ ] **Resize Handling**
  - Detect viewport size changes each frame; rebuild the offscreen framebuffer and descriptor when dimensions change.
  - Ensure resizing waits for GPU idle before reallocating resources.
  - *Acceptance:* Logging or QA note confirming viewport resize triggers offscreen rebuild without validation errors.

- [ ] **Render Pipeline Updates**
  - Modify `SceneRenderer` to target the offscreen framebuffer (instead of swapchain) for the geometry pass.
  - Preserve existing render pass for SSAO/shadow pipelines or adapt them to sample from the offscreen image.
  - Ensure the swapchain render pass now only draws a fullscreen quad (or ImGui) that displays the offscreen texture.
  - *Acceptance:* Scene renders correctly into offscreen target; swapchain frame contains just the ImGui composite referencing that texture.

- [ ] **ImGui Integration**
  - In the `Viewport` window, call `ImGui::Image` (or `ImageButton`) with the descriptor pointing at the offscreen color attachment.
  - Clamp aspect ratio to the viewport bounds while avoiding stretching.
  - *Acceptance:* Visual QA showing scene visible within the `Viewport` panel (screenshot recorded).

- [ ] **Camera Input & Aspect**
  - Keep aspect ratio tied to viewport size (already tracked) to ensure projection matches the offscreen buffer.
  - Verify camera controls still work when viewport is undocked or resized.
  - *Acceptance:* Manual QA: pan/tilt works, no cursor capture outside viewport.

- [ ] **Cleanup & Tests**
  - Update renderer teardown to release offscreen resources.
  - Extend tests (where possible) to assert that viewport dimensions feed into renderer (e.g., new unit for aspect ratio handling).
  - Update documentation (`docs/ui_docking.md`) explaining that the viewport now displays the rendering directly.
  - *Acceptance:* Tests pass; docs updated; QA run captured.

