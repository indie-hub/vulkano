Done so far:
- CMake project scaffolded (GLFW, GLM, ImGui)
- Vulkan triangle renders with ImGui overlay hooks
- Shaders compile via glslc to bin/shaders
- Basic test hooked via CTest
- Fixed validation warnings by switching to per-frame fences and per-image render-finished semaphores; added images_in_flight tracking
- Added missing includes and minor tidy-ups

Next:
- Add more debug names/markers
- Expand unit tests for FPS smoothing
- Improve resize callback handling if needed
