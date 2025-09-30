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

# Depth Testing Plan

## Findings
- Current render pass exposes only a colour attachment; no depth attachment is defined (see SceneRenderer::create_render_pass).
- Graphics pipeline leaves `pDepthStencilState` unset, so depth testing and writes are fully disabled.
- Framebuffers are built with swapchain image views only, and command buffers clear colour without touching depth.

## Goals
- Introduce a depth buffer that matches the swapchain extent and makes front-to-back occlusion deterministic.
- Keep responsibilities split across focused components (context, renderer, frame resources) and respect RAII for all Vulkan objects.
- Preserve clean validation output and swapchain recreation resilience on macOS MoltenVK.

## Implementation Steps
1. **Depth format selection helper**
   - Add a vk-level utility (e.g., `vk::DepthFormatResolver`) that queries supported depth formats on the physical device and returns the optimal `VkFormat` (prefer `VK_FORMAT_D32_SFLOAT`, fall back through `VK_FORMAT_D32_SFLOAT_S8_UINT`, `VK_FORMAT_D24_UNORM_S8_UINT`).
   - Cover helper logic with unit tests using stubbed `vkGetPhysicalDeviceFormatProperties` responses.
2. **Depth image abstraction**
   - Introduce a small RAII type (e.g., `vk::DepthImage`) that owns the depth `VkImage`, `VkDeviceMemory`, and `VkImageView`, constructed from the selected format and swapchain extent.
   - Ensure it exposes the image view and handles destruction in its destructor; support move semantics for swapchain recreation.
3. **Render pass update**
   - Extend `SceneRenderer::create_render_pass` to declare a depth attachment with load-op clear, store-op `DONT_CARE`, final layout `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL`, and reference it in the subpass.
   - Update subpass dependency to include depth stages (`VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT` and `VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT`).
4. **Pipeline depth-stencil state**
   - Build a `VkPipelineDepthStencilStateCreateInfo` with depth test/write enabled, compare op `VK_COMPARE_OP_LESS_OR_EQUAL`, and set `pipelineInfo.pDepthStencilState` to it.
   - Keep stencil disabled but initialise the struct explicitly to satisfy validation layers.
5. **Framebuffer and render pass begin info**
   - When creating framebuffers, bind both colour and depth image views; adjust attachment count accordingly.
   - During command buffer recording, supply two `VkClearValue` entries (colour + depth) and ensure depth clears to 1.0F.
6. **Submission and synchronization tweaks**
   - Update `Application::run` submit wait stages to include `VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT` alongside colour output for depth readiness.
7. **Swapchain recreation path**
   - Tie depth image lifecycle to swapchain extent changes: destroy depth resources before tearing down framebuffers and recreate them immediately after new swapchain images are available.
   - Validate resize handling by triggering repeated window resizes; ensure no stale depth views remain.
8. **Validation & verification**
   - Rebuild and run the renderer, confirm depth-based occlusion (sphere/cube behind plane disappear when expected).
   - Run existing unit tests plus new depth helper tests; capture validation layer output to confirm it stays clean.

## Acceptance Criteria
- Renderer uses a depth attachment with a format supported by the active GPU; validation confirms layout transitions and memory bindings.
- Objects render with correct occlusion when moving the FPS camera (plane hides lower portions of cube/sphere when appropriate).
- Swapchain recreation (window resize) rebuilds depth resources without crashes or validation warnings.
- Unit tests pass, including coverage for the new depth-format helper, and the application runs to completion on macOS.

# SSAO Implementation Plan

## Baseline
- Renderer currently performs a single forward pass directly into the swapchain with depth testing only.
- No G-buffer, descriptor sets for post-processing, or full-screen quad infrastructure exist yet.

## Phase 1 – Geometry Pass Upgrade
1. Extend `SceneRenderer` to render into an off-screen framebuffer with separate colour, normal, and depth attachments while keeping the swapchain pass for final composition.
2. Allocate RAII-managed images/views for albedo, view-space normals, and linear depth; ensure they rebuild on swapchain resize.
3. Update shaders to output required attributes (world/view-space normals, positions) and adjust push constants if additional data is needed.
4. Define descriptor layouts exposing G-buffer attachments to later passes.
5. Verify geometry pass teardown/recreation is clean during window resizing.

## Phase 2 – SSAO Sample Data
1. Implement deterministic SSAO kernel generation (32–64 hemisphere samples) stored in a GPU buffer.
2. Create a small noise texture for tangent-space randomisation and upload it to the GPU.
3. Add unit tests validating kernel distribution (length ≤ 1, hemisphere bias) and reproducibility via seeded RNG.

## Phase 3 – SSAO Pass
1. Introduce a full-screen SSAO pipeline (graphics or compute) sampling G-buffer depth/normals, the kernel buffer, and noise texture.
2. Configure descriptor sets and samplers for these resources.
3. Implement the SSAO shader to reconstruct view-space positions, accumulate occlusion, and output to an occlusion target.
4. Integrate pass submission and handle necessary image layout transitions.

## Phase 4 – Blur/Filter Pass
1. Add separable or bilateral blur passes to smooth the raw SSAO output.
2. Use the occlusion texture as input, writing to a blurred SSAO attachment suitable for composition.
3. Make filter parameters (radius, sharpness) configurable.

## Phase 5 – Composition
1. Modify the final swapchain pass to blend the blurred SSAO term with the colour buffer (e.g., multiply diffuse lighting).
2. Bind the SSAO texture via descriptor/push constant updates.
3. Ensure ImGui overlay renders correctly after composition.

## Phase 6 – Runtime Integration
1. Add configuration structs and ImGui controls for SSAO parameters (enable flag, radius, bias, sample count).
2. Rebuild SSAO resources on swapchain recreation and window resize.
3. Provide debug visualisation (raw occlusion toggle) for validation.

## Phase 7 – Testing & Validation
1. Expand automated tests for kernel math and projection reconstruction helpers.
2. Run Vulkan validation layers to confirm descriptor usage and layout transitions.
3. Manually verify behaviour across camera movement and lighting variations.
4. Benchmark performance impact and document findings.

## Acceptance Criteria
- Geometry pass outputs albedo, normals, and depth textures with resize-safe lifecycle management.
- SSAO kernel/noise generation is deterministic and unit-tested.
- SSAO + blur passes render without validation errors; scene shows correct ambient occlusion and can be toggled.
- ImGui exposes SSAO controls; runtime remains stable through resizes and camera motion.
- Build/tests succeed (`cmake --build`, `ctest`); validation layers report no new issues.

# SSAO Quality Improvement Plan

## Goal
Lift the placeholder SSAO implementation to a production-quality ambient occlusion pipeline with physically grounded sampling, temporal stability, and useful debug tooling.

## Phases
1. **Data Preparation**
   - Reconstruct view-space positions in the SSAO shader using the linear depth attachment and the inverse projection matrix supplied via a uniform.
   - Ensure normals are in view space and normalized; add validation unit tests that compare transformed normals against expected values for canonical meshes.
   - Acceptance criteria: SSAO shader receives per-fragment view-space position/normal accurate within floating-point tolerance verified by tests.

2. **Kernel Integration & Biasing**
   - Feed the existing 64-sample kernel into the shader, randomizing per-fragment rotation via the noise texture.
   - Implement hemisphere sampling logic: project samples into clip space, check depth differences against a configurable radius, and accumulate occlusion with distance/angle falloff.
   - Acceptance criteria: Raw SSAO buffer in debug view shows smooth occlusion gradients around contact areas without large constant regions.

3. **Edge-Aware Blur Pass**
   - Add a separable bilateral blur (horizontal + vertical) operating on the SSAO output while respecting depth discontinuities via linear depth texture sampling.
   - Extend descriptors/pipelines accordingly and provide ImGui controls for blur radius and depth threshold.
   - Acceptance criteria: Blurred SSAO removes block artifacts while preserving edges (validated visually and via automated test checking that high-contrast edges remain within tolerance).

4. **Parameterization & Runtime Controls**
   - Surface radius, bias, strength, base ambient, and blur parameters in ImGui with sane defaults and clamped ranges.
   - Update SSAO config UBO to include new parameters; ensure hot updates do not stall the GPU (host-coherent buffer writes only).
   - Acceptance criteria: UI controls adjust SSAO behavior in real time without validation warnings or noticeable hitches.

5. **Testing & Validation**
   - Add unit tests for SSAO math helpers (position reconstruction, sample falloff) and integration tests that render a small scene off-screen, sampling the occlusion texture to assert expected ordering (e.g., occlusion under cube > plane far field).
   - Run validation layers to confirm all new passes respect layout transitions and descriptor bindings.
   - Acceptance criteria: `ctest` passes with new coverage, runtime validation remains clean, and debug view clearly highlights occlusion around geometry intersections.

## Acceptance Criteria
- SSAO debug mode displays contact-shadow gradients with visible occlusion near mesh intersections and minimal banding.
- Blurred SSAO integrates into the lighting pass without flicker or shading seams during camera motion.
- Parameter tweaks (radius/strength/bias/blur) respond in real time via ImGui.
- Automated tests cover reconstruction helpers and blur weights, and the render validation scene confirms occlusion ordering.

## SSAO Self-Occlusion Refinement Plan

### Goal
Reduce artificial self-occlusion on convex surfaces (e.g., sphere) while preserving contact shadows at geometry intersections.

### Phase A – Sampling Heuristics
1. **Accurate Occluder Reconstruction**
   - Reconstruct occluder view positions using `reconstruct_view_position(sampleDepth, sampleUV)` instead of assuming kernel offsets.
   - Use the delta between fragment and occluder positions for geometry-based checks (angle, distance).
   - Acceptance: SSAO shader now differentiates between same-surface vs. true occluder samples.

2. **Angular & Normal Filtering**
   - Reject samples whose direction relative to the surface normal is below an angular cosine threshold.
   - Optionally reject samples whose reconstructed normal (from the normal buffer) is within an epsilon of the fragment normal.
   - Acceptance: Sphere retains a bright cap, minimal halo.

3. **Depth & Distance Falloff**
   - Apply exponential weights for positive depth differences and distance, fully skipping samples that are closer than current depth minus bias.
   - Acceptance: Contact shadows remain soft and localized; no self-darkening on flat areas.

### Phase B – Runtime Controls & Persistence
1. Add ImGui sliders for angle cutoff, distance falloff, depth falloff, and normal epsilon; update SSAO UBO to include these parameters.
2. Ensure updates occur once per frame before command buffers record (no in-flight descriptor updates).
   - Acceptance: Adjusting sliders affects SSAO output live with clean validation and clearly perceptible falloff changes.

### Phase C – Testing & Validation
1. Extend SSAO unit tests to cover new math helpers (e.g., angle rejection, falloff function).
2. Capture debug renders for sphere/cube plane scenes before/after to confirm improvement.
3. Run full validation (build, ctest, runtime with VULKANO_MAX_FRAMES=120) and ensure no Vulkan warnings.
- Acceptance: Tests pass; debug image shows realistic SSAO; validation layers remain clean.

## SSAO Blur Resource Plan
1. **Render Targets**
   - Allocate a dedicated `VK_FORMAT_R16_SFLOAT` image for the blurred occlusion output; usage flags: `COLOR_ATTACHMENT_BIT | SAMPLED_BIT`.
   - Ensure the blur target shares the same extent as the swapchain and is recreated alongside G-buffer images.

2. **Shader Inputs**
   - Horizontal/vertical blur shaders read: raw SSAO occlusion, linear depth texture, configurable kernel weights (via UBO).
   - Descriptor set layout: binding 0 = occlusion sampler, binding 1 = depth sampler, binding 2 = blur parameters uniform buffer.

3. **Pipelines**
   - Fullscreen triangle vertex shader (reuse existing screen-space VS pattern).
   - Fragment shader performs bilateral weighting using radius and depth sigma uniforms; outputs to blur target.

4. **Command Flow**
   - SSAO pass writes to occlusion image, transitions to read-only.
   - Blur pass consumes occlusion/depth, writes to blur target, transitions result for sampling by composition.

5. **Integration Hooks**
   - Blur descriptor owns adjustable parameters (radius, depth falloff) controlled through ImGui.
   - Final composition samples blurred texture instead of raw occlusion when blur enabled.

# Material Renderer Plan

## Goal
Introduce a material-driven forward renderer that supports multiple material types (lambertian, specular, and PBR-ready placeholders), each with texture bindings, while integrating with existing SSAO and blur passes.

## Phase 1 – Material Abstractions
1. Define CPU-side material structs (albedo color, roughness, metallic, texture handles) and a material registry that assigns stable IDs.
2. Extend scene mesh definitions to reference a material ID.

### Acceptance Criteria
- Inventory of current scene asset flow captured (meshes, textures, shader bindings) and reflected in updated plan notes.
- Material registry exposes stable identifiers, default material, and basic allocation tests pass.
- Each scene mesh references a valid material ID with placeholder parameters propagating through frame data without crashes.
- Project builds with `cmake --build build`, `ctest` remains green, and runtime (`VULKANO_MAX_FRAMES=120 ./bin/vulkano_renderer`) renders existing scene unchanged visually.

### Current Scene & Material Data Flow Snapshot
- **Mesh authoring:** `vulkano::scene::MeshFactory` returns `MeshData` with per-vertex position, normal, and baked RGB colour; no material identifiers or texture references are present.
- **Scene binding:** `Application::run` constructs `std::vector<SceneRenderer::SceneMesh>` (plane/cube/sphere) by embedding `MeshData` plus model matrices; colours are assigned at mesh-generation time and implicitly treated as albedo.
- **GPU upload:** `SceneRenderer::set_scene` converts `scene::Vertex` into renderer-local `Vertex` structs and creates host-visible vertex/index buffers per mesh (`SceneRenderer::upload_mesh`); there is no shared material buffer—colour travels inside the vertex stream.
- **Shader usage:** `scene.vert` forwards `inColor` to the fragment stage; `scene.frag` applies a fixed directional Lambert diffuse term and multiplies the result by the interpolated colour, treating it as albedo. SSAO strength/ambient parameters modulate the final colour via descriptor set 0 bindings.
- **Frame orchestration:** `Application` records per-frame push constants (model/view/projection) and reuses the same mesh list during swapchain recreation; there is no runtime path to vary material parameters independently from geometry.

## Phase 2 – GPU Data & Shaders
1. Define `MaterialGpu` struct layout (align to std140/430) carrying base color, metallic, roughness, ambient, texture indices; document alignment decisions in comments.
2. Choose descriptor set slot (e.g., set 2) hosting a storage buffer for materials and optional sampled texture arrays; outline push-constant material index passage.
3. Extend pipeline layout plan: add descriptor set layout creation, descriptor pool sizing, and binding updates during renderer initialization and swapchain recreation.
4. Plan CPU upload flow: stage `MaterialGpu` buffer on application init and refresh when materials mutate; integrate with `FrameResources` or dedicated uploader to avoid per-frame reallocations.
5. Update shader TODOs: vertex stage forwards instance material index; fragment stage dereferences storage buffer and optionally samples textures.

### Acceptance Criteria
- Plan captures concrete GPU structure, descriptor bindings, and pipeline layout changes with attention to alignment and lifetime management.
- Identified update path ensures material buffer refresh occurs before command recording without stalls.
- Shader updates scoped (inputs, bindings, uniform names) and reflected in plan to guide implementation.

## Phase 3 – Texture/Resource Management
1. Adopt stb_image (or provided loader) under `third_party` with RAII wrapper that decodes sRGB colour, linear data, and exposes dimensions/channel info.
2. Define `TextureAsset` pipeline: disk path -> CPU pixel buffer -> `vk::ColorImage` + sampler creation (choose mipmapping strategy, filtering, address modes documented).
3. Extend `MaterialRegistry` to map texture IDs (albedo/normal/metallic-roughness/AO) to GPU handles with reference counting and hot-reload hooks.
4. Update descriptor set plan: pack texture array bindings into set 1 alongside the material buffer, ensuring descriptor pool sizes cover worst-case material counts.
5. Plan swapchain recreation + shutdown teardown order so images/samplers survive and are released cleanly.

### Acceptance Criteria
- Written loading flow covers asset discovery, format assumptions (sRGB vs linear), and fallback textures for missing assets.
- GPU sampler settings (filtering, wrap, anisotropy) are specified and justified for PBR use.
- Material registry stores texture handles without leaks; plan describes how updates propagate to descriptor sets.
- Resize/recreation lifecycle spelled out ensuring textures persist across swapchain rebuilds.

## Phase 4 – ImGui Material Editor
1. Add ImGui panel to list materials, adjust parameters (color, roughness, metallic), and trigger texture reloads.
2. Ensure edits trigger GPU buffer updates only when necessary (avoid mid-command-buffer updates).
3. Acceptance: UI tweaks reflect immediately in render output; descriptor updates occur safely before recording draws.

## Phase 5 – Testing & Validation
1. Add unit tests for material registry (ID allocation, duplicate prevention) and texture loader error handling.
2. Capture screenshot-based regression (manual) ensuring materials render as expected with SSAO enabled.
3. Run `ctest`, rebuild, and execute runtime with validation layers clean.

# Physically Based Lighting Plan

## Goal
Introduce a metallic-roughness physically based lighting model that works with the upcoming material system and existing SSAO pass, delivering believable diffuse/specular responses under one or more lights.

## Phase 1 – Lighting Data & Infrastructure
1. Define CPU light structs (directional, point) with color/intensity; add a light registry similar to materials.
2. Upload light data to GPU via uniform/SSBO; update frame resources to refresh the buffer safely before command recording.
3. Acceptance: Renderer compiles with a basic directional light; objects lit using current lambertian shading.

## Phase 2 – PBR Shading Core
1. Implement Cook–Torrance BRDF in shaders (GGX NDF, Smith G, Schlick Fresnel) plus Lambert diffuse.
2. Extend material parameters to supply baseColor, metallic, roughness, ambient occlusion, emissive.
3. Acceptance: Debug scene shows plausible metallic vs. dielectric highlights with tunable roughness; validation layers clean.

## Phase 3 – Texture & SSAO Integration
1. Sample material textures (albedo, metallic-roughness, normal) and feed SSAO output as ambient occlusion multiplier.
2. Add optional normal mapping to enhance shading; ensure tangent basis is supplied in vertex data.
3. Acceptance: Cube/sphere react to textures/roughness correctly; toggling SSAO affects indirect term but not direct lighting.

## Phase 4 – Extended Lighting & Controls
1. Support multiple lights (at least one directional + one point); add ImGui panel to adjust light properties live.
2. Optionally prepare hooks for environment lighting (IBL) to be implemented later.
3. Acceptance: UI can toggle/edit lights; shading responds instantly without validation warnings.

## Phase 5 – Testing & Validation
1. Add math unit tests for BRDF helper functions (Fresnel, distribution, geometry terms) and tangent-space utilities.
2. Rebuild, run `ctest`, and exercise runtime with validation layers enabled.
3. Acceptance: Tests pass; screenshots confirm metallic vs. dielectric behaviour across roughness values.
