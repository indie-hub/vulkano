## Leather UV Seam Investigation Plan

### Goal
Identify why specific leather panels on the mid-century lounge chair sample incorrect regions of the albedo texture and deliver a verified fix that restores consistent leather grain across the model.

### Constraints
- Follow AGENTS.md conventions (angle-bracket includes, braces, const correctness, etc.).
- Work in baby steps tracked via `.agent/TODO.md`.
- All code must compile with existing CMake targets and pass tests before completion.
- Update documentation and changelog when user-facing behaviour changes.
- No new branches; commit after each TODO step is completed.

### Step-by-Step Plan
1. **Capture Baseline UV Data**
   - Instrument the importer or add a temporary debug utility to dump UV ranges for each mesh subset in the chair asset.
   - Produce a UV heatmap render (simple shader writing `uv` to color) to visually confirm the misaligned areas.
   - Save representative screenshots/logs under `.agent/`.
2. **Trace Material Assignments**
   - Inspect the imported scene graph to ensure each mesh part references the intended material index.
   - Verify texture paths and sampler settings for the affected material.
3. **Inspect UV Indexing and Flips**
   - Audit Assimp post-process flags (e.g., `aiProcess_FlipUVs`) and confirm they match our texture orientation.
   - Check whether the problematic faces have mirrored UVs or discontinuities that our tangent/UV pipeline mishandles.
4. **Isolate Mesh Subsets**
   - If the asset merges multiple panels into one mesh, split or identify subsets to confirm whether only certain index ranges exhibit issues.
   - Cross-reference with the original glTF primitives to ensure we’re not merging materials that should stay distinct.
5. **Implement UV Fix**
   - If UV flips are incorrect, adjust importer configuration or per-mesh transforms.
   - If the issue stems from incorrect texture coordinate channels, remap them during import.
   - For seams caused by non-uniform scaling, apply corrective UV transforms or regenerate coordinates where necessary (maintain original detail).
6. **Validation & Tests**
   - Update or add unit tests covering UV handling for imported meshes (e.g., ensure flipped/merged UVs remain consistent).
   - Run the renderer with the leather asset, capture “after” screenshots showing corrected panels.
   - Ensure regression tests and build all pass.
7. **Documentation & Changelog**
   - Document the fix in `docs/importing_models.md` or relevant sections.
   - Add an entry to `CHANGELOG.md` summarising the UV correction.
   - Update `.agent/TODO.md` with the next queued tasks.

### Acceptance Criteria
- UV debug visualisation shows smooth, continuous coordinates across the previously affected leather panels.
- The leather albedo renders without obvious seams, patches, or sampling from unrelated areas.
- Automated tests validating UV handling for imported assets pass locally.
- Documentation reflects the changes, and changelog entry captures the fix.
