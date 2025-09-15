add_executable(vulkano_tests
    tests_main.cpp
    icosphere_tests.cpp
)
target_link_libraries(vulkano_tests PRIVATE vulkano Catch2::Catch2WithMain)
add_test(NAME vulkano_tests COMMAND vulkano_tests)

