// SPDX-License-Identifier: GPL-2.0
/*
 * driver/../s2mm006_typec.c - S2MM006 USB typec function driver
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

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif
#include <linux/usb/typec.h>
#include <linux/usb/typec/common/pdic_core.h>
#include "s2mm006_ext.h"
#include "s2mm006_typec.h"

#if IS_ENABLED(CONFIG_TYPEC)
static int s2mm006_send_role_swap_message(void *data, S2MM006_SWAP_Type cmd)
{
	struct s2mm006_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = S2MM006_REG_CC_CTRL4;
	u8 W_DATA;
	CC_CTRL4_Type CC_CTRL4;
	int ret = 0;

	if (cmd == PD_DR_SWAP)
		CC_CTRL4.BITS.DrSwap = 1;
	else if (cmd == PD_PR_SWAP)
		CC_CTRL4.BITS.PrSwap = 1;
	else if (cmd == PD_VCONN_SWAP)
		CC_CTRL4.BITS.VcSwap = 1;
	else {
		pr_err("%s, invalid cmd : %d\n", __func__, cmd);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s %s sent\n", __func__, ((cmd == PD_DR_SWAP) ? "DR_SWAP" :
				((cmd == PD_PR_SWAP) ? " PR_SWAP" : "VCONN_SWAP")));

	W_DATA = CC_CTRL4.BYTE;

	ret = s2mm006_write_byte(i2c, REG_ADD, &W_DATA);
	if (ret < 0)
		goto err;
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_set_port_role(struct s2mm006_data *usbpd_data,
	uint32_t mode)
{
	uint16_t reg = S2MM006_REG_CC_CTRL1;
	int ret = 0;
	uint8_t data, read_data;
	CC_CTRL1_Type CC_CTRL1;
	const char * const str_mode[] = {
		"SINK",
		"SOURCE",
		"OPEN(DRP_0uA)",
		"DRP",
		"DRP_TrySnk",
	};

	ret = s2mm006_read_byte(usbpd_data->i2c, reg, &read_data);
	if (ret < 0)
		goto err;
	CC_CTRL1.BYTE = read_data;

	if (mode == S2MM006_PORT_ROLE_SINK)
		CC_CTRL1.BITS.PortRole = S2MM006_PORT_ROLE_SINK;
	else if (mode == S2MM006_PORT_ROLE_SOURCE)
		CC_CTRL1.BITS.PortRole = S2MM006_PORT_ROLE_SOURCE;
	else if (mode == S2MM006_PORT_ROLE_DRP_0uA)
		CC_CTRL1.BITS.PortRole = S2MM006_PORT_ROLE_DRP_0uA;
	else if (mode == S2MM006_PORT_ROLE_DRP)
		CC_CTRL1.BITS.PortRole = S2MM006_PORT_ROLE_DRP;
	else if (mode == S2MM006_PORT_ROLE_DRP_TRY_SNK) {
		CC_CTRL1.BITS.PortRole = S2MM006_PORT_ROLE_DRP;
		CC_CTRL1.BITS.TrySnkEn = 1;
	} else {
		pr_err("%s, invalid mode : %d\n", __func__, mode);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s,  %s\n", __func__, str_mode[mode]);

	data = CC_CTRL1.BYTE;

	ret = s2mm006_write_byte(usbpd_data->i2c, reg, &data);
	if (ret < 0)
		goto err;
	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	return ret;
}

static int s2mm006_rprd_mode_change(struct s2mm006_data *usbpd_data, u8 mode)
{
	int ret = 0;

	pr_info("%s, mode=%s\n", __func__, ((mode == TYPE_C_ATTACH_DFP) ? "DFP" :
		(mode == TYPE_C_ATTACH_UFP) ? "UFP" :
			(mode == TYPE_C_ATTACH_DRP) ? "DRP" : "undefined"));

	switch (mode) {
	case TYPE_C_ATTACH_DFP:
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_DRP_0uA);
		if (ret)
			goto err;
		msleep(1000);
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_SOURCE);
		if (ret)
			goto err;
		msleep(200);
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_DRP);
		if (ret)
			goto err;
		break;
	case TYPE_C_ATTACH_UFP:
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_DRP_0uA);
		if (ret)
			goto err;
		msleep(1000);
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_SINK);
		if (ret)
			goto err;
		msleep(200);
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_DRP);
		if (ret)
			goto err;
		break;
	case TYPE_C_ATTACH_DRP:
		ret = s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_DRP);
		if (ret)
			goto err;
		break;
	default:
		break;
	};

	return 0;
err:
	pr_err("%s error. ret=%d\n", __func__, ret);
	s2mm006_set_port_role(usbpd_data, S2MM006_PORT_ROLE_DRP);
	return ret;
}

int s2mm006_vconn_set(void *data, enum typec_role role)
{
	struct s2mm006_data *usbpd_data = data;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s usbpd_data null\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s\n", __func__);

	ret = s2mm006_send_role_swap_message(usbpd_data, PD_VCONN_SWAP);

err:
	return ret;
}

int s2mm006_dr_set(void *data, enum typec_data_role role)
{
	struct s2mm006_data *usbpd_data = data;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s usbpd_data null\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s\n", __func__);

	ret = s2mm006_send_role_swap_message(usbpd_data, PD_DR_SWAP);

err:
	return ret;
}

int s2mm006_pr_set(void *data, enum typec_role role)
{
	struct s2mm006_data *usbpd_data = data;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s usbpd_data null\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s\n", __func__);

	ret = s2mm006_send_role_swap_message(usbpd_data, PD_PR_SWAP);

err:
	return ret;
}

int s2mm006_port_type_set(void *data, enum typec_port_type port_type)
{
	struct s2mm006_data *usbpd_data = data;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data null\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s\n", __func__);

	switch (port_type) {
	case TYPEC_PORT_SRC:
		ret = s2mm006_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DFP);
		if (ret)
			goto err;
		break;
	case TYPEC_PORT_SNK:
		ret = s2mm006_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_UFP);
		if (ret)
			goto err;
		break;
	case TYPEC_PORT_DRP:
		ret = s2mm006_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
		if (ret)
			goto err;
		break;
	default:
		break;
	}
	return 0;
err:
	pr_err("%s error.ret=%d\n", __func__, ret);
	return ret;
}
#endif

