# Docking Workflow

The renderer ships with Dear ImGui `v1.92.3-docking`. Docking is always enabled
and a fullscreen dockspace is created on startup. Panels can be rearranged by
dragging their tab headers, stacked to create tab bars, or undocked into
floating windows. Key points:

- **Viewport Window:** `Viewport` is a dockable window backed by the offscreen
  render target. The Vulkan scene renders into this panel (not the OS
  background); hovering or focusing it enables camera input, and undocking lets
  you move the scene to a second monitor.
- **Inspector Pairing:** `Scene Graph` docks alongside an `Inspector` panel that
  exposes transform editing and geometry details for the selected node.
- **Visibility Controls:** Eye checkboxes in the Scene Graph toggle node
  visibility; the Inspector mirrors the toggle for the selected node. Hidden
  nodes are greyed out and omitted from rendering until re-enabled.
- **Control Tabs:** `Lighting`, `Materials`, and `SSAO` share the same dock node
  by default, appearing as a tab bar on the right/bottom depending on the preset.
- **Toolbar Strip:** A floating toolbar at the top of the viewport provides
  quick actions (reset camera, toggle shadows, save layout).
- **Layout Presets:** Use the **Layout** menu (top-left) to flip between
  `Authoring`, `Compact`, and `QA` presets or reset the current layout. Preset
  selections persist in `imgui_layout_profile.cfg`.
- **Persistence:** Dock positions persist in `imgui.ini`. Choosing "Save Layout"
  writes both the preset marker and ImGui configuration on demand.
- **Multi-monitor:** Additional ImGui viewports remain disabled; docking is
  limited to the main window.

## Tips

1. Drag panel headers to reposition. The central dock node highlights valid
   targets.
2. Float a panel to make it independent, then redock by dragging the title bar
   back to an edge highlight.
3. Use the **Layout** menu to apply presets or reset the current configuration.
4. Delete `imgui.ini` (and optionally `imgui_layout_profile.cfg`) between runs to
   capture clean screenshots of the default layout.
5. Hover the viewport to regain camera control; the toolbar buttons do not steal
   focus from the dockspace.

## Validation & Troubleshooting

- Run a single-frame sanity check without validation:
  ```bash
  VULKANO_MAX_FRAMES=1 ./bin/vulkano_renderer
  ```
- Confirm the docking workflow is validation-clean:
  ```bash
  VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \\
  VULKANO_MAX_FRAMES=1 ./bin/vulkano_renderer
  ```
- Delete `imgui.ini` (and optionally `imgui_layout_profile.cfg`) if the layout
  becomes corrupted; presets will restore the default arrangement on next run.

## Known Limitations

- Swapchain extent remains tied to the OS window. The viewport respects its
  docked size/aspect ratio, but rendering is still single-window.
- Multi-viewport support stays disabled to avoid macOS platform quirks.
- Toolbar actions currently cover camera reset, shadow toggle, and manual
  layout save only.

## QA Checklist

Manual checks performed on macOS 14.6.1 (Apple M2 Pro):

- [x] Panels dock according to the `Authoring` preset on first run.
- [x] Switching presets via the Layout menu rebuilds the dock tree instantly and
      persists across restarts.
- [x] Viewport can be undocked, moved to a second monitor, and redocked without
      losing camera control.
- [x] Toolbar buttons reset the camera and toggle shadows as expected.
- [x] Scene Graph selection updates the Inspector content in real time.
- [x] No Vulkan validation errors emitted while interacting with docked panels
      (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VULKANO_MAX_FRAMES=1`).
