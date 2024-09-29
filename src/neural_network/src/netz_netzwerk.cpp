#include "netz.hpp"
#include "netz_formulas.hpp"
#include <initializer_list>
#include <stdexcept>

netz::Netzwerk::Netzwerk(std::initializer_list<double> inputs,
		std::initializer_list<size_t> layer_sizes)
		: __inputs(inputs), __layers() {
	for (const size_t size : layer_sizes) {
		AddLayer(size);
	}
}

netz::Netzwerk& netz::Netzwerk::AddLayer(size_t s) {
	std::function<void(Neuron&)> connect_neuron;

	if (!__layers.empty()) {
		connect_neuron = [this](Neuron& neuron) -> void {
			const Layer& last = *(__layers.end() - 2);
			for (const Neuron& n : last) {
				neuron.AddInput(n.GetOutput());
			}
		};
	} else {
		connect_neuron = [this](Neuron& neuron) -> void {
			for (double input : __inputs) {
				neuron.AddInput(input);
			}
		};
	}

	__layers.emplace_back(s, Neuron(math::actfunc::Sigma));

	for (	auto iter = __layers.back().begin();
		iter != __layers.back().end(); iter++ )
		connect_neuron(*iter);

	__needs_recalculation = true;

	return *this;
}

netz::Netzwerk& netz::Netzwerk::AddInput(double input) {
	__inputs.push_back(input);
	__needs_recalculation = true;

	if (__layers.empty()) return *this;

	for (int i = 0; i < __layers.front().size(); i++) {
		Neuron& n = __layers.front().at(i);
		n.AddInput(input);
	}

	return *this;
}

void netz::Netzwerk::Update() {
	for (size_t k = 1; k < __layers.size(); k++) {
		Layer& prev_layer = __layers[k - 1];
		for (Neuron& n : __layers.at(k)) {
			for (size_t po = 0; po < prev_layer.size(); po++) {
				n.SetInput(po, prev_layer.at(po).GetOutput());
			}
		}
	}
}

std::vector<double> netz::Netzwerk::GetOuputs() {
	if (__needs_recalculation) {
		Update();
		__needs_recalculation = false;
	}

	std::vector<double> outputs;
	for (const Neuron& n : __layers.back()) {
		outputs.push_back(n.GetOutput());
	}

	return outputs;
}

netz::Netzwerk& netz::Netzwerk::SetInput(size_t index, double input) {
	if (index >= __inputs.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	for (int i = 0; i < __layers.front().size(); i++) {
		Neuron& n = __layers.front().at(i);
		n.SetInput(index, input);
	}

	__inputs[index] = input;
	__needs_recalculation = true;

	return *this;
}

double netz::Netzwerk::GetInput(size_t index) const {
	if (index >= __inputs.size()) {
		throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
	}

	return __inputs.at(index);
}

std::ostream& netz::Netzwerk::DumpWeights(std::ostream& out) const {
	bool is_first = true;
	for (int k = 0; k < __layers.size(); k++) {
		for (int i = 0; i < __layers.at(k).size(); i++) {
			for (	int j = 0;
				j < __layers.at(k).at(i).InputSize();
				j++) {
				if (!is_first) {
					out << std::endl;
				}

				is_first = false;
				out << __layers.at(k).at(i).GetWeight(j);
			}
		}
	}
	return out;
}

netz::Netzwerk& netz::Netzwerk::ReadWeights(std::istream& in) {
	bool is_first = true;
	for (size_t k = 0; k < __layers.size(); k++) {
		for (size_t i = 0; i < __layers.at(k).size(); i++) {
			for (	size_t j = 0;
				j < __layers.at(k).at(i).InputSize();
				j++) {

				double w;
				in >> w;
				__layers.at(k).at(i).SetWeight(j, w);
			}
		}
	}

	return *this;
}