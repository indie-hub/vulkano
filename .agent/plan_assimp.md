# Assimp Integration Plan

## 1. Requirements & Inventory
- Identify target asset formats (OBJ, FBX, glTF) and required data (meshes, normals, tangents, materials, animations?).
- Check existing mesh/material abstractions to understand mapping.
- **Acceptance:** Documented list of asset features to support and existing gaps.

## 2. Build & Dependencies
- Decide between vendored Assimp, FetchContent, or system package.
- Update CMake to fetch/build Assimp with minimal configuration (disable tools/tests to keep build fast).
- Ensure compiler definitions and include paths align with project settings.
- **Acceptance:** "cmake --build" succeeds on target platforms with Assimp linked into app library.

## 3. Import Pipeline Design
- Define asset loader interface (e.g., `AssetImporter` class) that converts Assimp scene data into internal mesh/material structs.
- Plan support for multiple meshes per file, scene hierarchy (for transforms), and material extraction including textures.
- Outline default fallbacks when data is missing (normals, tangents, UVs).
- **Acceptance:** Design doc (this plan) details data flow diagrams or bullet steps for conversion.

## 4. Implementation Phases
### 4.1 Loader Scaffolding
- Wrap Assimp initialization, scene loading, and error handling.
- Convert `aiMesh` to internal `scene::MeshData` (positions, normals, UVs, tangents, indices).
- Handle vertex deduplication / U32 index conversions as needed.
- **Acceptance:** New loader function returns mesh data for a basic OBJ with unit tests verifying counts.

### 4.2 Material & Texture Mapping
- Parse `aiMaterial` to fill `scene::Material` (base color, metallic/roughness, normal, AO, emissive, textures).
- Copy textures to asset directory or return file references for existing texture cache.
- **Acceptance:** Loader populates material registry entries; tests assert properties/texture paths match sample file metadata.

### 4.3 Scene Composition
- Support multi-mesh files: maintain transforms per node; create `SceneMesh` entries with combined matrices.
- Optionally expose animation channels (flag for future work) but ensure graceful ignore for now.
- **Acceptance:** Complex model (multiple meshes) loads into scene with expected mesh/material counts.

### 4.4 Integration with Renderer
- Add command/UI to import asset at runtime (dialog or config path) and update `SceneRenderer` resources.
- Provide CLI or config path for offline loading during app initialization.
- **Acceptance:** Running app loads sample asset via new path; render output matches expectations (manual QA screenshot).

## 5. Testing Strategy
- Unit tests for mesh/material conversion using small fixture assets.
- Integration test to load asset headlessly and validate vertex/material counts.
- Automation to ensure missing features emit warnings, not crashes.
- **Acceptance:** Tests pass; coverage includes success and failure cases (missing textures, unsupported formats).

## 6. Documentation & QA
- Update README/docs on how to add assets, supported formats, known limitations.
- Extend QA checklist (import simple mesh, multi-mesh, textures) and capture screenshots.
- **Acceptance:** Docs merged; QA notes appended with Assimp scenarios.
