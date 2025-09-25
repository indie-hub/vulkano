# Texture Feature Iteration Plan

## Separate Albedo & Normal Toggles
- **Goal**: Allow users to independently enable/disable albedo and normal map sampling per primitive for easier inspection.
- **UI/UX**: Replace single `Use Textures` checkbox with two checkboxes (`Use Albedo Map`, `Use Normal Map`). Disable normal-strength slider when normal map toggle is off.
- **Render Data**: Extend `PrimitiveProperties` with two booleans (`albedoEnabled`, `normalEnabled`). Adjust push constants (or descriptor selection flags) to carry both bits independently.
- **Descriptors**: Reuse existing combined material descriptor; sampling code will branch based on toggles without needing descriptor rebinding. Ensure fallback textures remain bound to avoid descriptor churn.
- **Shader Work**: Update fragment shader to test each toggle individually—skip albedo sampling if disabled, skip normal sampling (fall back to vertex normal) if normal toggle is false. Keep shadow calculations intact.
- **Validation**: Confirm toggles update live via ImGui, add focused unit/integration coverage (e.g., verify push-constant bits) or adjust smoke test logging.

## Metal-Inspired Normal Map Generator
- **Objective**: Provide a second procedural normal-map style evocative of brushed/hammered metal and allow runtime selection between the existing noisy map and the new style.
- **Generator Design**:
  - Base the metal preset on anisotropic patterns (e.g., directional noise along a dominant axis with subtle radial perturbations) to mimic brushed surfaces.
  - Mix in low-frequency sine or spiral components to emulate machining marks; keep heights shallow to avoid extreme slopes.
  - Normalise and bias into tangent space as with current generator.
- **Integration**:
  - Extend procedural generation API to accept an enum/type for `NormalMapStyle` (`RandomNoise`, `BrushedMetal`).
  - Cache GPU textures for both styles to avoid regenerating per primitive; expose choice per primitive in `PrimitiveProperties`.
  - Update ImGui to offer a combo for normal map style when normals are enabled.
- **Fallback & Assets**: Maintain current 512×512 resolution; allow seed override for random style, fixed pattern for metal to ensure recognisable result.
- **Validation**: Add unit expectations verifying gradient alignment/variance and integration test to ensure toggling updates descriptor content without validation errors.
