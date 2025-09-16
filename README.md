# Vulkan Triangle with ImGui

Simple Vulkan 1.2 application in C++20 using GLFW, GLM and Dear ImGui. It opens a window, clears to black, draws a single white triangle, and overlays runtime stats (FPS, frame time, device name, swapchain extent) via ImGui. Validation layers are enabled in Debug builds and debug utils names/labels are set.

Requirements
- Vulkan SDK installed and `glslc` in PATH
- CMake 3.20+
- A C++20 compiler (Clang/GCC/MSVC)

Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Run
```
bin/vulkano_app
```

Tests
```
ctest --test-dir build --output-on-failure
```

Notes
- Binaries and SPIR-V shaders are written to `bin/`.
- On macOS, MoltenVK (Vulkan Portability) is supported; portability extensions are enabled.
