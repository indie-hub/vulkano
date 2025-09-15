#include <catch2/catch_all.hpp>

#include <vulkano/app.hpp>

TEST_CASE("AppConfig default sizes are sane") {
    const vulkano::AppConfig cfg { };
    REQUIRE(cfg.width >= 640U);
    REQUIRE(cfg.height >= 480U);
}

