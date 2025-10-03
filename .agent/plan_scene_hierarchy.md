# Plan: Hierarchical Scene Graph with Nested Parent Nodes

## Interpretation of Request
The current grouping places meshes directly under a group node but lacks an explicit hierarchy that distinguishes between top-level groups and individual mesh nodes in a consistent structure. The goal is to ensure every logical group (e.g., imported asset) has a dedicated parent node, with each mesh represented as a child node beneath it. The UI and renderer should reflect this hierarchy so that manipulating the parent affects all children while children retain their local offsets.

## Acceptance Criteria
1. **Data Model**: Scene graph explicitly separates group nodes and mesh nodes; each node can have a parent transform and optional mesh payload.
2. **Importer**: For every imported asset, build a group node representing the asset root. Each Assimp mesh becomes a child node with local transform relative to the asset root.
3. **Runtime Scene**: Built-in objects (plane/cube/sphere) appear under a named group, matching the hierarchy pattern used for imports.
4. **Renderer**: Flattening traversal composes parent and child transforms, ensuring combined matrices feed the GPU exactly once per mesh.
5. **UI**: Scene Graph panel displays nested nodes (group → meshes). Editing transforms at either level updates the tree and marks descendants dirty.
6. **Tests**: Unit tests validate transform composition across parent-child chains; integration test confirms that editing group transform updates child matrices.
7. **Docs**: Documentation updated to describe the hierarchical scene graph and parent-child editing workflow.

## Step-by-Step Plan

### Phase 1 – Review & Design
1. Audit existing `SceneRenderer::SceneNode` usage and current flattening logic introduced earlier.
2. Define clear semantics for node types: group (no mesh) versus mesh (has geometry). Decide whether groups can also carry geometry.
3. Identify where scene graph instances are constructed (built-in scene, importer results, UI operations).

### Phase 2 – Data Model Enhancements
4. Extend `SceneNode` with flags or helper methods (`is_group`, `is_mesh`) to clarify node intent.
5. Adjust constructors/utility functions to enforce creation of a dedicated group node for each asset or runtime grouping.
6. Introduce helper to append mesh nodes to a parent while keeping local transforms separate from world transforms.

### Phase 3 – Importer Updates
7. Modify `AssetImporter` to produce a root node representing the imported scene/group, assigning the asset’s root transform there.
8. Ensure each Assimp mesh node becomes a child with its local transform relative to the asset root; retain mesh names for UI display.
9. Handle edge cases: scenes without meshes should still produce an empty group; missing node names fall back to generated labels.

### Phase 4 – Renderer Flattening & Dirty Flags
10. Refine flattening traversal to pass down accumulated parent matrices and store composed result on mesh nodes.
11. Introduce dirty-flag propagation from parent to children so recomputation happens only when ancestors change.
12. Update bounding-box computation to respect world transforms derived from hierarchy.

### Phase 5 – Runtime Scene Construction
13. Update application bootstrap to create a consistent hierarchy: `Root` → `Scene` → individual meshes. For imports: `Root` → `Imported/<asset>` → meshes.
14. Ensure runtime additions/removals (e.g., future features) use helper APIs to preserve structure.

### Phase 6 – UI / ImGui Panel
15. Rework Scene Graph panel to render group nodes and mesh nodes distinctly (icons or labels) and to allow transform editing at both levels.
16. When editing a group, mark subtree dirty so children world transforms update; editing children should not affect siblings or parents.
17. Add optional visibility toggles or highlighting to demonstrate hierarchy depth (optional stretch goal).

### Phase 7 – Testing & Documentation
18. Add unit tests verifying parent-child transform multiplication and dirty propagation (e.g., parent rotate updates child matrix).
19. Extend integration tests covering imported asset editing via group node.
20. Update documentation (`docs/importing_models.md` or new doc) to describe hierarchy structure and editing instructions.

### Phase 8 – Finalisation
21. Update `.agent/TODO.md` with implementation/testing tasks (already done for plan drafting).
22. After implementation, run builds/tests, validate runtime UI, and commit with reasoning summary referencing this plan.

## Risks & Mitigations
- **Deep Hierarchies**: Ensure traversal uses iterative approach or tail recursion to avoid stack issues; test with nested assets.
- **Transform Drift**: Confirm quaternion normalization and matrix decomposition remain stable after multiple edits.
- **UI Complexity**: Large scenes may overwhelm UI; consider future pagination/filtering (out of scope but note in docs).

## Verification Checklist
- [ ] Unit tests for hierarchy math succeed.
- [ ] Integration test confirms parent edits propagate to children.
- [ ] Manual QA shows Scene Graph UI preserving hierarchy and transforms.
- [ ] Documentation reflects new workflow.
