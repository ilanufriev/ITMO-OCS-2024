#include "netzp_io.hpp"
#include "netzp_config.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include <linux/limits.h>
#include <vector>

namespace netzp {

bool NeuronData::operator==(const NeuronData& other) const {
    return layer == other.layer                 &&
           neuron == other.neuron               &&
           weights_count == other.weights_count &&
           weights == other.weights;
}

const std::vector<uchar> NeuronData::Serialize() const {
    std::vector<uchar> result;

    result.push_back(layer);
    result.push_back(neuron);
    result.push_back(weights_count);

    for (float weight : weights) {
        for (uchar byte : ToBytesVector(weight)) {
            result.push_back(byte);
        }
    }

    result.shrink_to_fit();
    return result;
}

std::ostream& operator<<(std::ostream& out, const NeuronData& neuron_data) {
    out << "NeuronData { "
        << "Layer: " << static_cast<int>(neuron_data.layer) << ", "
        << "Neuron: " << static_cast<int>(neuron_data.neuron) << ", "
        << "Weights Count: " << static_cast<int>(neuron_data.weights_count) << ", "
        << "Weights: [";

    for (size_t i = 0; i < neuron_data.weights.size(); ++i) {
        out << neuron_data.weights[i];
        if (i < neuron_data.weights.size() - 1) {
            out << ", ";
        }
    }

    out << "] }";
    return out;
}

bool NetzwerkData::operator==(const NetzwerkData& other) const {
    return neurons_count == other.neurons_count &&
           neurons == other.neurons;
}

const std::vector<uchar> NetzwerkData::Serialize() const {
    std::vector<uchar> result;

    result.push_back(neurons_count);
    for (const NeuronData& n : neurons) {
        for (const auto byte : n.Serialize()) {
            result.push_back(byte);
        }
    }

    result.shrink_to_fit();
    return result;
}

std::ostream& operator<<(std::ostream& out, const NetzwerkData& netz_data) {
    out << "NetzwerkData { ";
    out << "Neurons Count: " << static_cast<int>(netz_data.neurons_count) << ", "
        << "Neurons: [";

    for (size_t i = 0; i < netz_data.neurons.size(); ++i) {
        out << netz_data.neurons[i];
        if (i < netz_data.neurons.size() - 1) {
            out << ", ";
        }
    }

    out << "] }";
    return out;
}

std::vector<MemRequest>
InOutController::BytesToRequests(const std::vector<uchar>& bytes, offset_t offset) const {
    std::vector<MemRequest> result;
    for (const auto byte : bytes) {
        result.emplace_back();
        result.back().addr      = offset;
        result.back().data_wr   = byte;
        result.back().master_id = MASTER_ID;
        result.back().op_type   = MemOperationType::WRITE;

        offset++;
    }

    result.shrink_to_fit();
    return result;
}

void InOutController::MainProcess() {
    static constexpr int DEBUG_MSG_LEVEL = 1;

    while (true) {
        sc_core::wait();
        new_reply_ = false;

        DataVector<MemRequest> input_data_requests;
        DataVector<MemRequest> netz_data_requests;

        // Transform input bytes into requests
        if (input_data_changed_) {
            DEBUG_OUT(DEBUG_MSG_LEVEL) << "Input data updated" << std::endl;

            for (int i = 0; i < INPUT_COUNT; i++) {
                input_data_requests.data.emplace_back();
                input_data_requests.data.back().data_wr = data_inputs[i]->read() == true
                                                        ? 0x01
                                                        : 0x00;
                input_data_requests.data.back().addr      = INPUTS_OFFSET + i;
                input_data_requests.data.back().master_id = MASTER_ID;
                input_data_requests.data.back().op_type   = MemOperationType::WRITE;
            }

            input_data_changed_ = false;
        }

        // Transform netzwerk data into requests
        if (netz_data_changed_) {
            DEBUG_OUT(DEBUG_MSG_LEVEL) << "Netz data updated" << std::endl;

            netz_data_requests.data = std::move(BytesToRequests(netz_data->read().Serialize(),
                                                                NETZ_DATA_OFFSET));
            netz_data_changed_ = false;
        }

        while (!input_data_requests.data.empty()) {
            sc_core::wait();
            requests->write(input_data_requests);

            if (new_reply_) {
                input_data_requests.data.clear();
                new_reply_ = false;
            }
        }

        while (!netz_data_requests.data.empty()) {
            sc_core::wait();

            requests->write(netz_data_requests);

            if (new_reply_) {
                netz_data_requests.data.clear();
                new_reply_ = false;
            }
        }
    }
}

void InOutController::AtNetzDataChange() {
    netz_data_changed_ = true;
}

void InOutController::AtDataInputChange() {
    input_data_changed_ = true;
}

void InOutController::AtReply() {
    new_reply_ = true;
}

InOutController::InOutController(sc_core::sc_module_name const&) {
    SC_THREAD(MainProcess);
    sensitive << clk.pos();

    SC_METHOD(AtDataInputChange);
    for (int i = 0; i < INPUT_COUNT; i++) sensitive << data_inputs[i];

    SC_METHOD(AtNetzDataChange);
    sensitive << netz_data;

    SC_METHOD(AtReply);
    sensitive << replies;
}

} // namespace netzp