# Vulkano Codex

Vulkano Codex is a minimal Vulkan 1.3+/GLFW sample built with C++20 that renders a white triangle on a black background and overlays runtime statistics with Dear ImGui. The application targets macOS via MoltenVK and follows SOLID/Clean Code guidelines with a RAII-oriented architecture.

## Features
- Vulkan instance/device creation with validation layers and `VK_EXT_debug_utils` markers in debug builds.
- Swapchain management with automatic recreation on window resize.
- Graphics pipeline that reads SPIR-V shader binaries from disk and renders a white triangle via push constants.
- Dear ImGui overlay displaying FPS, frame time (ms), physical device name, and swapchain extent.
- Unit and integration tests built on Catch2.

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
A resizable GLFW window should appear with the triangle and ImGui stats overlay. Validation output is printed to stderr when enabled.

## Tests
Configure with testing enabled (default) and run Catch2-based tests:
```bash
ctest --test-dir build
```
The integration test attempts to initialise the Vulkan application and will self-skip with a warning if the runtime environment lacks a Vulkan-capable device.
