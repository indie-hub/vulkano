cmake_minimum_required(VERSION 3.25)
project(vulkano_codex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

include(FetchContent)

# Try to find GLFW; fallback to FetchContent if unavailable
find_package(glfw3 QUIET)
if (NOT glfw3_FOUND)
  message(STATUS "glfw3 not found; fetching via FetchContent")
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
endif()

# Vulkan headers (FetchContent) and library (find if available)
find_package(Vulkan QUIET)
if (NOT Vulkan_FOUND)
  message(STATUS "Vulkan not found; fetching Vulkan-Headers for compilation")
  FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.3.296
  )
  FetchContent_MakeAvailable(VulkanHeaders)
endif()

# Header-only GLM
find_package(glm QUIET)
if (NOT glm_FOUND)
  message(STATUS "glm not found; fetching via FetchContent")
  FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
  )
  set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glm)
endif()

# Dear ImGui (core + backends for GLFW/Vulkan)
set(IMGUI_FETCHED OFF)
find_path(IMGUI_INCLUDE_DIR imgui.h PATH_SUFFIXES imgui)
if (NOT IMGUI_INCLUDE_DIR)
  message(STATUS "imgui not found; fetching via FetchContent")
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
  )
  FetchContent_MakeAvailable(imgui)
  set(IMGUI_FETCHED ON)
endif()

if (IMGUI_FETCHED)
  # Build a minimal imgui static library including required backends
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  )
  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  if (Vulkan_FOUND)
    target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
  elseif (APPLE AND MOLTENVK_LIBRARY)
    target_link_libraries(imgui PUBLIC ${MOLTENVK_LIBRARY})
  endif()
  target_link_libraries(imgui PUBLIC glfw)
endif()

add_library(vulkano STATIC
  src/app.cpp
  src/vulkan_context.cpp
  src/imgui_overlay.cpp
  src/stats.cpp
)

target_sources(vulkano
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/vulkano/app.hpp
      include/vulkano/vulkan_context.hpp
      include/vulkano/stats.hpp
)

target_include_directories(vulkano PUBLIC include)
target_compile_definitions(vulkano PRIVATE GLFW_INCLUDE_VULKAN)

if (TARGET glfw)
  target_link_libraries(vulkano PUBLIC glfw)
else()
  find_package(glfw3 REQUIRED)
  target_link_libraries(vulkano PUBLIC glfw)
endif()

# Vulkan headers and libs
if (Vulkan_FOUND)
  target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
  target_compile_definitions(vulkano PUBLIC VULKAN_LOADER_AVAILABLE=1)
else()
  # Try to locate a loader on Apple (MoltenVK or Vulkan loader) for linking if present
  if (APPLE)
    find_library(MOLTENVK_LIBRARY MoltenVK)
    if (MOLTENVK_LIBRARY)
      target_link_libraries(vulkano PUBLIC ${MOLTENVK_LIBRARY})
      target_compile_definitions(vulkano PUBLIC VULKAN_LOADER_AVAILABLE=1)
    else()
      target_compile_definitions(vulkano PUBLIC VULKAN_LOADER_AVAILABLE=0)
    endif()
  else()
    target_compile_definitions(vulkano PUBLIC VULKAN_LOADER_AVAILABLE=0)
  endif()
  if (TARGET VulkanHeaders)
    target_include_directories(vulkano PUBLIC ${VulkanHeaders_SOURCE_DIR}/include)
  endif()
endif()

if (TARGET glm)
  target_link_libraries(vulkano PUBLIC glm)
else()
  find_package(glm REQUIRED)
  target_link_libraries(vulkano PUBLIC glm::glm)
endif()

if (TARGET imgui)
  target_link_libraries(vulkano PUBLIC imgui)
endif()

add_executable(vulkano_app src/main.cpp)
target_link_libraries(vulkano_app PRIVATE vulkano)

include(CTest)
if (BUILD_TESTING)
  if (EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
  endif()
endif()

# --- Shader compilation (GLSL -> SPIR-V) ---
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin bin)
find_program(GLSLANG_VALIDATOR_EXECUTABLE NAMES glslangValidator HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin bin)

set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/triangle.vert
    ${CMAKE_SOURCE_DIR}/shaders/triangle.frag
)

set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SHADER_BINS)
foreach(SHADER ${SHADER_SOURCES})
  get_filename_component(SHADER_NAME ${SHADER} NAME)
  set(SPV ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)
  list(APPEND SHADER_BINS ${SPV})
  if (GLSLC_EXECUTABLE)
    add_custom_command(
      OUTPUT ${SPV}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
      COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=$<IF:$<STREQUAL:${SHADER_NAME},triangle.vert>,vert,frag> -O -o ${SPV} ${SHADER}
      DEPENDS ${SHADER}
      COMMENT "Compiling ${SHADER_NAME} -> ${SPV} with glslc"
      VERBATIM)
  elseif (GLSLANG_VALIDATOR_EXECUTABLE)
    add_custom_command(
      OUTPUT ${SPV}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
      COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -V -o ${SPV} ${SHADER}
      DEPENDS ${SHADER}
      COMMENT "Compiling ${SHADER_NAME} -> ${SPV} with glslangValidator"
      VERBATIM)
  else()
    message(WARNING "No GLSL compiler (glslc or glslangValidator) found. Shaders will not be compiled.")
    # Create a phony rule to avoid CMake errors; runtime will fail if shaders are missing
    add_custom_command(
      OUTPUT ${SPV}
      COMMAND ${CMAKE_COMMAND} -E echo "Missing GLSL compiler; ${SPV} not generated"
      DEPENDS ${SHADER}
      VERBATIM)
  endif()
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SHADER_BINS})
add_dependencies(vulkano_app compile_shaders)
