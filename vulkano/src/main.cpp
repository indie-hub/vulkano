#include <vulkano/app.hpp>
#include <iostream>

int main()
{
    try {
        vulkano::App app {};
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

