# TODO — Vulkano Codex

Short-term fixes and next steps. Keep updated as work progresses.

## Done
- Enable Vulkan portability on macOS (MoltenVK):
  - Add VK_KHR_portability_enumeration + instance flag.
  - Add VK_KHR_portability_subset device extension when available.
  - Use concurrent image sharing when graphics/present queues differ.
 - Add public header Doxygen documentation across modules.
 - Add CHANGELOG.md (Keep a Changelog) and architecture doc.

## Next
- SSAO pipeline: G-buffer, SSAO, blur, and compose passes.
- ImGui controls wiring for SSAO params and mesh subdivisions.
- Swapchain resize path validation and cleanup ordering.
- Add end-to-end test harness (headless where possible) and unit tests for math/icosphere.
- macOS README notes: MoltenVK install, VK_ICD_FILENAMES hints.
- CI: format (clang-format) and clang-tidy jobs.

## Notes
- Pushing to origin failed (no permission). Either set upstream to a fork with GITHUB_TOKEN or grant access.
  - Local commits created after each file edit; pushes failed with 403.
