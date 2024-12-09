SC_INCLUDE_DIR		:= $(wildcard $(HOME)/.local/include/systemc-3.0.1)
SC_LIB_DIR			:= $(wildcard $(HOME)/.local/lib/systemc-3.0.1)

NETZP_INCLUDE_DIR	:= $(wildcard $(PWD)/include)
NETZP_SRC_DIR		:= $(wildcard $(PWD)/src)
NETZP_SRC_FILES		:= $(wildcard $(NETZP_SRC_DIR)/*.cpp)

NETZ_TARGET			:= netzp

default:
	clang++ -I$(NETZP_INCLUDE_DIR) \
			-g \
			-I$(SC_INCLUDE_DIR) \
			-L$(SC_LIB_DIR) \
			-Wl,-rpath=$(SC_LIB_DIR) \
			-v -lsystemc -lm \
			-o $(NETZ_TARGET) $(NETZP_SRC_FILES)
