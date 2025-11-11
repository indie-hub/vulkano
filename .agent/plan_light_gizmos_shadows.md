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
   - **Existing flow:**
     - ImGui checkbox updates `scene::Light::castsShadow` stored in `LightRegistry` (see `Application::run` lighting panel logic).
     - `Application` sets `lightsDirty = true`, triggering `lightBuffer.update` and `renderer->set_light_resources` on the next frame.
     - `SceneRenderer::set_light_resources` scans the registry, captures the first directional light flagged for shadows, and stores its direction/color/intensity into `m_lightDirection`, `m_lightColor`, `m_lightIntensity`, and `m_primaryLightCastsShadow`.
     - The method also updates the single directional debug mesh and point gizmo list before caching `m_lightBuffer = &buffer`.
     - Later, `SceneRenderer::record_command_buffer` calls `compute_light_view_projection` which returns `std::nullopt` when `m_primaryLightCastsShadow` is false or when scene bounds are invalid; otherwise, it builds the matrix from `m_lightDirection` plus cached scene AABB.
     - The resulting matrix and a `shadowEnabled` flag are packed into push constants (`PushConstants::lightViewProjection` and `shadowParams`) for the main scene pass.
     - Fragment shader samples the shadow map only if `shadowEnabled > 0` and the light’s GPU flag indicates it casts shadows.
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
   - **QA matrix:**
     1. *Single directional light (default sun)* — toggle `Casts Shadow` off/on; expect shadow map to clear when off and restore identical result when on.
     2. *Two directional lights* — disable primary, enable secondary; expect shadow map orientation to match secondary direction and debug gizmo to highlight active caster.
     3. *Rapid toggle between lights* — alternate checkboxes every frame for ~10 seconds; expect stable shadow map transitions without validation errors.
     4. *Directional removal* — delete active caster while secondary is disabled; expect shadow pass to skip and shader receive `shadowEnabled = 0`.
     5. *Directional removal with fallback* — delete active caster while secondary has shadows enabled; expect immediate promotion of secondary and updated matrix.
     6. *Point light regression* — ensure point light toggles remain absent and gizmos continue rendering without shadow influence.
     7. *Scene bounds invalidation* — load scene with no meshes (or clear scene); expect `compute_light_view_projection` to return null and rendering to continue without crashes.
7. **Testing strategy**
   - Enumerate unit/integration tests to add (e.g., registry → renderer mapping, shader branch coverage via spirv-cross validation, screenshot diff harness).
   - Define manual validation steps (run demo, toggle lights, inspect debug shadow view per light).
   - *Acceptance:* Test plan recorded with pass/fail criteria and tooling (Catch2, RenderDoc, etc.).

## Multi-Caster Shadow Map Upgrade (Babe Steps)

### Goal C — Expand Shadow Resources for Multiple Casters
1. **Inventory current shadow resource ownership**
   - Document which methods allocate, bind, and destroy the single `ShadowMap` (image, framebuffer, descriptors).
   - Capture existing assumptions about one-pass rendering and descriptor bindings.
   - *Acceptance:* Notes list every function touching `m_shadowMap`, `m_shadowPass`, and shadow descriptor sets, including lifetimes.
   - **Ownership inventory:**
     - `SceneRenderer::SceneRenderer` calls `create_shadow_resources()` and `create_shadow_descriptors()` to allocate `m_shadowMap`, initialise `m_shadowPass`, and set up descriptor pools/sets.
     - `create_shadow_resources()` creates the depth image (`ShadowMap`), framebuffer, and sets `m_shadowExtent`; destruction mirrored in `destroy_shadow_resources()` invoked from destructor and swapchain recreation paths.
     - `create_shadow_descriptors()` allocates `m_shadowDescriptorPool` and `m_shadowDescriptorSet`, binding the shadow map view/sampler; `destroy_shadow_descriptors()` frees them.
     - `record_command_buffer()` invokes `record_shadow_pass()` when shadow pass enabled; `record_shadow_pass()` uses `m_shadowPass` to render into `m_shadowMap`.
     - `destroy_light_debug_mesh()` does not touch shadow resources; only `destroy_shadow_resources()` and `destroy_shadow_descriptors()` manage the lifetime alongside swapchain teardown.
     - Swapchain recreation triggers `destroy_shadow_resources()`/`create_shadow_resources()` via `recreateSwapchain()` (not shown here but wired in application).
2. **Decide maximum supported casters and budgeting**
   - Choose an initial upper bound (e.g., 3 directional lights) along with memory estimates per shadow map (format, resolution).
   - Record trade-offs for higher counts vs. performance/memory hits.
   - *Acceptance:* Decision documented with numeric limits, memory calculations, and justification.
   - **Decision & budget:**
     - Support up to **3** simultaneous shadow-casting directional lights in the initial multi-caster release.
     - Each shadow map remains at 2048×2048, VK_FORMAT_D32_SFLOAT (4 bytes per texel): `2048 * 2048 * 4 ≈ 16 MB` per map.
     - Total shadow-map memory footprint at capacity: `3 * 16 MB = 48 MB`, plus negligible framebuffer/descriptor overhead.
     - Trade-off rationale: 3 lights covers typical key/fill/rim setups without blowing GPU memory; higher counts would scale memory and command buffer time linearly.
     - Document extension path to raise the limit when VRAM budgets allow, potentially via configuration setting.
3. **Design `ShadowResources` pool structure**
   - Sketch a struct holding per-caster entries: light id, image, framebuffer, descriptor set, and dirty flags.
   - Specify allocation strategy (eager allocate up to max vs. lazy create on demand).
   - *Acceptance:* Struct diagram recorded with fields, ownership semantics, and init/destroy routines.
   - **Proposed structure:**
     - `struct ShadowSlot { scene::LightId id; ShadowMap map; ShadowPass pass; VkDescriptorSet descriptor; bool active; bool dirtyMatrix; glm::mat4 viewProjection; };`
     - `struct ShadowResources { std::vector<ShadowSlot> slots; VkDescriptorSetLayout layout; VkDescriptorPool pool; VkExtent2D extent; VkFormat format; };`
     - Allocate `slots` upfront to the maximum caster count (3). Each slot owns its own `ShadowMap`/`ShadowPass` pair so maps can be rendered independently.
     - `ShadowResources::initialise` creates descriptor layout/pool sized for max casters and initialises each `ShadowSlot` with inactive state and allocated resources (images/framebuffers) at shared resolution.
     - `ShadowResources::release` walks slots, destroying shadow maps/passes and freeing descriptors/pool/layout.
     - Each slot’s `active` flag indicates whether it currently serves a light; `dirtyMatrix` tracks when the view-projection needs recomputation.
     - Descriptor strategy: allocate per-slot descriptor set binding the map’s image/sampler; main renderer will bind all active sets via array or dynamic binding.
4. **Plan descriptor layout changes**
   - Determine whether to use array descriptors or bindless approach for multiple shadow maps.
   - Outline updates needed in pipeline layout, shader bindings, and descriptor writes.
   - *Acceptance:* Descriptor update matrix enumerating new set/binding assignments and compatibility with existing shaders.
   - **Descriptor plan:**
     - Extend existing shadow descriptor set to bind an array of combined image samplers sized to the max caster count (3).
     - `ShadowResources` allocates a descriptor pool with `descriptorCount = maxCasters` for combined image samplers and a single descriptor set reused each frame.
     - `SceneRenderer` writes all active shadow map `VkDescriptorImageInfo` entries to the array; inactive slots receive a dummy depth texture (or reuse last active) to keep array dense.
     - Pipeline layout update: shadow descriptor set (set = 3) now exposes binding 0 as `descriptorCount = maxCasters`; shaders read via index provided in light GPU data.
     - Compatibility: existing single-caster shader path will be refactored to loop over active casters, using the descriptor index encoded in `LightGpu` entries.
     - Note potential future extension to bindless/BDA if descriptor array limits become a concern.

### Goal D — Update Renderer Selection Logic
1. **Define caster prioritisation rules**
   - Establish stable ordering (e.g., registry index) and tie-breaking when more lights request shadows than capacity.
   - Include strategy for retaining previous casters to avoid flicker when toggles churn.
   - *Acceptance:* Prioritisation spec with explicit examples for over-capacity scenarios.
   - **Prioritisation rules:**
     - Primary ordering by `LightId` (registry index); lowest index has highest priority.
     - Maintain a small LRU cache to favour lights that previously held a slot when all are still casting shadows, preventing flicker when toggles rapidly toggle.
     - When more than 3 directional lights request shadows, assign slots to the first 3 indices; remaining lights fall back to no shadows.
     - If a high-priority light toggles off, its slot is released and reassigned to the next eligible light in index order.
     - Edge case: if a low-index light toggles on while capacity full, it preempts the highest-index active slot; the displaced light’s shadow slot is recycled.
     - Record transitions in a debug log (future optional) to help QA verify slot assignments.
2. **Map registry changes to resource pool**
   - Pseudocode the sync routine that reconciles `LightRegistry` against the shadow pool, assigning or releasing slots.
   - Account for additions, removals, type changes, and toggles mid-frame.
   - *Acceptance:* Algorithm description covering all mutation cases with noted complexity.
   - **Slot sync algorithm:**
     1. Build a vector `directionalLights` of `(LightId, Light)` pairs filtered for directional type.
     2. Sort `directionalLights` by `LightId` ascending.
     3. Iterate existing `ShadowResources::slots` updating `active` flags to false.
     4. For each light in `directionalLights`:
        - Skip if `castsShadow == false`.
        - Check if any slot already holds this light id; if so, mark `active = true`, update `dirtyMatrix` if direction/intensity changed, continue.
        - Otherwise, gather available slots (inactive). If none, apply prioritisation by comparing current light against lowest-priority active slot (per rules). Replace target slot if higher priority.
        - When (re)assigning a slot: set `id`, `active = true`, `dirtyMatrix = true`, and refresh descriptor image info for that slot.
     5. After assignment loop, any slot left inactive is considered free; optional step to reuse descriptors for dummy texture.
     6. Complexity: O(N log N) for sort (N directional lights), plus O(maxCasters) comparisons per assignment.
3. **Update matrix computation path**
   - Plan how `compute_light_view_projection` (or successor) will iterate over active casters and produce per-slot matrices.
   - Decide caching strategy to avoid redundant recomputations when lights idle.
   - *Acceptance:* Flow documented showing inputs, caching, and outputs for each slot.
   - **Matrix computation plan:**
     - Introduce `compute_shadow_matrices(const ShadowResources&, const scene::LightRegistry&)` returning a vector of matrices aligned with slot ordering.
     - For each active slot, fetch the corresponding light from registry, recompute matrix if `dirtyMatrix` or scene bounds changed; cache result in slot.
     - Use existing orthographic frustum calculation per light direction but allow per-slot overrides (future cascades).
     - Function returns span/vector of matrices to be uploaded to shader buffer or push constants.
     - Inactive slots produce identity matrices; shader will ignore them based on active count.
     - Complexity: O(activeCasters) per frame when dirty; otherwise reuse cached matrices.

### Goal E — Shader & Command Buffer Changes
1. **Plan shader inputs for multiple matrices**
   - Decide between SSBO of matrices, push constant array, or descriptor buffer update.
   - Ensure alignment with chosen descriptor layout and SPIR-V limits.
   - *Acceptance:* Input layout recorded with counts, types, and binding indices.
2. **Determine sampling logic in fragment shader**
   - Define how each light record references its shadow map (index, flag) and how the shader loops over casters.
   - Include fallbacks when a light lacks a shadow slot.
   - *Acceptance:* High-level pseudocode demonstrating shader control flow and branch guards.
3. **Update command recording order**
   - Plan the sequence for rendering each shadow map (multiple passes) before main scene render.
   - Note synchronization requirements between passes (barriers, semaphores if any).
   - *Acceptance:* Command buffer timeline documented with stages and required barriers.

### Goal F — Debugging, UI, and QA
1. **UI adjustments**
   - Outline updates to ImGui panels to display active casters, allow prioritisation (if manual), and show per-shadow-map stats.
   - *Acceptance:* UI requirements list describing new controls/labels.
2. **Debug visualisation plan**
   - Decide how to preview multiple shadow maps (e.g., atlas grid, selectable dropdown).
   - *Acceptance:* Debug display approach written with expected behaviour when cycling through maps.
3. **QA expansion**
   - Extend the existing QA matrix with cases covering more than one active caster, capacity overflows, and toggles during runtime.
   - *Acceptance:* Updated QA checklist appended with multi-caster scenarios.

### Goal G — Testing Strategy Extension
1. **Unit/integration coverage**
   - Specify new tests for slot assignment, matrix caching, and descriptor binding validation.
   - Identify areas where mocks or fixtures are needed to simulate multiple lights.
   - *Acceptance:* Test list recorded with expected assertions and tooling.
2. **Performance validation**
   - Define profiling steps to measure the cost of additional shadow passes (CPU & GPU timing).
   - *Acceptance:* Performance plan captured with thresholds or regression triggers.
   - **Automated tests:**
     - Add Catch2 unit tests for new shadow state helpers (e.g., `promote_shadow_caster` function) verifying selection logic when lights change order or toggles.
     - Extend existing `light_registry` tests to cover `castsShadow` sanitisation for point vs directional lights and ensure removal updates indices.
     - Introduce integration-style test harness (headless) that builds a mock renderer instance, injects synthetic scene bounds, and asserts `compute_light_view_projection` returns the expected matrix for given light direction.
     - Validate shader branching via SPIR-V reflection or offline shader test ensuring the new shadow flag toggles the sampling branch.
   - **Manual/visual tests:**
     - Reuse the QA matrix scenarios; capture screenshots for baseline and promotion cases.
     - Use RenderDoc to inspect the shadow map after promoting different casters, verifying depth values reflect the correct light orientation.
   - **Tooling:**
     - Ensure headless integration test runs under `ctest` with deterministic seed.
     - Document need for GPU validation layers (VK_LAYER_KHRONOS_validation) during manual runs to catch descriptor mismatches.
