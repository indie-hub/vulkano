cmake_minimum_required(VERSION 3.25)
project(VulkanoCodex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

include(FetchContent)

# Dependencies: GLFW, GLM, ImGui
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
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

# ImGui backends
add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_backend PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_backend PUBLIC glfw)

find_package(Vulkan REQUIRED)

add_library(vulkano_codex STATIC
    src/app.cpp
    src/imgui_layer.cpp
    src/shaders.cpp
    src/utils.cpp
)

target_sources(vulkano_codex
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano_codex/app.hpp
            include/vulkano_codex/imgui_layer.hpp
            include/vulkano_codex/utils.hpp
)

target_include_directories(vulkano_codex PUBLIC include)
target_link_libraries(vulkano_codex PUBLIC glfw Vulkan::Vulkan imgui_backend glm)

add_executable(vulkano_codex_app src/main.cpp)
target_link_libraries(vulkano_codex_app PRIVATE vulkano_codex)

find_program(GLSLANG_VALIDATOR glslangValidator)
set(SHADER_OUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})
set(VERT_SHADER ${CMAKE_SOURCE_DIR}/shaders/triangle.vert)
set(FRAG_SHADER ${CMAKE_SOURCE_DIR}/shaders/triangle.frag)
set(VERT_SPV ${SHADER_OUT_DIR}/triangle.vert.spv)
set(FRAG_SPV ${SHADER_OUT_DIR}/triangle.frag.spv)

if(GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT ${VERT_SPV}
        COMMAND ${GLSLANG_VALIDATOR} -V ${VERT_SHADER} -o ${VERT_SPV}
        DEPENDS ${VERT_SHADER}
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT ${FRAG_SPV}
        COMMAND ${GLSLANG_VALIDATOR} -V ${FRAG_SHADER} -o ${FRAG_SPV}
        DEPENDS ${FRAG_SHADER}
        COMMENT "Compiling fragment shader"
    )
    add_custom_target(shaders ALL DEPENDS ${VERT_SPV} ${FRAG_SPV})
    add_dependencies(vulkano_codex_app shaders)
    target_compile_definitions(vulkano_codex PUBLIC SHADER_DIR="${SHADER_OUT_DIR}")
else()
    message(WARNING "glslangValidator not found; ensure SPIR-V shaders exist in bin/shaders")
    target_compile_definitions(vulkano_codex PUBLIC SHADER_DIR="${SHADER_OUT_DIR}")
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
