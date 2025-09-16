#include <vulkano/app.hpp>

int main() {
    const vulkano::AppConfig config {};
    vulkano::App app {config};
    app.run();
    return 0;
}

