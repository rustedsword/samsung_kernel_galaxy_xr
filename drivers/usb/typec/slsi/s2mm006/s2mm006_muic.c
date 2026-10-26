// SPDX-License-Identifier: GPL-2.0
/*
 * s2mm006_muic.c - S2MM006 micro USB switch device driver
 *
 * Copyright (C) 2020 Samsung Electronics
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
#define pr_fmt(fmt)	"[MUIC] " fmt

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/muic/common/muic.h>
#include <linux/muic/slsi/platform/muic_platform_manager.h>

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
#include <linux/muic/common/muic_sysfs.h>
#endif

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif

#if IS_ENABLED(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#if IS_ENABLED(CONFIG_CP_UART_NOTI)
#include <soc/samsung/exynos-modem-ctrl.h>
#endif

#include "s2mm006_ext.h"
#include "s2mm006_muic.h"

/* Prototypes of the Static symbols of s2mm006-muic */
static int s2mm006_muic_detect_chg_type(struct s2mm006_muic_data *muic_data);
static void s2mm006_muic_handle_attached_dev(struct s2mm006_muic_data *muic_data);
static void s2mm006_muic_get_detect_info(struct s2mm006_muic_data *muic_data);
static int _s2mm006_muic_get_vbus_det(struct s2mm006_muic_data *muic_data);
static int _s2mm006_muic_get_chg_type(struct s2mm006_muic_data *muic_data);
static int s2mm006_hv_muic_get_vbus_adc(struct s2mm006_muic_data *muic_data);

/*
 * Debuging functions
 */
static const char *dev_to_str(muic_attached_dev_t n)
{
	char *ret;

	switch (n) {
	ENUM_STR(ATTACHED_DEV_NONE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_USB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CDP_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_OTG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNDEFINED_CHARGING_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_DESKDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNKNOWN_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_DESKDOCK_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CARDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_ON_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_ON_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_USB_OFF_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_USB_ON_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_USB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AUDIODOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_MHL_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CHARGING_CABLE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_5V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_9V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_5V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_9V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HMT_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_VZW_ACC_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_USB_LANHUB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE2_CHG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_MUIC_TA, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_ADAPTER_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_CHARGER_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_NONE_TYPE3_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNSUPPORTED_ID_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TIMEOUT_OPEN_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_WIRELESS_PAD_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_POWERPACK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNDEFINED_RANGE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CHK_WATER_REQ, ret);
	ENUM_STR(ATTACHED_DEV_CHK_WATER_DRY_REQ, ret);
	ENUM_STR(ATTACHED_DEV_GAMEPAD_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CHECK_OCP, ret);
	ENUM_STR(ATTACHED_DEV_RDU_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNKNOWN_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_NUM, ret);
	fallthrough;
	default:
		return "invalid";
	}
	return ret;
}

static const char * const s2mm006_path[] = {
	[S2MM006_PATH_OPEN]	= "MANUAL OPEN",
	[S2MM006_PATH_USB]	= "USB",
	[S2MM006_PATH_UART]	= "UART",
	[S2MM006_PATH_JTAG]	= "JTAG",
	[S2MM006_PATH_HICCUP]	= "HICCUP",
	[S2MM006_PATH_AUTO]	= "AUTO",
};

static void s2mm006_muic_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static void s2mm006_muic_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static int s2mm006_muic_set_wake_lock(struct s2mm006_muic_data *muic_data)
{
	struct wakeup_source *muic_ws = NULL;

	muic_ws = wakeup_source_register(NULL, "s2mm006_muic_wake");

	if (muic_ws == NULL) {
		pr_info("%s, Fail to register ws\n", __func__);
		goto err;
	}

	muic_data->muic_ws = muic_ws;

	return 0;
err:
	return -1;
}

static int s2mm006_muic_path_set(struct s2mm006_muic_data *muic_data, int path)
{
	u8 reg_val = 0;
	int ret = 0;

	if (path > S2MM006_PATH_AUTO) {
		ret = -ENOMSG;
		goto err;
	}

	pr_info("%s path=%s\n", __func__, s2mm006_path[path]);

	ret = s2mm006_read_byte(muic_data->i2c, S2MM006_REG_BC_CTRL2, &reg_val);
	if (ret < 0)
		goto err;

	switch (path) {
	case S2MM006_PATH_OPEN:
		reg_val |= BC_CTRL2_MANUAL_SW_MASK;
		reg_val &= ~BC_CTRL2_DPDM_SWITCHING_MASK;
		reg_val |= BC_CTRL2_DPDM_SWITCHING_OPEN_MASK;
		break;
	case S2MM006_PATH_USB:
		reg_val |= BC_CTRL2_MANUAL_SW_MASK;
		reg_val &= ~BC_CTRL2_DPDM_SWITCHING_MASK;
		reg_val |= BC_CTRL2_DPDM_SWITCHING_USB_MASK;
		break;
	case S2MM006_PATH_UART:
		reg_val |= BC_CTRL2_MANUAL_SW_MASK;
		reg_val &= ~BC_CTRL2_DPDM_SWITCHING_MASK;
		reg_val |= BC_CTRL2_DPDM_SWITCHING_UART_MASK;
		break;
	case S2MM006_PATH_JTAG:
		reg_val |= BC_CTRL2_MANUAL_SW_MASK;
		reg_val &= ~BC_CTRL2_DPDM_SWITCHING_MASK;
		reg_val |= BC_CTRL2_DPDM_SWITCHING_JTAG_MASK;
		break;
	case S2MM006_PATH_HICCUP:
		reg_val |= BC_CTRL2_MANUAL_SW_MASK;
		reg_val &= ~BC_CTRL2_DPDM_SWITCHING_MASK;
		reg_val |= BC_CTRL2_DPDM_SWITCHING_HICCUP_MASK;
		break;
	case S2MM006_PATH_AUTO:
		reg_val &= ~BC_CTRL2_MANUAL_SW_MASK;
		reg_val &= ~BC_CTRL2_DPDM_SWITCHING_MASK;
		reg_val |= BC_CTRL2_DPDM_SWITCHING_OPEN_MASK;
		break;
	default:
		ret = -EPROTO;
		goto err;
	}
	ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_BC_CTRL2, &reg_val);
	if (ret < 0)
		goto err;
	return 0;
err:
	pr_err("%s. error. ret=%d\n", __func__, ret);
	return ret;
}

static void s2mm006_hv_muic_set_request_reg(struct s2mm006_muic_data *muic_data, muic_hv_voltage_t hv_ctrl)
{
	u8 reg_val, tx_data = 0;
	u8 chg_type = 0;
	int ret = 0;

	/* Set V_I_BYTE with hv_ctrl */
	if (hv_ctrl == HV_5V) {
		tx_data = 0x08;
	} else if (hv_ctrl == HV_9V) {
		tx_data = 0x46;
	} else {
		pr_err("%s invalid hv_ctrl: %d\n", __func__, hv_ctrl);
		return;
	}

	chg_type = _s2mm006_muic_get_chg_type(muic_data);
	pr_info("%s chg_type: %d, hv_ctrl: %d\n", __func__, chg_type, hv_ctrl);

	if (chg_type == CHG_TYPE_HVDCP_AFC) {
		/* Request AFC protocol to S2MM006 with tx_data(V_I_BYTE) */
		ret = s2mm006_read_byte(muic_data->i2c, S2MM006_REG_AFC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;
		reg_val |= AFC_CTRL1_AFC_EN_MASK;
		ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_AFC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;

		ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_AFC_CTRL3, &tx_data);
		if (ret < 0)
			goto err;

		reg_val |= AFC_CTRL1_AFC_REQ_MASK;
		ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_AFC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;
	} else if (chg_type == CHG_TYPE_HVDCP_QC) {
		/* Request QC protocol to S2MM006 with tx_data(V_I_BYTE) */
		ret = s2mm006_read_byte(muic_data->i2c, S2MM006_REG_QC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;

		reg_val |= QC_CTRL1_QC_EN_MASK;
		ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_QC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;

		ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_QC_TX_DATA, &tx_data);
		if (ret < 0)
			goto err;

		ret = s2mm006_read_byte(muic_data->i2c, S2MM006_REG_AFC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;

		reg_val |= AFC_CTRL1_AFC_REQ_MASK;
		ret = s2mm006_write_byte(muic_data->i2c, S2MM006_REG_AFC_CTRL1, &reg_val);
		if (ret < 0)
			goto err;
	} else {
		pr_err("%s invalid chg_type: %d\n", __func__, chg_type);
	}
	return;
err:
	pr_err("%s. i2c error\n", __func__);
}

static int s2mm006_hv_muic_get_vbus_adc(struct s2mm006_muic_data *muic_data)
{
	u8 reg_val;
	int vbus_adc = 0, ret = 0;

	reg_val = 0;
	ret = s2mm006_write_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_STATUS2, &reg_val);
	if (ret < 0)
		goto err;

	/* Request to measure VBUS_ADC */
	ret = s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_CTRL1, &reg_val);
	if (ret < 0)
		goto err;

	reg_val |= BC_CTRL1_VBUS_ADC_REQ_MASK;
	ret = s2mm006_write_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_CTRL1, &reg_val);
	if (ret < 0)
		goto err;

	/* vbus_adc is measured within 10ms with VBUS_ADC INT */
	usleep_range(10000, 11000);

	ret = s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_STATUS2, &reg_val);
	if (ret < 0)
		goto err;

	vbus_adc = (reg_val & BC_STATUS2_VBUS_ADC_MASK) * 500; /* 500mV per 1 code step */

	pr_info("%s VBUS_ADC:%d [mV]\n", __func__, vbus_adc);

	return vbus_adc; /* mV */
err:
	pr_err("%s. i2c error\n", __func__);
	return vbus_adc;
}


static int s2mm006_hv_muic_set_hv_ctrl(struct s2mm006_muic_data *muic_data, muic_hv_voltage_t hv_ctrl)
{
	int i, retry = 3;
	int vbus_adc = 0, vbus_det = 0, chg_type = 0;
	int ret = 0;

	for (i = 0; i < retry; i++) {
		vbus_det = _s2mm006_muic_get_vbus_det(muic_data);
		chg_type = _s2mm006_muic_get_chg_type(muic_data);
		if (chg_type == CHG_TYPE_NO_CONNECTION || vbus_det == 0) {
			pr_err("%s invalid state vbus_det:%d, chg_type:%d\n", __func__, vbus_det, chg_type);
			ret = -EINVAL; /* error */
			break;
		}

		/* Request AFC/QC 9V/5V */
		s2mm006_hv_muic_set_request_reg(muic_data, hv_ctrl);
		msleep(300); /* Wait for protocol */

		ret = s2mm006_hv_muic_get_vbus_adc(muic_data);
		if (ret < 0)
			return ret; /* error to measure VBUS_ADC */

		vbus_adc = ret;

		if (hv_ctrl == HV_5V && IS_VBUS_5V(vbus_adc)) {
			if (chg_type == CHG_TYPE_HVDCP_AFC)
				muic_data->new_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
			else if (chg_type == CHG_TYPE_HVDCP_QC)
				muic_data->new_dev = ATTACHED_DEV_QC_CHARGER_5V_MUIC;
			return 0; /* hv ctrl done */
		} else if (hv_ctrl == HV_9V && IS_VBUS_9V(vbus_adc)) {
			if (chg_type == CHG_TYPE_HVDCP_AFC)
				muic_data->new_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
			else if (chg_type == CHG_TYPE_HVDCP_QC)
				muic_data->new_dev = ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			return 0; /* hv ctrl done */
		}
	}

	pr_info("%s error occurred.\n", __func__);
	return -ETIME;
}

static int s2mm006_ops_handle_hv_work(void *mdata)
{
	struct s2mm006_muic_data *muic_data = (struct s2mm006_muic_data *)mdata;
	struct muic_share_data *sdata = muic_data->sdata;
	int ret = 0;

	pr_info("%s +\n", __func__);

	switch (sdata->hv_voltage) {
	case HV_5V:
		ret = s2mm006_hv_muic_set_hv_ctrl(muic_data, HV_5V);
		if (ret < 0) {
			pr_info("%s hv work failed. hv_voltage=(%d)\n", __func__,
					sdata->hv_voltage);
			goto ERR;
		}
		muic_platform_handle_hv_attached_dev(sdata, muic_data->new_dev);
		break;
	case HV_9V:
		ret = s2mm006_hv_muic_set_hv_ctrl(muic_data, HV_9V);
		if (ret < 0) {
			pr_info("%s hv work failed. hv_voltage=(%d)\n", __func__,
					sdata->hv_voltage);
			goto ERR;
		}
		muic_platform_handle_hv_attached_dev(sdata, muic_data->new_dev);
		break;
	default:
		break;
	}
ERR:
	pr_info("%s -\n", __func__);
	return ret;
}

static inline int _s2mm006_muic_get_rid_adc(struct s2mm006_muic_data *muic_data)
{
	u8 rid;

	s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_FACTORY_STATUS, &rid);
	return rid & FACTORY_STATUS_RID_VALUE_MASK;
}

static int _s2mm006_muic_get_vbus_det(struct s2mm006_muic_data *muic_data)
{
	u8 data;

	s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_STATUS1, &data);
	return data & BC_STATUS1_VBUS_DET_MASK;
}

static int _s2mm006_muic_get_chg_type(struct s2mm006_muic_data *muic_data)
{
	BC_STATUS1_Type bc_status1;

	s2mm006_read_byte(muic_data->i2c, S2MM006_REG_BC_STATUS1, &bc_status1.BYTE);
	return bc_status1.BITS.ChgType;
}

int s2mm006_set_gpio_uart_sel(struct s2mm006_muic_data *muic_data, int uart_sel)
{
	const char *mode;
	struct muic_share_data *sdata = muic_data->sdata;
	int uart_sel_gpio = sdata->gpio_uart_sel;
	int uart_sel_val;
	int ret;

	ret = gpio_request(uart_sel_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, 1);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, 0);
		break;
	default:
		mode = "Error";
		break;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	gpio_free(uart_sel_gpio);

	pr_info("%s, GPIO_UART_SEL(%d)=%c\n",
		mode, uart_sel_gpio, (uart_sel_val == 0 ? 'L' : 'H'));

	return 0;
}

static void s2mm006_muic_get_detect_info(struct s2mm006_muic_data *muic_data)
{
	s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_STATUS1, &(muic_data->reg[DEVICE_TYPE1]));
	s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_STATUS2, &(muic_data->reg[DEVICE_TYPE2]));
	s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_BC_STATUS3, &(muic_data->reg[DEVICE_TYPE3]));

	muic_data->vbvolt = !!(muic_data->reg[DEVICE_TYPE1] & BC_STATUS1_VBUS_DET_MASK);
	pr_info("dev[1:0x%02x, 2:0x%02x, 3:0x%02x], vbus[%x]\n", muic_data->reg[DEVICE_TYPE1],
		muic_data->reg[DEVICE_TYPE2], muic_data->reg[DEVICE_TYPE3], muic_data->vbvolt);
}

/*
 * Interface Functions
 */
#if IS_ENABLED(CONFIG_MUIC_S2MM006_RID)
static int s2mm006_if_get_adc(void *mdata)
{
	struct s2mm006_muic_data *muic_data = (struct s2mm006_muic_data *)mdata;
	int ret = 0;

	mutex_lock(&muic_data->switch_mutex);

	ret = _s2mm006_muic_get_rid_adc(muic_data);

	mutex_unlock(&muic_data->switch_mutex);

	return ret;
}
#endif

static int s2mm006_ops_get_vbus_state(void *mdata)
{
	struct s2mm006_muic_data *muic_data = (struct s2mm006_muic_data *)mdata;

	return _s2mm006_muic_get_vbus_det(muic_data);
}

static int s2mm006_ops_set_gpio_uart_sel(void *mdata, int uart_path)
{
	struct s2mm006_muic_data *muic_data = (struct s2mm006_muic_data *)mdata;

	return s2mm006_set_gpio_uart_sel(muic_data, uart_path);
}

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
static int s2mm006_ops_show_register(void *mdata, char *mesg)
{
#if IS_ENABLED(CONFIG_MUIC_DEBUG)
	struct s2mm006_muic_data *muic_data =
		(struct s2mm006_muic_data *)mdata;

	if (mesg != NULL)
		s2mm006_read_reg_dump(muic_data, mesg);
	pr_info("%s %s\n", __func__, mesg);
#endif
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int s2mm006_ops_set_hiccup_mode(void *mdata, bool en)
{
	struct s2mm006_muic_data *muic_data = (struct s2mm006_muic_data *)mdata;
	int ret = 0;

	pr_info("%s en=%d +\n", __func__, en);
	if (en) {
		ret = s2mm006_muic_path_set(muic_data, S2MM006_PATH_HICCUP);
		if (ret < 0)
			goto err;
	} else {
		ret = s2mm006_muic_path_set(muic_data, S2MM006_PATH_OPEN);
		if (ret < 0)
			goto err;

		ret = s2mm006_muic_path_set(muic_data, S2MM006_PATH_AUTO);
		if (ret < 0)
			goto err;
	}
err:
	pr_info("%s en=%d ret=%d -\n", __func__, en, ret);
	return ret;
}
#endif

static int s2mm006_muic_reg_init(struct s2mm006_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	s2mm006_muic_get_detect_info(muic_data);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_handle((!!muic_data->vbvolt) ? STATUS_VBUS_HIGH : STATUS_VBUS_LOW);
#endif /* CONFIG_VBUS_NOTIFIER */

	return ret;
}

static int s2mm006_muic_reset_data(struct s2mm006_muic_data *muic_data)
{
	muic_data->new_dev = ATTACHED_DEV_NONE_MUIC;
	muic_data->is_vbus_adc_valid = 0;

	return 0;
}

static int s2mm006_muic_detect_chg_type(struct s2mm006_muic_data *muic_data)
{
	u8 chg_type;
	struct muic_share_data *sdata = muic_data->sdata;

	muic_data->new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	chg_type = _s2mm006_muic_get_chg_type(muic_data);
	pr_info("%s new chg_type:%d\n", __func__, chg_type);

	/* Attached */
	switch (muic_data->reg[DEVICE_TYPE1] & BC_STATUS1_CHG_TYPE_MASK) {
	case BC_STATUS1_CHG_TYPE_NO_CONN_MASK:
		pr_info("%s: No Connection DETECTED\n", __func__);
		muic_data->new_dev = ATTACHED_DEV_NONE_MUIC;
		return S2MM006_DETECT_DETACH;
	case BC_STATUS1_CHG_TYPE_TIMEOUT_MASK:
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		pr_info("%s: SDP_1P8S(USB) DETECTED\n", __func__);
		muic_data->new_dev = ATTACHED_DEV_USB_MUIC;
#else
		pr_info("%s: SDP_1P8S DETECTED\n", __func__);
		muic_data->new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
#endif
		break;
	case BC_STATUS1_CHG_TYPE_CDP_MASK:
		muic_data->new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s USB_CDP DETECTED\n", __func__);
		break;
	case BC_STATUS1_CHG_TYPE_SDP_MASK:
		pr_info("%s USB DETECTED\n", __func__);
		muic_data->new_dev = ATTACHED_DEV_USB_MUIC;
		break;
	case BC_STATUS1_CHG_TYPE_DCP_MASK:
		muic_data->new_dev = ATTACHED_DEV_TA_MUIC;
		muic_data->afc_check = true;
		pr_info("%s DEDICATED CHARGER DETECTED\n", __func__);
		break;
	case BC_STATUS1_CHG_TYPE_HVDCP_MASK:
		/* HVDCP is detected */
		return S2MM006_DETECT_SKIP;
	case BC_STATUS1_CHG_TYPE_HVDCP_AFC_MASK:
		/* AFC is detected */
		muic_platform_send_hv_ready(sdata);
		return S2MM006_DETECT_SKIP;
	case BC_STATUS1_CHG_TYPE_HVDCP_QC_MASK:
		/* QC is detected (AFC Nack) */
		muic_platform_send_hv_ready(sdata);
		return S2MM006_DETECT_SKIP;
	default:
		break;
	}

	if (muic_data->new_dev != ATTACHED_DEV_UNKNOWN_MUIC)
		return S2MM006_DETECT_DONE;
	else
		return S2MM006_DETECT_NONE;
}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
static int s2mm006_muic_detect_dev_rid_array(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	muic_data->new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	switch (muic_data->rid) {
	case FACTORY_STATUS_RID_VALUE_523K_MASK:
		pr_info("ADC_JIG_UART_OFF DETECTED\n");
		if (muic_data->vbvolt)
			muic_data->new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
		else
			muic_data->new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		break;
	case FACTORY_STATUS_RID_VALUE_301K_MASK:
	case FACTORY_STATUS_RID_VALUE_255K_MASK:
		muic_data->new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("ADC JIG_USB_ON DETECTED\n");
		break;
	case FACTORY_STATUS_RID_VALUE_619K_MASK:
		muic_data->new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		pr_info("ADC JIG_UART_ON DETECTED\n");
		break;
	default:
		pr_info("%s unsupported ADC(0x%02x)\n", __func__, muic_data->adc);
	}

	muic_platform_handle_rid(sdata, muic_data->new_dev);

	return muic_data->new_dev;
}
#endif /* CONFIG_MUIC_S2MM006_RID */

static void s2mm006_muic_handle_attached_dev(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	if (muic_data->new_dev != ATTACHED_DEV_UNKNOWN_MUIC
			&& muic_data->new_dev != sdata->attached_dev) {
		muic_platform_handle_attach(sdata, muic_data->new_dev);
	}
}

static void s2mm006_muic_handle_detach(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	s2mm006_muic_reset_data(muic_data);
	muic_platform_handle_detach(sdata);
}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
static int _s2mm006_muic_detach_handler(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	sdata = muic_data->sdata;
	if (sdata == NULL) {
		pr_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	mutex_lock(&muic_data->muic_mutex);
	s2mm006_muic_wake_lock(muic_data->muic_ws);

	if (MUIC_IS_ATTACHED(sdata->attached_dev) == false) {
		pr_err("%s Cable type already was detached\n", __func__);
		goto detach_skip;
	}
	s2mm006_muic_handle_detach(muic_data);

	pr_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));
detach_skip:
	s2mm006_muic_wake_unlock(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}
#endif

static void s2mm006_muic_sys_msg_isr(struct s2mm006_muic_data *muic_data)
{
	u8 data;

	s2mm006_read_byte(muic_data->i2c, S2MM006_REG_BC_STATUS3, &data);
	pr_info("%s SYS_MESSAGE(%#x)\n", __func__,
		(data >> BC_STATUS3_SYS_MESSAGE_SHIFT) & BC_STATUS3_SYS_MESSAGE_WIDTH);
}


static void s2mm006_muic_adc_msg_isr(struct s2mm006_muic_data *muic_data)
{
}

static void s2mm006_muic_chg_type_isr(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata;
	int det_ret = S2MM006_DETECT_NONE;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return;
	}

	sdata = muic_data->sdata;
	if (sdata == NULL) {
		pr_err("%s data NULL\n", __func__);
		return;
	}

	mutex_lock(&muic_data->muic_mutex);
	s2mm006_muic_wake_lock(muic_data->muic_ws);

	pr_info("%s start(%s)\n", __func__, dev_to_str(sdata->attached_dev));
	s2mm006_muic_get_detect_info(muic_data);

	det_ret = s2mm006_muic_detect_chg_type(muic_data);
	if (det_ret == S2MM006_DETECT_DONE)
		s2mm006_muic_handle_attached_dev(muic_data);
	else if (det_ret == S2MM006_DETECT_SKIP)
		goto attach_skip;
	else if (det_ret == S2MM006_DETECT_DETACH)
		s2mm006_muic_handle_detach(muic_data);

attach_skip:
	pr_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	s2mm006_muic_wake_unlock(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);
}

static void s2mm006_muic_vbus_adc_isr(struct s2mm006_muic_data *muic_data)
{
	/* VBUS_ADC value update */
	muic_data->is_vbus_adc_valid = 1;
}

static void s2mm006_muic_vbus_det_isr(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return;
	}

	sdata = muic_data->sdata;
	if (sdata == NULL) {
		pr_err("%s data NULL\n", __func__);
		return;
	}

	mutex_lock(&muic_data->muic_mutex);
	s2mm006_muic_wake_lock(muic_data->muic_ws);
	pr_info("%s start(%s)\n", __func__, dev_to_str(sdata->attached_dev));

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	if (_s2mm006_muic_get_vbus_det(muic_data))
		vbus_notifier_handle(STATUS_VBUS_HIGH);
	else
		vbus_notifier_handle(STATUS_VBUS_LOW);
#endif /* CONFIG_VBUS_NOTIFIER */

	pr_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));
	s2mm006_muic_wake_unlock(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);
}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
void s2mm006_muic_rid_det_isr(struct s2mm006_muic_data *muic_data)
{
	u8 rid;

	s2mm006_read_byte(muic_data->i2c, S2MM006_MUIC_REG_FACTORY_STATUS, &rid);
	if ((muic_data->rid == FACTORY_STATUS_RID_VALUE_OPEN_MASK)
		&& (rid & FACTORY_STATUS_RID_VALUE_MASK)) {
		muic_data->rid = (rid & FACTORY_STATUS_RID_VALUE_MASK);
		s2mm006_muic_detect_dev_rid_array(muic_data);
		s2mm006_muic_handle_attached_dev(muic_data);
	} else if ((muic_data->rid != FACTORY_STATUS_RID_VALUE_OPEN_MASK)
		&& ((rid & FACTORY_STATUS_RID_VALUE_MASK) == FACTORY_STATUS_RID_VALUE_OPEN_MASK)) {
		muic_data->rid = (rid & FACTORY_STATUS_RID_VALUE_MASK);
		_s2mm006_muic_detach_handler(muic_data);
	}
}
#endif

void process_muic(struct s2mm006_muic_data *muic_data, u8 irq)
{
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	s2mm006_muic_rid_det_isr(muic_data);
#endif
	if (irq & BC_INT_SYS_MESSAGE_MASK)
		s2mm006_muic_sys_msg_isr(muic_data);

	if (irq & BC_INT_ADC_MESSAGE_MASK)
		s2mm006_muic_adc_msg_isr(muic_data);

	if (irq & BC_INT_CHG_TYPE_MASK)
		s2mm006_muic_chg_type_isr(muic_data);

	if (irq & BC_INT_VBUS_ADC_MASK)
		s2mm006_muic_vbus_adc_isr(muic_data);

	if (irq & BC_INT_VBUS_DET_MASK)
		s2mm006_muic_vbus_det_isr(muic_data);
}

static void register_muic_ops(struct muic_ic_data *ic_data)
{
	ic_data->m_ops.get_vbus_state = s2mm006_ops_get_vbus_state;
	ic_data->m_ops.set_gpio_uart_sel = s2mm006_ops_set_gpio_uart_sel;
	ic_data->m_ops.show_register = s2mm006_ops_show_register;
	ic_data->m_ops.handle_hv_work = s2mm006_ops_handle_hv_work;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	ic_data->m_ops.set_hiccup_mode = s2mm006_ops_set_hiccup_mode;
#endif
}

struct s2mm006_muic_data *s2mm006_muic_init(struct i2c_client *i2c)
{
	struct s2mm006_muic_data *muic_data;
	struct muic_share_data *sdata;
	struct muic_ic_data *ic_data;

	int ret = 0;

	pr_info("%s start\n", __func__);
	muic_data = kzalloc(sizeof(*muic_data), GFP_KERNEL);
	if (unlikely(!muic_data)) {
		pr_err("%s out of memory\n", __func__);
		goto err1;
	}

	sdata = kzalloc(sizeof(*sdata), GFP_KERNEL);
	if (unlikely(!sdata)) {
		pr_err("%s out of memory\n", __func__);
		goto err2;
	}

	muic_data->sdata = sdata;

	ic_data = kzalloc(sizeof(*ic_data), GFP_KERNEL);
	if (unlikely(!ic_data)) {
		pr_err("%s out of memory\n", __func__);
		goto err3;
	}

	register_muic_ops(ic_data);
	ic_data->drv_data = (void *) muic_data;
	sdata->ic_data = ic_data;

	ret = register_muic_platform_layer(sdata);
	if (ret < 0) {
		pr_err("%s fail to register muic platform layer\n", __func__);
		goto err4;
	}

	muic_data->sdata->hv_work_delay = 100;
	muic_data->i2c = i2c;
	muic_data->afc_check = false;
	muic_data->rescan_cnt = 0;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	muic_data->rid = FACTORY_STATUS_RID_VALUE_OPEN_MASK;
#endif
	mutex_init(&muic_data->muic_mutex);
	mutex_init(&muic_data->switch_mutex);
	mutex_init(&muic_data->bcd_rescan_mutex);
	mutex_init(&muic_data->afc_mutex);
	s2mm006_muic_set_wake_lock(muic_data);

	muic_platform_init_gpio_cb(sdata);
	muic_platform_init_switch_dev(sdata);

	s2mm006_muic_reg_init(muic_data);
	s2mm006_muic_reset_data(muic_data);

	s2mm006_muic_get_detect_info(muic_data);
	s2mm006_muic_chg_type_isr(muic_data);

	pr_info("%s success end\n", __func__);
	return muic_data;
err4:
	kfree(ic_data);
err3:
	kfree(sdata);
err2:
	kfree(muic_data);
err1:
	pr_err("%s error end\n", __func__);
	return NULL;
}

int s2mm006_muic_remove(struct s2mm006_muic_data *muic_data)
{
	if (muic_data) {
		pr_info("%s\n", __func__);

		muic_platform_deinit_switch_dev(muic_data->sdata);
		disable_irq_wake(muic_data->i2c->irq);
		mutex_destroy(&muic_data->afc_mutex);
		mutex_destroy(&muic_data->bcd_rescan_mutex);
		mutex_destroy(&muic_data->switch_mutex);
		mutex_destroy(&muic_data->muic_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
		unregister_muic_platform_layer(muic_data->sdata);
	}

	return 0;
}

void s2mm006_muic_shutdown(struct s2mm006_muic_data *muic_data)
{
	pr_info("%s\n", __func__);

	if (!muic_data->i2c) {
		pr_err("%s no muic i2c client\n", __func__);
		return;
	}
}

#if IS_ENABLED(CONFIG_PM)
int s2mm006_muic_suspend(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	sdata->suspended = true;

	return 0;
}

int s2mm006_muic_resume(struct s2mm006_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	sdata->suspended = false;

	if (sdata->need_to_noti) {
		if (sdata->attached_dev)
			MUIC_SEND_NOTI_ATTACH(sdata->attached_dev);
		else
			MUIC_SEND_NOTI_DETACH(sdata->attached_dev);

		sdata->need_to_noti = false;
	}

	return 0;
}
#else
#define s2mm006_muic_suspend NULL
#define s2mm006_muic_resume NULL
#endif

