# TODO — Vulkano Codex

High-level plan to reach acceptance criteria.

1) Bootstrap project
- CMake project with options (validation, wayland, imgui docking)
- FetchContent: GLFW, GLM, Catch2, stb (header-only), ImGui (+ backends)
- Baseline .clang-format/.clang-tidy; CI stub

2) Core engine scaffolding
- VulkanContext: instance, debug utils, physical/logical device, queues
- Swapchain: surface, formats, image views, recreate on resize
- VMA allocator: buffers/images helper
- Frame graph-ish: render passes for GBuffer, SSAO, Blur, Compose
- ImGuiLayer: init, new frame, render draw data

3) Geometry & assets
- Icosphere generator with subdivisions 0..6
- Tangent/bitangent generation from UVs (robust)
- Procedural fallback textures: checker albedo + flat/normal map

4) Renderer passes
- GBuffer: position/depth, normal (view space), albedo (sRGB)
- SSAO: kernel + noise texture; radius, bias, power
- Blur: separable Gaussian (configurable radius)
- Compose: Blinn-Phong + SSAO; gamma correction

5) UI + Controls
- ImGui controls as per spec
- Fly camera (WASD, mouse look), toggle cursor capture

6) Tests
- Unit tests for icosphere topology and tangent generation
- Minimal compile/run sanity in CI (headless skip rendering)

7) Docs
- README with build/run instructions (Windows/Linux/macOS)
- Notes on Vulkan SDK/MoltenVK

Milestone 1 (organized repo):
- Project skeleton settled; unified CMake, per-config bin/lib outputs
- CI added (Linux/Windows/macOS with Vulkan SDK)
- .gitignore expanded; tools scripts for format/tidy/build
- Icosphere generator + tests in place
- Renderer/ImGui scaffolding; shader compile step in CMake

Next milestones tracked here as we implement passes.

Upcoming (M2):
- SSAO G-buffer layout and attachments; basic full-screen pipeline
- Texture loader (stb) and procedural fallbacks
- ImGui controls wiring for lighting/material/SSAO
