Baseline Shadow Notes
=====================

Commit 68b166b (current HEAD) computes the light-space bounding box by projecting
each cascade frustum slice into the light view and taking component-wise min/max.
It adds a depth padding of range * 0.5 + radius and leaves the XY bounds tightly
bound to the oriented box. That tight fit causes the following observable issues:

- Any camera rotation changes the projected min/max, so the orthographic volume
  slides with the camera even when the scene is static, producing shimmering.
- The XY bounds are not snapped to texel units, so floating-point drift in the
  view-projection matrix further exacerbates shimmering.
- Bias values remain constant per cascade and are not derived from the cascade
  size, making acne prominent on large cascades and insufficient coverage on
  close ones.

These observations come from reviewing `compute_cascaded_shadow_data` in
`src/cascaded_shadow_map.cpp` (lines 160-238) and the bias usage in
`src/application.cpp` (lines 1640-1705).
