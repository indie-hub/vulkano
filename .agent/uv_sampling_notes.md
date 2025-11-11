## UV Sampling Notes

- `TextureImage::create` (`src/app/texture_image.cpp:283`) builds samplers with:
  - `magFilter = VK_FILTER_LINEAR`
  - `minFilter = VK_FILTER_LINEAR`
  - `mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR`
  - `addressModeU/V/W = VK_SAMPLER_ADDRESS_MODE_REPEAT`
  - `anisotropyEnable = VK_FALSE`, `maxAnisotropy = 1.0`
  - `unnormalizedCoordinates = VK_FALSE`
  - LOD bias defaults (no explicit bias set).
- All textures, including the lounge chair atlas, use this single configuration; there is no per-material variation.
- With tightly packed UV islands, linear + trilinear filtering blends across adjacent texels, which matches the bleed observed on the leather strips.
- Environment overrides (`VULKANO_TEX_FILTER=nearest`, `VULKANO_TEX_WRAP=clamp`) are now available to flip the sampler at runtime for quick experiments.
- Forcing point sampling (nearest/nearest) removes the wood tone but produces severe aliasing; clamping has no visible effect, confirming filtering—not wrapping—is responsible.
- Setting `VULKANO_DEBUG_UV=1` now forces the fragment shader to output `vec3(U,V,0)`, making it easy to inspect UV orientation in the renderer screenshots.

Next: experiment with point filtering and clamp-to-edge sampling to measure the visual impact.
