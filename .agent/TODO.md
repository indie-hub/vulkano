# Vulkano Codex – TODO

Status: Forward Phong + normal mapping running; depth enabled; ImGui controls wired – DONE
Update: SSAO controls added and AO hook in forward shader. G-buffer prepass (normals+depth to RGBA16F), SSAO fullscreen pass (AO to R8), and a simple isotropic blur pass are integrated. Robust swapchain‑recreation paths remain. Validation layer enablement is now conditional on availability; app gracefully falls back if Vulkan init fails.
Verification: Built Debug locally, tests pass, headless run OK.

## High-level Plan
- Scaffold CMake project and folders
- Add GLFW window loop (runs without Vulkan if unavailable)
- Prepare Vulkan instance/device scaffolding (feature flags)
- Add ImGui setup (next)
- Implement icosphere mesh + CPU subdivision
- Add Vulkan render passes: G-buffer, SSAO, blur, compose
- Hook ImGui controls (subdivisions, lighting, SSAO)
- Add tests (unit + minimal E2E)
- Add shaders + compile step
- Add textures/assets and runtime packaging

## Short-term Tasks
- [x] Configure CMake with options: ENABLE_VALIDATION, ENABLE_GLFW_WAYLAND, ENABLE_IMGUI_DOCKING
- [x] FetchContent: GLFW, GLM, Catch2 (tests); defer ImGui for next step
- [x] Create `app/` with executable entry point
- [x] Implement `vulkano::Window` wrapper around GLFW
- [x] Add runtime output directories to `app/bin/<config>`
- [x] Add `.clang-format` from AGENTS.md
- [x] Basic unit test scaffold builds

## Next Iteration
- [x] Fix the runtime_error: Failed to create Vulkan instance
- [x] Icosphere mesh generation with tangents/bitangents
- [x] Unit tests for icosphere counts and normals
- [x] Vulkan instance/device/swapchain, debug utils (basic clear)
- [x] ImGui integration (imgui_impl_glfw + imgui_impl_vulkan)
- [x] Mesh GPU upload + basic pipeline (draw icosphere) [conditional on shaders/.spv present]
- [x] Runtime subdivisions trigger CPU rebuild + GPU reupload
- [x] Camera + MVP matrices and per-pixel Phong shading
- [x] Tangent-space normal mapping with procedural textures
- [x] ImGui controls: light/material (normal strength, color, intensity, shininess)
- [x] Prepass: view-space normal + depth offscreen (sampleable)
- [x] SSAO: kernel + 4x4 noise texture, write AO (R8)
- [x] Blur: single-pass isotropic blur into AO_blur (reduces noise)
- [x] Integrate: forward shading samples AO_blur (already wired)
- [x] Resize-safe swapchain recreation for multi-pass images
- [x] Fix: terminating due to uncaught exception of type std::runtime_error: Failed to create Vulkan instance (conditional validation, graceful fallback)
- [ ] Split vulkan_context.cpp into smaller files (instance, device, swapchain, sync, utils)

## Session Summary (current)
- Built Debug successfully on macOS; headless run OK (`HEADLESS_RUN_MS=500`).
- All unit tests pass (`ctest` Debug).
- Implemented AO blur pass (single-pass isotropic) and forward wiring; shaders compiled and included.
- Verified fresh configure + build today; ran tests and a short headless app run.
- Attempted push; skipped due to missing `GITHUB_TOKEN` in environment. Local commits are on `feature/swapchain-recreation`.
 - Current: Verified build + tests in this session; ready to push if token present.

### 2025-09-16 Quick Verification
- Reconfigured and rebuilt `RelWithDebInfo` successfully.
- Tests passed locally via `ctest`.
- Ran headless app for 200ms to validate event loop.
- No code changes required this pass; preparing to push pending changes if token is available.

### 2025-09-15 Additional Notes
- Verified full local rebuild from clean `build/` directory.
- Validated non-Vulkan path still works in headless mode.
- Prepared to push upstream; using `GITHUB_TOKEN` if present and remote `origin` at `indie-hub/vulkano`.

## Immediate TODOs
- [x] Add CI workflow to configure, build, run unit tests, and attempt headless app run on Linux and macOS (MoltenVK notes). Note: GLFW headless on Linux requires xvfb-run (wired in CI).
- [ ] Optionally switch AO blur to separable Gaussian (2-pass) for better performance.
- [ ] Harden swapchain/image recreation paths (destroy/recreate offscreen images, pipelines, and descriptor sets on resize).
- [x] Add helper scripts in tools/ (build_debug.sh, format_all.sh)
- [ ] Split `vulkan_context.cpp` into smaller translation units (instance, device, swapchain, renderpasses, pipelines, resources) while preserving interfaces.

## Session Plan (short)
- Add shaders: gbuffer.vert/frag, fullscreen.vert, ssao.frag
- Add offscreen images + render passes + pipelines (guarded by VULKAN) — DONE
- Record command buffers: G-buffer -> SSAO -> forward — DONE
- Keep non-Vulkan build working; run headless app to verify
- Commit often; push at end

## 2025-09-16 CI Update
- Added `.github/workflows/ci.yml` running Linux (with `xvfb-run`) and macOS builds.
- CI compiles with FetchContent deps (GLFW/GLM/Catch2). Vulkan remains optional; tests run headless (no Vulkan path required).
- Next: consider caching deps and adding a matrix including `ENABLE_VALIDATION` toggles.

## Acceptance Criteria – Self-check (initial)
- App runs; window opens; headless path also runs.
- Phong + tangent-space normal mapping visible; normal strength adjustable.
- ImGui slider updates subdivisions and geometry re-uploads correctly.
- SSAO toggles and parameters affect output; blur reduces noise.
- Resizing triggers swapchain recreation without errors in normal usage.
- Validation layers clean in Debug on supported platforms.

### 2025-09-16 Notes
- Implemented swapchain recreation on `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` during acquire/present.
- Recreates: swapchain, image views, render pass, framebuffers, command pool/buffers, sync objects, offscreen (G-buffer/SSAO/blur) targets, and pipelines sized to the new extent.
- Guarded against zero-sized extents (minimized window) by skipping frame.

## Progress Log
- Implemented G-buffer (RGBA16F) and SSAO (R8) offscreen resources and pipelines.
- Wired descriptor sets: forward (albedo, normal, AO), SSAO (UBO + ND texture).
- Updated UBO to carry AO radius in screen.w; AO power uses screen.z.
- Recorded command buffers to render: G-buffer -> SSAO -> forward -> ImGui.
- Added GLSL shaders and compiled SPIR-V binaries to repo.
- Built and ran headless on macOS (no Vulkan): OK. Unit tests: OK.
- Push blocked: GITHUB_TOKEN not set in environment; local commits are in place.

## Notes
- All includes must use angle brackets.
- No code in headers; only declarations.
- Prefer RAII and `const` correctness.
 - SSAO will be implemented as a separate pass with sampled depth+normal and a tiled noise texture. Forward Phong is a milestone to validate mesh/UBO/descriptor plumbing first.
