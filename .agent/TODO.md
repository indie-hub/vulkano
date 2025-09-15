# Vulkano Codex – TODO

Status: Forward Phong + normal mapping running; depth enabled; ImGui controls wired – DONE
Update: SSAO controls added and AO hook in forward shader. AO texture currently placeholder (white); next step is to implement prepass normals+depth and SSAO + blur to fill AO.

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
- [ ] Prepass: view-space normal + depth offscreen (sampleable)
- [ ] SSAO: kernel + 4x4 noise texture, write AO (R8)
- [ ] Blur: separable pass into AO_blur
- [ ] Integrate: forward shading samples AO_blur (already wired)
- [ ] Resize-safe swapchain recreation for multi-pass images

## Notes
- All includes must use angle brackets.
- No code in headers; only declarations.
- Prefer RAII and `const` correctness.
 - SSAO will be implemented as a separate pass with sampled depth+normal and a tiled noise texture. Forward Phong is a milestone to validate mesh/UBO/descriptor plumbing first.
