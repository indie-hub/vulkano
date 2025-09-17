# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning.

## [0.2.0] - 2025-09-17
### Added
- Per-primitive albedo (sRGB) and tangent-space normal map (UNORM) support.
- Procedural fallbacks when textures are not provided:
  - Checkerboard albedo (256×256, 8×8 squares), colors exposed via named constants.
  - Blue-noise-ish normal map (128×128), tangent-space packed RGBA8 with debiased high frequencies.
- Mipmapping for both albedo and normal textures (linear blit when supported).
- Two Vulkan samplers: albedo (linear + mipmaps + anisotropy if available, sRGB) and normal (linear + mipmaps, UNORM).
- ImGui controls per primitive: toggle Use Albedo Map / Use Normal Map, Normal Strength [0..2], UV tiling for plane, Base Color and Shininess.
- ImGui stats panel shows device name, swapchain extent, texture sizes, sampler configuration and anisotropy capability.
- Validation layers in Debug and VK_EXT_debug_utils object names + scope labels.
- End-to-end smoke test and resize stress test; unit tests for procedural textures.

### Changed
- Centralized configuration for procedural textures in `include/vulkano/config.hpp` and `src/config.cpp` to avoid magic numbers.
- Shader pipeline to support vertex tangents and TBN construction for normal mapping.

### Notes
- External textures can be provided via environment variables `VK_ALBEDO_PATH` and `VK_NORMAL_PATH` (loaded with stb_image, vertically flipped); otherwise procedurals are used.
- Vulkan version targeted is 1.3 for portability (MoltenVK on macOS). Headers may be newer; features beyond 1.3 are avoided.

## [0.1.0] - 2025-09-16
### Added
- Initial Vulkan + GLFW + GLM + ImGui application skeleton.
- Blinn-Phong lighting with plane, cube, and icosphere primitives.
- Basic UI with FPS, frame time, device name, and swapchain extent.

