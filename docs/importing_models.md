# Importing External Models

The renderer can load external meshes through the Assimp importer. Set the
`VULKANO_MODEL` environment variable to an asset path before launching the
sample app.

```bash
VULKANO_MODEL=/absolute/path/to/model.gltf ./bin/vulkano_renderer
```

## Supported Formats
Assimp 6.0.2 is configured with the default importer set, which covers common
formats such as OBJ, FBX, glTF, COLLADA, PLY, and STL. Animations and skinning
data are ignored for now; only static meshes and materials are converted.

## Workflow
1. Ensure textures referenced by the model are accessible via relative paths.
2. Launch the renderer with `VULKANO_MODEL` pointing to the asset file.
3. The importer registers new materials, uploads meshes, and appends them to the
   existing scene.
4. Material buffers and textures are rebuilt automatically.

If loading fails, the application logs an error with the Assimp message.

## Descriptor Limits
The renderer currently reserves 12 material texture slots per frame, leaving
room for SSAO and shadow samplers on macOS (16 total sampler descriptors per
stage). Imported materials without textures use shared fallback descriptors to
stay within this limit.
