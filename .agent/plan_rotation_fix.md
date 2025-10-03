# Plan: Correct Parent Rotation Behaviour

## Observed Issue
Rotating a parent node causes children to orbit the world origin, which means child transforms are effectively stored in world space or the hierarchy composes matrices incorrectly. We already noted this in the rotation audit; this document lays out the detailed implementation plan.

## Acceptance Criteria
1. Rotating any parent node (built-in scene or imported group) rotates children around the parent pivot instead of orbiting the origin.
2. Built-in meshes and imported assets retain their original world poses when parent transforms are identity.
3. Scene Graph edits/ displays local transforms only; world matrices are composed at render time.
4. Scale sliders show five decimal places and accept `1e-5` min values.
5. Unit/integration tests cover parent-child transform composition.
6. Documentation explains the local nature of Scene Graph transforms.

## Baby-Step Plan

### Phase 1 – Diagnostics
1. Instrument `SceneRenderer::flatten_nodes` to print parent/child matrices when rotating a parent to confirm children carry world transforms.
2. Optionally add an ImGui debug table showing local vs world positions during runtime for visual confirmation.

### Phase 2 – Fix Transform Storage
3. Ensure the Assimp importer records the root node transform and stores each mesh’s **local** `aiNode::mTransformation`.
4. When building runtime `SceneNode`s (built-in scene + imports):
   - Assign the root node’s transform directly to the parent group.
   - Convert child world transforms to local using `inverse(parentWorld) * world`; fallback to identity (with a warning) if inversion fails.
5. Guarantee no code (UI, renderer) overwrites `SceneNode::transform` with world matrices.

### Phase 3 – Renderer Flattening
6. Confirm `flatten_nodes` computes `worldMatrix = parentWorld * localMatrix` and never mutates node locals.
7. Use a stack/cached approach if needed to avoid extra allocations while traversing.

### Phase 4 – UI Adjustments
8. Scene Graph panel edits local transforms once per node, marking `sceneDirty` once; groups label the controls as "Local Transform".
9. Keep the scale slider format at `"%.5f"` with minimum `1e-5` so tiny values remain visible.
10. Optional: add read-only world transform display for debugging (stretch goal).

### Phase 5 – Testing
11. Extend `tests/transform_tests.cpp` with parent-child rotation/translation/scale cases.
12. Add an integration smoke test (or scripted QA scenario) that rotates a parent 90° and verifies expected child world positions/matrices.
13. Manual QA replicating the rotation video scenario with both built-in and imported meshes.

### Phase 6 – Documentation & Cleanup
14. Update `docs/importing_models.md` (Scene Graph section) to emphasise displayed transforms are local and group edits affect subtrees.
15. Remove instrumentation, rebuild (`cmake --build build`), run tests (`ctest --test-dir build`), and capture validation screenshots.
16. Commit with a message summarising the fix and referencing this plan.

## Risks & Mitigations
- **Non-invertible parents** (zero scale): fallback to identity locals, log warning.
- **FP drift**: normalize quaternions, clamp scales when editing.
- **Complex hierarchies**: test with multiple assets and nested groups to ensure stability.

