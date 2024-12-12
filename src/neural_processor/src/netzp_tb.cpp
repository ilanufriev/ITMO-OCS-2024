#include <iterator>
#include <memory>
#include <systemc>
#include <iostream>
#include "netzp_config.hpp"
#include "netzp_mem.hpp"

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

int sc_main(int argc, char **argv) {
    using namespace sc_core;
    using namespace sc_dt;

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

    netzp::Mem mem("Memory", 64);
    netzp::MemController bus("Bus");

    std::vector<std::unique_ptr<Tester>> masters;
    masters.emplace_back(new Tester("Master1"));
    masters.emplace_back(new Tester("Master2"));

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

    for (int i = 0; i < port_count; i++) {
        masters[i]->clk(clk);
    }

    for (int i = 0; i < port_count; i++) {
        bus.access_granted[i](access_granted[i]);
        bus.access_request[i](access_request[i]);
        bus.requests_in[i](request[i]);
        bus.replies_out[i](reply[i]);

        masters[i]->reply_in(reply[i]);
        masters[i]->request_out(request[i]);
        masters[i]->access_granted(access_granted[i]);
        masters[i]->access_request(access_request[i]);
    }

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "begin" << std::endl;

    rst = 1;
    RunCycles(clk, 10, sc_core::SC_NS);

    rst = 0;
    RunCycles(clk, 10, sc_core::SC_NS);

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "try write 1" << std::endl;

    masters[0]->request_next.master_id = 1;
    masters[0]->request_next.addr = 0x00;
    masters[0]->request_next.data_wr = 0x32;
    masters[0]->request_next.op_type = netzp::MemOperationType::WRITE;
    masters[0]->access_request_next = true;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[0]->access_request_next = false;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[1]->request_next.master_id = 2;
    masters[1]->request_next.addr = 0x01;
    masters[1]->request_next.data_wr = 0x45;
    masters[1]->request_next.op_type = netzp::MemOperationType::WRITE;
    masters[1]->access_request_next = true;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[1]->access_request_next = false;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[0]->request_next.master_id = 1;
    masters[0]->request_next.addr = 0x3F;
    masters[0]->request_next.data_wr = 0xFF;
    masters[0]->request_next.op_type = netzp::MemOperationType::WRITE;
    masters[0]->access_request_next = true;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[0]->access_request_next = false;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[1]->request_next.master_id = 2;
    masters[1]->request_next.addr = 0x3E;
    masters[1]->request_next.data_wr = 0xFF;
    masters[1]->request_next.op_type = netzp::MemOperationType::WRITE;
    masters[1]->access_request_next = true;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[1]->access_request_next = false;
    RunCycles(clk, 10, sc_core::SC_NS);

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "Read begin" << std::endl;

    masters[0]->request_next.master_id = 1;
    masters[0]->request_next.addr = 0x3F;
    masters[0]->request_next.data_wr = 0;
    masters[0]->request_next.op_type = netzp::MemOperationType::READ;
    masters[0]->access_request_next = true;
    RunCycles(clk, 10, sc_core::SC_NS);

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "Read: " << masters[0]->reply.data_rd << std::endl;

    masters[0]->access_request_next = false;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[1]->request_next.master_id = 2;
    masters[1]->request_next.addr = 0x01;
    masters[1]->request_next.data_wr = 0;
    masters[1]->request_next.op_type = netzp::MemOperationType::READ;
    masters[1]->access_request_next = true;
    RunCycles(clk, 10, sc_core::SC_NS);

    masters[1]->access_request_next = false;
    RunCycles(clk, 10, sc_core::SC_NS);

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "Read: " << masters[1]->reply.data_rd << std::endl;

    DEBUG_OUT(DEBUG_LEVEL_MSG) << "Mem dump:" << std::endl;
    mem.Dump(std::cout);

    rst = 1;
    RunCycles(clk, 5, SC_NS);

    std::cout << "Mem dump:" << std::endl;
    mem.Dump(std::cout);

    return 0;
}