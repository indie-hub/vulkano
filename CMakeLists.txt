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

add_library(vulkano_app STATIC
    src/app/application.cpp
    src/app/glfw_library.cpp
    src/app/window.cpp
    src/app/vulkan_context.cpp
)

set_target_properties(vulkano_app PROPERTIES OUTPUT_NAME vulkano_app)

target_sources(vulkano_app
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/app/application.hpp
            include/vulkano/app/glfw_library.hpp
            include/vulkano/app/window.hpp
            include/vulkano/app/vulkan_context.hpp
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
)

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

if(APPLE)
    target_compile_definitions(vulkano_renderer PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif()

include(CTest)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
