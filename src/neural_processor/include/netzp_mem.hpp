#ifndef _NETZP_MEM_H_
#define _NETZP_MEM_H_

#include "netzp_config.hpp"
#include <systemc>
#include <vector>

namespace netzp {

constexpr unsigned int BYTE  = 1;
constexpr unsigned int KBYTE = BYTE  * 1024;
constexpr unsigned int MBYTE = KBYTE * 1024;
constexpr unsigned int GBYTE = MBYTE * 1024;

using mem_data_t = sc_dt::sc_uint<8>;
using mem_addr_t = sc_dt::sc_uint<16>;
using mem_master_id_t = sc_dt::sc_uint<8>;

struct MemData {
    mem_addr_t            data_addr;
    mem_data_t            data_wr;
    bool                  w_en_s;
    bool                  r_en_s;
    bool                  w_en_m;
    bool                  r_en_m;
    mem_data_t            data_rd;

    MemData();

    MemData(const MemData& other);

    bool operator == (const MemData& other) const;
};

std::ostream& operator << (std::ostream& out, const MemData& signals);

enum class MemOperationType {
    WRITE,
    READ,
    NONE
};

enum class MemOperationStatus {
    OK,
    ERROR,
    NONE
};

struct MemRequest {
    mem_master_id_t  master_id;
    MemOperationType op_type;
    mem_addr_t       addr;
    mem_data_t       data_wr;

    MemRequest();

    MemRequest(const MemRequest& other);

    bool operator == (const MemRequest& other) const;
};

std::ostream& operator << (std::ostream& out, const MemRequest& req);

struct MemReply {
    mem_master_id_t    master_id;
    MemOperationType   op_type;
    MemOperationStatus status;
    mem_data_t         data_rd;

    MemReply();

    MemReply(const MemReply& other);

    bool operator == (const MemReply& other) const;
};

std::ostream& operator << (std::ostream& out, const MemReply& reply);

class Mem : public sc_core::sc_module {
private:
    static constexpr unsigned int MEMSIZE = 64 * KBYTE;
    mem_data_t               data_rd_next_;
    bool                     ack_next_;
    std::vector<mem_data_t>  mem_;

    void AtClk();
    void AtAck();
    void MemAccess();

public:
    // I/O
    sc_core::sc_in<bool>          clk;
    sc_core::sc_in<bool>          rst;

    sc_core::sc_in<mem_data_t>    data_wr;
    sc_core::sc_in<mem_addr_t>    addr;
    sc_core::sc_in<bool>          w_en;
    sc_core::sc_in<bool>          r_en;
    sc_core::sc_in<bool>          ack_in;

    sc_core::sc_out<bool>         ack_out;
    sc_core::sc_out<mem_data_t>   data_rd;

    void Dump(std::ostream& out) const;

    explicit Mem(sc_core::sc_module_name const&, int memsize = MEMSIZE);
};

class MemController : public sc_core::sc_module {
private:
    static constexpr long unsigned int MAX_CONNECTIONS = CONFIG_MEMORY_CONTROLLER_MAX_CONNECTIONS;

    mem_data_t data_wr_next_;
    mem_addr_t addr_next_;
    bool       w_en_next_;
    bool       r_en_next_;
    bool       ack_out_next_;

    bool access_granted_next_[MAX_CONNECTIONS];
    MemRequest        request_;
    MemReply          reply_next_;

    sc_dt::sc_uint<8> current_access_next_;
    sc_core::sc_signal<sc_dt::sc_uint<8>> current_access_;

public:
    sc_core::sc_in<bool>           clk;
    sc_core::sc_in<bool>           rst;

    // Memory side
    sc_core::sc_out<mem_data_t>    data_wr;
    sc_core::sc_out<mem_addr_t>    addr;
    sc_core::sc_out<bool>          w_en;
    sc_core::sc_out<bool>          r_en;
    sc_core::sc_out<bool>          ack_out;

    sc_core::sc_in<bool>           ack_in;
    sc_core::sc_in<mem_data_t>     data_rd;

    // Masters side
    sc_core::sc_in<bool>                                   access_request[MAX_CONNECTIONS];
    sc_core::sc_out<bool>                                  access_granted[MAX_CONNECTIONS];
    sc_core::sc_port<sc_core::sc_signal_in_if<MemRequest>> requests_in[MAX_CONNECTIONS];
    sc_core::sc_port<sc_core::sc_signal_out_if<MemReply>>  replies_out[MAX_CONNECTIONS];

public:
    explicit MemController(sc_core::sc_module_name const&);

    void AtClk();
    void AtRequest();
    void AtAccessRequest();
    void AtAck();
    void AtCounter();
};

} // namespace netzp

#endif // _NETZP_MEM_H_
