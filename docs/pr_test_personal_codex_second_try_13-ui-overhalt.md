# PR Test Checklist — `personal/codex-second-try/13-ui-overhalt` → `develop`

This checklist documents the exact verification reviewers must perform before approving or merging the branch into `develop`. It is intended to be pasted (or linked) in the PR's **Test Plan** section.

## 1. Environment Preparation
1. macOS 14.5+ (Apple Silicon) with Xcode command-line tools.
2. Vulkan SDK ≥ 1.3.290 with MoltenVK installed and `VULKAN_SDK` exported.
3. Python 3.11+ available on `PATH` for asset tooling scripts.
4. Clean workspace: `git fetch --all --prune` and ensure `personal/codex-second-try/13-ui-overhalt` is checked out.

## 2. Repository Integrity Checks
1. `git log -3 --format='%h %an <%ae> %cn <%ce>'`
   - ✅ Expect every entry to show `bruno o <bond.bales_0i@icloud.com>` for both author and committer.
2. `git fsck --strict`
   - ✅ Should report only dangling objects (expected after rewrite) and zero missing objects.

## 3. Configure & Build
1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
   - ✅ Configures without fatal errors; third-party warnings may appear but must be unchanged from baseline.
2. `cmake --build build -j$(sysctl -n hw.ncpu)`
   - ✅ Targets built: `vulkano_app`, `vulkano_renderer`, `vulkano_tests`, shader targets, tooling binaries.

## 4. Automated Test Suite
1. `ctest --test-dir build --output-on-failure`
   - ✅ `vulkano_tests` passes in <1s; no unexpected warnings in stdout/stderr.
2. Optional Debug validation: reconfigure with `-DCMAKE_BUILD_TYPE=Debug`, rebuild, and rerun `ctest` if time permits.

## 5. Runtime Smoke & Visual QA
1. Launch renderer: `./build/bin/vulkano_app`.
2. Observe ImGui overlay renders with FPS counter and SSAO controls.
3. Toggle SSAO Enable/Blur toggles; confirm on-screen lighting updates without hitching or validation errors.
4. Open UV debug view (ImGui → Diagnostics → UV Debug) and confirm lounge chair leather seams show no wood bleed.
5. Capture screenshots (⌘+Shift+4) of:
   - Default shaded view.
   - SSAO debug view.
   - UV debug view for leather panels.
   Store captures with timestamp in `docs/baseline/verification/` for future regressions.

## 6. Regression Spot-Checks
1. Run `tools/dump_mesh_uvs --asset assets/chair/lounge_chair.glb --output /tmp/chair_uv.json` and ensure command exits 0.
2. Compare `/tmp/chair_uv.json` vs the previous baseline using `tools/compare_uvs.py` (if available) to confirm no geometry drift.
3. Review application logs (stdout) for Vulkan validation layer warnings; there should be none.

## 7. Documentation & Communication
1. Reference this checklist in the PR description under **Test Plan**.
2. Mention that git history was rewritten; remind reviewers to `git fetch --prune` + `git reset --hard origin/develop` after merge.
3. Attach captured screenshots or upload them to the PR for visual confirmation.

## Pass/Fail Criteria
- ✅ All commands complete successfully.
- ✅ No visual regressions (compare against `docs/baseline/...`).
- ✅ No new validation layer warnings or crashes.
- ❌ Any deviation halts the merge until resolved.

## Notes
- Keep the backup mirror `../vulkano_codex_backup.git` until the rewritten history is fully deployed.
- Update this file if additional QA scenarios emerge during review.
