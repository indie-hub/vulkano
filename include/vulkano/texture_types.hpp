#pragma once

#include <cstdint>

namespace vulkano {

enum class NormalMapStyle : std::uint32_t {
    RandomNoise = 0U,
    BrushedMetal = 1U
};

} // namespace vulkano

