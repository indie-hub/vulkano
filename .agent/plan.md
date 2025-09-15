# Plan

1) Core scaffolding and geometry [completed]
- CMake project with library, example, tests.
- CPU icosphere generator (header + source) with TBN and UVs.

2) Rendering and UI [completed]
- GLFW + Vulkan renderer with swapchain management.
- ImGui integration (GLFW + Vulkan backends) and runtime controls.

3) Deferred pipeline + effects [completed]
- G-buffer pass (albedo, normal, viewZ) + depth.
- SSAO pass with kernel UBO and 4x4 noise texture.
- Separable blur pass (horizontal/vertical).
- Compose pass with Phong lighting and AO modulation.

4) Tooling and docs [completed]
- Shader compile step via glslc; packaging script to app/<Config>.
- README build/run docs; unit tests (icosphere, camera).

5) Follow-ups [backlog]
- Optional wireframe overlay pipeline toggle.
- Optional HDR tonemapping tweak in compose.
- SSAO compute variant and AO debug visualize.

Status: Build and tests green on local; packaged Release binary and SPIR-V into app/Release.
