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
- [COMPLETED] Add more debug names/markers
- [COMPLETED] Expand unit tests for FPS smoothing
- [COMPLETED] Improve resize callback handling
  - Window framebuffer callback now flags app to recreate swapchain before next frame
- [COMPLETED] Make debug utils extension conditional on Debug

Status (final verification):
- Built Debug successfully; binaries in `bin/` including SPIR-V shaders in `bin/shaders`.
- Tests pass via CTest (`build_sanity`, `fps_smoothing`).
- App integrates GLFW + Vulkan + GLM + ImGui.
- Triangle renders with push-constant color (white) over black clear.
- ImGui overlay shows FPS, frame time, device name, and swapchain extent.
- Swapchain resize recreation works; validation layers clean in Debug.

Run notes:
- Ensure Vulkan SDK is installed and `glslc` is on PATH.
- Launch with `bin/vulkano_app`.

Notes (2025-09-16):
- Verified clean Debug build on local machine; binaries land in bin/.
- CTest passes: sanity + FPS smoothing.
- ImGui overlay renders FPS/frame time, device name, and swapchain extent.
- Validation layers enabled via ENABLE_VALIDATION on Debug; debug utils names and labels in place.
- Resize path tested via framebuffer resize flag; swapchain is recreated safely.
- ImGui font upload: wired CommandPool into ImGui init and use backend helper CreateFontsTexture() to ensure fonts are uploaded reliably.

Nice-to-have:
- Separate transfer/upload path for ImGui font creation with explicit command buffer
- Add CI workflow for Linux/macOS/Windows to build and run tests

Verification (automated run):
- Built Debug and ran tests successfully on this machine.
- Build time: 2025-09-16T13:25:00Z.
  - Re-verified build + CTest on this pass; binaries in bin/ and SPIR-V in bin/shaders.
  - CMake FetchContent fetched GLFW/GLM/ImGui; Vulkan SDK with glslc present.

Additional verification (this pass):
- Configured with CMake, built targets (Shaders, glfw, glm, imgui backends, app, tests).
- ctest ran and passed: build_sanity, fps_smoothing.
- Confirmed binaries in `bin/` and shaders in `bin/shaders`.

Next steps (if needed):
- Optional: add CI workflow to build and run tests across platforms
- Optional: add runtime toggle for v-sync/present mode via ImGui
