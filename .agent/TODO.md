# Vulkano Codex — Agent TODO

High-level plan
- Renderer: implement swapchain creation, image views, render pass chain (G-buffer, SSAO, blur, composite), framebuffers, command buffers, sync, and per-frame resources.
- Mesh pipeline: GPU buffers (vertex/index), attribute formats (pos/normal/uv/tangent/bitangent).
- Descriptors: UBOs for camera/light/material/SSAO, samplers and sampled images, SSAO kernel + noise texture.
- ImGui: hook up imgui_impl_vulkan with renderer render pass, draw UI, and present.
- Camera: fly camera controls (WASD + mouse), ImGui toggle for cursor lock.
- Shaders: confirm SPIR-V compilation via glslc; adjust as needed.
- Swapchain: resize handling and full recreation path.
- Tests: expand icosphere tests (counts for multiple subdivisions, tangent orthogonality), SSAO kernel generation unit test.

Next steps
1. Implement swapchain creation (query caps, choose extent/format, present mode).
2. Add G-buffer render pass with attachments and pipeline layout.
3. Add mesh upload + descriptor setup for gbuffer pass.
4. SSAO + blur textures/pipelines.
5. Composite pass + presentation.
6. Integrate ImGui over final pass.

