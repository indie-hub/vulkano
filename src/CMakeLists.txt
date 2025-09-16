add_library(vulkano STATIC
    app.cpp
    window.cpp
    icosphere.cpp
    imgui_layer.cpp
)

target_link_libraries(vulkano
    PUBLIC
        vulkano_headers
        glfw
        glm
)

target_include_directories(vulkano PUBLIC ${CMAKE_SOURCE_DIR}/include)

if (Vulkan_FOUND)
    target_link_libraries(vulkano PUBLIC Vulkan::Vulkan)
    target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=1)
    target_sources(vulkano PRIVATE vulkan_context.cpp)
    if (ENABLE_VALIDATION)
        target_compile_definitions(vulkano PUBLIC VULKANO_ENABLE_VALIDATION=1)
    else()
        target_compile_definitions(vulkano PUBLIC VULKANO_ENABLE_VALIDATION=0)
    endif()
    # ImGui files and backends
    target_sources(vulkano PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(vulkano PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
else()
    target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
endif()
