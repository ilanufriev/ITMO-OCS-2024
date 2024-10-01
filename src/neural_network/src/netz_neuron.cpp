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
	inputs__.push_back(input);
	weights__.push_back(GetRandomDouble());
	needs_recalculation__ = true;

	return *this;
}

netz::Neuron& netz::Neuron::SetInput(size_t index, double input) {
	if (index >= inputs__.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}
	inputs__[index] = input;
	needs_recalculation__ = true;

	return *this;
}

netz::Neuron& netz::Neuron::ResetInputs() {
	inputs__.clear();
	needs_recalculation__ = true;

	return *this;
}

double netz::Neuron::GetOutput() const {
	if (needs_recalculation__) {
		output__ = activation_func__(
			math::DotProduct(inputs__, weights__));
		needs_recalculation__ = false;
	}

	return output__;
}

netz::Neuron& netz::Neuron::SetWeight(size_t index, double weight) {
	if (index >= weights__.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	weights__[index] = weight;
	needs_recalculation__ = true;

	return *this;
}

size_t netz::Neuron::InputSize() const {
	return inputs__.size();
}

double netz::Neuron::GetInput(size_t index) const {
	if (index >= inputs__.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	return inputs__.at(index);
}

double netz::Neuron::GetWeight(size_t index) const {
	if (index >= weights__.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	return weights__.at(index);
}