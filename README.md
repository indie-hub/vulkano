# Vulkano Codex

Vulkano Codex is a Vulkan 1.3+/GLFW sample built with C++20 that renders a small lit scene using a plane, cube, and icosphere primitives. The application targets macOS via MoltenVK and follows SOLID/Clean Code guidelines with a RAII-oriented architecture while exposing live scene controls through Dear ImGui.

## Features
- Vulkan instance/device creation with validation layers and `VK_EXT_debug_utils` markers in debug builds.
- Swapchain management with automatic recreation on window resize plus depth buffering.
- Blinn-Phong lighting pipeline with per-primitive push constants (colour, shininess, ambient/specular terms, texture toggles) and a uniform-buffer backed camera/light.
- Albedo + tangent-space normal mapping per primitive with independent albedo/normal toggles, adjustable normal strength, and selectable procedural styles (random noise or brushed metal) when no textures are supplied.
- Cascaded shadow mapping with adjustable split distribution, depth bias, and PCF filtering to mitigate acne and soften edges across the scene.
- CPU-generated plane, cube, and icosphere meshes uploaded once to device-local buffers.
- Dear ImGui overlays for runtime stats and scene controls (light position/intensity, primitive transforms/materials, icosphere subdivisions).
- Unit and integration tests built on Catch2 covering configuration, mesh generation, and scene defaults.

## Prerequisites
- CMake 3.25 or newer.
- A C++20 toolchain (Clang 15+/GCC 12+/MSVC 17.4+).
- Vulkan SDK 1.3+ (MoltenVK on macOS) with `glslc` available on the `PATH`.
- Git for fetching third-party dependencies through `FetchContent`.

## Build Instructions
```bash
cmake -S . -B build
cmake --build build
```
The executables and required shader binaries are copied to `bin/`.

## Running
After a successful build, launch the sample from the project root:
```bash
./bin/vulkano_codex
```
A resizable GLFW window opens showing the lit plane, cube, and icosphere above a black background. Validation output is printed to stderr when enabled.

### Scene Controls
- `Runtime Stats` shows FPS, frame time, device name, and swapchain extent.
- `Scene Controls` allows adjusting light position/intensity and each primitive's position, rotation, scale, base colour, shininess, ambient, and specular strengths. Textured primitives can toggle albedo and normal maps independently, select the procedural normal style, and tune normal strength. The icosphere also supports rebuilding with subdivisions 0–5.
- The `Shadows` section exposes cascade count, split distribution (`λ`), resolution, depth bias, and PCF radius tweaks so you can balance quality, shimmering, and performance.

## Tests
Configure with testing enabled (default) and run Catch2-based tests:
```bash
ctest --test-dir build
```
The integration test attempts to initialise the Vulkan application and will self-skip with a warning if the runtime environment lacks a Vulkan-capable device.
