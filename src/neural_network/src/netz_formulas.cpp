#include <cmath>
#include "netz_formulas.hpp"

double netz::math::actfunc::Sigma(double x) noexcept {
    return 1 / (1 + std::exp(-x));
}

double netz::math::actfunc::SigmaMirrored(double x) noexcept {
    return std::exp(x) / (1 + std::exp(-x));
}

double netz::math::DeltaCoeffLastLayer(double expected_value,
        double output_value) noexcept {
    return output_value * (1 - output_value)
        * (expected_value - output_value);
}

double netz::math::GetNewWeight(double weight, double alpha,
        double delta,
        double prev_output) noexcept {
    return weight + (alpha * delta * prev_output);
}