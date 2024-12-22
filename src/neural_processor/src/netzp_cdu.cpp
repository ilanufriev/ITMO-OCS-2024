#include "netzp_cdu.hpp"
#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_wait.h"
#include <stdexcept>
#include <string>

namespace netzp {

void CentralDispatchUnit::AtCoreReady() {
}

void CentralDispatchUnit::AtMemReply() {
    has_mem_reply_ = true;
}

void CentralDispatchUnit::CheckAllCoreOutputs() {
    for (int i = 0; i < CORE_COUNT; i++) {
        const auto& core_output = core_outputs_[i].read();
        const auto neuron = core_output.data.neuron;

        if (outputs_ready_[neuron] == false && core_ready_[i].read() == true && !core_cold_[i]) {
            DEBUG_OUT_MODULE(1) << "Got output " << core_output << " from core " << i << std::endl;
            AddOutput(core_output.output, neuron);
            core_cold_[i] = true;
        }
    }
}

bool CentralDispatchUnit::IsAllReady() const {
    bool all_ready = true;
    for (int i = 0; i < outputs_size_; i++) {
        if (outputs_ready_[i] == false) {
            all_ready = false;
        }
    }

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

void CentralDispatchUnit::AddOutput(fp_t value, CentralDispatchUnit::size_type index) {
    if (index >= outputs_.max_size())
        throw std::invalid_argument("Index " + std::to_string(index) + " out of bounds");

    outputs_[index] = value;
    outputs_ready_[index] = true;
}

void CentralDispatchUnit::AssignNeurons() {
    for (int i = 0; i < CORE_COUNT; i++) {
        const auto& core_output = core_outputs_[i].read();
        const auto neuron = core_output.data.neuron;
        const bool core_ready = core_ready_[i].read();

        if ((core_ready || core_cold_[i]) && (neurons_size_ != 0)) {
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

void CentralDispatchUnit::ResetCores() {
    for (int i = 0; i < CORE_COUNT; i++) {
        core_cold_[i] = true;
    }
}

void CentralDispatchUnit::MainProcess() {
    const offset_t neurons_off                   = InOutController::NETZ_DATA_OFFSET + 1;
    const size_t   neuron_static_size            = sizeof(NeuronData::count_type) * 3;
    const offset_t neuron_data_layer_off         = 0;
    const offset_t neuron_data_neuron_off        = 1;
    const offset_t neuron_data_weights_count_off = 2;
    const offset_t neuron_data_weights_off       = 3;

    DataVector<MemRequest> ready_byte_reqs;
    ready_byte_reqs.data.emplace_back();
    ready_byte_reqs.data.back().data_wr   = 0x00;
    ready_byte_reqs.data.back().addr      = InOutController::IO_FLAGS_ADDR;
    ready_byte_reqs.data.back().master_id = MASTER_ID;
    ready_byte_reqs.data.back().op_type   = MemOperationType::READ;

    while (true) {
        has_mem_reply_ = false;

        sc_core::wait();

        if (rst.read()) {

            finished->write(false);

            has_mem_reply_ = false;
            ResetCores();
            ResetOutputs();
            ResetNeurons();
            continue;
        }

        if (!start.read() || finished.read()) {
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
        ndata.neuron  = 0;
        ndata.layer   = 0;
        outputs_size_ = 0;

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

                    CheckAllCoreOutputs();
                    AssignNeurons();

                    sc_core::wait();

                    // Check if all of them finished
                    bool all_ready = IsAllReady();

                    // If finished, get the results and put them into inputs array
                    // for the next layer
                    if (all_ready) {
                        inputs_      = outputs_;
                        inputs_size_ = outputs_size_;

                        ResetOutputs();
                        ResetCores();
                        ResetNeurons();

                        DEBUG_OUT_MODULE(1) << "Outputs ready after reset:" << std::endl;
                        for (int i = 0; i < 50; i++) {
                            DEBUG_OUT_MODULE(1) << "outputs_ready_[" << i << "] = " << outputs_ready_[i] << std::endl;
                        }

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
                    CheckAllCoreOutputs();
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

        // this wait() call is necessary for the last layer of neurons to be
        // checked correctly.
        sc_core::wait();

        while (true) {
            sc_core::wait();

            CheckAllCoreOutputs();
            AssignNeurons();

            sc_core::wait();

            bool all_ready = IsAllReady();
            if (all_ready) {
                break;
            }
        }

        std::vector<uchar> output_bytes;
        for (const auto byte : ToBytesVector(outputs_size_))
            output_bytes.push_back(byte);


        for (int i = 0; i < outputs_size_; i++) {
            for (const auto byte : ToBytesVector(outputs_[i]))
                output_bytes.push_back(byte);
        }

        DataVector<MemRequest> output_req;
        output_req.data = BytesToWriteRequests(InOutController::IO_OUTPUTS_BASE_ADDR,
                                               output_bytes, MASTER_ID);

        while (true) {
            sc_core::wait();
            mem_requests->write(output_req);

            if (has_mem_reply_) {
                finished->write(true);
                has_mem_reply_ = false;
                break;
            }
        }
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