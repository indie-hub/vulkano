# Per-Light Shadow Controls

This guide explains how to configure and validate shadow casting for individual
lights in the Vulkano sample renderer.

## UI Workflow

- Open the **Lighting** panel (hotkey `L` or via the ImGui menu bar).
- Each light in the scene is listed as `Light <index>` with editable
  properties.
- Directional lights expose a `Casts Shadow` checkbox. Toggle it to enable or
  disable contribution to the shadow map.
- Point lights do not currently support shadow casting; the checkbox is hidden
  and the renderer never uploads their shadow flags.

Changing the checkbox marks the lighting data as dirty. On the next frame the
application rebuilds the GPU light buffer, updates descriptors, and refreshes
debug gizmos before drawing.

## Renderer Behaviour

- At most one directional light may cast shadows at a time. The renderer scans
  the light registry for the first directional light with `castsShadow == true`
  and treats it as the primary shadow caster.
- If no directional light is flagged, the shadow pass is skipped entirely and
  shading falls back to shadowless lighting.
- The GPU light buffer packs the flag into `LightGpu::shadowParams.x`; the
  fragment shader multiplies lighting by a shadow factor only when the flag is
  set.

## Debugging

- Enable **Debug Shadow Map** in the Lighting panel to render the shadow depth
  texture in the top-right corner. This helps verify the light view frustum and
  PCF settings.
- Adjust **Depth Bias** and **PCF Radius** sliders while watching the debug
  view to tune bias artifacts and penumbra size after changing shadow-casting
  lights.

## Troubleshooting

- If shadows vanish after disabling a light, ensure that at least one
  directional light still has `Casts Shadow` enabled.
- Shadow acne usually indicates the bias is too low for the current light
  angle; increase **Depth Bias** slightly.
- Peter-panning (objects detaching from shadows) suggests the bias is too high;
  reduce **Depth Bias** until contact is restored.
- Excessively soft shadows may mean **PCF Radius** is too large; dial it back
  to sharpen edges.
