# Uncomment the following to enable debug.
#DEBUG = y

#ifeq ($(DEBUG),y)
#	DBGFLAGS = -O -g -DML_DEBUG
#else
#	DBGFLAGS = -O2
#endif

#ccflags-y += $(DBGFLAGS)
CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	obj-m := ml_driver.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	
endif
