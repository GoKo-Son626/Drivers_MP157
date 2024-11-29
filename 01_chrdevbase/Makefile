KERNELDIR := /home/goko/linux/Need_of_image/3-kernel_atk/linux-5.4.31
CURRENT_PATH := $(shell pwd)
obj-m := chrdevbase.o

build: kernel_modules
# -C: Switch to the target
kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean