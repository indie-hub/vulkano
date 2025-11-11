## Tangent-Space Investigation Notes

### Observations
- Imported meshes rely on Assimp tangents. In `src/app/asset_importer.cpp:307`, the loader assigns `vertex.tangent = mesh.HasTangentsAndBitangents() ? to_vec3(mesh.mTangents[v]) : glm::vec3 {1.0F, 0.0F, 0.0F};`, which falls back to a constant axis when tangents are absent. The mid-century lounge chair GLTF does not ship tangents, so every vertex receives the fallback vector.
- Procedural primitives call `scene::compute_tangent_frames` (`src/scene/mesh.cpp:140` etc.), but imported meshes never invoke that routine, leaving tangents/bitangent signs uncomputed.
- Vertex shader transforms tangents with the normal matrix and forwards a `fragBitangentSign` (`shaders/scene.vert:37-44`). Fragment shader reconstructs the TBN basis and samples the normal map (`shaders/scene.frag:70-100`). With constant tangents, the TBN basis collapses, producing the streaked highlights visible in the screenshot.

### Conclusion
The renderer's shader path assumes valid tangents, but the importer does not generate them when Assimp omits tangent data. We need to run `scene::compute_tangent_frames` (or equivalent) for imported meshes before uploading to the GPU so that normal-map sampling receives a stable basis.
