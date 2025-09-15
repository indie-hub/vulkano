# Vulkano Codex – TODO

Status: Initial scaffolding (CMake + GLFW window) – DONE

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
- [ ] Vulkan instance/device/swapchain, debug utils
- [ ] ImGui integration (imgui_impl_glfw + imgui_impl_vulkan)
- [ ] Icosphere mesh generation with tangents/bitangents

## Notes
- All includes must use angle brackets.
- No code in headers; only declarations.
- Prefer RAII and `const` correctness.
