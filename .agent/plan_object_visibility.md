# Plan: Toggle Scene Object Visibility via UI

## Goal
Allow users to hide or show individual scene nodes (including mesh instances and groups) at runtime through the UI, ensuring render commands and GPU buffers respect the visibility state.

## Step-by-Step Tasks

1. **Catalogue Affected Systems**
   - Review `SceneRenderer::SceneNode`, mesh flattening, material/light registries, and ImGui scene graph panel to understand current data flow.
   - Document which layers (scene registry, renderer, GPU buffers) need a visibility flag to prevent stale draw submissions.
   - *Acceptance:* Short note in this plan enumerating each component requiring visibility awareness.

2. **Extend Scene Data Model**
   - Add a `visible` boolean to `SceneRenderer::SceneNode` (and corresponding runtime scene structures) with default `true`.
   - Update loaders/importers to initialise the flag, ensuring serialization (if any) preserves state.
   - *Acceptance:* Code compiles; all constructors/transformations set visibility explicitly.

3. **Propagate Visibility into Renderer Preparation**
   - Modify the node-flattening logic to skip meshes marked invisible and to carry visibility down the hierarchy (hidden parent hides descendants).
   - Ensure material/light registries and gizmo caches skip updates for hidden meshes/lights.
   - *Acceptance:* GPU mesh upload path logs/skips invisible nodes; unit tests covering flattening updated/passing.

4. **Integrate Visibility with GPU Descriptors**
   - Ensure per-frame buffers (material/light, shadow resources) exclude hidden entities and reclaim resources when visibility toggles off.
   - Guarantee SSAO, shadow, and gizmo passes don't draw hidden geometry.
   - *Acceptance:* Frame capture shows draw call count dropping when objects hidden; no validation errors from dangling descriptors.

5. **UI Controls**
   - Update the Scene Graph panel to include a visibility checkbox for each node (group + mesh).
   - Add context menu or toolbar action for “Hide/Show Selected” and ensure the checkbox reflects real state.
   - *Acceptance:* Interacting with the checkbox immediately toggles visibility in the viewport.

6. **Persistence (Optional if required)**
   - Decide whether visibility should persist between runs (e.g., via layout/profile file). If so, serialise the flag; otherwise, reset to visible on startup but document behaviour.
   - *Acceptance:* Behaviour documented; persistence implemented if chosen.

7. **Testing & Validation**
   - Add unit tests for flattening/visibility propagation and an integration test that toggles node visibility and checks renderer state (e.g., draws recorded or buffer counts change).
   - Run Vulkan validation with repeated toggles to ensure no resource leaks.
   - *Acceptance:* `ctest` passes; validation run (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VULKANO_MAX_FRAMES=1 ./bin/vulkano_renderer`) reports clean.

8. **Documentation Update**
   - Update `docs/ui_docking.md` (or scene documentation) with instructions on using visibility toggles, including screenshots if helpful.
   - *Acceptance:* Doc committed with clear steps; QA checklist references visibility workflow.

## Notes
- Hidden nodes should still exist in the scene graph for transforms and potential reactivation.
- Consider animating visibility transitions later (e.g., fade) but keep scope to binary toggle.

## Acceptance Summary
- Visibility flag present across scene data structures with default `true`.
- Renderer skips draw submissions and GPU updates for hidden nodes.
- ImGui scene graph provides intuitive controls that reflect state immediately.
- Tests and validation remain green after rapid toggling routines.
- Documentation explains the feature and any persistence limitations.
