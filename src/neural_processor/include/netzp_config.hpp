#ifndef _NETZP_CONFIG_H_
#define _NETZP_CONFIG_H_

// USEFUL MACROS

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


// CONFIGURATION CONSTANTS
constexpr long unsigned int CONFIG_MEMORY_CONTROLLER_MAX_CONNECTIONS = 2;
constexpr long unsigned int CONFIG_INPUT_PICTURE_WIDTH = 7;
constexpr long unsigned int CONFIG_INPUT_PICTURE_HEIGHT = 7;

#endif