[x] Scaffold SSAO: params, shaders, UI (no rendering yet)
[x] Add G-buffer images (normal + depth) and render pass
[] Fix pipeline layout during resize (recreate order)
[] Add G-buffer pipeline and draw pre-pass
[] Implement SSAO pass reading depth+normal+noise → aoRaw
[] Implement optional AO blur (H+V) → aoBlur
[] Add compose pass: litColor × AO → swapchain
[] Handle resize: recreate AO/G-buffer, descriptors, pipelines
[] ImGui controls: enable, kernel/radius/bias/power, blur, strength
[] Validation clean-up, names/markers; docs/tests update
