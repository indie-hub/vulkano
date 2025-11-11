# Vulkan Renderer Execution Plan

## Goal
Deliver a right-handed Vulkan renderer that opens a GLFW window, renders a white triangle on a black background, and exposes an ImGui overlay. All math utilities must use glm. Runtime must be verified on macOS using Vulkan via MoltenVK.

## Phases
1. **Platform bootstrap**
   - Integrate GLFW for window creation and input handling.
   - Load Vulkan instance, select validation layers, and set up debug messenger.
   - Create surface from GLFW window and choose physical/logical devices supporting graphics and presentation.
   - Configure swap chain, image views, render pass, framebuffers, command pools, and synchronization primitives.

2. **Rendering pipeline**
   - Define right-handed coordinate transforms using glm (avoid legacy left-handed conversions).
   - Build graphics pipeline (shader modules, fixed-function state, pipeline layout) for a single triangle.
   - Upload static vertex data for a white triangle using vertex buffers and stage with `VK_BUFFER_USAGE_TRANSFER_DST_BIT`.
   - Record command buffers to clear to black and draw the triangle each frame.

3. **ImGui overlay integration**
   - Bring in ImGui with Vulkan backend and GLFW bindings.
   - Establish per-frame descriptor sets and font texture uploads via staging.
   - Render ImGui draw data after triangle pass within same frame submission, ensuring render pass subpass compatibility.

4. **Application structure & lifecycle**
   - Encapsulate subsystems into SOLID-compliant C++20 classes separated into headers (`include/vulkano/...`) and implementation (`src/...`).
   - Provide RAII wrappers for Vulkan objects and ensure deterministic destruction order.
   - Wire main loop to process events, rebuild swap chain on resize, and support clean shutdown.

5. **Testing and packaging**
   - Add unit tests (e.g., math helpers, configuration parsing) and integration smoke tests using headless validation (if feasible).
   - Configure CTest targets and basic CI script stub.
   - Produce packaged binaries under `/bin` with necessary shaders, config, and ImGui resources.

## Acceptance Criteria
- Builds with CMake on macOS/Linux using C++20, links against Vulkan SDK (via MoltenVK on macOS), GLFW, and ImGui; glm used for all math structures (no custom math types).
- Executable runs on macOS, opening the window and rendering correctly using the platform Vulkan stack (MoltenVK) without driver-specific workarounds.
- Renderer uses right-handed coordinate system (verified via glm matrices and camera setup).
- On launch, a window opens showing a white triangle centered on black background, sustaining at least 60 FPS on reference hardware.
- ImGui overlay toggles successfully and can display a simple diagnostics panel (FPS, frame timings).
- Validation layers report no errors or warnings during normal execution.
- Swap chain recreate logic handles window resize without crashes or resource leaks (checked via sanitizers or manual stress).
- Unit and integration tests pass via `ctest`; build artifacts and necessary runtime assets collected in `/bin`.
