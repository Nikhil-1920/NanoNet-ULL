obj-m += nanonet.o
nanonet-objs := src/nanonet.o src/micro_stack.o src/packet_processor.o \
                src/response_sender.o src/control_interface.o src/optimizations.o \
                src/security.o src/debug.o

KERNEL_DIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

EXTRA_CFLAGS += -O3 -march=native -mtune=native -DCONFIG_PREEMPT_NONE -fomit-frame-pointer

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	gcc -o tools/nanonet_control tools/nanonet_control.c
	gcc -o tools/packet_generator tools/packet_generator.c

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f tools/nanonet_control tools/packet_generator

install:
	sudo insmod nanonet.ko
	sudo chmod 666 /dev/nanonet

uninstall:
	sudo rmmod nanonet

test:
	./scripts/test.sh