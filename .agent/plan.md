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

# VulkanContext Refactor Plan

## Objectives
- Decompose `VulkanContext` into smaller, single-responsibility services adhering to SOLID and Clean Code principles.
- Improve testability by isolating decisions (instance creation, device selection, swapchain management) behind focused classes with deterministic lifecycles.
- Preserve existing external behaviour (public API consumed by application/renderer) while clarifying ownership and resource teardown.

## Proposed Components
1. **InstanceFactory** (`include/vulkano/vk/instance.hpp` + `src/vk/instance.cpp`)
   - Build Vulkan instance, validation layers, and extension list.
   - Own debug messenger setup/teardown via RAII.
2. **SurfaceFactory** (`include/vulkano/vk/surface.hpp`)
   - Encapsulate GLFW surface creation using provided `Window` handle.
3. **PhysicalDeviceSelector** (`include/vulkano/vk/physical_device.hpp`)
   - Evaluate available GPUs, queue families, and device extensions.
   - Produce a `PhysicalDeviceSelection` value object (device handle + queue family indices).
4. **LogicalDeviceManager** (`include/vulkano/vk/logical_device.hpp`)
   - Create logical device, retrieve graphics/present queues, and expose getters.
5. **SwapchainManager** (`include/vulkano/vk/swapchain.hpp`)
   - Handle swapchain creation, image views, recreation parameters, and image format/extent accessors.
6. **VulkanContextFacade** (`include/vulkano/app/vulkan_context.hpp`)
   - Compose the above managers, providing high-level methods currently used by `TriangleRenderer`, `FrameResources`, and others.
   - Coordinate orderly destruction in reverse dependency order.

## Migration Steps
1. **Analysis & Interface Definition**
   - Document current usage of `VulkanContext` methods throughout codebase.
   - Define target public API for facade and supporting components.
2. **Extract Instance/Debug Messenger**
   - Move instance creation + debug messenger into `InstanceFactory` with RAII destructor.
   - Update facade to own `InstanceFactory` member.
3. **Introduce Physical Device Selection Module**
   - Relocate queue discovery, extension checks, suitability evaluation.
   - Add dedicated unit tests covering selection heuristics (mocking surface capabilities via injected structs).
4. **Isolate Logical Device & Queues**
   - Create `LogicalDeviceManager` controlling device creation, queue retrieval, and wait-idle logic.
   - Ensure `FrameResources` interacts via facade accessor only.
5. **Extract Swapchain Management**
   - Move swapchain creation, image view setup, and format/extent retrieval inside `SwapchainManager`.
   - Provide methods for recreation hooks (future resizing work).
6. **Wire Facade & Call Sites**
   - Update application and renderer code to depend on facade getters or dedicated accessors (e.g., `context.swapchain().image_views()` -> `swapchain_manager.image_views()`).
7. **Testing & Validation**
   - Extend unit tests for new modules (e.g., verifying extension lists, queue family classification).
   - Run full build, unit tests, and validation layers to confirm no regressions.

## Acceptance Criteria
- `VulkanContext` header exposes a slim facade that delegates to smaller components, each conforming to single responsibilities.
- Each new component provides deterministic RAII destruction and has unit coverage for critical decision logic.
- Application, renderer, and frame resources compile against the refactored API without behavioural regressions (triangle + ImGui render, validation passes).
- Documentation in `/docs` (new section or README update) summarises module responsibilities and interactions.

# Multi-Primitive Renderer Plan

## Goals
- Extend the renderer to draw a plane (ground), cube, and sphere with the sphere and cube resting on the plane.
- Maintain SOLID structure by introducing reusable geometry/mesh abstractions and ensuring transformations remain right-handed.
- Preserve existing ImGui overlay and validation-clean execution.

## Proposed Architecture
1. **Geometry Module**
   - Add `Mesh`/`MeshBuilder` helpers to encapsulate vertex/index buffers and layout descriptions.
   - Provide procedural generation utilities for plane, cube, and UV sphere vertices using glm for positioning.
2. **Renderer Enhancements**
   - Expand `TriangleRenderer` into a more general `SceneRenderer` capable of handling multiple meshes and model matrices.
   - Introduce uniform/descriptor updates (e.g., per-object push constants or uniform buffers) for individual transforms and colours.
3. **Scene Composition**
   - Define a `Scene` description (plane + cube + sphere) with positioning: cube and sphere above plane with distinct heights.
   - Compute appropriate model matrices and optional materials (colours) for each object.
4. **Lighting Placeholder (optional)**
   - (Optional enhancement) prepare structure for future lighting by keeping pipeline compatible with normal data, even if unlit for now.

## Implementation Steps
1. **Mesh Abstractions**
   - Create `Mesh` class (vertex/index buffers, counts) and `GeometryFactory` generating plane/cube/sphere data.
   - Unit-test procedural generators (dimensions, vertex counts, right-handed orientation).
2. **Renderer Refactor**
   - Rename/replace `TriangleRenderer` with `SceneRenderer` supporting multiple meshes and draw calls.
   - Update shaders to accept vertex colours/normals as needed and push constant per-object transforms.
3. **Scene Setup**
   - Build a `Scene` object inside `Application::run` that creates meshes and defines transforms for plane (Y=0), cube (elevated), sphere (elevated).
   - Integrate with render loop to draw all objects each frame.
4. **Testing & Validation**
   - Extend existing math/tests to cover new mesh factories.
   - Run full build, unit tests, and validation.
   - Manual run to confirm geometry arrangement.

## Acceptance Criteria
- Renderer displays plane, cube, and sphere simultaneously with cube/sphere positioned on top of plane.
- Mesh abstraction and scene renderer follow SOLID (single responsibility, composable components).
- Existing ImGui overlay and validation remain clean.
- New procedural geometry has unit test coverage verifying vertex counts/orientation.

# Resize Handling Plan

## Objectives
- Detect window resizes and rebuild swapchain-dependent resources (framebuffers, scene renderer, per-frame buffers) without scaling artifacts.
- Ensure ImGui and scene maintain proper aspect ratio and projection updates after recreation.
- Preserve validation cleanliness and SOLID structure.

## Required Changes
1. **Window Events & Sync**
   - Use GLFW framebuffer resize callback to flag swapchain recreation.
   - Handle `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` during acquire/present to trigger recreation.

2. **Swapchain Recreate Pipeline**
   - Add `VulkanContext::recreate_swapchain` (or dedicated swapchain manager method) rebuilding swapchain, image views, depth buffers (if added), and updating renderer-dependent resources.
   - Destroy and rebuild scene renderer framebuffers and recompute projection matrix based on new extent.
   - Update ImGui renderer to refresh render pass attachments if necessary.

3. **Resource Ownership**
   - Encapsulate recreation logic in high-level `Renderer` facade or `Application` helper for clarity.
   - Ensure FrameResources command buffers & semaphores remain valid or are recreated if counts change.

4. **Testing & Validation**
   - Manual resize tests to confirm dynamic adjustment.
   - Extend unit tests (if feasible) to validate aspect ratio computation/extraction.
   - Run full build, `ctest`, and validation run to confirm no regression.

## Steps
1. Introduce resize callback storing `framebufferResized` flag in window wrapper.
2. Implement swapchain recreation routine that waits for idle, rebuilds swapchain + framebuffers, updates projection in scene renderer, and notifies ImGui (if needed).
3. Update main loop to check flag and handle recreation after present/acquire out-of-date errors.
4. Retest manual resizing, run automated tests, ensure validation stays clean.

## Acceptance Criteria
- Resizing the window rebuilds swapchain and scene without stretching.
- Scene projection and aspect ratio update correctly; cube/sphere maintain positions.
- Validation layers remain silent during resize sequences.

# FPS Camera Plan

## Objectives
- Introduce a first-person camera system with WASD + mouse look controls while preserving right-handed coordinates.
- Update view/projection matrices dynamically in the scene renderer.
- Keep code SOLID: separate input handling, camera state, and renderer responsibilities.

## Proposed Architecture
1. **Camera Component**
   - Add `Camera` class storing position, orientation (yaw/pitch), and matrices.
   - Provide methods for forward/right/up vectors, movement, and look adjustments with clamped pitch.
2. **Input Controller**
   - Extend window/input system with cursor capture/toggle and per-frame delta retrieval.
   - Use GLFW callbacks for mouse movement; store state in new `CameraController` (update on `poll_events`).
3. **Renderer Integration**
   - SceneRenderer uses camera-provided view/projection each frame (no stored view/projection inside renderer).
   - Push constants updated per draw call with camera matrices.
4. **Application Glue**
   - Maintain camera state in `Application`; update based on delta time, keyboard, and mouse input.
   - Toggle mouse capture with e.g. Escape to free cursor.
5. **Testing**
   - Unit tests for camera vector math (forward/right/up and matrix generation).
   - Manual validation: move around the scene; ensure right-handed controls (W forward, S backward, A left, D right).

## Steps
1. Implement `Camera` (position, yaw, pitch, `view_matrix`, `projection`, `move` including Q/E vertical motion, and `rotate` with pitch clamp).
2. Add input handling: GLFW cursor position callback (store last cursor positions and deltas), key state queries, and RMB-based cursor capture so movement/rotation only apply while RMB held (release restores cursor).
3. Create `CameraController` to process input each frame and adjust camera accordingly.
4. Update SceneRenderer to accept camera view/projection each frame (remove internal stored matrices).
5. Modify Application loop to update camera before rendering: when RMB pressed, capture cursor, process WASD/QE movement and mouse deltas each frame; when released, skip camera updates. Pass camera view/projection to renderer push constants.
6. Add unit tests for camera math (Catch2) and integration checks (e.g., small camera move results expected view).
7. Build, run tests, manual validation of FPS movement/mouse look.

## Acceptance Criteria
- W/A/S/D move camera (right-handed: W forward along -Z, A left, etc.), Q/E move vertically; mouse look adjusts yaw/pitch while RMB held with clamped pitch range.
- Renderer uses updated camera view/projection; objects maintain correct orientation and lighting.
- Cursor hidden/captured during camera control and can be released (Escape).
- Unit tests for camera math pass and overall test suite remains green.
- Application remains validation-clean after movement and resizing.
