# TODO

Status: Scaffolding, Vulkan core, rendering, ImGui, resize, tests complete; CI pending

1. Initialize repo scaffolding
   - Create CMake project targeting C++20, output to `bin/`
   - Add `.clang-format`, `.clang-tidy`
   - Add basic directory structure: `include/`, `src/`, `tests/`, `cmake/`, `docs/`

2. Dependencies via CMake FetchContent
   - GLFW (with Vulkan surface)
   - GLM
   - Dear ImGui + backends (glfw/vulkan)
   - Catch2 (tests)

3. Core headers (no implementation in headers)
   - `<vulkano/window.hpp>`: GLFW window RAII
   - `<vulkano/vulkan_context.hpp>`: Instance, device, swapchain, render pass, pipeline, command buffers, sync
   - `<vulkano/imgui_layer.hpp>`: ImGui integration (init/frame/shutdown)
   - `<vulkano/app.hpp>`: App orchestrator
   - `<vulkano/utils/fps.hpp>`: FPS/frame time smoothing

4. Implement core modules (small pieces)
   - `Window` wrapper (GLFW init/terminate, resize flag)
   - Vulkan instance + debug utils (validation on Debug)
   - Surface, physical device pick, queue families, logical device
   - Swapchain + image views
   - Render pass (clear to black), framebuffers
   - Command pool + buffers
   - Vertex buffer (3 vertices in NDC via GLM)
   - Graphics pipeline (vertex + fragment shaders, push constant color)
   - Sync objects (fences/semaphores), draw loop with acquire/draw/present
   - Resize handling: recreate swapchain dependents

5. ImGui overlay
   - Initialize ImGui with GLFW + Vulkan backend
   - Draw FPS, frame time (ms), device name, swapchain extent
   - Render after scene, before present

6. Shaders
   - Embed precompiled SPIR-V for simple passthrough vertex and push-constant color fragment

7. Tests
   - Unit tests for FPS smoothing util
   - Build tests with Catch2

8. Build & run
   - Ensure `bin/` contains executable and run-time assets [DONE]
   - Verify compiles and runs locally [DONE]

9. Docs/CI
   - Add `docs/README.md` build/run instructions [DONE]
   - Add minimal CI workflow (configure + build + tests + format + tidy) [PENDING]

10. Finalize
   - Review against AGENTS.md checklist
   - Commit frequently; final push
