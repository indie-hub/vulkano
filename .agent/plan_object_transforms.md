# Plan: Object Transform Parameter Support (Position, Rotation, Scale)

## Goal
Introduce per-object transform parameters (translation, rotation, scaling) that can be authored, edited, persisted, and rendered consistently across CPU data structures, GPU buffers, and UI controls.

## Context & Assumptions
- Current `SceneRenderer::SceneMesh` (and related material/mesh logic) assumes static model matrices.
- ImGui panels already expose some scene controls; new transform controls should integrate without breaking existing layout.
- Transformation values must survive model imports (Assimp) and manual scene authoring paths.
- Engine uses column-major matrices (glm defaults) and right-handed coordinate system.
- Rotation should be user-friendly (Euler in degrees) but stored internally as matrices/quaternions.
- Scaling must be uniform and non-uniform, defaulting to `1,1,1`.

## Constraints
- Adhere to C++20, SOLID, Clean Code guidelines from AGENTS.md.
- Maintain descriptor/synchronisation budget (reusing existing buffers where possible).
- Avoid regressions in current rendering or Assimp import pipeline.
- Keep GPU buffer updates deterministic (no per-frame heap churn).

## Acceptance Criteria
1. **Data Model**: Each object stores position (vec3), rotation (quaternion or Euler), and scale (vec3); defaults match current behaviour (identity transform).
2. **GPU Sync**: Model matrices derived from parameters are uploaded to GPU buffers accurately and used by shaders.
3. **User Controls**: ImGui exposes editable transform controls (position sliders, rotation degrees, scale vectors) with validation and reset to defaults.
4. **Import/Export**: Imported models respect their original transform nodes; manual scene meshes maintain editable parameters.
5. **Tests**: Unit tests cover transform component defaults and matrix generation; integration tests confirm rendering updates when parameters change.
6. **Docs**: Developer guide updated with instructions for manipulating object transforms.

## Step-by-Step Plan

### Phase 1 – Data & API groundwork
1. Audit current structures (`SceneRenderer::SceneMesh`, `scene::MeshData`, application scene setup) for transform usage.
2. Introduce a dedicated `Transform` struct (position/rotation/scale) with sensible defaults and helper methods (e.g., `to_matrix`).
3. Replace raw model matrices in CPU-side structures with `Transform`, keeping backward-compatible conversion where necessary.
4. Update constructors/factories to populate transforms and adjust existing call sites (plane/cube generation, Assimp importer, etc.).

### Phase 2 – GPU pipeline updates
5. Extend material/mesh update path to compute model matrices from transforms prior to buffer upload; ensure minimal allocations.
6. Adjust shader push-constants or storage buffers to accept updated matrices (no API changes expected, but revalidate alignment).
7. Verify command buffer recording still binds correct matrices; add debug logging or assertions if needed.

### Phase 3 – UI & Interaction
8. Add ImGui panel components for editing transforms per object (position XYZ, rotation XYZ in degrees, scale XYZ).
9. Implement change detection wiring to flag renderer/material buffers dirty when transforms change.
10. Provide reset-to-identity actions and clamp/normalise rotation input to avoid drift.

### Phase 4 – Import & Persistence
11. Map Assimp node transforms into the new `Transform` struct during import (decompose matrix into translation/rotation/scale).
12. Ensure manual scene assembly (hard-coded meshes) sets explicit transforms equal to previous matrices to prevent layout changes.

### Phase 5 – Testing & Documentation
13. Write unit tests for `Transform::to_matrix` and for decomposition logic (if added).
14. Add integration test (Catch2) that modifies transforms and asserts updated GPU/model matrix results.
15. Update docs (`docs/importing_models.md` or new section) describing transform editing workflow.
16. Perform runtime validation (manual QA) confirming gizmos/lighting remain correct.

### Phase 6 – Finalisation
17. Review TODO.md, mark tasks, and ensure commits include reasoning summary referencing this plan.
18. Gather screenshots or notes if UI changes significantly (optional but recommended).

## Risks & Mitigations
- **Gimbal Lock**: Using Euler inputs may introduce lock—mitigate by storing rotations as quaternions internally and converting for UI.
- **Performance**: Per-frame matrix recomputation must be efficient; cache matrices and recompute only when inputs change.
- **Import Variability**: Some assets have non-uniform scales or negative determinants; add safeguards (e.g., fallback to identity if decomposition fails).

## Verification Checklist
- [ ] Unit tests pass locally (`ctest`).
- [ ] Renderer runs with sample scene; transforms editable at runtime without validation errors.
- [ ] Assimp-imported model positions/rotations match source asset.
- [ ] Documentation updated and linted.
