## UV Flip Inspection Notes

- Tool: `bin/dump_mesh_uvs --compare-uv-flip .agent/mid_century_lounge_chair_4k-2/mid_century_lounge_chair_4k.gltf`
- Result snapshot:
  - Without `aiProcess_FlipUVs`: `v` range ≈ `[0.003010, 0.996795]`
  - With `aiProcess_FlipUVs`: `v` range ≈ `[0.003205, 0.996990]`
- Observation: Assimp's flip raises the `v` values by ~0.000195, keeping the span symmetric around 0.5. This matches our importer output, confirming the flip is active.
- Both configurations report a single mesh (4203 vertices) mapped to material index `0`, so the seam is unlikely caused by unintended material splits.
- Next focus areas:
  1. Analyse UV continuity per face subset to see whether certain regions reuse the same portion of the atlas.
  2. Review whether the leather material expects additional texture transforms from the source glTF (none detected so far).
