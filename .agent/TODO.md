Plan (high level)

- SSAO integration: add offscreen AO targets, descriptor sets, pipelines.
- Blur pass: separable horizontal+vertical using fullscreen tri.
- Wire ImGui SSAO params to UBO; update each frame.
- Compose: sample scene * AO; gamma correct.
- Validate swapchain recreation; rebuild targets/pipelines on resize.
- Unit tests: keep icosphere shape/TBN checks.

Status

- Done: G-buffer pass (scene color, normal, viewZ, depth).
- Done: SSAO pass integrated; 4x4 noise, 64-sample kernel UBO; adjustable params.
- Done: Blur pass integrated; horizontal + vertical via push constants.
- Done: Compose pass sampling scene and final AO.
- Done: ImGui UI hooked into params; per-frame UBO updates.
- Done: Robust shader lookup and glslc compile step.
- Done: Build succeeds on macOS toolchain; tests pass (Catch2).
- Done: App launches; packaging script produces runnable folder under app/Release.
 - Done: Verified full configure+build+ctest locally; packaged Release binary and SPIR-V shaders into app/Release.
- Todo/Nice: SSAO compute variant, kernel randomization per frame, AO debug visualize.

Next Steps

- Add wireframe overlay toggle and optional pipeline state.
- Add optional HDR tonemapping in compose.
- Done: additional unit tests (camera projection/view invariants).
- Verify runtime on Windows/Linux; document MoltenVK specifics tested on macOS.

Session 2025-09-15

- Verified clean configure + build (Release) locally with shader compile.
- Ran unit tests via ctest: green.
- Packaged runnable folder under app/Release with binaries + SPIR-V.
- Performed quick smoke run (launched and terminated after ~1s).
- Reviewed headers for no inline impl and angle-bracket includes: compliant.
- No source changes needed; repo already meets acceptance criteria.

Fix Log

- 2025-09-15: Fixed VK_NULL_HANDLE command pool usage causing vkAllocateCommandBuffers validation error by creating the command pool before AO noise/texture uploads in Renderer constructor.

- Session 2025-09-15 16:19:27: Built Release, tests green, packaged app; no code changes needed.
