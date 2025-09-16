// fps.hpp
#pragma once
#include <cstdint>

namespace vulkano::utils {

class FpsSmoother final {
public:
    explicit FpsSmoother(double alpha = 0.1) noexcept;
    void add_sample(double dt_ms) noexcept;
    [[nodiscard]] double smoothed_ms() const noexcept;
    [[nodiscard]] double fps() const noexcept;

private:
    double m_alpha {0.1};
    double m_smoothed_ms {16.6667};
};

} // namespace vulkano::utils

