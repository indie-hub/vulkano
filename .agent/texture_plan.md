# Texture & Normal Mapping Plan

## Goals
- Support per-primitive albedo (sRGB) and tangent-space normal textures with Vulkan samplers.
- Provide procedural checkerboard albedo and blue-noise normal fallbacks when files are absent.
- Extend shaders, descriptors, and push constants to apply textures with Blinn-Phong lighting and normal strength control.
- Expose ImGui controls per primitive for toggles, normal strength, colour, and stats, plus global sampler info.
- Maintain macOS compatibility and existing validation/debug behaviour.

## Data & Structures
- Extend `MeshVertex` with `glm::vec4 tangent` (xyz tangent, w = handedness) and ensure GPU binding/layout updated.
- Add `MaterialMapSettings` to `PrimitiveProperties`:
  - `bool useAlbedo` / `useNormal`
  - `float normalStrength`
  - `glm::vec2 uvTilingOverride` (used for plane, default {1,1})
  - `std::string albedoPath` / `normalPath`
- Expand `ScenePrimitive` to hold `MaterialTextures` (albedo + normal `TextureImage` handles, descriptor set reference).

## Vulkan Resources
- Add `TextureImage` RAII wrapper managing `VkImage`, memory, view, and mipmaps.
- Introduce `TextureSamplerSet` with two `VkSampler` objects (albedo sRGB + anisotropy, normal UNORM).
- Create `MaterialDescriptorSet` allocator using new descriptor set layout (set=1) with bindings:
  - binding0: combined image sampler (albedo)
  - binding1: combined image sampler (normal)
- Adjust pipeline layout to include global set (UBO + shadows) and material set per primitive.

## Texture Loading & Generation
- Integrate `stb_image` for external files (flip vertically, load RGBA, sRGB for albedo, UNORM for normal).
- Implement procedural generators:
  - Checkerboard 256x256, 8x8 squares, configurable colours.
  - Blue-noise-ish normal 128x128 using hash-based high-frequency noise and single high-pass (3x3 blur subtraction).
- Both outputs stored as RGBA8 byte arrays; upload with staging buffer; generate mipmaps via blit, handling format capabilities.
- Cache procedural textures globally to share across primitives.

## Rendering Flow
- During scene initialisation, create fallback textures and assign to primitives.
- Provide methods to load/replace textures per primitive (currently triggered via toggles/state changes without file IO; maintain toggles).
- Update command buffer recording to bind descriptor set 1 per primitive.
- Extend push constants to include `materialFlags` (useAlbedo/useNormal/normalStrength etc.).
- Update shaders:
  - Vertex: pass tangent, bitangent, UV; build TBN matrix.
  - Fragment: sample albedo (sRGB) and normal map (UNORM), apply normal strength, fallback to default when toggled off.
  - Maintain shadow sampling as is.

## UI Updates
- Scene panel per primitive: toggles for albedo/normal usage, `Normal Strength` slider [0,2], texture status labels (fallback/custom).
- For plane primitives, expose UV tiling sliders affecting mesh rebuild.
- Global panel: display sampler settings (filter, anisotropy), currently bound texture names.

## Testing
- Unit tests:
  - Validate procedural texture data properties (checkerboard pattern, normal vector normalization).
  - Verify tangent generation for primitives (normals orthogonal, tangents normalized).
- Integration test updates to confirm descriptor counts and toggles propagate (mock/inspect struct state).

## Build & Packaging
- Add `third_party/stb/stb_image.h` and integrate into CMake include dirs.
- Ensure shaders recompiled (SPIR-V) into `shaders/*.spv` via existing workflow.
- Update documentation (README or dedicated doc snippet) summarising texture feature.

## Risks & Considerations
- Mipmap generation requires format support; add capability checks and fallbacks.
- Normal strength scaling ensures >0 results clamped.
- Manage resource cleanup (descriptor sets, images) during swapchain recreation and app teardown.

## Step 3 Notes (Descriptors & Rendering)
- Extend `PrimitiveProperties` with map toggles (`useAlbedoMap`, `useNormalMap`), `normalStrength`, optional file paths.
- Update `MeshVertex` to include tangent (vec4). Regenerate tangents in primitives.
- Introduce `MaterialTextureSet` struct on GPU side: holds `TextureImage` handles for albedo/normal plus descriptor set + status enum (Fallback / Custom / Missing).
- Create global `TextureLibrary` in `VulkanApplication` to own fallback textures, samplers, descriptor layouts/pools.
- Rebuild descriptor layouts:
  - Set 0: global (UBO + shadow).
  - Set 1: material (albedo sampler, normal sampler).
- Allocate descriptor sets per primitive; update whenever textures change or toggles require fallback.
- Modify command buffer bind to use both descriptor sets.
- Extend push constants with flags/normal strength and base color.
- Update shaders for tangent-space normal mapping with TBN matrix and texture sampling fallback.
- Update scene init to assign fallback textures and compute tangents for meshes.
