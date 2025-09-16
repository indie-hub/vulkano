# TODO

Short-term plan:

- [done] Initialize CMake project (C++20), output to `bin/`.
- [done] Add Vulkan + GLFW + GLM + ImGui dependencies (FetchContent).
- [done] Implement RAII `App` that creates Vulkan instance/device/swapchain, render pass, pipeline, framebuffers, command buffers, sync.
- [done] Compile GLSL to SPIR-V at build-time; load from `bin/shaders`.
- [done] Render a white triangle on black; color via push constant.
- [done] Add ImGui overlay (FPS, frame time, device name, swapchain extent).
- [done] Handle resize by recreating swapchain dependents.
- [done] Add a small unit test (FPS smoothing utility).
- [done] README with build/run instructions.
- [todo] Split app.cpp into smaller translation units.
- [todo] Add clang-tidy config and CI workflow.

Notes:
- macOS requires MoltenVK and portability enumeration; request extensions accordingly.
- Validation layers in Debug; disable in Release.
- Use angle-bracket includes with project paths.
