# Vulkano Renderer

## Project Description
Vulkano is a C++20 Vulkan renderer that targets macOS (via MoltenVK) and Linux
desktops. It focuses on clean architecture (SOLID, RAII, dependency injection)
while showcasing modern real-time techniques such as:

- Physically inspired lighting with per-light shadow controls.
- Screen-space ambient occlusion (SSAO) with configurable kernel/noise passes.
- Material-driven rendering backed by GPU buffers and texture caches.
- ImGui-based tooling (docking layouts, debug overlays, UV diagnostics).
- Automated geometry import through Assimp plus command-line helpers.

The repository doubles as a reference implementation for building Vulkan
applications with clear separation between platform bootstrap, renderer core,
testing infrastructure, and documentation.

## Key Features
- **Multi-pass renderer**: geometry + SSAO + blur + composite pipelines with
  shader hot-reload support during development.
- **Per-light debugging**: runtime toggles for cascaded shadows, visualization
  of frusta, and QA playbooks for regressions.
- **Material & texture system**: CPU-side registry mirrors GPU storage buffers
  so assets can be reloaded without restarting the app.
- **Diagnostics & tooling**: ImGui docking layouts, UV debug views, mesh dump
  utilities, and automated Catch2 regression tests.
- **Automation-first workflow**: `.agent/TODO.md` drives incremental work,
  while docs under `docs/` capture every investigation and acceptance plan.

## Getting Started
### Prerequisites
- CMake 3.25+
- Ninja or Make (CMake will auto-detect)
- Clang or GCC with full C++20 support
- Vulkan SDK 1.3.290+ (MoltenVK required on macOS)
- Python 3.11+ for asset scripts in `tools/`

### Configure & Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Run
```bash
./build/bin/vulkano_app
```

Use the ImGui overlay (press `Tab` if hidden) to access renderer controls:
- Toggle SSAO, blur passes, and per-light shadow maps.
- Switch to UV debug mode when validating imported assets.
- Save/load docking layouts via `imgui_layout_profile.cfg`.

### Tests
```bash
ctest --test-dir build --output-on-failure
```

## Project Layout
```
include/    Public headers
src/        Renderer and platform implementation
tests/      Catch2-based unit/integration tests
docs/       Design notes, QA guides, and investigation plans
.agent/     Planning artifacts (TODOs, per-feature plans)
tools/      CLI diagnostics (mesh dumping, UV comparison)
shaders/    GLSL sources compiled to SPIR-V during build
```

## Documentation Map
- [Per-Light Shadow Controls](docs/lighting_shadows.md) — toggle/validate
  directional light shadows in the sample scene.
- [Shadow Toggle Runtime Validation](docs/qa/shadow_toggle_validation.md) — QA
  checklist for multi-light shadow scenarios.
- [Importing Models](docs/importing_models.md) — workflow for bringing assets
  in through Assimp with correct tangents/UV channels.
- [Docking Workflow](docs/ui_docking.md) — configure ImGui layouts and persist
  them safely across sessions.
- [`docs/pr_test_personal_codex_second_try_13-ui-overhalt.md`](docs/pr_test_personal_codex_second_try_13-ui-overhalt.md)
  — reviewer checklist for merging this branch into `develop`.

## Support & Contribution
1. Follow `.agent/TODO.md` when adding or reviewing work; every change should be
   tied to a plan under `.agent/` and recorded in `CHANGELOG.md`.
2. Run `clang-format`/`clang-tidy` before pushing; CI expects a clean tree.
3. When rewriting history (e.g., author fixes), coordinate backups and update
   docs as shown in `.agent/plan_git_identity_rewrite.md`.

For questions, open an issue describing hardware, driver versions, and the
exact validation steps you followed.
