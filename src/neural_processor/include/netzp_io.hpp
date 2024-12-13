#ifndef _NETZP_IO_H_
#define _NETZP_IO_H_

#include "netzp_config.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include <deque>
#include <ostream>

namespace netzp {

struct NeuronData {
    sc_uint8 layer;
    sc_uint8 neuron;
    sc_uint8 weights_count;
    std::vector<float> weights;

    NeuronData() = default;
    NeuronData(const NeuronData& other) = default;

    bool operator == (const NeuronData& other) const;

    const std::vector<uchar> Serialize() const;
};

std::ostream& operator<<(std::ostream& out, const NeuronData& neuron_data);

struct NetzwerkData {
    sc_uint8 neurons_count;
    std::vector<NeuronData> neurons;

    NetzwerkData() = default;
    NetzwerkData(const NetzwerkData& other) = default;

    bool operator == (const NetzwerkData& other) const;

    const std::vector<uchar> Serialize() const;
};

std::ostream& operator<<(std::ostream& out, const NetzwerkData& netz_data);

class InOutController : public sc_core::sc_module {
private:
    static constexpr config_int_t INPUT_COUNT      = CONFIG_INPUT_PICTURE_HEIGHT
                                                   * CONFIG_INPUT_PICTURE_WIDTH;
    static constexpr config_int_t INPUTS_OFFSET    = CONFIG_INPUT_DATA_OFFSET;
    static constexpr config_int_t NETZ_DATA_OFFSET = INPUTS_OFFSET + INPUT_COUNT;
    static constexpr config_int_t MASTER_ID        = 2;

    MemRequest              *request_next_;
    bool                     access_request_next_;
    bool                     ready_to_send_;

    bool                     input_data_changed_;
    bool                     netz_data_changed_;
    bool                     new_reply_;

public:
    // System side
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;

    // User side
    sc_core::sc_in<bool>            data_inputs[INPUT_COUNT];
    sc_signal_port_in<NetzwerkData> netz_data;

    sc_signal_port_out<DataVector<MemRequest>> requests;
    sc_signal_port_in<DataVector<MemReply>>  replies;

private:
    std::vector<MemRequest> BytesToRequests(const std::vector<uchar>& bytes, offset_t offset) const;

public:
    explicit InOutController(sc_core::sc_module_name const&);

    void AtClk();

    void AtReply();
    void AtDataInputChange();
    void AtNetzDataChange();
    void MainProcess();
};

} // namespace netzp

#endif