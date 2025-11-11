# Plan: Advanced ImGui Layout Enhancements

## Objective
Refine the docking UI to provide a more flexible workflow. Deliver a toolkit of
layout presets, an independent viewport window, and polished panel placement so
power users can customise the workspace quickly.

## Phase 0 — Baseline Assessment
1. **Capture Current Layout State**
   - Record which panels exist, their default dock locations, and how the viewport
     is embedded.
   - Note any layout persistence behaviours (e.g., `.ini` location).
   - *Acceptance:* Short summary appended to this plan describing today’s layout
     and listing all panels.

## Phase 1 — Viewport Window Extraction
2. **Decouple Scene Viewport**
   - Render the swapchain output inside its own ImGui window (e.g., `Viewport`).
   - Ensure the renderer respects the window’s size for projection and updates
     the swapchain when resized.
   - *Acceptance:* Manual test allows undocking/dragging the viewport; rendering
     remains correct after resize.

3. **Camera Focus Handling**
   - Route input so camera controls activate only when the viewport window is
     focused/hovered.
   - Support cursor locking/releasing for FPS mode when the viewport is active.
   - *Acceptance:* QA checklist entry verifying camera interaction behaves the
     same before and after undocking.

## Phase 2 — Preset Layouts & Menu
4. **Implement Layout Presets**
   - Add helper functions to build multiple preset docks (e.g., `Authoring`,
     `QA`, `Minimal`).
   - Provide a menu/toolbar button to reset to each preset (clearing and
     rebuilding the dock tree).
   - *Acceptance:* Selecting a preset rearranges panels deterministically; layout
     persists across restarts.

5. **Compact Sidebar Configuration**
   - Create a preset where Scene Graph + inspector stack in a single vertical
     dock (tabs or top/bottom split).
   - *Acceptance:* Preset recorded with screenshot/QA note showing compact view.

## Phase 3 — Panel Refinements
6. **Context-Sensitive Inspector**
   - Pair Scene Graph with an inspector panel showing details for the selected
     node (materials, transforms).
   - Use docking splits so the inspector panel aligns directly below/next to the
     hierarchy.
   - *Acceptance:* Selecting a node updates inspector contents; default layout
     places inspector with Scene Graph.

7. **Consolidated Controls Tabs**
   - Group Lighting/SSAO/Materials into a tab bar dock in the right or bottom
     region.
   - Add icons or colored headers to differentiate panel categories.
   - *Acceptance:* Tabbed panel renders correctly; icons/colors visible and do
     not break dark theme.

8. **Toolbar Strip**
   - Introduce a narrow, non-dockable toolbar window with shortcuts (reset
     camera, reload model, toggle shadows).
   - Ensure it anchors to an edge without interfering with dockspace.
   - *Acceptance:* Toolbar appears every run, respects theme, buttons invoke
     actions.

## Phase 4 — Persistence & Profiles
9. **Layout Profile Serialization**
   - Store the last chosen preset in an app config (or ImGui settings) so the
     renderer restores the same profile on restart.
   - *Acceptance:* Restarting after switching presets restores the selected one
     without manual intervention.

10. **Reset & Cleanup Utilities**
    - Provide commands to clear ImGui settings (`imgui.ini`) and rebuild default
      layouts from within the UI.
    - *Acceptance:* Triggering “Reset Layout” returns to default preset without
      deleting files manually.

## Phase 5 — Testing & Documentation
11. **Automated Coverage**
    - Extend tests to assert viewport focus toggling logic and preset helper
      invariants (e.g., color attachment count remains unchanged).
    - *Acceptance:* New tests pass on CI; failures would surface when preset
      builders diverge.

12. **QA Session**
    - Document manual docking scenarios (dragging viewport, switching presets,
      toolbar usage) with screenshots.
    - *Acceptance:* QA notes stored in `docs/qa` referencing dates and outcomes.

13. **Documentation Update**
    - Update `docs/ui_docking.md` with instructions for presets, viewport
      undocking, and toolbar actions.
    - *Acceptance:* Documentation merged, mentioning new features with images or
      GIF references.

## Phase 6 — Defer & Track Extras
14. **Future Enhancements**
    - Log stretch goals (multi-viewport support, layout export/import) in
      `.agent/FUTURE.md`.
    - *Acceptance:* Future tasks recorded once current scope completes.

## Completion Checklist
- Viewport window dockable/floating, camera focus respected.
- Preset menu operational with at least two distinct layouts.
- Inspector and control panels organised per design.
- Toolbar provides quick actions.
- Tests and docs updated; QA signoff captured.
