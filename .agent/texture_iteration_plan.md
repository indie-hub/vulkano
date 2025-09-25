# Texture Feature Iteration Plan

## Separate Albedo & Normal Toggles
- **Goal**: Allow users to independently enable/disable albedo and normal map sampling per primitive for easier inspection.
- **UI/UX**: Replace single `Use Textures` checkbox with two checkboxes (`Use Albedo Map`, `Use Normal Map`). Disable normal-strength slider when normal map toggle is off.
- **Render Data**: Extend `PrimitiveProperties` with two booleans (`albedoEnabled`, `normalEnabled`). Adjust push constants (or descriptor selection flags) to carry both bits independently.
- **Descriptors**: Reuse existing combined material descriptor; sampling code will branch based on toggles without needing descriptor rebinding. Ensure fallback textures remain bound to avoid descriptor churn.
- **Shader Work**: Update fragment shader to test each toggle individually—skip albedo sampling if disabled, skip normal sampling (fall back to vertex normal) if normal toggle is false. Keep shadow calculations intact.
- **Validation**: Confirm toggles update live via ImGui, add focused unit/integration coverage (e.g., verify push-constant bits) or adjust smoke test logging.
