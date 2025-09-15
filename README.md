# Vulkano Codex

Minimal, production-quality C++20 Vulkan application skeleton. Current stage: window + event loop using GLFW. Vulkan device, ImGui, icosphere, SSAO, and full pipeline to be added incrementally.

## Build

Requirements:
- CMake 3.25+
- A C++20 compiler (Clang, GCC, or MSVC)
- Internet access for FetchContent dependencies

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
```

Run the app:
```
./app/bin/Debug/vulkano_app
```

Run tests:
```
ctest --test-dir build -C Debug --output-on-failure
```

## Next Steps
- Wire Vulkan instance/device/swapchain with validation layers
- Integrate ImGui (GLFW + Vulkan backends)
- Implement icosphere with runtime subdivisions and TBN
- Add G-buffer, SSAO, blur, and composite passes
