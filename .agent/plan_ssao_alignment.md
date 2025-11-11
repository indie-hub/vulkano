# Plan: Correct SSAO Alignment in Docked Viewport

## Goal
Ensure the SSAO pass samples g-buffer data in the same coordinate space and resolution as the embedded viewport so contact shadows sit directly beneath geometry, without offsets when the viewport size differs from the swapchain.

## Step-by-Step Plan

1. **Reproduce & Instrument Current Behaviour**
   - Launch the renderer with validation enabled and vary the viewport size (dock/undock, resize).
   - Capture screenshots plus note SSAO slider values to confirm the occlusion blob drifts relative to geometry.
   - *Acceptance:* A short observation log describing the misalignment under at least two viewport sizes (default and a reduced panel).

2. **Audit Extent & Uniform Wiring**
   - Trace how viewport extent flows into `SceneRenderer`, `SSAOPass`, `SSAOBlurPass`, and `SSAODescriptors::set_camera_parameters`.
   - Confirm whether any SSAO resource still uses the swapchain extent (current code paths: `context.swapchain_extent()` during init/resize) and whether noise scale / projection uniforms are updated when the viewport changes.
   - *Acceptance:* Written notes summarising each extent source, pinpointing mismatches.

3. **Refactor SSAO Resource Resizing**
   - Update SSAO & blur passes to accept the viewport extent supplied by `SceneRenderer::set_viewport_extent`.
   - Ensure resizing waits for GPU idle, recreates images/framebuffers/descriptors, and keeps descriptor bindings in sync with new g-buffer views.
   - *Acceptance:* Code compiles; SSAO/blur resources now expose a `resize(VkExtent2D viewportExtent)` entry point distinct from swapchain events.

4. **Synchronise Uniform Parameters with Viewport Changes**
   - Recompute SSAO noise scale, projection/inverse matrices, and attenuation uniforms whenever the viewport extent changes.
   - Guarantee the camera aspect ratio and projection matrix passed to SSAO remain consistent with the viewport dimensions (no reliance on swapchain extent).
   - *Acceptance:* Uniform updates triggered by viewport resize verified via logging/asserts in debug build (removed afterwards).

5. **Adjust SSAO Composite & Blur Pipelines**
   - If necessary, resize composite target(s) and blur ping-pong images to the viewport extent; ensure descriptor image writes are refreshed.
   - Validate that blur kernels operate on the new resolution without reading undefined regions.
   - *Acceptance:* No validation warnings during viewport resizing with SSAO enabled.

6. **Validation & Visual QA**
   - Run `VULKANO_MAX_FRAMES=1 ./bin/vulkano_renderer` plus the validation-layer variant after resizing the viewport and toggling SSAO.
   - Capture before/after screenshots showing the occlusion correctly centred beneath the sphere and other test objects.
   - *Acceptance:* Validation passes cleanly; QA notes confirm visual alignment.

7. **Testing & Documentation**
   - Extend unit/integration coverage (e.g., a test ensuring `SSAODescriptors::set_camera_parameters` stores the provided viewport extent) or add a regression test harness if feasible.
   - Update `docs/ui_docking.md` (or a dedicated SSAO note) summarising the new viewport-coupled SSAO behaviour and any tuning tips.
   - *Acceptance:* Tests pass locally; documentation reflects the fix.

## Deliverables
- Code changes aligning SSAO sampling with the viewport resolution.
- Updated tests/docs plus QA artefacts demonstrating correct SSAO placement.
- Clean Vulkan validation run with SSAO enabled post-resize.
