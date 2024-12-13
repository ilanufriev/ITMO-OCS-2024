#include <cstddef>
#include <iomanip>
#include <memory>
#include <systemc>
#include <iostream>
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"

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

class Tester : public sc_core::sc_module {
public:
    netzp::MemRequest request_next;
    netzp::MemReply   reply;

    bool access_request_next = false;

    sc_core::sc_in<bool> clk;

    sc_core::sc_port<sc_core::sc_signal_out_if<netzp::MemRequest>, 0> request_out;
    sc_core::sc_out<bool>                                             access_request;

    sc_core::sc_port<sc_core::sc_signal_in_if<netzp::MemReply>, 0>    reply_in;
    sc_core::sc_in<bool>                                              access_granted;

    void AtClk() {
        if (clk->read()) {
            access_request.write(access_request_next);

            if (access_request->read() == true && access_granted->read() == true)
                request_out->write(request_next);
        }
    }

    void Reset() {
        DEBUG_BEGIN_MODULE__;
        access_request_next = 0;
    }

    void AtReply() {
        DEBUG_BEGIN_MODULE__;
        reply = reply_in->read();
    }

    explicit Tester(sc_core::sc_module_name const &name) {
        SC_METHOD(AtClk);
        sensitive << clk.pos();

        SC_METHOD(AtReply);
        sensitive << reply_in;
    }
};

std::vector<netzp::MemRequest> BytesToRequests(const std::vector<uchar>& bytes, offset_t offset) {
    std::vector<netzp::MemRequest> result;

    for (const auto byte : bytes) {
        result.emplace_back();
        result.back().addr = offset;
        result.back().data_wr = byte;
        result.back().master_id = 1;
        result.back().op_type = netzp::MemOperationType::WRITE;
        offset++;
    }

    result.shrink_to_fit();
    return result;
}

std::vector<netzp::MemRequest> ReadRequests(offset_t base_addr, size_t size) {
    std::vector<netzp::MemRequest> result;

    for (offset_t addr = base_addr; addr < (base_addr + size); addr++) {
        result.emplace_back();
        result.back().addr = addr;
        result.back().data_wr = 0;
        result.back().master_id = 1;
        result.back().op_type = netzp::MemOperationType::READ;
    }

    result.shrink_to_fit();
    return result;
}

int sc_main(int argc, char **argv) {
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
            33.14, 32.34, 10.23
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

    std::vector<std::unique_ptr<Tester>> masters;
    masters.emplace_back(new Tester("Master1"));

    netzp::InOutController *io = new netzp::InOutController("iocon");
    netzp::MemIO *memio = new netzp::MemIO("memio");

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

    masters[0]->clk(clk);
    masters[0]->reply_in(reply[0]);
    masters[0]->request_out(request[0]);
    masters[0]->access_granted(access_granted[0]);
    masters[0]->access_request(access_request[0]);

    io->clk(clk);
    io->rst(rst);
    io->replies(replies_signal);
    io->requests(requests_signal);
    io->netz_data(netz_data);

    memio->clk(clk);
    memio->rst(rst);
    memio->access_granted(access_granted[1]);
    memio->access_request(access_request[1]);
    memio->reply(reply[1]);
    memio->request(request[1]);
    memio->requests_from_host(requests_signal);
    memio->replies_to_host(replies_signal);


    for (config_int_t i = 0; i < CONFIG_INPUT_PICTURE_WIDTH * CONFIG_INPUT_PICTURE_HEIGHT; i++) {
        io->data_inputs[i](data_inputs[i]);
    }

    for (int i = 0; i < port_count; i++) {
        bus.access_granted[i](access_granted[i]);
        bus.access_request[i](access_request[i]);
        bus.requests_in[i](request[i]);
        bus.replies_out[i](reply[i]);
    }

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "begin" << std::endl;

    rst = 1;
    RunCycles(clk, 10, sc_core::SC_NS);

    rst = 0;
    RunCycles(clk, 10, sc_core::SC_NS);

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "try write 1" << std::endl;

    netz_data = netz;
    // requests.data = BytesToRequests({ 0x21, 0x22, 0x23, 0x24, 0x25 }, 0x02);
    // requests_signal.write(requests);
    RunCycles(clk, 2000, sc_core::SC_NS);

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "Mem dump:" << std::endl;
    mem.Dump(std::cout);

    requests.data = ReadRequests(0x02, 5);
    requests_signal.write(requests);
    RunCycles(clk, 300, sc_core::SC_NS);

    DEBUG_OUT(1) << "READ:" << std::endl;
    for (const auto byte : replies_signal.read().data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << byte.data << " ";
    }
    std::cout << std::dec << std::endl;


    // delete io;
    delete memio;

    return 0;
}