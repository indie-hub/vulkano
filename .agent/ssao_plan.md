# SSAO Implementation Plan

## Goals
- Introduce screen-space ambient occlusion to enhance contact shadows while keeping the existing forward renderer intact.
- Maintain real-time performance on the sample scene (target ≥60 FPS on current hardware) with scalable quality knobs.

## High-Level Phases
1. **G-Buffer Export Pass**
   - Add a minimal geometry pass rendering to a framebuffer with RG16F view-space normals and R32F linear depth (or reuse depth buffer via copy).
   - Reuse current mesh pipeline with simplified shaders writing only position/normal; driven by existing primitive data.
   - Synchronize outputs for subsequent SSAO pass (layout transitions).

2. **SSAO Kernel & Noise Setup**
   - Generate a hemisphere sample kernel (32–64 samples) in view space and upload via UBO.
   - Build a small tiled noise texture (4×4) for per-fragment tangent rotation.

3. **SSAO Pass**
   - Implement a compute (preferred) or fullscreen graphics pass sampling normal/depth attachments to accumulate occlusion using kernel + noise.
   - Parameters: sample radius, bias, power, intensity.
   - Output single-channel R8 texture (occlusion factor).

4. **Blur / Denoise**
   - Apply separable bilateral blur (horizontal + vertical) in compute or graphics to reduce noise while respecting depth discontinuities.
   - Tunable blur radius.

5. **Composite & UI Integration**
   - Sample blurred occlusion in main lighting pass; modulate ambient term.
   - Extend ImGui with SSAO enable toggle and sliders for radius, bias, power, intensity, blur radius, quality preset (kernel size / resolution).
   - Optionally allow half-resolution SSAO with upscale toggle.

6. **Validation & Tooling**
   - Add Catch2 coverage for kernel generation utilities.
   - Extend integration smoke test to ensure SSAO resources build/bind successfully when enabled.
   - Update README with SSAO description and controls.

## Key Technical Tasks
- Extend descriptor sets: new SSAO descriptor layout (normals, depth, kernel UBO, noise sampler, random seed).
- Create SSAO pipeline objects (compute shader modules) and command buffer recording steps (barriers between passes).
- Manage additional images (normal RT, SSAO raw, SSAO blur) with lifetime tracking and resize handling.
- Add shader assets: geometry normal pass (vs/ps), SSAO compute, blur compute.

## Acceptance Criteria
1. **Rendering**: When SSAO is enabled, ambient shading exhibits plausible occlusion; disabling reverts to pre-SSAO output with no artifacts.
2. **Performance**: Default quality maintains ≥60 FPS on reference machine; half-resolution option available for scaling.
3. **Stability**: No Vulkan validation errors/warnings during SSAO passes (layout transitions, descriptor usage, barriers verified).
4. **Controls**: ImGui exposes SSAO toggle plus radius, bias, intensity, power, blur radius, and quality preset; changes apply immediately.
5. **Resizing**: Window resize triggers correct recreation of SSAO images without leaks or crashes.
6. **Testing & Docs**: Kernel generation unit tests pass; integration smoke test covers SSAO enable path; README documents feature and controls; full build + test suite passes (`cmake --build build`, `ctest --test-dir build --output-on-failure`).

