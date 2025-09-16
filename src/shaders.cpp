// shaders.cpp
#include <cstdint>
#include <string>
#include <vector>

#include <shaderc/shaderc.hpp>

namespace {

constexpr const char* kVertexShaderGLSL {
    R"(\
#version 450
layout(location = 0) in vec2 inPos;
void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
}
)"};

constexpr const char* kFragmentShaderGLSL {
    R"(\
#version 450
layout(push_constant) uniform Push { vec4 color; } pushData;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = pushData.color;
}
)"};

std::vector<std::uint32_t> compile(const std::string& src, shaderc_shader_kind kind, const std::string& name) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(src, kind, name.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error{"Shader compilation failed: " + std::string{result.GetErrorMessage()}};
    }
    return {result.cbegin(), result.cend()};
}

} // namespace

namespace shaders {

std::vector<std::uint32_t> vertex_spirv() {
    static std::vector<std::uint32_t> cache = compile(kVertexShaderGLSL, shaderc_vertex_shader, "triangle.vert");
    return cache;
}

std::vector<std::uint32_t> fragment_spirv() {
    static std::vector<std::uint32_t> cache = compile(kFragmentShaderGLSL, shaderc_fragment_shader, "triangle.frag");
    return cache;
}

} // namespace shaders

