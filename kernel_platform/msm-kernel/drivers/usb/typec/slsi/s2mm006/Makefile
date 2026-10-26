#
# Makefile for muic devices
#

subdir-ccflags-y := -Wformat
obj-$(CONFIG_PDIC_S2MM006)		+= pdic_s2mm006.o
pdic_s2mm006-y := s2mm006.o s2mm006_pd.o s2mm006_sysfs.o s2mm006_typec.o
pdic_s2mm006-$(CONFIG_PDIC_ALTERNATE_MODE) += s2mm006_alternate.o
pdic_s2mm006-$(CONFIG_S2MM006_FW_DOWNLOAD) += s2mm006_fw.o
ifeq ($(CONFIG_SEC_FACTORY), y)
pdic_s2mm006-$(CONFIG_MUIC_S2MM006) += s2mm006_muic.o
endif
