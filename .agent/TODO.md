// Task: Extend Vulkan app with FPS camera controls, ImGui gating, cursor lock

- [x] Verify build succeeds and tests pass locally (Debug).
- [x] Sanity-check code for FPS camera: RMB look gating, keyboard movement gating, FOV scroll when not captured, cursor hide/lock toggling, and Camera panel in ImGui.
- [ ] Manual QA: Run the app and verify acceptance criteria interactively (RMB look, ImGui not affecting camera, scroll FOV, lock toggle).
- [ ] Add a focused unit test for input gating logic (extract minimal gate helpers from `vulkano::App` for pure tests without GLFW).
- [ ] Update CHANGELOG with “feat: FPS camera with ImGui gating and cursor lock”.
- [ ] Confirm portability on macOS: run app with MoltenVK installed; validate no validation errors during resize and input.

