/*
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
#define pr_fmt(fmt) "vbus_switch: " fmt

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#include "s2mm006.h"
#include "s2mm006_ext.h"
#include "s2mm006_muic.h"
#include "s2mm006_alternate.h"
#include "s2mm006_vbus_sw.h"

static struct Vbus_sw_gpio vbus_sw_gpio_data1 = {
	.vbus_sw_gpio_num = 107,
};
static struct Vbus_sw_s2mm006 vbus_sw_s2mm006_data2 = {
	.vbus_sw_ctrl_reg = 0x4E,
	.vbus_sw_status_reg = 0x1C,
};

static struct Vbus_sw_gpio vbus_sw_gpio_data3 = {
	.vbus_sw_gpio_num = 108,
};

static int Vbus_sw_gpio_init(void* data) {
	struct Vbus_sw_gpio* vbus_sw_gpio = (struct Vbus_sw_gpio*) data;
	pr_info("%s\n", __func__);

	if (!gpio_is_valid(vbus_sw_gpio->vbus_sw_gpio_num)) {
		pr_info("%s:%d, gpio not specified\n", __func__, __LINE__);
	} else{
		gpio_direction_output(vbus_sw_gpio->vbus_sw_gpio_num, 0); 
	}

	return 0;
}

static int Vbus_sw_gpio_on(void* data) {
	struct Vbus_sw_gpio* vbus_sw_gpio = (struct Vbus_sw_gpio*) data;
	pr_info("%s\n", __func__);
	gpio_direction_output(vbus_sw_gpio->vbus_sw_gpio_num, 1); 

	
	return 0;
}
static int Vbus_sw_gpio_off(void* data) {
	struct Vbus_sw_gpio* vbus_sw_gpio = (struct Vbus_sw_gpio*) data;
	pr_info("%s\n", __func__);
	gpio_direction_output(vbus_sw_gpio->vbus_sw_gpio_num, 0); 
	
	return 0;
}

static int Vbus_sw_s2mm006_init(void* data) {
	pr_info("%s\n", __func__);
	
	return 0;
}

static int Vbus_sw_s2mm006_on(void* data) {
	struct Vbus_sw_s2mm006* vbus_sw_s2mm006_data = (struct Vbus_sw_s2mm006*) data;
	struct i2c_client* i2c = vbus_sw_s2mm006_data->i2c;
	int ret = 0;
	u8 W_DATA = 0x22;
	pr_info("%s\n", __func__);

	if (IS_ERR(i2c)) {
		pr_err("%s pdic i2c is not present try again\n", __func__);
		ret = -ENODEV;
		goto out;
	}
	ret = s2mm006_write_byte(i2c, S2MM006_REG_VBUS_SW_CTRL, &W_DATA);
	if (ret < 0) {
		pr_err("%s pdic i2c err try again\n", __func__);
		return ret;
	}

	return 0;

out:
	return ret;
}
static int Vbus_sw_s2mm006_off(void* data) {
	struct Vbus_sw_s2mm006* vbus_sw_s2mm006_data = (struct Vbus_sw_s2mm006*) data;
	struct i2c_client* i2c = vbus_sw_s2mm006_data->i2c;
	int ret = 0;
	u8 W_DATA = 0;
	pr_info("%s\n", __func__);

	if (IS_ERR(i2c)) {
		pr_err("%s pdic i2c is not present try again\n", __func__);
		ret = -ENODEV;
		goto out;
	}
	ret = s2mm006_write_byte(i2c, S2MM006_REG_VBUS_SW_CTRL, &W_DATA);
	if (ret < 0) {
		pr_err("%s pdic i2c err try again\n", __func__);
		return ret;
	}

	return 0;

out:
	return ret;
}

static int Vbus_sw_s2mm006_status(void* data, void* val) {
	struct Vbus_sw_s2mm006* vbus_sw_s2mm006_data = (struct Vbus_sw_s2mm006*) data;
	struct i2c_client* i2c = vbus_sw_s2mm006_data->i2c;
	int ret = 0;
	uint8_t R_DATA = 0;

	pr_info("%s\n", __func__);

	if (IS_ERR(i2c)) {
		pr_err("%s pdic i2c is not present try again\n", __func__);
		ret = -ENODEV;
	}
	ret = s2mm006_read_byte(i2c, S2MM006_REG_VBUS_SW_STATUS, &R_DATA);
	if (ret < 0) {
		pr_err("%s pdic i2c err try again\n", __func__);
		return ret;
	}

	*((uint8_t*) val) = R_DATA;

	return 0;
}

// static int Vbus_sw_init_default(void* data) {
// 	pr_info("%s: there is no sw init func\n", __func__);
	
// 	return 0;
// }

// static int Vbus_sw_on_default(void* data) {
// 	pr_info("%s: there is no sw on func\n", __func__);
	
// 	return 0;
// }

// static int Vbus_sw_off_default(void* data) {
// 	pr_info("%s: there is no sw on func\n", __func__);
	
// 	return 0;
// }

static int Vbus_sw_status_default(void* data, void* val) {
	pr_info("%s: there is no sw on func\n", __func__);
	
	return 0;
}

static struct Vbus_sw_common static_vbus_sw_data[VBUS_SW_NUM] = {
	{
		.data = &vbus_sw_gpio_data1,
		.vbus_sw_init = Vbus_sw_gpio_init,
		.vbus_sw_on = Vbus_sw_gpio_on,
		.vbus_sw_off = Vbus_sw_gpio_off,
		.vbus_sw_status = Vbus_sw_status_default,
	},
	{
		.data = &vbus_sw_s2mm006_data2,
		.vbus_sw_init = Vbus_sw_s2mm006_init,
		.vbus_sw_on = Vbus_sw_s2mm006_on,
		.vbus_sw_off = Vbus_sw_s2mm006_off,
		.vbus_sw_status = Vbus_sw_s2mm006_status,
	},
	{
		.data = &vbus_sw_gpio_data3,
		.vbus_sw_init = Vbus_sw_gpio_init,
		.vbus_sw_on = Vbus_sw_gpio_on,
		.vbus_sw_off = Vbus_sw_gpio_off,
		.vbus_sw_status = Vbus_sw_status_default,
	},
};

int s2mm006_vbus_switch_init(struct i2c_client* usbpd_data_i2c)
{
	int ret = 0;
	int i = 0;
	pr_info("[VBUS_SW]: %s\n", __func__);

	if (IS_ERR(usbpd_data_i2c)) {
		pr_err("%s pdic i2c is not present try again\n", __func__);
		return -ENODEV;
	}

	vbus_sw_s2mm006_data2.i2c = usbpd_data_i2c;

	for(i = 0; i < VBUS_SW_NUM; ++i) {
		static_vbus_sw_data[i].vbus_sw_init(static_vbus_sw_data[i].data);
	}


	return ret;
}

int s2mm006_vbus_switch_on (int sw_num) {
	int ret = 0;

	if (sw_num < 0 || VBUS_SW_NUM <= sw_num) {
		pr_err("%s vbus sw num is not valid try again\n", __func__);
		return -ENODEV;
	}

	ret = static_vbus_sw_data[sw_num].vbus_sw_on(static_vbus_sw_data[sw_num].data);

	return ret;
}
EXPORT_SYMBOL(s2mm006_vbus_switch_on);

int s2mm006_vbus_switch_off (int sw_num) {
	int ret = 0;

	if (sw_num < 0 || VBUS_SW_NUM <= sw_num) {
		pr_err("%s vbus sw num is not valid try again\n", __func__);
		return -ENODEV;
	}

	ret = static_vbus_sw_data[sw_num].vbus_sw_off(static_vbus_sw_data[sw_num].data);

	return ret;
}
EXPORT_SYMBOL(s2mm006_vbus_switch_off);

int s2mm006_vbus_switch_status (int sw_num, void* val) {
	int ret = 0;

	if (sw_num < 0 || VBUS_SW_NUM <= sw_num) {
		pr_err("%s vbus sw num is not valid try again\n", __func__);
		return -ENODEV;
	}

	ret = static_vbus_sw_data[sw_num].vbus_sw_status(static_vbus_sw_data[sw_num].data, val);

	return ret;
}
EXPORT_SYMBOL(s2mm006_vbus_switch_status);

MODULE_AUTHOR("Samsung Corporation");
MODULE_DESCRIPTION("vbus switch");
