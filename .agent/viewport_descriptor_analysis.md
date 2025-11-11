# Viewport Descriptor Lifecycle Analysis

## Resize path trace
- `SceneRenderer::set_viewport_extent()` guards zero/unchanged extents then calls `recreate_viewport_resources()`.
- `recreate_viewport_resources()` performs a full teardown (`destroy_scene_framebuffer()`, `destroy_color_resources()`, `destroy_depth_resources()`, `destroy_viewport_descriptor()`) before recreating images, depth, framebuffer, and finally calling `ensure_viewport_descriptor()`.
- `destroy_viewport_descriptor()` currently frees the ImGui texture descriptor via `ImGui_ImplVulkan_RemoveTexture()` and resets cached handles (`m_viewportDescriptor`, `m_viewportSampler`, `m_viewportImageView`). It also clears the registered `ImTextureData` TexID but leaves its status as-is.
- `ensure_viewport_descriptor()` allocates a sampler if needed, lazily registers an `ImTextureData`, refreshes width/height, and rebinds the descriptor to the latest `m_sceneColorImage.view()`.

## Descriptor state verification
- Because `destroy_viewport_descriptor()` executes before new color images are created, there is no lingering descriptor referencing destroyed image views. The cached `m_viewportImageView` is cleared and only set after a successful `ImGui_ImplVulkan_AddTexture()` call.
- `ensure_viewport_descriptor()` only attempts to (re)create the descriptor after confirming both the ImGui renderer backend is initialised (`BackendRendererUserData != nullptr`) and the new `VkImageView` is valid. Descriptor writes on re-use path (`vkUpdateDescriptorSets`) target the cached descriptor and update the stored view handle.
- The registered `ImTextureData` is reused across resizes. We must ensure its status matches the descriptor lifecycle (`ImTextureStatus_Destroyed` when descriptor removed, `ImTextureStatus_OK` when rebound) to stay in sync with the docking branch texture workflow.

## SSAO descriptor refresh
- `Application::drawDockspace()` propagates the docked viewport size each frame. When `SceneRenderer::set_viewport_extent()` reports a change and both the normal and linear depth image views are non-null, `SSAODescriptors::update_gbuffer_views()` is invoked to patch bindings 2 and 3 of the SSAO descriptor set.
- `update_gbuffer_views()` writes the new `VkImageView` handles with `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, so the SSAO pass receives the latest g-buffer images after every viewport rebuild.
- Additional viewport-dependent resources (SSAO blur pass depth view) are updated in the same branch, ensuring no stale image views remain after resize.

## Issues uncovered during analysis
- The upgraded Dear ImGui 1.92 changes the default `ImTextureID` type to `ImU64`. Our helper assumed a pointer type and introduces a failing `static_assert`, preventing compilation. The conversion helper must accept either pointer or integral types and the alias in `scene_renderer.hpp` should defer to Dear ImGui's definition.
- When releasing the viewport descriptor we currently only null the TexID. We should also mark the `ImTextureData` as destroyed to keep Dear ImGui's texture tracker consistent across frames and avoid backend assertions when validation is enabled.
- No SSAO descriptor issues were found in code, but we should confirm at runtime after repairing the viewport texture plumbing and ensure validation layers remain clean post-resize.

## Next steps
1. Update the viewport texture helper utilities to support integral `ImTextureID` values and clean up the alias in `scene_renderer.hpp`.
2. Propagate proper `ImTextureStatus` transitions when tearing down or recreating the viewport texture, then rebuild and validate with VK_LAYER_KHRONOS_validation enabled.
3. Proceed to implement persistent viewport texture descriptor updates once the lifecycle handling compiles and validation passes.
