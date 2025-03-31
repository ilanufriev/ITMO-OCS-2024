#pragma once
#include <functional>
#include <initializer_list>
#include <vector>
#include "netz_formulas.hpp"

namespace netz {
    class Netzwerk;
    class Neuron;
    using Layer = std::vector<netz::Neuron>;
}

class netz::Netzwerk {
public:
    Netzwerk() = default;
    Netzwerk(std::initializer_list<double> inputs,
        std::initializer_list<size_t> layer_sizes);

    Netzwerk& AddInput(double input);
    Netzwerk& AddLayer(size_t s);

    Netzwerk& SetInput(size_t index, double input);
    double GetInput(size_t index) const;

    template<typename NumberContainer>
    void AdjustWeights(double alpha, const NumberContainer& expected_values);

    std::vector<double> GetOuputs();

    std::ostream& DumpWeights(std::ostream& out) const;
    std::ostream& DumpStructure(std::ostream& out) const;

    Netzwerk& ReadWeights(std::istream& in);
    static netz::Netzwerk ReadStructure(std::istream& in);
private:
    void Update();

    std::vector<Layer>	layers__;
    std::vector<double>	inputs__;
    mutable bool		needs_recalculation__ = true;
};

class netz::Neuron {
public:
    Neuron() = delete;
    explicit Neuron(std::function<double(double)> activation_func)
        : activation_func__(activation_func) {}

    Neuron& AddInput(double input);
    Neuron& SetInput(size_t index, double input);
    Neuron& SetWeight(size_t index, double weight);
    Neuron& ResetInputs();
    double GetInput(size_t index) const;
    double GetWeight(size_t index) const;
    double GetOutput() const;
    size_t InputSize() const;
private:
    std::vector<double>             inputs__;
    std::vector<double>             weights__;
    std::function<double(double)>	activation_func__;
    mutable double                  output__;
    mutable bool                    needs_recalculation__ = true;
};

template<typename NumberContainer>
void netz::Netzwerk::AdjustWeights(double alpha,
        const NumberContainer& expected_values) {
    Layer *next_layer = &layers__.back();

    std::vector<double> deltas_next;

    // Сначала отдельно расчитывается дельта для последнего слоя
    for (size_t i = 0; i < next_layer->size(); i++) {
        Neuron& n = next_layer->at(i);
        double delta = math::DeltaCoeffLastLayer(expected_values.at(i),
            n.GetOutput());

        for (size_t j = 0; j < n.InputSize(); j++) {
            double new_weight = n.GetWeight(j)
                    + alpha * delta * n.GetInput(j);

            n.SetWeight(j, new_weight);
        }

        deltas_next.push_back(delta);
    }

    // Потом для всех остальных
    for (auto iter = layers__.rbegin() + 1; iter != layers__.rend(); iter++) {
        Layer& l = *iter;
        std::vector<double> deltas;

        for (size_t i = 0; i < l.size(); i++) {
            Neuron& n = l.at(i);
            std::vector<double> weights_next;
            for (size_t k = 0; k < next_layer->size(); k++) {
                const Neuron& n_next = next_layer->at(k);
                weights_next.push_back(n_next.GetWeight(i));
            }

            double delta = math::DeltaCoeffHiddenLayer(
                n.GetOutput(), deltas_next,
                weights_next);

            for (size_t j = 0; j < n.InputSize(); j++) {
                double new_weight = n.GetWeight(j)
                        + alpha * delta * n.GetInput(j);
                n.SetWeight(j, new_weight);
            }

            deltas.push_back(delta);
        }

        deltas_next = std::move(deltas);
        next_layer = &l;
    }

    needs_recalculation__ = true;
}