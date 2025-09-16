#include <glm/glm.hpp>
#include <cassert>
#include <cstdio>

int main() {
    // Basic GLM sanity
    glm::vec2 a{1.0f, 2.0f};
    glm::vec2 b{2.0f, 3.0f};
    auto c = a + b;
    assert(c.x == 3.0f && c.y == 5.0f);
    std::puts("sanity ok");
    return 0;
}

