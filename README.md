# vulkano

Build
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja`
- Build: `cmake --build build --config Debug`
- Run: `./bin/vulkano_app`

Notes
- On macOS, install MoltenVK (Vulkan SDK) to run the app.
- To enable runtime GLSL compilation (for shaders), configure with `-DVULKANO_CODEX_USE_SHADERC=ON`.
