#ifndef _NETZP_CONFIG_H_
#define _NETZP_CONFIG_H_

// USEFUL MACROS

#include <systemc>

#define PRINTVAL(__val) \
    #__val << " = " << __val

#define DEBUG 2

enum DebugLevel {
    DEBUG_LEVEL_BEGIN = 10,
    DEBUG_LEVEL_MSG = 2,
};

#define DEBUG_OUT(__level) \
    if (DEBUG >= (__level)) std::cerr << "[DEBUG] " << __func__ << ": "

#define DEBUG_OUT_MODULE(__level) \
    DEBUG_OUT(__level) << name() << "." << __func__ << ": "

#define DEBUG_BEGIN__ \
    DEBUG_OUT(DEBUG_LEVEL_BEGIN) << ": started" << std::endl

#define DEBUG_BEGIN_MODULE__ \
    DEBUG_OUT_MODULE(DEBUG_LEVEL_BEGIN) << "started" << std::endl

using config_int_t = unsigned int;

// CONFIGURATION CONSTANTS
constexpr config_int_t CONFIG_MEMORY_CONTROLLER_MAX_CONNECTIONS = 2;
constexpr config_int_t CONFIG_IO_RSVD_MEMORY_SIZE = 1026;
constexpr config_int_t CONFIG_IO_RSVD_MEMORY_BASE_ADDR = 0x00;
constexpr config_int_t CONFIG_INPUT_PICTURE_WIDTH = 7;
constexpr config_int_t CONFIG_INPUT_PICTURE_HEIGHT = 7;
constexpr config_int_t CONFIG_INPUT_DATA_OFFSET = CONFIG_IO_RSVD_MEMORY_BASE_ADDR + CONFIG_IO_RSVD_MEMORY_SIZE;
constexpr config_int_t CONFIG_COMP_CORE_COUNT = 1;
constexpr config_int_t CONFIG_CDU_MAX_NEURONS_COUNT = 255;
constexpr config_int_t CONFIG_NETZ_MAX_OUTPUTS = 3;

// USINGS
template <typename T>
using sc_signal_port_in = sc_core::sc_port<sc_core::sc_signal_in_if<T>>;

template <typename T>
using sc_signal_port_out = sc_core::sc_port<sc_core::sc_signal_out_if<T>>;

using sc_uint8    = sc_dt::sc_uint<8>;
using sc_uint16   = sc_dt::sc_uint<16>;
using sc_uint32   = sc_dt::sc_uint<32>;
using sc_uint64   = sc_dt::sc_uint<64>;
using offset_t    = unsigned int;
using uchar       = unsigned char;
using fp_t        = float;

#endif