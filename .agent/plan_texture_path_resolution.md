# Plan: Resolve External Texture Paths Relative to Imported Scene

## Goal
Ensure textures referenced by imported assets (e.g., `.gltf`, `.obj`) are resolved using the source file’s directory so they load correctly even when using relative paths.

## Step-by-Step Tasks

1. **Reproduce & Confirm Current Behaviour**
   - Import the `mid_century_lounge_chair_4k-2` sample via `VULKANO_MODEL` and observe fallback textures in the viewport.
   - Verify the UI still lists relative texture paths (e.g., `textures/albedo.png`) and log warnings from `MaterialTextureCache` showing fallback usage.
   - *Acceptance:* Short QA log in this plan noting the exact failing path(s).

2. **Audit Texture Path Flow**
   - Trace how Assimp provides texture URIs (`AssetImporter::import_material`) and where they’re stored in `scene::Material`.
   - Document how `MaterialTextureCache::rebuild` canonicalises and loads textures, highlighting the missing base-directory join.
   - *Acceptance:* Written bullet list summarising path hand-off points and the failing canonicalisation step.

3. **Capture Source Directory During Import**
   - Extend `AssetImporter::load_scene` to compute and store the directory of the source asset (e.g., `ImportedScene.sourceDirectory`).
   - Propagate this directory to material import routines so each texture path can be resolved relative to it.
   - *Acceptance:* `ImportedScene` contains the canonical source directory and unit tests (if any) compiled.

4. **Normalise Texture Paths**
   - Update `import_material` (or an ancillary helper) to join relative texture URIs with the recorded source directory.
   - Preserve embedded (`*0`, etc.) and already-absolute paths without modification.
   - *Acceptance:* Added helper is unit-tested or covered by assertions verifying correct output for relative, absolute, and embedded paths.

5. **Update Material Registry / Cache**
   - Ensure `MaterialTextureCache::rebuild` sees fully-resolved filesystem paths for external textures, so `load_texture_from_file` succeeds.
   - Add logging or debug asserts to detect missing files after normalisation.
   - *Acceptance:* Running with the sample model loads actual texture data (visible in viewport) without falling back to solid colours.

6. **Regression Tests**
   - Add unit tests to cover path resolution (e.g., feed `import_material` mocked data with relative URIs and verify resolved path).
   - Optionally add an integration test that simulates loading an asset from a temporary directory.
   - *Acceptance:* `ctest` passes with new coverage; tests fail prior to fix.

7. **Documentation Update**
   - Mention in `docs/importing_models.md` (or relevant doc) that textures referenced relative to the model file are supported.
   - Include troubleshooting note for missing textures and how to structure directories.
   - *Acceptance:* Updated doc committed with clear instructions.

8. **Validation & QA**
   - Re-run the lounge chair sample and at least one other asset, toggling visibility to ensure textures display across UI interactions.
   - Run validation layers (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VULKANO_MAX_FRAMES=1 ./bin/vulkano_renderer`) to confirm no new warnings.
   - *Acceptance:* QA log/screenshot demonstrating textured model plus clean validation output.

## Notes
- Keep embedded texture remapping untouched—ensure the logic branches early for keys beginning with `embedded://`.
- Consider caching resolved paths to avoid recomputation if multiple materials use the same image.
- Maintain compatibility with absolute paths provided by users.
