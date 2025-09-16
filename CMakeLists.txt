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

if (TARGET glfw)
  target_link_libraries(vulkano PUBLIC glfw)
else()
  find_package(glfw3 REQUIRED)
  target_link_libraries(vulkano PUBLIC glfw)
endif()

if (TARGET glm)
  target_link_libraries(vulkano PUBLIC glm)
else()
  find_package(glm REQUIRED)
  target_link_libraries(vulkano PUBLIC glm::glm)
endif()

add_executable(vulkano_app src/main.cpp)
target_link_libraries(vulkano_app PRIVATE vulkano)

include(CTest)
if (BUILD_TESTING)
  if (EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
  endif()
endif()
