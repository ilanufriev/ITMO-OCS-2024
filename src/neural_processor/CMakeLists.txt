# For infomation about linking against systemc with CMAKE see INSTALL.md: https://github.com/accellera-official/systemc

cmake_minimum_required(VERSION 3.10)

project(neural_processor VERSION 0.1.0)

find_package(SystemCLanguage CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD ${SystemC_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ${SystemC_CXX_STANDARD_REQUIRED})

add_executable(netzp 
        src/netzp_bus.cpp
        src/netzp_cdu.cpp
        src/netzp_comp_core.cpp
        src/netzp_io.cpp
        src/netzp_mem.cpp
        src/netzp_tb.cpp
        src/netzp_utils.cpp
        )

target_link_libraries(netzp PUBLIC SystemC::systemc)
target_include_directories(netzp PUBLIC include)