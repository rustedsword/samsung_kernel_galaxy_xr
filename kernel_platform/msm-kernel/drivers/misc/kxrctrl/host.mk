KERNEL_NAME ?= $(shell uname -r)
KERNEL_PATH = /lib/modules/$(KERNEL_NAME)/build
CROSS_COMPILE =
ARCH = x86_64
CFLAGS = -Wall -Werror -DCONFIG_KXR_NO_VFS_IMPORT -DCONFIG_KXR_SIMULATION_TEST

all:
	+make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_KINECTICS_XR_NORDIC=m -C $(KERNEL_PATH) M=$(PWD) EXTRA_CFLAGS="$(CFLAGS)"

clean:
	rm -rfv *.ko *.o *.cmd *.mod.c *.order *.symvers
	rm -rfv .*.cmd .tmp_versions
