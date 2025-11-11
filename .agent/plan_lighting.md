# Multi-Light Expansion Plan

## Goal
Support multiple active lights in the scene renderer (e.g., add a second directional or point light) while keeping the material/lighting UIs manageable and ensuring Vulkan resources remain validation-clean.

## Step-by-Step Plan

1. **Lighting Data Model Update**
   - Extend `scene::Light` to tag light types (directional, point) and add intensity/color falloff parameters as needed.
   - Update `LightRegistry` to store multiple lights, expose enumeration helpers, and maintain stable IDs.
   - Acceptance: Unit tests cover adding/removing multiple lights and retrieving ordered lists.

2. **GPU Light Buffer Expansion**
   - Revise `LightGpu` structure to include light type, position, and range for non-directional lights.
   - Rebuild SSBO layout (`LightBuffer`) to upload arrays of lights; handle alignment padding.
   - Acceptance: `LightBuffer` updates deliver N lights without overruns; tests verify buffer sizing.

3. **Shader Adjustments**
   - Update `shaders/scene.frag` to loop over the light array, handling directional vs point lighting (Lambert + Cook-Torrance contributions).
   - Introduce attenuation for point lights and ensure SSAO multiplier remains compatible.
   - Acceptance: Shader compiles; basic scenes show contributions from multiple lights.

4. **Scene Renderer Integration**
   - Modify `SceneRenderer` descriptor bindings to supply the larger light buffer; adapt push constants if per-light data needed.
   - Ensure command recording binds the light SSBO once; confirm descriptor pool sizes remain adequate.
   - Acceptance: Application runs with updated descriptor set; Vulkan validation reports clean.

5. **ImGui Controls**
   - Expand Lighting panel to list all lights with collapsible sections for properties (type, direction/position, color, intensity, range).
   - Provide controls to add/remove lights, switch types, and adjust parameters safely (with clamp ranges).
   - Acceptance: UI edits update the registry in real time; buffer rebuild occurs only when data changes.

6. **Runtime Wiring**
   - On startup, create two lights (e.g., sun + fill light) with distinct colors/angles to demonstrate interplay.
   - Update runtime loop to detect registry changes and refresh GPU buffer once per frame when dirty.
   - Acceptance: Visual inspection shows both lights affecting scene; toggling them off/on works.

7. **Testing & Validation**
   - Add unit/integration tests for light buffer packing and shader math (e.g., verifying per-light contributions in isolation).
   - Rebuild, run `ctest`, and execute the renderer with validation layers to ensure no warnings or crashes.
   - Acceptance: All tests pass; runtime remains stable through light edits and toggles.

8. **Documentation**
   - Update `assets/README.md` (if new light-related assets), `docs/lighting.md` (if created) with usage notes and limitations.
   - Record ImGui usage tips (e.g., typical intensity ranges) in README or docs.
   - Acceptance: Documentation merged with code, providing guidance for future contributors.

## Notes
- Consider future work for spotlights and area lights; keep data structures extensible.
- Monitor performance impacts when increasing light count; consider compute-based deferred lighting if necessary.
