Done so far:
- CMake project scaffolded (GLFW, GLM, ImGui)
- Vulkan triangle renders with ImGui overlay hooks
- Shaders compile via glslc to bin/shaders
- Basic test hooked via CTest
- Fixed validation warnings by switching to per-frame fences and per-image render-finished semaphores; added images_in_flight tracking
- Added missing includes and minor tidy-ups
- Verified Debug build succeeds; binaries output to bin/
- CTest passes (sanity + FPS smoothing)

Next:
- Add more debug names/markers [DONE]
- Expand unit tests for FPS smoothing [DONE]
- Improve resize callback handling [DONE]
  - Window framebuffer callback now flags app to recreate swapchain before next frame

Nice-to-have:
- Separate transfer/upload path for ImGui font creation with explicit command buffer
- Add CI workflow for Linux/macOS/Windows to build and run tests
