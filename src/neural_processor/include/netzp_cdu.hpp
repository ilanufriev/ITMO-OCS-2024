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
public:
    static constexpr config_int_t CORE_COUNT  = CONFIG_COMP_CORE_COUNT;
    static constexpr config_int_t MAX_NEURONS = CONFIG_CDU_MAX_NEURONS_COUNT;
    static constexpr config_int_t MAX_OUTPUTS = CONFIG_NETZ_MAX_OUTPUTS;
    static constexpr config_int_t MASTER_ID   = 2;

private:
    ComputCore *compcore[CORE_COUNT];

    sc_core::sc_signal<ComputationData> core_inputs_ [CORE_COUNT];
    sc_core::sc_signal<ComputationData> core_outputs_[CORE_COUNT];
    sc_core::sc_signal<bool>            core_ready_  [CORE_COUNT];

    bool is_first_layer_ = true;
    bool core_finished_  = false;
    bool has_mem_reply_  = false;

    std::array<fp_t, MAX_NEURONS>       inputs_;
    std::array<fp_t, MAX_NEURONS>       outputs_;
    std::array<bool, MAX_NEURONS>       outputs_ready_;

    std::array<NeuronData, CORE_COUNT>  neurons_;
    std::array<bool, CORE_COUNT> core_cold_;

    size_t inputs_size_;
    size_t outputs_size_;
    size_t neurons_size_;

    bool finished_ = false;

public:
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;
    sc_core::sc_in<bool> start;

    // MemIO connection ports
    sc_signal_port_in<DataVector<MemReply>>    mem_replies;
    sc_signal_port_out<DataVector<MemRequest>> mem_requests;

private:
    bool CheckAllCoreOutputs();
    void ResetOutputs();
    void ResetNeurons();
    void AddOutput(fp_t output, size_t index);
    void AddNeuron(const NeuronData& data);
    NeuronData PopNeuron();

    void AssignNeurons();

public:
    explicit CentralDispatchUnit(sc_core::sc_module_name const&);

    void MainProcess();
    void AtCoreReady();
    void AtMemReply();
    void AtStart();
};

} // namespace netzp


#endif // _NETZP_CDU_H_