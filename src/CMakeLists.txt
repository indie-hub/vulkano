add_library(vulkano STATIC
    app.cpp
    window.cpp
    icosphere.cpp
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
else()
    target_compile_definitions(vulkano PUBLIC VULKANO_HAS_VULKAN=0)
endif()
