#ifndef __MAX77831_H__
#define __MAX77831_H__
/*
** =============================================================================
** MAX77831 Booster IC
** File:
**     max77831.h
**
** Description:
**     Header file for max77831.c
**
** =============================================================================
*/
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/input.h>
#include "tmc5271_ss.h"

struct max77831_drvdata {
	struct device	*dev;
	struct i2c_client	*booster_client;
	struct regulator *reg_boost_vio;
	int step_boost_en;
	int step_boost_intb;
};


//BOOSTER DATA
struct max77831_booster_const
{
	int voltage;
	u8 reg_val;
};

static const struct max77831_booster_const max77831_BoosterVolReg[] =
{
	{ 35, 0x3D }, // REG_3.5V
	{ 40, 0x44 }, // REG_4V
	{ 45, 0x4D }, // REG_4.5V
	{ 50, 0x56 }, // REG_5V
	{ 55, 0x5E }, // REG_5.5V
	{ 60, 0x67 }, // REG_6V
	{ 65, 0x6F }, // REG_6.5V
	{ 70, 0x78 }, // REG_7V
	{ 75, 0x80 }, // REG_7.5V
	{ 80, 0x89 }, // REG_8V
	{ 85, 0x91 }, // REG_8.5V
	{ 90, 0x9A }, // REG_9V
};

#define INT_MASK_REG 0x11
#define VOLTAGE_CHG	0x13
#define BOOSTER_OCP	0x10
#define REV_REG	0x80
#define BOOST_V_DIV_FACTOR 17

/* exported function prototypes */
int max77831_i2c_write(struct i2c_client *client, u8 addr, u8 val);
int max77831_i2c_read(struct i2c_client *client, u8 addr);
#endif
