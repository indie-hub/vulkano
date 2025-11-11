## PR Verification Plan — personal/codex-second-try/13-ui-overhalt → develop

### Goal
Provide a deterministic, repeatable checklist covering automated and manual validation so reviewers can confidently merge `personal/codex-second-try/13-ui-overhalt` into `develop` after the git identity rewrite and recent renderer updates.

### Constraints
- Follow AGENTS.md: C++20 toolchain, angle-bracket includes, brace initialisation, SOLID.
- Tests must execute on macOS 14 + Apple Silicon with Vulkan SDK 1.3.290+, MoltenVK, and configured GPU entitlement.
- Use Release builds for performance-sensitive validation, Debug builds for assertions.
- Document every command with exact arguments so CI mirrors the same steps.

### Test Matrix
1. **Repository Integrity**
   - `git log -3 --format='%h %an <%ae> %cn <%ce>'` → confirm rewritten commits show the corrected identity.
   - `git fsck --strict` → ensure no corruption after history rewrite.
2. **Configuration & Build**
   - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
   - `cmake --build build -j$(sysctl -n hw.ncpu)`
3. **Automated Tests**
   - `ctest --test-dir build --output-on-failure` (Catch2 suite).
4. **Runtime Smoke**
   - Launch `./build/bin/vulkano_app`.
   - Verify ImGui overlay renders (FPS counter, SSAO controls) and responds to input.
   - Toggle SSAO/blur controls and ensure real-time updates without validation errors in console.
5. **Visual QA**
   - Inspect lounge chair leather seams for bleed/regressions via UV debug view.
   - Capture screenshots for comparison with `docs/baseline/` assets.
6. **Tooling & Docs**
   - Ensure `.agent/plan_pr_merge_develop.md` and `docs/pr_test_personal_codex_second_try_13-ui-overhalt.md` stay in sync.
   - Update CHANGELOG under "Added" with the new PR verification resources.

### Exit Criteria
- All commands succeed without warnings beyond known third-party noise (GLFW/Assimp conversions).
- Visual inspection shows no regressions versus baseline captures.
- Documentation updated and referenced in PR description (Test Plan section can link to the new doc).
