[2025-09-15T15:50:57] OpenAI Codex v0.34.0 (research preview)
--------
workdir: /Users/bruno/Documents/Development/bruno/vulkano_codex
model: gpt-5
provider: openai
approval: never
sandbox: danger-full-access
reasoning effort: medium
reasoning summaries: auto
--------
[2025-09-15T15:50:57] User instructions:
Make often commits and push in the end.
Make sure the code compiles and runs before submitting your work.
Never finish without commiting first.

Use the .agent/ directory as a scratchpad for your work. Store long term plans and todo lists there. Keep the TODO.md file updated and start by checking it.

You will need to write end to end and unit tests for the project. But make sure to spend most of your time on the actual coding, not on the testing. A good heuristic is to spend 80% of your time on the actual porting, and 20% on the
testing.

Use the GITHUB_TOKEN environment variable to access github and the user: indie-hub

Make a toplevel folder for the application with all the necessary running (binary!) files inside. Use an appropriate folder structure.

Goal

Build a minimal, production-quality Vulkan application in modern C++20 that opens a window (GLFW), renders a dynamically-subdividable icosphere with per-pixel Phong shading, tangent-space normal mapping, and screen-space ambient occlusion (SSAO). Provide an ImGui UI to pick the number of icosphere subdivisions at runtime. Target Windows/Linux; macOS via MoltenVK.

Tech & Libraries
	•	Language/Std: C++20 (RAII, const whenever possible, uniform initialisation {}).
	•	Graphics: Vulkan 1.2+ (or highest available), validation layers, debug utils.
	•	Windowing: GLFW (Vulkan surface).
	•	Math: GLM (right-handed, radians).
	•	UI: Dear ImGui with imgui_impl_glfw + imgui_impl_vulkan.
	•	Images: stb_image (albedo/normal load).
	•	Optional macOS: MoltenVK.
	•	Build: CMake (no globbing; explicitly list files).

Visual Features
	•	Geometry: Icosphere with runtime-selectable subdivisions (0–6). Changing the value regenerates CPU mesh, recomputes tangents/bitangents, re-uploads GPU buffers.
	•	Shading: Per-pixel Phong (N.L diffuse + Blinn-Phong specular), normal mapping in tangent space (TBN).
	•	Lighting: Single moveable point/spot light (ImGui controls: position, colour, intensity, shininess).
	•	SSAO pass:
	•	Geometry pass outputs: world/ view-space normals, depth (linear or view-space z), albedo.
	•	SSAO kernel (e.g., 32 or 64 hemisphere samples) + 4×4 noise texture, radius and bias adjustable via ImGui.
	•	Blur pass (separable Gaussian).
	•	Composite: lit * AO with optional AO strength slider.

Runtime UI (ImGui)
	•	Icosphere subdivisions: integer slider [0..6].
	•	Light: position (vec3), colour, intensity, shininess.
	•	Materials: diffuse/albedo colour, normal map strength.
	•	SSAO: enable/disable, kernel size, radius, bias, power/strength, blur on/off, blur radius.
	•	Stats: FPS, frame time, Vulkan device name, swapchain extent.

Rendering Pipeline (suggested)
	1.	G-Buffer pass (subpass 0):
	•	Colour attachments: Albedo (RGBA8), Normal (RGBA16F), maybe Roughness/Metallic if you want future PBR; Depth: D32/24S8.
	•	Vertex attributes: position, normal, tangent, bitangent, texcoord.
	2.	SSAO pass (separate render pass or subpass 1 with input attachments):
	•	Sample depth + normals, generate AO term, write to AO texture (R8).
	3.	SSAO blur pass (compute or graphics):
	•	Blur AO into AO_blur.
	4.	Lighting/composite pass:
	•	Full-screen quad combining G-buffer with AO_blur and Phong lighting.
	5.	ImGui pass:
	•	Draw UI on top.

If subpasses are too heavy for v1, separate render passes with explicit image layouts/transitions are fine.

Shaders (GLSL; compile to SPIR-V)
	•	gbuffer.vert: VS for icosphere (outputs pos, uv, TBN, view pos).
	•	gbuffer.frag: writes Albedo, Normal (from normal map via TBN), and depth.
	•	ssao.frag (or compute): samples kernel in tangent-to-view hemisphere using depth+normal to create AO.
	•	blur.frag (or compute): separable blur for AO.
	•	compose.frag: Phong lighting + AO multiplication; includes gamma correction and optional HDR tonemap (ACES approximate fine if trivial).

Geometry: Icosphere
	•	Start from 12-vertex icosahedron, subdivide each triangle; normalise to unit sphere per subdivision.
	•	Compute per-vertex tangents/bitangents from UVs (MikkTSpace-like is nice, but a robust standard tangent calc is fine).
	•	Generate UVs suitable for sphere (simple latitude/longitude unwrap or octahedral; ensure continuity at seam).
	•	Provide example textures (albedo + normal) or generate a checkerboard if none found.
	•	If no textures are provided: generate a checkerboard albedo and a blue noise based normal map

Descriptors & Resources
	•	Uniform buffer(s) for camera matrices (view, proj), per-frame.
	•	Push constants or storage for model matrix.
	•	Combined image samplers: Albedo, Normal map.
	•	SSAO: kernel samples uniform buffer, noise texture (tile 4×4 across screen), read-only depth+normal attachments as sampled/input attachments.
	•	Sampler objects: one for material maps; separate for AO/blur reads.

Swapchain & Sync
	•	Handle swapchain recreation on resize (destroy dependent framebuffers/pipelines, recreate).
	•	Per-frame resources (frames-in-flight = min 2, match swapchain images).
	•	Fences/semaphores for acquire/present; layout transitions (barriers) correct and explicit.

Controls / Camera
	•	Basic fly camera (WASD + mouse look) with ImGui toggles to lock/unlock cursor.
	•	FOV, near/far via ImGui.

CMake Requirements
	•	Options: ENABLE_VALIDATION, ENABLE_GLFW_WAYLAND (if Linux), ENABLE_IMGUI_DOCKING (optional).
	•	Find or fetch: GLFW, GLM, Vulkan, ImGui (with backends), stb.
	•	Output binaries to bin/ per-config (bin/Debug, bin/Release).
	•	On macOS, link MoltenVK and set VK_ICD_FILENAMES or add a note to run with proper loader config.
    * Do not use globbing; list files explicitly in CMake.

Code Quality & Style
	•	Clean, layered architecture; single-responsibility classes.
	•	No raw new/delete. Use unique_ptr, vector, span.
	•	Always use braces, even for one-liners. Prefer {} initialisers for members.
	•	Prefer angle-bracket includes and project includes over local includes.
	•	Error handling: fail fast with clear messages; assert on invariant violations in debug.
	•	Validation layers enabled in Debug; VK_EXT_debug_utils naming and markers.
	•	Document public headers (brief Doxygen-style comments).
	•	Provide a short README.md with run instructions.

ImGui Interactions
	•	Changing “Subdivisions” triggers mesh rebuild + GPU buffer reupload.
	•	Sliders: AO radius (e.g., 0.1–2.0), AO bias (0.001–0.05), kernel size (16/32/64), AO power (0–2), Blur radius (1–5), Normal strength (0–2), Shininess (8–256).
	•	Toggle wireframe overlay (optional second pipeline state).

Performance & Correctness
	•	Target 60 FPS on mid-range GPU; keep CPU rebuild under ~20 ms for subdivisions ≤ 4.
	•	Ensure correct sRGB sampling for albedo; perform gamma-correct output.
	•	Depth linearisation consistent across SSAO/compose passes.
	•	Normal map sampling uses TBN constructed from per-vertex tangent/bitangent and normal; orthonormalise TBN if needed.

Deliverables
	•	Complete source tree as above.
	•	Precompiled SPIR-V shaders or CMake shader compile step.
	•	Two small sample textures for demo.
	•	README with build/run steps and key controls.
	•	Notes on macOS/MoltenVK specifics (if any limitations vs. Windows/Linux).

Acceptance Criteria (must pass)
	•	App runs, shows shaded icosphere with normal mapping visible (toggle normal strength to 0 to compare).
	•	ImGui slider reliably changes subdivisions and updates geometry.
	•	SSAO visibly darkens creases/cavities; turning it off/on, adjusting radius/bias produces expected changes; blur reduces noise.
	•	Resizing the window recreates the swapchain without crashes or leaks.
	•	Validation layers report no errors in normal usage.
	•	Code compiles cleanly with C++20 on MSVC or Clang/GCC (CI matrix optional).




[2025-09-15T15:51:05] thinking

**Planning Vulkan App Development**

I've got a large development task with a specific environment setup. It has danger-full-access and requires frequent commits. I need to use a scratchpad with TODO.md; I'll check if it exists and create one if not. The plan is to build a Vulkan app using various libraries. It's a big undertaking, and time is tight. I want to bootstrap a minimal Vulkan app that opens a window, sets up everything, and integrates necessary features progressively, starting with the basics and expanding later.
**Implementing Shading Techniques**

I can start by implementing single-pass forward shading with normal mapping, then iterate to include G-Buffer and SSAO later. But I need to ensure SSAO is in our acceptance criteria, so a simple implementation is crucial. I could include precompiled SPIR-V binaries or compile shaders at build time. It might require a small step in Python or CMake, adding a dependency on the Vulkan SDK, but that's manageable. I need to check if the necessary tools are installed and ensure the code compiles and runs before submission, but building could pose challenges here.
**Stubbing a Vulkan Sample**

I’m considering creating a minimal sample that could compile without the Vulkan SDK by using volk or linking dynamically. I still need the necessary headers, and volk utilizes Vulkan headers, which simplifies things. I might vend Vulkan-Headers as a submodule or use FetchContent for installation, along with GLFW and GLM. For linking, the Vulkan loader library is required, with different options for Linux and macOS. While linking to the Vulkan library could be optional with volk, the goal is to ensure cross-platform compatibility, which may necessitate the Vulkan SDK.
**Setting Up CMake for Dependencies**

I’m considering using CMake to fetch dependencies while requiring Vulkan to be installed. However, there's a conflict since the environment might not allow compilation, yet the instructions stress that the code must compile and run before submission. This is tricky, but I can create the repository structure and make frequent commits. I’d like to include unit tests for the icosphere generator and math components, as they can compile without Vulkan. I’ll enable BUILD_TESTING and use ctest, but the configuration may fail if Vulkan isn't installed.
**Exploring Repository for Testing**

I can set options to build tests without needing Vulkan. I’ll structure CMake with ENABLE_VULKAN set to OFF by default, which will help with building unit tests, though acceptance requires a Vulkan app. The user wants us to spend 80% of our time coding and to commit often. I might need to use GITHUB_TOKEN for access and check if they want us to create a new repository. I should check the current path, /Users/.../vulkano_codex, and run git status to explore the repository.
[2025-09-15T15:51:34] codex
**Implementing Shading Techniques**

I can start by implementing single-pass forward shading with normal mapping, then iterate to include G-Buffer and SSAO later. But I need to ensure SSAO is in our acceptance criteria, so a simple implementation is crucial. I could include precompiled SPIR-V binaries or compile shaders at build time. It might require a small step in Python or CMake, adding a dependency on the Vulkan SDK, but that's manageable. I need to check if the necessary tools are installed and ensure the code compiles and runs before submission, but building could pose challenges here.
[2025-09-15T15:51:34] codex
**Stubbing a Vulkan Sample**

I’m considering creating a minimal sample that could compile without the Vulkan SDK by using volk or linking dynamically. I still need the necessary headers, and volk utilizes Vulkan headers, which simplifies things. I might vend Vulkan-Headers as a submodule or use FetchContent for installation, along with GLFW and GLM. For linking, the Vulkan loader library is required, with different options for Linux and macOS. While linking to the Vulkan library could be optional with volk, the goal is to ensure cross-platform compatibility, which may necessitate the Vulkan SDK.
[2025-09-15T15:51:34] codex
**Setting Up CMake for Dependencies**

I’m considering using CMake to fetch dependencies while requiring Vulkan to be installed. However, there's a conflict since the environment might not allow compilation, yet the instructions stress that the code must compile and run before submission. This is tricky, but I can create the repository structure and make frequent commits. I’d like to include unit tests for the icosphere generator and math components, as they can compile without Vulkan. I’ll enable BUILD_TESTING and use ctest, but the configuration may fail if Vulkan isn't installed.
[2025-09-15T15:51:34] codex
**Exploring Repository for Testing**

I can set options to build tests without needing Vulkan. I’ll structure CMake with ENABLE_VULKAN set to OFF by default, which will help with building unit tests, though acceptance requires a Vulkan app. The user wants us to spend 80% of our time coding and to commit often. I might need to use GITHUB_TOKEN for access and check if they want us to create a new repository. I should check the current path, /Users/.../vulkano_codex, and run git status to explore the repository.
[2025-09-15T15:51:34] codex

I’m going to explore the repo, check for an existing .agent/TODO.md, and review current files to plan the scaffold and commits. Then I’ll initialize CMake project structure and add the core skeleton before iterating.
[2025-09-15T15:51:34] exec bash -lc 'ls -la' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:51:34] bash -lc 'ls -la' succeeded in 24ms:
total 104
drwxr-xr-x@ 19 bruno  staff    608 15 Sep 16:50 .
drwxr-xr-x@ 27 bruno  staff    864 15 Sep 16:39 ..
drwxr-xr-x@  3 bruno  staff     96 15 Sep 16:42 .agent
-rw-r--r--@  1 bruno  staff    404 15 Sep 16:44 .clang-format
drwxr-xr-x@ 14 bruno  staff    448 15 Sep 16:50 .git
-rw-r--r--@  1 bruno  staff    466 15 Sep 16:46 .gitignore
drwxr-xr-x   4 bruno  staff    128 15 Sep 16:50 .idea
-rw-r--r--@  1 bruno  staff  11798 15 Sep 16:40 AGENTS.md
drwxr-xr-x@  5 bruno  staff    160 15 Sep 16:44 app
drwxr-xr-x@ 14 bruno  staff    448 15 Sep 16:44 build
drwxr-xr-x  10 bruno  staff    320 15 Sep 16:43 cmake-build-debug
-rw-r--r--@  1 bruno  staff   1365 15 Sep 16:43 CMakeLists.txt
drwxr-xr-x@  4 bruno  staff    128 15 Sep 16:43 include
-rw-r--r--@  1 bruno  staff   1064 15 Sep 10:59 LICENSE
-rw-r--r--   1 bruno  staff     71 15 Sep 16:49 NOTES.md
-rw-r--r--   1 bruno  staff  13803 15 Sep 16:51 output.md
-rw-r--r--@  1 bruno  staff    779 15 Sep 16:44 README.md
drwxr-xr-x@  5 bruno  staff    160 15 Sep 16:43 src
drwxr-xr-x@  5 bruno  staff    160 15 Sep 16:44 tests
[2025-09-15T15:51:34] tokens used: 6,045

[2025-09-15T15:51:36] thinking

**Listing project directories**

I’m checking an existing repository, and it has a CMakeLists.txt file along with directories for include, src, app, and tests. It’s great that everything is organized. I’ll list these directories for a clearer overview, which makes it easier to understand the project structure. This will help in identifying what each part of the repository is used for. It seems straightforward enough! Let's move forward with listing them.
[2025-09-15T15:51:40] exec bash -lc "ls -la app && echo '---' && ls -la include && echo '---' && ls -la src && echo '---' && ls -la tests && echo '---' && sed -n '1,200p' CMakeLists.txt && echo '---' && sed -n '1,200p' README.md && echo '---' && sed -n '1,200p' .agent/TODO.md || true" in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:51:40] bash -lc "ls -la app && echo '---' && ls -la include && echo '---' && ls -la src && echo '---' && ls -la tests && echo '---' && sed -n '1,200p' CMakeLists.txt && echo '---' && sed -n '1,200p' README.md && echo '---' && sed -n '1,200p' .agent/TODO.md || true" succeeded in 54ms:
total 16
drwxr-xr-x@  5 bruno  staff  160 15 Sep 16:44 .
drwxr-xr-x@ 19 bruno  staff  608 15 Sep 16:50 ..
drwxr-xr-x@  3 bruno  staff   96 15 Sep 16:44 bin
-rw-r--r--@  1 bruno  staff  552 15 Sep 16:43 CMakeLists.txt
-rw-r--r--@  1 bruno  staff  702 15 Sep 16:45 main.cpp
---
total 8
drwxr-xr-x@  4 bruno  staff  128 15 Sep 16:43 .
drwxr-xr-x@ 19 bruno  staff  608 15 Sep 16:50 ..
-rw-r--r--@  1 bruno  staff  315 15 Sep 16:43 CMakeLists.txt
drwxr-xr-x@  4 bruno  staff  128 15 Sep 16:43 vulkano
---
total 24
drwxr-xr-x@  5 bruno  staff   160 15 Sep 16:43 .
drwxr-xr-x@ 19 bruno  staff   608 15 Sep 16:50 ..
-rw-r--r--@  1 bruno  staff  1420 15 Sep 16:45 app.cpp
-rw-r--r--@  1 bruno  staff   449 15 Sep 16:43 CMakeLists.txt
-rw-r--r--@  1 bruno  staff  2174 15 Sep 16:43 window.cpp
---
[2025-09-15T15:51:40] tokens used: 12,762

[2025-09-15T15:51:41] thinking

I'm excited that the repository is already set up with window.cpp and app.cpp files. Now, I need to get to work on opening the include headers. This seems like a logical next step to continue the development process. I'm curious to see what specific functionalities are included in those headers! It feels like moving into an interesting phase of the project. Let's see what I can do with the setup so far!
[2025-09-15T15:51:48] exec bash -lc "sed -n '1,200p' include/CMakeLists.txt && echo '---' && rg -n \"#include\" -n include -S && echo '---' && sed -n '1,200p' include/vulkano/window.hpp && echo '---' && sed -n '1,200p' src/window.cpp && echo '---' && sed -n '1,200p' src/app.cpp && echo '---' && sed -n '1,200p' app/main.cpp && echo '---' && sed -n '1,200p' app/CMakeLists.txt && echo '---' && sed -n '1,200p' src/CMakeLists.txt && echo '---' && sed -n '1,200p' tests/CMakeLists.txt && echo '---' && sed -n '1,200p' tests/sanity_tests.cpp && echo '---' && sed -n '1,200p' tests/tests_main.cpp" in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:51:48] bash -lc "sed -n '1,200p' include/CMakeLists.txt && echo '---' && rg -n \"#include\" -n include -S && echo '---' && sed -n '1,200p' include/vulkano/window.hpp && echo '---' && sed -n '1,200p' src/window.cpp && echo '---' && sed -n '1,200p' src/app.cpp && echo '---' && sed -n '1,200p' app/main.cpp && echo '---' && sed -n '1,200p' app/CMakeLists.txt && echo '---' && sed -n '1,200p' src/CMakeLists.txt && echo '---' && sed -n '1,200p' tests/CMakeLists.txt && echo '---' && sed -n '1,200p' tests/sanity_tests.cpp && echo '---' && sed -n '1,200p' tests/tests_main.cpp" succeeded in 70ms:
add_library(vulkano_headers INTERFACE)

target_sources(vulkano_headers
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            vulkano/app.hpp
            vulkano/window.hpp
)

target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

---
include/vulkano/window.hpp:3:#include <cstdint>
include/vulkano/window.hpp:4:#include <memory>
include/vulkano/window.hpp:5:#include <string>
include/vulkano/app.hpp:3:#include <cstdint>
include/vulkano/app.hpp:4:#include <memory>
include/vulkano/app.hpp:5:#include <string>
[2025-09-15T15:51:48] tokens used: 14,816
[2025-09-15T15:52:05] exec bash -lc "sed -n '1,200p' include/vulkano/app.hpp && echo '---' && sed -n '1,300p' src/app.cpp" in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:52:05] bash -lc "sed -n '1,200p' include/vulkano/app.hpp && echo '---' && sed -n '1,300p' src/app.cpp" succeeded in 33ms:
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace vulkano {

class Window;

struct AppConfig {
    std::string title;
    std::uint32_t width {1280};
    std::uint32_t height {720};
    bool enable_vulkan {true};
};

class Application final {
public:
    explicit Application(const AppConfig& config);
[2025-09-15T15:52:05] tokens used: 17,060

[2025-09-15T15:52:13] thinking

**Considering project setup**

I’m starting with a skeleton for the project, and I'm noticing that AGENTS.md had strict rules regarding code structure. It uses angle brackets for includes and curly braces, which I'm seeing in the current code for loops. There’s also '#pragma once' which is good. I realize we need to implement the full Vulkan pipeline, and that sounds big! I plan to iterate and commit often. It’s essential to ensure the code compiles and runs before submitting any changes. I'll try to configure and build to achieve the initial passing state with cmake.
[2025-09-15T15:52:16] codex

I’ll configure and build the current skeleton to ensure it compiles and runs, then iterate.
[2025-09-15T15:52:23] exec bash -lc 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j && ./app/bin/Debug/vulkano_app & sleep 1; pkill -f vulkano_app || true && ctest --test-dir build -C Debug --output-on-failure' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:52:24] bash -lc 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j && ./app/bin/Debug/vulkano_app & sleep 1; pkill -f vulkano_app || true && ctest --test-dir build -C Debug --output-on-failure' succeeded in 1.42s:
-- Including Cocoa support
CMake Deprecation Warning at build/_deps/glm-src/CMakeLists.txt:1 (cmake_minimum_required):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


CMake Deprecation Warning at build/_deps/glm-src/CMakeLists.txt:2 (cmake_policy):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


-- GLM: Version 1.0.1
[2025-09-15T15:52:24] tokens used: 18,036

[2025-09-15T15:52:28] thinking

**Planning next steps**

Great, the build and tests have passed! The app launch didn’t last long and was terminated. Now, I'm gearing up to implement Vulkan instance, device, swapchain, and integrate ImGui. I need to remember to make frequent commits along the way. Let's check the current baseline and make a commit since there's already a .git repository set up. It'll keep everything organized and manageable as I move forward!
[2025-09-15T15:52:29] exec bash -lc 'git status --porcelain && git log --oneline -n 3' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:52:29] bash -lc 'git status --porcelain && git log --oneline -n 3' succeeded in 53ms:
?? .idea/
?? AGENTS.md
?? NOTES.md
?? output.md
c31c443 chore: update .agent/TODO progress (skeleton complete)
973181c chore: add .agent/TODO with plan and next steps
0281ecd test: add Catch2 test scaffold and basic sanity test
[2025-09-15T15:52:29] tokens used: 18,727

[2025-09-15T15:52:32] thinking

**Evaluating files for commit**

I’m looking at some untracked files: .idea, AGENTS.md, NOTES.md, and output.md. I think I should consider adding AGENTS.md since it contains repo-specific instructions, so that might be alright to commit. However, I know that .idea should be ignored, and it looks like there’s already a .gitignore in place, so I’ll need to check that. Let’s ensure I'm not committing anything unnecessary!
[2025-09-15T15:52:35] exec bash -lc "sed -n '1,200p' .gitignore" in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:52:35] bash -lc "sed -n '1,200p' .gitignore" succeeded in 26ms:
# Prerequisites
*.d

# Compiled Object files
*.slo
*.lo
*.o
*.obj

# Precompiled Headers
*.gch
*.pch

# Linker files
*.ilk

# Debugger Files
*.pdb

# Compiled Dynamic libraries
[2025-09-15T15:52:35] tokens used: 18,909

[2025-09-15T15:52:45] thinking

**Updating project elements**

I’m thinking about adding `.idea` to the `.gitignore` file to keep it out of the repo. I’ll also include `AGENTS.md` and possibly `NOTES.md`, while `output.md` may not be necessary since it’s just a chat transcript. The next steps involve implementing Vulkan scaffolding classes like Instance, Device, and Surface, starting with adding the Vulkan instance and surface. The focus first is on clearing the screen with a solid color before integrating the UI and other features.
**Implementing Vulkan on macOS**

On macOS, I need to include MoltenVK and make sure we detect the platform properly. When using FetchContent for ImGui, I can add docking as an option and implement the backends. Since the ImGui Vulkan backend requires Vulkan, if Vulkan isn't found, I want to allow the app to build with `enable_vulkan=false`. I'll adjust the Application class to check if Vulkan is enabled and then initialize it for the rendering loop; otherwise, it will run a window-only loop. I need to ensure that Vulkan code is properly guarded to compile only when necessary.
**Designing Vulkan classes**

I'm planning out some classes for Vulkan. First up is `VulkanContext`, which will handle everything from instances to sync objects. I’ll create a wrapper for ImGui and Vulkan/GLFW in `imgui_layer.hpp`. Then there's the mesh structure to manage vertices and indices, and a factory for generating icospheres. For shaders, I’ll implement simple SPIR-V loading. To keep things manageable for the first commit, I’ll focus on getting the `VulkanContext` to at least clear the screen and validate the build, keeping in mind that Vulkan might not be available on this Mac.
**Wrapping includes and organizing compilation**

I need to ensure that source files compile smoothly, so includes should be wrapped with `ifdef` macros to prevent errors. Specifically, I'll wrap `<vulkan/vulkan.h>` in `#if VULKANO_HAS_VULKAN` and provide empty stubs if Vulkan isn't found. It seems simpler to split Vulkan source files and conditionally add them in CMake when Vulkan is available. Later, I’ll integrate ImGui with FetchContent but only if Vulkan is present. I'll also implement new headers for Vulkan and work on the icosphere mesh generator and its unit test, as it has no Vulkan dependencies.
**Implementing tests and updates**

I'm focusing on spending some time coding while also addressing tests. First, I need to update `src/CMakeLists` to include the new CPP files. I’ll start with the mesh and icosphere generator, then modify the Application to use a graphics placeholder when Vulkan isn't enabled—no changes there yet. After that, I'll commit everything. In the testing phase, I plan to create `tests/icosphere_tests.cpp` to check the vertex count and ensure normal lengths are correct. Remembering AGENTS.md, I’ll only have declarations in headers and implementations in .cpp files. I'll also update the TODO list to track our progress.
[2025-09-15T15:53:12] codex
**Implementing Vulkan on macOS**

On macOS, I need to include MoltenVK and make sure we detect the platform properly. When using FetchContent for ImGui, I can add docking as an option and implement the backends. Since the ImGui Vulkan backend requires Vulkan, if Vulkan isn't found, I want to allow the app to build with `enable_vulkan=false`. I'll adjust the Application class to check if Vulkan is enabled and then initialize it for the rendering loop; otherwise, it will run a window-only loop. I need to ensure that Vulkan code is properly guarded to compile only when necessary.
[2025-09-15T15:53:12] codex
**Designing Vulkan classes**

I'm planning out some classes for Vulkan. First up is `VulkanContext`, which will handle everything from instances to sync objects. I’ll create a wrapper for ImGui and Vulkan/GLFW in `imgui_layer.hpp`. Then there's the mesh structure to manage vertices and indices, and a factory for generating icospheres. For shaders, I’ll implement simple SPIR-V loading. To keep things manageable for the first commit, I’ll focus on getting the `VulkanContext` to at least clear the screen and validate the build, keeping in mind that Vulkan might not be available on this Mac.
[2025-09-15T15:53:12] codex
**Wrapping includes and organizing compilation**

I need to ensure that source files compile smoothly, so includes should be wrapped with `ifdef` macros to prevent errors. Specifically, I'll wrap `<vulkan/vulkan.h>` in `#if VULKANO_HAS_VULKAN` and provide empty stubs if Vulkan isn't found. It seems simpler to split Vulkan source files and conditionally add them in CMake when Vulkan is available. Later, I’ll integrate ImGui with FetchContent but only if Vulkan is present. I'll also implement new headers for Vulkan and work on the icosphere mesh generator and its unit test, as it has no Vulkan dependencies.
[2025-09-15T15:53:12] codex
**Implementing tests and updates**

I'm focusing on spending some time coding while also addressing tests. First, I need to update `src/CMakeLists` to include the new CPP files. I’ll start with the mesh and icosphere generator, then modify the Application to use a graphics placeholder when Vulkan isn't enabled—no changes there yet. After that, I'll commit everything. In the testing phase, I plan to create `tests/icosphere_tests.cpp` to check the vertex count and ensure normal lengths are correct. Remembering AGENTS.md, I’ll only have declarations in headers and implementations in .cpp files. I'll also update the TODO list to track our progress.
[2025-09-15T15:53:12] codex

Next, I’ll add the mesh and icosphere generator (CPU-side), unit tests for it, and prepare Vulkan scaffolding headers. This keeps builds passing without Vulkan while laying groundwork.
[2025-09-15T15:53:12] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/.gitignore
@@ -51 +51,2 @@
 app/bin/
+.idea/
[2025-09-15T15:53:12] apply_patch(auto_approved=true) exited 0 in 19ms:
Success. Updated the following files:
M .gitignore
[2025-09-15T15:53:12] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/

[2025-09-15T15:53:12] tokens used: 26,479
[2025-09-15T15:53:12] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/

[2025-09-15T15:53:20] apply_patch auto_approved=true:
A /Users/bruno/Documents/Development/bruno/vulkano_codex/include/vulkano/mesh.hpp
#pragma once

#include <cstdint>
#include <vector>

namespace vulkano {

struct Vertex final {
    float position[3];
    float normal[3];
    float tangent[3];
    float bitangent[3];
    float texcoord[2];
};

struct Mesh final {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace vulkano

[2025-09-15T15:53:20] apply_patch(auto_approved=true) exited 0 in 48ms:
Success. Updated the following files:
A include/vulkano/mesh.hpp
[2025-09-15T15:53:20] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+

[2025-09-15T15:53:20] tokens used: 26,860
[2025-09-15T15:53:20] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+

[2025-09-15T15:53:34] apply_patch auto_approved=true:
A /Users/bruno/Documents/Development/bruno/vulkano_codex/include/vulkano/icosphere.hpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace vulkano {

struct Mesh;

class IcosphereBuilder final {
public:
    IcosphereBuilder() = default;
    ~IcosphereBuilder() = default;

    IcosphereBuilder(const IcosphereBuilder&) = delete;
    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;

    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
};

} // namespace vulkano

[2025-09-15T15:53:34] apply_patch(auto_approved=true) exited 0 in 71ms:
Success. Updated the following files:
A include/vulkano/icosphere.hpp
[2025-09-15T15:53:34] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+

[2025-09-15T15:53:34] tokens used: 27,388
[2025-09-15T15:53:34] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+

[2025-09-15T15:55:24] apply_patch auto_approved=true:
A /Users/bruno/Documents/Development/bruno/vulkano_codex/src/icosphere.cpp
#include <vulkano/icosphere.hpp>
#include <vulkano/mesh.hpp>

#include <array>
#include <cmath>
#include <unordered_map>

namespace vulkano {

namespace {

struct Vec3 final {
    float x {};
    float y {};
    float z {};
};

struct Vec2 final {
    float u {};
    float v {};
};

static inline Vec3 normalize(const Vec3 v) noexcept {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len == 0.0F) {
        return Vec3 {0.0F, 0.0F, 0.0F};
    }
    const float inv = 1.0F / len;
    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
}

static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 mul(const Vec3 a, const float s) noexcept {
    return Vec3 {a.x * s, a.y * s, a.z * s};
}

static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static inline float dot(const Vec3 a, const Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec2 compute_uv(const Vec3 n) noexcept {
    // Longitude/latitude mapping
    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
    return Vec2 {u, v};
}

struct Key final {
    std::uint32_t a {};
    std::uint32_t b {};
};

struct KeyHash final {
    std::size_t operator()(const Key& k) const noexcept {
        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
    }
};

struct KeyEq final {
    bool operator()(const Key& x, const Key& y) const noexcept {
        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
    }
};

} // namespace

std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
    auto mesh = std::make_unique<Mesh>();
    mesh->vertices.clear();
    mesh->indices.clear();

    // Icosahedron vertices
    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
    std::vector<Vec3> positions {
        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
    };

    for (auto& p : positions) {
        p = normalize(p);
    }

    std::vector<std::uint32_t> faces {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
    };

    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
        const Key key {a < b ? a : b, a < b ? b : a};
        const auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }
        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
        positions.push_back(mid);
        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
        cache.emplace(key, index);
        return index;
    };

    for (std::uint32_t i = 0; i < subdivisions; ++i) {
        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
        std::vector<std::uint32_t> new_faces {};
        new_faces.reserve(faces.size() * 4U);
        for (std::size_t f = 0; f < faces.size(); f += 3) {
            const std::uint32_t i0 = faces[f + 0];
            const std::uint32_t i1 = faces[f + 1];
            const std::uint32_t i2 = faces[f + 2];
            const std::uint32_t m0 = midpoint(i0, i1, cache);
            const std::uint32_t m1 = midpoint(i1, i2, cache);
            const std::uint32_t m2 = midpoint(i2, i0, cache);
            // Create 4 new faces
            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
        }
        faces.swap(new_faces);
    }

    // Convert to Mesh with normals/tangents/bitangents and UVs
    mesh->vertices.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const Vec3 n = normalize(positions[i]);
        const Vec2 uv = compute_uv(n);
        Vertex v {};
        v.position[0] = n.x;
        v.position[1] = n.y;
        v.position[2] = n.z;
        v.normal[0] = n.x;
        v.normal[1] = n.y;
        v.normal[2] = n.z;
        v.tangent[0] = 1.0F;
        v.tangent[1] = 0.0F;
        v.tangent[2] = 0.0F;
        v.bitangent[0] = 0.0F;
        v.bitangent[1] = 1.0F;
        v.bitangent[2] = 0.0F;
        v.texcoord[0] = uv.u;
        v.texcoord[1] = uv.v;
        mesh->vertices[i] = v;
    }

    mesh->indices = faces;

    // Compute tangents/bitangents per triangle (simple accumulation)
    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        const std::uint32_t i0 = mesh->indices[i + 0];
        const std::uint32_t i1 = mesh->indices[i + 1];
        const std::uint32_t i2 = mesh->indices[i + 2];

        const Vertex& v0 = mesh->vertices[i0];
        const Vertex& v1 = mesh->vertices[i1];
        const Vertex& v2 = mesh->vertices[i2];

        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};

        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};

        const float x1 = p1.x - p0.x;
        const float x2 = p2.x - p0.x;
        const float y1 = p1.y - p0.y;
        const float y2 = p2.y - p0.y;
        const float z1 = p1.z - p0.z;
        const float z2 = p2.z - p0.z;

        const float s1 = w1.u - w0.u;
        const float s2 = w2.u - w0.u;
        const float t1 = w1.v - w0.v;
        const float t2 = w2.v - w0.v;

        const float r = (s1 * t2 - s2 * t1);
        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};

        tan1[i0] = add(tan1[i0], sdir);
        tan1[i1] = add(tan1[i1], sdir);
        tan1[i2] = add(tan1[i2], sdir);
        tan2[i0] = add(tan2[i0], tdir);
        tan2[i1] = add(tan2[i1], tdir);
        tan2[i2] = add(tan2[i2], tdir);
    }

    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
        Vec3 t = tan1[i];
        // Gram-Schmidt orthogonalize
        t = sub(t, mul(n, dot(n, t)));
        t = normalize(t);
        const Vec3 b = cross(n, t);
        mesh->vertices[i].tangent[0] = t.x;
        mesh->vertices[i].tangent[1] = t.y;
        mesh->vertices[i].tangent[2] = t.z;
        mesh->vertices[i].bitangent[0] = b.x;
        mesh->vertices[i].bitangent[1] = b.y;
        mesh->vertices[i].bitangent[2] = b.z;
    }

    return mesh;
}

} // namespace vulkano

[2025-09-15T15:55:24] apply_patch(auto_approved=true) exited 0 in 61ms:
Success. Updated the following files:
A src/icosphere.cpp
[2025-09-15T15:55:24] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+

[2025-09-15T15:55:24] tokens used: 30,994
[2025-09-15T15:55:24] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+

[2025-09-15T15:55:28] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/src/CMakeLists.txt
@@ -3,2 +3,3 @@
     window.cpp
+    icosphere.cpp
 )
@@ -20,2 +21 @@
 endif()
-
[2025-09-15T15:55:28] apply_patch(auto_approved=true) exited 0 in 56ms:
Success. Updated the following files:
M src/CMakeLists.txt
[2025-09-15T15:55:28] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+

[2025-09-15T15:55:28] tokens used: 34,449
[2025-09-15T15:55:28] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+

[2025-09-15T15:55:40] apply_patch auto_approved=true:
A /Users/bruno/Documents/Development/bruno/vulkano_codex/tests/icosphere_tests.cpp
#include <catch2/catch_all.hpp>

#include <vulkano/icosphere.hpp>
#include <vulkano/mesh.hpp>

#include <cmath>

static float length3(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

TEST_CASE("Icosphere vertex count and normals") {
    vulkano::IcosphereBuilder builder {};

    for (std::uint32_t s = 0; s <= 3; ++s) {
        auto mesh = builder.build(s);
        REQUIRE(mesh != nullptr);
        // Faces grow by factor 4 each subdivision starting from 20 triangles
        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
        // Normals should be ~unit length
        for (const auto& v : mesh->vertices) {
            const float len = length3(v.normal);
            REQUIRE(len == Approx(1.0F).margin(1e-3F));
        }
    }
}

[2025-09-15T15:55:40] apply_patch(auto_approved=true) exited 0 in 79ms:
Success. Updated the following files:
A tests/icosphere_tests.cpp
[2025-09-15T15:55:40] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:55:40] tokens used: 40,091
[2025-09-15T15:55:40] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:55:43] exec bash -lc 'git add -A && git commit -m "feat(mesh): add CPU icosphere generator with tangents/bitangents; unit tests"' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:55:43] bash -lc 'git add -A && git commit -m "feat(mesh): add CPU icosphere generator with tangents/bitangents; unit tests"' succeeded in 71ms:
[personal/codex/second_try b587217] feat(mesh): add CPU icosphere generator with tangents/bitangents; unit tests
 9 files changed, 3526 insertions(+), 1 deletion(-)
 create mode 100644 AGENTS.md
 create mode 100644 NOTES.md
 create mode 100644 include/vulkano/icosphere.hpp
 create mode 100644 include/vulkano/mesh.hpp
 create mode 100644 output.md
 create mode 100644 src/icosphere.cpp
 create mode 100644 tests/icosphere_tests.cpp
[2025-09-15T15:55:43] tokens used: 40,578
[2025-09-15T15:55:43] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:55:46] exec bash -lc 'cmake --build build -j && ctest --test-dir build -C Debug --output-on-failure' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:55:49] bash -lc 'cmake --build build -j && ctest --test-dir build -C Debug --output-on-failure' succeeded in 2.79s:
-- Including Cocoa support
CMake Deprecation Warning at build/_deps/glm-src/CMakeLists.txt:1 (cmake_minimum_required):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


CMake Deprecation Warning at build/_deps/glm-src/CMakeLists.txt:2 (cmake_policy):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


-- GLM: Version 1.0.1
[2025-09-15T15:55:49] tokens used: 41,387
[2025-09-15T15:55:49] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+


[2025-09-15T15:55:51] thinking

**Preparing VulkanContext implementation**

I'm focusing on preparing the VulkanContext header and adding a placeholder cpp file under specific conditions. I need to include `vulkano/vulkan_context.hpp` and `src/vulkan_context.cpp`, making sure that these files are only added when `Vulkan_FOUND` is true. Additionally, I want to modify the Application so it can optionally initialize Vulkan based on whether `config.enable_vulkan` is set and the relevant macro is defined. Let's get started on that implementation!
[2025-09-15T15:55:57] apply_patch auto_approved=true:
A /Users/bruno/Documents/Development/bruno/vulkano_codex/include/vulkano/vulkan_context.hpp
#pragma once

#include <cstdint>
#include <memory>

namespace vulkano {

class Window;

class VulkanContext final {
public:
    VulkanContext() = delete;
    explicit VulkanContext(const Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;

    void wait_idle() const noexcept;
    void draw_frame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vulkano

[2025-09-15T15:55:57] apply_patch(auto_approved=true) exited 0 in 90ms:
Success. Updated the following files:
A include/vulkano/vulkan_context.hpp
[2025-09-15T15:55:57] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:55:57] tokens used: 42,303
[2025-09-15T15:55:57] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:56:57] apply_patch auto_approved=true:
A /Users/bruno/Documents/Development/bruno/vulkano_codex/src/vulkan_context.cpp
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#if VULKANO_HAS_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <array>
#include <stdexcept>

namespace vulkano {

struct VulkanContext::Impl final {
    VkInstance instance {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
    uint32_t graphics_queue_family {0U};

    Impl() = default;
    ~Impl() = default;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
    // Simple stderr print would go here in future
    (void)pCallbackData;
    return VK_FALSE;
}

static void create_instance(VkInstance* instance) {
    VkApplicationInfo app_info {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vulkano";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "vulkano";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t ext_count = 0U;
    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
    std::array<const char*, 4> extensions {};
    for (uint32_t i = 0; i < ext_count; ++i) {
        extensions[i] = exts[i];
    }

    VkInstanceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = ext_count;
    ci.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    ci.enabledLayerCount = 1U;
    ci.ppEnabledLayerNames = layers;
#endif

    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan instance"};
    }
}

VulkanContext::VulkanContext(const Window& window)
    : impl {std::make_unique<Impl>()} {
    create_instance(&impl->instance);
    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
    if (glfw_win == nullptr) {
        throw std::runtime_error {"Invalid GLFW window handle"};
    }
    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan surface"};
    }
    // Physical device selection (very naive)
    uint32_t count = 0U;
    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
    if (count == 0U) {
        throw std::runtime_error {"No Vulkan physical devices found"};
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
    impl->physical_device = devices[0];

    // Queue family selection
    uint32_t qcount = 0U;
    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
    uint32_t gfx_index = UINT32_MAX;
    for (uint32_t i = 0; i < qcount; ++i) {
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            gfx_index = i;
            break;
        }
    }
    if (gfx_index == UINT32_MAX) {
        throw std::runtime_error {"No graphics queue family found"};
    }
    impl->graphics_queue_family = gfx_index;

    const float priority = 1.0F;
    VkDeviceQueueCreateInfo qci {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = gfx_index;
    qci.queueCount = 1U;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1U;
    dci.pQueueCreateInfos = &qci;

    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create logical device"};
    }
    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
}

VulkanContext::~VulkanContext() {
    if (impl != nullptr) {
        if (impl->device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(impl->device);
        }
        if (impl->device != VK_NULL_HANDLE) {
            vkDestroyDevice(impl->device, nullptr);
        }
        if (impl->surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
        }
        if (impl->instance != VK_NULL_HANDLE) {
            vkDestroyInstance(impl->instance, nullptr);
        }
    }
}

VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;

void VulkanContext::wait_idle() const noexcept {
    if (impl->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(impl->device);
    }
}

void VulkanContext::draw_frame() {
    // Placeholder: swapchain/present will be added. No-op for now.
}

} // namespace vulkano

#else

namespace vulkano {

class Window;

VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
}

VulkanContext::~VulkanContext() = default;
VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
void VulkanContext::wait_idle() const noexcept {
}
void VulkanContext::draw_frame() {
}

} // namespace vulkano

#endif

[2025-09-15T15:56:58] apply_patch(auto_approved=true) exited 0 in 97ms:
Success. Updated the following files:
A src/vulkan_context.cpp
[2025-09-15T15:56:58] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:56:58] tokens used: 44,452
[2025-09-15T15:56:58] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:02] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/include/CMakeLists.txt
@@ -9,2 +9,5 @@
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
@@ -12,2 +15 @@
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
[2025-09-15T15:57:02] apply_patch(auto_approved=true) exited 0 in 93ms:
Success. Updated the following files:
M include/CMakeLists.txt
[2025-09-15T15:57:02] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:02] tokens used: 47,627
[2025-09-15T15:57:03] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..9e2b8d151c65f65ea2fd17791a52bc3a61f16009
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -18,4 +19,3 @@
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:09] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/src/CMakeLists.txt
@@ -18,2 +18,3 @@
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
[2025-09-15T15:57:09] apply_patch(auto_approved=true) exited 0 in 104ms:
Success. Updated the following files:
M src/CMakeLists.txt
[2025-09-15T15:57:09] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:09] tokens used: 47,921
[2025-09-15T15:57:09] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:15] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/src/app.cpp
@@ -2,2 +2,3 @@
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
@@ -27,5 +28,15 @@
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
[2025-09-15T15:57:15] apply_patch(auto_approved=true) exited 0 in 93ms:
Success. Updated the following files:
M src/app.cpp
[2025-09-15T15:57:15] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:15] tokens used: 48,481
[2025-09-15T15:57:15] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:18] exec bash -lc 'git add -A && git commit -m "feat(vulkan): add VulkanContext stub (conditional); wire into Application when available"' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:57:18] bash -lc 'git add -A && git commit -m "feat(vulkan): add VulkanContext stub (conditional); wire into Application when available"' succeeded in 55ms:
[personal/codex/second_try d2b4d2e] feat(vulkan): add VulkanContext stub (conditional); wire into Application when available
 6 files changed, 6653 insertions(+), 2 deletions(-)
 create mode 100644 include/vulkano/vulkan_context.hpp
 create mode 100644 src/vulkan_context.cpp
[2025-09-15T15:57:18] tokens used: 49,000
[2025-09-15T15:57:18] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:21] exec bash -lc 'cmake --build build -j && ctest --test-dir build -C Debug --output-on-failure' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:57:23] bash -lc 'cmake --build build -j && ctest --test-dir build -C Debug --output-on-failure' exited 2 in 1.93s:
-- Including Cocoa support
CMake Deprecation Warning at build/_deps/glm-src/CMakeLists.txt:1 (cmake_minimum_required):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


CMake Deprecation Warning at build/_deps/glm-src/CMakeLists.txt:2 (cmake_policy):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


-- GLM: Version 1.0.1
[2025-09-15T15:57:23] tokens used: 49,281
[2025-09-15T15:57:23] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..762599ac965b4abf78af201a25f53525982fcb38
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
+
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+


[2025-09-15T15:57:32] thinking

**Analyzing compilation issues**

I’m noticing that the compiler is failing because, in the Vulkan_FOUND=0 environment, it still attempts to compile `src/vulkan_context.cpp`. We added that file to compile conditionally when Vulkan_FOUND is true, so that's puzzling. There’s some confusion with the preprocessor and whether it recognizes `std::vector`, which could lead to errors because it’s trying to use it without including the necessary header. The undefined template error suggests that VULKANO_HAS_VULKAN might be treated as true when it shouldn't be.
**Debugging CMake compilation**

In CMake, I set `target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)` when Vulkan isn't found, but it seems we're still building `vulkan_context.cpp`, which implies `Vulkan_FOUND` is true. I wonder if `find_package(Vulkan QUIET)` is picking up MoltenVK headers on macOS, perhaps from Homebrew. We're missing the `<vector>` include at the top of `vulkan_context.cpp` when `VULKANO_HAS_VULKAN` is set to 1, even though we're using `std::vector`. I should patch this to add the `<vector>` include.
[2025-09-15T15:57:38] codex
**Debugging CMake compilation**

In CMake, I set `target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)` when Vulkan isn't found, but it seems we're still building `vulkan_context.cpp`, which implies `Vulkan_FOUND` is true. I wonder if `find_package(Vulkan QUIET)` is picking up MoltenVK headers on macOS, perhaps from Homebrew. We're missing the `<vector>` include at the top of `vulkan_context.cpp` when `VULKANO_HAS_VULKAN` is set to 1, even though we're using `std::vector`. I should patch this to add the `<vector>` include.
[2025-09-15T15:57:44] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/src/vulkan_context.cpp
@@ -8,2 +8,3 @@
 #include <array>
+#include <vector>
 #include <stdexcept>
@@ -172,2 +173 @@
 #endif
-
[2025-09-15T15:57:44] apply_patch(auto_approved=true) exited 0 in 114ms:
Success. Updated the following files:
M src/vulkan_context.cpp
[2025-09-15T15:57:44] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..11d9dd5186fb9c4bd3f932a6849a391c1398f1b5
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <vector>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:44] tokens used: 50,612
[2025-09-15T15:57:44] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..11d9dd5186fb9c4bd3f932a6849a391c1398f1b5
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <vector>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:46] exec bash -lc 'cmake --build build -j && ctest --test-dir build -C Debug --output-on-failure' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:57:47] bash -lc 'cmake --build build -j && ctest --test-dir build -C Debug --output-on-failure' succeeded in 1.27s:
[  2%] Built target glm
[ 17%] Built target glfw
[ 17%] Building CXX object src/CMakeFiles/vulkano.dir/vulkan_context.cpp.o
[ 91%] Built target Catch2
[ 92%] Built target Catch2WithMain
/Users/bruno/Documents/Development/bruno/vulkano_codex/src/vulkan_context.cpp:26:39: warning: unused function 'debug_cb' [-Wunused-function]
   26 | static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
      |                                       ^~~~~~~~
1 warning generated.
[ 93%] Linking CXX static library libvulkano.a
[ 95%] Built target vulkano
[ 95%] Linking CXX executable /Users/bruno/Documents/Development/bruno/vulkano_codex/app/bin/Debug/vulkano_app
[ 96%] Linking CXX executable vulkano_tests
[ 97%] Built target vulkano_app
[100%] Built target vulkano_tests
Test project /Users/bruno/Documents/Development/bruno/vulkano_codex/build
    Start 1: vulkano_tests
1/1 Test #1: vulkano_tests ....................   Passed    0.31 sec

100% tests passed, 0 tests failed out of 1
[2025-09-15T15:57:47] tokens used: 50,801
[2025-09-15T15:57:48] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..11d9dd5186fb9c4bd3f932a6849a391c1398f1b5
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <vector>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:57:51] exec bash -lc 'git add -A && git commit -m "fix(vulkan): include <vector> for VulkanContext; build passes on macOS"' in /Users/bruno/Documents/Development/bruno/vulkano_codex
[2025-09-15T15:57:51] bash -lc 'git add -A && git commit -m "fix(vulkan): include <vector> for VulkanContext; build passes on macOS"' succeeded in 67ms:
[personal/codex/second_try bc31f40] fix(vulkan): include <vector> for VulkanContext; build passes on macOS
 2 files changed, 3145 insertions(+), 1 deletion(-)
[2025-09-15T15:57:51] tokens used: 51,262
[2025-09-15T15:57:51] turn diff:
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..11d9dd5186fb9c4bd3f932a6849a391c1398f1b5
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <vector>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+


[2025-09-15T15:57:54] thinking

**Planning integration steps**

I'm considering integrating ImGui next. I want to first make sure that the .agent/TODO.md file is up to date. I'll edit it to mark the icosphere task as done, and then I'll note that the next steps will be working on the Vulkan swapchain and integrating ImGui. It feels good to keep track of tasks like this so I can focus on what’s coming next!
[2025-09-15T15:58:00] apply_patch auto_approved=true:
M /Users/bruno/Documents/Development/bruno/vulkano_codex/.agent/TODO.md
@@ -26,5 +26,6 @@
 ## Next Iteration
-- [ ] Vulkan instance/device/swapchain, debug utils
+- [x] Icosphere mesh generation with tangents/bitangents
+- [x] Unit tests for icosphere counts and normals
+- [ ] Vulkan instance/device/swapchain, debug utils (basic clear)
 - [ ] ImGui integration (imgui_impl_glfw + imgui_impl_vulkan)
-- [ ] Icosphere mesh generation with tangents/bitangents
 
[2025-09-15T15:58:00] apply_patch(auto_approved=true) exited 0 in 105ms:
Success. Updated the following files:
M .agent/TODO.md
[2025-09-15T15:58:00] turn diff:
diff --git a/.agent/TODO.md b/.agent/TODO.md
index fa08dfb13dad01eb2da91e0a30472f409ed2e73b..b30d2619e80b8a439e630295a76a6bd8f97072d3
--- a/.agent/TODO.md
+++ b/.agent/TODO.md
@@ -24,9 +24,10 @@
 - [x] Basic unit test scaffold builds
 
 ## Next Iteration
-- [ ] Vulkan instance/device/swapchain, debug utils
+- [x] Icosphere mesh generation with tangents/bitangents
+- [x] Unit tests for icosphere counts and normals
+- [ ] Vulkan instance/device/swapchain, debug utils (basic clear)
 - [ ] ImGui integration (imgui_impl_glfw + imgui_impl_vulkan)
-- [ ] Icosphere mesh generation with tangents/bitangents
 
 ## Notes
 - All includes must use angle brackets.
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..11d9dd5186fb9c4bd3f932a6849a391c1398f1b5
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <vector>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:58:00] tokens used: 56,064
[2025-09-15T15:58:00] turn diff:
diff --git a/.agent/TODO.md b/.agent/TODO.md
index fa08dfb13dad01eb2da91e0a30472f409ed2e73b..b30d2619e80b8a439e630295a76a6bd8f97072d3
--- a/.agent/TODO.md
+++ b/.agent/TODO.md
@@ -24,9 +24,10 @@
 - [x] Basic unit test scaffold builds
 
 ## Next Iteration
-- [ ] Vulkan instance/device/swapchain, debug utils
+- [x] Icosphere mesh generation with tangents/bitangents
+- [x] Unit tests for icosphere counts and normals
+- [ ] Vulkan instance/device/swapchain, debug utils (basic clear)
 - [ ] ImGui integration (imgui_impl_glfw + imgui_impl_vulkan)
-- [ ] Icosphere mesh generation with tangents/bitangents
 
 ## Notes
 - All includes must use angle brackets.
diff --git a/.gitignore b/.gitignore
index 08082907e166289aa572746010a66bfd9566f95d..90fcff7f14bd4769bbcc137279b5a17eea9ddfb7
--- a/.gitignore
+++ b/.gitignore
@@ -49,3 +49,4 @@
 
 # App runtime binaries
 app/bin/
+.idea/
diff --git a/include/CMakeLists.txt b/include/CMakeLists.txt
index 1d0b6ac0af4831e6a26f6b01e36f895c347551ba..7f73b15e88179825c32e9b01d514bbc4116b7ef4
--- a/include/CMakeLists.txt
+++ b/include/CMakeLists.txt
@@ -7,7 +7,9 @@
         FILES
             vulkano/app.hpp
             vulkano/window.hpp
+            vulkano/mesh.hpp
+            vulkano/icosphere.hpp
+            vulkano/vulkan_context.hpp
 )
 
 target_include_directories(vulkano_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
-
diff --git a/include/vulkano/icosphere.hpp b/include/vulkano/icosphere.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..d5ac4b31925b09921af3d237646060cf254cfdc7
--- /dev/null
+++ b/include/vulkano/icosphere.hpp
@@ -0,0 +1,25 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+#include <vector>
+
+namespace vulkano {
+
+struct Mesh;
+
+class IcosphereBuilder final {
+public:
+    IcosphereBuilder() = default;
+    ~IcosphereBuilder() = default;
+
+    IcosphereBuilder(const IcosphereBuilder&) = delete;
+    IcosphereBuilder& operator=(const IcosphereBuilder&) = delete;
+    IcosphereBuilder(IcosphereBuilder&&) noexcept = default;
+    IcosphereBuilder& operator=(IcosphereBuilder&&) noexcept = default;
+
+    [[nodiscard]] std::unique_ptr<Mesh> build(std::uint32_t subdivisions) const;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/mesh.hpp b/include/vulkano/mesh.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..966477560a19a957d55c1cd2e5cd348cd8decf05
--- /dev/null
+++ b/include/vulkano/mesh.hpp
@@ -0,0 +1,22 @@
+#pragma once
+
+#include <cstdint>
+#include <vector>
+
+namespace vulkano {
+
+struct Vertex final {
+    float position[3];
+    float normal[3];
+    float tangent[3];
+    float bitangent[3];
+    float texcoord[2];
+};
+
+struct Mesh final {
+    std::vector<Vertex> vertices;
+    std::vector<std::uint32_t> indices;
+};
+
+} // namespace vulkano
+
diff --git a/include/vulkano/vulkan_context.hpp b/include/vulkano/vulkan_context.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..3df309ea6cd5251c49ded7fcc76a61a6e7722685
--- /dev/null
+++ b/include/vulkano/vulkan_context.hpp
@@ -0,0 +1,30 @@
+#pragma once
+
+#include <cstdint>
+#include <memory>
+
+namespace vulkano {
+
+class Window;
+
+class VulkanContext final {
+public:
+    VulkanContext() = delete;
+    explicit VulkanContext(const Window& window);
+    ~VulkanContext();
+
+    VulkanContext(const VulkanContext&) = delete;
+    VulkanContext& operator=(const VulkanContext&) = delete;
+    VulkanContext(VulkanContext&&) noexcept;
+    VulkanContext& operator=(VulkanContext&&) noexcept;
+
+    void wait_idle() const noexcept;
+    void draw_frame();
+
+private:
+    struct Impl;
+    std::unique_ptr<Impl> impl;
+};
+
+} // namespace vulkano
+
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
index 989a6f262d2cbfc89cae32d9bf2698ee34e92bcc..00e0900ef418dfddaf6535877b78b3267d055473
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -1,6 +1,7 @@
 add_library(vulkano STATIC
     app.cpp
     window.cpp
+    icosphere.cpp
 )
 
 target_link_libraries(vulkano
@@ -15,7 +16,7 @@
 if (Vulkan_FOUND)
     target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
+    target_sources(vulkano PRIVATE vulkan_context.cpp)
 else()
     target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
 endif()
-
diff --git a/src/app.cpp b/src/app.cpp
index c01ccd7862ed39cf453c7c225b370b47fe271ec8..b119a42382036efeb288e1f40fcb4901a28bd1f6
--- a/src/app.cpp
+++ b/src/app.cpp
@@ -1,5 +1,6 @@
 #include <vulkano/app.hpp>
 #include <vulkano/window.hpp>
+#include <vulkano/vulkan_context.hpp>
 
 #include <chrono>
 #include <thread>
@@ -25,9 +26,19 @@
 }
 
 void Application::main_loop() {
+    // Optional Vulkan context; created lazily if available and enabled
+    std::unique_ptr<VulkanContext> vk { };
+    (void)vk;
     while (!window->should_close()) {
         window->poll_events();
-        // Placeholder: rendering will be added in subsequent steps.
+#if VULKANO_HAS_VULKAN
+        if (!vk && true) {
+            vk = std::make_unique<VulkanContext>(*window);
+        }
+        if (vk) {
+            vk->draw_frame();
+        }
+#endif
         std::this_thread::sleep_for(std::chrono::milliseconds {16});
     }
 }
diff --git a/src/icosphere.cpp b/src/icosphere.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..c01c581a37130b22a83db3e435c15a133fb97cd5
--- /dev/null
+++ b/src/icosphere.cpp
@@ -0,0 +1,220 @@
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <array>
+#include <cmath>
+#include <unordered_map>
+
+namespace vulkano {
+
+namespace {
+
+struct Vec3 final {
+    float x {};
+    float y {};
+    float z {};
+};
+
+struct Vec2 final {
+    float u {};
+    float v {};
+};
+
+static inline Vec3 normalize(const Vec3 v) noexcept {
+    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
+    if (len == 0.0F) {
+        return Vec3 {0.0F, 0.0F, 0.0F};
+    }
+    const float inv = 1.0F / len;
+    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
+}
+
+static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
+}
+
+static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
+}
+
+static inline Vec3 mul(const Vec3 a, const float s) noexcept {
+    return Vec3 {a.x * s, a.y * s, a.z * s};
+}
+
+static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
+    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
+}
+
+static inline float dot(const Vec3 a, const Vec3 b) noexcept {
+    return a.x * b.x + a.y * b.y + a.z * b.z;
+}
+
+static inline Vec2 compute_uv(const Vec3 n) noexcept {
+    // Longitude/latitude mapping
+    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
+    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
+    return Vec2 {u, v};
+}
+
+struct Key final {
+    std::uint32_t a {};
+    std::uint32_t b {};
+};
+
+struct KeyHash final {
+    std::size_t operator()(const Key& k) const noexcept {
+        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
+    }
+};
+
+struct KeyEq final {
+    bool operator()(const Key& x, const Key& y) const noexcept {
+        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
+    }
+};
+
+} // namespace
+
+std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
+    auto mesh = std::make_unique<Mesh>();
+    mesh->vertices.clear();
+    mesh->indices.clear();
+
+    // Icosahedron vertices
+    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
+    std::vector<Vec3> positions {
+        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
+        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
+        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
+    };
+
+    for (auto& p : positions) {
+        p = normalize(p);
+    }
+
+    std::vector<std::uint32_t> faces {
+        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
+        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
+        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
+    };
+
+    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
+        const Key key {a < b ? a : b, a < b ? b : a};
+        const auto it = cache.find(key);
+        if (it != cache.end()) {
+            return it->second;
+        }
+        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
+        positions.push_back(mid);
+        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
+        cache.emplace(key, index);
+        return index;
+    };
+
+    for (std::uint32_t i = 0; i < subdivisions; ++i) {
+        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
+        std::vector<std::uint32_t> new_faces {};
+        new_faces.reserve(faces.size() * 4U);
+        for (std::size_t f = 0; f < faces.size(); f += 3) {
+            const std::uint32_t i0 = faces[f + 0];
+            const std::uint32_t i1 = faces[f + 1];
+            const std::uint32_t i2 = faces[f + 2];
+            const std::uint32_t m0 = midpoint(i0, i1, cache);
+            const std::uint32_t m1 = midpoint(i1, i2, cache);
+            const std::uint32_t m2 = midpoint(i2, i0, cache);
+            // Create 4 new faces
+            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
+        }
+        faces.swap(new_faces);
+    }
+
+    // Convert to Mesh with normals/tangents/bitangents and UVs
+    mesh->vertices.resize(positions.size());
+    for (std::size_t i = 0; i < positions.size(); ++i) {
+        const Vec3 n = normalize(positions[i]);
+        const Vec2 uv = compute_uv(n);
+        Vertex v {};
+        v.position[0] = n.x;
+        v.position[1] = n.y;
+        v.position[2] = n.z;
+        v.normal[0] = n.x;
+        v.normal[1] = n.y;
+        v.normal[2] = n.z;
+        v.tangent[0] = 1.0F;
+        v.tangent[1] = 0.0F;
+        v.tangent[2] = 0.0F;
+        v.bitangent[0] = 0.0F;
+        v.bitangent[1] = 1.0F;
+        v.bitangent[2] = 0.0F;
+        v.texcoord[0] = uv.u;
+        v.texcoord[1] = uv.v;
+        mesh->vertices[i] = v;
+    }
+
+    mesh->indices = faces;
+
+    // Compute tangents/bitangents per triangle (simple accumulation)
+    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
+    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
+        const std::uint32_t i0 = mesh->indices[i + 0];
+        const std::uint32_t i1 = mesh->indices[i + 1];
+        const std::uint32_t i2 = mesh->indices[i + 2];
+
+        const Vertex& v0 = mesh->vertices[i0];
+        const Vertex& v1 = mesh->vertices[i1];
+        const Vertex& v2 = mesh->vertices[i2];
+
+        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
+        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
+        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};
+
+        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
+        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
+        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};
+
+        const float x1 = p1.x - p0.x;
+        const float x2 = p2.x - p0.x;
+        const float y1 = p1.y - p0.y;
+        const float y2 = p2.y - p0.y;
+        const float z1 = p1.z - p0.z;
+        const float z2 = p2.z - p0.z;
+
+        const float s1 = w1.u - w0.u;
+        const float s2 = w2.u - w0.u;
+        const float t1 = w1.v - w0.v;
+        const float t2 = w2.v - w0.v;
+
+        const float r = (s1 * t2 - s2 * t1);
+        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
+        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
+        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};
+
+        tan1[i0] = add(tan1[i0], sdir);
+        tan1[i1] = add(tan1[i1], sdir);
+        tan1[i2] = add(tan1[i2], sdir);
+        tan2[i0] = add(tan2[i0], tdir);
+        tan2[i1] = add(tan2[i1], tdir);
+        tan2[i2] = add(tan2[i2], tdir);
+    }
+
+    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
+        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
+        Vec3 t = tan1[i];
+        // Gram-Schmidt orthogonalize
+        t = sub(t, mul(n, dot(n, t)));
+        t = normalize(t);
+        const Vec3 b = cross(n, t);
+        mesh->vertices[i].tangent[0] = t.x;
+        mesh->vertices[i].tangent[1] = t.y;
+        mesh->vertices[i].tangent[2] = t.z;
+        mesh->vertices[i].bitangent[0] = b.x;
+        mesh->vertices[i].bitangent[1] = b.y;
+        mesh->vertices[i].bitangent[2] = b.z;
+    }
+
+    return mesh;
+}
+
+} // namespace vulkano
+
diff --git a/src/vulkan_context.cpp b/src/vulkan_context.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..11d9dd5186fb9c4bd3f932a6849a391c1398f1b5
--- /dev/null
+++ b/src/vulkan_context.cpp
@@ -0,0 +1,173 @@
+#include <vulkano/vulkan_context.hpp>
+#include <vulkano/window.hpp>
+
+#if VULKANO_HAS_VULKAN
+#include <vulkan/vulkan.h>
+#include <GLFW/glfw3.h>
+
+#include <array>
+#include <vector>
+#include <stdexcept>
+
+namespace vulkano {
+
+struct VulkanContext::Impl final {
+    VkInstance instance {VK_NULL_HANDLE};
+    VkSurfaceKHR surface {VK_NULL_HANDLE};
+    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
+    VkDevice device {VK_NULL_HANDLE};
+    VkQueue graphics_queue {VK_NULL_HANDLE};
+    uint32_t graphics_queue_family {0U};
+
+    Impl() = default;
+    ~Impl() = default;
+};
+
+static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
+    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
+    // Simple stderr print would go here in future
+    (void)pCallbackData;
+    return VK_FALSE;
+}
+
+static void create_instance(VkInstance* instance) {
+    VkApplicationInfo app_info {};
+    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
+    app_info.pApplicationName = "vulkano";
+    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.pEngineName = "vulkano";
+    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
+    app_info.apiVersion = VK_API_VERSION_1_2;
+
+    uint32_t ext_count = 0U;
+    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
+    std::array<const char*, 4> extensions {};
+    for (uint32_t i = 0; i < ext_count; ++i) {
+        extensions[i] = exts[i];
+    }
+
+    VkInstanceCreateInfo ci {};
+    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
+    ci.pApplicationInfo = &app_info;
+    ci.enabledExtensionCount = ext_count;
+    ci.ppEnabledExtensionNames = extensions.data();
+
+#ifndef NDEBUG
+    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
+    ci.enabledLayerCount = 1U;
+    ci.ppEnabledLayerNames = layers;
+#endif
+
+    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
+    if (res != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan instance"};
+    }
+}
+
+VulkanContext::VulkanContext(const Window& window)
+    : impl {std::make_unique<Impl>()} {
+    create_instance(&impl->instance);
+    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
+    if (glfw_win == nullptr) {
+        throw std::runtime_error {"Invalid GLFW window handle"};
+    }
+    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create Vulkan surface"};
+    }
+    // Physical device selection (very naive)
+    uint32_t count = 0U;
+    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
+    if (count == 0U) {
+        throw std::runtime_error {"No Vulkan physical devices found"};
+    }
+    std::vector<VkPhysicalDevice> devices(count);
+    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
+    impl->physical_device = devices[0];
+
+    // Queue family selection
+    uint32_t qcount = 0U;
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
+    std::vector<VkQueueFamilyProperties> qprops(qcount);
+    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
+    uint32_t gfx_index = UINT32_MAX;
+    for (uint32_t i = 0; i < qcount; ++i) {
+        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
+            gfx_index = i;
+            break;
+        }
+    }
+    if (gfx_index == UINT32_MAX) {
+        throw std::runtime_error {"No graphics queue family found"};
+    }
+    impl->graphics_queue_family = gfx_index;
+
+    const float priority = 1.0F;
+    VkDeviceQueueCreateInfo qci {};
+    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
+    qci.queueFamilyIndex = gfx_index;
+    qci.queueCount = 1U;
+    qci.pQueuePriorities = &priority;
+
+    VkDeviceCreateInfo dci {};
+    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
+    dci.queueCreateInfoCount = 1U;
+    dci.pQueueCreateInfos = &qci;
+
+    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
+        throw std::runtime_error {"Failed to create logical device"};
+    }
+    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
+}
+
+VulkanContext::~VulkanContext() {
+    if (impl != nullptr) {
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDeviceWaitIdle(impl->device);
+        }
+        if (impl->device != VK_NULL_HANDLE) {
+            vkDestroyDevice(impl->device, nullptr);
+        }
+        if (impl->surface != VK_NULL_HANDLE) {
+            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
+        }
+        if (impl->instance != VK_NULL_HANDLE) {
+            vkDestroyInstance(impl->instance, nullptr);
+        }
+    }
+}
+
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+
+void VulkanContext::wait_idle() const noexcept {
+    if (impl->device != VK_NULL_HANDLE) {
+        vkDeviceWaitIdle(impl->device);
+    }
+}
+
+void VulkanContext::draw_frame() {
+    // Placeholder: swapchain/present will be added. No-op for now.
+}
+
+} // namespace vulkano
+
+#else
+
+namespace vulkano {
+
+class Window;
+
+VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
+}
+
+VulkanContext::~VulkanContext() = default;
+VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
+VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
+void VulkanContext::wait_idle() const noexcept {
+}
+void VulkanContext::draw_frame() {
+}
+
+} // namespace vulkano
+
+#endif
diff --git a/tests/icosphere_tests.cpp b/tests/icosphere_tests.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..b7771bab388e94eeabd7ead139cadc0d1b47c9d7
--- /dev/null
+++ b/tests/icosphere_tests.cpp
@@ -0,0 +1,28 @@
+#include <catch2/catch_all.hpp>
+
+#include <vulkano/icosphere.hpp>
+#include <vulkano/mesh.hpp>
+
+#include <cmath>
+
+static float length3(const float v[3]) {
+    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
+}
+
+TEST_CASE("Icosphere vertex count and normals") {
+    vulkano::IcosphereBuilder builder {};
+
+    for (std::uint32_t s = 0; s <= 3; ++s) {
+        auto mesh = builder.build(s);
+        REQUIRE(mesh != nullptr);
+        // Faces grow by factor 4 each subdivision starting from 20 triangles
+        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
+        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
+        // Normals should be ~unit length
+        for (const auto& v : mesh->vertices) {
+            const float len = length3(v.normal);
+            REQUIRE(len == Approx(1.0F).margin(1e-3F));
+        }
+    }
+}
+

[2025-09-15T15:58:02] exec bash -lc 'git add -A && git commit -m "chore(todo): update .agent/TODO with icosphere progress and next steps"' in /Users/bruno/Documents/Development/bruno/vulkano_codex
