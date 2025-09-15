#include <vulkano/app.hpp>

#include <stdexcept>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/renderer.hpp>

namespace vulkano {

App::App()
{
    m_ctx = std::make_unique<VulkanContext>();
    m_ctx->init_window(1280, 720, "Vulkano Codex");
    m_ctx->init_vulkan();
    m_renderer = std::make_unique<Renderer>(*m_ctx);
}

App::~App() = default;

int App::run()
{
    while (!m_ctx->should_close()) {
        m_ctx->poll_events();
        m_renderer->draw_frame();
    }
    return 0;
}

} // namespace vulkano
