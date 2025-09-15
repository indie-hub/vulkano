# Plan

1. Set up CMake project structure with library and tests. [done]
2. Implement CPU icosphere generator in C++20 (header + source). [done]
3. Provide main program placeholder that uses generator. [done]
4. Add unit tests verifying vertex counts. [done]
5. Document build instructions in README. [done]
6. Integrate GLFW and Vulkan to open a window. [done]
7. Implement `Application` class managing instance and window lifetime. [done]
8. Replace placeholder `main` with `Application` usage. [done]
9. Integrate ImGui (GLFW+Vulkan backends), add UI for subdivisions/light/material/SSAO. [in-progress]
10. Extend renderer to multi-pass: G-buffer, SSAO, blur, compose. [todo]
11. Add normal map sampling, TBN, textures loader (stb). [todo]
12. Add stats in ImGui: FPS, frame, device name, extent. [todo]
