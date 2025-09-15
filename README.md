# Vulkano Codex

Minimal, production-quality C++20 Vulkan application scaffold.

Current stage
- GLFW window + event loop
- CPU icosphere mesh generator with tangents/bitangents (tested)
- Vulkan context: instance + surface + device + swapchain + color+depth render pass; per-frame draw with depth test. Debug utils enabled in Debug builds.
  - On platforms without Vulkan/MoltenVK, the app still builds and runs the window loop.
- ImGui integration (GLFW + Vulkan). UI shows subdivision slider and lighting/material controls.
- Forward Phong shading with tangent-space normal mapping (procedural checkerboard albedo + flat normal map). MVP via push constants; per-frame UBO for camera/light/material.
- SSAO: G-buffer prepass (view-space normals + linear depth packed into RGBA16F), SSAO fullscreen pass writing AO (R8), forward shading multiplies AO. Basic kernel-based AO implemented; blur/TAA pending.

Shaders:
- Forward shaders: `shaders/forward.vert`, `shaders/forward.frag` (+ compiled `.spv` included). These implement Phong + normal mapping.
- Fallback simple shaders: `shaders/simple.vert` and `shaders/simple.frag` (+ `.spv`). Used if forward shaders are missing.

Compile shaders (requires Vulkan SDK tools present in PATH):
```
# Forward shading
glslc shaders/forward.vert -o shaders/forward.vert.spv
glslc shaders/forward.frag -o shaders/forward.frag.spv

# Fallback simple
glslc shaders/simple.vert -o shaders/simple.vert.spv
glslc shaders/simple.frag -o shaders/simple.frag.spv

# G-buffer + SSAO
glslc shaders/gbuffer.vert -o shaders/gbuffer.vert.spv
glslc shaders/gbuffer.frag -o shaders/gbuffer.frag.spv
glslc shaders/fullscreen.vert -o shaders/fullscreen.vert.spv
glslc shaders/ssao.frag -o shaders/ssao.frag.spv
```

## Build

Requirements:
- CMake 3.25+
- A C++20 compiler (Clang, GCC, or MSVC)
- Internet access for FetchContent dependencies
- Vulkan SDK or MoltenVK (optional at this stage; enables Vulkan rendering path)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
```

Run the app:
```
./app/bin/Debug/vulkano_app

Tip: to run headless for CI sanity without a visible window, set the environment variable `HEADLESS_RUN_MS` (milliseconds):

```
HEADLESS_RUN_MS=500 ./app/bin/Debug/vulkano_app
```
```

Run tests:
```
ctest --test-dir build -C Debug --output-on-failure
```

## Runtime controls (ImGui)
- Icosphere subdivisions [0..6] (rebuilds mesh + GPU buffers)
- Light: position, color, intensity, shininess
- Material: normal map strength
- SSAO: enable, kernel size, radius, bias, power, blur on/off, blur radius (blur not yet implemented)

## Platform notes
- Windows/Linux: Vulkan 1.2+; ensure Vulkan SDK runtime (loader) installed to pick up your GPU driver’s ICD
- macOS: requires MoltenVK (via Vulkan SDK). You may need to export `VK_ICD_FILENAMES` to point to MoltenVK’s JSON if the loader cannot find it automatically.

## Next Steps
- Add separable blur pass for AO and wire composite strength
- Robust swapchain recreation for all offscreen resources
- Optional wireframe overlay pipeline
