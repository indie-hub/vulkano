#include <vulkano/app_config.hpp>

#include <utility>

namespace vulkano {

namespace {
    constexpr WindowExtent defaultWindowExtent {};
    const std::string defaultApplicationName {"Vulkano Codex"};
    const std::string defaultEngineName {"Codex Engine"};
} // namespace

AppConfigBuilder::AppConfigBuilder()
    : m_applicationName {defaultApplicationName}
    , m_engineName {defaultEngineName}
    , m_windowExtent {defaultWindowExtent}
    , m_validationEnabled {true} {
}

auto AppConfigBuilder::with_application_name(std::string name) -> AppConfigBuilder& {
    m_applicationName = std::move(name);
    return *this;
}

auto AppConfigBuilder::with_engine_name(std::string name) -> AppConfigBuilder& {
    m_engineName = std::move(name);
    return *this;
}

auto AppConfigBuilder::with_window_extent(WindowExtent extent) -> AppConfigBuilder& {
    m_windowExtent = extent;
    return *this;
}

auto AppConfigBuilder::with_validation_enabled(bool enabled) -> AppConfigBuilder& {
    m_validationEnabled = enabled;
    return *this;
}

auto AppConfigBuilder::build() const -> AppConfig {
    AppConfig config {};
    config.m_applicationName = m_applicationName;
    config.m_engineName = m_engineName;
    config.m_windowExtent = m_windowExtent;
    config.m_validationEnabled = m_validationEnabled;
    return config;
}

auto AppConfig::application_name() const noexcept -> const std::string& {
    return m_applicationName;
}

auto AppConfig::engine_name() const noexcept -> const std::string& {
    return m_engineName;
}

auto AppConfig::window_extent() const noexcept -> WindowExtent {
    return m_windowExtent;
}

auto AppConfig::validation_enabled() const noexcept -> bool {
    return m_validationEnabled;
}

} // namespace vulkano
