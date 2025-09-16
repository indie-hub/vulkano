cmake_minimum_required(VERSION 3.25)
project(VulkanoCodex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

include(FetchContent)

# Options
option(VULKANO_CODEX_BUILD_TESTS "Build tests" ON)
option(VULKANO_CODEX_ENABLE_VALIDATION "Enable Vulkan validation layers in Debug" ON)

# Dependencies
set(FETCHCONTENT_QUIET OFF)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# volk (Vulkan loader)
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG master
)
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)

# vk-bootstrap
FetchContent_Declare(
    vkbootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG v1.3.275
)
set(VKB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VKB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vkbootstrap)

# VMA
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(vma)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_backend PUBLIC glfw)
target_compile_definitions(imgui_backend PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_library(vulkano STATIC
    src/app.cpp
)

target_sources(vulkano
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/app.hpp
)

target_include_directories(vulkano PUBLIC include)
target_link_libraries(vulkano
    PUBLIC
        glfw
        glm::glm
        volk
        vk-bootstrap
        imgui_backend
)
target_include_directories(vulkano PUBLIC ${vma_SOURCE_DIR}/include)

target_compile_definitions(vulkano PUBLIC
    $<$<CONFIG:Debug>:VULKANO_CODEX_DEBUG=1>
    VK_NO_PROTOTYPES
    GLFW_INCLUDE_VULKAN)
target_compile_definitions(imgui_backend PUBLIC VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

add_executable(vulkano_app src/main.cpp)
target_link_libraries(vulkano_app PRIVATE vulkano)

# Put runtime outputs in bin/
set_target_properties(vulkano_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

include(CTest)
if (BUILD_TESTING AND VULKANO_CODEX_BUILD_TESTS)
    add_subdirectory(tests)
endif()

option(VULKANO_CODEX_USE_SHADERC "Enable runtime GLSL compilation via shaderc" OFF)
if (VULKANO_CODEX_USE_SHADERC)
  # shaderc (for runtime GLSL -> SPIR-V)
  FetchContent_Declare(
      shaderc
      GIT_REPOSITORY https://github.com/google/shaderc.git
      GIT_TAG main
  )
  set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
  set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
  set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(shaderc)
  target_link_libraries(vulkano PUBLIC shaderc_combined)
endif()
