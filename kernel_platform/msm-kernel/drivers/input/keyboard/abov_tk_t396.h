/* abov_touchkey.h -- Linux driver for abov chip as touchkey
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef __LINUX_ABOV_TOUCHKEY_H
#define __LINUX_ABOV_TOUCHKEY_H

#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
#include "../sec_input/sec_input.h"
#endif

#define ABOV_TK_NAME	"a96t396xx"
#define ABOV_ID		0x40
#define MAX_KEY_NUM		3

extern int get_lcd_attached(char *);

struct abov_touchkey_platform_data {
	unsigned long irq_flag;
	int gpio_en;
	int gpio_int;
	int gpio_sda;
	int gpio_scl;
	int gpio_rst;
	int gpio_tkey_led_en;
	int gpio_seperated;
	struct regulator *vdd_io_vreg;
	struct regulator *avdd_vreg;
	struct regulator *dvdd_vreg;
	void (*input_event) (void *data);
	int (*power) (void *, bool on);
	int (*keyled) (bool on);
	char *fw_path;
	bool boot_on_ldo;
	int bringup;
	bool ta_notifier;
	bool support_fw_header;
};

#endif /* LINUX_ABOV_TOUCHKEY_H */

