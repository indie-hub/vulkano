# Metallic & Occlusion Map Enhancement Plan

## 1. Assess Current Material Data Flow
- Review CPU material structs, GPU material buffer layout, texture cache bindings, and shader usage for metallic/roughness/occlusion data.
- Record how scalar metallic/roughness values and existing textures move from CPU → GPU → shader.
- **Acceptance:** Summary captures current flow and gaps (unused channels, redundant uniforms).

## 2. Define Enhanced Texture Schema
- Decide channel packing for new maps (e.g., R=metallic, G=roughness, B=occlusion) and default fallback behaviour.
- Extend `scene::Material` with additional metadata/flags for metallic & occlusion textures.
- **Acceptance:** Schema documented with defaults, validation rules, and compatibility notes.

## 3. Extend Material Registry & Serialization
- Add path fields/toggles to `scene::Material` and ensure serialization/deserialization handles new data gracefully.
- Update unit tests to confirm backwards compatibility and correct persistence.
- **Acceptance:** Tests prove legacy materials load; new fields persist correctly.

## 4. Augment Texture Loading & Cache
- Recognise metallic/occlusion textures in loading pipeline; create GPU images/samplers with consistent formats.
- Provide fallback textures when sources are missing; ensure cache invalidation updates descriptors.
- **Acceptance:** Loader tests cover success/failure paths; cache returns valid handles for new textures.

## 5. Expand Material Buffer & Descriptor Layout
- Update GPU material buffer struct to include metallic/occlusion texture indices and factors.
- Adjust descriptor arrays/push constants to bind additional textures without exceeding limits.
- **Acceptance:** Buffer size calculations updated; descriptors bind new slots; Catch2 tests validate struct packing.

## 6. Update Shaders
- Modify fragment shader to sample metallic/occlusion maps, blending with scalar properties for BRDF and ambient terms.
- Ensure shader guards handle missing textures by falling back to scalar values.
- **Acceptance:** Shaders compile; rendered output reacts correctly to texture presence (manual QA screenshot or checksum).

## 7. ImGui & Authoring Workflow
- Add UI controls for selecting metallic/occlusion textures, toggling usage, and previewing channels.
- Ensure changes propagate to runtime registry and hot-reload path.
- **Acceptance:** UI supports browse/remove operations; toggles stay in sync with material data.

## 8. Testing & Validation
- Extend Catch2 suite for material registry, buffer, and shader configuration around new maps.
- Add integration smoke test (frame capture or checksum) showcasing metallic/occlusion effects; run with validation layers to ensure cleanliness.
- **Acceptance:** New tests pass; validation output remains clean; screenshot/checksum documents visual change.

## 9. Documentation & QA Updates
- Update materials README/CHANGELOG with workflow for metallic/occlusion maps.
- Append QA checklist scenarios: fallback behaviour, combined map usage, runtime toggles.
- **Acceptance:** Docs merged; QA list covers at least three representative scenarios.
