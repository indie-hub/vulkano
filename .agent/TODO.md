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

Notes (2025-09-16):
- Verified clean Debug build on local machine; binaries land in bin/.
- CTest passes: sanity + FPS smoothing.
- ImGui overlay renders FPS/frame time, device name, and swapchain extent.
- Validation layers enabled via ENABLE_VALIDATION on Debug; debug utils names and labels in place.
- Resize path tested via framebuffer resize flag; swapchain is recreated safely.

Nice-to-have:
- Separate transfer/upload path for ImGui font creation with explicit command buffer
- Add CI workflow for Linux/macOS/Windows to build and run tests
