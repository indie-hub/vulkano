# Assimp Integration Plan (Baby Steps)

## Phase 0 – Prep & Research
1. **Confirm Supported Features**
   - Inspect existing `scene::MeshData`, `scene::Material`, `SceneRenderer::SceneMesh` to identify mandatory attributes (positions, normals, tangents, UVs, indices, material IDs).
   - Note missing pieces (e.g., bone/animation support) that we will explicitly defer.
   - **Acceptance:** Notes appended to this document listing: required vertex attributes, material fields, and unsupported features flagged as future work.
2. **Choose Sample Assets**
   - Collect at least two small test models (single-mesh OBJ, multi-mesh glTF) to keep fixtures lightweight.
   - **Acceptance:** Asset filenames recorded here with expected mesh/material counts for reference.

## Phase 1 – Build Integration
1. **FetchContent Blueprint**
   - Draft minimal CMake snippet using `FetchContent` for Assimp (set `ASSIMP_BUILD_TESTS OFF`, `ASSIMP_BUILD_TOOLS OFF`, `ASSIMP_BUILD_SAMPLES OFF`).
   - **Acceptance:** Snippet pasted into this plan with confirmatory note about flags.
2. **CMake Integration**
   - Modify top-level `CMakeLists.txt`: declare FetchContent, add Assimp target, link `vulkano_app` against `assimp`.
   - Add header include dirs if required.
   - **Acceptance:** `cmake --build` succeeds on clean tree; `git status` shows no CMake errors.
3. **License/Third-Party Docs**
   - Drop pointer in docs/third_party.md (or create file) acknowledging Assimp usage and license ref.
   - **Acceptance:** Documentation entry committed.

## Phase 2 – Loader Scaffolding
1. **Create Asset Importer Interface**
   - Add `include/vulkano/app/asset_importer.hpp` with a simple interface (`load_scene(std::string_view path) -> ImportedScene`).
   - Define `ImportedMesh`, `ImportedMaterial`, `ImportedScene` structs (CPU-only for now).
   - **Acceptance:** Header compiles; unit placeholder test ensures struct sizes.
2. **Assimp Wrapper Implementation**
   - Add `src/app/asset_importer.cpp` using Assimp: `Assimp::Importer importer; importer.ReadFile(...)` with `aiProcess_Triangulate`, `aiProcess_GenNormals`, `aiProcess_CalcTangentSpace`, etc.
   - Parse `aiScene` into intermediate structures without touching renderer yet.
   - **Acceptance:** Load simple OBJ; `ImportedScene.meshes.size()` and vertex counts match expected constants (unit test).
3. **Error Handling**
   - Ensure missing file, unsupported format, or zero-mesh scenes throw descriptive exceptions.
   - **Acceptance:** Unit test asserts thrown message contains file path hint.

## Phase 3 – Mesh Conversion
1. **Mesh Builder**
   - Implement helper converting `aiMesh` to `scene::MeshData` (positions, normals, tangents (if present), UVs, indices (uint32_t)).
   - Generate fallback tangents if Assimp didn’t produce them (simple orthogonal basis from normals/UVs).
   - **Acceptance:** Unit test loads sample mesh and checks vertex attribute vectors equal expected sizes.
2. **Index Handling**
   - Validate Assimp indices fit into uint32; if not, split mesh or throw.
   - **Acceptance:** Test asset with large indices triggers error message referencing index limit.

## Phase 4 – Material & Texture Conversion
1. **Material Property Mapping**
   - Extract colors, metallic/roughness, emissive, opacity from `aiMaterial`. Map to `scene::MaterialProperties` with fallback defaults.
   - **Acceptance:** Unit test ensures numeric properties equal the values embedded in fixture material.
2. **Texture Path Handling**
   - Resolve texture file paths relative to imported asset. Copy or reference existing pipeline (no copying initially; just record path).
   - Support base color, normal, metallic/roughness, AO, emissive maps; log warnings for unhandled types.
   - **Acceptance:** Loader returns materials with correct `.textures.*Path` fields; missing textures produce warn log (unit test verifying log capture or stub).

## Phase 5 – Scene Assembly
1. **Node Traversal**
   - Traverse `aiScene` nodes, accumulate transforms into `SceneRenderer::SceneMesh` entries (store final model matrix per mesh).
   - **Acceptance:** Multi-mesh fixture yields expected number of `SceneMesh` with non-identity matrices verified in unit/integration test.
2. **Material Registry Sync**
   - Add importer function to push new `scene::Material` instances into registry, capturing created IDs for mesh assignments.
   - **Acceptance:** After import, material registry size increases by imported count; IDs recorded in `SceneMesh.material` match registry lookup.

## Phase 6 – Renderer Integration
1. **Application Hook**
   - Add CLI argument or hard-coded sample load in `Application::run` to invoke new importer and append meshes to existing scene list.
   - **Acceptance:** Running app shows imported model rendered with textures (manual verification + screenshot note).
2. **ImGui Tools (Optional)**
   - Add “Import Model” button (file dialog or path text input) to load additional assets at runtime.
   - **Acceptance:** Manual QA confirms button triggers import without crash.

## Phase 7 – Testing & QA
1. **Automated Tests**
   - Unit tests for mesh/material conversion (Phases 3 & 4 already targeted).
   - Integration test: load sample asset via importer and assert final mesh/material counts.
   - **Acceptance:** `ctest` includes new tests and passes on CI.
2. **Manual QA Checklist**
   - Document steps: load OBJ, load glTF with multiple meshes, load asset with missing textures, confirm warnings and runtime behaviour.
   - Capture at least one screenshot showing imported asset.
   - **Acceptance:** QA checklist updated; screenshot stored under docs or assets/previews.

## Phase 8 – Documentation
1. **User Guide**
   - Update README (or docs/importing.md) with instructions: enabling Assimp, supported formats, placing assets, known limitations.
   - **Acceptance:** Docs committed; includes table of supported features and limitations.
2. **Changelog Entry**
   - Add entry describing Assimp integration and new import workflow.
   - **Acceptance:** CHANGELOG updated under “Added”.

## Phase 9 – Clean-up & Follow-ups
- Identify deferred features (animations, skinning, cameras) and create TODO entries if needed.
- Plan asset caching or runtime selection improvements for future iteration.
- **Acceptance:** Section appended to plan noting future work items.
