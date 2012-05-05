DRIVER = bq76pl536

ifneq ($(KERNELRELEASE),)
    obj-m := $(DRIVER).o
else
    PWD := $(shell pwd)

default:
ifeq ($(strip $(KERNELDIR)),)
	$(error "KERNELDIR is undefined!")
else
	$(MAKE) -C $(KERNELDIR) CROSS_COMPILE=${CC} M=$(PWD) modules
endif


clean:
	rm -rf *~ *.ko *.o *.mod.c modules.order Module.symvers .$(DRIVER)* .tmp_versions

endif

