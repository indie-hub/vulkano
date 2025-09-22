# Primitive Rendering Expansion Design

## Overview
The current renderer draws a single host-visible triangle and pushes color via push constants. To support a ground plane, cube, and icosphere with Blinn–Phong shading and runtime controls, we introduce a modular scene/primitive system with GPU-managed meshes, descriptor-backed materials, and depth-aware rendering. All additions follow RAII, SOLID, and Clean Code requirements and continue to surface stats through ImGui.

## Key Components

### 1. Geometry & Primitives
- `MeshData`: value type holding CPU-side `std::vector<Vertex>` and `std::vector<std::uint32_t>` plus metadata (bounds). Provides helper to compute normals if needed.
- `Primitive`: abstract base exposing
  - `virtual const MeshData& mesh() const noexcept = 0;`
  - `virtual glm::mat4 model_matrix() const noexcept = 0;`
  - `virtual PrimitiveProperties& properties() noexcept` / `const` variant for ImGui (position, rotation, scale, base color, shininess, optional rebuild triggers).
- Concrete primitives (`PlanePrimitive`, `CubePrimitive`, `IcospherePrimitive`) generate CPU mesh in constructors using clean helper functions. All member data initialised via `{}`.
- `PlaneParameters`, `CubeParameters`, `IcosphereParameters` structs encapsulate configurable dimensions, tiling, subdivisions, ensuring no magic numbers in logic. Icosphere generator supports subdivision 0–5 with indexed geometry and shared vertex deduplication.

### 2. GPU Resources
- Extend `Buffer` to support
  - device-local vertex/index buffers created via staging copy (`VK_BUFFER_USAGE_TRANSFER_DST_BIT | VERTEX/INDEX`).
  - host-visible staging builder helper for one-off copies.
- Introduce `MeshGpuResources` RAII wrapper owning vertex/index buffers and device memory binding. Handles upload in constructor using existing command allocator and a transient command buffer (dedicated queue submit with fence + idle wait for simplicity).
- `Scene` class aggregates primitives, maintains `std::vector<std::unique_ptr<Primitive>>` and corresponding `MeshGpuResources`. Provides iteration for rendering and handles rebuilding GPU buffers when primitives flag dirty (e.g., changing icosphere subdivisions).

### 3. Rendering Pipeline
- New `Vertex` layout: position (location 0), normal (1), uv (2). Update binding/attribute descriptions accordingly.
- Introduce uniform buffers:
  - `GlobalUbo { glm::mat4 view; glm::mat4 projection; glm::vec3 lightPosition; float lightIntensity; glm::vec3 cameraPosition; float padding; }` stored in persistent host-visible buffer updated each frame.
- Push constants per primitive: `struct PrimitivePushConstants { glm::mat4 model; glm::vec3 baseColor; float shininess; };`
- Add descriptor set layout + pool for global UBO (set 0). Pipeline layout includes descriptor set and push constant range. Update shaders to consume new data and implement Blinn–Phong shading.
- Extend render pass to include depth attachment. Introduce `DepthResources` RAII class creating image + view (VK_FORMAT_D32_SFLOAT). Update framebuffers to include depth view. Acquire format via physical-device-supported depth list.
- Update graphics pipeline to enable depth testing, use SPIR-V compiled from new shaders in `shaders/mesh.vert/.frag`. Configure dynamic viewport/scissor as before (flipped Y for macOS).

### 4. Camera & Controls
- Create `CameraController` that maintains orbit camera state (target, distance, yaw, pitch) and builds view matrix via GLM right-handed conventions. Responds to GLFW mouse drag + scroll (with ImGui capture checks). Provide `update(delta, windowInput)` call from main loop.
- Projection derived from swapchain extent plus configurable vertical FOV constant and near/far planes stored in named constants.

### 5. ImGui Integration
- Retain existing stats window.
- Add "Scene" window with
  - light controls (position slider, intensity float).
  - collapsible sections per primitive editing transform (position xyz, rotation xyz degrees, scale xyz), color, shininess.
  - For `IcospherePrimitive`, subdivisions slider (0–5) triggering CPU mesh rebuild and GPU reupload via scene interface.

### 6. Shader & Asset Pipeline
- Replace triangle shaders with `mesh.vert/.frag`; ensure CMake compiles them and post-build copies to `bin/shaders`. Uniform binding constants & push constant offsets defined in header shared between CPU/GPU (e.g., `include/vulkano/shader_types.hpp`).

### 7. Testing Strategy
- Unit tests:
  - Mesh generation counts (plane vertex/index counts match dimensions & winding; cube unique vertices 24; icosphere subdivisions produce expected counts; normals unit length within epsilon).
  - Vertex layout functions return expected binding/attribute formats.
- Integration test: instantiate app, ensure scene initialises with three primitives (validate primitive count via exported accessor or logs) while still skipping gracefully when Vulkan unavailable.
- Keep runtime-coded tests minimal to satisfy 20% effort guideline.

### 8. Build & Packaging
- Update CMake to include new source files explicitly and compile new shaders. Ensure `bin/` retains all output binaries and shader SPIR-V. Mac compatibility enforced via MoltenVK safe features (no unsupported extensions). Validation toggle continues to respect config.

### 9. Next Steps
1. Implement primitive hierarchy and mesh generators following this design.
2. Build GPU upload path, descriptor sets, and render loop updates.
3. Integrate ImGui controls and camera/light logic.
4. Update tests, shaders, and documentation; rebundle binaries.
