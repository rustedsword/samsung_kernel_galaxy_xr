# SPDX-License-Identifier: GPL-2.0

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# ANORAK BASE DTB
QCOM_DTB := ../../vendor/qcom
SEC_ANORAK_BASE_DTB := $(QCOM_DTB)/anorak.dtb $(QCOM_DTB)/anorakp.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

SEC_XRVST2_USA_OPEN_DTBO := \
		anorak-sec-xrvst2-usa-open-overlay-r00.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r01.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r02.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r03.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r04.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r05.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r06.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r07.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r08.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r09.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r10.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r11.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r12.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r13.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r14.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r15.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r16.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r17.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r18.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r19.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r20.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r21.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r22.dtbo \
		anorak-sec-xrvst2-usa-open-overlay-r25.dtbo \
		anorak-sec-xrvst2-kor-open-overlay-r25.dtbo

$(eval $(call sec_dtbo_build, \
		$(SEC_XRVST2_USA_OPEN_DTBO),$(CONFIG_ARCH_ANORAK),$(SEC_ANORAK_BASE_DTB)))

endif