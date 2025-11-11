# Shadow Toggle Runtime Validation

Follow this checklist to validate per-light shadow toggles in the sample
renderer. Perform the steps on each target platform build (macOS, Windows,
Linux) prior to release.

## Test Bed Preparation
- Launch `vulkano_renderer` with default scene assets.
- Open the **Lighting** ImGui panel and enable **Show Light Gizmo**.
- Ensure at least two directional lights exist: keep the default sun, then add
  a second directional light using the panel button. Leave one point light
  active for regression coverage.

## Validation Steps
1. **Baseline Shadow Capture**
   - Verify the default directional light casts shadows on all meshes.
   - Enable **Debug Shadow Map** and screenshot the viewport plus debug panel.
   - Record light direction, intensity, and bias values.
2. **Toggle Primary Light Off**
   - Uncheck `Casts Shadow` on the default directional light.
   - Confirm the scene renders without shadows and the debug shadow map view
     clears to white.
   - Re-toggle the checkbox and confirm shadows return with identical bias
     artefacts to the baseline screenshot.
3. **Alternate Caster Validation**
   - Disable shadows on the first light.
   - Enable `Casts Shadow` on the second directional light.
   - Confirm the shadow map updates to the new light’s orientation and shadows
     fall in the expected direction.
   - Capture a screenshot and log light direction to verify matrix change.
4. **Rapid Toggle Stress**
   - Alternate the shadow checkbox between both lights every frame for ~10
     seconds.
   - Watch for flicker, stale shadow maps, or command buffer validation errors
     in the console.
5. **Point Light Regression**
   - Ensure point lights ignore shadow toggles (checkbox hidden) and continue to
     render gizmos without shadow artefacts.
6. **Removal Scenario**
   - Delete the second directional light while it is the active caster.
   - Confirm the renderer falls back to the default light and shadow debug view
     updates accordingly.

## Reporting
- Store screenshots and parameter notes in the release QA bundle.
- Document any artefacts with reproduction steps and attach RenderDoc captures
  if applicable.
- Mark the TODO checklist item complete once all steps pass on every platform.
