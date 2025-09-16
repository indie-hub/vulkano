cmake_minimum_required(VERSION 3.20)

project(vulkano_codex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include(FetchContent)

option(ENABLE_VALIDATION "Enable Vulkan validation layers in Debug" ON)

find_package(Vulkan REQUIRED)

# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
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
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# Dear ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

# Shaders
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SPIRV_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SPIRV_DIR})

find_program(GLSLC glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin bin)
if(NOT GLSLC)
  message(FATAL_ERROR "glslc not found. Please install Vulkan SDK and ensure glslc is in PATH.")
endif()

set(SHADERS
  ${SHADER_DIR}/triangle.vert
  ${SHADER_DIR}/triangle.frag
)

set(SPIRV_SHADERS)
foreach(SHADER ${SHADERS})
  get_filename_component(FILE_NAME ${SHADER} NAME)
  set(SPIRV_FILE ${SPIRV_DIR}/${FILE_NAME}.spv)
  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
    COMMAND ${GLSLC} -fshader-stage=$<IF:$<STREQUAL:${FILE_NAME},triangle.vert>,vert,frag> ${SHADER} -o ${SPIRV_FILE}
    DEPENDS ${SHADER}
    COMMENT "Compiling ${FILE_NAME} to SPIR-V"
    VERBATIM)
  list(APPEND SPIRV_SHADERS ${SPIRV_FILE})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_SHADERS})

add_executable(vulkano_app
  src/main.cpp
  src/vulkan_app.cpp
  src/imgui_layer.cpp
)

target_include_directories(vulkano_app PRIVATE include)

target_link_libraries(vulkano_app PRIVATE
  Vulkan::Vulkan
  glfw
  glm
)

# ImGui sources
target_sources(vulkano_app PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(vulkano_app PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

add_dependencies(vulkano_app Shaders)

target_compile_definitions(vulkano_app PRIVATE SPIRV_DIR="${SPIRV_DIR}")

if(MSVC)
  target_compile_options(vulkano_app PRIVATE /W4 /permissive-)
else()
  target_compile_options(vulkano_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug AND ENABLE_VALIDATION)
  target_compile_definitions(vulkano_app PRIVATE ENABLE_VALIDATION)
endif()
