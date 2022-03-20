CONFIG_MODULE_SIG=n
ccflags-y := -O2

obj-m += mailslot.o # obj-m stands for object module

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load:
	insmod ./mailslot.ko

unload:
	rmmod mailslot

