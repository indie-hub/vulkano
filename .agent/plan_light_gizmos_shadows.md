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
