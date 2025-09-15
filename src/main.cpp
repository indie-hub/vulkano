#include <iostream>

#include <vulkan_app/icosphere.hpp>

int main() {
    const auto mesh {vulkan_app::make_icosphere(0)};
    std::cout << "vertices: " << mesh.vertices.size() << '\n';
    return 0;
}
