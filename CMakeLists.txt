cmake_minimum_required(VERSION 3.25)
project(VulkanoCodex VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
foreach(CONFIG IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${PROJECT_SOURCE_DIR}/bin)
endforeach()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()

find_package(Vulkan REQUIRED)

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc executable not found. Ensure the Vulkan SDK is installed and glslc is in PATH.")
endif()

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.6
)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
)

FetchContent_MakeAvailable(glfw glm)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(imgui
        PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui
        PUBLIC
            glfw
            Vulkan::Vulkan
    )
    target_compile_features(imgui PUBLIC cxx_std_20)
endif()
add_library(imgui::imgui ALIAS imgui)

add_library(vulkano_codex_lib STATIC
    src/app_config.cpp
    src/application.cpp
    src/buffer.cpp
    src/command_allocator.cpp
    src/framebuffers.cpp
    src/glfw_context.cpp
    src/graphics_pipeline.cpp
    src/render_pass.cpp
    src/swapchain.cpp
    src/synchronization.cpp
    src/vertex.cpp
    src/primitives.cpp
    src/vulkan_context.cpp
    src/window.cpp
)

set(SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert
)

set(SPIRV_OUTPUTS)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV_FILE ${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -o ${SPIRV_FILE} ${SHADER}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${FILE_NAME}"
        VERBATIM
    )
    list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(vulkano_codex_lib compile_shaders)

target_include_directories(vulkano_codex_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(vulkano_codex_lib
    PUBLIC
        Vulkan::Vulkan
        glfw
        imgui::imgui
        glm::glm
)

add_executable(vulkano_codex
    src/main.cpp
)

add_dependencies(vulkano_codex compile_shaders)
add_custom_command(TARGET vulkano_codex POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:vulkano_codex>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_OUTPUTS} $<TARGET_FILE_DIR:vulkano_codex>/shaders
)

target_link_libraries(vulkano_codex
    PRIVATE
        vulkano_codex_lib
)

include(CTest)
if(BUILD_TESTING)
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
