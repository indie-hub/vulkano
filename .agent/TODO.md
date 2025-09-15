# Vulkano Codex – TODO

Status: Initial scaffolding

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
- [ ] Configure CMake with options: ENABLE_VALIDATION, ENABLE_GLFW_WAYLAND, ENABLE_IMGUI_DOCKING
- [ ] FetchContent: GLFW, GLM, Catch2 (tests); defer ImGui for next step
- [ ] Create `app/` with executable entry point
- [ ] Implement `vulkano::Window` wrapper around GLFW
- [ ] Add runtime output directories to `app/bin/<config>`
- [ ] Add `.clang-format` from AGENTS.md
- [ ] Basic unit test scaffold builds

## Next Iteration
- [ ] Vulkan instance/device/swapchain, debug utils
- [ ] ImGui integration (imgui_impl_glfw + imgui_impl_vulkan)
- [ ] Icosphere mesh generation with tangents/bitangents

## Notes
- All includes must use angle brackets.
- No code in headers; only declarations.
- Prefer RAII and `const` correctness.
