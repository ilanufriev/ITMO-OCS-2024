#ifndef _NETZP_CDU_H_
#define _NETZP_CDU_H_

#include "netzp_comp_core.hpp"
#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module_name.h"

namespace netzp {

class CentralDispatchUnit : public sc_core::sc_module {
private:
    static constexpr config_int_t CORE_COUNT  = CONFIG_COMP_CORE_COUNT;
    static constexpr config_int_t MAX_NEURONS = CONFIG_CDU_MAX_NEURONS_COUNT;
    static constexpr config_int_t MASTER_ID   = 2;


    ComputCore *compcore[CORE_COUNT];

    sc_core::sc_signal<ComputationData> core_inputs_ [CORE_COUNT];
    sc_core::sc_signal<ComputationData> core_outputs_[CORE_COUNT];
    sc_core::sc_signal<bool>            core_ready_  [CORE_COUNT];

    bool is_first_layer_ = true;
    bool core_finished_  = false;
    bool has_mem_reply_  = false;

    std::array<fp_t, MAX_NEURONS> inputs_;
    std::array<fp_t, MAX_NEURONS> outputs_;
    std::array<NeuronData, MAX_NEURONS> neurons_;
public:
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;

    // MemIO connection ports
    sc_signal_port_in<DataVector<MemReply>>    mem_replies;
    sc_signal_port_out<DataVector<MemRequest>> mem_requests;
private:

public:
    explicit CentralDispatchUnit(sc_core::sc_module_name const&);

    void MainProcess();
    void AtCoreReady();
    void AtMemReply();
};

} // namespace netzp


#endif // _NETZP_CDU_H_