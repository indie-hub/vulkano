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
- macOS (MoltenVK):
  - Install MoltenVK (via Vulkan SDK or Homebrew) and ensure the Vulkan loader can find the MoltenVK ICD JSON.
    Example: `export VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json` (path varies by install).
  - The app enables `VK_KHR_portability_enumeration` and sets `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, and
    enables `VK_KHR_portability_subset` on the device when available.
- Shaders are compiled to `${build}/shaders/*.spv` if `glslc` is found; otherwise copied as placeholders.
