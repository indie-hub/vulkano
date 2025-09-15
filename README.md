# Vulkano Codex

Modern C++20 Vulkan 1.2 application that opens a GLFW window and renders a dynamically-subdividable icosphere with per-pixel Phong shading, tangent-space normal mapping, and SSAO. Includes an ImGui UI for runtime controls.

## Build

Prerequisites: Vulkan SDK (for headers, loader, and `glslc`), a C++20 compiler, and CMake 3.25+.

Options:
- `ENABLE_VALIDATION=ON` to enable Vulkan validation layers in Debug.
- `ENABLE_GLFW_WAYLAND=ON` on Wayland Linux if needed.
- `ENABLE_IMGUI_DOCKING=ON` to enable ImGui docking (build-time flag only).

Build:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_VALIDATION=ON
cmake --build build --config Release
```

## Run

```
./build/bin/vulkan_app_example
```

## Test

```
ctest --test-dir build
```

## Controls (ImGui)
- Subdivisions: 0..6; regenerates mesh and reuploads buffers.
- Light: position, color, intensity, shininess.
- Material: albedo, normal map strength.
- SSAO: enable, kernel size (16/32/64), radius, bias, power, blur and blur radius.
- Stats: FPS, frame time, device name, swapchain extent.

## Notes
- On macOS, install MoltenVK and ensure Vulkan loader finds the ICD (e.g., set `VK_ICD_FILENAMES`).
- Shaders are compiled to `${build}/shaders/*.spv` if `glslc` is found; otherwise copied as placeholders.
