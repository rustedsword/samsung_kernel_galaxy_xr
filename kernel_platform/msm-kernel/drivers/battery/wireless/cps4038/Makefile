obj-$(CONFIG_WIRELESS_CHARGER_CPS4038)	+= cps4038-charger.o

cps4038-charger-$(CONFIG_WIRELESS_CHARGER_CPS4038) += cps4038_fod.o cps4038_cmfet.o cps4038_charger.o

ccflags-y := -Wformat

ifdef CONFIG_SEC_KUNIT
ccflags-y := -Wno-unused-variable
GCOV_PROFILE_cps4038_charger.o		:= $(CONFIG_SEC_KUNIT)
endif