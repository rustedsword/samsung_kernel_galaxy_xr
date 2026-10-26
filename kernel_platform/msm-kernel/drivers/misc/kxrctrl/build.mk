ARCH ?= arm64

KERNEL_PATH = $(OUT)/obj/KERNEL_OBJ
CROSS_COMPILE = aarch64-linux-gnu-
CFLAGS = -Wall -Werror -DCONFIG_KXR_NO_VFS_IMPORT

SIGN_DIR  = $(KERNEL_PATH)/scripts/sign-file
MODSECKEY = $(KERNEL_PATH)/certs/signing_key.pem
MODPUBKEY = $(KERNEL_PATH)/certs/signing_key.x509

all:
	+make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_KINECTICS_XR_NORDIC=m -C $(KERNEL_PATH) M=$(PWD) EXTRA_CFLAGS="$(CFLAGS)"
	+$(SIGN_DIR) sha512 $(MODSECKEY) $(MODPUBKEY) $(PWD)/aphost.ko

clean:
	rm -rfv *.ko *.o *.cmd *.mod.c *.order *.symvers
	rm -rfv .*.cmd .tmp_versions
