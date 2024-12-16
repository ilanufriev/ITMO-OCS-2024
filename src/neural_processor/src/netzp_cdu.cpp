#include "netzp_cdu.hpp"
#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_wait.h"
#include <iomanip>
#include <stdexcept>
#include <string>

namespace netzp {

void CentralDispatchUnit::AtCoreReady() {
    core_finished_ = true;
}

void CentralDispatchUnit::AtMemReply() {
    has_mem_reply_ = true;
}

bool CentralDispatchUnit::CheckAllCoreOutputs() {
    bool all_ready = true;
    for (int i = 0; i < CORE_COUNT; i++) {
        const auto& core_output = core_outputs_[i].read();
        const auto neuron = core_output.data.neuron;

        // DEBUG_OUT_MODULE(1) << "neuron[" << +core_output.data.layer << ", "
        //                     << +core_output.data.neuron << "] here!" << std::endl;

        if (outputs_ready_[neuron] == false && core_ready_[i].read() == true) {
            AddOutput(core_output.output, neuron);
        }
    }

    for (int i = 0; i < outputs_size_; i++) {
        if (outputs_ready_[i] == false) {
            all_ready = false;
        }
    }

    // DEBUG_OUT_MODULE(1) << "Outputs ready:" << std::endl;
    // for (int i = 0; i < outputs_size_; i++) {
    //     DEBUG_OUT_MODULE(1) << "outputs_ready_[" << i << "] = " << outputs_ready_[i] << std::endl;
    // }

    return all_ready;
}

void CentralDispatchUnit::ResetOutputs() {
    for (int i = 0; i < outputs_.max_size(); i++) {
        outputs_[i] = 0;
        outputs_ready_[i] = false;
    }

    outputs_size_ = 0;
}

void CentralDispatchUnit::ResetNeurons() {
    neurons_size_ = 0;
}

void CentralDispatchUnit::AddNeuron(const NeuronData& data) {
    if (neurons_size_ >= neurons_.max_size()) {
        throw std::invalid_argument("neurons size exceeded");
    }

    neurons_[neurons_size_++] = data;
}

NeuronData CentralDispatchUnit::PopNeuron() {
    if (neurons_size_ == 0) {
        throw std::invalid_argument("Popping an empty stack");
    }

    return neurons_[--neurons_size_];
}

void CentralDispatchUnit::AddOutput(fp_t value, size_t index) {
    if (index >= outputs_.max_size())
        throw std::invalid_argument("Index " + std::to_string(index) + " out of bounds");

    outputs_[index] = value;
    outputs_ready_[index] = true;
}

void CentralDispatchUnit::AtStart() {
    if (start->read() == false) {
        finished_ = false;
    }
}

void CentralDispatchUnit::AssignNeurons() {
    for (int i = 0; i < CORE_COUNT; i++) {
        const auto& core_output = core_outputs_[i].read();
        const auto neuron = core_output.data.neuron;
        const bool core_ready = core_ready_[i].read();

        if (core_ready || core_cold_[i]) {
            if (!core_cold_[i]) {
                DEBUG_OUT_MODULE(1) << "GOT OUTPUT " << core_output << " from core " << i << std::endl;
                AddOutput(core_output.output, neuron);
            }

            if (neurons_size_ != 0) {
                NeuronData ndata = PopNeuron();

                ComputationData cdata;
                cdata.data = std::move(ndata);
                cdata.inputs = std::vector(inputs_.begin(), inputs_.begin() + inputs_size_);

                core_inputs_[i].write(cdata);
                core_cold_[i] = false;

                DEBUG_OUT_MODULE(1) << "ASSIGNED " << ndata << " to core " << i << std::endl;
            }
        }
    }
}

void CentralDispatchUnit::MainProcess() {
    const offset_t neurons_off                   = InOutController::NETZ_DATA_OFFSET + 1;
    const size_t   neuron_static_size            = sizeof(NeuronData::count_type) * 3;
    const offset_t neuron_data_layer_off         = 0;
    const offset_t neuron_data_neuron_off        = 1;
    const offset_t neuron_data_weights_count_off = 2;
    const offset_t neuron_data_weights_off       = 3;

    while (true) {
        has_mem_reply_ = false;

        for (int i = 0; i < CORE_COUNT; i++) {
            core_cold_[i] = true;
        }

        sc_core::wait();
        if (rst.read()) {
            continue;
        }

        if (!(start.read() == true) || finished_ == true) {
            continue;
        }

        // fetch inputs
        DataVector<MemRequest> input_req;
        input_req.data = ReadMemorySpanRequests(InOutController::INPUTS_OFFSET,
                                                InOutController::INPUT_COUNT,
                                                MASTER_ID);
        while (true) {
            sc_core::wait();
            mem_requests->write(input_req);
            if (has_mem_reply_) {
                const auto bytes = RepliesToBytes(mem_replies->read().data);

                for (int i = 0; i < InOutController::INPUT_COUNT; i++) {
                    inputs_[i] = bytes[i];
                }

                inputs_size_ = InOutController::INPUT_COUNT;
                has_mem_reply_ = false;
                break;
            }
        }

        // fetch_neuron_count
        DEBUG_OUT(1) << "fetch" << std::endl;
        uchar neuron_count = 0;

        DataVector<MemRequest> netz_req;
        netz_req.data = ReadMemorySpanRequests(InOutController::NETZ_DATA_OFFSET,
                                                    sizeof(neuron_count), MASTER_ID);
        while (true) {
            sc_core::wait();
            mem_requests->write(netz_req);
            if (has_mem_reply_) {
                auto bytes     = RepliesToBytes(mem_replies->read().data);
                neuron_count   = *bytes.data();
                has_mem_reply_ = false;

                DEBUG_OUT(1) << +neuron_count << std::endl;
                break;
            }
        }

        DEBUG_OUT(1) << "neuron fetch" << std::endl;

        // And now we start fetching neurons one by one
        NeuronData ndata;
        ndata.neuron        = 0;
        ndata.layer         = 0;
        outputs_size_       = 0;

        offset_t current_offset = neurons_off;
        for (int k = 0; k < neuron_count; k++) {
            sc_core::wait();

            DEBUG_OUT(1) << "neuron #" << k << std::endl;

            // First, get static data
            DataVector<MemRequest> neuron_req;
            neuron_req.data = ReadMemorySpanRequests(current_offset, neuron_static_size,
                                                     MASTER_ID);

            NeuronData ndata_next;
            while (true) {
                sc_core::wait();
                mem_requests->write(neuron_req);
                if (has_mem_reply_) {
                    const auto bytes         = RepliesToBytes(mem_replies->read().data);
                    ndata_next.layer         = bytes.at(neuron_data_layer_off);
                    ndata_next.neuron        = bytes.at(neuron_data_neuron_off);
                    ndata_next.weights_count = bytes.at(neuron_data_weights_count_off);
                    has_mem_reply_           = false;
                    break;
                }
            }

            DEBUG_OUT(1) << "check layer" << std::endl;

            // If suddenly the layer is now different, we first wait for the previous layer
            // to finish
            if (ndata_next.layer != ndata.layer) {
                while (true) {
                    sc_core::wait();

                    while (neurons_size_ > 0) {
                        sc_core::wait();
                        AssignNeurons();
                    }

                    // Check if all of them finished
                    bool all_ready = CheckAllCoreOutputs();

                    // If finished, get the results and put them into inputs array
                    // for the next layer
                    if (all_ready) {
                        inputs_      = outputs_;
                        inputs_size_ = outputs_size_;

                        ResetOutputs();
                        break;
                    }
                }
                // resume computations
            }

            outputs_size_++;

            DEBUG_OUT_MODULE(1) << "Get weights" << std::endl;

            // Then get the weights
            DataVector<MemRequest> weights_req;
            weights_req.data = ReadMemorySpanRequests(current_offset + neuron_data_weights_off,
                                                      ndata_next.weights_count * sizeof(fp_t),
                                                      MASTER_ID);
            std::vector<uchar> weights_bytes;

            while (true) {
                sc_core::wait();
                mem_requests->write(weights_req);
                if (has_mem_reply_) {
                    weights_bytes  = RepliesToBytes(mem_replies->read().data);
                    has_mem_reply_ = false;
                    break;
                }
            }

            ndata_next.weights = BytesToFloatingPoints(weights_bytes);

            // Finally, a neuron
            ndata = ndata_next;

            AddNeuron(ndata);
            if (neurons_size_ == neurons_.max_size()) {
                while (neurons_size_ > 0) {
                    sc_core::wait();
                    AssignNeurons();
                }
            }

            DEBUG_OUT_MODULE(1) << "Outputs ready:" << std::endl;
            for (int i = 0; i < 50; i++) {
                DEBUG_OUT_MODULE(1) << "outputs_ready_[" << i << "] = " << outputs_ready_[i] << std::endl;
            }

            // move to the next
            current_offset += ndata.SizeInBytes();
        }

        while (true) {
            DEBUG_OUT_MODULE(1) << "Waiting for the cores to finish" << std::endl;
            DEBUG_OUT_MODULE(1) << PRINTVAL(outputs_size_) << std::endl;
            sc_core::wait();
            bool all_ready = CheckAllCoreOutputs();
            if (all_ready) {
                break;
            }
        }

        DEBUG_OUT_MODULE(1) << "Got outputs:" << std::endl;

        // DEBUG_OUT_MODULE(1) << "Outputs ready:" << std::endl;
        // for (int i = 0; i < 50; i++) {
        //     DEBUG_OUT_MODULE(1) << "outputs_ready_[" << i << "] = " << outputs_ready_[i] << std::endl;
        // }

        DEBUG_OUT_MODULE(1) << PRINTVAL(outputs_[0]) << std::endl;
        DEBUG_OUT_MODULE(1) << PRINTVAL(outputs_[1]) << std::endl;
        DEBUG_OUT_MODULE(1) << PRINTVAL(outputs_[2]) << std::endl;

        finished_ = true;
    }
}

CentralDispatchUnit::CentralDispatchUnit(sc_core::sc_module_name const&) {
    for (int i = 0; i < CORE_COUNT; i++) {
        std::string name = "Compcore_" + std::to_string(i);
        compcore[i] = new ComputCore(name.c_str());

        compcore[i]->clk(clk);
        compcore[i]->rst(rst);
        compcore[i]->input_data(core_inputs_[i]);
        compcore[i]->output_data(core_outputs_[i]);
        compcore[i]->ready(core_ready_[i]);
    }

    SC_THREAD(MainProcess);
    sensitive << clk.pos();

    SC_METHOD(AtCoreReady);
    for (int i = 0; i < CORE_COUNT; i++) sensitive << core_ready_[i];

    SC_METHOD(AtMemReply);
    sensitive << mem_replies;
}

} // namespace netzp