#include <systemc>
#include <iostream>
#include "netzp_mem.hpp"
#include "sysc/communication/sc_signal_ifs.h"
#include "sysc/datatypes/int/sc_uint.h"

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
    sc_core::sc_in<bool> clk;

    sc_core::sc_port<netzp::MemBusIf> port;

    netzp::MemData::addr_t data_addr = 0;
    netzp::MemData::data_t data_rd   = 0;
    netzp::MemData::data_t data_wr   = 0;
    bool                      r_en   = 0;
    bool                      w_en   = 0;

    void AtClk() {
        std::cout << name() << ": @clk" << std::endl;
        netzp::MemData& s = port->GetDataRW();
        bool is_free = (!s.w_en && !s.r_en);
        if (is_free) {
            if (r_en) {
                s.data_addr = data_addr;
                s.r_en      = r_en;
                data_rd     = s.data_rd;
            }
            if (w_en) {
                std::cout << "Line is free and we are writing" << std::endl;
                s.data_addr = data_addr;
                s.w_en      = w_en;
                s.data_wr   = data_wr;
            }
        } else if (s.r_en) {
            if (r_en) {
                s.data_addr = data_addr;
                s.r_en      = r_en;
                data_rd     = s.data_rd;
            }
        } else if (s.w_en) {
            if (w_en) {
                std::cout << "Line is busy and we are writing" << std::endl;
                s.data_addr = data_addr;
                s.w_en      = w_en;
                s.data_wr   = data_wr;
            }
        }
    }

    void Reset() {
        data_addr = 0;
        data_rd   = 0;
        data_wr   = 0;
        netzp::MemData& s = port->GetDataRW();

        if (r_en) {
            r_en   = 0;
            s.r_en = 0;
        } else if (w_en) {
            w_en   = 0;
            s.w_en = 0;
        }
    }

    Tester(sc_core::sc_module_name const &name) {
        SC_METHOD(AtClk);
        sensitive << clk.pos();
    }
};

int sc_main(int argc, char **argv) {
    using namespace sc_core;
    using namespace sc_dt;

    netzp::MemChannel channel("MemChannel");
    netzp::MemData&   channel_s = channel.GetDataRW();
    channel_s.data_addr = 0;
    channel_s.data_wr = 0;
    channel_s.data_rd = 0;
    channel_s.w_en = 0;
    channel_s.r_en = 0;

    sc_signal<bool>   rst(0);
    sc_signal<bool>   clk(0);

    netzp::Mem mem("Memory", 64);

    mem.clk(clk);
    mem.rst(rst);

    mem.port(channel);

    Tester test1("Tester1");

    test1.clk(clk);
    test1.port(channel);

    Tester test2("Tester2");

    test2.clk(clk);
    test2.port(channel);

    std::cout << "@Begin" << std::endl;

    clk = 0;
    rst = 0;
    RunCycles(clk, 5, SC_NS);

    std::cout << "@AssertReset" << std::endl;

    rst = 1;

    RunCycles(clk, 5, SC_NS);

    std::cout << "@DeAssertReset" << std::endl;

    rst = 0;

    RunCycles(clk, 5, SC_NS);

    std::cout << "@TryWrite1" << std::endl;

    test1.w_en = 1;
    test1.data_wr = 0x08;
    test1.data_addr = 0x00;
    RunCycles(clk, 5, SC_NS);
    test1.Reset();
    RunCycles(clk, 5, SC_NS);

    std::cout << "@TryWrite2" << std::endl;

    test2.w_en = 1;
    test2.data_wr = 0x09;
    test2.data_addr = 0x01;
    RunCycles(clk, 5, SC_NS);
    test2.Reset();
    RunCycles(clk, 5, SC_NS);

    std::cout << "@TryWrite3" << std::endl;

    test1.w_en = 1;
    test1.data_wr = 0x10;
    test1.data_addr = 0x02;
    RunCycles(clk, 5, SC_NS);
    test1.Reset();
    RunCycles(clk, 5, SC_NS);

    std::cout << "@TryWrite4" << std::endl;

    test2.w_en = 1;
    test2.data_wr = 0x11;
    test2.data_addr = 0x03;
    RunCycles(clk, 5, SC_NS);
    test2.Reset();
    RunCycles(clk, 5, SC_NS);

    std::cout << "@TryRead" << std::endl;

    test1.w_en = 0;
    test1.r_en = 1;
    test1.data_addr = 0x01;
    RunCycles(clk, 5, SC_NS);
    std::cout << "Read: " << test1.data_rd << std::endl;
    std::cout << "Mem dump:" << std::endl;
    mem.Dump(std::cout);

    channel_s.r_en = 0;
    rst = 1;
    RunCycles(clk, 5, SC_NS);

    std::cout << "Mem dump:" << std::endl;
    mem.Dump(std::cout);

    return 0;
}