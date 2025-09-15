# Vulkano Codex

Minimal, production-quality C++20 Vulkan application scaffold.

Current stage
- GLFW window + event loop
- CPU icosphere mesh generator with tangents/bitangents (tested)
- Vulkan context: instance + surface + device + swapchain + render pass + clear-only frame submit (enabled when Vulkan present)
  - On platforms without Vulkan/MoltenVK, the app still builds and runs the window loop.
- ImGui integration (GLFW + Vulkan). UI shows subdivision slider (hooked; GPU upload pending)

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
- Integrate ImGui (GLFW + Vulkan backends)
- Implement runtime icosphere subdivisions + GPU buffer upload
- Add G-buffer, SSAO, blur, and composite passes
