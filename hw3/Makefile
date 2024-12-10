obj-m := kfetch_mod_110550108.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	sudo insmod kfetch_mod_110550108.ko

unload:
	sudo rmmod kfetch_mod_110550108

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
