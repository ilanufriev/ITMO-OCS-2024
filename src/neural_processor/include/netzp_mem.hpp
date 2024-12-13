#ifndef _NETZP_MEM_H_
#define _NETZP_MEM_H_

#include "netzp_config.hpp"
#include "netzp_utils.hpp"
#include <deque>
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
    mem_data_t         data;
    mem_addr_t         addr;

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

    bool       access_granted_next_[MAX_CONNECTIONS];
    MemRequest request_;
    MemReply   reply_next_;

    sc_dt::sc_uint<8>                     current_access_next_;
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
    sc_core::sc_in<bool>           access_request[MAX_CONNECTIONS];
    sc_core::sc_out<bool>          access_granted[MAX_CONNECTIONS];
    sc_signal_port_in<MemRequest>  requests_in[MAX_CONNECTIONS];
    sc_signal_port_out<MemReply>   replies_out[MAX_CONNECTIONS];

public:
    explicit MemController(sc_core::sc_module_name const&);

    void AtClk();
    void AtRequest();
    void AtAccessRequest();
    void AtAck();
    void AtCounter();
};

class MemIO : public sc_core::sc_module {
private:
    std::deque<MemRequest> requests_fifo_;
    std::deque<MemReply> replies_fifo_;

    size_t requests_size_;

    sc_core::sc_signal<bool> reply_ready_;
    bool                     reply_ready_next_;

public:
    // System side
    sc_core::sc_in<bool>   clk;
    sc_core::sc_in<bool>   rst;

    // Memory side
    sc_signal_port_out<MemRequest> request;
    sc_signal_port_in<MemReply>    reply;
    sc_core::sc_out<bool>          access_request;
    sc_core::sc_in<bool>           access_granted;

    // User side
    sc_signal_port_in<DataVector<MemRequest>> requests_from_host;
    sc_signal_port_out<DataVector<MemReply>>  replies_to_host;

    bool new_request_ = false;
    bool new_reply_   = false;

private:

public:
    void AtRequestsFromHost();
    void AtReply();
    void AtClk();
    void MainProcess();

    MemIO(sc_core::sc_module_name const&);
};

} // namespace netzp

#endif // _NETZP_MEM_H_
