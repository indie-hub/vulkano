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
if (ENABLE_VALIDATION)
    target_compile_definitions(vulkan_app PUBLIC VULKAN_APP_ENABLE_VALIDATION=1)
endif()

add_executable(vulkan_app_example src/main.cpp)
target_link_libraries(vulkan_app_example PRIVATE vulkan_app)

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
