# Plan: Audit Parent-Child Rotation Composition

## Interpretation
When rotating a parent node in the Scene Graph, children orbit the origin instead of rotating around the parent’s pivot. This suggests the hierarchy either stores world-space transforms for children or recomposes matrices incorrectly (e.g., applying parent at the wrong side, decomposing/localizing in world space, or re-normalizing with inverse). We need to audit transform math, pinpoint the bug, and adjust storage/composition so children truly inherit their parent’s frame.

## Acceptance Criteria
1. **Correct Pivot Behaviour**: Rotating a parent node spins children around the parent’s pivot point, maintaining local offsets.
2. **Consistent Transform Storage**: Child nodes hold local transforms relative to their parent; world transforms are computed only during flattening.
3. **Importer Consistency**: Imported meshes remain positioned correctly after fix; no visual regression when parent transforms are identity.
4. **UI Accuracy**: Scene Graph sliders reflect local transforms (position/rotation/scale) without drifting.
5. **Tests**: Add unit tests verifying composed rotation/translation; optional integration test to confirm runtime behaviour.
6. **Documentation**: Update notes if user workflow changes (e.g., emphasising local transforms).

## Audit Steps

### Phase 1 – Analysis
1. Review `SceneRenderer::flatten_nodes` to confirm matrix multiplication order (`parent * local`). Ensure no transpose/inverse sneaks in downstream.
2. Examine UI editing path: confirm `editTransform` modifies local transform and marks scene dirty; check for accidental world-space writes.
3. Inspect importer: confirm child nodes store local transform (not world). Verify no inverse root transform is applied incorrectly.
4. Look at application start-up: built-in nodes set local transforms (position/rotation/scale) relative to parent.

### Phase 2 – Reproduction & Logging
5. Run the renderer, rotate a parent, and capture actual world matrices by adding debug logs or temporary ImGui display of computed matrices.
6. Compare `parent.matrix()` and `child.transform.matrix()` to expected values; compute local re-derived transform = `inverse(parent) * world` to detect mismatch.
7. Check if child local transforms are being overwritten by world transforms during flattening or UI editing.

### Phase 3 – Fix Strategy
8. If child transforms are world-based, change importer/setup to compute local transform = inverse(parent) * world (with robust inversion handling).
9. Ensure flattening multiplies `parentMatrix * localMatrix` and never writes back to local transforms.
10. For UI editing, provide optional world-space display but write to local transform only; avoid recomputation loops.

### Phase 4 – Implementation Steps
11. Adjust importer to compute hierarchical locals using the actual parent transform chain (recurse along Assimp nodes).
12. Update application grouping code to compute child locals from world data when building nodes (if necessary).
13. Verify flattening/time-critical code doesn’t mutate node transforms; mark dirty flags instead.

### Phase 5 – Testing & Validation
14. Add unit tests covering rotation composition: parent rotates, child offset expected (similar to existing test but using inverse to compute local).
15. Add integration smoke test (Catch2 or runtime QA) validating rotation pivot behaviour (e.g., rotate 90 degrees and assert world positions match expected values).
16. Manual QA with sample asset to ensure visual correctness.

### Phase 6 – Documentation & Final Steps
17. Document behaviour in Scene Graph section (local vs world transforms).
18. Update `.agent/TODO.md` tasks for implementation/testing after plan approval.
19. Run build/tests and commit with reasoning referencing this plan.

## Risks & Mitigations
- **Non-invertible Parents**: When parent scale contains zero/negative values, inversion may fail; handle by fallbacks or warnings.
- **Floating-Point Drift**: Repeated conversions between matrices/quaternions can cause drift; normalize rotations and clamp scales.
- **Complex Imports**: Deep hierarchies may expose additional issues; test with multiple assets.

## Verification Checklist
- [ ] Parent rotation spins children around parent pivot in runtime.
- [ ] Unit/integration tests covering rotation pass.
- [ ] No regressions in existing scenes (built-in meshes unchanged when parent transform identity).
- [ ] Documentation updated.
