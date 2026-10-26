/*
 * max77705.h - Driver for the Maxim 77705
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX34427_H__
#define __MAX34427_H__

#define MAX34427_REG_CTL		0x01
#define MAX34427_REG_ACC_CNT	0x02
#define MAX34427_REG_ACC_CH1	0x03
#define MAX34427_REG_ACC_CH2	0x04
#define MAX34427_REG_VOL_CH1	0x07
#define MAX34427_REG_VOL_CH2	0x08
#define MAX34427_REG_DEV_ID		0x0F	// Bit[7:3] Fixed at 0x09
#define MAX34427_REG_RATE		0x20
#define MAX34427_REG_PWRDN		0x21

#define MAX34427_CMD_UPDATE		0x00
#define MAX34427_CMD_CONTROL	0x01
#define MAX34427_CMD_ACC_COUNT	0x02
#define MAX34427_CMD_PWR_ACC_1	0x03
#define MAX34427_CMD_PWR_ACC_2	0x04
#define MAX34427_CMD_V_CH1		0x07
#define MAX34427_CMD_V_CH2		0x08
#define MAX34427_CMD_DID		0x0F
#define MAX34427_CMD_B_UPDATE	0x00
#define MAX34427_CMD_B_PWR_R	0x10
#define MAX34427_CMD_B_VOLT_R	0x11
#define MAX34427_CMD_RATE		0x20
#define MAX34427_CMD_PWRDN		0x21

#define POWER_FULL_SCALE		(1 << 30)
#define CURRENT_FULL_SCALE		(1 << 16)
#define VOLT_FULL_SCALE		(1 << 14)
#define VOLT_CORR			24

struct max34427_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex i2c_lock;
	struct max34427_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *vio;
};

struct max34427_platform_data {
	int pin_reset;
	u64 corr_current;
	u64 corr_power;
};

#endif /* __MAX34427_H__ */
