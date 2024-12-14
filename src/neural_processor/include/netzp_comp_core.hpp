#ifndef _NETZP_COMP_CORE_H_
#define _NETZP_COMP_CORE_H_

#include "netzp_config.hpp"
#include "netzp_io.hpp"
#include "sysc/kernel/sc_module_name.h"
#include <systemc>

namespace netzp {

struct ComputationData {
    NeuronData          data;
    std::vector<fp_t> inputs;
    fp_t              output;

    ComputationData();
    ComputationData(const ComputationData& other) = default;

    bool operator==(const ComputationData& other) const;
};

std::ostream& operator<<(std::ostream& out, const ComputationData& data);

class AccumulationCore : public sc_core::sc_module {
private:
    bool has_new_data_;
    fp_t product_next_;
public:
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;

    sc_signal_port_in<ComputationData> data;
    sc_core::sc_out<fp_t> result;

private:

public:
    explicit AccumulationCore(sc_core::sc_module_name const&);

    void AtData();
    void AtClk();
};

class ActivationCore : public sc_core::sc_module {
private:
    fp_t result_next_;

public:
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;

    sc_core::sc_in<fp_t>  data;
    sc_core::sc_out<fp_t> result;

private:
    fp_t ActivationFunctionSigma(fp_t x) const;

public:
    explicit ActivationCore(sc_core::sc_module_name const&);

    void AtClk();
    void AtData();
};

class ComputCore : public sc_core::sc_module {
private:
    AccumulationCore *accumulator_;
    ActivationCore   *activator_;

    sc_core::sc_signal<fp_t> activator_out_;
    sc_core::sc_signal<fp_t> accumulator_out_;

    ComputationData output_data_next_;
    ComputationData compdata_current_;

    bool ready_next_;
public:
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;

    sc_signal_port_in<ComputationData>  input_data;
    sc_signal_port_out<ComputationData> output_data;
    sc_core::sc_out<bool>               ready;

private:

public:
    explicit ComputCore(sc_core::sc_module_name const &);

    void AtClk();
    void AtInputData();
    void AtOutputReady();

    ~ComputCore();
};

} // namespace netzp

#endif // _NETZP_COMP_CORE_H_