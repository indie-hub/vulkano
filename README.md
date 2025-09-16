# Vulkano Codex

Build & Run (macOS/Linux/Windows)

- Prereqs: CMake 3.25+, C++20 compiler, Vulkan SDK 1.2+ (on macOS: install MoltenVK + vulkan-loader).
- CMake will fetch GLFW, GLM, ImGui, and Catch2 via FetchContent.

Commands

- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- Build: `cmake --build build -j`
- Run app: `./bin/vulkano_codex_app`
- Run tests: `ctest --test-dir build`

Notes

- Shaders compile at build time using `glslangValidator` into `bin/shaders/`.
- Validation layers are enabled in Debug builds. On macOS, portability extensions are enabled for MoltenVK.
