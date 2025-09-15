# Shader Pipeline Overview

This document describes the shader stages used by the renderer, their inputs/outputs,
and the data flow across the implemented multi-pass pipeline (G-buffer, SSAO, blur, compose).

## Conventions
- Right-handed coordinate system with GLM; radians throughout.
- sRGB sampling for albedo; gamma-correct output to the swapchain surface.
- Tangent-space normal mapping via per-vertex TBN; TBN is orthonormalized in the shader if needed.

## Vertex Attributes
- `location = 0`: `vec3 position`
- `location = 1`: `vec3 normal`
- `location = 2`: `vec2 uv`
- `location = 3`: `vec3 tangent`
- `location = 4`: `vec3 bitangent`

## Uniforms and Resources
- Camera UBO: view, proj, viewProj, camera position.
- Light UBO: position, color, intensity, shininess.
- Material UBO: albedo color, normal strength.
- Samplers: albedo texture (sRGB), normal map (linear).
- SSAO UBO: kernelSize, radius, bias, power; blur params.
- SSAO noise: 4×4 tiled noise texture sampled in screen space.

## Passes

### 1) G-buffer
Vertex shader (`gbuffer.vert`):
- Transforms positions to clip space.
- Passes world/view-space position, UV, and TBN basis to the fragment stage.

Fragment shader (`gbuffer.frag`):
- Samples albedo (sRGB) and normal map (linear).
- Reconstructs tangent-space normal and transforms to view space using TBN.
- Outputs:
  - `Albedo`: RGBA8 sRGB (material color, alpha = 1).
  - `Normal`: RGBA16F (view-space normal, W unused or roughness placeholder).
  - Depth: D32 or D24S8 written by depth test.

### 2) SSAO
Fragment (`ssao.frag`):
- Reads linearized depth and view-space normals from G-buffer.
- Uses a hemisphere kernel oriented by the normal; offsets sampled within radius.
- Applies bias to reduce self-occlusion.
- Outputs AO factor to R8 texture.

### 3) Blur
Fragment (`blur.frag`):
- Separable Gaussian blur (horizontal then vertical) over AO texture.
- Radius configurable; skip if disabled.

### 4) Compose / Lighting
Fragment (`compose.frag`):
- Phong lighting (N·L diffuse + Blinn-Phong specular) using albedo and normal.
- Multiplies by AO (blurred if enabled) with adjustable power/strength.
- Applies gamma correction and optional tonemapping if extended.

## Depth Linearization
Depth from the G-buffer is converted to linear/view-space Z to maintain consistency across SSAO and composition.
Ensure the same near/far parameters as the camera UBO.
