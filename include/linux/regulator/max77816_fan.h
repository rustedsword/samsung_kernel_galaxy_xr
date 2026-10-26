// SPDX-License-Identifier: GPL-2.0+
/*
 * max77816_fan.h - REGULATOR device driver for MAX77816 for FAN
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
#ifndef __LINUX_REGULATOR_MAX77816_FAN_H
#define __LINUX_REGULATOR_MAX77816_FAN_H

/* MAX77816 Registers for Fan*/
enum {
	MAX77816_FAN_REG_DEVICE_ID,
	MAX77816_FAN_REG_STATUS,
	MAX77816_FAN_REG_CONFIG1,
	MAX77816_FAN_REG_CONFIG2,
	MAX77816_FAN_REG_VOUT,
	MAX77816_FAN_REG_VOUT_H,
	MAX77816_FAN_REG_INT_MASK,
	MAX77816_FAN_REG_INT,
};

/* MAX77816 Chip Variation for FAN*/
enum {
	MAX77816_FAN_SUB_A_F = 1,
	MAX77816_FAN_SUB_B,
	MAX77816_FAN_SUB_C,
	MAX77816_FAN_SUB_D,
	MAX77816_FAN_SUB_E,
};

/* MAX77816 Regulator Option for FAN*/
enum {
	MAX77816_FAN_ID_VOUT,
	MAX77816_FAN_ID_VOUT_H,
	MAX77816_FAN_MAX_REGULATORS,
};


#define MAX77816_FAN_MASK_CHIP_REV		(0x7 << 0)
#define MAX77816_FAN_MASK_VERSION		(0xf << 3)

#define MAX77816_FAN_MASK_INT_OCP		(0x1 << 0)
#define MAX77816_FAN_MASK_INT_OVP		(0x1 << 1)
#define MAX77816_FAN_MASK_INT_POK		(0x1 << 2)
#define MAX77816_FAN_MASK_INT_THM		(0x1 << 3)

#define MAX77816_FAN_MASK_ST		(0xf)
#define MAX77816_FAN_MASK_ST_OCP		(0x1 << 0)
#define MAX77816_FAN_MASK_ST_OVP		(0x1 << 1)
#define MAX77816_FAN_MASK_ST_POK		(0x1 << 2)
#define MAX77816_FAN_MASK_ST_TSHDN		(0x1 << 3)

#define MAX77816_FAN_MASK_BB_EN		(0x1 << 6)
#define MAX77816_FAN_MASK_PD_EN		(0x1 << 5)
#define MAX77816_FAN_MASK_POK_POL		(0x1 << 4)

#define MAX77816_FAN_MASK_FPWM		(0x1 << 0)
#define MAX77816_FAN_MASK_AD		(0x1 << 0)
#define MAX77816_FAN_MASK_OVP_TH		(0x3 << 2)
#define MAX77816_FAN_MASK_RD_SR		(0x1 << 4)
#define MAX77816_FAN_MASK_RU_SR		(0x1 << 5)
#define MAX77816_FAN_MASK_ILIM		(0x3 << 6)

#define MAX77816_FAN_MASK_GPIO_CFG		(0x07)
#define MAX77816_FAN_MASK_VOUT		(0x7f)
#define MAX77816_FAN_MASK_VOUT_H		(0x7f)

#define MAX77816_FAN_VOUT_MIN_UV		2600000
#define MAX77816_FAN_VOUT_STEP_UV		20000

#define MAX77816_FAN_VOUT_H_MIN_UV		2600000
#define MAX77816_FAN_VOUT_H_STEP_UV		20000

#define MAX77816_FAN_AD_DISABLE		0

#endif //__LINUX_REGULATOR_MAX77816_FAN_H

