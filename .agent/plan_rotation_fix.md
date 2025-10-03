# Plan: Correct Parent Rotation Behaviour

## Observed Issue
Rotating a parent node causes children to orbit the world origin, indicating their transforms are stored in world space or the hierarchy composes matrices incorrectly. We already noted this in the rotation audit; now we need a focused implementation plan.

## Acceptance Criteria
1. Rotating any parent node (built-in scene or imported asset group) rotates children around the parent pivot with no unintended orbit.
2. Child nodes retain local offsets (position/scale) relative to the parent even after multiple edits.
3. Existing assets render identically when parent transforms are identity.
4. UI displays and edits local transforms only; world matrices are derived during flattening.
5. Unit/integration tests validate parent-child transform composition (including rotations).

## Baby-step Plan

### Phase 1 – Confirm Current Behaviour
1. Instrument flattening (`flatten_nodes`) to log matrices for parent + child; verify world matrix used to create mesh.
2. During runtime, rotate a parent and capture resulting child world matrix to reproduce orbit pattern.

### Phase 2 – Compute Local Transforms Properly
3. Ensure importer builds tree recursion: for each Assimp node, compute local matrix relative to parent (use `parentInverse * world`).
4. Update `SceneNode` creation in the application to use provided local transforms directly (avoid re-decomposing world matrices).
5. Remove any code that overwrites `SceneNode::transform` with world matrices during flattening/UI editing.

### Phase 3 – Flattening & Dirty Flags
6. Confirm flattening multiplies `parentMatrix * localMatrix` only, without writing back to `SceneNode` local transform.
7. Introduce `worldMatrix` cache or stack local (without storing) so we never mutate locals.
8. Ensure dirty flags propagate: when editing a parent local transform, mark subtree dirty; children stay the same locally.

### Phase 4 – UI Adjustments
9. In the Scene Graph panel, show local transforms for both groups and meshes; avoid duplicate edits (only local, once per node).
10. Optionally add read-only world transform display for debugging.

### Phase 5 – Testing
11. Add unit test: parent rotation + translation + scale applied to child offset yields expected world point (extend existing test to use non-uniform transforms).
12. Add integration test: load simple hierarchy, rotate parent, assert GPU matrix matches expected (if feasible) or rely on unit tests plus manual QA.

### Phase 6 – Validation & Documentation
13. Manual QA with rotation video scenario to ensure corrected behaviour.
14. Update docs (Scene Graph section) explaining transforms are local; mention parent editing rotates children at pivot.
15. Finish by updating `.agent/TODO.md`, running builds/tests, and committing with summary.

## Risks & Mitigations
- Non-invertible parent matrices (zero scale) -> fallback to identity or warn user.
- FP rounding when decomposing -> normalize quaternions and clamp scales.
