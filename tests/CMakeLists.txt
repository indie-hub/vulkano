cmake_minimum_required(VERSION 3.25)

include(FetchContent)

# Fetch Catch2 if not provided by system
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(catch2)

add_executable(vulkano_tests
    tests_main.cpp
    stats_tests.cpp
    geometry_tests.cpp
    textures_tests.cpp
    camera_tests.cpp
)
target_link_libraries(vulkano_tests PRIVATE vulkano Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(vulkano_tests)

# End-to-end smoke test that runs the built app briefly and exits
add_test(NAME app_smoke
         COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkano_app)
set_tests_properties(app_smoke PROPERTIES
    ENVIRONMENT "VK_APP_AUTOCLOSE_MS=800;VK_SHADER_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders"
)

# Resize stress test: run app with automated window resizes to exercise swapchain recreation
add_test(NAME app_resize
         COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkano_app)
set_tests_properties(app_resize PROPERTIES
    ENVIRONMENT "VK_APP_AUTOCLOSE_MS=1500;VK_TEST_RESIZE=1;VK_TEST_RESIZE_INTERVAL_MS=250;VK_SHADER_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders"
)
