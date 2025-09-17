- [x] Prepare shader + descriptor pipeline for textures (albedo sRGB, normal UNORM):
  - Add combined image samplers to descriptor set layout, create samplers, upload procedural textures with mipmaps, and bind in descriptors. Update mesh shaders to sample albedo and apply tangent-space normal mapping with default parameters. Ensure build succeeds on macOS.

- [ ] Add per-primitive material controls:
  - Extend Material with useAlbedo, useNormal, normalStrength. Push constants carry toggles and strength; UI exposes toggles, strength slider [0..2], plane UV tiling; show bound textures and sampler caps.

- [ ] Optional external texture loading via stb_image:
  - Implement loaders for albedo (sRGB) and normal (UNORM) with vertical flip option and mipmap generation. Wire minimal UI buttons (no file dialog; toggles only if unavailable).

- [ ] Robustness and polish:
  - Device feature checks for blit/mipmap fallbacks, validation markers/names, tidy-up on swapchain recreation, and brief docs in README.
