// Public interface for the Vulkan application
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace vulkano {

struct AppConfig final {
    int initialWidth {1280};
    int initialHeight {720};
    std::string windowTitle {"Vulkano Codex"};
    bool enableValidation {false};
};

class App final {
public:
    App() = delete;
    explicit App(const AppConfig& config);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    void run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vulkano

