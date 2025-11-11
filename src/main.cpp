#include <vulkano/app/application.hpp>

int main() {
    vulkano::app::Application application {};
    const int exitCode {application.run()};
    return exitCode;
}
