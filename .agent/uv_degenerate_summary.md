## Degenerate UV Triangle Summary

- Command: `bin/dump_mesh_uvs --report-degenerate-uvs --uv-area-threshold 1e-4 .agent/mid_century_lounge_chair_4k-2/mid_century_lounge_chair_4k.gltf`
- Output stored in `.agent/uv_degenerate_triangles.log`.
- Findings:
  - 3,788 out of 4,203 indexed triangles exhibit UV area ≤ 1.0e-4, indicating the chair mesh packs most faces into extremely thin UV strips.
  - The earliest clusters correspond to strips along the seat perimeter (indices 0–35), matching the visible stretched patches.
  - UV coordinates for these triangles hold nearly constant `u` or `v` values (e.g., `[0.6404, 0.6471]`), implying the atlas maps multiple leather panels into narrow slivers.
  - Positions show the issue spans the seat ring rather than being confined to a single vertex, pointing to baked UVs in the source asset rather than runtime corruption.
- Next steps:
  1. Determine whether the glTF defines multiple primitives/materials for these strips that we merged during import.
  2. Consider splitting the mesh by material or regenerating UVs for the problematic panels when feasible.
