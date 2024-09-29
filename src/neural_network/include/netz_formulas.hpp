#pragma once
#include <functional>
#include <numeric>
#include <execution>
#include <stdexcept>
#include <sstream>

// Пространство имен нейронной сети
namespace netz {

#define ErrMsg(__msg) \
	ErrMsgImpl(__func__, __msg)

const std::string ERR_MSG_INDEX_OOB = "Index out of bounds!";
const std::string ERR_MSG_SIZES_DIFFER = "Candidates sizes differ!";

inline std::string ErrMsgImpl(const std::string& func_name,
		const std::string& msg) {
	std::ostringstream out;
	out << func_name << ": " << msg;
	return out.str();
}

}

// Математические функции
namespace netz::math {

template<typename NumberContainer>
double DotProduct(const NumberContainer& v1,
		const NumberContainer& v2);

template<typename NumberContainer>
double ErrorEstimation(const NumberContainer& expected_values,
		const NumberContainer& output_values);

double DeltaCoeffLastLayer(double expected_value,
		double output_value) noexcept;

template<typename NumberContainer>
double DeltaCoeffHiddenLayer(double output_value,
		const NumberContainer& deltas_next,
		const NumberContainer& weights_next);

double GetNewWeight(double weight, double alpha,
		double delta,
		double prev_output) noexcept;

} // namespace netz::math

// Функции активации
namespace netz::math::actfunc {

double Sigma(double x) noexcept;

double SigmaMirrored(double x) noexcept;

} // netz::math::actfunc

template<typename NumberContainer>
double netz::math::DotProduct(
		const NumberContainer& v1,
		const NumberContainer& v2) {

	if (v1.size() != v2.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_SIZES_DIFFER));
	}

	return std::transform_reduce(std::execution::par, v1.cbegin(),
			v1.cend(), v2.cbegin(),
			0.0, std::plus<>(),
			std::multiplies<>());
}

template<typename NumberContainer>
double netz::math::ErrorEstimation(
		const NumberContainer& expected_values,
		const NumberContainer& output_values) {
	if (expected_values.size() != output_values.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_SIZES_DIFFER));
	}

	return 0.5 * std::transform_reduce(std::execution::par,
			expected_values.cbegin(), expected_values.cend(),
			output_values.cbegin(), 0.0, std::plus<>(),
			std::minus<>() );
}

template<typename NumberContainer>
double netz::math::DeltaCoeffHiddenLayer(double output_value,
		const NumberContainer& deltas_next,
		const NumberContainer& weights_next) {
	return output_value * (1 - output_value) *
		netz::math::DotProduct(deltas_next, weights_next);
}