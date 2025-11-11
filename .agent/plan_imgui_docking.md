# Plan: Adopt Dear ImGui Docking UI

## Objective
Introduce the official Dear ImGui docking branch so the renderer exposes a flexible dockspace-driven UI. The end state must let users reposition panels, undock the viewport, and persist their layout without disrupting rendering or camera controls.

## Preconditions
- Current ImGui backend (GLFW + Vulkan) is compiling from `third_party/imgui`.
- Frame lifecycle already calls `ImGui::NewFrame()`/`Render()` and draws an overlay window.
- `imgui.ini` is stored alongside the executable; confirm read/write permissions.

## Phase 1 — Baseline Audit
1. **Catalogue existing integration points**
   - Locate context creation, configuration flags, font loading, backend init, frame begin/end calls.
   - Map all current overlay panels and their dependencies (scene graph, SSAO controls, metrics).
   - *Acceptance:* Notes appended to this plan summarising files/functions responsible for ImGui lifecycle and listing every panel that must become a dockable window.

2. **Confirm Dear ImGui version**
   - Verify commit hash/tag currently vendored so the docking branch version can be matched.
   - *Acceptance:* Version string recorded in plan (e.g., `v1.90.x`), ensuring upgrade path compatibility.

## Phase 2 — Upgrade Dear ImGui to Docking Branch
3. **Fetch docking sources**
   - Update `third_party/imgui` to the official `docking` branch (matching current tag).
   - Sync auxiliary files: `imgui_widgets.cpp`, `imgui_tables.cpp`, backends, and license notes.
   - *Acceptance:* Directory matches upstream layout; no stale files remain.

4. **Update CMake targets**
   - Amend `third_party/imgui/CMakeLists.txt` (or equivalent) to compile new units and include backend files required for docking/views.
   - Ensure build still links against renderer without warnings.
   - *Acceptance:* `cmake --build build` succeeds; linker includes updated objects.

5. **Smoke test build**
   - Launch application with docking disabled to confirm no regressions before toggling new flags.
   - *Acceptance:* Renderer starts and exits cleanly; validation layers remain silent.

## Phase 3 — Enable Dockspace Workflow
6. **Activate docking flags**
   - Set `ImGuiConfigFlags_DockingEnable` (and optionally `ImGuiConfigFlags_ViewportsEnable` if platform-ready).
   - Adjust backend initialisation for multi-viewport (window transparency, platform updates) when enabled.
   - *Acceptance:* Application runs without ImGui assertions; IO flags confirm docking activation.

7. **Create persistent dockspace**
   - At frame start, call `ImGui::DockSpaceOverViewport()` (or build a dedicated hidden window hosting `DockSpace`).
   - Provide default layout builder executed when no layout is stored (split viewport, side panels, bottom console).
   - *Acceptance:* UI displays dock separators; panels can be rearranged; default layout reproducible after deleting `imgui.ini`.

8. **Split panels into windows**
   - Convert existing monolithic overlay to standalone windows:
     - `Viewport`
     - `Scene Graph`
     - `Materials`
     - `Lights`
     - `Post-Processing` (SSAO/shadows)
     - `Metrics/Console`
   - Use consistent naming, tooltips, and flags (e.g., disable collapse only where necessary).
   - *Acceptance:* Each window appears as a dockable tab; content matches previous overlay functionality.

9. **Integrate viewport rendering**
   - Ensure the Vulkan render target is presented inside the `Viewport` window (`ImGui::Image` with descriptor), respecting window size.
   - Adjust camera controller to react only when viewport is focused/hovered.
   - *Acceptance:* Viewport resizing updates render resolution; camera controls unaffected by other window interactions.

10. **Input focus and cursor management**
    - Gate FPS camera capture by viewport focus; release cursor when interacting with UI panels.
    - Handle multi-viewport cursor transitions if enabled (set `io.ConfigViewportsNoAutoMerge` as appropriate).
    - *Acceptance:* Users can drag panels without breaking camera controls; cursor hiding/restoring behaves deterministically.

## Phase 4 — Persistence, Styling, and QA
11. **Layout persistence & reset**
    - Confirm `imgui.ini` writes new dock layout.
    - Add menu command or hotkey to reset to default dock builder layout.
    - *Acceptance:* Layout customisations survive restart; reset restores canonical arrangement.

12. **Style polish**
    - Review theme for dock tabs, separators, viewport background; tweak colors for readability.
    - Ensure DPI/Retina scaling preserved.
    - *Acceptance:* Visual inspection checklist signed; no illegible text or artifacts.

13. **Validation & testing**
    - Extend automated tests where feasible (e.g., flag assertions via integration test harness or config snapshot).
    - Update QA notes with manual scenarios: docking panels, undocking viewport, restoring layout, multi-monitor if supported.
    - *Acceptance:* Tests pass; QA log stored under `docs/qa` referencing runtime observations.

## Phase 5 — Documentation & Follow-up
14. **Documentation update**
    - Add `docs/ui_docking.md` (or extend existing docs) describing docking usage, reset option, viewport undocking, known limitations.
    - Update README quick start to mention docking-enabled UI.
    - *Acceptance:* Documentation merged with screenshots or GIF references; reviewed for clarity.

15. **Track future enhancements**
    - Note potential stretch goals (saved layouts per asset, window presets, theme selection) in `.agent/FUTURE.md`.
    - *Acceptance:* Future ideas recorded for prioritisation.

## Completion Checklist
- Docking build compiles without warnings.
- UI panels operate independently in dockspace and viewport can undock.
- Camera controls respect UI focus.
- Tests and QA scenarios executed.
- Documentation and TODO entries updated before final commit.
