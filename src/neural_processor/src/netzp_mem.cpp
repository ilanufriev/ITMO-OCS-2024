#include "netzp_mem.hpp"
#include "sysc/kernel/sc_module.h"
#include <iostream>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace netzp {

    constexpr char INDEX_OOB[] = "Memory index out of bounds";

    MemData::MemData()
        : data_addr()
        , data_wr()
        , data_rd()
        , w_en()
        , r_en()
    { /* nothing */ }

    MemData::MemData(const MemData& other)
        : data_addr(other.data_addr)
        , data_wr(other.data_wr)
        , data_rd(other.data_rd)
        , w_en(other.w_en)
        , r_en(other.r_en)
    { /* nothing */ }

    bool MemData::operator == (const MemData& other) const {
        return (data_addr == other.data_addr) &&
               (data_rd   == other.data_rd)   &&
               (data_wr   == other.data_wr)   &&
               (w_en      == other.w_en)      &&
               (r_en      == other.r_en);
    }

    std::ostream& operator << (std::ostream& out, const MemData& signals) {
        out << "data_addr = " << signals.data_addr << std::endl
            << "data_rd = "   << signals.data_rd   << std::endl
            << "data_wr = "   << signals.data_wr   << std::endl
            << "w_en = "      << signals.w_en      << std::endl
            << "r_en = "      << signals.r_en      << std::endl;
        return out;
    }

    MemChannel::MemChannel(sc_core::sc_module_name const &name) {}

    MemData& MemChannel::GetDataRW() {
        return data_;
    }

    const MemData& MemChannel::GetDataRO() const {
        return data_;
    }

    void Mem::AtClk() {
        MemData& s = port->GetDataRW();

        std::cout << "@clk: " << std::endl
                    << s << std::endl;

        if (rst->read()) {
            for (int i = 0; i < mem_.size(); i++) {
                mem_[i] = 0;
            }

            s.data_rd = 0;
        } else if (clk->read()) {
            if (s.data_addr > mem_.size()) {
                throw std::invalid_argument(INDEX_OOB);
            }

            if (s.r_en) {
                s.data_rd = mem_.at(s.data_addr);
            } else if (s.w_en) {
                mem_[s.data_addr] = s.data_wr;
            }
        }
    }

    void Mem::Dump(std::ostream& out) const {
        int row_w = 32;
        int hex_group_w = 2;

        for (int i = 0; i < mem_.size(); i += row_w) {
            for (int j = 0; (j < row_w) && (i + j < mem_.size()); j += 1) {
                if (j % hex_group_w == 0 && j != 0) {
                    out << " ";
                }
                out << std::hex << std::noshowbase << std::setfill('0') << std::setw(2)
                    << +static_cast<unsigned char>(mem_.at(i + j).to_uint());
            }
            out << std::endl;
        }
    }

    Mem::Mem(sc_core::sc_module_name const &modname, int memsize) : mem_(memsize) {
        SC_METHOD(AtClk);
        sensitive << clk.pos();
    }
}