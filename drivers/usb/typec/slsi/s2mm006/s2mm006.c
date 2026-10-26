// SPDX-License-Identifier: GPL-2.0
/*
 * driver/../s2mm006.c - S2MM006 USBPD device driver
 *
 * Copyright (C) 2015 Samsung Electronics
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
#include <linux/usb/typec/common/pdic_param.h>
#include <linux/usb_notify.h>
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#include <linux/sti/abc_common.h>
#endif
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif
#include <linux/battery/sec_pd.h>

#include "s2mm006.h"
#include "s2mm006_ext.h"
#include "s2mm006_muic.h"
#include "s2mm006_alternate.h"

#define S2MM006_I2C_RETRY	5
#define S2MM006_FIRMWARE_BIN_DEFAULT_NAME	"s2mm006_fw.bin"

#if IS_ENABLED(CONFIG_S2MM006_POGO)
int s2mm006_is_pogo(void *_data);
#if IS_ENABLED(CONFIG_SEC_PD)
struct pdic_notifier_struct* s2mm006_get_pd_noti(void *_data);
#endif
#endif

const struct pdic_ops s2mm006_ops = {
	.dr_set			= s2mm006_dr_set,
	.pr_set			= s2mm006_pr_set,
	.vconn_set		= s2mm006_vconn_set,
	.port_type_set	= s2mm006_port_type_set,
	.get_alt_info   = s2mm006_get_alt_info,
	.set_alt_mode = s2mm006_set_enable_alternate_mode,
	.dp_info_clear = s2mm006_dp_info_clear,
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	.is_pogo = s2mm006_is_pogo,
#if IS_ENABLED(CONFIG_SEC_PD)
	.get_pd_noti = s2mm006_get_pd_noti,
#endif
#endif
	.sbu_switch_control = s2mm006_sbu_control,
};

static const char *const usbpd_irq_str[] = {
	"BC_INT", "CC_INT", "PD_INT", "FAC_INT", "PRT_INT", "PD_INT2"
};

static const char *const usbpd_is_sink_str[] = {
	"TYPEC", "POGO"
};

#if IS_ENABLED(CONFIG_S2MM006_POGO)
static enum power_supply_property pdic_props[] = {
        POWER_SUPPLY_PROP_ONLINE,
};

static char *pdic_supplied_to[] = {
	"battery",
};
#endif

#if IS_ENABLED(CONFIG_S2MM006_POGO) && !IS_ENABLED(CONFIG_SEC_FACTORY)
static const char * const s2mm006_path[] = {
	[S2MM006_PATH_OPEN]	= "MANUAL OPEN",
	[S2MM006_PATH_USB]	= "USB",
	[S2MM006_PATH_UART]	= "UART",
	[S2MM006_PATH_JTAG]	= "JTAG",
	[S2MM006_PATH_HICCUP]	= "HICCUP",
	[S2MM006_PATH_AUTO]	= "AUTO",
};
#endif

void *get_s2mm006_usbpd_data(void)
{
	struct device_node *np = NULL;
	struct i2c_client *client = NULL;
	void *data = NULL;

	np = of_find_compatible_node(NULL, NULL, "slsi,s2mm006");
	if (np == NULL) {
		pr_err("%s of_find_compatible_node err\n", __func__);
		goto err;
	}

	client = of_find_i2c_device_by_node(np);
	if (client == NULL) {
		pr_err("%s of_find_i2c_device_by_node err\n", __func__);
		of_node_put(np);
		goto err;
	}

	data = i2c_get_clientdata(client);

	of_node_put(np);
err:
	return data;
}

static void s2mm006_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static void s2mm006_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static void s2mm006_wake_lock_timeout(struct wakeup_source *ws,
	unsigned int ms)
{
	__pm_wakeup_event(ws, ms);
}

static int s2mm006_set_wake_lock(struct s2mm006_data *usbpd_data)
{
	struct wakeup_source *pdic_ws = NULL;
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (s2mm006_is_pogo(usbpd_data))
		pdic_ws = wakeup_source_register(NULL, "s2mm006_pdic_wake");
	else
#endif
		pdic_ws = wakeup_source_register(NULL, "s2mm006_pdic_typec_wake");

	if (pdic_ws == NULL) {
		pr_info("%s, Fail to register ws\n", __func__);
		goto err;
	}

	usbpd_data->pdic_ws = pdic_ws;

	s2mm006_wake_lock(usbpd_data->pdic_ws);
	s2mm006_wake_unlock(usbpd_data->pdic_ws);

	return 0;
err:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
// s2mm006 unit functions
////////////////////////////////////////////////////////////////////////////////
int s2mm006_read_bytes(const struct i2c_client *i2c, u16 reg, u8 *val, u16 size)
{
	int ret = 0, i2c_retry = 0;
	u8 wbuf[2];
	struct i2c_msg msg[2];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&usbpd_data->i2c_mutex);
	msg[0].addr = i2c->addr;
	msg[0].flags = i2c->flags;
	msg[0].len = 2;
	msg[0].buf = wbuf;
	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = val;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	do {
		ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
	} while (ret < 0 &&  i2c_retry++ < S2MM006_I2C_RETRY);

	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		pr_err("i2c read16 fail reg:0x%x error %d\n", reg, ret);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=pdic@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=pdic@WARN=i2c_fail");
#endif
#endif
	}
	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}

int s2mm006_read_byte_flash(const struct i2c_client *i2c, u16 reg, u8 *val, u16 size)
{
	int ret = 0, i2c_retry = 0;
	u8 wbuf[2];
	struct i2c_msg msg[2];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	u8 W_DATA[1];

	udelay(20);
	W_DATA[0] = 0xAA;
	ret = s2mm006_write_byte(i2c, 0x10, &W_DATA[0]);
	if (ret < 0) {
		pr_err("%s. i2c error\n", __func__);
		goto err;
	}
	udelay(20);

	mutex_lock(&usbpd_data->i2c_mutex);
	msg[0].addr = i2c->addr;
	msg[0].flags = i2c->flags;
	msg[0].len = 2;
	msg[0].buf = wbuf;
	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = val;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	do {
		ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
	} while (ret < 0 &&  i2c_retry++ < S2MM006_I2C_RETRY);

	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		pr_err("i2c read16 fail reg:0x%x error %d\n", reg, ret);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=pdic@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=pdic@WARN=i2c_fail");
#endif
#endif
	}
	mutex_unlock(&usbpd_data->i2c_mutex);
err:

#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=pdic@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=pdic@WARN=i2c_fail");
#endif
#endif
	return ret;
}

int s2mm006_write_bytes(const struct i2c_client *i2c, u16 reg, u8 *val, u16 size)
{
	int ret = 0, i2c_retry = 0;
	u8 buf[258] = {0,};
	struct i2c_msg msg[1];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (size > 256) {
		pr_err("I2C error, over the size %d", size);
		return -EIO;
	}

	mutex_lock(&usbpd_data->i2c_mutex);
	msg[0].addr = i2c->addr;
	msg[0].flags = 0;
	msg[0].len = size+2;
	msg[0].buf = buf;

	buf[0] = (reg & 0xFF00) >> 8;
	buf[1] = (reg & 0xFF);
	memcpy(&buf[2], val, size);

	do {
		ret = i2c_transfer(i2c->adapter, msg, 1);
	} while (ret < 0 &&  i2c_retry++ < S2MM006_I2C_RETRY);

	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		pr_err("i2c write fail reg:0x%x error %d\n", reg, ret);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=pdic@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=pdic@WARN=i2c_fail");
#endif
#endif
	}
	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}

int s2mm006_read_byte(const struct i2c_client *i2c, u16 reg, u8 *val)
{
	int ret = 0, i2c_retry = 0;
	u8 wbuf[2], rbuf;
	struct i2c_msg msg[2];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&usbpd_data->i2c_mutex);
	msg[0].addr = i2c->addr;
	msg[0].flags = i2c->flags;
	msg[0].len = 2;
	msg[0].buf = wbuf;
	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &rbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	do {
		ret = i2c_transfer(i2c->adapter, msg, 2);
	} while (ret < 0 &&  i2c_retry++ < S2MM006_I2C_RETRY);

	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		pr_err("i2c read16 fail reg(0x%x), error %d\n", reg, ret);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=pdic@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=pdic@WARN=i2c_fail");
#endif
#endif
	}
	mutex_unlock(&usbpd_data->i2c_mutex);

	*val = rbuf;
	return ret;
}

int s2mm006_write_byte(const struct i2c_client *i2c, u16 reg, u8 *val)
{
	int ret = 0, i2c_retry = 0;
	u8 wbuf[3];
	struct i2c_msg msg[1];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&usbpd_data->i2c_mutex);
	msg[0].addr = i2c->addr;
	msg[0].flags = 0;
	msg[0].len = 3;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);
	wbuf[2] = (*val & 0xFF);

	do {
		ret = i2c_transfer(i2c->adapter, msg, 1);
	} while (ret < 0 &&  i2c_retry++ < S2MM006_I2C_RETRY);

	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		pr_err("i2c write fail reg(0x%x:%x), error %d\n", reg, *val, ret);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_ABC_IFPMIC_EVENT)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=pdic@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=pdic@WARN=i2c_fail");
#endif
#endif
	}
	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}

void s2mm006_reset(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	FW_UPDATE_CTRL1_Type read_data;
	uint16_t reg = S2MM006_REG_DEVICE_FW_ID;
	u8 r_data = 0;
	int i, ret = 0;

	pr_info("%s +\n", __func__);

	/* for Wake up*/
	for (i = 0; i < 5; i++)
		s2mm006_read_byte(i2c, reg, &r_data);

	ret = s2mm006_read_byte(i2c, S2MM006_REG_FW_UPDATE_CTRL1, &read_data.BYTE);
	if (ret < 0)
		goto err;

	read_data.BITS.SoftReset = 1;
	udelay(10);

	ret = s2mm006_write_byte(i2c, S2MM006_REG_FW_UPDATE_CTRL1, &read_data.BYTE);
	if (ret < 0)
		goto err;

	msleep(100);
	pr_info("%s -\n", __func__);
	return;
err:
	pr_err("%s. i2c error -\n", __func__);
}

void s2mm006_set_factory_ctrl(struct s2mm006_data *usbpd_data,
		int usbid, int sleep)
{
	FACTORY_CTRL_Type factory_ctrl;
	uint8_t id = S2MM006_FACTORY_OPEN;
	int ret = 0;

	pr_info("%s usbid=%d, sleep=%d\n", __func__, usbid, sleep);

	ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_FACTORY_CTRL, &factory_ctrl.BYTE);
	if (ret < 0)
		goto err;

	id = factory_ctrl.BITS.Fcy_Id;

	switch (usbid) {
	case S2MM006_FACTORY_255K:
		id = S2MM006_RID_255K;
		factory_ctrl.BITS.LeakageTest = 1;
		break;
	case S2MM006_FACTORY_301K:
		id = S2MM006_RID_301K;
		factory_ctrl.BITS.LeakageTest = 0;
		break;
	case S2MM006_FACTORY_523K:
		id = S2MM006_RID_523K;
		factory_ctrl.BITS.LeakageTest = 0;
		break;
	case S2MM006_FACTORY_619K:
		id = S2MM006_RID_619K;
		factory_ctrl.BITS.LeakageTest = 0;
		break;
	case S2MM006_FACTORY_NOCON:
		break;
	case S2MM006_FACTORY_OPEN:
	default:
		id = S2MM006_RID_OPEN_0;
		factory_ctrl.BITS.LeakageTest = 0;
		break;
	}

	factory_ctrl.BITS.Fcy_Id = id;
	usbpd_data->usb_boot_mode = id;

	factory_ctrl.BITS.SleepTest = !!sleep;

	ret = s2mm006_write_byte(usbpd_data->i2c, S2MM006_REG_FACTORY_CTRL,
			&factory_ctrl.BYTE);
	if (ret < 0)
		goto err;

	return;
err:
	pr_err("%s. i2c error\n", __func__);
}

void s2mm006_reconnect(struct s2mm006_data *usbpd_data)
{
	/* need check */
}

void s2mm006_manual_LPM(struct s2mm006_data *usbpd_data, int cmd)
{
	/* need force to LPM */
}
void s2mm006_cc_open(struct s2mm006_data *usbpd_data, uint32_t mode)
{
	uint16_t reg = S2MM006_REG_CC_CTRL2;
	uint8_t data;
	CC_CTRL2_Type CC_CTRL2;

	CC_CTRL2.BYTE = 0;

	if (mode == 1)
		CC_CTRL2.BITS.CcOpen = 1;
	else if (mode == 0)
		CC_CTRL2.BITS.CcOpen = 0;
	else
		pr_info("%s, invalid mode : %d\n", __func__, mode);

	data = CC_CTRL2.BYTE;

	s2mm006_write_byte(usbpd_data->i2c, reg, &data);
	pr_info("%s, set %s\n", __func__, (mode == CC_OPEN) ?
		"cc_open" : "cc_not_open");
}

static int register_gpio_pins(struct s2mm006_data *usbpd_data)
{
	int ret = 0;

	if (gpio_is_valid(usbpd_data->irq_gpio)) {
		ret = gpio_request(usbpd_data->irq_gpio, "s2mm006_irq");
		pr_info("%s:irq gpio value %d\n", __func__, gpio_get_value(usbpd_data->irq_gpio));
		if (ret) {
			pr_err("s2mm006_irq gpio request error %d\n", ret);
			goto err1;
		}
		gpio_direction_input(usbpd_data->irq_gpio);
	}

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (gpio_is_valid(usbpd_data->irq_vbus_gpio)) {
		ret = gpio_request(usbpd_data->irq_vbus_gpio, "s2mm006_vbus_irq");
		pr_info("%s:vbus_irq gpio value %d\n", __func__, gpio_get_value(usbpd_data->irq_vbus_gpio));
		if (ret) {
			pr_err("s2mm006_irq gpio request error %d\n", ret);
			goto err_pogo;
		}
		gpio_direction_input(usbpd_data->irq_vbus_gpio);
	}
#endif

	if (gpio_is_valid(usbpd_data->redriver_en)) {
		ret = gpio_request(usbpd_data->redriver_en, "s2mm006_redriver_en");
		if (ret) {
			pr_err("s2mm006_redriver_en gpio request error %d\n", ret);
			goto err2;
		}
		gpio_direction_output(usbpd_data->redriver_en, 0);
	}

	if (gpio_is_valid(usbpd_data->s2mm006_om)) {
		ret = gpio_request(usbpd_data->s2mm006_om, "s2mm006_om");
		if (ret) {
			pr_err("s2mm006_om gpio request error %d\n", ret);
			goto err3;
		}
		gpio_direction_output(usbpd_data->s2mm006_om, 0);
	}

	if (gpio_is_valid(usbpd_data->vbus_discharging)) {
		ret = gpio_request(usbpd_data->vbus_discharging, "vbus_discharging");
		if (ret) {
			pr_err("vbus_discharging gpio request error %d\n", ret);
			goto err4;
		}
		gpio_direction_output(usbpd_data->vbus_discharging, 1);
	}

	if (!s2mm006_is_pogo(usbpd_data)) {
		if (gpio_is_valid(usbpd_data->otg_sw)) {
			ret = gpio_request(usbpd_data->otg_sw, "otg_sw");
			if (ret) {
				pr_err("otg_sw gpio request error %d\n", ret);
				goto err4;
			}
			gpio_direction_output(usbpd_data->otg_sw, 0);
			pr_info("%s: otg_sw gpio value %d (%d)\n", __func__,
					gpio_get_value(usbpd_data->otg_sw), __LINE__);
		}

		if (gpio_is_valid(usbpd_data->otg_buck_en)) {
			ret = gpio_request(usbpd_data->otg_buck_en, "otg_buck_en");
			if (ret) {
				pr_err("otg_buck_en gpio request error %d\n", ret);
				goto err4;
			}
			gpio_direction_output(usbpd_data->otg_buck_en, 0);
			pr_info("%s: otg_buck_en gpio value %d (%d)\n", __func__,
					gpio_get_value(usbpd_data->otg_buck_en), __LINE__);
		}
	}

	return 0;
err4:
	if (gpio_is_valid(usbpd_data->s2mm006_om))
		gpio_free(usbpd_data->s2mm006_om);
err3:
	if (gpio_is_valid(usbpd_data->redriver_en))
		gpio_free(usbpd_data->redriver_en);
err2:
	if (gpio_is_valid(usbpd_data->irq_gpio))
		gpio_free(usbpd_data->irq_gpio);
#if IS_ENABLED(CONFIG_S2MM006_POGO)
err_pogo:
	if (gpio_is_valid(usbpd_data->irq_vbus_gpio))
		gpio_free(usbpd_data->irq_vbus_gpio);
#endif
err1:
	return ret;
}

static void unregister_gpio_pins(struct s2mm006_data *usbpd_data)
{
	if (gpio_is_valid(usbpd_data->vbus_discharging))
		gpio_free(usbpd_data->vbus_discharging);
	if (gpio_is_valid(usbpd_data->s2mm006_om))
		gpio_free(usbpd_data->s2mm006_om);
	if (gpio_is_valid(usbpd_data->redriver_en))
		gpio_free(usbpd_data->redriver_en);
	if (gpio_is_valid(usbpd_data->irq_gpio))
		gpio_free(usbpd_data->irq_gpio);
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (gpio_is_valid(usbpd_data->irq_vbus_gpio))
		gpio_free(usbpd_data->irq_vbus_gpio);
#endif
}

static int s2mm006_check_factory(struct s2mm006_data *usbpd_data)
{
	FACTORY_STATUS_Type FACTORY_STATUS;
	FACTORY_CTRL_Type factory_ctrl;
	uint8_t fcy_id = S2MM006_FACTORY_OPEN;
	uint8_t rid = 0, ccid = 0;
	int cc_factory = 0, rid_factory = 0, usb_factory = 0;
	int ret = S2MM006_NOFACTORY, i2c_ret = 0;

	i2c_ret = s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_FACTORY_CTRL, &factory_ctrl.BYTE);
	if (i2c_ret < 0)
		goto err;

	fcy_id = factory_ctrl.BITS.Fcy_Id;

	i2c_ret = s2mm006_read_byte(usbpd_data->i2c,
		S2MM006_REG_FACTORY_STATUS, &FACTORY_STATUS.BYTE);
	if (i2c_ret < 0)
		goto err;

	ccid = FACTORY_STATUS.BITS.CcidValue;
	rid = FACTORY_STATUS.BITS.RidValue;

	switch (rid) {
	case S2MM006_RID_255K:
	case S2MM006_RID_301K:
	case S2MM006_RID_523K:
	case S2MM006_RID_619K:
		rid_factory = 1;
		break;
	default:
		break;
	}

	switch (ccid) {
	case S2MM006_RID_255K:
	case S2MM006_RID_301K:
	case S2MM006_RID_523K:
	case S2MM006_RID_619K:
		cc_factory = 1;
		break;
	default:
		break;
	}

	if (FACTORY_STATUS.BITS.Factory && fcy_id)
		usb_factory = 1;

	pr_info("%s ccid=%d rid=%d fcy_id=%d factory=%d\n",
		__func__, ccid, rid, fcy_id, FACTORY_STATUS.BITS.Factory);

	pr_info("%s rid fac=%d, cc fac=%d, usb fac=%d\n",
		__func__, rid_factory, cc_factory, usb_factory);

	if (usb_factory)
		ret = S2MM006_USBID;
	else if (cc_factory)
		ret = S2MM006_CCID;
	else if (rid_factory)
		ret = S2MM006_RID;
	else
		ret = S2MM006_NOFACTORY;
	return ret;
err:
	pr_err("%s. i2c error\n", __func__);
	return ret;
}

void s2mm006_om_reset(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t reg = S2MM006_REG_DEVICE_FW_ID;
	u8 r_data = 0;
	int i;

	pr_info("%s +\n", __func__);

	/* for Wake up*/
	for (i = 0; i < 5; i++)
		s2mm006_read_byte(i2c, reg, &r_data);

	/* need to OM reset operation */
	gpio_direction_output(usbpd_data->s2mm006_om, 1);
	msleep(20);
	gpio_direction_output(usbpd_data->s2mm006_om, 0);
	msleep(80);

	pr_info("%s -\n", __func__);
}

static void s2mm006_init_val(struct s2mm006_data *usbpd_data)
{
	/* Init */
	usbpd_data->pd_state = 0;
	usbpd_data->PowerRole = S2MM006_SINK;
	usbpd_data->DataRole = S2MM006_UFP;
	usbpd_data->SrcCur = S2MM006_RP_CURRENT_NOATTACH;
	usbpd_data->is_short = 0;
	usbpd_data->cc_status = 0;
	usbpd_data->cc_pin_active = 0;
	usbpd_data->cur_rid = RID_OPEN;
	usbpd_data->water_det = 0;
	usbpd_data->pd_contract = false;
	usbpd_data->fcy_wakeup = false;
	usbpd_data->last_battery_status = -1;
	s2mm006_alt_info_clear(usbpd_data);
	s2mm006_pd_init(usbpd_data);
}

static int need_fw_update(struct s2mm006_data *usbpd_data,
		u8 fw_ver, u8 fw_test_ver, u8 force)
{
	int ret = 0;
	int header_fw_ver, header_fw_test_ver;
	header_fw_ver = s2mm006_get_header_fw_ver(S2MM006_MAIN);	
	header_fw_test_ver = s2mm006_get_header_fw_ver(S2MM006_MAIN_TEST);

	if (force) {
		ret = FACTORY_FW_UPDATE;
		goto result;
	}

	if ((fw_ver >= 0xf0)) {
		ret = FACTORY_FW_UPDATE;
		goto result;
	}

	if (fw_ver != header_fw_ver) {
		ret = MAIN_FW_UPDATE;
		goto result;
	}

	if (fw_test_ver != header_fw_test_ver) {
		ret = MAIN_FW_UPDATE;
		goto result;
	}
result:
	pr_info("%s ret=%d\n", __func__, ret);

	return ret;
}

static int need_factory_fw_update(struct s2mm006_data *usbpd_data,
		u8 fw_boot_ver, u8 force)
{
	int ret = 0;

	if (force) {
		ret = FACTORY_FW_UPDATE;
		goto result;
	}

	if (fw_boot_ver != s2mm006_get_header_fw_ver(S2MM006_FACTORY_BOOT)) {
		ret = FACTORY_FW_UPDATE;
		goto result;
	}
result:
	pr_info("%s ret=%d\n", __func__, ret);

	return ret;
}

static int need_fw_update_boot_ver_added(struct s2mm006_data *usbpd_data,
		u8 fw_ver, u8 fw_test_ver, u8 fw_boot_ver, u8 force)
{
	int ret = 0;

	ret = need_fw_update(usbpd_data, fw_ver, fw_test_ver, force);
	ret |= need_factory_fw_update(usbpd_data, fw_boot_ver, force);

	pr_info("%s ret=%d\n", __func__, ret);

	return ret;
}

static void s2mm006_store_pdic_version(DEVICE_HW_ID_Type hw_ver,
	u8 fw_ver, u8 fw_test_ver, u8 boot_ver)
{
	u8 hw_rev[4] = {0, };
	u8 sw_main[3] = {0, };
	u8 sw_boot = 0;

	hw_rev[0] = hw_ver.BITS.HWVer;
	hw_rev[1] = hw_ver.BITS.VendorID;
	sw_main[0] = fw_ver;
	sw_main[1] = fw_test_ver;
	sw_boot = boot_ver;

	store_ccic_version(&hw_rev[0], &sw_main[0], &sw_boot);

	sw_main[0] = s2mm006_get_header_fw_ver(S2MM006_MAIN);
	sw_main[1] = s2mm006_get_header_fw_ver(S2MM006_MAIN_TEST);
	sw_boot = s2mm006_get_header_fw_ver(S2MM006_FACTORY_BOOT);

	store_ccic_bin_version(&sw_main[0], &sw_boot);
}

static void s2mm006_fw_check_update(struct s2mm006_data *usbpd_data)
{
	DEVICE_HW_ID_Type hw_ver;
	u8 fw_ver = 0, fw_test_ver = 0, boot_ver = 0;
	int fw_update_type = 0;
	int ret = 0;

	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_HW_ID, &hw_ver.BYTE);
	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_FW_ID, &fw_ver);
	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_FW_TEST_ID, &fw_test_ver);
	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_BOOT_ID, &boot_ver);
	pr_info("%s,HW_VER : %X\n", __func__, hw_ver.BITS.HWVer);
	pr_info("%s,HW_VENDERID : %X\n", __func__, hw_ver.BITS.VendorID);
	pr_info("%s,FW_VER : %02X, FW_TEST_VER : %02X, BOOT_VER: %02X\n",
			__func__, fw_ver, fw_test_ver, boot_ver);

	pr_info("%s,Binary FW_VER : %02X, FW_TEST_VER : %02X, BOOT_VER: %02X\n",
			__func__, s2mm006_get_header_fw_ver(S2MM006_MAIN),
			s2mm006_get_header_fw_ver(S2MM006_MAIN_TEST),
			s2mm006_get_header_fw_ver(S2MM006_FACTORY_BOOT));

	if (s2mm006_check_factory(usbpd_data) != S2MM006_USBID)
		s2mm006_om_reset(usbpd_data);
	else {
		if (!check_factory_mode_boot())
			pr_err("%s. it is abnormal. mode mismatch.\n", __func__);
	}

	fw_update_type = need_fw_update_boot_ver_added(usbpd_data, fw_ver, fw_test_ver, boot_ver, 0);

	if (fw_update_type == FACTORY_FW_UPDATE) { 
		if (s2mm006_flash_factory_fw(usbpd_data, NULL, 0, FW_BUILT_IN) < 0) {
			pr_err("%s: s2mm006_flash_factory_fw 1st fail, try again\n", __func__);
			if (s2mm006_flash_factory_fw(usbpd_data, NULL, 0, FW_BUILT_IN) < 0)
				pr_err("%s: s2mm006_flash_factory_fw 2st fail\n", __func__);
		}

		/* fw update with fw bin */
		s2mm006_fw_sysfs_update(usbpd_data, 1);
	}
	else {
		ret = s2mm006_fw_sysfs_update(usbpd_data, 1);
		if (ret < 0) {
			pr_err("%s: s2mm006 main_fw 1st fail, try again\n", __func__);
			ret = s2mm006_fw_sysfs_update(usbpd_data, 1);
			if (ret < 0) {
				pr_err("%s: s2mm006 main_fw 2st fail\n", __func__);
			}
		}
		if (ret == 0) {
			pr_err("%s: fw updated with fw bin.\n", __func__);
		}
	}

	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_FW_ID, &fw_ver);
	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_FW_TEST_ID, &fw_test_ver);
	s2mm006_read_byte(usbpd_data->i2c,
			S2MM006_REG_DEVICE_BOOT_ID, &boot_ver);

	pr_info("%s,Final FW_VER : %02X, FW_TEST_VER : %02X, BOOT_VER: %02X\n",
			__func__, fw_ver, fw_test_ver, boot_ver);

	usbpd_data->firm_ver = fw_ver;

	s2mm006_store_pdic_version(hw_ver, fw_ver, fw_test_ver, boot_ver);
}

void s2mm006_init_register(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	u8 read_byte;
	int ret = 0;

	if (usbpd_data->water_detect_support) {
		ret = s2mm006_read_byte(i2c, S2MM006_REG_WATER_CTRL1, &read_byte);
		if (ret < 0)
			goto err;
		(*((I2C_WATER_CTRL1_Type *)&read_byte)).BITS.WaterEn = 1;
		ret = s2mm006_write_byte(i2c, S2MM006_REG_WATER_CTRL1, &read_byte);
		if (ret < 0)
			goto err;
	}

	if (!s2mm006_is_pogo(usbpd_data))
		s2mm006_set_enable_alternate_mode(usbpd_data, usbpd_data->set_en_alt_mode);

	return;
err:
	pr_err("%s. i2c error\n", __func__);
}

static int s2mm006_fw_ver_check(void *data)
{
	struct s2mm006_data *usbpd_data = data;

	if (usbpd_data->firm_ver < s2mm006_get_header_fw_ver(S2MM006_MAIN))
		return PDIC_FW_VERSION_INVALID;
	return 0;
}

static void s2mm006_int_clear(struct s2mm006_data *usbpd_data, uint16_t reg, uint8_t data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	int ret = 0;

	ret = s2mm006_write_byte(i2c, reg, &data);
	if (ret < 0)
		pr_err("%s. i2c error\n", __func__);
}

static void s2mm006_get_status(struct s2mm006_data *usbpd_data)
{
	int i;
	uint8_t buf1[20] = {0, };
	uint8_t buf2[10] = {0, };

	for (i = 0; i < 13; i++)
		s2mm006_read_byte(usbpd_data->i2c, (i + S2MM006_REG_BC_STATUS1), &buf1[i]);

	for (i = 0; i < 8; i++)
		s2mm006_read_byte(usbpd_data->i2c, (i + S2MM006_REG_FW_PD_STATE0), &buf2[i]);

	pr_info("%s BC_STATUS(0x%02x, 0x%02x, 0x%02x)\n", __func__,
		buf1[BUF1_ADDR(S2MM006_REG_BC_STATUS1)], buf1[BUF1_ADDR(S2MM006_REG_BC_STATUS2)],
		buf1[BUF1_ADDR(S2MM006_REG_BC_STATUS3)]);
	pr_info("%s CC_STATUS(0x%02x, 0x%02x)\n", __func__,
		buf1[BUF1_ADDR(S2MM006_REG_CC_STATUS1)], buf1[BUF1_ADDR(S2MM006_REG_CC_STATUS2)]);
	pr_info("%s PD_STATUS(0x%02x, 0x%02x, 0x%02x)\n", __func__,
		buf1[BUF1_ADDR(S2MM006_REG_PD_STATUS1)], buf1[BUF1_ADDR(S2MM006_REG_PD_STATUS2)],
		buf1[BUF1_ADDR(S2MM006_REG_PD_STATUS3)]);
	pr_info("%s FAC_STATUS(0x%02x, 0x%02x)\n", __func__,
		buf1[BUF1_ADDR(S2MM006_REG_FACTORY_STATUS)], buf1[BUF1_ADDR(S2MM006_REG_FACTORY_STATUS2)]);
	pr_info("%s PRT_STATUS(0x%02x, 0x%02x)\n", __func__,
		buf1[BUF1_ADDR(S2MM006_REG_PROTECTION_STATUS1)], buf1[BUF1_ADDR(S2MM006_REG_PROTECTION_STATUS2)]);
	pr_info("%s FWU_STATUS(0x%02x)\n", __func__,
		buf1[BUF1_ADDR(S2MM006_REG_FW_UPDATE_STATUS)]);

	pr_info("%s PD_STATE(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n", __func__,
		buf2[BUF2_ADDR(S2MM006_REG_FW_PD_STATE0)], buf2[BUF2_ADDR(S2MM006_REG_FW_PD_STATE1)],
		buf2[BUF2_ADDR(S2MM006_REG_FW_PD_STATE2)], buf2[BUF2_ADDR(S2MM006_REG_FW_PD_STATE3)]);
	pr_info("%s MU_STATE(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n", __func__,
		buf2[BUF2_ADDR(S2MM006_REG_FW_MU_STATE0)], buf2[BUF2_ADDR(S2MM006_REG_FW_MU_STATE1)],
		buf2[BUF2_ADDR(S2MM006_REG_FW_MU_STATE2)], buf2[BUF2_ADDR(S2MM006_REG_FW_MU_STATE3)]);
}

////////////////////////////////////////////////////////////////////////////////
// s2mm006 IRQ Handler functions
////////////////////////////////////////////////////////////////////////////////

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
static int s2mm006_path_set(struct i2c_client *i2c, int path)
{
	u8 reg_val = 0;
	u8 prev_reg_val = 0;
	int ret = 0;

	if (path > S2MM006_PATH_AUTO) {
		ret = -ENOMSG;
		goto err;
	}

	pr_info("%s path=%s\n", __func__, s2mm006_path[path]);

	ret = s2mm006_read_byte(i2c, S2MM006_REG_BC_CTRL2, &reg_val);
	prev_reg_val = reg_val;

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

	if (prev_reg_val != reg_val)
		ret = s2mm006_write_byte(i2c, S2MM006_REG_BC_CTRL2, &reg_val);
	else
		pr_err("%s. set path skip. same register value.\n", __func__);

	if (ret < 0)
		goto err;
	return 0;
err:
	pr_err("%s. error. ret=%d\n", __func__, ret);
	return ret;
}
#endif

static void s2mm006_int_factory_rid(void *data, int mode)
{
	struct s2mm006_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	FACTORY_STATUS_Type FACTORY_STATUS;
	S2MM006_RID_Type s2mm006_rid = -1;
	u8 rid;
	int ret = 0;

	if (mode > S2MM006_RID)
		return;

	pr_info("%s mode = %s\n", __func__, (mode == S2MM006_RID) ? "RID" : "CCIC");
	ret = s2mm006_read_byte(i2c, S2MM006_REG_FACTORY_STATUS, &FACTORY_STATUS.BYTE);
	if (ret < 0)
		goto err;

	if (mode == S2MM006_CCID)
		s2mm006_rid = FACTORY_STATUS.BITS.CcidValue;
	else if (mode == S2MM006_RID)
		s2mm006_rid = FACTORY_STATUS.BITS.RidValue;

	switch (s2mm006_rid) {
	case S2MM006_RID_OPEN_0:
	case S2MM006_RID_OPEN_6:
		rid = RID_OPEN;
		break;
	case S2MM006_RID_1K:
		rid = RID_001K;
		break;
	case S2MM006_RID_255K:
		rid = RID_255K;
		break;
	case S2MM006_RID_301K:
		rid = RID_301K;
		break;
	case S2MM006_RID_523K:
		rid = RID_523K;
		break;
	case S2MM006_RID_619K:
		rid = RID_619K;
		break;
	case S2MM006_RID_Reserved:
		rid = RID_UNDEFINED;
		break;
	default:
		break;
	}

	pr_info("prev_rid:%x , RID:%x\n", usbpd_data->cur_rid, rid);

	if (rid) {
		if (usbpd_data->cur_rid != rid) {
			if (mode == S2MM006_CCID) {
				if (rid == RID_OPEN)
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_ROPEN, 0, 0);
				else if (rid == RID_301K)
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_R301K, 0, 0);
				else if (rid == RID_255K)
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_R255K, 0, 0);
				else if (rid == RID_523K) {
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
					s2mm006_path_set(i2c, S2MM006_PATH_UART);
#endif
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_R523K, 0, 0);
				}
				else if (rid == RID_619K) {
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
					s2mm006_path_set(i2c, S2MM006_PATH_UART);
#endif
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_R619K, 0, 0);
				}
				else
					;
			} else if (mode == S2MM006_RID) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
				s2mm006_muic_rid_det_isr(usbpd_data->muic_data);
#endif
				if (rid == RID_OPEN)
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_UOPEN, 0, 0);
				else if (rid == RID_301K)
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_U301K, 0, 0);
				else if (rid == RID_255K)
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_U255K, 0, 0);
				else if (rid == RID_523K) {
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
					s2mm006_path_set(i2c, S2MM006_PATH_UART);
#endif
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_U523K, 0, 0);
				}
				else if (rid == RID_619K) {
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
					s2mm006_path_set(i2c, S2MM006_PATH_UART);
#endif
					pdic_policy_send_msg(usbpd_data->pp_data, MSG_U619K, 0, 0);
				}
				else
					;
			}
		}
		usbpd_data->cur_rid = rid;
	}
	return;
err:
	pr_err("%s. i2c error\n", __func__);
}

static void s2mm006_int_factory_mode(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	FACTORY_STATUS_Type FACTORY_STATUS;
	FACTORY_CTRL_Type factory_ctrl;
	uint8_t fcy_id = S2MM006_RID_OPEN_0;
	bool factory = 0;
	int ret = 0;

	ret = s2mm006_read_byte(i2c, S2MM006_REG_FACTORY_STATUS,
		&FACTORY_STATUS.BYTE);
	if (ret < 0)
		goto err;

	ret = s2mm006_read_byte(i2c,
			S2MM006_REG_FACTORY_CTRL, &factory_ctrl.BYTE);
	if (ret < 0)
		goto err;

	fcy_id = factory_ctrl.BITS.Fcy_Id;

	factory = FACTORY_STATUS.BITS.Factory;
	if (factory)
		usbpd_data->usb_boot_mode = fcy_id;
	else {
		usbpd_data->usb_boot_mode = S2MM006_RID_OPEN_0;
		factory_ctrl.BITS.Fcy_Id = S2MM006_RID_OPEN_0;
		ret = s2mm006_write_byte(i2c,
			S2MM006_REG_FACTORY_CTRL, &factory_ctrl.BYTE);
		if (ret < 0)
			goto err;
	}

	pr_info("%s factory=%d fcy_id=%d\n", __func__, factory, fcy_id);
	return;
err:
	pr_err("%s. i2c error\n", __func__);
}

static void s2mm006_int_factory_wakeup(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	FACTORY_STATUS_Type FACTORY_STATUS;
	bool fcy_wakeup = 0;
	int ret = 0;

	pr_info("%s +\n", __func__);

	ret = s2mm006_read_byte(i2c, S2MM006_REG_FACTORY_STATUS,
		&FACTORY_STATUS.BYTE);
	if (ret < 0)
		goto err;

	fcy_wakeup = FACTORY_STATUS.BITS.FcyWakeUp;
	if (fcy_wakeup)
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_NOCC_WAKE, 0, 0);

	usbpd_data->fcy_wakeup = fcy_wakeup;

	pr_info("%s fcy_wakeup=%d -\n", __func__, fcy_wakeup);
	return;
err:
	pr_err("%s. i2c error -\n", __func__);
}

static void s2mm006_factory_int(struct s2mm006_data *usbpd_data, FACTORY_INT_Type FAC_INT)
{
	if (FAC_INT.BITS.CcidDet)
		s2mm006_int_factory_rid(usbpd_data, S2MM006_CCID);
	if (FAC_INT.BITS.RidDet)
		s2mm006_int_factory_rid(usbpd_data, S2MM006_RID);
	if (FAC_INT.BITS.Factory)
		s2mm006_int_factory_mode(usbpd_data);
	if (FAC_INT.BITS.FcyWakeUp)
		s2mm006_int_factory_wakeup(usbpd_data);
	if (FAC_INT.BITS.FwUpdate)
		;
	if (FAC_INT.BITS.FwModeEnter)
		;
	if (FAC_INT.BITS.IfcSenseApInt)
		;
	if (FAC_INT.BITS.FwCmdReceive)
		;

}

#if IS_ENABLED(CONFIG_USB_HW_PARAM)
static enum usb_hw_param s2mm006_select_short_type(PROTECTION_STATUS2_Type PROTECTION_STATUS2) {
	if (PROTECTION_STATUS2.BITS.Cc1Ovp | PROTECTION_STATUS2.BITS.Cc2Ovp)
		return USB_CCIC_VBUS_CC_SHORT_COUNT;
	else if (PROTECTION_STATUS2.BITS.Sbu1Ovp | PROTECTION_STATUS2.BITS.Sbu2Ovp)
		return USB_CCIC_VBUS_SBU_SHORT_COUNT;

	return USB_CCIC_HW_PARAM_MAX;
}
#endif

static void s2mm006_int_ovp(void *data)
{
	struct s2mm006_data *usbpd_data = data;
	PROTECTION_STATUS2_Type PROTECTION_STATUS2;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	enum usb_hw_param hw_param_index;
	struct otg_notify *o_notify = get_otg_notify();
#endif

	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PROTECTION_STATUS2, &PROTECTION_STATUS2.BYTE);

	pr_info("%s, OVP cc1(%d), cc2(%d), sbu1(%d), sbu2(%d), dp(%d), dm(%d)\n",
			__func__, PROTECTION_STATUS2.BITS.Cc1Ovp, PROTECTION_STATUS2.BITS.Cc2Ovp,
			PROTECTION_STATUS2.BITS.Sbu1Ovp, PROTECTION_STATUS2.BITS.Sbu2Ovp,
			PROTECTION_STATUS2.BITS.DpOvp, PROTECTION_STATUS2.BITS.DmOvp);

	if (PROTECTION_STATUS2.BYTE & 0x0f) {
		usbpd_data->is_short = true;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		hw_param_index = s2mm006_select_short_type(PROTECTION_STATUS2);
		if (o_notify) {
			if (0 <= hw_param_index && hw_param_index < USB_CCIC_HW_PARAM_MAX)
				inc_hw_param(o_notify, hw_param_index);
		}
#endif
	}
}

static void s2mm006_int_cc_water(void *data)
{
	struct s2mm006_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	PROTECTION_STATUS1_Type PROTECTION_STATUS1;

	pr_info("%s\n", __func__);

	if (!usbpd_data->water_detect_support) {
		dev_info(&i2c->dev, "%s: It does not support water detection\n", __func__);
		return;
	}

	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PROTECTION_STATUS1,
			&PROTECTION_STATUS1.BYTE);

	if (PROTECTION_STATUS1.BITS.Water) {
		pr_info("== WATER DETECT ==\n");
		/* check notify to MUIC
		 * usbpd_data->muic_data->~water~ = 1;
		 */
		usbpd_data->water_det = 1;
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_WATER, 0, 0);
	} else {
		pr_info("== WATER RUN-DRY DETECT ==\n");
		usbpd_data->water_det = 0;
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_DRY, 0, 0);
	}
}

static void s2mm006_int_cc_usbkiller(void *data)
{
	struct s2mm006_data *usbpd_data = data;
	PROTECTION_STATUS1_Type PROTECTION_STATUS1;

	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PROTECTION_STATUS1,
			&PROTECTION_STATUS1.BYTE);

	pr_info("%s. killer=%d\n", __func__, PROTECTION_STATUS1.BITS.KillerDet);

	if (PROTECTION_STATUS1.BITS.KillerDet)
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_KILLER, 0, 0);
}

static void s2mm006_protection_int(struct s2mm006_data *usbpd_data, PROTECTION_INT_Type PRT_INT)
{
	if (PRT_INT.BITS.Ocp)
		;
	if (PRT_INT.BITS.Ovp)
		s2mm006_int_ovp(usbpd_data);
	if (PRT_INT.BITS.Water)
		s2mm006_int_cc_water(usbpd_data);
	if (PRT_INT.BITS.KillerDet)
		s2mm006_int_cc_usbkiller(usbpd_data);
	if (PRT_INT.BITS.ResetDone)
		;
}

static void s2mm006_int_cc_detach(void *data)
{
	struct s2mm006_data *usbpd_data = data;

	pr_info("%s +\n", __func__);

	if (usbpd_data->alt_info.dp_device)
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_DP_DISCONN, 0, 0);

	pdic_policy_send_msg(usbpd_data->pp_data, MSG_CCOFF, 0, 0);

	if (usbpd_data->PowerRole == S2MM006_SOURCE)
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_VBUSOFF, 0, 0);

	s2mm006_init_val(usbpd_data);

	pr_info("%s -\n", __func__);
}

static void process_rp_current(struct s2mm006_data *usbpd_data)
{
	u8 read_data = 0;
	CC_STATUS1_Type CC_STATUS1;
	int msg = MSG_INITIAL;

	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_CC_STATUS1, &read_data);
	CC_STATUS1.BYTE = read_data;

	pr_info("%s. src cur=0x%x\n", __func__, CC_STATUS1.BITS.SrcCur);

	if (usbpd_data->SrcCur != CC_STATUS1.BITS.SrcCur) {
		if (CC_STATUS1.BITS.SrcCur == S2MM006_RP_CURRENT_DEFAULT)
			msg = MSG_RP56K;
		else if (CC_STATUS1.BITS.SrcCur == S2MM006_RP_CURRENT_1_5A)
			msg = MSG_RP22K;
		else if (CC_STATUS1.BITS.SrcCur == S2MM006_RP_CURRENT_3A)
			msg = MSG_RP10K;

		usbpd_data->SrcCur = CC_STATUS1.BITS.SrcCur;
		pdic_policy_send_msg(usbpd_data->pp_data, msg, 0, 0);
	}
}

static S2MM006_CC_PIN_ACTIVE_Type s2mm006_get_side_check(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	CC_STATUS2_Type CC_STATUS2;

	s2mm006_read_byte(i2c, S2MM006_REG_CC_STATUS2, &CC_STATUS2.BYTE);

	return CC_STATUS2.BITS.CcPinActive;
}

static void s2mm006_int_cc_status(struct s2mm006_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	CC_STATUS1_Type cc_status1;
	CC_STATUS2_Type cc_status2;
	static const char * const status1_str[] = {
		"No Connection",
		"Sink",
		"Source",
		"Powered Cable",
		"Debug",
		"Audio",
		"Error",
		"Disabled"};
	static const char * const status2_str[] = {
		"No Connection",
		"cc1",
		"cc2",
		"reserve"};

	s2mm006_read_byte(i2c, S2MM006_REG_CC_STATUS1, &cc_status1.BYTE);

	pr_info("%s, %s is attached.\n", __func__,
		status1_str[cc_status1.BITS.CcStatus]);

	if (cc_status1.BITS.CcStatus == S2MM006_CC_SINK ||
			cc_status1.BITS.CcStatus == S2MM006_CC_SOURCE) {
		cc_status2.BITS.CcPinActive
				= s2mm006_get_side_check(usbpd_data);

		if (cc_status2.BITS.CcPinActive == S2MM006_CC_1ACTIVE) {
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
			s2mm006_path_set(i2c, S2MM006_PATH_USB);
#endif
			pdic_policy_send_msg(usbpd_data->pp_data, MSG_CC1, 0, 0);
		}
		else if (cc_status2.BITS.CcPinActive == S2MM006_CC_2ACTIVE) {
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
			s2mm006_path_set(i2c, S2MM006_PATH_USB);
#endif
			pdic_policy_send_msg(usbpd_data->pp_data, MSG_CC2, 0, 0);
		}
		else
			;
		usbpd_data->cc_pin_active = cc_status2.BITS.CcPinActive;
		pr_info("%s cc active=%s\n", __func__,
			status2_str[cc_status2.BITS.CcPinActive]);
	}
	usbpd_data->cc_status = cc_status1.BITS.CcStatus;
}

static void s2mm006_cc_int(struct s2mm006_data *usbpd_data, CC_INT_Type CC_INT)
{
	u8 read_data = 0;

	if (CC_INT.BITS.vSafe0v)
		;
	if (CC_INT.BITS.Attach)
		;
	if (CC_INT.BITS.Detach) {
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_CC_STATUS1, &read_data);
		if (((CC_STATUS1_Type)read_data).BITS.Attach == 0)
			s2mm006_int_cc_detach(usbpd_data);
	}

	if (CC_INT.BITS.SrcCur)
		process_rp_current(usbpd_data);
	if (CC_INT.BITS.ccStatus)
		s2mm006_int_cc_status(usbpd_data);
	if (CC_INT.BITS.VconnStatus)
		;
	if (CC_INT.BITS.VbusReq) {
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_CC_STATUS2, &read_data);
		if (((CC_STATUS2_Type)read_data).BITS.VbusReq) {
			pdic_policy_send_msg(usbpd_data->pp_data, MSG_VBUSON, 0, 0);
		} else {
			pdic_policy_send_msg(usbpd_data->pp_data, MSG_VBUSOFF, 0, 0);
		}
	}
}

static void s2mm006_usbpd_check_power_role(struct s2mm006_data *usbpd_data, int mode)
{
	if (mode == S2MM006_SINK)
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_SNK, 0, 0);
	else if (mode == S2MM006_SOURCE)
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_SRC, 0, 0);
	else
		;
	usbpd_data->PowerRole = mode;
}

static void s2mm006_usbpd_check_data_role(struct s2mm006_data *usbpd_data, int mode)
{
	if (mode == S2MM006_DFP) {
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_DFP, 0, 0);
	} else if (mode == S2MM006_UFP) {
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_UFP, 0, 0);
		if (usbpd_data->DataRole == S2MM006_DFP)
			s2mm006_alt_info_clear(usbpd_data);
	} else
		;
	usbpd_data->DataRole = mode;
}

static void s2mm006_pd_int(struct s2mm006_data *usbpd_data, PD_INT_Type PD_INT)
{
	u8 read_data = 0;
	uint8_t ReadMSG[32];
	uint32_t *DataObject = (uint32_t *)ReadMSG;
	DATA_BATTERY_STATUS_TYPE *data_batt_status;
	EXTENDED_STATUS_TYPE *ext_status;
	EXTENDED_SRC_CAP_TYPE *ext_src_cap;
	u8 battery_info = 0;
	struct pdic_notifier_struct *pd_noti = &usbpd_data->pd_noti;

	if (PD_INT.BITS.PdMessage)
		;
	if (PD_INT.BITS.PowerRole) {
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PD_STATUS2, &read_data);
		s2mm006_usbpd_check_power_role(usbpd_data,
			((PD_STATUS2_Type)read_data).BITS.PowerRole);
	}

	if (PD_INT.BITS.DataRole) {
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PD_STATUS2, &read_data);
		s2mm006_usbpd_check_data_role(usbpd_data,
			((PD_STATUS2_Type)read_data).BITS.DataRole);
	}

	if (PD_INT.BITS.PsReady) {
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PD_STATUS2, &read_data);
		if (((PD_STATUS2_Type)read_data).BITS.PsReady == PS_READY_RECEIVED)
			process_ps_ready_received(usbpd_data);
		else if (((PD_STATUS2_Type)read_data).BITS.PsReady == PS_READY_SENT)
			process_ps_ready_sent(usbpd_data);
	}

	if (PD_INT.BITS.VconnRole)
		;

	if (PD_INT.BITS.BatteryStatus &&
		!strcmp(usbpd_data->name, "s2mm006-pogo")) {
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_PD_STATUS4, &read_data);

		if (((PD_STATUS4_Type)read_data).BITS.BatteryStatus) {
			s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_BAT_STATUS, ReadMSG, 32);
			if (*(uint16_t *)ReadMSG == 0) {
				pr_info("%s, Invalid MSG_Header line(%d)\n", __func__, __LINE__);
				return;
			} else {
				data_batt_status = (DATA_BATTERY_STATUS_TYPE *)(&DataObject[1]);

				pr_info("%s, BATTERY_STATUS capacity(%d)\n", __func__,
						data_batt_status->BYTES.BatteryPresentCapacity);

				data_batt_status->BYTES.BatteryPresentCapacity += 1;

				/* SOC(%) = read capacity * 100000 / (3 * 320 * 388) */
				/* ex) 289 * 100000 / (3 * 320 * 388) = 77.588% */
				pd_noti->sink_status.capacity = data_batt_status->BYTES.BatteryPresentCapacity * 100000 / 372480;

				pr_info("%s, BATTERY_STATUS capacity(%d), battInfo(0x%x)\n", __func__,
						data_batt_status->BYTES.BatteryPresentCapacity,
						data_batt_status->BYTES.BatteryInfo);

				battery_info = data_batt_status->BYTES.BatteryInfo & 0x0c;
			}

			s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_STATUS, ReadMSG, 32);
			if (*(uint16_t *)ReadMSG == 0) {
				pr_info("%s, Invalid MSG_Header line(%d)\n", __func__, __LINE__);

				/* if read_temp fail, return temp=30 in s2mm006_read_status */
				pd_noti->sink_status.temp = 30;
			} else {
				ext_status = (EXTENDED_STATUS_TYPE *)(&DataObject[1]);
				pd_noti->sink_status.temp = ext_status->BYTES.InternalTemp;

				pr_info("%s, STATUS temp(%d)\n", __func__,
					pd_noti->sink_status.temp);
			}

			s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_SRC_CAP_EXT, ReadMSG, 32);
			if (*(uint16_t *)ReadMSG == 0) {
				pr_info("%s, Invalid MSG_Header line(%d)\n", __func__, __LINE__);
				return;
			} else {
				ext_src_cap = (EXTENDED_SRC_CAP_TYPE *)(&DataObject[1]);

				pd_noti->sink_status.vid = ext_src_cap->ID.VendorID;
				pd_noti->sink_status.pid = ext_src_cap->ID.ProductID;
				pd_noti->sink_status.xid = ext_src_cap->ID.XID >> 24;
			}

			pr_info("%s, VID(0x%x), PID(0x%x), XID(0x%x)\n", __func__,
				pd_noti->sink_status.vid,
				pd_noti->sink_status.pid,
				pd_noti->sink_status.xid);

			pdic_policy_send_msg(usbpd_data->pp_data, MSG_BATSTATUS, battery_info >> 2, 0);
		}
	}
}

static void s2mm006_pd2_int(struct s2mm006_data *usbpd_data, PD_INT2_Type PD_INT2)
{
#ifdef CONFIG_PDIC_ALTERNATE_MODE
	if (PD_INT2.BITS.AlternateMode)
		s2mm006_int_altmode(usbpd_data);
#endif
}

static void s2mm006_usbpd_irq_process(struct s2mm006_data *usbpd_data)
{
	int irq_gpio_status[2];
	int i;
	uint32_t ret;
	uint8_t REG_ADD;
	char str[256] = {0, };
#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
	int sink = 0;
	BC_STATUS1_Type bc_status1;
#endif

	pr_info("%s [%s]\n", __func__, usbpd_data->name);

	irq_gpio_status[0] = gpio_get_value(usbpd_data->irq_gpio);
	pr_info("IRQ0:%02d\n", irq_gpio_status[0]);

	if (s2mm006_fw_ver_check(usbpd_data) == PDIC_FW_VERSION_INVALID)
		goto ver_err;

	REG_ADD = S2MM006_REG_BC_INT;
	for (i = 0; i < 6; i++) {
		ret = s2mm006_read_byte(usbpd_data->i2c, REG_ADD++, &(usbpd_data->INT_STATUS.DATA[i]));
		if (ret < 0) {
			pr_info("%s, i2c read fail, %d status\n", __func__, ret);
			usbpd_data->INT_STATUS.DATA[i] = 0;
		}
	}

	REG_ADD = S2MM006_REG_BC_INT;
	for (i = 0; i < 6; i++)
		s2mm006_int_clear(usbpd_data, REG_ADD++, usbpd_data->INT_STATUS.DATA[i]);

	for (i = 0; i < 6; i++) {
		if (usbpd_data->INT_STATUS.DATA[i])
			sprintf(str+strlen(str), ", %s:0x%02x", usbpd_irq_str[i], usbpd_data->INT_STATUS.DATA[i]);
	}
	pr_info("%s%s\n", __func__, str);
	s2mm006_get_status(usbpd_data);

	if (usbpd_data->INT_STATUS.BYTE.FACTORY_INT.BYTE)
		s2mm006_factory_int(usbpd_data, usbpd_data->INT_STATUS.BYTE.FACTORY_INT);

	if (usbpd_data->INT_STATUS.BYTE.PROTECTION_INT.BYTE)
		s2mm006_protection_int(usbpd_data, usbpd_data->INT_STATUS.BYTE.PROTECTION_INT);

	if (usbpd_data->INT_STATUS.BYTE.CC_INT.BYTE)
		s2mm006_cc_int(usbpd_data, usbpd_data->INT_STATUS.BYTE.CC_INT);

	if (usbpd_data->INT_STATUS.BYTE.PD_INT.BYTE)
		s2mm006_pd_int(usbpd_data, usbpd_data->INT_STATUS.BYTE.PD_INT);

	if (usbpd_data->INT_STATUS.BYTE.PD_INT2.BYTE)
		s2mm006_pd2_int(usbpd_data, usbpd_data->INT_STATUS.BYTE.PD_INT2);

#if IS_ENABLED(CONFIG_MUIC_S2MM006)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	sink = s2mm006_is_pogo(usbpd_data);
	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_BC_STATUS1, &bc_status1.BYTE);

	pr_info("%s: pdic type: %s(%d), bc_status1(%02x)\n", __func__, usbpd_is_sink_str[sink], sink, bc_status1.BYTE);
	if ((usbpd_data->INT_STATUS.BYTE.BC_INT.BYTE || bc_status1.BITS.VbusDet) && sink == 0)
		process_muic(usbpd_data->muic_data, usbpd_data->INT_STATUS.DATA[0]);
	else
		pr_info("%s: process muic was skipped, pdic_type: %s, bc_int.byte:%02x\n", __func__, usbpd_is_sink_str[sink], usbpd_data->INT_STATUS.BYTE.BC_INT.BYTE);

#endif
#endif
ver_err:
	irq_gpio_status[1] = gpio_get_value(usbpd_data->irq_gpio);
	pr_info("IRQ1:%02d", irq_gpio_status[1]);
}

#if IS_ENABLED(CONFIG_S2MM006_POGO)
int s2mm006_is_pogo(void *_data)
{
	struct s2mm006_data *usbpd_data = _data;

	return usbpd_data->port_role == S2MM006_PORT_ROLE_SINK;
}

static irqreturn_t s2mm006_usbpd_vbus_irq_thread(int irq, void *data)
{
	struct s2mm006_data *usbpd_data = data;
	int irq_gpio_status[2];

	irq_gpio_status[0] = gpio_get_value(usbpd_data->irq_vbus_gpio);
	pr_info("IRQ0:%02d\n", irq_gpio_status[0]);



	irq_gpio_status[1] = gpio_get_value(usbpd_data->irq_vbus_gpio);
	pr_info("IRQ1:%02d", irq_gpio_status[1]);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_SEC_PD)
struct pdic_notifier_struct* s2mm006_get_pd_noti(void *_data)
{
	struct s2mm006_data *usbpd_data = _data;

	return (void *)(&usbpd_data->pd_noti);
}
#endif
#endif

static irqreturn_t s2mm006_usbpd_irq_thread(int irq, void *data)
{
	struct s2mm006_data *usbpd_data = data;

	pr_info("%s [%s] times %d\n", __func__, usbpd_data->name, ++usbpd_data->wq_times);
	s2mm006_wake_lock_timeout(usbpd_data->pdic_ws, 1000);

	mutex_lock(&usbpd_data->irq_mutex);

	if (usbpd_data->suspended) {
		pr_info("%s suspend state. it will be run after resume.\n", __func__);
		s2mm006_wake_lock_timeout(usbpd_data->pdic_ws, 2000);
		usbpd_data->keep_irq = true;
		goto suspended_error;
	}

	if (delayed_work_pending(&usbpd_data->irq_work))
		cancel_delayed_work(&usbpd_data->irq_work);

	usbpd_data->pdic_check_at_booting = 0;
	usbpd_data->keep_irq = false;

	s2mm006_usbpd_irq_process(usbpd_data);
suspended_error:
	mutex_unlock(&usbpd_data->irq_mutex);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_S2MM006_POGO)
static int s2mm006_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mm006_data *usbpd_data = power_supply_get_drvdata(psy);
	VBUS_CTRL_Type vbus_ctrl;
	VBUS_STATUS_Type vbus_status;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s, val(%s)\n", __func__, ((val->intval) ? "Short" : "Open"));

		vbus_ctrl.BYTE = 0;
		if (val->intval) {
			vbus_ctrl.BITS.VbusPort = 1;
			vbus_ctrl.BITS.VbusConsumer = 1;
		}
		s2mm006_write_byte(usbpd_data->i2c, S2MM006_REG_VBUS_CTRL, &vbus_ctrl.BYTE);

		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_VBUS_STATUS, &vbus_status.BYTE);
		if (val->intval
				&& vbus_status.BITS.VbusCollisionPrevent
				&& vbus_status.BITS.VbusConsumer)
			pr_info("%s, VbusStatus(0x%x), close ok\n",
					__func__, vbus_status.BYTE);
		else if (!val->intval
				&& !vbus_status.BITS.VbusCollisionPrevent
				&& !vbus_status.BITS.VbusConsumer)
			pr_info("%s, VbusStatus(0x%x), open ok\n",
					__func__, vbus_status.BYTE);
		else
			pr_info("%s, VbusStatus(0x%x) is invalid, open or close Fail\n",
					__func__, vbus_status.BYTE);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mm006_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mm006_init_psy(struct s2mm006_data *_data, struct device *parent)
{
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	if (_data == NULL || parent == NULL) {
		pr_err("%s NULL data\n", __func__);
		return -1;
	}

	_data->pdic_desc.name		= _data->name;
	_data->pdic_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	_data->pdic_desc.get_property   = s2mm006_get_property;
	_data->pdic_desc.set_property   = s2mm006_set_property;
	_data->pdic_desc.properties     = pdic_props;
	_data->pdic_desc.num_properties = ARRAY_SIZE(pdic_props);

	psy_cfg.drv_data = _data;
	psy_cfg.supplied_to = pdic_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(pdic_supplied_to);

	_data->psy_pdic = power_supply_register(parent, &_data->pdic_desc, &psy_cfg);
	if (IS_ERR(_data->psy_pdic)) {
		ret = (int)PTR_ERR(_data->psy_pdic);
		pr_err("%s: Failed to Register psy_pdic, ret : %d\n",
			__func__, ret);
	}
	return ret;

}
#endif

#if IS_ENABLED(CONFIG_OF)
static int of_s2mm006_usbpd_dt(struct device *dev,
			       struct s2mm006_data *usbpd_data)
{
	struct device_node *np = dev->of_node;
	const char * main_fw_name;
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	bool is_sink = false, is_source = false, is_try_snk = false;
#endif

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	is_sink = of_property_read_bool(np, "sink");
	is_source = of_property_read_bool(np, "source");
	is_try_snk = of_property_read_bool(np, "try_snk");

	if (is_sink && !is_source)
		usbpd_data->port_role = S2MM006_PORT_ROLE_SINK;
	else if (!is_sink & is_source)
		usbpd_data->port_role = S2MM006_PORT_ROLE_SOURCE;
	else if (is_sink && is_source) {
		if (is_try_snk)
			usbpd_data->port_role = S2MM006_PORT_ROLE_DRP_TRY_SNK;
		else
			usbpd_data->port_role = S2MM006_PORT_ROLE_DRP;
	}

	of_property_read_string(np, "s2mm006_pdic_name", &usbpd_data->name);
	if (of_property_read_string(np, "s2mm006_main_fw_name", &main_fw_name) == 0) {
		/* there is fw bin name in dtsi */
		usbpd_data->main_fw_name = main_fw_name;
	} else {
		usbpd_data->main_fw_name = S2MM006_FIRMWARE_BIN_DEFAULT_NAME;
	}

	pr_info("%s: sink(%d), source(%d), try_sink(%d) port_role(%d)\n", __func__, is_sink, is_source, is_try_snk, usbpd_data->port_role);
	pr_info("%s: main fw bin name: %s\n", __func__, usbpd_data->main_fw_name);
#endif

	usbpd_data->irq_gpio = of_get_named_gpio(np, "usbpd,usbpd_int", 0);
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	usbpd_data->irq_vbus_gpio = of_get_named_gpio(np, "usbpd,usbpd_vbus_int", 0);
	pr_info("%s:irq vbus gpio: %d, value: %d\n", __func__, usbpd_data->irq_vbus_gpio, gpio_get_value(usbpd_data->irq_vbus_gpio));
#endif
	usbpd_data->redriver_en = of_get_named_gpio(np, "usbpd,redriver_en", 0);

	usbpd_data->s2mm006_om = of_get_named_gpio(np, "usbpd,hrst_om", 0);
	pr_info("%s:hrst_om gpio value %d\n", __func__, gpio_get_value(usbpd_data->s2mm006_om));

	usbpd_data->vbus_discharging = of_get_named_gpio(np, "usbpd,vbus_discharging", 0);

	if (!s2mm006_is_pogo(usbpd_data)) {
		usbpd_data->otg_sw = of_get_named_gpio(np, "usbpd,otg_sw", 0);
		pr_info("%s: otg_sw gpio value %d\n", __func__, gpio_get_value(usbpd_data->otg_sw));

		usbpd_data->otg_buck_en = of_get_named_gpio(np, "usbpd,otg_buck_en", 0);
		pr_info("%s: otg_buck_en gpio value %d\n", __func__, gpio_get_value(usbpd_data->otg_buck_en));
	}

	usbpd_data->water_detect_support
		= of_property_read_bool(np, "usbpd,water_detect_support");

	usbpd_data->support_pd = of_property_read_bool(np, "support_pd_role_swap");

	pr_info("usbpd_irq=%d redriver_en=%d s2mm006_om=%d\n"
		"vbus_discharging=%d water=%d\n",
		usbpd_data->irq_gpio, usbpd_data->redriver_en, usbpd_data->s2mm006_om,
		usbpd_data->vbus_discharging,
		usbpd_data->water_detect_support);

	return 0;
}
#endif /* CONFIG_OF */

static void pdic_state_check_work(struct work_struct *wk)
{
	struct s2mm006_data *usbpd_data =
		container_of(to_delayed_work(wk), struct s2mm006_data, irq_work);

	pr_info("%s check state=%d, keep_irq=%d\n", __func__,
		usbpd_data->pdic_check_at_booting, usbpd_data->keep_irq);

	mutex_lock(&usbpd_data->irq_mutex);

	if (usbpd_data->pdic_check_at_booting ||
		usbpd_data->keep_irq)  {
		s2mm006_usbpd_irq_process(usbpd_data);
		usbpd_data->pdic_check_at_booting = 0;
		usbpd_data->keep_irq = false;
	}

	mutex_unlock(&usbpd_data->irq_mutex);
}

#if IS_ENABLED(CONFIG_S2MM006_POGO) && !IS_ENABLED(CONFIG_SEC_FACTORY)
static int s2mm006_bc1p2_disable(struct i2c_client *i2c)
{
	u8 reg_val = 0;
	int ret = 0;

	/* disable bc 1.2*/
	ret = s2mm006_read_byte(i2c, S2MM006_MUIC_REG_BC_CTRL1, &reg_val);
	if (ret < 0)
		goto err;

	reg_val &= BC_CTRL1_BC1P2_DISABLE_NMASK;
	ret = s2mm006_write_byte(i2c, S2MM006_MUIC_REG_BC_CTRL1, &reg_val);
	if (ret < 0)
		goto err;

	return ret; /* mV */
err:
	pr_err("%s. i2c error\n", __func__);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_MUIC_S2MM006)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
static void s2mm006_config_factory_bin_muic(struct s2mm006_data *usbpd_data) {
	struct i2c_client *i2c = usbpd_data->i2c;

	if (!s2mm006_is_pogo(usbpd_data)) {
		pr_info("%s: s2mm006 typec port muic init\n", __func__);
		usbpd_data->muic_data = s2mm006_muic_init(i2c);
	}
}
#else
static void s2mm006_config_user_bin_muic(struct s2mm006_data *usbpd_data) {
	struct i2c_client *i2c = usbpd_data->i2c;
	pr_info("%s: disable bc 1.2\n", __func__);
	/* typec bc 1.2 disable */
	s2mm006_bc1p2_disable(i2c);
}
#endif

static void s2mm006_config_muic(struct s2mm006_data *usbpd_data) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	s2mm006_config_factory_bin_muic(usbpd_data);
#else
	s2mm006_config_user_bin_muic(usbpd_data);
#endif
}
#endif

static int s2mm006_usbpd_probe(struct i2c_client *i2c,
			       const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct s2mm006_data *usbpd_data;
	int ret = 0;
	uint16_t REG_ADD;
	uint8_t MSG_BUF[32] = {0,};
	SINK_VAR_SUPPLY_Typedef *pSINK_MSG;
	MSG_HEADER_Typedef *pMSG_HEADER;
	uint32_t *MSG_DATA;
	uint8_t cnt;

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	CC_CTRL1_Type cc_ctrl1;
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct pp_ic_data *ic_data;
	ppdic_data_t ppdic_data;
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	ppdic_sysfs_property_t ppdic_sysfs_prop;
#endif
#endif

	pr_info("%s\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c functionality check error\n");
		return -EIO;
	}
	usbpd_data = devm_kzalloc(&i2c->dev, sizeof(struct s2mm006_data), GFP_KERNEL);
	if (!usbpd_data)
		return -ENOMEM;
#if IS_ENABLED(CONFIG_OF)
	if (i2c->dev.of_node)
		of_s2mm006_usbpd_dt(&i2c->dev, usbpd_data);
	else {
		pr_err("not found pdic dt! ret:%d\n", ret);
		return -ENODEV;
	}
#endif

	i2c_set_clientdata(i2c, usbpd_data);

	ret = register_gpio_pins(usbpd_data);
	if (ret)
		goto err_gpio;

	usbpd_data->irq = gpio_to_irq(usbpd_data->irq_gpio);
	pr_info("%s:IRQ NUM %d\n", __func__, usbpd_data->irq);

	usbpd_data->dev = &i2c->dev;
	usbpd_data->i2c = i2c;
	mutex_init(&usbpd_data->i2c_mutex);
	mutex_init(&usbpd_data->irq_mutex);

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (gpio_is_valid(usbpd_data->irq_vbus_gpio)) {
		usbpd_data->irq_vbus = gpio_to_irq(usbpd_data->irq_vbus_gpio);
		pr_info("%s:VBUS IRQ NUM %d\n", __func__, usbpd_data->irq_vbus);
	}
	else
		pr_info("%s: request VBUS IRQ NUM failed, %d\n", __func__, usbpd_data->irq_vbus);
#endif

	pr_info("%s: %s\n", __func__, usbpd_data->name);

	device_init_wakeup(usbpd_data->dev, 1);

	s2mm006_alternate_init(usbpd_data);

	s2mm006_init_val(usbpd_data);

	s2mm006_set_wake_lock(usbpd_data);

	pr_info("probed, irq %d\n", usbpd_data->irq_gpio);

	for (cnt = 0; cnt < 32; cnt++)
		MSG_BUF[cnt] = 0;

	REG_ADD = S2MM006_TX_SINK_CAPA;
	ret = s2mm006_read_bytes(i2c, REG_ADD, MSG_BUF, 32);
	if (ret < 0) {
		s2mm006_om_reset(usbpd_data);
		msleep(1000);
		ret = s2mm006_read_bytes(i2c, REG_ADD, MSG_BUF, 32);
		if (ret < 0) {
			pr_err("%s has i2c read error.\n", __func__);
			goto err_i2c;
		}
	}

	s2mm006_fw_check_update(usbpd_data);

	s2mm006_init_register(usbpd_data);

	MSG_DATA = (uint32_t *)&MSG_BUF[0];

	/* default value is written by PDIC FW. If you need others, overwrite it.*/
	pMSG_HEADER = (MSG_HEADER_Typedef *)&MSG_BUF[0];
	pMSG_HEADER->BITS.Number_of_obj += 1;
	pSINK_MSG = (SINK_VAR_SUPPLY_Typedef *)&MSG_BUF[8];
	pSINK_MSG->DATA = 0x8F019032;
	s2mm006_write_bytes(i2c, REG_ADD, &MSG_BUF[0], 32);

	for (cnt = 0; cnt < 32; cnt++)
		MSG_BUF[cnt] = 0;
	ret = s2mm006_read_bytes(i2c, REG_ADD, MSG_BUF, 32);

	pr_info("changed TX_SNK_CAPA [need 8f019032]\n");
	for (cnt = 0; cnt < 8; cnt++)
		pr_info("   0x%08X\n", MSG_DATA[cnt]);

	pr_info("%s: %d\n", __func__, __LINE__);
	usbpd_data->pd_contract = false;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	ic_data = devm_kzalloc(usbpd_data->dev,
			sizeof(struct pp_ic_data), GFP_KERNEL);
	if (!ic_data)
		goto err_pdic_policy;
	ic_data->dev = usbpd_data->dev;
	ic_data->p_ops = &s2mm006_ops;
	ic_data->pd_noti = &usbpd_data->pd_noti;
	ic_data->support_pd = usbpd_data->support_pd;
	ic_data->vbus_dischar_gpio = usbpd_data->vbus_discharging;
	ic_data->drv_data = (void *)usbpd_data;
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	/* driver case 1
	if (usbpd->port_role == S2MM006_PORT_ROLE_SINK)
		ic_data->is_pogo = 1;
	else
		ic_data->is_pogo = 0;*/
#endif
	usbpd_data->ic_data = ic_data;
	pr_info("%s\n", __func__);

	usbpd_data->pp_data = pdic_policy_init(ic_data);
	if (!usbpd_data->pp_data) {
		pr_info("%s: %d\n", __func__, __LINE__);
		goto err_policy_init;
	}
	pr_info("%s: %d\n", __func__, __LINE__);

	ppdic_data = devm_kzalloc(usbpd_data->dev, sizeof(pdic_data_t), GFP_KERNEL);
	pr_info("%s: %d\n", __func__, __LINE__);
	if (!ppdic_data)
		goto err_alloc;

	pr_info("%s: %d\n", __func__, __LINE__);
	ppdic_data->name = "s2mm006";
	ppdic_data->drv_data = usbpd_data;

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (!s2mm006_is_pogo(usbpd_data)) {
		pr_info("%s: %d\n", __func__, __LINE__);
		ret = s2mm006_pdic_sysfs_init(usbpd_data, &ppdic_sysfs_prop);
		if (ret)
			goto err_alloc;

		ppdic_data->pdic_sysfs_prop = ppdic_sysfs_prop;
		ret = pdic_core_register_chip(ppdic_data);

		if (ret)
			goto err_alloc;

		ppdic_data->set_enable_alternate_mode = s2mm006_set_enable_alternate_mode;
	}
#endif
	ppdic_data->fw_data.data = (void *)usbpd_data;
	ppdic_data->fw_data.firmware_update = s2mm006_firmware_update_callback;
	ppdic_data->fw_data.get_prev_fw_size = s2mm006_get_firmware_size;
	pr_info("%s: %d\n", __func__, __LINE__);

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (!s2mm006_is_pogo(usbpd_data)) {
		ret = pdic_misc_init(ppdic_data);
		if (ret)
			pr_err("pdic_misc_init is failed, error %d\n", ret);
		else
			usbpd_data->ppdic_data = ppdic_data;
	}
#endif
	pr_info("%s: %d\n", __func__, __LINE__);
#endif

#if IS_ENABLED(CONFIG_MUIC_S2MM006)
	s2mm006_config_muic(usbpd_data);
#endif

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_CC_CTRL1, &cc_ctrl1.BYTE);

	/* s2mm006_set_port_role */
	if (usbpd_data->port_role == S2MM006_PORT_ROLE_SINK)
		cc_ctrl1.BITS.PortRole = S2MM006_PORT_ROLE_SINK;
	else if (usbpd_data->port_role == S2MM006_PORT_ROLE_SOURCE)
		cc_ctrl1.BITS.PortRole = S2MM006_PORT_ROLE_SOURCE;
	else if (usbpd_data->port_role == S2MM006_PORT_ROLE_DRP_0uA)
		cc_ctrl1.BITS.PortRole = S2MM006_PORT_ROLE_DRP_0uA;
	else if (usbpd_data->port_role == S2MM006_PORT_ROLE_DRP)
		cc_ctrl1.BITS.PortRole = S2MM006_PORT_ROLE_DRP;
	else if (usbpd_data->port_role == S2MM006_PORT_ROLE_DRP_TRY_SNK) {
		cc_ctrl1.BITS.PortRole = S2MM006_PORT_ROLE_DRP;
		cc_ctrl1.BITS.TrySnkEn = 1;
	} else {
		pr_err("%s, invalid mode : %d\n", __func__, usbpd_data->port_role);
	}

	s2mm006_write_byte(usbpd_data->i2c, S2MM006_REG_CC_CTRL1, &cc_ctrl1.BYTE);

	s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_CC_CTRL1, &cc_ctrl1.BYTE);
	pr_info("%s: TrySnkEn: %x, PortRole %x\n", __func__, cc_ctrl1.BITS.TrySnkEn, cc_ctrl1.BITS.PortRole);
#endif

	pr_info("%s: %d\n", __func__, __LINE__);
	usbpd_data->pdic_check_at_booting = 1;
	INIT_DELAYED_WORK(&usbpd_data->irq_work, pdic_state_check_work);
	schedule_delayed_work(&usbpd_data->irq_work, msecs_to_jiffies(200));
	pr_info("%s: %d\n", __func__, __LINE__);

	pr_info("%s: %d\n", __func__, __LINE__);
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	pr_info("%s pdic type(%s)\n", __func__, usbpd_is_sink_str[s2mm006_is_pogo(usbpd_data)]);
#endif

	if (s2mm006_is_pogo(usbpd_data)) {
		ret = request_threaded_irq(usbpd_data->irq, NULL, s2mm006_usbpd_irq_thread,
			(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), "s2mm006-usbpd", usbpd_data);
	}
	else
		ret = request_threaded_irq(usbpd_data->irq, NULL, s2mm006_usbpd_irq_thread,
			(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), "s2mm006-usbpd_typec", usbpd_data);
		
	pr_info("%s: %d\n", __func__, __LINE__);
	if (ret) {
		pr_err("Failed to request IRQ %d, error %d\n", usbpd_data->irq, ret);
		goto err_init_irq;
	}
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (gpio_is_valid(usbpd_data->irq_vbus_gpio)) {
		pr_info("%s:%d irq_vbus_gpio is valid\n", __func__, __LINE__);
		if (s2mm006_is_pogo(usbpd_data))
			ret = request_threaded_irq(usbpd_data->irq_vbus, NULL, s2mm006_usbpd_vbus_irq_thread,
				(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), "s2mm006-usbpd-vbus", usbpd_data);
		else
			ret = request_threaded_irq(usbpd_data->irq_vbus, NULL, s2mm006_usbpd_vbus_irq_thread,
				(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), "s2mm006-usbpd-vbus_typec", usbpd_data);
		if (ret) {
			pr_err("Failed to request VBUS-IRQ %d, error %d\n", usbpd_data->irq_vbus, ret);
			goto err_init_irq;
		}
		
		ret = s2mm006_init_psy(usbpd_data, &i2c->dev);
		if (ret < 0)
			pr_err("Failed to register pdic psy\n");
	}
	else
		pr_info("%s:%d irq_vbus_gpio is invalid\n", __func__, __LINE__);
#endif

	pr_info("%s: %d\n", __func__, __LINE__);

	return ret;
err_init_irq:
	if (usbpd_data->irq)
		usbpd_data->irq = 0;

	cancel_delayed_work_sync(&usbpd_data->irq_work);

#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
	if (!s2mm006_is_pogo(usbpd_data)) {
		s2mm006_muic_remove(usbpd_data->muic_data);
	}
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (!s2mm006_is_pogo(usbpd_data)) {
		if (usbpd_data->ppdic_data && usbpd_data->ppdic_data->misc_dev)
			pdic_misc_exit();

		pdic_core_unregister_chip();
	}
#endif
err_alloc:
	pdic_policy_deinit(usbpd_data->ic_data);
err_policy_init:
err_pdic_policy:
#endif
err_i2c:
	s2mm006_alternate_deinit(usbpd_data);
	mutex_destroy(&usbpd_data->i2c_mutex);
	mutex_destroy(&usbpd_data->irq_mutex);

	unregister_gpio_pins(usbpd_data);
err_gpio:
	if (usbpd_data->i2c) {
		i2c_set_clientdata(usbpd_data->i2c, NULL);

		disable_irq_wake(usbpd_data->i2c->irq);
		free_irq(usbpd_data->i2c->irq, usbpd_data);
	}

	return ret;
}

static int s2mm006_usbpd_remove(struct i2c_client *i2c)
{
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	s2mm006_int_cc_detach(usbpd_data);

	if (usbpd_data->irq) {
		free_irq(usbpd_data->irq, usbpd_data);
		usbpd_data->irq = 0;
	}

	cancel_delayed_work_sync(&usbpd_data->irq_work);

#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
	if (!s2mm006_is_pogo(usbpd_data)) {
		s2mm006_muic_remove(usbpd_data->muic_data);
	}
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	if (!s2mm006_is_pogo(usbpd_data)) {
		if (usbpd_data->ppdic_data && usbpd_data->ppdic_data->misc_dev)
			pdic_misc_exit();

		pdic_core_unregister_chip();
	}
#endif
	pdic_policy_deinit(usbpd_data->ic_data);
#endif

	s2mm006_alternate_deinit(usbpd_data);
	mutex_destroy(&usbpd_data->i2c_mutex);
	mutex_destroy(&usbpd_data->irq_mutex);

	unregister_gpio_pins(usbpd_data);

	if (usbpd_data->i2c) {
		i2c_set_clientdata(usbpd_data->i2c, NULL);

		disable_irq_wake(usbpd_data->i2c->irq);
		free_irq(usbpd_data->i2c->irq, usbpd_data);
	}

	return 0;
}

static void s2mm006_usbpd_shutdown(struct i2c_client *i2c)
{
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	pr_info("%s: pd_state=%d, water=%d\n", __func__,
		usbpd_data->pd_state, usbpd_data->water_det);

	if (usbpd_data->irq)
		disable_irq(usbpd_data->irq);

#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
	if (!s2mm006_is_pogo(usbpd_data)) {
		s2mm006_muic_shutdown(usbpd_data->muic_data);
	}
#endif

	if (s2mm006_check_factory(usbpd_data) != S2MM006_USBID)
		s2mm006_om_reset(usbpd_data);
}

#if IS_ENABLED(CONFIG_PM)
static int s2mm006_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	pr_info("%s\n", __func__);

	if (usbpd_data->irq) {
		if (device_may_wakeup(dev))
			enable_irq_wake(usbpd_data->irq);
	}
#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
	if (!s2mm006_is_pogo(usbpd_data)) {
		s2mm006_muic_suspend(usbpd_data->muic_data);
	}
#endif
	if (delayed_work_pending(&usbpd_data->irq_work))
		cancel_delayed_work_sync(&usbpd_data->irq_work);
	usbpd_data->suspended = true;
	return 0;
}

static int s2mm006_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	pr_info("%s\n", __func__);

	usbpd_data->suspended = false;
#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
	if (!s2mm006_is_pogo(usbpd_data)) {
		s2mm006_muic_resume(usbpd_data->muic_data);
	}
#endif

	if (usbpd_data->irq) {
		if (device_may_wakeup(dev))
			disable_irq_wake(usbpd_data->irq);
	}

	if (usbpd_data->keep_irq) {
		pr_info("%s usbpd_data->keep_irq. run irq work\n",
			__func__);
		schedule_delayed_work(&usbpd_data->irq_work,
			msecs_to_jiffies(150));
	}
	return 0;
}
#else
#define s2mm006_suspend	NULL
#define s2mm006_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id s2mm006_usbpd_id[] = {
	{ USBPD006_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, s2mm006_usbpd_id);

#if IS_ENABLED(CONFIG_OF)
static const  struct of_device_id s2mm006_i2c_dt_ids[] = {
	{ .compatible = "slsi,s2mm006" },
	{ }
};
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
const struct dev_pm_ops s2mm006_pm = {
	.suspend = s2mm006_suspend,
	.resume = s2mm006_resume,
};
#endif /* CONFIG_PM */

static struct i2c_driver s2mm006_usbpd_driver = {
	.driver		= {
		.name	= USBPD006_DEV_NAME,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mm006_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mm006_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mm006_usbpd_probe,
	.remove		= s2mm006_usbpd_remove,
	.shutdown	= s2mm006_usbpd_shutdown,
	.id_table	= s2mm006_usbpd_id,
};

#if IS_ENABLED(CONFIG_S2MM006_POGO)
static const struct i2c_device_id s2mm006_usbpd_typec_id[] = {
	{ USBPD006_TYPEC_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, s2mm006_usbpd_typec_id);

static const  struct of_device_id s2mm006_typec_i2c_dt_ids[] = {
	{ .compatible = "slsi,s2mm006_typec" },
	{ }
};

static struct i2c_driver s2mm006_typec_usbpd_driver = {
	.driver		= {
		.name	= USBPD006_TYPEC_DEV_NAME,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mm006_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mm006_typec_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mm006_usbpd_probe,
	.remove		= s2mm006_usbpd_remove,
	.shutdown	= s2mm006_usbpd_shutdown,
	.id_table	= s2mm006_usbpd_typec_id,
};
#endif

static int __init s2mm006_usbpd_init(void)
{
	int ret = 0;

	pr_info("%s, power port usbpd driver, ++\n", __func__);
	ret = i2c_add_driver(&s2mm006_usbpd_driver);
	if (ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_S2MM006_POGO)
	pr_info("%s, typec usbpd driver, ++\n", __func__);
	ret = i2c_add_driver(&s2mm006_typec_usbpd_driver);
	if (ret < 0)
		return ret;
#endif
	return ret;
}
late_initcall(s2mm006_usbpd_init);

static void __exit s2mm006_usbpd_exit(void)
{
	i2c_del_driver(&s2mm006_usbpd_driver);
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	i2c_del_driver(&s2mm006_typec_usbpd_driver);
#endif
}
module_exit(s2mm006_usbpd_exit);

MODULE_DESCRIPTION("s2mm006 USB PD driver");
MODULE_LICENSE("GPL");

