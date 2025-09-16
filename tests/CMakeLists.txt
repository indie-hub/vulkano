cmake_minimum_required(VERSION 3.25)

include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(vulkano_tests
    tests_main.cpp
    timing_tests.cpp
)
target_link_libraries(vulkano_tests PRIVATE vulkano Catch2::Catch2WithMain)

add_test(NAME vulkano_tests COMMAND vulkano_tests)
