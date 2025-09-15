# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning.

## [Unreleased]
- SSAO pass fine-tuning and blur radius options.
- Additional end-to-end tests and CI matrix.
- Expanded documentation for shader stages and resource bindings.
- Added `docs/SHADERS.md` and expanded README with platform notes.

## [0.1.0] - 2025-09-15
### Added
- Minimal Vulkan application using C++20 and GLFW opening a window.
- Runtime‑subdividable icosphere CPU mesh generator (0–6 subdivisions) with UVs and TBN.
- Per‑pixel Phong shading with tangent‑space normal mapping.
- Dear ImGui integration (GLFW + Vulkan backends) with controls for subdivisions, light, material, SSAO.
- Initial SSAO pipeline scaffolding (G‑buffer outputs, kernel/params, blur and compose hooks).
- Shader build step (glslc to SPIR‑V if available) and robust shader file lookup at runtime.
- Cross‑platform notes for macOS/MoltenVK and validation layers toggling.
- Unit tests for icosphere vertex/index counts (Catch2).

### Changed
- CMake structure with explicit file lists; emits binaries to `bin/`.

### Fixed
- Robust swapchain resize handling and resource teardown ordering.

### Documentation
- Root README with build/run instructions and controls overview.

### Tooling
- clang‑format and clang‑tidy baseline configurations.

<!-- Links -->
[Unreleased]: https://github.com/indie-hub/vulkano/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/indie-hub/vulkano/releases/tag/v0.1.0
