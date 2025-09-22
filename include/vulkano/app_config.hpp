#pragma once

#include <cstdint>
#include <string>

namespace vulkano {

struct WindowExtent final {
    std::uint32_t width {1280U};
    std::uint32_t height {720U};
};

class AppConfig final {
public:
    [[nodiscard]] auto application_name() const noexcept -> const std::string&;
    [[nodiscard]] auto engine_name() const noexcept -> const std::string&;
    [[nodiscard]] auto window_extent() const noexcept -> WindowExtent;
    [[nodiscard]] auto validation_enabled() const noexcept -> bool;

private:
    friend class AppConfigBuilder;
    std::string m_applicationName {};
    std::string m_engineName {};
    WindowExtent m_windowExtent {};
    bool m_validationEnabled {true};
};

class AppConfigBuilder final {
public:
    AppConfigBuilder();

    [[nodiscard]] auto with_application_name(std::string name) -> AppConfigBuilder&;
    [[nodiscard]] auto with_engine_name(std::string name) -> AppConfigBuilder&;
    [[nodiscard]] auto with_window_extent(WindowExtent extent) -> AppConfigBuilder&;
    [[nodiscard]] auto with_validation_enabled(bool enabled) -> AppConfigBuilder&;
    [[nodiscard]] auto build() const -> AppConfig;

private:
    std::string m_applicationName;
    std::string m_engineName;
    WindowExtent m_windowExtent;
    bool m_validationEnabled;
};

} // namespace vulkano
