# Vulkano Codex — Vulkan + GLFW + ImGui (C++20)

Small, production‑grade sample that opens a GLFW window and renders a single white triangle on a black background using Vulkan. It overlays runtime stats via Dear ImGui: FPS, frame time (ms), Vulkan device name, and swapchain extent. Built with C++20, CMake, and adheres to SOLID/Clean Code guidelines.

This project targets desktop platforms and works on macOS via MoltenVK (Vulkan Portability). Validation layers are enabled in Debug builds and VK_EXT_debug_utils markers/names are applied.

## Build

Prereqs:
- CMake ≥ 3.25
- C++20 compiler (Clang/GCC/MSVC)
- Vulkan SDK (for `glslc` or `glslangValidator` to compile shaders)
- Git and internet access (FetchContent fetches GLFW, GLM, ImGui if not present)

Commands:
- Debug
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build -j`
- Release
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build -j`

Artifacts are placed in `bin/`:
- `bin/vulkano_app`
- `bin/shaders/triangle.vert.spv`, `bin/shaders/triangle.frag.spv`
- `bin/imgui.ini` (created at first run)

## Run

From repo root:
- `./bin/vulkano_app`

Environment options:
- `VK_APP_AUTOCLOSE_MS=800` — auto‑close window after N milliseconds (handy for CI/smoke tests)
- `VK_SHADER_DIR=./bin/shaders` — override shader directory discovery

## macOS Notes (MoltenVK)

Vulkan on macOS uses MoltenVK (Vulkan Portability over Metal). Install the Vulkan SDK (includes MoltenVK) from LunarG or via package managers.

Tips:
- Ensure `glfwGetRequiredInstanceExtensions` returns Metal surface extensions; this project uses them via GLFW.
- Instance requests Vulkan 1.3 as baseline to match MoltenVK support; headers may be newer. The app avoids features beyond Vulkan 1.3 to remain portable on macOS.

## Features
- C++20, RAII, no magic numbers in logic (named constants/config)
- Vulkan instance/device/swapchain/render‑pass/framebuffers/cmd buffers/sync
- Vertex buffer with 3 GLM vertices (NDC)
- Pipeline with push‑constant color (fragment shader is not hardcoded)
- Resize handling with full swapchain recreation
- ImGui overlay (imgui_impl_glfw + imgui_impl_vulkan)
- Validation layers in Debug; debug utils names + labels

## Tests

Unit tests (Catch2) cover FPS smoothing. A smoke test launches the app headlessly with autoclose:
- `ctest --test-dir build --output-on-failure`

## Troubleshooting
- Missing shaders: ensure Vulkan SDK tools exist (`glslc` or `glslangValidator`) so CMake can compile GLSL into SPIR‑V.
- No Vulkan loader found: on macOS, install Vulkan SDK; the build will link against MoltenVK if available.
