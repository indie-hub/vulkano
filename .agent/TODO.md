# TODO

Short-term plan:

- Initialize CMake project (C++20), output to `bin/`.
- Add Vulkan + GLFW + GLM + ImGui dependencies (FetchContent where sensible).
- Implement RAII `App` that creates Vulkan instance/device/swapchain, render pass, pipeline, framebuffers, command buffers, sync.
- Embed SPIR-V shaders in `.cpp` to avoid external tooling.
- Render a white triangle on black; color via push constant.
- Add ImGui overlay (FPS, frame time, device name, swapchain extent).
- Handle resize by recreating swapchain dependents.
- Add a small unit test (FPS smoothing utility).
- Add `tools/` scripts and `README` build/run instructions.

Notes:
- macOS requires MoltenVK and portability enumeration; request extensions accordingly.
- Validation layers in Debug; disable in Release.
- Use angle-bracket includes with project paths.

