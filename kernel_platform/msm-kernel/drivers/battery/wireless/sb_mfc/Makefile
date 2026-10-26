obj-$(CONFIG_SB_MFC)		+= sb-mfc.o
sb-mfc-$(CONFIG_SB_MFC)		+= sb_mfc.o

ifeq ($(CONFIG_SEC_KUNIT), y)
 GCOV_PROFILE_sb_mfc.o := $(CONFIG_SEC_KUNIT)
endif

ccflags-y := -Wformat
