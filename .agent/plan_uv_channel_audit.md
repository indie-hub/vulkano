## UV Channel Audit Plan

### Goal
Determine whether the scrambled textures originate from incorrect UV channel selection, inverted coordinates, or other renderer-side issues by instrumenting the pipeline and collecting proof before applying fixes.

### Step-by-Step Plan
1. **UV Visualisation Shader**
   - Add a debug rendering path (toggle or shader variant) that outputs `vec3(U, V, 0)` for the current mesh.
   - Capture screenshots for both the mid-century chair and the cassette player with this shader active.
   - Expected outcome: coherent gradients if UVs are loaded correctly; nonsensical patterns if coordinates are mismatched.

2. **Assimp UV Channel Inspection**
   - During import, log the UV set index reported for each texture (`aiMaterial::GetTexture(..., uvIndex)` and `AI_MATKEY_UVWSRC_*`).
   - Record the indices for base colour, normal, and metallic/roughness textures for the problematic assets.
   - Determine whether any texture expects `TEXCOORD_1`, `TEXCOORD_2`, etc.

3. **Renderer Attribute Verification**
   - Ensure `SceneRenderer`’s vertex input layout includes all available UV sets from `scene::Vertex` (currently only `uv`).
   - If materials reference additional UV channels, track whether they are propagated through `ImportedMesh`, `MeshData`, and GPU buffers.

4. **Texture Flip Sanity Check**
   - Temporarily disable `aiProcess_FlipUVs` and the stb_image vertical flip on a test branch (one at a time).
   - Rebuild and render with the UV debug shader to see if gradients invert but remain consistent.
   - Revert the flip change after capturing results.

5. **Texture Sample Validation**
   - Use the existing Python helper to sample the atlases at logged UVs; compare values with the debug shader output.
   - This confirms whether we’re sampling the intended texels.

6. **Synthesis & Decision**
   - Summarise findings in `.agent/uv_sampling_notes.md` (or new notes file) to identify the root cause.
   - If UV channel mismatch is confirmed, extend the importer/renderer to support multiple UV sets.
   - If flips are incorrect, adjust post-process flags or texture-loader orientation accordingly.

### Acceptance Criteria
- UV debug shader screenshots show expected gradients once the correct channel/orientation is used.
- Assimp logging clarifies which UV set each texture needs.
- The renderer either handles multiple UV sets or applies the correct flip, eliminating the scrambled textures.
- Findings and chosen fix path documented in `.agent/` notes and reflected in TODO updates.
