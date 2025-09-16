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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

include(FetchContent)

find_package(Vulkan REQUIRED)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# ImGui
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
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_backend PUBLIC glfw Vulkan::Vulkan)

add_library(vulkano STATIC
    src/window.cpp
    src/vulkan_context.cpp
    src/imgui_layer.cpp
    src/app.cpp
    src/utils/fps.cpp
)
target_sources(vulkano
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/window.hpp
            include/vulkano/vulkan_context.hpp
            include/vulkano/imgui_layer.hpp
            include/vulkano/app.hpp
            include/vulkano/utils/fps.hpp
)
target_include_directories(vulkano PUBLIC include)
target_link_libraries(vulkano PUBLIC Vulkan::Vulkan glfw imgui_backend glm::glm)

add_executable(vulkano_app src/main.cpp)
target_link_libraries(vulkano_app PRIVATE vulkano)

# Shaders
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})
set(VERT_SRC ${SHADER_SRC_DIR}/triangle.vert)
set(FRAG_SRC ${SHADER_SRC_DIR}/triangle.frag)
set(VERT_SPV ${SHADER_BIN_DIR}/triangle.vert.spv)
set(FRAG_SPV ${SHADER_BIN_DIR}/triangle.frag.spv)
add_custom_command(OUTPUT ${VERT_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=vert ${VERT_SRC} -o ${VERT_SPV}
    DEPENDS ${VERT_SRC}
    COMMENT "Compiling vertex shader")
add_custom_command(OUTPUT ${FRAG_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=frag ${FRAG_SRC} -o ${FRAG_SPV}
    DEPENDS ${FRAG_SRC}
    COMMENT "Compiling fragment shader")
add_custom_target(compile_shaders DEPENDS ${VERT_SPV} ${FRAG_SPV})
add_dependencies(vulkano_app compile_shaders)

include(CTest)
if (BUILD_TESTING)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.6.0
    )
    FetchContent_MakeAvailable(catch2)
    add_executable(vulkano_tests
        tests/tests_main.cpp
        tests/fps_smoother_tests.cpp
    )
    target_link_libraries(vulkano_tests PRIVATE vulkano Catch2::Catch2WithMain)
    add_test(NAME vulkano_tests COMMAND vulkano_tests)
endif()
