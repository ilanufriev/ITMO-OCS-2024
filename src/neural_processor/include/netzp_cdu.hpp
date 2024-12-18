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

    using size_type = uchar;

private:
    ComputCore *compcore[CORE_COUNT];

    sc_core::sc_signal<ComputationData> core_inputs_ [CORE_COUNT];
    sc_core::sc_signal<ComputationData> core_outputs_[CORE_COUNT];
    sc_core::sc_signal<bool>            core_ready_  [CORE_COUNT];

    bool has_mem_reply_  = false;

    std::array<fp_t, MAX_NEURONS>       inputs_;
    std::array<fp_t, MAX_NEURONS>       outputs_;
    std::array<bool, MAX_NEURONS>       outputs_ready_;

    std::array<NeuronData, CORE_COUNT>  neurons_;
    std::array<bool, CORE_COUNT> core_cold_;

    size_type inputs_size_;
    size_type outputs_size_;
    size_type neurons_size_;

public:
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;
    sc_core::sc_in<bool> start;
    sc_core::sc_out<bool> finished;

    // MemIO connection ports
    sc_signal_port_in<DataVector<MemReply>>    mem_replies;
    sc_signal_port_out<DataVector<MemRequest>> mem_requests;

private:
    bool CheckAllCoreOutputs();
    void ResetOutputs();
    void ResetNeurons();
    void ResetCores();
    void AddOutput(fp_t output, size_type index);
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