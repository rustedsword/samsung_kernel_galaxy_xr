/*
 *
 * $Id: stk6d2x.h
 *
 * Copyright (C) 2012~2018 Bk, sensortek Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#ifndef __STK68210_H__
#define __STK68210_H__

#include <linux/types.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define STK6D2X_DEV_NAME         "STK6D2X"

#define ALS_DEBUG_EN

#define LOG_TAG "[ALS_DBG]"
#define ALS_ERR(X, ...) pr_err("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)

#ifdef ALS_DEBUG_EN
#define ALS_INFO(X, ...) pr_err("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#define ALS_DBG(X, ...) pr_err("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#else
#define ALS_INFO(X, ...) pr_info("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#define ALS_DBG(X, ...) pr_debug("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#endif

enum stk6d2x_chip_id {
	STK68210,
	STK68310,
};

struct stk6d2x_platform_data {
	const char *name;
	u8 device_control;
	u8 initial_brightness;
	unsigned int period_ns;
	int size_program;
	unsigned int led_en_val_odd;
	unsigned int led_en_val_even;
	unsigned int led_en_val_all;
	unsigned int led_en_val_off;
};

struct stk6d2x {
	const char* chipname;
	enum stk6d2x_chip_id chip_id;
	struct i2c_client* client;
	struct device* dev;
	struct stk6d2x_platform_data* pdata;
	//struct regulator* supply;	/* regulator for VDD input */
	//struct regulator* enable;	/* regulator for EN/VDDIO input */
	u32 set_param;
	u32 force_trigger;
	u8 slave_index;
	u8 pattern_data;
	u8 brightness_data;
	u8 vboostrange_data;
	u8 pwm_direct_mode_ctrl_data;
	u8 vboost_max_data;
	u8 led_fault_th_data;
	u8 pwm_direct_mode_current_data;
	u8 led_current_data;
	u8 device_ctrl_data;
	u8 bl_on_off_flag;
	struct mutex 		mutex;
}; 



#endif // __STK68210_H__
