#include "netz.hpp"
#include "netz_formulas.hpp"
#include <random>
#include <stdexcept>


std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dis(0.0, 1.0);

double GetRandomDouble() {
	return dis(gen);
}

netz::Neuron& netz::Neuron::AddInput(double input) {
	__inputs.push_back(input);
	__weights.push_back(GetRandomDouble());
	__needs_recalculation = true;

	return *this;
}

netz::Neuron& netz::Neuron::SetInput(size_t index, double input) {
	if (index >= __inputs.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}
	__inputs[index] = input;
	__needs_recalculation = true;

	return *this;
}

netz::Neuron& netz::Neuron::ResetInputs() {
	__inputs.clear();
	__needs_recalculation = true;

	return *this;
}

double netz::Neuron::GetOutput() const {
	if (__needs_recalculation) {
		__output = __activation_func(
			math::DotProduct(__inputs, __weights));
		__needs_recalculation = false;
	}

	return __output;
}

netz::Neuron& netz::Neuron::SetWeight(size_t index, double weight) {
	if (index >= __weights.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	__weights[index] = weight;
	__needs_recalculation = true;

	return *this;
}

size_t netz::Neuron::InputSize() const {
	return __inputs.size();
}

double netz::Neuron::GetInput(size_t index) const {
	if (index >= __inputs.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	return __inputs.at(index);
}

double netz::Neuron::GetWeight(size_t index) const {
	if (index >= __weights.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	return __weights.at(index);
}