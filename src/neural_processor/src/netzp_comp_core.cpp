#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include <stdexcept>

namespace netzp {

constexpr char WEIGHTS_AND_INPUTS_DIFFER[] = "Weights and inputs are of different sizes";

ComputationData::ComputationData(): output(0) {}

// Overloading the << operator for ComputationData
std::ostream& operator<<(std::ostream& out, const ComputationData& computation_data) {
    out << "ComputationData { "
        << "Data: " << computation_data.data << ", "
        << "Inputs: [";

    for (size_t i = 0; i < computation_data.inputs.size(); ++i) {
        out << computation_data.inputs[i];
        if (i < computation_data.inputs.size() - 1) {
            out << ", ";
        }
    }

    out << "], Output: " << computation_data.output << " }";
    return out;
}

bool ComputationData::operator==(const ComputationData& other) const {
    return data == other.data &&
           inputs == other.inputs &&
           output == other.output;
}

void AccumulationCore::AtClk() {
    static constexpr int DEBUG_LEVEL_MSG = 1;
    if (rst.read()) {
        result->write(0);
    } else if (clk->read()) {
        result->write(product_next_);
    }
}

void AccumulationCore::AtData() {
    if (data->read().data.weights_count != data->read().inputs.size())
        throw std::invalid_argument(WEIGHTS_AND_INPUTS_DIFFER);

    product_next_ = 0;
    const NeuronData& neuron          = data->read().data;
    const std::vector<fp_t>& inputs = data->read().inputs;

    for (int i = 0; i < neuron.weights_count; i++) {
        product_next_ += neuron.weights.at(i) * inputs.at(i);
    }
}

AccumulationCore::AccumulationCore(sc_core::sc_module_name const&) {
    SC_METHOD(AtClk);
    sensitive << clk.pos();

    SC_METHOD(AtData);
    sensitive << data;
}

void ActivationCore::AtClk() {
    if (rst.read()) {
        result.write(0);
    } else if (clk.read()) {
        result.write(result_next_);
    }
}

void ActivationCore::AtData() {
    result_next_ = ActivationFunctionSigma(data->read());
}

fp_t ActivationCore::ActivationFunctionSigma(fp_t x) const {
    return 1 / (1 + std::exp(-x));
}

ActivationCore::ActivationCore(sc_core::sc_module_name const&) {
    SC_METHOD(AtClk);
    sensitive << clk.pos();

    SC_METHOD(AtData);
    sensitive << data;
}

void ComputCore::AtClk() {
    if (rst->read()) {
        output_data->write(ComputationData());
    } else if (clk->read()) {
        output_data->write(output_data_next_);
        ready->write(ready_next_);
    }
}

void ComputCore::AtOutputReady() {
    output_data_next_ = compdata_current_;
    output_data_next_.output = accumulator_out_.read();
    ready_next_ = true;
}

void ComputCore::AtInputData() {
    compdata_current_ = input_data->read();
    ready_next_ = false;
}

ComputCore::ComputCore(sc_core::sc_module_name const &name) {
    accumulator_ = new AccumulationCore("AccumulationCore");

    accumulator_->clk(clk);
    accumulator_->rst(rst);
    accumulator_->data(input_data);
    accumulator_->result(accumulator_out_);

    activator_   = new ActivationCore("ActivationCore");

    activator_->clk(clk);
    activator_->rst(rst);
    activator_->data(accumulator_out_);
    activator_->result(activator_out_);

    SC_METHOD(AtClk);
    sensitive << clk.pos();

    SC_METHOD(AtOutputReady);
    sensitive << accumulator_out_;
}

ComputCore::~ComputCore() {
    if (accumulator_)
        delete accumulator_;
    if (activator_)
        delete activator_;
}

} // namespace netzp