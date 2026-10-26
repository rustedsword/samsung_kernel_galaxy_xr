// SPDX-License-Identifier: GPL-2.0+
/*
 * max77720.h - REGULATOR device driver for MAX77720
 * Copyright (C) 2019  Maxim Integrated.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LINUX_REGULATOR_MAX77720_H
#define __LINUX_REGULATOR_MAX77720_H

//#define MAX77720_REG_DEBUG_EN

#define LOG_TAG_MAX77720 "MAX77720:"
#define MAX_ERR(X, ...) pr_err("%s %s : "X, LOG_TAG_MAX77720, __func__, ## __VA_ARGS__)

#ifdef MAX77720_REG_DEBUG_EN
#define MAX_INFO(X, ...) pr_err("%s %s : "X, LOG_TAG_MAX77720, __func__, ## __VA_ARGS__)
#define MAX_DBG(X, ...) pr_err("%s %s : "X, LOG_TAG_MAX77720, __func__, ## __VA_ARGS__)
#else
#define MAX_INFO(X, ...) pr_info("%s %s : "X, LOG_TAG_MAX77720, __func__, ## __VA_ARGS__)
#define MAX_DBG(X, ...) pr_debug("%s %s : "X, LOG_TAG_MAX77720, __func__, ## __VA_ARGS__)
#endif

/* To Update Max77720 from outside*/
int max77720_write_reg(int addr, int data);

/* MAX77720 Registers */
enum {
	MAX77720_REG_DEVICE_ID,
	MAX77720_REG_STATUS,
	MAX77720_REG_CONFIG1,
	MAX77720_REG_CONFIG2,
	MAX77720_REG_VOUT,
	MAX77720_REG_INT_MASK,
	MAX77720_REG_INT,
};

/* MAX77720 Chip Variation */
enum {
	MAX77720_SUB_A_F = 1,
	MAX77720_SUB_B,
	MAX77720_SUB_C,
	MAX77720_SUB_D,
	MAX77720_SUB_E,
};

/* MAX77720 Regulator Option */
enum {
	MAX77720_ID_VOUT,
	MAX77720_ID_VOUT_H,
	MAX77720_MAX_REGULATORS,
};


#define MAX77720_MASK_CHIP_REV     (0x7 << 0)
#define MAX77720_MASK_VERSION	      (0xF << 3)

#define MAX77720_MASK_INT_OCP		(0x1 << 0)
#define MAX77720_MASK_INT_OVP		(0x1 << 1)
#define MAX77720_MASK_INT_POK		(0x1 << 2)
#define MAX77720_MASK_INT_THM		(0x1 << 3)

#define MAX77720_MASK_ST                (0xF)
#define MAX77720_MASK_ST_OCP        (0x1 << 0)
#define MAX77720_MASK_ST_OVP        (0x1 << 1)
#define MAX77720_MASK_ST_POK        (0x1 << 2)
#define MAX77720_MASK_ST_TSHDN    (0x1 << 3)

#define MAX77720_MASK_BB_EN          (0x1 << 6)
#define MAX77720_MASK_PD_EN          (0x1 << 5)
#define MAX77720_MASK_POK_POL      (0x1 << 4)

#define MAX77720_MASK_FPWM		(0x1 << 0)
#define MAX77720_MASK_AD                (0x1 << 0)
#define MAX77720_MASK_OVP_TH		(0x3 << 2)
#define MAX77720_MASK_RD_SR		(0x1 << 4)
#define MAX77720_MASK_RU_SR		(0x1 << 5)
#define MAX77720_MASK_ILIM             (0x3 << 6)

#define MAX77720_MASK_GPIO_CFG     (0x07)
#define MAX77720_MASK_VOUT		(0x7F)

#define MAX77720_VOUT_MIN_UV		11000000
#define MAX77720_VOUT_STEP_UV		20000

#define MAX77720_AD_DISABLE		0

#endif //__LINUX_REGULATOR_MAX77720_H

