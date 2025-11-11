## UV & Tangent Investigation Plan

### Goal
Confirm whether UV coordinates and tangent frames propagate correctly from imported assets into the renderer, diagnose any discrepancies, and implement fixes that restore physically plausible shading for textured materials.

### Constraints & References
- Adhere to C++20, SOLID, and Clean Code policies defined in `AGENTS.md`.
- Maintain `#include <...>` style includes and brace-initialisation rules.
- All updates must compile under the existing CMake configuration and pass tests before submission.
- Tests must be added/updated alongside code changes; runtime validation is required.

### Step-by-Step Plan
1. **Snapshot Current State**
   - Capture CPU-side vertex dumps (positions, UVs, tangents) for the problematic chair mesh using an instrumented debug path in `.agent/` scripts or temporary logging hooks.
   - Record the corresponding GPU staging data by inspecting mapped buffers after `SceneRenderer::upload_mesh`.
   - Collect Vulkan validation output and runtime screenshots for baseline comparison.
2. **UV Pipeline Verification**
   - Trace UV data flow from Assimp importer (`AssetImporter::import_mesh`) through `scene::MeshData`, `SceneRenderer::upload_mesh`, and vertex shader inputs.
   - Add a temporary shader variant or debug material that visualises UV coordinates (`vec3(uv, 0)`), ensuring the output lines up with the mesh surface.
   - Check for any transforms (e.g., Y-flip) applied during texture sampling or descriptor setup.
3. **Tangent Frame Audit**
   - Review `scene::compute_tangent_frames` for robustness with degenerate UVs and mirrored seams.
   - Instrument tangent generation to emit statistics (length, handedness) for meshes at load time.
   - Inspect vertex shader tangent/bitangent reconstruction logic and normal-map sampling code for orientation mismatches.
4. **Shader & Material Pipeline Review**
   - Confirm descriptor bindings align with shader expectations (texture indices, normal map usage flags).
   - Audit fragment shader normal-map decode (channel swizzles, scale/bias) to ensure it matches source texture conventions.
   - Verify metallic/roughness workflow isn’t clobbering UV channels or altering sampler states.
5. **Fix Implementation**
   - Address any importer-level UV interpretation issues (e.g., coordinate flipping) discovered in steps 2–4.
   - Update tangent generation to handle degenerate UV triangles, ensuring consistent handedness across seams.
   - Adjust shader logic (vertex/fragment) for corrected tangent frames or normal map decoding as needed.
   - Ensure changes remain compatible with procedural primitives (plane, cube, sphere) created via `MeshFactory`.
6. **Testing & Validation**
   - Add focused unit tests for tangent frame computation covering degenerate UVs, mirrored seams, and standard quads.
   - Extend integration tests (or golden-image validation where applicable) to assert UV correctness within the rendering pipeline.
   - Run full build, unit/integration tests, and smoke the runtime scene to capture updated screenshots and validation logs.
7. **Documentation & Changelog**
   - Update relevant docs (e.g., `docs/importing_models.md`) to describe UV/tangent expectations and troubleshooting tips.
   - Append a new entry to `CHANGELOG.md` summarising the UV/tangent fixes and testing performed.

### Acceptance Criteria
- UV debug visualisation renders continuous gradients without distortion across mesh seams.
- Normal-mapped materials (e.g., the mid-century lounge chair) display coherent lighting without rippling artifacts.
- Tangent generation unit tests cover at least: planar quad, mirrored UV seam, degenerate triangle fallback.
- Automated tests (unit + integration) pass locally; Vulkan validation layers report no new warnings.
- Documentation and `CHANGELOG.md` reflect the investigation and resulting fixes.
