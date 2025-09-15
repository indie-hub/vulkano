#pragma once

#include <memory>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/renderer.hpp>

namespace vulkano {

class App final {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    std::unique_ptr<VulkanContext> m_ctx {};
    std::unique_ptr<Renderer> m_renderer {};
};

} // namespace vulkano

