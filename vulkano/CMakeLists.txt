cmake_minimum_required(VERSION 3.25)
project(VulkanoCodex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_VALIDATION "Enable Vulkan validation layers" ON)
option(ENABLE_GLFW_WAYLAND "Enable GLFW Wayland backend on Linux" OFF)
option(ENABLE_IMGUI_DOCKING "Enable ImGui docking features" OFF)

include(FetchContent)

if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

# Dependencies
find_package(Vulkan REQUIRED)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(catch2)

# stb_image (header-only)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Sources
add_library(vulkano STATIC
    src/app.cpp
    src/vulkan_context.cpp
    src/swapchain.cpp
    src/imgui_layer.cpp
    src/renderer.cpp
    src/mesh.cpp
    src/icosphere.cpp
    src/camera.cpp
)

target_sources(vulkano
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/app.hpp
            include/vulkano/vulkan_context.hpp
            include/vulkano/swapchain.hpp
            include/vulkano/imgui_layer.hpp
            include/vulkano/renderer.hpp
            include/vulkano/mesh.hpp
            include/vulkano/icosphere.hpp
            include/vulkano/camera.hpp
            include/vulkano/types.hpp
)

target_include_directories(vulkano PUBLIC include)

target_link_libraries(vulkano
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm
)

target_compile_definitions(vulkano PUBLIC
    $<$<BOOL:${ENABLE_VALIDATION}>:VULKANO_ENABLE_VALIDATION>
    $<$<BOOL:${ENABLE_IMGUI_DOCKING}>:IMGUI_ENABLE_DOCKING>
)

# ImGui backend objects
add_library(imgui_backends STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_backends PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_backends PUBLIC glfw Vulkan::Vulkan)

target_link_libraries(vulkano PUBLIC imgui_backends)

# Example executable
add_executable(vulkano_app src/main.cpp)
target_link_libraries(vulkano_app PRIVATE vulkano)

set_target_properties(vulkano_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()
