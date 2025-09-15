# Vulkano Codex – TODO

Status: Forward Phong + normal mapping running; depth enabled; ImGui controls wired – DONE
Update: SSAO controls added and AO hook in forward shader. G-buffer prepass (normals+depth to RGBA16F), SSAO fullscreen pass (AO to R8), and a simple isotropic blur pass are integrated. Robust swapchain‑recreation paths remain.
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
- [ ] Resize-safe swapchain recreation for multi-pass images

## Session Summary (current)
- Built Debug successfully on macOS; headless run OK (`HEADLESS_RUN_MS=500`).
- All unit tests pass (`ctest` Debug).
- Implemented AO blur pass (single-pass isotropic) and forward wiring; shaders compiled and included.
- Attempted push; skipped due to missing `GITHUB_TOKEN` in environment. Local commits are on `personal/codex/second_try`.
 - Current: Verified build + tests in this session; ready to push if token present.

## Immediate TODOs
- Add CI workflow to configure, build, run unit tests, and attempt headless app run on Linux and macOS (MoltenVK notes).
- Optionally change blur to separable Gaussian (2 passes) for performance.
- Harden swapchain/image recreation paths (destroy/recreate offscreen images, pipelines, and descriptor sets on resize).

## Session Plan (short)
- Add shaders: gbuffer.vert/frag, fullscreen.vert, ssao.frag
- Add offscreen images + render passes + pipelines (guarded by VULKAN) — DONE
- Record command buffers: G-buffer -> SSAO -> forward — DONE
- Keep non-Vulkan build working; run headless app to verify
- Commit often; push at end

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
