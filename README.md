# Vulkano Codex — Vulkan + GLFW + ImGui (C++20)

Small, production‑grade sample that opens a GLFW window and renders a lit scene using Vulkan + GLFW + Dear ImGui. The scene contains a ground plane, a cube, and an icosphere with depth testing and per‑pixel Blinn‑Phong shading (ambient + diffuse + specular). It overlays runtime stats via Dear ImGui: FPS, frame time (ms), Vulkan device name, and swapchain extent. Built with C++20, CMake, and adheres to SOLID/Clean Code guidelines.

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
- `bin/shaders/mesh.vert.spv`, `bin/shaders/mesh.frag.spv` (and legacy triangle shaders)
- `bin/imgui.ini` (created at first run)

Runtime bundle (relocatable):
- `cmake --build build --target dist`
- Outputs to `bin/dist/` containing:
  - `bin/dist/vulkano_app`
  - `bin/dist/shaders/` (SPIR-V)
  - `bin/dist/imgui.ini` (created on first run)
  - You can zip/copy `bin/dist/` to run elsewhere without the build tree.

## Run

From repo root:
- `./bin/vulkano_app`

Environment options:
- `VK_APP_AUTOCLOSE_MS=800` — auto‑close window after N milliseconds (handy for CI/smoke tests)
- `VK_SHADER_DIR=./bin/shaders` — override shader directory discovery

Controls (ImGui):
- Stats: FPS, frame time, device name, swapchain extent
- Light: position (XYZ), intensity
- Per primitive (Plane, Cube, Icosphere): position, rotation (radians), scale, base color, shininess
- Icosphere: optional subdivisions slider (rebuilds CPU mesh + re‑uploads GPU buffers)

Acceptance criteria (what you should see):
- Window opens on a black clear background
- Ground plane at y=0 with a cube (left) and icosphere (right) above it
- Depth testing correct (no visual popping or ordering issues)
- ImGui overlay shows FPS, frame time, device name, swapchain extent
- Resizing works without validation errors; scene and UI redraw correctly

Scene setup (defaults):
- Plane: 10×10 at y=0, grey base color
- Cube: scale 0.5, position (-1, 0.5, 0), warm orange
- Icosphere: scale 0.5, position (1, 0.5, 0), subdivisions=2, light blue

## macOS Notes (MoltenVK)

Vulkan on macOS uses MoltenVK (Vulkan Portability over Metal). Install the Vulkan SDK (includes MoltenVK) from LunarG or via package managers.

Tips:
- Ensure `glfwGetRequiredInstanceExtensions` returns Metal surface extensions; this project uses them via GLFW.
- Instance requests Vulkan 1.3 as baseline to match MoltenVK support; headers may be newer. The app avoids features beyond Vulkan 1.3 to remain portable on macOS.

## Features
- C++20, RAII, no magic numbers in logic (named constants/config)
- Vulkan instance/device/swapchain/render‑pass/framebuffers/cmd buffers/sync
- One graphics pipeline with depth testing; vertex format: position/normal/uv
- Blinn‑Phong shading (ambient/diffuse/specular) with per‑primitive material via push constants; global camera/light via UBO
- Depth‑correct rendering of plane, cube, icosphere; CPU geometry uploaded once using staging buffers
- Resize handling with full swapchain recreation
- ImGui overlay (imgui_impl_glfw + imgui_impl_vulkan)
- Validation layers in Debug; VK_EXT_debug_utils names + markers

## Tests

Unit tests (Catch2) cover FPS smoothing. A smoke test launches the app headlessly with autoclose:
- `ctest --test-dir build --output-on-failure`

## Troubleshooting
- Missing shaders: ensure Vulkan SDK tools exist (`glslc` or `glslangValidator`) so CMake can compile GLSL into SPIR‑V.
- No Vulkan loader found: on macOS, install Vulkan SDK; the build will link against MoltenVK if available.
