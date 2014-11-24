obj-m += ili9341.o
KDIR ?= /home/valy/work/rpi/linux
all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
