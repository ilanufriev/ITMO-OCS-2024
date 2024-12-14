#include "netzp_cdu.hpp"
#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module.h"
#include <string>

namespace netzp {

void CentralDispatchUnit::AtCoreReady() {
    core_finished_ = true;
}

void CentralDispatchUnit::AtMemReply() {
    has_mem_reply_ = true;
}

void CentralDispatchUnit::MainProcess() {
    const offset_t neurons_off                   = InOutController::NETZ_DATA_OFFSET + 1;
    const size_t   neuron_static_size            = sizeof(NeuronData::count_type) * 3;
    const offset_t neuron_data_layer_off         = 0;
    const offset_t neuron_data_neuron_off        = 1;
    const offset_t neuron_data_weights_count_off = 2;

    while (true) {
        uchar neuron_count   = 0;
        uchar current_neuron = 0;

        sc_core::wait();
        if (rst.read()) {
            continue;
        }

        has_mem_reply_ = false;

        // fetch_neuron_count

        DataVector<MemRequest> netz_requests;
        netz_requests.data = ReadMemorySpanRequests(InOutController::NETZ_DATA_OFFSET,
                                                    sizeof(neuron_count), MASTER_ID);

        while (true) {
            sc_core::wait();
            mem_requests->write(netz_requests);
            if (has_mem_reply_) {
                auto bytes = RepliesToByes(mem_replies->read().data);
                neuron_count = *bytes.data();
                has_mem_reply_ = false;
                break;
            }
        }

        // end fetch_neuron_count

        // fetch_neurons_per_layer
        offset_t current_offset = neurons_off;
        std::vector<uchar> buffer;

        while (true) {
            sc_core::wait();

            DataVector<MemRequest> neuron_requests;
            netz_requests.data = ReadMemorySpanRequests(current_offset, neuron_static_size, MASTER_ID);
            mem_requests->write(netz_requests);

            NeuronData n;

            if (has_mem_reply_) {
                auto bytes      = RepliesToByes(mem_replies->read().data);
                n.layer         = bytes.at(neuron_data_layer_off);
                n.neuron        = bytes.at(neuron_data_neuron_off);
                n.weights_count = bytes.at(neuron_data_weights_count_off);
            }

            current_neuron++;
        }

        if (core_finished_) {
            for (int i = 0; i < CORE_COUNT; i++) {
                if (core_ready_[i].read() == true) {
                    const auto& core_output = core_outputs_[i].read();
                    outputs_[core_output.data.neuron] = core_output.output;
                }
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
}

} // namespace netzp