add_library(vulkano_codex_tests_config INTERFACE)

target_link_libraries(vulkano_codex_tests_config
    INTERFACE
        Catch2::Catch2WithMain
)

target_compile_features(vulkano_codex_tests_config INTERFACE cxx_std_20)

target_include_directories(vulkano_codex_tests_config INTERFACE ${PROJECT_SOURCE_DIR}/include)

add_executable(vulkano_codex_tests
    unit/app_config_tests.cpp
    unit/vertex_tests.cpp
    unit/primitives_tests.cpp
    unit/camera_tests.cpp
    integration/application_smoke_tests.cpp
)

target_link_libraries(vulkano_codex_tests
    PRIVATE
        vulkano_codex_lib
        vulkano_codex_tests_config
)

set_target_properties(vulkano_codex_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_dependencies(vulkano_codex_tests compile_shaders)

add_test(NAME vulkano_codex_tests COMMAND vulkano_codex_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
