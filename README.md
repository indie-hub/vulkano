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

## Camera Controls (FPS)

- Mouse look: Hold Right Mouse Button to hide/lock cursor and look around.
- Movement: W/A/S/D to move, Space = up, Left Ctrl = down.
- Sprint: Hold Shift to increase movement speed.
- FOV: Mouse wheel adjusts field‑of‑view when ImGui is not using the wheel.
- ImGui gating: When ImGui wants the mouse/keyboard, camera input is ignored.
- Lock: The “Camera” panel has a “Lock camera” checkbox to disable look/move and restore normal cursor.

Notes:
- Cursor lock toggles only while RMB is pressed; released restores normal cursor.
- Keyboard movement works when RMB is pressed, or when ImGui is not capturing the keyboard.

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

## Textures & Normal Mapping

- Per‑primitive albedo (sRGB) and tangent‑space normal map (UNORM) with mipmaps.
- Procedural fallbacks when no external files are provided:
  - Albedo: checkerboard 256×256, 8×8 squares, colors: off‑white (0.92) and dark‑grey (0.12).
  - Normal: blue‑noise‑ish 128×128 detail normal; tangent‑space packed in RGBA8.
- External textures (optional) via environment variables (stb_image loads, flipped vertically):
  - `VK_ALBEDO_PATH=/path/to/albedo.png`
  - `VK_NORMAL_PATH=/path/to/normal.png`
- Samplers:
  - Albedo: linear filtering, mipmapped, sRGB sampling, anisotropy if supported.
  - Normal: linear filtering, mipmapped, UNORM sampling, no sRGB.
- UI controls (per primitive): Use Albedo Map, Use Normal Map, Normal Strength [0..2], Plane UV tiling, Base Color and Shininess.
- UI (global, read‑only): device name, swapchain extent, texture sizes, sampler and anisotropy support.

## Tests

Unit tests (Catch2) cover FPS smoothing. A smoke test launches the app headlessly with autoclose:
- `ctest --test-dir build --output-on-failure`

## Troubleshooting
- Missing shaders: ensure Vulkan SDK tools exist (`glslc` or `glslangValidator`) so CMake can compile GLSL into SPIR‑V.
- No Vulkan loader found: on macOS, install Vulkan SDK; the build will link against MoltenVK if available.
