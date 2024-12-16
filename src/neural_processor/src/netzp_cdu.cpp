#include "netzp_cdu.hpp"
#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module.h"
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

        DEBUG_OUT_MODULE(1) << "neuron[" << +core_output.data.layer << ", "
                            << +core_output.data.neuron << "] here!" << std::endl;

        if (outputs_ready_[neuron] == false && core_ready_[i].read() == true) {
            // outputs_[neuron] = core_output.output;
            // outputs_ready_[neuron] = true;
            OutputAdd(core_output.output, neuron);
        }
    }

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

void CentralDispatchUnit::OutputAdd(fp_t value, size_t index) {
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

void CentralDispatchUnit::MainProcess() {
    const offset_t neurons_off                   = InOutController::NETZ_DATA_OFFSET + 1;
    const size_t   neuron_static_size            = sizeof(NeuronData::count_type) * 3;
    const offset_t neuron_data_layer_off         = 0;
    const offset_t neuron_data_neuron_off        = 1;
    const offset_t neuron_data_weights_count_off = 2;
    const offset_t neuron_data_weights_off       = 3;

    while (true) {
        has_mem_reply_ = false;

        // Means that the cores have not yet been assigned with any task
        std::array<bool, CORE_COUNT> core_cold;

        for (int i = 0; i < CORE_COUNT; i++) {
            core_cold[i] = true;
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

            // Now let's check the cores
            bool neuron_assigned = false;
            for (int i = 0; i < CORE_COUNT; i++) {
                const auto& core_output = core_outputs_[i].read();

                // If core ready
                if (core_ready_[i].read() == true || core_cold[i]) {

                    // Only put into outputs if it is from the current layer
                    // (cores may still hold values from previous layers which we do not
                    // want to bleed into next layer)

                    if (!core_cold[i] && core_output.data.layer == ndata.layer) {
                        // Put into outputs
                        OutputAdd(core_output.output, core_output.data.neuron);
                        DEBUG_OUT_MODULE(1) << "outputs[" << +core_output.data.neuron << "] = " << outputs_[core_output.data.neuron] << std::endl;
                    }

                    // Assign new neuron to one of the cores
                    if (!neuron_assigned) {

                        ComputationData cdata;
                        cdata.data   = ndata;
                        cdata.inputs = std::vector(inputs_.begin(), inputs_.begin() + inputs_size_);
                        core_inputs_[i].write(cdata);
                        neuron_assigned = true;

                        DEBUG_OUT_MODULE(1) << "Assigned compdata: " << cdata
                                            << " to core " << i << std::endl;
                        core_cold[i] = false;
                    }
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