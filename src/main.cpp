#include <vulkano/app.hpp>

int main() {
    const vulkano::AppConfig cfg {};
    vulkano::App app {cfg};
    app.run();
    return 0;
}

