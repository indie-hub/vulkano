add_library(vulkano_codex_tests_config INTERFACE)

target_link_libraries(vulkano_codex_tests_config
    INTERFACE
        Catch2::Catch2WithMain
)

target_compile_features(vulkano_codex_tests_config INTERFACE cxx_std_20)
