# Docking Workflow

The renderer ships with the Dear ImGui docking branch (v1.92.3). Docking is
always enabled and a fullscreen dockspace is created on startup. Panels can be
rearranged by dragging their tab headers, stacked to create tab bars, or undocked
into floating windows. Key points:

- **Default Layout:**
  - `Scene Graph` on the left for hierarchy edits.
  - `Materials` in the centre dock.
  - `Lighting` on the right for inspector controls.
  - `SSAO` at the bottom for post-processing tuning.
  - The `Diagnostics` overlay floats independently of the dockspace.
- **Persistence:** Layout customizations persist in `imgui.ini`. Delete that file
  (or use ImGui's "Reset Layout" button when available) to restore defaults.
- **Viewport:** The scene viewport currently remains embedded in the main window;
  future work may promote it to its own dockable window.
- **Multi-monitor:** Additional ImGui viewports are disabled for now. Docking is
  limited to the main window.

## Tips

1. Drag panel headers to reposition. The central dock node highlights valid
   targets.
2. Float a panel to make it independent, then redock by dragging the title bar
   back to an edge highlight.
3. Use the window context menu (right-click the tab) to split or close docks.
4. Delete `imgui.ini` between runs to capture clean screenshots of the default
   layout.

## Known Limitations

- The viewport is not yet separated from the central dock.
- Multi-viewport support remains disabled to avoid platform complications on
  macOS.
- Dock layout reset is manual (clear `imgui.ini`).

## QA Checklist

Manual checks performed on macOS 14.6.1 (Apple M2 Pro):

- [x] Panels dock according to default layout on first run.
- [x] Scene Graph, Materials, Lighting, and SSAO windows can be rearranged and
      persist across restarts.
- [x] Undocking and redocking windows leaves rendering and camera controls
      unaffected.
- [x] Diagnostics overlay remains independent of the dockspace.
- [x] No Vulkan validation errors emitted while interacting with docked panels.

