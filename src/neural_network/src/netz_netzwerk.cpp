#include "netz.hpp"
#include "netz_formulas.hpp"
#include <initializer_list>
#include <stdexcept>

netz::Netzwerk::Netzwerk(std::initializer_list<double> inputs,
        std::initializer_list<size_t> layer_sizes)
        : inputs__(inputs), layers__() {
    for (const size_t size : layer_sizes) {
        AddLayer(size);
    }
}

netz::Netzwerk& netz::Netzwerk::AddLayer(size_t s) {
    std::function<void(Neuron&)> connect_neuron;

    if (!layers__.empty()) {
        connect_neuron = [this](Neuron& neuron) -> void {
            const Layer& last = *(layers__.end() - 2);
            for (const Neuron& n : last) {
                neuron.AddInput(n.GetOutput());
            }
        };
    } else {
        connect_neuron = [this](Neuron& neuron) -> void {
            for (double input : inputs__) {
                neuron.AddInput(input);
            }
        };
    }

    layers__.emplace_back(s, Neuron(math::actfunc::Sigma));

    for (	auto iter = layers__.back().begin();
        iter != layers__.back().end(); iter++ )
        connect_neuron(*iter);

    needs_recalculation__ = true;

    return *this;
}

netz::Netzwerk& netz::Netzwerk::AddInput(double input) {
    inputs__.push_back(input);
    needs_recalculation__ = true;

    if (layers__.empty()) return *this;

    for (int i = 0; i < layers__.front().size(); i++) {
        Neuron& n = layers__.front().at(i);
        n.AddInput(input);
    }

    return *this;
}

void netz::Netzwerk::Update() {
    for (size_t k = 1; k < layers__.size(); k++) {
        Layer& prev_layer = layers__[k - 1];
        for (Neuron& n : layers__.at(k)) {
            for (size_t po = 0; po < prev_layer.size(); po++) {
                n.SetInput(po, prev_layer.at(po).GetOutput());
            }
        }
    }
}

std::vector<double> netz::Netzwerk::GetOuputs() {
    if (needs_recalculation__) {
        Update();
        needs_recalculation__ = false;
    }

    std::vector<double> outputs;
    for (const Neuron& n : layers__.back()) {
        outputs.push_back(n.GetOutput());
    }

    return outputs;
}

netz::Netzwerk& netz::Netzwerk::SetInput(size_t index, double input) {
    if (index >= inputs__.size()) {
        throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
    }

    for (int i = 0; i < layers__.front().size(); i++) {
        Neuron& n = layers__.front().at(i);
        n.SetInput(index, input);
    }

    inputs__[index] = input;
    needs_recalculation__ = true;

    return *this;
}

double netz::Netzwerk::GetInput(size_t index) const {
    if (index >= inputs__.size()) {
        throw std::invalid_argument(ErrMsg(ERR_MSG_INDEX_OOB));
    }

    return inputs__.at(index);
}

std::ostream& netz::Netzwerk::DumpWeights(std::ostream& out) const {
    bool is_first = true;
    for (int k = 0; k < layers__.size(); k++) {
        for (int i = 0; i < layers__.at(k).size(); i++) {
            for (int j = 0; j < layers__.at(k).at(i).InputSize(); j++) {
                if (!is_first) {
                    out << std::endl;
                }

                is_first = false;
                out << layers__.at(k).at(i).GetWeight(j);
            }
        }
    }
    return out;
}

netz::Netzwerk& netz::Netzwerk::ReadWeights(std::istream& in) {
    bool is_first = true;
    for (size_t k = 0; k < layers__.size(); k++) {
        for (size_t i = 0; i < layers__.at(k).size(); i++) {
            for (size_t j = 0; j < layers__.at(k).at(i).InputSize(); j++) {
                double w;
                in >> w;
                layers__.at(k).at(i).SetWeight(j, w);
            }
        }
    }

    return *this;
}