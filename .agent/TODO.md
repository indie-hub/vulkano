[x] Scaffold SSAO: params, shaders, UI (no rendering yet)
[] Add G-buffer targets (normal, depth sampled) and pass
[] Implement SSAO pass reading depth+normal+noise → aoRaw
[] Implement optional AO blur (H+V) → aoBlur
[] Add compose pass: litColor × AO → swapchain
[] Handle resize: recreate AO/G-buffer, descriptors, pipelines
[] ImGui controls: enable, kernel/radius/bias/power, blur, strength
[] Validation clean-up, names/markers; docs/tests update
