# variables that can be overwritten from outside
DIR_ROOTFS	?= ~/testbed/install/rootfs
RTE_TARGET	?= x86_64-native-linuxapp-gcc
RTE_SDK		?= $(HOME)/Documents/PhD/Projects/testbed/install/dpdk-20.02/share/dpdk

# variables needed to include header files
THIS_FILE		:= $(lastword $(MAKEFILE_LIST))
THIS_FILE_ABS	:= $(abspath $(THIS_FILE))
THIS_DIR		:= $(dir $(THIS_FILE_ABS))

# dpdk variables
include $(RTE_SDK)/mk/rte.vars.mk

# Target name
APP          = testapp

# Source files
SRCS-y      += main.c config.c commands.c timestamp.c loops.c stats.c nfv_socket.c nfv_socket_simple.c nfv_socket_dpdk.c dpdk.c

# Extra compilation and preprocessing flags
CFLAGS      += -iquote $(THIS_DIR)/inc
CFLAGS      += -Wall -Wextra -march=native
CFLAGS      += -O3
# NOTICE: the next one is only for DEBUG PURPOSES!
# CFLAGS      += -O0 -ggdb
# CFLAGS      += $(WERROR_FLAGS)

LDFLAGS     += -lstdc++

CXXFLAGS    += -std=c++14

include $(RTE_SDK)/mk/rte.extapp.mk

# This target can be used to print out some variables for debug purposes
.PHONY: print_vars
print_vars:
	@echo $(THIS_DIR)
	@echo $(WERROR_FLAGS)
	@echo CPPFLAGS: $(CPPFLAGS)
	@echo CFLAGS: $(CFLAGS)

%.o: %.cpp
	@echo "  CXX $@"
	@$(CXX) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

# rewriting installation rule to copy target in container
install: build _postinstall __postinstall
__postinstall: build _postinstall
	cp -T $(THIS_DIR)/build/$(APP) $(DIR_ROOTFS)/$(APP)

# used to list all targets in current makefile, since many are inherited
# .PHONY: list
# list:
# 	@$(MAKE) -pRrq -f $(THIS_FILE) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
