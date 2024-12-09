#ifndef _NETZP_MEM_H_
#define _NETZP_MEM_H_

#include "sysc/communication/sc_interface.h"
#include "sysc/communication/sc_mutex.h"
#include "sysc/communication/sc_prim_channel.h"
#include "sysc/communication/sc_signal_ifs.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include <systemc>
#include <vector>

namespace netzp {

constexpr unsigned int BYTE  = 1;
constexpr unsigned int KBYTE = BYTE  * 1024;
constexpr unsigned int MBYTE = KBYTE * 1024;
constexpr unsigned int GBYTE = MBYTE * 1024;

struct MemData {
    using data_t = sc_dt::sc_uint<8>;
    using addr_t = sc_dt::sc_uint<16>;

    addr_t            data_addr;
    data_t            data_wr;
    bool              w_en;
    bool              r_en;
    data_t            data_rd;

    MemData();

    MemData(const MemData& other);

    bool operator == (const MemData& other) const;
};

std::ostream& operator << (std::ostream& out, const MemData& signals);

class MemBusIf : public virtual sc_core::sc_interface {
public:
    using data_t = MemData::data_t;
    using addr_t = MemData::addr_t;

    virtual MemData& GetDataRW() = 0;
    virtual const MemData& GetDataRO() const = 0;
};

class MemChannel final : public MemBusIf, public sc_core::sc_module {
private:
    MemData data_;
public:
    MemChannel(sc_core::sc_module_name const &name);

    virtual MemData& GetDataRW() override;
    virtual const MemData& GetDataRO() const override;
};

class Mem : public sc_core::sc_module {
private:
    static constexpr unsigned int MEMSIZE = 64 * KBYTE;
    using data_t = MemData::data_t;
    using addr_t = MemData::addr_t;

    data_t data_rd_next_;
    std::vector<data_t>  mem_;

    void AtClk();
    void MemAccess();

public:
    // I/O
    sc_core::sc_in<bool>          clk;
    sc_core::sc_in<bool>          rst;

    sc_core::sc_port<MemBusIf, 0> port;

    void Dump(std::ostream& out) const;

    explicit Mem(sc_core::sc_module_name const&, int memsize = MEMSIZE);
};

} // namespace netzp

#endif // _NETZP_MEM_H_
