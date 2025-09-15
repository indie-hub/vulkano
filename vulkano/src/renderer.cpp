#include <vulkano/renderer.hpp>
#include <vulkano/vulkan_context.hpp>

namespace vulkano {

class Renderer::Impl {
public:
    VulkanContext& ctx;
    Mesh mesh {std::vector<Vertex> {}, std::vector<std::uint32_t> {}};
    RenderSettings settings {};

    explicit Impl(VulkanContext& c) : ctx {c} {}
};

Renderer::Renderer(VulkanContext& ctx)
    : m_impl {std::make_unique<Impl>(ctx)}
{
}

Renderer::~Renderer() = default;

void Renderer::draw_frame()
{
    // Stub: poll events to keep window responsive
    (void)m_impl;
}

void Renderer::on_resize()
{
}

RenderSettings& Renderer::settings() noexcept
{
    return m_impl->settings;
}

} // namespace vulkano

