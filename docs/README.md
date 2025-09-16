VulkanoCodex
================

A minimal Vulkan + GLFW + GLM + ImGui C++20 app that renders a white triangle on black and shows an ImGui overlay with FPS, frame time, device name, and swapchain extent. Binaries are placed in `bin/`.

Build
- Prereqs: Vulkan SDK (loader, headers, runtime), CMake >= 3.25, a C++20 compiler.
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- Build: `cmake --build build --config Release -j`
- Run: `./build/bin/vulkano_app`

Tests
- Enable tests: `-DBUILD_TESTING=ON`
- Run tests: `ctest --test-dir build -V`

Notes
- Validation layers are enabled in Debug builds.
- Shaders are compiled at build/run via shaderc (embedded GLSL sources).
