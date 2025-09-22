#include <cstdlib>
#include <exception>
#include <iostream>

#include <vulkano/app_config.hpp>
#include <vulkano/application.hpp>

auto main() -> int {
    try {
        const auto config = vulkano::AppConfigBuilder {}.build();
        vulkano::VulkanApplication application {config};
        application.run();
        return EXIT_SUCCESS;
    } catch(const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
    } catch(...) {
        std::cerr << "Fatal error: unknown exception" << '\n';
    }
    return EXIT_FAILURE;
}
