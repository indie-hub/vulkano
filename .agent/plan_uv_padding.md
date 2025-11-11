## Leather UV Padding Mitigation Plan

### Goal
Mitigate the leather/wood bleed caused by tightly packed UV strips in the chair’s atlas without re-authoring the source asset.

### Constraints
- Keep current geometry and texture files intact; any fix must be applied at load time.
- Follow repository standards (AGENTS.md): C++20, angle-bracket includes, brace initialisers, const correctness, noexcept where appropriate.
- Update `.agent/TODO.md` as each step completes and commit before moving to the next.
- Ensure all builds/tests pass; document behaviour changes and update changelog.
- Avoid changing global sampler state unless scoped to the affected materials.

### Step-by-Step Plan
1. **Quantify Bleed Risk**
   - Extend the UV diagnostic to bucket triangles by UV area and report per-material statistics.
   - Capture “before” screenshots using a UV debug shader to visualise the problematic strips.
2. **Prototype Texture Padding**
   - Add a CPU-side helper that expands suspect textures by duplicating border texels (configurable padding width).
   - Trigger the helper for textures whose associated mesh exceeds a UV degeneracy threshold.
3. **Integrate Into Loader**
   - Invoke the padding helper in the texture-loading path before GPU upload.
   - Record padded texture dimensions and adjust UV scale/offset if necessary so sampling still hits the intended texels.
4. **Runtime Controls**
   - Expose a material flag or debug toggle to enable/disable padding, aiding visual validation.
   - Log when padding is applied to help QA verify coverage.
5. **Validation**
   - Re-run the diagnostic utility and UV debug shader to confirm strips now sample leather pixels only.
   - Add unit tests for the padding helper (e.g., synthetic atlas with tight UVs) and ensure existing tests pass.
6. **Documentation & Changelog**
   - Update `docs/importing_models.md` with guidance on automatic padding and limitations.
   - Add a changelog entry under “Fixed”.
   - Queue any follow-up tasks in `.agent/TODO.md`.

### Acceptance Criteria
- UV debug view shows continuous gradients with no cross-material bleed.
- Runtime rendering of the lounge chair no longer exhibits wood patches on leather sections.
- New padding tests pass, and existing test suite remains green.
- Documentation and changelog reflect the change.
