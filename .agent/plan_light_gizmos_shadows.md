# Multi-Light Gizmo + Per-Light Shadow Toggle Plan

## Goals
1. Render light gizmos for all active lights (directional and point).
2. Allow toggling shadow casting per light (initially for directional/point lights).
3. Keep behaviour validation-clean and expose controls in ImGui.

## Step-by-Step

### 1. Analyze current lighting-state usage
- Inspect `SceneRenderer` to understand how it stores the primary light direction and debug mesh.
- Note how ImGui currently edits only the first light and how the shadow pass uses the primary directional light.
- Acceptance: Document clarifications in `.agent/plan_light_gizmos_shadows.md` (done).

### 2. Extend light data structures for shadow flags
- Add `bool castsShadow` to `scene::Light` (default true for directional, false for point?).
- Update `LightRegistry` sanitization and tests to preserve the flag.
- Acceptance: Tests cover adding/removing lights and verifying shadow flags persist.

### 3. Update GPU light buffer
- Add shadow flag per light in `LightGpu` (e.g., pack into colorType.w or new component).
- Ensure shader reads (for now) just uses the first shadow-casting directional light.
- Acceptance: GPU buffer layout matches CPU struct; tests updated.

### 4. Render gizmos for all lights
- Create separate debug meshes for each light: directional arrow for directional lights and sphere/billboard for point lights.
- Upload and draw them in `record_command_buffer` with distinct colors.
- Acceptance: At runtime, all lights display markers; toggling show/hide works.

### 5. ImGui controls per light
- In Lighting panel, add a checkbox per light to toggle `castsShadow`.
- Orientation controls: direction for directional, position/range for point (already present).
- Acceptance: UI interacts with per-light shadow flag; setting dirty triggers buffer rebuild.

### 6. Shadow pass selection logic
- Modify `compute_light_view_projection` to choose the first directional light with `castsShadow`.
- If none, skip shadow pass.
- Acceptance: Shadow pass no longer runs when no shadow-casting directional light exists.

### 7. Descriptor + shader guard
- Update fragment shader to skip shadow sampling when the light does not cast shadows.
- Potential future: support point-light shadows; currently remain directional-only but use flag for gating.
- Acceptance: Scenes with shadows disabled on main directional light show no shadow influence.

### 8. Testing & Validation
- Update light tests for new flag.
- Build & run `ctest`.
- Visual verification: add second light, see gizmo; toggle `castsShadow` to disable shadows.
- Note: keep existing shadow debug view working for primary shadow-casting light.

## Remediation Plan (Babe Steps)

### Goal A — Render a Gizmo Per Light Instance
1. **Catalogue gizmo lifecycle code paths**
   - Review `SceneRenderer::set_light_resources`, `create_light_debug_mesh`, and points where point-light meshes are spawned.
   - Capture current assumptions (single directional mesh, vector for points) in this document.
   - *Acceptance:* Notes list every function touching `m_lightDebugMesh` and `m_pointLightDebugMeshes`, including creation, update, and destruction hooks.
   - **Findings:**
     - `SceneRenderer::SceneRenderer` invokes `create_light_debug_mesh()` to allocate the single directional gizmo at construction.
     - `SceneRenderer::create_light_debug_mesh()` builds vertex/index buffers for the arrow mesh and seeds `m_lightDebugMesh`.
     - `SceneRenderer::destroy_light_debug_mesh()` frees buffers, resets counts, and cascades into `destroy_point_light_debug_meshes()`.
     - `SceneRenderer::destroy_point_light_debug_meshes()` iterates `m_pointLightDebugMeshes`, freeing per-point buffers and clearing the vector.
     - `SceneRenderer::set_light_resources()` rewrites the directional gizmo vertex data in place, rebuilds every point-light mesh, and stores them back into `m_pointLightDebugMeshes` after destroying the prior list.
     - `SceneRenderer::record_command_buffer()` binds and draws `m_lightDebugMesh` (single arrow) and then iterates `m_pointLightDebugMeshes` for point markers when `m_showLightDebug` is true.
     - `SceneRenderer::~SceneRenderer()` calls `destroy_light_debug_mesh()` as part of teardown which in turn clears point gizmos.
2. **Model per-light gizmo storage**
   - Design a lightweight struct keyed by `LightId` that owns GPU buffers for each gizmo.
   - Decide update strategy (rebuild whole list vs. incremental) based on registry churn.
   - *Acceptance:* New struct sketch recorded here with fields, ownership semantics, and update triggers.
   - **Proposed data model:**
     - Introduce `LightGizmoHandle` (struct) containing `scene::LightId id`, `DebugMesh mesh`, `glm::mat4 transform`, and `scene::LightType type` for quick dispatch.
     - Store directional gizmos in a dedicated `std::optional<LightGizmoHandle>` to preserve the single-arrow pipeline while enabling multiple entries later.
     - Maintain point gizmos in `std::vector<LightGizmoHandle>` keyed by stable registry indices; entries own their GPU buffers via the embedded `DebugMesh`.
     - Track a `bool dirty` flag on each handle to allow lazy vertex updates when color/intensity changes without reallocating buffers.
     - Manage a renderer-level `GizmoCache` struct with two collections (directional/point) plus helper methods `sync_from_registry(const scene::LightRegistry&)` and `release(VkDevice)`.
   - **Update policy:**
     - On `set_light_resources`, call `GizmoCache::sync_from_registry` which diffs registry contents against cached handles.
     - Added lights allocate new buffers through shared helper factories; removed lights trigger buffer destruction via `release` on the handle.
     - Directional gizmo updates mutate the existing handle in place (no reallocation) when the corresponding light remains but changes attributes.
3. **Update gizmo creation logic**
   - Implement factory helpers that build directional and point gizmo vertex/index buffers on demand.
   - Ensure buffers honour existing memory allocation patterns (host visible staging or device local).
   - *Acceptance:* Pseudocode or interface definition captured, showing inputs/outputs and resource lifetime guarantees.
   - **Helper interfaces:**
     - `create_directional_gizmo(const VulkanContext&, const scene::Light&, VkDeviceSize)` → returns `DebugMesh` populated with arrow geometry sized to the light’s intensity (optional scale factor) and colored via `light.color`.
     - `create_point_gizmo(const VulkanContext&, const scene::Light&)` → returns `DebugMesh` for billboard/cross marker positioned at origin; caller multiplies transform.
     - `update_directional_gizmo_vertices(const VulkanContext&, DebugMesh&, const scene::Light&)` → remaps vertex buffer to refresh color/direction vectors without reallocating.
     - `destroy_gizmo(DebugMesh&, const VulkanContext&)` → releases buffers and resets counts; reused by cache eviction logic.
   - **Lifetime contract:**
     - All helpers expect buffers allocated with `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | HOST_COHERENT_BIT` to match current debug mesh approach.
     - Callers own the returned `DebugMesh` and must invoke `destroy_gizmo` when removing a light.
     - Helper functions never mutate global renderer state; they operate purely on provided handles and context references.
4. **Wire gizmo updates into `set_light_resources`**
   - Generate or refresh gizmo data for every light after descriptor updates.
   - Remove the single `m_lightDebugMesh` dependency and replace draw state with per-light collection.
   - *Acceptance:* Sequence diagram or bullet list describing update order (descriptor write → gizmo sync → draw list cache).
5. **Draw all gizmos during command recording**
   - Iterate over directional and point gizmo lists when `m_showLightDebug` is true.
   - Apply per-light transforms (position, direction) when binding vertex buffers.
   - *Acceptance:* Rendering pass plan notes which pipelines/descriptors are reused and how instance transforms are supplied.
   - **Draw submission strategy:**
     - Maintain a transient `std::vector<DrawItem>` assembled each frame containing `(DebugMesh*, glm::mat4 model, scene::LightType type)` pulled from the gizmo cache.
     - Directional gizmos: reuse the existing debug pipeline; compute model matrix from light direction by rotating base arrow mesh so its -Z axis aligns with `light.direction`, then translating to scene origin or light position (configurable).
     - Submit directional draws first to preserve legacy visual order, binding vertex/index buffers per gizmo and updating a small push constant block with the model matrix and color tint.
     - Batch point gizmos by binding shared pipeline/state once, then iterating over cached handles, pushing model matrix per draw; point models already include translation and scale from cache.
     - When no gizmos exist for a given type, skip the bind/draw to avoid redundant state changes.
     - Acceptance: pseudo-flow logged here describing `if (showLightDebug) → assemble draw items → bind pipeline → for each item {bind buffers, push constants, draw}`.
6. **Handle light removal and registry shrink**
   - Define logic for releasing GPU resources when a light is removed or changes type.
   - Add validation to keep gizmo list aligned with registry indices after deletions.
   - *Acceptance:* Documented algorithm for shrinking the gizmo cache with complexity guarantees and edge-case coverage.

### Goal B — Respect Per-Light Shadow Matrices
1. **Document current shadow matrix flow**
   - Trace from ImGui toggle → `LightRegistry` → `SceneRenderer::compute_light_view_projection` → push constants.
   - Identify where single `m_lightDirection`/`m_primaryLightCastsShadow` collapses multiple lights.
   - *Acceptance:* Flow description logged here, highlighting every mutation of the light direction and matrix.
2. **Define target behaviour**
   - Choose strategy: (a) single shadow map that tracks the highest-priority casting directional light, or (b) multiple shadow maps (one per caster) with indexed sampling.
   - Record priority rules (e.g., nearest shadow-casting light, stable ordering).
   - *Acceptance:* Decision written with justification (performance, memory) and acceptance criteria for when multiple casters are active.
   - **Decision:** adopt a single-shadow-map strategy for the next increment.
     - Maintain an ordered list of directional lights by registry index; the first entry with `castsShadow == true` becomes the active caster.
     - When the active caster changes (toggle, removal, insertion ahead of it), flag the shadow resources dirty so the view-projection matrix and light-space transforms recompute before the next draw.
     - Defer multi-shadow-map support until performance budgets and memory costs are revisited; document extension points for adding a shadow-map pool later.
   - **Acceptance criteria:**
     - Toggling shadows off on the current caster immediately promotes the next eligible directional light and updates the debug shadow map.
     - When no directional light casts shadows, the shadow pass is skipped and the fragment shader receives an identity light matrix with `shadowEnabled = 0`.
     - Reordering or removing lights yields deterministic caster selection (lowest registry index wins) without dangling descriptors.
3. **Adjust scene state to track per-light shadow data**
   - Extend renderer state to hold a struct per directional light with: light id, cached view-projection matrix, shadow map handle (if owned), and dirty flags.
   - Outline how these structs sync with registry changes.
   - *Acceptance:* Data model documented with field list and lifecycle notes.
   - **Proposed state:**
     - Define `DirectionalShadowSlot` containing:
         - `scene::LightId id;`
         - `glm::mat4 viewProjection;`
         - `glm::vec3 direction;`
         - `bool castsShadow;`
         - `bool dirtyMatrix;`
     - Add `std::vector<DirectionalShadowSlot> m_directionalShadows;` to `SceneRenderer` to mirror the light registry order.
     - Maintain `std::optional<scene::LightId> m_activeShadowCaster;` identifying the currently selected slot.
     - On each `set_light_resources` call, rebuild the vector by iterating registry directional lights, updating existing slots in place when ids align, and marking `dirtyMatrix` when direction/intensity/scene bounds change.
     - When `m_activeShadowCaster` differs from the first slot with `castsShadow == true`, update the optional, flag the shadow pass dirty, and reset the cached matrix to force recomputation before the next frame.
     - Store `glm::mat4 m_lastLightViewProjection;` for the matrix actually uploaded to shaders, avoiding recomputing when nothing changes.
4. **Update shadow map resource management**
   - If single shadow map selected, document algorithm to select the active caster and update matrix when toggles change.
   - If multiple maps selected, plan allocation strategy (pooling, max count) and descriptor layout changes.
   - *Acceptance:* Resource plan specifies Vulkan objects touched, creation/destruction triggers, and limits (e.g., max 4 directional lights).
   - **Resource algorithm (single map):**
     - Keep existing `ShadowMap` allocation (image + framebuffer) but gate ownership behind a small `ShadowResources` struct storing `bool needsRefresh;` and `scene::LightId owner;`.
     - When `m_activeShadowCaster` changes, set `needsRefresh = true` and update `owner`.
     - During frame build, if `needsRefresh` is true, call `compute_light_view_projection` with the new slot and write the matrix to `m_lastLightViewProjection`; reset flag once command buffers are recorded.
     - Ensure descriptor set `m_shadowDescriptorSet` remains bound; only the matrix updates, so no VkDescriptor updates required when owner switches.
     - If no active caster exists, skip shadow pass, leave image untouched, and set `owner = scene::LightId::invalid()`.
     - For future multi-map expansion, note that the `ShadowResources` struct can evolve into a vector with a fixed budget (e.g., 2 cascaded maps) driving descriptor array bindings.
5. **Modify command recording/push constants**
   - Describe how per-light matrices reach the shader (additional push constants, SSBO, or descriptor buffer).
   - Plan shader changes to sample the correct matrix per fragment (e.g., index from light buffer).
   - *Acceptance:* Step outlines data flow from CPU struct to GPU shader, including synchronization points.
6. **Revise ImGui + validation paths**
   - Ensure UI reflects active shadow casters and allows selecting which lights own shadow maps if capacity limited.
   - Plan QA checklist covering scenarios: multiple directional lights toggled, toggles flipped mid-frame, light deletions.
   - *Acceptance:* QA matrix documented, listing cases and expected visual outcomes.
7. **Testing strategy**
   - Enumerate unit/integration tests to add (e.g., registry → renderer mapping, shader branch coverage via spirv-cross validation, screenshot diff harness).
   - Define manual validation steps (run demo, toggle lights, inspect debug shadow view per light).
   - *Acceptance:* Test plan recorded with pass/fail criteria and tooling (Catch2, RenderDoc, etc.).
