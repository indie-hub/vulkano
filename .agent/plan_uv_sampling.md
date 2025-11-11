## Leather UV Sampling Investigation Plan

### Goal
Determine whether the leather/wood bleed originates from renderer sampling configuration (filtering, LOD bias, anisotropy, wrap modes) rather than data corruption, and implement the minimal change needed to remove the artefact.

### Step-by-Step Plan
1. **Capture Baseline Sampler State**
   - Inspect `TextureImage::create_sampler` and descriptor bindings to record the current min/mag filter, mip filter, wrap modes, and anisotropy settings.
   - Dump the sampler configuration at runtime (debug log) for the leather material.
2. **Visual Diagnostics**
   - Add a temporary material override that forces point filtering.
   - Compare renders (point vs. linear vs. trilinear) and record screenshots to see whether the bleed disappears when filtering is tightened.
3. **Sampler Experiments**
   - Toggle `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`, lowered mip LOD bias, and anisotropy off/on to observe impact.
   - Run the UV debug shader in each configuration to correlate with visual differences.
4. **Choose Minimal Fix**
   - If a specific sampler configuration resolves the bleed without major side-effects, apply it selectively (e.g., for sRGB colour textures flagged by diagnostics).
   - Otherwise, fall back to padding plan (.agent/plan_uv_padding.md).
5. **Validation & Cleanup**
   - Rebuild, run tests, capture “after” screenshots, and revert temporary diagnostics.
   - Document sampler adjustments in `docs/importing_models.md` and update `CHANGELOG.md`.

### Acceptance Criteria
- Evidence (logs + screenshots) showing which sampler setting influences the bleed.
- Leather panels render cleanly with the chosen sampler configuration.
- No regressions in existing automated tests.
