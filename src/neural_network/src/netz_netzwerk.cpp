#include "netz.hpp"
#include "netz_formulas.hpp"
#include <initializer_list>
#include <iostream>
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

std::ostream& netz::Netzwerk::DumpStructure(std::ostream& out) const {
    bool is_first = true;
    out << ">" << inputs__.size() << std::endl;
    for (int k = 0; k < layers__.size(); k++) {
        for (int i = 0; i < layers__.at(k).size(); i++) {
        out << "@" << k << "/" << i << std::endl;

            for (int j = 0; j < layers__.at(k).at(i).InputSize(); j++) {
                if (!is_first) {
                    out << std::endl;
                }

                // is_first = false;
                out << "#" << layers__.at(k).at(i).GetWeight(j) << std::endl;
            }
        }
    }

    return out;
}

netz::Netzwerk netz::Netzwerk::ReadStructure(std::istream& in) {
    std::string line;
    std::vector<std::vector<std::vector<double>>> weights;
    int input_count = 0;

    int layer = 0;
    int neuron = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue; // Skip empty lines
        std::cout << "Line: " << line << std::endl;

        if (line[0] == '>') {
            std::string input_count_str = line.substr(1); // remove '>'
            input_count = std::stoi(input_count_str);

        } else if (line[0] == '@') {
            // Parse the indices
            std::string indices = line.substr(1); // Remove '@'
            size_t delimiterPos = indices.find('/');
            if (delimiterPos == std::string::npos) {
                throw std::runtime_error("Invalid index format.");
            }

            layer  = std::stoi(indices.substr(0, delimiterPos));
            neuron = std::stoi(indices.substr(delimiterPos + 1));

            // Ensure the array is large enough
            if (layer >= weights.size()) {
                weights.resize(layer + 1);
            }

            if (neuron >= weights[layer].size()) {
                weights[layer].resize(neuron + 1);
            }

        } else if (line[0] == '#') {
            double value = std::stod(line.substr(1)); // Remove '#'
            weights[layer][neuron].push_back(value);

        }
    }

    Netzwerk netz;

    for (layer = 0; layer < weights.size(); layer++) {
        netz.AddLayer(weights.at(layer).size());
    }

    for (int i = 0; i < input_count; i++) {
        netz.AddInput(0);
    }

    for (layer = 0; layer < weights.size(); layer++) {
        for (neuron = 0; neuron < weights.at(layer).size(); neuron++) {
            for (int weight = 0; weight < weights.at(layer).at(neuron).size(); weight++) {
                netz.layers__.at(layer).at(neuron).SetWeight(
                        weight,
                        weights.at(layer).at(neuron).at(weight)
                    );
            }
        }
    }

    return netz;
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