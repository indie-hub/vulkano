# TODO / Plan
Phases:
1) Swapchain + forward pipeline + draw icosphere. [done]
2) Mesh buffers upload + resize handling. [done]
3) ImGui integration with subdivision control. [done]
4) G-buffer, SSAO, blur, composite passes. [in-progress]
5) Validation, cleanup, docs/tests updates. [todo]

Notes:
- Follow AGENTS.md: angle-bracket includes, braces, RAII, const.
- Commit and push after each file edit.
Next:
- Add offscreen G-buffer images + render pass, wire geometry pass to write albedo/normal/depth.
- Add fullscreen pipelines for SSAO, blur (separable), and compose.
- Generate SSAO kernel + noise textures on CPU and upload to GPU.
- Hook ImGui toggles to descriptor updates and constants.
