#ifndef _NETZP_IO_H_
#define _NETZP_IO_H_

#include "netzp_config.hpp"
#include "netzp_mem.hpp"
#include "netzp_utils.hpp"
#include <deque>
#include <ostream>

namespace netzp {

std::vector<uchar> RepliesToByes(const std::vector<MemReply>& replies);

std::vector<MemRequest> ReadMemorySpanRequests(mem_addr_t base_addr, size_t size,
                                               mem_master_id_t master_id);

struct NeuronData {
    using count_type = uchar;

    count_type layer         = 0;
    count_type neuron        = 0;
    count_type weights_count = 0;
    std::vector<fp_t> weights;

    NeuronData() = default;
    NeuronData(const NeuronData& other) = default;

    bool operator == (const NeuronData& other) const;

    const std::vector<uchar> Serialize() const;

    size_t SizeInBytes() const;

    static NeuronData Deserialize(const uchar *bytes);
};

std::ostream& operator<<(std::ostream& out, const NeuronData& neuron_data);

struct NetzwerkData {
    uchar neurons_count = 0;
    std::vector<NeuronData> neurons;

    NetzwerkData() = default;
    NetzwerkData(const NetzwerkData& other) = default;

    bool operator == (const NetzwerkData& other) const;

    const std::vector<uchar> Serialize() const;

    static NetzwerkData Deserialize(const uchar *bytes);
};

std::ostream& operator<<(std::ostream& out, const NetzwerkData& netz_data);

class InOutController : public sc_core::sc_module {
private:
    MemRequest              *request_next_;
    bool                     access_request_next_;
    bool                     ready_to_send_;

    bool                     input_data_changed_;
    bool                     netz_data_changed_;
    bool                     new_reply_;

    DataVector<MemRequest>   input_data_requests_;
    DataVector<MemRequest>   netz_data_requests_;

public:
    static constexpr config_int_t INPUT_COUNT      = CONFIG_INPUT_PICTURE_HEIGHT
                                                   * CONFIG_INPUT_PICTURE_WIDTH;
    static constexpr config_int_t INPUTS_OFFSET    = CONFIG_INPUT_DATA_OFFSET;
    static constexpr config_int_t NETZ_DATA_OFFSET = INPUTS_OFFSET + INPUT_COUNT;
    static constexpr config_int_t MASTER_ID        = 1;

    // System side
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst;

    // User side
    sc_core::sc_in<bool>            data_inputs[INPUT_COUNT];
    sc_signal_port_in<NetzwerkData> netz_data;

    sc_signal_port_out<DataVector<MemRequest>> requests;
    sc_signal_port_in<DataVector<MemReply>>    replies;

private:
    std::vector<MemRequest> BytesToRequests(const std::vector<uchar>& bytes, offset_t offset) const;

public:
    explicit InOutController(sc_core::sc_module_name const&);

    void SendInputDataAtClk();

    void AtReply();
    void AtDataInputChange();
    void AtNetzDataChange();
    void MainProcess();
};

} // namespace netzp

#endif