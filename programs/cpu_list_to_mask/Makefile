# variables that can be overwritten from outside
DIR_ROOTFS	?= ~/testbed/install/rootfs

APP = cpu_list_to_mask

.PHONY = all compile install clean

all: compile install

compile: $(APP)

install: compile
	cp -T cpu_list_to_mask $(DIR_ROOTFS)/$(APP)

$(APP): main.c
	$(CC) -o $@ $^

clean:
	rm -f $(APP) *.o
