set(VULKANO_TEST_SOURCES
    triangle_transforms_tests.cpp
    geometry_tests.cpp
    camera_tests.cpp
    ssao_samples_tests.cpp
    depth_format_tests.cpp
    material_registry_tests.cpp
    material_gpu_tests.cpp
    texture_loader_tests.cpp
    light_registry_tests.cpp
    light_gpu_tests.cpp
    shadow_resources_tests.cpp
    asset_importer_tests.cpp
    mesh_packing_tests.cpp
    transform_tests.cpp
    scene_graph_tests.cpp
    renderer_constants_tests.cpp
)

add_executable(vulkano_tests
    ${VULKANO_TEST_SOURCES}
)

target_link_libraries(vulkano_tests
    PRIVATE
        Catch2::Catch2WithMain
        vulkano_math
        vulkano_app
)

target_compile_definitions(vulkano_tests
    PRIVATE
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RIGHT_HANDED
        TEST_ASSET_DIR="${CMAKE_SOURCE_DIR}/tests/assets"
)

add_test(NAME vulkano_tests COMMAND vulkano_tests)
