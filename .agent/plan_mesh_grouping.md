# Plan: Parent Transform Nodes for Imported Mesh Groups

## Goal
When importing or composing complex models, group multiple meshes under a shared parent transform so position/rotation/scale adjustments can be applied at the group level without losing per-mesh offsets. Maintain runtime editability and GPU correctness.

## Current State
- Each `SceneRenderer::SceneMesh` owns its own `scene::Transform`; there is no hierarchy.
- Assimp-imported scenes append meshes directly with baked transforms.
- ImGui exposes per-mesh transforms only.
- GPU buffers store a single model matrix per mesh; no parent composition.

## Desired Behaviour
1. Imported model produces a logical “group node” with its original transform; meshes below inherit parent transform while keeping individual offsets.
2. UI displays group entries with aggregate controls; editing the group updates child matrices live.
3. Runtime can add/remove meshes to a group; groups can be toggled or reset without data loss.
4. GPU updates apply combined matrices (parent * child).
5. Tests cover hierarchy math, ensuring detach scenarios maintain expected transforms.

## Acceptance Criteria
- **Hierarchy Data Model**: New structures represent parent-child relationships (e.g., `SceneNode` with children). Child world matrices recompute from parent+local transforms.
- **Import Pipeline**: Assimp loader builds node hierarchy mirroring scene graph, preserving mesh local transforms, and attaches a parent node for dynamic control.
- **Renderer Compatibility**: GPU upload path traverses hierarchy, submitting flattened meshes with composed matrices without redundant allocations.
- **UI Controls**: ImGui exposes parent nodes with collapsible child lists, allowing edits to group transform and per-mesh overrides.
- **Runtime Integrity**: Deleting a group (or clearing children) frees associated GPU buffers; no dangling references.
- **Testing**: Unit tests for hierarchy matrix composition + group operations; integration test validating grouped transform editing changes GPU matrices.
- **Documentation**: Update docs describing parent transform editing and import behaviour.

## Step-by-Step Plan

### Phase 1 – Data Model & Hierarchy Backbone
1. Introduce `scene::SceneNode` struct holding `Transform`, optional mesh pointer/index, and child indices.
2. Modify `SceneRenderer` to accept a list/tree of nodes instead of flat meshes (temporarily maintain compatibility by building a root node for existing content).
3. Implement traversal utilities (e.g., depth-first flatten) producing GPU submission list with cached world matrices.

### Phase 2 – Import & Scene Construction
4. Extend `AssetImporter` to emit node hierarchy (use Assimp nodes). Each import returns root node transform + mesh nodes.
5. Update application startup: create root node (`SceneGroup`) for static scene (plane/cube/sphere). Group imported meshes under a dedicated parent node labelled by asset filename.
6. Provide functions to append meshes to groups, reusing GPU buffers when possible.

### Phase 3 – GPU / Renderer Integration
7. Refactor `SceneRenderer::set_scene` to accept root nodes, recompute world matrices recursively, and upload flattened GPU meshes (maintain bounding box updates based on composed transforms).
8. Update command buffer recording (scene, shadows) to use flattened data; ensure caches/gizmos operate with new structure.
9. Cache world matrices per node, recomputing only when parent or local transform dirty flag set.

### Phase 4 – UI / Interaction
10. Add ImGui “Scene Graph” panel listing root groups and child meshes.
11. Implement transform editors at both group and child levels; editing group marks subtree dirty; editing child only applies locally.
12. Provide rename, visibility toggle, and reset options for groups (optional but beneficial). Ensure resets propagate correctly.

### Phase 5 – Runtime APIs & Maintenance
13. Expose helper functions to create/destroy groups at runtime (e.g., for future scripting).
14. Ensure deletion cleans GPU buffers, updates bounding volume, and removes ImGui entries without crashes.

### Phase 6 – Testing & Documentation
15. Add unit tests for:
    - Hierarchy composition (`parent` + `child` transforms).
    - Dirty flag propagation ensuring minimal recomputation.
16. Extend integration tests verifying import produces group node and editing parent transform updates child matrices.
17. Document new workflow (`docs/importing_models.md`, README) with instructions for manipulating groups.
18. Capture manual QA notes verifying UI updates and performance (no redundant GPU uploads).

### Phase 7 – Finalisation
19. Update `.agent/TODO.md` with implementation/testing tasks.
20. Commit once functionality + tests + docs complete, including reasoning summary referencing this plan.

## Risks & Mitigations
- **Traversal Performance**: Large hierarchies may cause rebuild spikes; mitigate with dirty flags & iterative stacks to avoid recursion depth issues.
- **Assimp Node Complexity**: Scenes may have deep nesting or missing transforms; add safeguards (fallback to identity, log warnings).
- **UI Overload**: For massive scenes, tree could be unwieldy; consider collapsible nodes by default and search filter (future work).

## Verification Checklist
- [ ] Unit tests (hierarchy math) pass.
- [ ] Integration test confirms grouped transforms update world matrices.
- [ ] Manual QA: import complex model, adjust parent transform via UI, observe expected behaviour without validation errors.
- [ ] Documentation updated and linted.
