// SPDX-License-Identifier: GPL-2.0
/*
 * driver/../s2mm006_sysfs.c - S2MM006 sysfs function
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define pr_fmt(fmt) "[PDIC] " fmt

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/common/pdic_sysfs.h>
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_policy.h>
#include <linux/usb_notify.h>
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif
#include "s2mm006_ext.h"
#include "s2mm006_muic.h"
#include "s2mm006_alternate.h"

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#define USB_MODE_MAX_STRING 3

static enum pdic_sysfs_property s2mm006_sysfs_properties[] = {
	PDIC_SYSFS_PROP_CHIP_NAME,
	PDIC_SYSFS_PROP_CUR_VERSION,
	PDIC_SYSFS_PROP_SRC_VERSION,
	PDIC_SYSFS_PROP_LPM_MODE,
	PDIC_SYSFS_PROP_STATE,
	PDIC_SYSFS_PROP_RID,
	PDIC_SYSFS_PROP_CTRL_OPTION,
	PDIC_SYSFS_PROP_BOOTING_DRY,
	PDIC_SYSFS_PROP_FW_WATER,
	PDIC_SYSFS_PROP_DEX_FAN_UVDM,
	PDIC_SYSFS_PROP_ACC_DEVICE_VERSION,
	PDIC_SYSFS_PROP_CONTROL_GPIO,
	PDIC_SYSFS_PROP_USBPD_IDS,
	PDIC_SYSFS_PROP_USBPD_TYPE,
	PDIC_SYSFS_PROP_CC_PIN_STATUS,
	PDIC_SYSFS_PROP_SBU_ADC,
	PDIC_SYSFS_PROP_VSAFE0V_STATUS,
	PDIC_SYSFS_PROP_OVP_IC_SHUTDOWN,
	PDIC_SYSFS_PROP_HMD_POWER,
#if defined(CONFIG_SEC_FACTORY)
	PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE,
#endif
	PDIC_SYSFS_PROP_USB_BOOT_MODE,
	PDIC_SYSFS_PROP_DP_SBU_SW_SEL,
	PDIC_SYSFS_PROP_FW_UPDATE,
	PDIC_SYSFS_PROP_MAX_COUNT,
};

static int s2mm006_sbu_measure(struct s2mm006_data *usbpd_data,
		struct s2mm006_sbu *sbu_info)
{
	I2C_WATER_CTRL1_Type water_ctrl1;
	SBU1_STATUS_Type sbu1_status;
	SBU2_STATUS_Type sbu2_status;
	int ret = 0;

	pr_info("%s +\n", __func__);

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_WATER_CTRL1, &water_ctrl1.BYTE);
	if (ret < 0)
		goto err;

	water_ctrl1.BITS.SbuMeas = 1;

	ret = s2mm006_write_byte(usbpd_data->i2c,
			S2MM006_REG_WATER_CTRL1, &water_ctrl1.BYTE);
	if (ret < 0)
		goto err;

	msleep(50);

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_SBU1_STATUS, &sbu1_status.BYTE);
	if (ret < 0)
		goto err;

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_SBU2_STATUS, &sbu2_status.BYTE);
	if (ret < 0)
		goto err;

	sbu_info->sbu1_vol = sbu1_status.BITS.Sbu1Voltage;
	sbu_info->sbu1_imp = sbu1_status.BITS.Sbu1Impedance;
	sbu_info->sbu2_vol = sbu2_status.BITS.Sbu2Voltage;
	sbu_info->sbu2_imp = sbu2_status.BITS.Sbu2Impedance;

	pr_info("%s sbu1_vol=0x%x,sbu1_imp=0x%x,sbu2_vol=0x%x,sbu2_imp=0x%x -\n",
			__func__, sbu_info->sbu1_vol, sbu_info->sbu1_imp,
			sbu_info->sbu2_vol, sbu_info->sbu2_imp);
	return 0;
err:
	pr_err("%s error. ret=%d -\n", __func__, ret);
	return ret;
}

static int s2mm006_get_cc_pin_status
	(struct s2mm006_data *usbpd_data)
{
	int cc_pin_active = 0, nocc_usbact = 0;
	int ret = 0;

	cc_pin_active = usbpd_data->cc_pin_active;
	nocc_usbact = usbpd_data->fcy_wakeup;

	if (cc_pin_active == S2MM006_CC_1ACTIVE)
		ret = PDIC_NOTIFY_PIN_STATUS_CC1_ACTIVE;
	else if (cc_pin_active == S2MM006_CC_2ACTIVE)
		ret = PDIC_NOTIFY_PIN_STATUS_CC2_ACTIVE;
	else if (nocc_usbact)
		ret = PDIC_NOTIFY_PIN_STATUS_NOCC_USB_ACTIVE;
	else
		ret = PDIC_NOTIFY_PIN_STATUS_NO_DETERMINATION;
	return ret;
}

static void s2mm006_set_sleep_test
		(struct s2mm006_data *usbpd_data, int on)
{
	s2mm006_set_factory_ctrl(usbpd_data, S2MM006_FACTORY_NOCON, on);
}

static void s2mm006_control_gpio_for_sbu(int onoff)
{
	struct otg_notify *o_notify = get_otg_notify();
	struct usb_notifier_platform_data *pdata = get_notify_data(o_notify);

	if (o_notify && o_notify->set_ldo_onoff)
		o_notify->set_ldo_onoff(pdata, onoff);
}

static ssize_t s2mm006_set_usb_boot_mode(struct s2mm006_data *usbpd_data,
		const char *buf, size_t size)
{
	char *usb_mode = NULL;
	ssize_t ret = -ENOMEM;
	int sret = 0, usbid = 0;

	if (size > USB_MODE_MAX_STRING) {
		ret = -E2BIG;
		goto error;

	}
	usb_mode = kzalloc(size+1, GFP_KERNEL);
	if (!usb_mode)
		goto error;

	sret = sscanf(buf, "%s", usb_mode);
	if (sret != 1) {
		ret = -EBUSY;
		goto error1;
	}

	pr_info("%s usb_mode=%s\n", __func__, usb_mode);

	if (!strncmp(usb_mode, "LC", 2))
		usbid = S2MM006_FACTORY_255K;
	else if (!strncmp(usb_mode, "IB", 2))
		usbid = S2MM006_FACTORY_619K;
	else if (!strncmp(usb_mode, "OB", 2))
		usbid = S2MM006_FACTORY_523K;
	else if (!strncmp(usb_mode, "DL", 2))
		usbid = S2MM006_FACTORY_301K;
	else {
		ret = -ENOPARAM;
		goto error1;
	}

	s2mm006_set_factory_ctrl(usbpd_data, usbid, 0);

	ret = size;
error1:
	kfree(usb_mode);
error:
	pr_info("%s ret=%zd\n", __func__, ret);
	return ret;
}

static int s2mm006_sysfs_get_prop(struct _pdic_data_t *ppdic_data,
					enum pdic_sysfs_property prop,
					char *buf)
{
	struct s2mm006_data *usbpd_data =
		(struct s2mm006_data *)ppdic_data->drv_data;
	void *pp_data = usbpd_data->pp_data;
	struct s2mm006_sbu sbu_info;
	int ret = 0;
	uint8_t main_fw = 0, main_test_fw = 0, factory_fw = 0;

	if (!usbpd_data) {
		pr_err("usbpd_data is null : request prop = %d", prop);
		ret = -ENXIO;
		goto err;
	}

	switch (prop) {
	case PDIC_SYSFS_PROP_LPM_MODE:
		ret = sprintf(buf, "%d\n", usbpd_data->manual_lpm_mode);
		pr_info("PDIC_SYSFS_PROP_LPM_MODE : %d", usbpd_data->manual_lpm_mode);
		break;
	case PDIC_SYSFS_PROP_STATE:
		ret = sprintf(buf, "%d\n", usbpd_data->cc_status);
		pr_info("PDIC_SYSFS_PROP_STATE : %d", usbpd_data->cc_status);
		break;
	case PDIC_SYSFS_PROP_CUR_VERSION:
		s2mm006_read_byte(usbpd_data->i2c,
				S2MM006_REG_DEVICE_FW_ID, &main_fw);
		s2mm006_read_byte(usbpd_data->i2c,
				S2MM006_REG_DEVICE_FW_TEST_ID, &main_test_fw);
		s2mm006_read_byte(usbpd_data->i2c,
				S2MM006_REG_DEVICE_BOOT_ID, &factory_fw);
		ret = sprintf(buf, "%02X.%02X.%02X\n",
				main_test_fw, main_fw, factory_fw);
		pr_info("PDIC_SYSFS_PROP_CUR_VERSION : %02X.%02X.%02X",
				main_test_fw, main_fw, factory_fw);
		break;
	case PDIC_SYSFS_PROP_SRC_VERSION:
		main_fw = s2mm006_get_header_fw_ver(S2MM006_MAIN);
		main_test_fw = s2mm006_get_header_fw_ver(S2MM006_MAIN_TEST);
		factory_fw = s2mm006_get_header_fw_ver(S2MM006_FACTORY_BOOT);
		ret = sprintf(buf, "%02X.%02X.%02X\n",
				main_test_fw, main_fw, factory_fw);
		pr_info("PDIC_SYSFS_PROP_CUR_VERSION : %02X.%02X.%02X",
				main_test_fw, main_fw, factory_fw);
		break;
	case PDIC_SYSFS_PROP_RID:
		ret = sprintf(buf, "%d\n", usbpd_data->cur_rid);
		pr_info("PDIC_SYSFS_PROP_RID : %d",
				usbpd_data->cur_rid);
		break;
	case PDIC_SYSFS_PROP_BOOTING_DRY:
		ret = s2mm006_sbu_measure(usbpd_data, &sbu_info);
		pr_info("PDIC_SYSFS_PROP_BOOTING_DRY : sbu1:%d sbu2:%d",
				sbu_info.sbu1_imp, sbu_info.sbu2_imp);
		/* 1 step 50K ohm. 13 is 650K ohm */
		if (ret == 0 && sbu_info.sbu1_imp > 13 && sbu_info.sbu2_imp > 13)
			ret = sprintf(buf, "%d\n", 1);
		else
			ret = sprintf(buf, "%d\n", 0);
		break;
	case PDIC_SYSFS_PROP_FW_WATER:
		ret = sprintf(buf, "%d\n", usbpd_data->water_det);
		pr_info("PDIC_SYSFS_PROP_FW_WATER : %d",
				usbpd_data->water_det);
		break;
	case PDIC_SYSFS_PROP_ACC_DEVICE_VERSION:
		pr_info("khos %s 0x%04x\n", __func__, usbpd_data->alt_info.bcd_device);
		ret = sprintf(buf, "%04x\n", usbpd_data->alt_info.bcd_device);
		break;
	case PDIC_SYSFS_PROP_USBPD_IDS:
		ret = sprintf(buf, "%04x:%04x\n",
			le16_to_cpu(usbpd_data->alt_info.vendor_id),
			le16_to_cpu(usbpd_data->alt_info.product_id));
		break;
	case PDIC_SYSFS_PROP_USBPD_TYPE:	/* for SWITCH_STATE */
		ret = sprintf(buf, "%d\n", pdic_policy_get_acc_type(pp_data));
		pr_info("usb: %s : %d\n",
			__func__, pdic_policy_get_acc_type(pp_data));
		break;
	case PDIC_SYSFS_PROP_CONTROL_GPIO:
		ret = s2mm006_sbu_measure(usbpd_data, &sbu_info);

		pr_info("PDIC_SYSFS_PROP_CONTROL_GPIO:sbu1_vol=0x%x,sbu2_vol=0x%x\n",
			sbu_info.sbu1_vol, sbu_info.sbu2_vol);
		ret = sprintf(buf, "%d %d\n", !!sbu_info.sbu1_vol, !!sbu_info.sbu2_vol);
		break;
	case PDIC_SYSFS_PROP_CC_PIN_STATUS:
		ret = sprintf(buf, "%d\n",
				s2mm006_get_cc_pin_status(usbpd_data));
		pr_info("PDIC_SYSFS_PROP_PIN_STATUS : %d",
				s2mm006_get_cc_pin_status(usbpd_data));
		break;
	case PDIC_SYSFS_PROP_SBU_ADC:
		ret = s2mm006_sbu_measure(usbpd_data, &sbu_info);
		pr_info("PDIC_SYSFS_PROP_SBU_ADC : sbu1:%d sbu2:%d",
				sbu_info.sbu1_imp, sbu_info.sbu2_imp);
		/* 1 step 50K ohm. 13 is 650K ohm */
		ret = sprintf(buf, "%d %d\n", sbu_info.sbu1_imp, sbu_info.sbu2_imp);
		break;
	case PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE:
		if (usbpd_data->water_detect_support)
			ret = sprintf(buf, "uevent\n");
		else
			ret = sprintf(buf, "unsupport\n");
		pr_info("%s : PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE : %s",
				__func__, buf);
		break;
	default:
		break;
	}
err:
	return ret;
}

static ssize_t s2mm006_sysfs_set_prop(struct _pdic_data_t *ppdic_data,
				    enum pdic_sysfs_property prop,
				    const char *buf, size_t size)
{
	struct s2mm006_data *usbpd_data =
		(struct s2mm006_data *)ppdic_data->drv_data;
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	struct s2mm006_data *pogo_usbpd_data = get_s2mm006_usbpd_data();	
#endif
	ssize_t ret = size;
	int val[3] = {0,};
	int aux_sw_sel = 0, aux_sw_en = 0;
	int mode = 0, rv = 0;

	switch (prop) {
	case PDIC_SYSFS_PROP_LPM_MODE:
		rv = sscanf(buf, "%d", &mode);
		pr_info("PDIC_SYSFS_PROP_LPM_MODE mode=%d", mode);
		switch (mode) {
		case 0:
			s2mm006_set_sleep_test(usbpd_data, 0);
			usbpd_data->manual_lpm_mode = 0;
			break;
		case 1:
		case 2:
			s2mm006_set_sleep_test(usbpd_data, 1);
			usbpd_data->manual_lpm_mode = 1;
			break;
		default:
			s2mm006_set_sleep_test(usbpd_data, 0);
			usbpd_data->manual_lpm_mode = 0;
			break;
		}
		break;
	case PDIC_SYSFS_PROP_CONTROL_GPIO:
		rv = sscanf(buf, "%d", &mode);
		/* original concept : mode 0 : SBU1/SBU2 set as open-drain status
		 *                   mode 1 : SBU1/SBU2 set as default status - Pull up
		 */
		pr_info("PDIC_SYSFS_PROP_CONTROL_GPIO mode=%d", mode);
		s2mm006_control_gpio_for_sbu(!mode);
		break;
	case PDIC_SYSFS_PROP_CTRL_OPTION:
		rv = sscanf(buf, "%d", &mode);
		pr_info("PDIC_SYSFS_PROP_CTRL_OPTION mode=%d", mode);
		break;
	case PDIC_SYSFS_PROP_USB_BOOT_MODE:
		pr_info("PDIC_SYSFS_PROP_USB_BOOT_MODE\n");
		ret = s2mm006_set_usb_boot_mode(usbpd_data, buf, size);
		break;
	case PDIC_SYSFS_PROP_DP_SBU_SW_SEL:
		if (strnchr(buf, size, '-')) {
			pr_err("%s range option not allowed\n", __func__);
			ret = -EINVAL;
			break;
		}

		get_options(buf, 3, val);

		aux_sw_sel = val[1];
		aux_sw_en = val[2];

		pr_info("PDIC_SYSFS_PROP_DP_SBU_SW_SEL.sbu_sw_en=%d, sbu_sw_sel=%d\n",
			aux_sw_en, aux_sw_sel);

		/* aux_sw_en = 0 => enable, aux_sw_en = 1 -> disable */
		/* aux_sw_sel = 0 => cc1 active, aux_sw_sel = 1 => cc2 active */
		if (aux_sw_en == 0) {
			if (aux_sw_sel == 0)
			s2mm006_sbu_control(usbpd_data, CLOSE_SBU_CC1_ACTIVE);
			else
			s2mm006_sbu_control(usbpd_data, CLOSE_SBU_CC2_ACTIVE);
		} else
			s2mm006_sbu_control(usbpd_data, OPEN_SBU);
		break;
	case PDIC_SYSFS_PROP_FW_UPDATE:
		rv = sscanf(buf, "%d", &mode);
		pr_info("%s, PDIC_SYSFS_PROP_FW_UPDATE : mode (%d)\n", __func__, mode);

#if IS_ENABLED(CONFIG_S2MM006_POGO)
		/* pogo pdic update */
		if (mode == 2 || mode == 3) {
			mode -= 2;
			s2mm006_fw_sysfs_update(pogo_usbpd_data, mode);
		}
		/* typec pdic update */
		else
#endif
			s2mm006_fw_sysfs_update(usbpd_data, mode);

		break;
	default:
		break;
	}

	return ret;
}

static int s2mm006_sysfs_is_writeable(struct _pdic_data_t *ppdic_data,
				    enum pdic_sysfs_property prop)
{
	int ret = 0;

	switch (prop) {
	case PDIC_SYSFS_PROP_LPM_MODE:
	case PDIC_SYSFS_PROP_CONTROL_GPIO:
	case PDIC_SYSFS_PROP_USB_BOOT_MODE:
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

static int s2mm006_sysfs_is_writeonly(struct _pdic_data_t *ppdic_data,
				    enum pdic_sysfs_property prop)
{
	int ret = 0;

	switch (prop) {
	case PDIC_SYSFS_PROP_CTRL_OPTION:
	case PDIC_SYSFS_PROP_DP_SBU_SW_SEL:
	case PDIC_SYSFS_PROP_FW_UPDATE:
		ret = 1;
		break;
	default:
		break;
	}

	return ret;
}

int s2mm006_pdic_sysfs_init(struct s2mm006_data *usbpd_data,
		ppdic_sysfs_property_t *ppdic_sysfs_prop)
{
	ppdic_sysfs_property_t s2mm006_sysfs_prop;
	int ret = 0;

	s2mm006_sysfs_prop = devm_kzalloc(usbpd_data->dev,
				sizeof(pdic_sysfs_property_t), GFP_KERNEL);
	if (!ppdic_sysfs_prop) {
		ret = -ENOMEM;
		goto err;
	}

	s2mm006_sysfs_prop->get_property = s2mm006_sysfs_get_prop;
	s2mm006_sysfs_prop->set_property = s2mm006_sysfs_set_prop;
	s2mm006_sysfs_prop->property_is_writeable = s2mm006_sysfs_is_writeable;
	s2mm006_sysfs_prop->property_is_writeonly = s2mm006_sysfs_is_writeonly;
	s2mm006_sysfs_prop->properties = s2mm006_sysfs_properties;
	s2mm006_sysfs_prop->num_properties = ARRAY_SIZE(s2mm006_sysfs_properties);

	*ppdic_sysfs_prop = s2mm006_sysfs_prop;
err:
	return ret;
}
#endif
