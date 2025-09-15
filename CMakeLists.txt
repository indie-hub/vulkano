cmake_minimum_required(VERSION 3.25)
project(vulkan_app VERSION 0.1.0 LANGUAGES CXX)

option(ENABLE_VALIDATION "Enable Vulkan validation layers in Debug" ON)
option(ENABLE_GLFW_WAYLAND "Enable GLFW Wayland backend on Linux" OFF)
option(ENABLE_IMGUI_DOCKING "Enable ImGui docking" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

find_package(Vulkan REQUIRED)

# Dear ImGui (fetch and build with GLFW + Vulkan backends)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.5
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_backend PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_backend PUBLIC glfw Vulkan::Vulkan)

if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(vulkan_app STATIC
    src/icosphere.cpp
    src/application.cpp
    src/renderer.cpp
    src/camera.cpp
)

target_sources(vulkan_app
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkan_app/icosphere.hpp
            include/vulkan_app/application.hpp
            include/vulkan_app/renderer.hpp
            include/vulkan_app/camera.hpp
            include/vulkan_app/types.hpp
)

target_include_directories(vulkan_app PUBLIC include)
target_link_libraries(vulkan_app PUBLIC glm glfw Vulkan::Vulkan)
target_link_libraries(vulkan_app PUBLIC imgui_backend)
if (ENABLE_VALIDATION)
    target_compile_definitions(vulkan_app PUBLIC VULKAN_APP_ENABLE_VALIDATION=1)
endif()

add_executable(vulkan_app_example src/main.cpp)
target_link_libraries(vulkan_app_example PRIVATE vulkan_app)

# Shader compilation (if glslc is available)
find_program(GLSLC glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)
set(SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ssao.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/blur.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compose.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/forward.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/forward.frag)
set(SPIRV_OUTPUTS)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV ${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv)
    list(APPEND SPIRV_OUTPUTS ${SPIRV})
    if (GLSLC)
        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
            COMMAND ${GLSLC} -O -o ${SPIRV} ${SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling ${FILE_NAME} to SPIR-V"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} ${SPIRV}
            DEPENDS ${SHADER}
            COMMENT "glslc not found; copying ${FILE_NAME} as placeholder"
            VERBATIM)
    endif()
endforeach()
add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(vulkan_app_example shaders)

include(CTest)
if (BUILD_TESTING)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
  )
  FetchContent_MakeAvailable(catch2)
  add_executable(icosphere_tests tests/icosphere_tests.cpp)
  target_link_libraries(icosphere_tests PRIVATE Catch2::Catch2WithMain vulkan_app)
  add_test(NAME icosphere_tests COMMAND icosphere_tests)
endif()
