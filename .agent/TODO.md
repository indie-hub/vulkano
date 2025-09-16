# TODO

- [x] Scaffold project: CMake, folders, .clang-format, minimal GLFW window app building to `bin/`.
- [x] Add Vulkan instance + surface creation with validation layers (Debug) and debug utils.
- [x] Pick physical device, queue families; create logical device and queues.
- [x] Create swapchain + image views; choose formats/present mode without magic numbers.
- [x] Create render pass, framebuffers, command pool/buffers, and sync primitives.
- [ ] Add shader pipeline (GLSL in `shaders/`, CMake glslc compile); vertex buffer for triangle.
- [ ] Draw triangle and present; clear to black; use push constant for colour.
- [ ] Integrate Dear ImGui (glfw + vulkan backends) with descriptor pool; draw overlay.
- [ ] Show FPS, frame time (ms), device name, swapchain extent in ImGui.
- [ ] Handle window resize: recreate swapchain and dependent resources; reinit ImGui if needed.
- [ ] Add unit tests (e.g., FPS smoothing) and lightweight e2e startup/shutdown.
- [ ] Wire CI-friendly build steps; finalize docs with build/run instructions.
