# variables that can be overwritten from outside
DIR_ROOTFS	?= ~/testbed/install/rootfs
RTE_TARGET	?= x86_64-native-linuxapp-gcc
RTE_SDK		?= /home/gara/testbed/install/dpdk-19.05/share/dpdk

# variables needed to include header files
THIS_FILE		:= $(lastword $(MAKEFILE_LIST))
THIS_FILE_ABS	:= $(abspath $(THIS_FILE))
THIS_DIR		:= $(dir $(THIS_FILE_ABS))

#.PHONY: print_vars
#print_vars:
#	@echo $(THIS_DIR)

# dpdk variables
include $(RTE_SDK)/mk/rte.vars.mk

# target name
APP		= testapp

# source files
SRCS-y		+= main.c common.c stats.c timestamp.c dpdk.c
SRCS-y		+= server.c client.c clientst.c send.c recv.c
SRCS-y		+= dpdk-server.c dpdk-client.c dpdk-clientst.c dpdk-send.c dpdk-recv.c

# do I need override on CFLAGS? also, change -O2 to -ggdb -O0 when debug
CPPFLAGS	+= -iquote $(THIS_DIR)/inc
CFLAGS		+= -Wall -Wextra -pedantic -Werror -march=native -O2 # -O0 -ggdb
#LDFLAGS		+= -pthread

# dpdk external app makefile
include $(RTE_SDK)/mk/rte.extapp.mk

# rewriting installation rule to copy target in container
install: build _postinstall __postinstall
__postinstall: build _postinstall
	cp -T $(THIS_DIR)/build/$(APP) $(DIR_ROOTFS)/$(APP)

# used to list all targets in current makefile, since many are inherited
.PHONY: list
list:
	@$(MAKE) -pRrq -f $(THIS_FILE) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
