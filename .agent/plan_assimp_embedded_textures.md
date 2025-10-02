# Embedded Texture Handling Plan

## Phase 0 — Reconnaissance
1. **Assimp Capabilities**
   - Verify how Assimp exposes embedded textures (`aiScene::mTextures`, `aiMaterial` texture slots with `src` IDs, `aiTexture::mHeight == 0` for compressed data).
   - Note supported formats (e.g., DDS, PNG) and required decode path.
   - **Acceptance:** Bullet list in this document summarizing how to detect and retrieve embedded textures.

2. **Current Texture Cache Review**
   - Evaluate `MaterialTextureCache::rebuild` to determine where embedded pixel data can be injected.
   - Identify fallback behaviour when no disk path exists.
   - **Acceptance:** Notes documenting integration points (e.g., new `ensure_embedded_texture` helper).

## Phase 1 — Design Decisions
1. **Storage Strategy**
   - Decide memory ownership: copy pixels into CPU `TextureData` and reuse existing upload pipeline.
   - Determine naming scheme / cache keys for embedded resources (e.g., `embedded:<scene_hash>:<index>`).
   - **Acceptance:** Design section appended to plan with chosen key format and memory handling.

2. **Decoder Requirements**
   - For compressed blobs (`mHeight == 0`) use `stbi_load_from_memory`; for raw RGBA data, copy directly.
   - Define error handling (log warning, fallback texture on failure).
   - **Acceptance:** Document decode path and failure fallback steps.

## Phase 2 — Implementation Steps
1. **Importer Enrichment**
   - Extend `ImportedMaterial` to include optional embedded texture payload references (e.g., `std::shared_ptr<TextureData>` or indices referencing a shared list).
   - Traverse `aiScene::mTextures` to collect embedded texture data once per import.
   - Map material texture slots (`aiMaterial::GetTexture`) that refer to embedded textures (paths like `*0`) to those payloads.
   - **Acceptance:** Unit tests verify importer detects embedded textures and attaches them to materials.

2. **Texture Cache Integration**
   - Update `MaterialTextureCache::rebuild` to accept embedded texture payloads along with file paths.
   - Add overload `ensure_embedded_texture(TextureData data)` to upload directly without requiring a filesystem path.
   - Ensure deduplication uses generated cache key (from design step).
   - **Acceptance:** Existing tests updated; new test ensures embedded texture increments descriptor count, not requiring disk file.

3. **Pipeline Wiring**
   - Modify `ImportedScene`/`AssetImporter` to carry extracted `TextureData` alongside materials.
   - On import, pass these payloads into texture cache when rebuilding.
   - **Acceptance:** Importing model with embedded textures results in runtime print/log confirming binder usage (optional manual verification).

## Phase 3 — Testing
1. **Fixture Asset**
   - Create or source a small asset with embedded texture (e.g., glTF with base64 image or FBX with embedded PNG).
   - Store sample under `tests/assets`. Update importer tests to load it and verify materials have texture usage flags set and descriptor count increments.
   - **Acceptance:** Catch2 test passes when verifying embedded asset uses texture (e.g., non-empty descriptor info index).

2. **Runtime Validation**
   - Manual QA: run app with `VULKANO_MODEL` pointing to embedded-texture asset, confirm textures appear (screenshot/description recorded).
   - Optionally capture validation layer output to ensure no sampler overflows.
   - **Acceptance:** QA notes appended to docs with observed render outcome.

## Phase 4 — Documentation
1. **Docs Update**
   - Update `docs/importing_models.md` with section on embedded textures, limitations, and supported formats, plus note about descriptor limits.
   - **Acceptance:** Doc committed referencing new capability.

2. **Changelog/TODO Update**
   - Add changelog entry noting embedded texture support.
   - Mark TODO items as completed once implementation/validation done.
   - **Acceptance:** TODO block updated; optional changelog entry added.

## Phase 5 — Follow-up Considerations
- Investigate caching strategy to avoid duplicate uploads when model re-imported.
- Consider streaming or larger allocator for many embedded textures.
- Document future tasks in plan.

