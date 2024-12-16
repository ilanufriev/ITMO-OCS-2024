#include <cstddef>
#include <fstream>
#include <iomanip>
#include <memory>
#include <systemc>
#include <iostream>
#include "netzp_cdu.hpp"
#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_time.h"

void RunCycles(sc_core::sc_signal<bool>& clk, int cycles, sc_core::sc_time_unit tu) {
    using namespace sc_core;

    for (int i = 0; i < cycles; i++) {
        clk = 0;
        sc_start(1, tu);
        clk = 1;
        sc_start(1, tu);
    }
    clk = 0;
}

enum {
    ARGV_IN_FILENAME = 1,
    ARGV_NETWORK_FILENAME = 2,
};

netzp::NetzwerkData ParseNetwork(std::istream& in) {
    std::string line;
    std::vector<std::vector<std::vector<double>>> weights;

    int input_count = 0;
    int layer = 0;
    int neuron = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue; // Skip empty lines

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

    netzp::NetzwerkData netz_data;

    netz_data.neurons_count = 0;
    for (const auto& l : weights) {
        netz_data.neurons_count += l.size();
    }

    for (layer = 0; layer < weights.size(); layer++) {
        for (neuron = 0; neuron < weights.at(layer).size(); neuron++) {
            auto& ndata = netz_data.neurons.emplace_back();
            ndata.layer = layer;
            ndata.neuron = neuron;

            for (int weight = 0; weight < weights.at(layer).at(neuron).size(); weight++) {
                ndata.weights.push_back(weights.at(layer).at(neuron).at(weight));
            }

            ndata.weights_count = ndata.weights.size();
        }
    }

    return netz_data;
}

int sc_main(int argc, char **argv) {
    using namespace sc_core;
    using namespace sc_dt;

    const char *input_filename = argv[ARGV_IN_FILENAME];
    const char *network_filename = argv[ARGV_NETWORK_FILENAME];

    std::ifstream inputs_file(input_filename);
    if (!inputs_file) {
        std::cout << "No such file: " << input_filename << std::endl;
        return 1;
    }



    std::ifstream network_file(network_filename);
    if (!network_file) {
        std::cout << "No such file: " << network_filename << std::endl;
        return 1;
    }

    netzp::NetzwerkData nd = ParseNetwork(network_file);
    DEBUG_OUT(1) << nd << std::endl;

    sc_signal<netzp::mem_data_t> data_rd(0);
    sc_signal<netzp::mem_data_t> data_wr(0);
    sc_signal<netzp::mem_addr_t> addr(0);
    sc_signal<bool>              w_en(0);
    sc_signal<bool>              r_en(0);
    sc_signal<bool>              ack_mem_to_con(0);
    sc_signal<bool>              ack_con_to_mem(0);

    const size_t port_count = CONFIG_MEMORY_CONTROLLER_MAX_CONNECTIONS;

    sc_vector<sc_signal<bool>>              access_granted("acc_granted", port_count);
    sc_vector<sc_signal<bool>>              access_request("acc_request", port_count);
    sc_vector<sc_signal<netzp::MemReply>>   reply("reply", port_count);
    sc_vector<sc_signal<netzp::MemRequest>> request("request", port_count);

    sc_vector<sc_signal<DataVector<netzp::MemRequest>>> requests_from_host("req_from_host", port_count);
    sc_vector<sc_signal<DataVector<netzp::MemReply>>> replies_to_host("rep_to_host", port_count);

    sc_signal<bool> rst(0);
    sc_signal<bool> clk(0);

    sc_signal<netzp::NetzwerkData> netz_data;
    sc_vector<sc_signal<bool>> input_signals("inputs", netzp::InOutController::INPUT_COUNT);
    for (int i = 0; i < input_signals.size() && !inputs_file.eof(); i++) {
        char c;
        inputs_file >> c;
        input_signals[i].write((c == '1' ? true : false));
    }

    netzp::CentralDispatchUnit cdu("cdu");

    sc_signal<bool> start;

    start.write(0);

    cdu.clk(clk);
    cdu.rst(rst);
    cdu.mem_replies(replies_to_host[1]);
    cdu.mem_requests(requests_from_host[1]);
    cdu.start(start);

    netzp::Mem memory("memory", 2 * netzp::KBYTE);

    memory.clk(clk);
    memory.rst(rst);

    memory.ack_in(ack_con_to_mem);
    memory.ack_out(ack_mem_to_con);
    memory.data_rd(data_rd);
    memory.data_wr(data_wr);
    memory.addr(addr);
    memory.w_en(w_en);
    memory.r_en(r_en);

    netzp::MemController bus("Membus");

    bus.clk(clk);
    bus.rst(rst);

    for (int i = 0; i < port_count; i++) {
        bus.access_granted[i](access_granted[i]);
        bus.access_request[i](access_request[i]);
        bus.requests_in[i](request[i]);
        bus.replies_out[i](reply[i]);
    }

    bus.ack_in(ack_mem_to_con);
    bus.ack_out(ack_con_to_mem);
    bus.data_rd(data_rd);
    bus.data_wr(data_wr);
    bus.addr(addr);
    bus.w_en(w_en);
    bus.r_en(r_en);

    netzp::MemIO iocon_memio("iocon_memio");

    iocon_memio.clk(clk);
    iocon_memio.rst(rst);

    iocon_memio.reply(reply[0]);
    iocon_memio.request(request[0]);
    iocon_memio.requests_from_host(requests_from_host[0]);
    iocon_memio.replies_to_host(replies_to_host[0]);
    iocon_memio.access_request(access_request[0]);
    iocon_memio.access_granted(access_granted[0]);

    netzp::MemIO cdu_memio("cdu_memio");

    cdu_memio.clk(clk);
    cdu_memio.rst(rst);

    cdu_memio.reply(reply[1]);
    cdu_memio.request(request[1]);
    cdu_memio.requests_from_host(requests_from_host[1]);
    cdu_memio.replies_to_host(replies_to_host[1]);
    cdu_memio.access_request(access_request[1]);
    cdu_memio.access_granted(access_granted[1]);

    netzp::InOutController iocon("iocon");

    iocon.clk(clk);
    iocon.rst(rst);

    iocon.netz_data(netz_data);
    for (int i = 0; i < netzp::InOutController::INPUT_COUNT; i++) {
        iocon.data_inputs[i](input_signals[i]);
    }

    iocon.requests(requests_from_host[0]);
    iocon.replies(replies_to_host[0]);

    rst = 1;
    clk = 0;
    RunCycles(clk, 5, SC_NS);

    rst = 0;
    clk = 0;
    RunCycles(clk, 5, SC_NS);

    netz_data.write(nd);
    RunCycles(clk, 15000, SC_NS);

    DEBUG_OUT(1) << "Memory dump: " << std::endl;
    memory.Dump(std::cout);

    start.write(true);
    RunCycles(clk, 100000, SC_NS);
    return 0;
}

int sc_main_old(int argc, char **argv) {
    using namespace sc_core;
    using namespace sc_dt;

    netzp::NeuronData neurons[4];
    neurons[0] = {
        .layer   = 0,
        .neuron  = 0,
        .weights_count = 3,
        .weights = {
            3.14, 2.34, 1.23
        }
    };

    neurons[1] = {
        .layer   = 0,
        .neuron  = 1,
        .weights_count = 3,
        .weights = {
            3.24, 1.34, 3.23
        }
    };

    neurons[2] = {
        .layer   = 1,
        .neuron  = 0,
        .weights_count = 3,
        .weights = {
            3.14, 2.4, 5.23
        }
    };

    neurons[3] = {
        .layer   = 1,
        .neuron  = 1,
        .weights_count = 2,
        .weights = {
            33.14, 32.34
        }
    };

    netzp::NetzwerkData netz = {
        .neurons_count = 4,
        .neurons = {
            neurons[0],
            neurons[1],
            neurons[2],
            neurons[3]
        }
    };

    auto serialized_neuron = neurons[0].Serialize();
    assert(neurons[0] == netzp::NeuronData::Deserialize(serialized_neuron.data()));

    auto serialized_netz = netz.Serialize();
    assert(netz == netzp::NetzwerkData::Deserialize(serialized_netz.data()));

    sc_signal<netzp::NetzwerkData> netz_data;

    sc_signal<bool> data_inputs[CONFIG_INPUT_PICTURE_HEIGHT * CONFIG_INPUT_PICTURE_WIDTH];
    for (config_int_t i = 0; i < CONFIG_INPUT_PICTURE_HEIGHT * CONFIG_INPUT_PICTURE_HEIGHT; i++) {
        data_inputs[i] = i % 2;
    }

    sc_signal<netzp::mem_data_t> data_rd(0);
    sc_signal<netzp::mem_data_t> data_wr(0);
    sc_signal<netzp::mem_addr_t> addr(0);
    sc_signal<bool>              w_en(0);
    sc_signal<bool>              r_en(0);
    sc_signal<bool>              ack_mem_to_con(0);
    sc_signal<bool>              ack_con_to_mem(0);

    const size_t port_count = CONFIG_MEMORY_CONTROLLER_MAX_CONNECTIONS;

    sc_vector<sc_signal<bool>>              access_granted("acc_granted", port_count);
    sc_vector<sc_signal<bool>>              access_request("acc_request", port_count);
    sc_vector<sc_signal<netzp::MemReply>>   reply("reply", port_count);
    sc_vector<sc_signal<netzp::MemRequest>> request("request", port_count);

    sc_signal<bool> rst(0);
    sc_signal<bool> clk(0);

    DataVector<netzp::MemRequest>            requests;
    sc_signal<DataVector<netzp::MemRequest>> requests_signal;
    sc_signal<DataVector<netzp::MemReply>>   replies_signal;

    netzp::Mem mem("Memory", netzp::KBYTE);
    netzp::MemController bus("Bus");

    netzp::InOutController *io = new netzp::InOutController("iocon");
    netzp::MemIO *memio = new netzp::MemIO("memio");

    netzp::AccumulationCore core("AccCore");
    netzp::ComputationData compdata;
    compdata.data = netz.neurons.at(0);
    compdata.inputs = { 1, 2, 3 };
    compdata.output = 0;

    sc_signal<fp_t> product;
    sc_signal<netzp::ComputationData> data_signal;

    core.clk(clk);
    core.rst(rst);
    core.data(data_signal);
    core.result(product);

    mem.clk(clk);
    mem.rst(rst);

    mem.ack_in(ack_con_to_mem);
    mem.ack_out(ack_mem_to_con);
    mem.addr(addr);
    mem.data_rd(data_rd);
    mem.data_wr(data_wr);
    mem.r_en(r_en);
    mem.w_en(w_en);

    bus.clk(clk);
    bus.rst(rst);

    bus.ack_in(ack_mem_to_con);
    bus.ack_out(ack_con_to_mem);
    bus.addr(addr);
    bus.data_rd(data_rd);
    bus.data_wr(data_wr);
    bus.r_en(r_en);
    bus.w_en(w_en);

    return 0;
}