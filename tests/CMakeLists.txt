include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(catch2)

add_executable(vulkano_codex_tests
    tests_main.cpp
    fps_tests.cpp
)
target_link_libraries(vulkano_codex_tests PRIVATE vulkano_codex Catch2::Catch2WithMain)
add_test(NAME vulkano_codex_tests COMMAND vulkano_codex_tests)

