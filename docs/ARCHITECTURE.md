# Architecture Overview

This document outlines the runtime architecture, render pipeline, and main modules of the Vulkan application.

## Runtime Modules
- `vulkan_app::Application`: Owns the window (GLFW), high‑level run loop, and orchestration. Hosts UI state (subdivisions, light, material, SSAO) and triggers mesh rebuilds.
- `vulkan_app::Renderer`: Encapsulates Vulkan objects and per‑frame rendering. Manages instance/device/swapchain, render pass, pipeline(s), resources, and draw submission. Exposes minimal accessors for ImGui backend initialization.
- `vulkan_app::ImGuiLayer`: Thin wrapper around Dear ImGui with GLFW + Vulkan backends. Builds the UI for mesh, lighting, materials, and SSAO; renders over the main render pass.
- `vulkan_app::Camera`: Simple camera with view/projection helpers. Parameters adjustable from UI.
- `vulkan_app::make_icosphere`: CPU generator for the icosphere mesh with UVs and TBN.
- `types.hpp`: Small PODs for UBOs and UI‑tweakable parameters.

## Render Pipeline
A multi‑pass pipeline with explicit synchronization. Initial version can operate as separate render passes if subpasses are not yet wired.

1. G‑Buffer pass
   - Attachments: Albedo (RGBA8 sRGB), Normal (RGBA16F in view space), Depth (D32 or D24S8).
   - Vertex attributes: position, normal, tangent, bitangent, texcoord.
   - Fragment: normal mapping in tangent space; outputs albedo and normals. Depth is written naturally.

2. SSAO pass
   - Samples depth + normals; applies hemisphere kernel and small 4×4 noise texture tiled across screen.
   - Outputs ambient occlusion factor (R8).

3. Blur pass
   - Separable Gaussian blur (horizontal then vertical) to denoise the AO buffer.

4. Compose/Lighting
   - Phong (N·L diffuse + Blinn‑Phong specular) using G‑buffer and AO (optionally modulated by strength factor).
   - Gamma correction on output; option to add simple ACES tonemap if extended later.

5. ImGui
   - UI rendered on top.

## Swapchain Lifecycle
- Recreate swapchain on window resize or `VK_ERROR_OUT_OF_DATE_KHR`/`VK_SUBOPTIMAL_KHR`.
- Destroy dependent objects in reverse order: framebuffers → images/views → pipeline → render pass → swapchain.
- Recreate and rebind descriptor sets/resources that depend on swapchain extent and formats.

## Resources and Descriptors
- Per‑frame UBOs: camera matrices, light, material, SSAO parameters.
- Combined image samplers: albedo + normal map for material.
- SSAO kernel/params in UBO; noise texture sampled as repeat‑tiled.

## Mesh Regeneration Flow
- UI change to “Subdivisions” marks `pending_subdivisions` in `Application`.
- `Application::rebuild_mesh_if_needed` invokes `make_icosphere`, recomputes TBN (generator outputs), and calls `Renderer::update_mesh` to upload new vertex/index buffers.
- Keeps CPU rebuild under ~20 ms for ≤ 4 subdivisions on mid‑range CPUs.

## Error Handling & Validation
- Debug builds enable validation layers (`ENABLE_VALIDATION`).
- Fail fast on Vulkan call failures with clear error messages; invariants asserted in debug builds.
- Uses `VK_EXT_debug_utils` to name objects and aid debugging when available.

## Platform Notes
- macOS via MoltenVK: portability enumeration instance flag and `VK_KHR_portability_subset` device extension; ensure loader sees MoltenVK ICD JSON.

## Build & Shaders
- Shaders compiled with `glslc` to `${build}/shaders/*.spv` if available; otherwise copied as placeholders (not runnable). Runtime resolution searches both build and source shader directories.

