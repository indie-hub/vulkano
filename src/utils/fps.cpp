// fps.cpp
#include <vulkano/utils/fps.hpp>

namespace vulkano::utils {

FpsSmoother::FpsSmoother(double alpha) noexcept : m_alpha {alpha}, m_smoothed_ms {16.6667} {
}

void FpsSmoother::add_sample(double dt_ms) noexcept {
    m_smoothed_ms = m_alpha * dt_ms + (1.0 - m_alpha) * m_smoothed_ms;
}

double FpsSmoother::smoothed_ms() const noexcept { return m_smoothed_ms; }

double FpsSmoother::fps() const noexcept {
    const double ms = m_smoothed_ms;
    return ms > 0.0 ? 1000.0 / ms : 0.0;
}

} // namespace vulkano::utils

