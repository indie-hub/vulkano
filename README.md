# Vulkano Codex

Minimal, production-quality C++20 Vulkan application scaffold.

Current stage
- GLFW window + event loop
- CPU icosphere mesh generator with tangents/bitangents (tested)
- Vulkan context: instance + surface + device + swapchain + render pass + clear-only frame submit (enabled when Vulkan present)
  - On platforms without Vulkan/MoltenVK, the app still builds and runs the window loop.
- ImGui integration (GLFW + Vulkan). UI shows subdivision slider; changing it rebuilds CPU mesh and, when shaders are present, reuploads GPU buffers.

Shaders:
- Simple passthrough shaders are provided in `shaders/simple.vert` and `shaders/simple.frag`.
- At runtime, the app looks for `shaders/simple.vert.spv` and `shaders/simple.frag.spv`. If absent, it skips mesh rendering and only clears + renders ImGui.

Compile shaders (requires Vulkan SDK tools present in PATH):
```
glslc shaders/simple.vert -o shaders/simple.vert.spv
glslc shaders/simple.frag -o shaders/simple.frag.spv
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

## Next Steps
- Add validation layers + debug utils
- Implement runtime icosphere subdivisions + GPU buffer upload [DONE]
- Add G-buffer, SSAO, blur, and composite passes
