/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  vbus switch header
 *
 * Copyright (C) 2011-2022 Samsung, Inc.
 * Author: Sangdo Lee <sangdoto.lee@samsung.com>
 *
 */

 /* usb notify layer v3.7 */

#ifndef __LINUX_VBUS_SW_H__
#define __LINUX_VBUS_SW_H__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_qos.h>
#include <linux/i2c.h>

#define VBUS_SW_NUM 3

struct Vbus_sw_common {
	void* data;
	int (*vbus_sw_init) (void* data);
	int (*vbus_sw_on) (void* data);
	int (*vbus_sw_off) (void* data);
	int (*vbus_sw_status) (void* data, void* val);
};

struct Vbus_sw_gpio {
	int vbus_sw_gpio_num;
};

struct Vbus_sw_s2mm006 {
	struct i2c_client *i2c;
	int vbus_sw_ctrl_reg;
	int vbus_sw_status_reg;
};

#endif /* __LINUX_USB_NOTIFY_H__ */
