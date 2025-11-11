cmake_minimum_required(VERSION 3.25)
project(vulkano_renderer VERSION 0.1.0 LANGUAGES CXX)

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

find_package(Vulkan REQUIRED)

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc executable not found in Vulkan SDK")
endif()

set(VULKANO_SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/scene.vert
    ${CMAKE_SOURCE_DIR}/shaders/scene.frag
)

set(VULKANO_SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
set(VULKANO_SHADER_TARGETS)

foreach(shader ${VULKANO_SHADER_SOURCES})
    get_filename_component(shader_name ${shader} NAME)
    set(output ${VULKANO_SHADER_OUTPUT_DIR}/${shader_name}.spv)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${VULKANO_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -o ${output} ${shader}
        DEPENDS ${shader}
        COMMENT "Compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    list(APPEND VULKANO_SHADER_TARGETS ${output})
endforeach()

add_custom_target(vulkano_shaders ALL
    DEPENDS ${VULKANO_SHADER_TARGETS}
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
)
FetchContent_MakeAvailable(glfw)

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.8
)
FetchContent_MakeAvailable(imgui)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

set(IMGUI_SOURCE_DIR ${imgui_SOURCE_DIR})
set(IMGUI_BACKENDS_DIR ${IMGUI_SOURCE_DIR}/backends)

add_library(imgui STATIC
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp
)

set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(imgui
    PUBLIC
        ${IMGUI_SOURCE_DIR}
        ${IMGUI_BACKENDS_DIR}
)


target_link_libraries(imgui
    PUBLIC
        glfw
        Vulkan::Vulkan
)

target_compile_definitions(imgui
    PUBLIC
        GLFW_INCLUDE_NONE
)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(catch2)

add_library(vulkano_math STATIC
    src/app/math.cpp
)

set_target_properties(vulkano_math PROPERTIES OUTPUT_NAME vulkano_math)

target_sources(vulkano_math
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/app/math.hpp
)

target_include_directories(vulkano_math
    PUBLIC
        include
)

target_link_libraries(vulkano_math
    PUBLIC
        glm::glm
)

add_library(vulkano_app STATIC
    src/app/application.cpp
    src/app/frame_resources.cpp
    src/app/glfw_library.cpp
    src/app/scene_renderer.cpp
    src/app/imgui_renderer.cpp
    src/app/vulkan_context.cpp
    src/app/window.cpp
    src/scene/mesh.cpp
    src/vk/instance.cpp
    src/vk/physical_device.cpp
    src/vk/logical_device.cpp
    src/vk/swapchain.cpp
)

set_target_properties(vulkano_app PROPERTIES OUTPUT_NAME vulkano_app)

target_sources(vulkano_app
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/app/application.hpp
            include/vulkano/app/frame_resources.hpp
            include/vulkano/app/glfw_library.hpp
            include/vulkano/app/scene_renderer.hpp
            include/vulkano/app/imgui_renderer.hpp
            include/vulkano/app/vulkan_context.hpp
            include/vulkano/app/window.hpp
            include/vulkano/vk/instance.hpp
            include/vulkano/vk/physical_device.hpp
            include/vulkano/scene/mesh.hpp
            include/vulkano/vk/logical_device.hpp
            include/vulkano/vk/swapchain.hpp
)

target_include_directories(vulkano_app
    PUBLIC
        include
)

target_link_libraries(vulkano_app
    PUBLIC
        glm::glm
    PRIVATE
        glfw
        Vulkan::Vulkan
        imgui
        vulkano_math
)

target_compile_definitions(vulkano_app
    PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RIGHT_HANDED
)

add_dependencies(vulkano_app vulkano_shaders)

if(APPLE)
    target_compile_definitions(vulkano_app PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif()

add_executable(vulkano_renderer
    src/main.cpp
)

target_link_libraries(vulkano_renderer
    PRIVATE
        vulkano_app
)

add_dependencies(vulkano_renderer vulkano_shaders)

if(APPLE)
    target_compile_definitions(vulkano_renderer PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif()

set(VULKANO_PACKAGE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/package)

add_custom_target(vulkano_package
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${VULKANO_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VULKANO_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VULKANO_PACKAGE_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vulkano_renderer> ${VULKANO_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders ${VULKANO_PACKAGE_DIR}/shaders
    COMMENT "Packaging runtime bundle to ${VULKANO_PACKAGE_DIR}"
    DEPENDS vulkano_renderer vulkano_shaders
)

include(CTest)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
