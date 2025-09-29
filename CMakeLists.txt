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

add_library(vulkano_app STATIC
    src/app/application.cpp
)

set_target_properties(vulkano_app PROPERTIES OUTPUT_NAME vulkano_app)

target_sources(vulkano_app
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/vulkano/app/application.hpp
)

target_include_directories(vulkano_app
    PUBLIC
        include
)

add_executable(vulkano_renderer
    src/main.cpp
)

target_link_libraries(vulkano_renderer
    PRIVATE
        vulkano_app
)

include(CTest)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
