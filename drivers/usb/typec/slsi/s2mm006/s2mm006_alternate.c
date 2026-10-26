// SPDX-License-Identifier: GPL-2.0
/*
 * S2MM006 USB PDIC Alternate mode driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
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
#define pr_fmt(fmt) "[PDIC][ALTERNATE] " fmt

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_policy.h>
#include <linux/usb/typec/common/pdic_spec_def.h>
#include <linux/usb_notify.h>
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif
#include "s2mm006_ext.h"
#include "s2mm006_alternate.h"

int s2mm006_sbu_control(void *data, int on)
{
	struct s2mm006_data *usbpd_data = data;
	SBU_CTRL1_Type sbu_ctrl1;
	int ret = 0;

	sbu_ctrl1.BYTE = 0;

	pr_info("%s : on : %d\n", __func__, on);

	if (!usbpd_data)
		return -ENODATA;;

	if (usbpd_data->sbu_status == on) {
		pr_info("%s : requested value is already set\n", __func__);
		return 0;
	} else
		usbpd_data->sbu_status = on;

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_SBU_CTRL1, &sbu_ctrl1.BYTE);
	if (ret < 0)
		goto err;

	pr_info("%s: SBU_CTRL1(before): %x\n", __func__, sbu_ctrl1.BYTE);

	switch (on) {
	case OPEN_SBU:
		sbu_ctrl1.BITS.SbuSel = 0;
		sbu_ctrl1.BITS.SbuEn = 0;
		break;
	case CLOSE_SBU_CC1_ACTIVE:
		sbu_ctrl1.BITS.SbuSel = 0;
		sbu_ctrl1.BITS.SbuEn = 1;
		break;
	case CLOSE_SBU_CC2_ACTIVE:
		sbu_ctrl1.BITS.SbuSel = 1;
		sbu_ctrl1.BITS.SbuEn = 1;
		break;
	default:
		break;
	}

	ret = s2mm006_write_byte(usbpd_data->i2c,
			S2MM006_REG_SBU_CTRL1, &sbu_ctrl1.BYTE);
	if (ret < 0)
		goto err;

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_SBU_CTRL1, &sbu_ctrl1.BYTE);
	if (ret < 0)
		goto err;

	pr_info("%s: SBU_CTRL1(after): %x\n", __func__, sbu_ctrl1.BYTE);

	return 0;
err:
	pr_info("%s i2c error\n", __func__);
	return ret;
}

static int s2mm006_alt_control(struct s2mm006_data *usbpd_data,
	int control, int enable)
{
	PD_CTRL1_Type pd_ctrl1;
	int ret = 0;

	pd_ctrl1.BYTE = 0;

	pr_info("%s control=%x enable=%d\n", __func__,
			control, enable);

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_PD_CTRL1, &pd_ctrl1.BYTE);
	if (ret < 0)
		goto err;

	switch (control) {
	case S2MM006_PD_CTRL1_START:
		pd_ctrl1.BITS.StartAlt = !!enable;
		break;
	case S2MM006_PD_CTRL1_CONTINUE:
		pd_ctrl1.BITS.ContinueAlt = !!enable;
		break;
	case S2MM006_PD_CTRL1_EXIT:
		pd_ctrl1.BITS.ExitAlt = !!enable;
		break;
	default:
		break;
	}

	ret = s2mm006_write_byte(usbpd_data->i2c,
			S2MM006_REG_PD_CTRL1, &pd_ctrl1.BYTE);
	if (ret < 0)
		goto err;
	return 0;
err:
	pr_info("%s i2c error\n", __func__);
	return ret;
}

void s2mm006_set_enable_alternate_mode(void *data, int mode)
{
	struct s2mm006_data *usbpd_data = (struct s2mm006_data *)data;
	static int drv_complete;
	static int prev_alt_mode;
	int error = 0;

	pr_info("%s [%s] mode %x\n", __func__, usbpd_data->name, mode);

	if (!usbpd_data) {
		error = __LINE__;
		goto err;
	}

	if (!strcmp(usbpd_data->name, "s2mm006-pogo")) {
		pr_info("%s is pogo port, just return\n", __func__);
		return;
	}

#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	store_usblog_notify(NOTIFY_ALTERNATEMODE, (void *)&mode, NULL);
#endif
	mutex_lock(&usbpd_data->alt_info_lock);
	usbpd_data->set_en_alt_mode = mode;
	mutex_unlock(&usbpd_data->alt_info_lock);

	if ((mode & ALTERNATE_MODE_NOT_READY) &&
	    (mode & ALTERNATE_MODE_READY)) {
		error = __LINE__;
		goto err;
	}
	if ((mode & ALTERNATE_MODE_START) && (mode & ALTERNATE_MODE_STOP)) {
		error = __LINE__;
		goto err;
	}

	if ((mode & ALTERNATE_MODE_STOP)) {
		s2mm006_alt_control(usbpd_data, S2MM006_PD_CTRL1_START, 0);
		pr_info("%s. stop alternate mode!\n", __func__);
	}

	if (mode & ALTERNATE_MODE_RESET) {
		pr_info("%s.mode is reset! drv_complete=%d, prev_alt_mode=%d",
			__func__, drv_complete, prev_alt_mode);
		if (drv_complete &&
		    (prev_alt_mode == ALTERNATE_MODE_START)) {
			prev_alt_mode = ALTERNATE_MODE_START;
			s2mm006_alt_control(usbpd_data, S2MM006_PD_CTRL1_START, 1);
		} else if (drv_complete &&
			   (prev_alt_mode == ALTERNATE_MODE_STOP)) {
			prev_alt_mode = ALTERNATE_MODE_STOP;
			s2mm006_alt_control(usbpd_data, S2MM006_PD_CTRL1_START, 0);
		} else {
			;
		}
	} else {
		if (mode & ALTERNATE_MODE_NOT_READY) {
			drv_complete = 0;
			pr_info("%s.alternate mode is not ready!\n", __func__);
		} else if (mode & ALTERNATE_MODE_READY) {
			drv_complete = 1;
			pr_info("%s.alternate mode is ready!\n", __func__);
		} else {
			;
		}

		if (drv_complete) {
			if (mode & ALTERNATE_MODE_START) {
				s2mm006_alt_control(usbpd_data, S2MM006_PD_CTRL1_START, 1);
			} else if (mode & ALTERNATE_MODE_STOP) {
				s2mm006_alt_control(usbpd_data, S2MM006_PD_CTRL1_START, 0);
			} else {
				error = __LINE__;
				goto err;
			}
		}
	}
	return;
err:
	pr_info("%s. error line=%d\n", __func__, error);
}

static const char * const vdm_command_type[] = {
	[S_TYPE_REQ] = "REQ",
	[S_TYPE_ACK] = "ACK",
	[S_TYPE_NAK] = "NAK",
	[S_TYPE_BUSY] = "BUSY",
};

static const char * const pd_ctrl2_dp_pin[] = {
	[S2MM006_PD_CTRL2_NO_SELETION] = "NO_SELECTION",
	[S2MM006_PD_CTRL2_PIN_A] = "PIN A",
	[S2MM006_PD_CTRL2_PIN_B] = "PIN B",
	[S2MM006_PD_CTRL2_PIN_C] = "PIN C",
	[S2MM006_PD_CTRL2_PIN_D] = "PIN D",
	[S2MM006_PD_CTRL2_PIN_E] = "PIN E",
	[S2MM006_PD_CTRL2_PIN_F] = "PIN F",
};

static const char *svid_strings(u16 svid)
{
	switch (svid) {
	case PD_SID:
		return "PD SID";
	case DP_SVID:
		return "DP SVID";
	case SAMSUNG_SVID:
		return "SAMSUNG SVID";
	default:
		return "";
	}
}

static int s2mm006_discover_id(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	DISCOVER_IDENTITY_RESPONSE	response;
	int ret = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_DIS_ID, (u8 *)&response,
			sizeof(DISCOVER_IDENTITY_RESPONSE));
	if (ret < 0)
		goto err;

	if (response.vdm_header.BITS.VDM_command != S_DISCOVER_IDENTIFY) {
		ret = -EPROTO;
		goto err;
	}

	mutex_lock(&usbpd_data->alt_info_lock);
	usbpd_data->alt_info.vendor_id = response.id_header.BITS.USB_Vendor_ID;
	usbpd_data->alt_info.product_id = response.product.BITS.Product_ID;
	usbpd_data->alt_info.bcd_device = response.product.BITS.Device_Version;

	pr_info("%s vendor id=0x%02x product id=0x%02x bcd device==0x%02x\n",
		__func__, usbpd_data->alt_info.vendor_id,
		usbpd_data->alt_info.product_id,
		usbpd_data->alt_info.bcd_device);
	mutex_unlock(&usbpd_data->alt_info_lock);

	pdic_policy_send_msg(usbpd_data->pp_data, MSG_DCOVER, 0, 0);
	return ret;
err:
	pr_err("%s error ret=%d\n", __func__, ret);
	return ret;
}

static void s2mm006_print_svid(struct s2mm006_data *usbpd_data)
{
	int i = 0;

	mutex_lock(&usbpd_data->alt_info_lock);
	for (i = 0; i < 12; i++) {
		if (usbpd_data->alt_info.svid[i] == 0)
			break;
		pr_info("%s 0x%02x\n", __func__, usbpd_data->alt_info.svid[i]);
	}
	mutex_unlock(&usbpd_data->alt_info_lock);
}

static int s2mm006_is_dp_svid(struct s2mm006_data *usbpd_data)
{
	int i = 0, ret = 0;

	mutex_lock(&usbpd_data->alt_info_lock);
	for (i = 0; i < 12; i++) {
		if (usbpd_data->alt_info.svid[i] == DP_SVID) {
			ret = 1;
			break;
		}
	}
	mutex_unlock(&usbpd_data->alt_info_lock);

	return ret;
}

static int s2mm006_discover_svid(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	DISCOVER_SVID_RESPONSE response;
	int ret = 0, i = 0, j = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_DIS_SVID, (u8 *)&response,
			sizeof(DISCOVER_SVID_RESPONSE));
	if (ret < 0)
		goto err;

	if (response.vdm_header.BITS.VDM_command != S_DISCOVER_SVIDS) {
		ret = -EPROTO;
		goto err;
	}

	mutex_lock(&usbpd_data->alt_info_lock);
	for (i = 0, j = 0; i < S2MM006_SVID_VDO_MAX; i++, j += 2) {
		usbpd_data->alt_info.svid[j] = response.svid_vdo[i].BITS.SVID_0;
		usbpd_data->alt_info.svid[j+1] = response.svid_vdo[i].BITS.SVID_1;
	}
	mutex_unlock(&usbpd_data->alt_info_lock);

	s2mm006_print_svid(usbpd_data);

	if (s2mm006_is_dp_svid(usbpd_data)) {
		pr_info("%s dp detected\n", __func__);
		mutex_lock(&usbpd_data->alt_info_lock);
		usbpd_data->alt_info.dp_device = 1;
		mutex_unlock(&usbpd_data->alt_info_lock);
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_DP_CONN, 0, 0);
	}

	return ret;
err:
	pr_err("%s error ret=%d\n", __func__, ret);
	return ret;

}

static int s2mm006_discover_mode(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	DP_DISCOVER_MODE_RESPONSE response;
	u16 svid = 0;
	int ret = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_DIS_MODE, (u8 *)&response,
			sizeof(DP_DISCOVER_MODE_RESPONSE));
	if (ret < 0)
		goto err;

	if (response.vdm_header.BITS.VDM_command != S_DISCOVER_MODES) {
		ret = -EPROTO;
		goto err;
	}

	svid = response.vdm_header.BITS.Standard_Vendor_ID;

	pr_info("%s svid=0x%02x(%s)\n", __func__, svid, svid_strings(svid));

	if (svid == DP_SVID) {
		if (response.msg_header.BITS.Number_of_obj > 1) {
			if (response.dp_mode_vdo.BITS.Port_Capability == S_DFP_D_CAPABLE) {
				ret = -EPROTO;
				goto err;
			}

			mutex_lock(&usbpd_data->alt_info_lock);
			if (response.dp_mode_vdo.BITS.Receptacle_Indication
					== S_USB_TYPE_C_RECEPTACLE)
				usbpd_data->alt_info.dp_pin_assignment
					= response.dp_mode_vdo.BITS.UFP_D_Pin_Assignments;
			else
				usbpd_data->alt_info.dp_pin_assignment
					= response.dp_mode_vdo.BITS.DFP_D_Pin_Assignments;
			mutex_unlock(&usbpd_data->alt_info_lock);
		} else {
			ret = -ECOMM;
			goto err;
		}

		s2mm006_alt_control(usbpd_data, S2MM006_PD_CTRL1_CONTINUE, 1);
	}

	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_enter_mode(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	ENTER_MODE_RESPONSE response;
	u16 svid = 0;
	u8 cmd_type = 0;
	int ret = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_ENTER_MODE,
			(u8 *)&response, sizeof(ENTER_MODE_RESPONSE));
	if (ret < 0)
		goto err;

	if (response.vdm_header.BITS.VDM_command != S_ENTER_MODE) {
		ret = -EPROTO;
		goto err;
	}

	svid = response.vdm_header.BITS.Standard_Vendor_ID;
	cmd_type = response.vdm_header.BITS.VDM_command_type;

	pr_info("%s svid=0x%02x cmd_type=%s\n", __func__, svid,
			vdm_command_type[cmd_type]);
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_exit_mode(struct s2mm006_data *usbpd_data)
{
	return 0;
}

static u8 s2mm006_choose_pin_assignment(struct s2mm006_data *usbpd_data,
		int multi_func)
{
	u8 choose_pin = 0;
	int dp_pin_assignment = 0;

	mutex_lock(&usbpd_data->alt_info_lock);
	dp_pin_assignment = usbpd_data->alt_info.dp_pin_assignment;
	mutex_unlock(&usbpd_data->alt_info_lock);

	if (multi_func) {
		if (dp_pin_assignment & DP_PIN_ASSIGNMENT_D)
			choose_pin = S2MM006_PD_CTRL2_PIN_D;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_B)
			choose_pin = S2MM006_PD_CTRL2_PIN_B;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_F)
			choose_pin = S2MM006_PD_CTRL2_PIN_F;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_C)
			choose_pin = S2MM006_PD_CTRL2_PIN_C;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_E)
			choose_pin = S2MM006_PD_CTRL2_PIN_E;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_A)
			choose_pin = S2MM006_PD_CTRL2_PIN_A;
		else
			choose_pin = S2MM006_PD_CTRL2_NO_SELETION;
	} else {
		if (dp_pin_assignment & DP_PIN_ASSIGNMENT_C)
			choose_pin = S2MM006_PD_CTRL2_PIN_C;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_E)
			choose_pin = S2MM006_PD_CTRL2_PIN_E;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_A)
			choose_pin = S2MM006_PD_CTRL2_PIN_A;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_D)
			choose_pin = S2MM006_PD_CTRL2_PIN_D;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_B)
			choose_pin = S2MM006_PD_CTRL2_PIN_B;
		else if (dp_pin_assignment & DP_PIN_ASSIGNMENT_F)
			choose_pin = S2MM006_PD_CTRL2_PIN_F;
		else
			choose_pin = S2MM006_PD_CTRL2_NO_SELETION;
	}

	pr_info("%s choose_pin = %d\n", __func__, choose_pin);

	if (choose_pin == S2MM006_PD_CTRL2_PIN_C ||
			choose_pin == S2MM006_PD_CTRL2_PIN_E ||
			choose_pin == S2MM006_PD_CTRL2_PIN_A)
		dwc3_restart_usb_host_mode(4);
	else
		dwc3_restart_usb_host_mode(2);

	return choose_pin;
}

static int s2mm006_change_notify_pin(u8 choose_pin)
{
	int ret = 0;

	switch (choose_pin) {
	case S2MM006_PD_CTRL2_PIN_A:
		ret = PDIC_NOTIFY_DP_PIN_A;
		break;
	case S2MM006_PD_CTRL2_PIN_B:
		ret = PDIC_NOTIFY_DP_PIN_B;
		break;
	case S2MM006_PD_CTRL2_PIN_C:
		ret = PDIC_NOTIFY_DP_PIN_C;
		break;
	case S2MM006_PD_CTRL2_PIN_D:
		ret = PDIC_NOTIFY_DP_PIN_D;
		break;
	case S2MM006_PD_CTRL2_PIN_E:
		ret = PDIC_NOTIFY_DP_PIN_E;
		break;
	case S2MM006_PD_CTRL2_PIN_F:
		ret = PDIC_NOTIFY_DP_PIN_F;
		break;
	default:
		ret = PDIC_NOTIFY_DP_PIN_UNKNOWN;
		break;
	}

	return ret;
}

static int s2mm006_check_ufpu_status(struct s2mm006_data *usbpd_data,
		u8 multi_func, u8 hpd_state, u8 hpd_irq)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	PD_CTRL2_Type command;
	int ret = 0;
	u8 choose_pin = 0;

	ret = s2mm006_read_byte(i2c, S2MM006_REG_PD_CTRL2, &command.BYTE);
	if (ret < 0)
		goto err;

	choose_pin = s2mm006_choose_pin_assignment(usbpd_data, multi_func);
	if (choose_pin == S2MM006_PD_CTRL2_NO_SELETION) {
		ret = -ESRCH;
		goto err;
	}

	mutex_lock(&usbpd_data->alt_info_lock);
	if (usbpd_data->alt_info.dp_selected_pin == s2mm006_change_notify_pin(choose_pin)) {
		pr_info("%s already setting pin=%s\n", __func__,
				pd_ctrl2_dp_pin[choose_pin]);
		goto skip;
	}
	command.BITS.Select_dp_pin = choose_pin;

	ret = s2mm006_write_byte(i2c, S2MM006_REG_PD_CTRL2, &command.BYTE);
	if (ret < 0) {
		mutex_unlock(&usbpd_data->alt_info_lock);
		goto err;
	}

	usbpd_data->alt_info.dp_selected_pin
			= s2mm006_change_notify_pin(choose_pin);
skip:
	usbpd_data->alt_info.hpd_state = 0;
	usbpd_data->alt_info.hpd_irq = 0;

	if (hpd_state == 1)
		usbpd_data->alt_info.hpd_state = PDIC_NOTIFY_HIGH;
	else
		usbpd_data->alt_info.hpd_state = PDIC_NOTIFY_LOW;

	if (hpd_irq == 1)
		usbpd_data->alt_info.hpd_irq = PDIC_NOTIFY_IRQ;

	pr_info("%s multi func=%d sel_dp_pin=%s hpd_state=%d hpd_irq=%d\n",
			__func__,
			multi_func,
			pd_ctrl2_dp_pin[choose_pin],
			usbpd_data->alt_info.hpd_state,
			usbpd_data->alt_info.hpd_irq);
	mutex_unlock(&usbpd_data->alt_info_lock);

	pdic_policy_send_msg(usbpd_data->pp_data, MSG_DP_HPD, 0, 0);
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_dp_status(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	DP_STATUS_RESPONSE response;
	int ret = 0;
	u8 multi_func = 0, hpd_state = 0, hpd_irq = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_DP_STATUS_UPDATE,
			(u8 *)&response, sizeof(DP_STATUS_RESPONSE));
	if (ret < 0)
		goto err;

	if (response.vdm_header.BITS.VDM_command != S_DISPLAYPORT_STATUS_UPDATE) {
		ret = -EPROTO;
		goto err;
	}

	pr_info("%s\n", __func__);

	multi_func = response.dp_status_update_vdo.BITS.Multi_Function_Preference;
	hpd_state = response.dp_status_update_vdo.BITS.HPD_State;
	hpd_irq = response.dp_status_update_vdo.BITS.HPD_Interrupt;

	ret = s2mm006_check_ufpu_status(usbpd_data, multi_func, hpd_state, hpd_irq);
	if (ret < 0)
		goto err;
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_dp_configure(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	DP_CONFIGURATION_RESPONSE response;
	u8 cmd_type = 0;
	int ret = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_DP_CONFIGURE,
			(u8 *)&response, sizeof(DP_CONFIGURATION_RESPONSE));
	if (ret < 0)
		goto err;

	if (response.vdm_header.BITS.VDM_command != S_DISPLAYPORT_CONFIGURE) {
		ret = -EPROTO;
		goto err;
	}

	cmd_type = response.vdm_header.BITS.VDM_command_type;

	pr_info("%s cmd_type=%s\n", __func__,
			vdm_command_type[cmd_type]);

	pdic_policy_send_msg(usbpd_data->pp_data, MSG_DP_LINK_CONF, 0, 0);
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_dp_attention(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	DP_ATTENTION_REQUEST request;
	int ret = 0;
	u16 svid = 0;
	u8 multi_func = 0, hpd_state = 0, hpd_irq = 0;

	ret = s2mm006_read_bytes(i2c, S2MM006_RX_ATTENTION,
			(u8 *)&request, sizeof(DP_ATTENTION_REQUEST));
	if (ret < 0)
		goto err;

	if (request.vdm_header.BITS.VDM_command != S_ATTENTION) {
		ret = -EPROTO;
		goto err;
	}

	svid = request.vdm_header.BITS.Standard_Vendor_ID;

	pr_info("%s svid=0x%02x(%s)\n", __func__, svid, svid_strings(svid));

	if (request.vdm_header.BITS.Standard_Vendor_ID != DP_SVID) {
		ret = -EPROTONOSUPPORT;
		goto err;
	}

	multi_func = request.dp_status_update_vdo.BITS.Multi_Function_Preference;
	hpd_state = request.dp_status_update_vdo.BITS.HPD_State;
	hpd_irq = request.dp_status_update_vdo.BITS.HPD_Interrupt;

	ret = s2mm006_check_ufpu_status(usbpd_data, multi_func, hpd_state, hpd_irq);
	if (ret < 0)
		goto err;
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

void s2mm006_int_altmode(struct s2mm006_data *usbpd_data)
{
	PD_STATUS3_Type PD_STATUS3;
	uint8_t state = 0;
	int ret = 0;

	ret = s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PD_STATUS3,
		&PD_STATUS3.BYTE);
	if (ret < 0)
		goto err;

	state = PD_STATUS3.BITS.AlternateMode;

	PD_STATUS3.BITS.AlternateMode = 0;

	ret = s2mm006_write_byte(usbpd_data->i2c, S2MM006_REG_PD_STATUS3,
		&PD_STATUS3.BYTE);
	if (ret < 0)
		goto err;

	pr_info("%s state=0x%02x\n", __func__, state);

	if (state & S2MM006_PD_STATUS_DISCOVER_ID) {
		pr_info("%s DISCOVER IDENTITY\n", __func__);
		ret = s2mm006_discover_id(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_DISCOVER_SVID) {
		pr_info("%s DISCOVER SVID\n", __func__);
		ret = s2mm006_discover_svid(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_DISCOVER_MODE) {
		pr_info("%s DISCOVER MODE\n", __func__);
		ret = s2mm006_discover_mode(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_ENTER_MODE) {
		pr_info("%s ENTER MODE\n", __func__);
		ret = s2mm006_enter_mode(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_EXIT_MODE) {
		pr_info("%s EXIT MODE\n", __func__);
		ret = s2mm006_exit_mode(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_DP_STATUS) {
		pr_info("%s DP STATUS\n", __func__);
		ret = s2mm006_dp_status(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_DP_CONFIGURE) {
		pr_info("%s DP CONFIGURE\n", __func__);
		ret = s2mm006_dp_configure(usbpd_data);
		if (ret < 0)
			goto err;
	}
	if (state & S2MM006_PD_STATUS_DP_ATTENTION) {
		pr_info("%s DP ATTENTION\n", __func__);
		ret = s2mm006_dp_attention(usbpd_data);
		if (ret < 0)
			goto err;
	}

	return;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
}

int s2mm006_get_alt_info(void *data, struct pdic_alt_info *alt_info)
{
	struct s2mm006_data *usbpd_data = data;

	mutex_lock(&usbpd_data->alt_info_lock);
	memcpy(alt_info, &usbpd_data->alt_info, sizeof(struct pdic_alt_info));
	mutex_unlock(&usbpd_data->alt_info_lock);

	return 0;
}

void s2mm006_alt_info_clear(void *data)
{
	struct s2mm006_data *usbpd_data = data;

	pr_info("%s +\n", __func__);

	mutex_lock(&usbpd_data->alt_info_lock);
	memset(&usbpd_data->alt_info, 0, sizeof(struct pdic_alt_info));
	mutex_unlock(&usbpd_data->alt_info_lock);

	pr_info("%s -\n", __func__);
}

void s2mm006_dp_info_clear(void *data)
{
	struct s2mm006_data *usbpd_data = data;

	pr_info("%s +\n", __func__);

	mutex_lock(&usbpd_data->alt_info_lock);
	usbpd_data->alt_info.dp_device = 0;
	usbpd_data->alt_info.dp_pin_assignment = 0;
	usbpd_data->alt_info.dp_selected_pin = 0;
	usbpd_data->alt_info.hpd_state = 0;
	usbpd_data->alt_info.hpd_irq = 0;
	mutex_unlock(&usbpd_data->alt_info_lock);

	pr_info("%s -\n", __func__);
}

void s2mm006_alternate_init(struct s2mm006_data *usbpd_data)
{
	mutex_init(&usbpd_data->alt_info_lock);
}

void s2mm006_alternate_deinit(struct s2mm006_data *usbpd_data)
{
	mutex_destroy(&usbpd_data->alt_info_lock);
}

