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
