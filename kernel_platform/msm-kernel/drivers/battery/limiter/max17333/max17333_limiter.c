/*
 * Copyright (c) 2023 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mfd/max17333.h>
#include "max17333_limiter.h"

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_FUELGAUGE_MAX17333)
#define USE_STATUS_ALERT
#endif

#define MAX17333_CURRENT_STANDARD_SCALAR 15625
#define MAX17333_CURRENT_SCALAR(rsense) (10/rsense)

#define MAX17333_QSCALE_CURRENT_STEP(_x, _bit) (((_x) & 0x7) == _bit)
#define MAX17333_QSCALE_CURRENT(_x) (MAX17333_QSCALE_CURRENT_STEP(_x, 0x0) ? 25 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x1) ? 50 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x2) ? 100 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x3) ? 200 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x4) ? 250 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x5) ? 400 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x6) ? 500 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x7) ? 1000 : 25)

static char *chg_supplied_to[] = {
	"max17333-charger",
};

static char *chg_sub_supplied_to[] = {
	"max17333-charger-sub",
};

static char *max17333_battery_type[] = {
	"SUB",
	"MAIN",
};

//#define __lock(_me)    mutex_lock(&(_me)->lock)
//#define __unlock(_me)  mutex_unlock(&(_me)->lock)

static enum power_supply_property max17333_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static struct device_attribute max17333_chg_attrs[] = {
	MAX17333_CHG_ATTR(max17333_chg_current),
	MAX17333_CHG_ATTR(max17333_chg_en),
	MAX17333_CHG_ATTR(max17333_chg_voltage),
	MAX17333_CHG_ATTR(max17333_prechg_current),
	MAX17333_CHG_ATTR(max17333_prequal_voltage),
	MAX17333_CHG_ATTR(max17333_ship_en),
	MAX17333_CHG_ATTR(max17333_commsh_en),
	MAX17333_CHG_ATTR(max17333_chg_pckp),
};

static int max17333_set_chg_current(struct max17333_charger_data *charger, unsigned int curr);
static bool max17333_get_dnr_status(struct max17333_charger_data *charger);

static inline int max17333_raw_voltage_to_uvolts(u16 lsb)
{
	return lsb * 625 / 8; /* 78.125uV per bit */
}

static inline int max17333_raw_pckp_to_uvolts(u16 lsb)
{
	return lsb * 2500 / 8; /* 312.5uV per bit */
}

static inline int max17333_raw_current_to_uamps(struct max17333_charger_data *charger, int curr)
{
	return curr * 15625 / ((int)charger->rsense * 10);
}

#if IS_ENABLED(CONFIG_FUELGAUGE_MAX17333)
static void max17333_periodic_dump(struct max17333_charger_data *charger)
{
	int i, ret;
	char *str = NULL;
	u16 read_val = 0;
	int index = 0;
	u16 max17333_debug_shdw_reg[0x81];
	u16 max17333_debug_reg[0x80 + 6];

    u16 vg_trim_val;
    u16 cg_trim_val;
    u16 tg_trim_val;
    u16 hconfig2;
    u16 temp_raw;
    u16 volt_raw;
    u16 curr_raw;
    u16 hconfig;
    u16 vfocv;
	u16 hprotcfg;
	u16 hprotcfg2;

	max17333_debug_shdw_reg[0] = REG_I2CCMD_SHDW;
	for (i = 1; i < 0x81; i++) {
		max17333_debug_shdw_reg[i] = REG_NXTABLE0_SHDW + i - 1;
	}

	max17333_debug_reg[0x80] = 0x79;
	max17333_debug_reg[0x81] = 0x7A;
	max17333_debug_reg[0x82] = 0x7B;
	max17333_debug_reg[0x83] = 0x7C;
	max17333_debug_reg[0x84] = 0xF0;
	max17333_debug_reg[0x85] = 0xF1;

	index = 0;
	for (i = 0; i < 0x50; i++) {
		max17333_debug_reg[index] = i;
		index++;
	}
	for (i = 0xa0; i < 0xc0; i++) {
		max17333_debug_reg[index] = i;
		index++;
	}
	for (i = 0xd0; i < 0xe0; i++) {
		max17333_debug_reg[index] = i;
		index++;
	}

	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0; i < ARRAY_SIZE(max17333_debug_shdw_reg); i++) {
		ret = max17333_read(charger->regmap_shdw, max17333_debug_shdw_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}

	pr_info("[MAX17333][%s] NV : %s\n", max17333_battery_type[charger->battery_type], str);
	kfree(str);
	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	for (i = 0; i < ARRAY_SIZE(max17333_debug_reg); i++) {
		ret = max17333_read(charger->regmap, max17333_debug_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}

	pr_info("[MAX17333][%s] FG: %s\n", max17333_battery_type[charger->battery_type], str);
	kfree(str);

    max17333_read(charger->regmap, REG_VG_TRIM, &vg_trim_val);
    max17333_read(charger->regmap, REG_CG_TRIM, &cg_trim_val);
    max17333_read(charger->regmap, REG_TG_TRIM, &tg_trim_val);
    max17333_read(charger->regmap, REG_HCONFIG2, &hconfig2);
    max17333_read(charger->regmap, REG_TEMP_RAW, &temp_raw);
    max17333_read(charger->regmap, REG_VOLT_RAW, &volt_raw);
    max17333_read(charger->regmap, REG_CURR_RAW, &curr_raw);
    max17333_read(charger->regmap, REG_HCONFIG, &hconfig);
    max17333_read(charger->regmap, REG_VFOCV, &vfocv);
    max17333_read(charger->regmap, REG_HPROTCFG, &hprotcfg);
    max17333_read(charger->regmap, REG_HPROTCFG2, &hprotcfg2);

    pr_info("[MAX17333][%s] HW: %04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh\n", max17333_battery_type[charger->battery_type],
			vg_trim_val, cg_trim_val, tg_trim_val, hconfig2, temp_raw, volt_raw, curr_raw, hconfig, vfocv, hprotcfg, hprotcfg2);

	max17333_read(charger->regmap, REG_CONFIG, &read_val);
	if ((read_val & MAX17333_CONFIG_MANCHG) == 0x00) {
		pr_info("%s read_val = 0x%x, manual chg get back to 0\n", __func__, read_val);
		//panic("manual chg get back to 0");
	}
}
#else
static void max17333_periodic_dump(struct max17333_charger_data *charger)
{
	int i, ret;
	char *str = NULL;
	u16 read_val = 0;
	u16 max17333_debug_shdw_reg[13] = {
		REG_I2CCMD_SHDW,
		REG_NCONFIG_SHDW,
		REG_NCHGCTRL1_SHDW,
		REG_NICHGTERM_SHDW,
		REG_NCHGCFG0_SHDW,
		REG_NCHGCTRL0_SHDW,
		REG_NUVPRTTH_SHDW,
		REG_NTPRTTH1_SHDW,
		REG_NTPRTTH3_SHDW,
		REG_NPROTCFG_SHDW,
		REG_NDELAYCFG_SHDW,
		REG_NODSCTH_SHDW,
		REG_NCHGCFG2_SHDW,
	};
	u16 max17333_debug_reg[16] = {
		REG_STATUS,
		REG_ATRATE,
		REG_CONFIG,
		REG_VCELL,
		REG_CURRENT,
		REG_AVGCURRENT,
		REG_CHARGING_CURRENT,
		REG_CHARGING_VOLTAGE,
		REG_CHGSTAT,
		REG_CONFIG2,
		REG_PROTALRTS,
		REG_STATUS2,
		REG_PROTSTATUS,
		REG_PCKP,
		REG_HCHGTARG,
		REG_HCHGPWM,
	};

	u16 max17333_debug_hw_reg[2] = {
		REG_HPROTCFG,
		REG_HPROTCFG2
	};

	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!str)
		return;

	/* NV registers */
	for (i = 0; i < ARRAY_SIZE(max17333_debug_shdw_reg); i++) {
		ret = max17333_read(charger->regmap_shdw, max17333_debug_shdw_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}
	pr_info("[MAX17333][%s] NV : %s\n", max17333_battery_type[charger->battery_type], str);
	kfree(str);

	/* FG registers */
	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	for (i = 0; i < ARRAY_SIZE(max17333_debug_reg); i++) {
		ret = max17333_read(charger->regmap, max17333_debug_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}
	pr_info("[MAX17333][%s] FG: %s\n", max17333_battery_type[charger->battery_type], str);
	kfree(str);

	/* HW registers */
	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	for (i = 0; i < ARRAY_SIZE(max17333_debug_hw_reg); i++) {
		ret = max17333_read(charger->regmap, max17333_debug_hw_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}
	pr_info("[MAX17333][%s] HW: %s\n", max17333_battery_type[charger->battery_type], str);

	kfree(str);
}
#endif

static inline int max17333_raw_capacity_to_uamph(struct max17333_charger_data *charger,
												 int cap)
{
	return cap * 5000 / (int)charger->rsense;
}

static int max17333_get_charging_status(struct max17333_charger_data *charger)
{
	return 0;
}

static unsigned int max17333_raw_current_to_chgdata(struct max17333_charger_data *charger,
												 unsigned int curr)
{
	unsigned int current_val;

	current_val = (curr * 1000) / MAX17333_CURRENT_SCALAR(charger->rsense);
	current_val = (current_val * 100) / MAX17333_CURRENT_STANDARD_SCALAR;

	return current_val;
}

static unsigned int max17333_raw_voltage_to_voltdata(unsigned int volt)
{
	return ((volt * 8 * 100) / 625) * 10;
}

static int max17333_chgdata_to_raw_current(struct max17333_charger_data *charger,
u16 chgdata)
{
	int current_val;

	current_val = max17333_raw_current_to_uamps(charger, sign_extend32(chgdata, 15));
	/* Convert to milliamps */
	current_val /= 1000;

	return current_val;
}

static int max17333_set_allowchg(struct max17333_charger_data *charger)
{
	u16 config_val;
	int ret;

	/* restore chg current */
	charger->bc32_chg_block = false;
	max17333_set_chg_current(charger, charger->bc32_chg_curr);

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is allowed\n", __func__, max17333_battery_type[charger->battery_type]);
	config_val &= ~(MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static int max17333_set_blockchg(struct max17333_charger_data *charger)
{
	u16 config_val;
	int ret;

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Never block charging.\n", __func__, max17333_battery_type[charger->battery_type]);
	charger->bc32_chg_block = true;
	max17333_set_chg_current(charger, 0);
	config_val &= ~(MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);
	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

#define BC32_MIN_CHG_CURR 0
#define BC32_VALID_CHG_CURR 200
static int max17333_set_chg_current(struct max17333_charger_data *charger, unsigned int curr)
{
	u16 current_val;
	int ret;

	/* save chg curr */
	if (curr >= BC32_VALID_CHG_CURR)
		charger->bc32_chg_curr = curr;
	/* never block charging, overwrite charging current */
	if (charger->bc32_chg_block)
		curr = BC32_MIN_CHG_CURR;

	pr_info("%s[%s] : curr(%d)mA\n", __func__, max17333_battery_type[charger->battery_type], curr);

	current_val = (u16)max17333_raw_current_to_chgdata(charger, curr);

	pr_info("%s[%s] : charging Current (0x%x)\n", __func__, max17333_battery_type[charger->battery_type], current_val);

	ret = max17333_write(charger->regmap, REG_CHARGING_CURRENT, current_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_CHARGING_CURRENT\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static int max17333_get_chg_current(struct max17333_charger_data *charger)
{
	u16 val = 0;
	int ret = 0, curr = 0;

	ret = max17333_read(charger->regmap, REG_CHARGING_CURRENT, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_CURRENT\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	curr = max17333_chgdata_to_raw_current(charger, val);

	return curr;
}

static bool max17333_get_dnr_status(struct max17333_charger_data *charger)
{
	u16 read_data = 0, read_data2 = 0;
	int ret = 0;
	bool dnr_status = false;

	ret = max17333_read(charger->regmap, REG_FSTAT, &read_data);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_FSTAT\n", __func__, max17333_battery_type[charger->battery_type]);
		return dnr_status;
	}
	read_data = (read_data & 0x0001);

	ret = max17333_read(charger->regmap, REG_HCONFIG, &read_data2);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_HCONFIG\n", __func__, max17333_battery_type[charger->battery_type]);
		return dnr_status;
	}
	read_data2 = (read_data2 & 0x4000);

	if (read_data | read_data2) {
		/* Data is Ready, this value is only avaliable when BIT0 of REG_FSTAT is 0 and BIT14 of REG_HCONFIG is 0. */
		/* Otherwise Data is not Ready */
		pr_err("%s[%s]: Data is not Ready\n", __func__, max17333_battery_type[charger->battery_type]);
		dnr_status = true;
		//panic("ma17333 data is not ready");
	}

	return dnr_status;
}


static ssize_t max17333_get_chg_current_sysfs(struct max17333_charger_data *charger, char *buf)
{
	int rc = 0, curr = 0;

	curr = max17333_get_chg_current(charger);

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "CURRENT : %d mA\n", curr);

	return rc;
}

static int max17333_set_prechg_current(struct max17333_charger_data *charger, unsigned int curr)
{
	u16 current_val, val;
	int ret;
	int qscale;

	pr_info("%s[%s] : curr(%d)mA\n", __func__, max17333_battery_type[charger->battery_type], curr);

	ret = max17333_read(charger->regmap_shdw, REG_NDESIGNCAP_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	qscale = MAX17333_QSCALE_CURRENT(BITS_VALUE(val, MAX17333_DESIGNCAP_QSCALE)) / (charger->rsense);

	if ((curr/qscale) >= 1)
		current_val = (curr / qscale) - 1;
	else
		current_val = 0;

	if (current_val < 0 || current_val > 0x7) {
		pr_err("%s[%s]: value is out of range\n", __func__, max17333_battery_type[charger->battery_type]);
		return 0;
	}

	val = BITS_SET(val, MAX17333_CHGCFG0_PRECHGCURR, current_val);

	pr_info("%s[%s] : Prequal charging Voltage (0x%x)(0x%x)\n", __func__, max17333_battery_type[charger->battery_type], val, current_val);

	ret = max17333_write(charger->regmap_shdw, REG_NCHGCFG0_SHDW, val);
	if (ret < 0)
		pr_err("%s[%s]: fail to write REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);

	return ret;
}


static ssize_t max17333_get_prechg_current(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int curr;
	int qscale;

	ret = max17333_read(charger->regmap_shdw, REG_NDESIGNCAP_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	qscale = MAX17333_QSCALE_CURRENT(BITS_VALUE(val, MAX17333_DESIGNCAP_QSCALE)) / (charger->rsense);

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCFG0_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	curr = (BITS_VALUE(val, MAX17333_CHGCFG0_PRECHGCURR) + 1) * qscale;

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "PRECHGCURR : %d mA\n", curr);

	return rc;
}


static int max17333_set_chg_voltage(struct max17333_charger_data *charger, unsigned int voltage)
{
	u16 voltage_val;
	int ret;

	pr_info("%s[%s] : volt(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage);

	voltage_val = (u16)max17333_raw_voltage_to_voltdata(voltage);

	pr_info("%s[%s] : charging Voltage (0x%x)\n", __func__, max17333_battery_type[charger->battery_type], voltage_val);

	ret = max17333_write(charger->regmap, REG_CHARGING_VOLTAGE, voltage_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static ssize_t max17333_get_chg_voltage(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int volt;

	ret = max17333_read(charger->regmap, REG_CHARGING_VOLTAGE, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	volt = max17333_raw_voltage_to_uvolts(val);
	volt /= 1000;

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "VOLTAGE : %d mV\n", volt);

	return rc;
}

static int max17333_set_prequal_voltage(struct max17333_charger_data *charger, unsigned int voltage)
{
	u16 voltage_val, val;
	int ret;

	pr_info("%s[%s] : volt(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage);

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCFG0_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	voltage -= 2200;
	voltage_val = voltage / 40;

	if (voltage_val < 0 || voltage_val > 0x1F) {
		pr_err("%s[%s]: value is out of range\n", __func__, max17333_battery_type[charger->battery_type]);
		return 0;
	}

	val = BITS_SET(val, MAX17333_CHGCFG0_PREQUALVOLT, voltage_val);

	pr_info("%s[%s] : Prequal charging Voltage (0x%x)(0x%x)\n", __func__, max17333_battery_type[charger->battery_type], val, voltage_val);

	ret = max17333_write(charger->regmap_shdw, REG_NCHGCFG0_SHDW, val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static ssize_t max17333_get_prequal_voltage(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int volt;

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCFG0_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	volt = 2200 + (BITS_VALUE(val, MAX17333_CHGCFG0_PREQUALVOLT) * 40);

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "PREQUALVOLT : %d mV\n", volt);

	return rc;
}

static ssize_t max17333_get_chg_enable(struct max17333_charger_data *charger, char *buf)
{
	int rc = 0, ret;
	u16 val = 0;

	// Read Charging Status
	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s] REG_I2CCMD_SHDW Register reading failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
		return ret;
	}

	if ((!(val & MAX17333_I2CCMD_BLOCKCHG)) && (!(val & MAX17333_I2CCMD_CHGDONE)))
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "I2CCmd : Charging Enabled!\n");
	else
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "I2CCmd : Charging Disabled!\n");

	return rc;
}

static void max17333_set_chg_enable(struct max17333_charger_data *charger, bool on)
{
	int ret;

	if (on) {
		ret = max17333_set_allowchg(charger);
		if (ret < 0) {
			dev_err(charger->dev, "[%s] Charging control failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
			return;
		}
	} else {
		ret = max17333_set_blockchg(charger);
		if (ret < 0) {
			dev_err(charger->dev, "[%s] Charging control failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
			return;
		}
	}
}

static bool max17333_get_ship_enable(struct max17333_charger_data *charger)
{
	bool shipen = false;
	int ret;
	u16 val = 0;

	// Read Status Register
	ret = max17333_read(charger->regmap, REG_STATUS, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s] REG_STATUS Register reading failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
		return false;
	}

	if (val & BIT_STATUS_SHIPRDY)
		shipen = true;

	return shipen;
}

static void max17333_set_ship_enable(struct max17333_charger_data *charger, bool on)
{
	u16 config;
	int ret;

	ret = max17333_read(charger->regmap, REG_CONFIG, &config);
	if (ret < 0)
		return;

	if (on) {
		config |= MAX17333_CONFIG_SHIP;
		pr_info("%s: Enable shipmode enter\n", __func__);
	} else {
		config &= ~MAX17333_CONFIG_SHIP;
		pr_info("%s: Disable shipmode enter\n", __func__);
	}

	ret = max17333_write(charger->regmap, REG_CONFIG, config);
	if (ret < 0)
		return;
}

static bool max17333_get_commsh_enable(struct max17333_charger_data *charger)
{
	bool commshen = false;
	int ret;
	u16 val = 0;

	// Read Config Register
	ret = max17333_read(charger->regmap, REG_CONFIG, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s] REG_CONFIG Register reading failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
		return false;
	}

	if (val & MAX17333_CONFIG_COMMSH)
		commshen = true;

	return commshen;
}

static int max17333_get_inow(struct max17333_charger_data *charger, u8 mode)
{
	int ret, inow;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_CURRENT, &reg);
	if (ret < 0)
		return 0;

	inow = max17333_raw_current_to_uamps(charger, sign_extend32(reg, 15));
	if (mode == SEC_BATTERY_CURRENT_MA)
		inow = inow / 1000;

	return inow;
}

static int max17333_get_iavg(struct max17333_charger_data *charger, u8 mode)
{
	int ret, inow;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_AVGCURRENT, &reg);
	if (ret < 0)
		return 0;

	inow = max17333_raw_current_to_uamps(charger, sign_extend32(reg, 15));
	if (mode == SEC_BATTERY_CURRENT_MA)
		inow = inow / 1000;

	return inow;
}

static int max17333_get_vcell(struct max17333_charger_data *charger, u8 mode)
{
	int ret, vcell;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_VCELL, &reg);
	if (ret < 0)
		return ret;
	vcell = max17333_raw_voltage_to_uvolts(reg);
	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vcell = vcell / 1000;

	return vcell;
}

static int max17333_get_bat_avg_vcell(struct max17333_charger_data *charger, u8 mode)
{
	int ret, vcell;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_AVGVCELL, &reg);
	if (ret < 0)
		return ret;

	vcell = max17333_raw_voltage_to_uvolts(reg);
	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vcell = vcell / 1000;

	return vcell;
}

static ssize_t max17333_get_pckp(struct max17333_charger_data *charger, char *buf)
{
	int ret = 0, cnvt_val = 0, rc = 0;
	u16 val = 0;

	ret = max17333_read(charger->regmap, REG_PCKP, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_PCKP (%d)\n", __func__, max17333_battery_type[charger->battery_type], ret);
		return ret;
	}

	cnvt_val = max17333_raw_pckp_to_uvolts(val) / 1000;

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "%d\n", cnvt_val);

	return rc;
}

static int max17333_set_commsh_enable(struct max17333_charger_data *charger, bool on)
{
	u16 config;
	int ret;

	ret = max17333_read(charger->regmap, REG_CONFIG, &config);
	if (ret < 0)
		return ret;

	if (on) {
		config |= MAX17333_CONFIG_COMMSH;
		pr_info("%s: Commsh bit is Enabled\n", __func__);
	} else {
		config &= ~MAX17333_CONFIG_COMMSH;
		pr_info("%s: Commsh bit is Disabled\n", __func__);
	}

	ret = max17333_write(charger->regmap, REG_CONFIG, config);
	if (ret < 0)
		return ret;

	return ret;
}

static void max17333_permfail_check(struct max17333_charger_data *charger)
{
	u16 protstatus, battstatus;
	int ret;

	ret = max17333_read(charger->regmap, REG_PROTSTATUS, &protstatus);
	if (ret < 0)
		return;

	ret = max17333_read(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, &battstatus);
	if (ret < 0)
		return;

	if ((protstatus & BIT_PROTSTATUS_PERMFAIL) || (battstatus & BIT_NBATTSTATUS_PERMFAIL)) {
		pr_info("%s: PROTSTATUS PERMFAIL bit clear\n", __func__);
		ret = max17333_write(charger->regmap, REG_PROTSTATUS, (protstatus & BIT_PROTSTATUS_PERMFAIL_MASK));
		if (ret < 0)
			return;

		usleep_range(1000, 2000);

		ret = max17333_read(charger->regmap, REG_PROTSTATUS, &protstatus);
		if (ret < 0)
			return;

		if ((protstatus & BIT_PROTSTATUS_PERMFAIL))
			max17333_write(charger->regmap, REG_PROTSTATUS, (protstatus & BIT_PROTSTATUS_PERMFAIL_MASK));

		pr_info("%s: NBATTSTATUS PERMFAIL bit clear\n", __func__);
		ret = max17333_write(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, (battstatus & BIT_NBATTSTATUS_PERMFAIL_MASK));
		if (ret < 0)
			return;

		usleep_range(1000, 2000);

		ret = max17333_read(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, &battstatus);
		if (ret < 0)
			return;

		if ((battstatus & BIT_NBATTSTATUS_PERMFAIL))
			max17333_write(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, (battstatus & BIT_NBATTSTATUS_PERMFAIL_MASK));
	}

	return;
}

static int max17333_chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max17333_chg_attrs); i++) {
		rc = device_create_file(dev, &max17333_chg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max17333_chg_attrs[i]);
	return rc;
}

ssize_t max17333_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17333_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17333_chg_attrs;
	int i = 0;

	dev_info(charger->dev, "[%s]\n", max17333_battery_type[charger->battery_type]);

	switch (offset) {
	case MAX17333_CHG_CURRENT:
		i = max17333_get_chg_current_sysfs(charger, buf);
		break;
	case MAX17333_CHG_EN:
		i = max17333_get_chg_enable(charger, buf);
		break;
	case MAX17333_CHG_VOLTAGE:
		i = max17333_get_chg_voltage(charger, buf);
		break;
	case MAX17333_PRECHG_CURRENT:
		i = max17333_get_prechg_current(charger, buf);
		break;
	case MAX17333_PREQUAL_VOLTAGE:
		i = max17333_get_prequal_voltage(charger, buf);
		break;
	case MAX17333_SHIP_EN:
		if (max17333_get_ship_enable(charger))
			i = snprintf(buf, PAGE_SIZE, "Ship is Ready!\n");
		else
			i = snprintf(buf, PAGE_SIZE, "Ship is Not Ready!\n");
		break;
	case MAX17333_COMMSH_EN:
		if (max17333_get_commsh_enable(charger))
			i = snprintf(buf, PAGE_SIZE, "Commsh was Enabled!\n");
		else
			i = snprintf(buf, PAGE_SIZE, "Commsh was Disabled!\n");
		break;
	case MAX17333_CHG_PCKP:
		i = max17333_get_pckp(charger, buf);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t max17333_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17333_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17333_chg_attrs;
	int x, ret;

	dev_info(charger->dev, "[%s]\n", max17333_battery_type[charger->battery_type]);

	switch (offset) {
	case MAX17333_CHG_CURRENT:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] current = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_current(charger, x);
		}
		ret = count;
		break;
	case MAX17333_CHG_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] chg en = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_CHG_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] voltage = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_voltage(charger, x);
		}
		ret = count;
		break;
	case MAX17333_PRECHG_CURRENT:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] prechg current = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_prechg_current(charger, x);
		}
		ret = count;
		break;
	case MAX17333_PREQUAL_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] prequal voltage = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_prequal_voltage(charger, x);
		}
		ret = count;
		break;
	case MAX17333_SHIP_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] ship en = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_ship_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_COMMSH_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] commsh en = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_commsh_enable(charger, x);
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}

	max17333_periodic_dump(charger);

	return ret;
}

static int max17333_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max17333_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max17333_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = max17333_get_iavg(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max17333_get_inow(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = max17333_get_bat_avg_vcell(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17333_get_vcell(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = max17333_get_chg_current(charger);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		//case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
		//	val->intval = max17333_get_ship_enable(charger);
		//	break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			max17333_periodic_dump(charger);
			max17333_get_dnr_status(charger);
			break;
		default:
			return 0;
		}
		break;
	default:
		return 0;
	}

	max17333_permfail_check(charger);

	return 0;
}

static int max17333_charger_set_property(struct power_supply *psy,
								 enum power_supply_property psp,
								 const union power_supply_propval *val)
{
	struct max17333_charger_data *charger =
		power_supply_get_drvdata(psy);
	int ret = 0;
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		max17333_set_chg_voltage(charger, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		//case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
		//	max17333_set_ship_enable(charger, val->intval);
		//	break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == SEC_BAT_CHG_MODE_CHARGING)
				max17333_set_chg_enable(charger, 1);
			else
				max17333_set_chg_enable(charger, 0);
			break;
		default:
			return 0;
		}
		break;
	default:
		return 0;
	}

	return ret;
}

static int max17333_chg_property_is_writeable(struct power_supply *psy,
										  enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

#ifdef USE_STATUS_ALERT
static irqreturn_t max17333_protalrt_irq_isr(int irq, void *data)
{
	struct max17333_charger_data *charger = data;
	int ret;
	u16 val;

	dev_info(charger->dev, "[%s] Protection Interrupt Handler!\n", max17333_battery_type[charger->battery_type]);

	/* Check Protection Alert type */

	ret = max17333_read(charger->regmap, REG_PROTALRTS, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	if (val & BIT_CHGWDT_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Charge Watch Dog Timer!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_TOOHOTC_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overtemperature for Charging!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_FULL_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Full Detection!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_TOOCOLDC_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Undertemperature!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_OVP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overvoltage!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_OCCP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overcharge Current!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_QOVFLW_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Q Overflow!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_PERMFAIL_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Permanent Failure!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_DIEHOT_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overtemperature for die temperature!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_TOOHOTD_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overtemperature for Discharging!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_UVP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Undervoltage Protection!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_ODCP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overdischarge current!\n", max17333_battery_type[charger->battery_type]);

	/* Clear alerts */
	ret = max17333_write(charger->regmap, REG_PROTALRTS, val & REG_PROTALRTS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

static irqreturn_t max17333_alrt_irq_isr(int irq, void *data)
{
	struct max17333_charger_data *charger = data;
	int ret;
	u16 staus, chgstat;

    u16 vg_trim_val;
    u16 cg_trim_val;
    u16 tg_trim_val;
    u16 hconfig2;
    u16 temp_raw;
    u16 volt_raw;
    u16 curr_raw;
    u16 hconfig;
    u16 vfocv;

	dev_info(charger->dev, "[%s]STATUS : Interrupt Handler!\n", max17333_battery_type[charger->battery_type]);

	/* Check alert type */
	ret = max17333_read(charger->regmap, REG_STATUS, &staus);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_STATUS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	if (staus & BIT_STATUS_POR)
		dev_info(charger->dev, "[%s] Alert: POR Detect!\n", max17333_battery_type[charger->battery_type]);
	if (staus & BIT_STATUS_CA) {
		dev_info(charger->dev, "[%s] Alert: CHARGING!\n", max17333_battery_type[charger->battery_type]);

		/* Check Charging type */
		ret = max17333_read(charger->regmap, REG_CHGSTAT, &chgstat);

		if (chgstat & BIT_STATUS_CP)
			dev_info(charger->dev, "[%s] Charging Alert: Heat limit!\n", max17333_battery_type[charger->battery_type]);
		if (chgstat & BIT_STATUS_CT)
			dev_info(charger->dev, "[%s] Charging Alert: FET Temperature limit!\n", max17333_battery_type[charger->battery_type]);
		if (chgstat & BIT_STATUS_DROPOUT)
			dev_info(charger->dev, "[%s] Charging Alert: Dropout!\n", max17333_battery_type[charger->battery_type]);
	}

    max17333_read(charger->regmap, REG_VG_TRIM, &vg_trim_val);
    max17333_read(charger->regmap, REG_CG_TRIM, &cg_trim_val);
    max17333_read(charger->regmap, REG_TG_TRIM, &tg_trim_val);
    max17333_read(charger->regmap, REG_HCONFIG2, &hconfig2);
    max17333_read(charger->regmap, REG_TEMP_RAW, &temp_raw);
    max17333_read(charger->regmap, REG_VOLT_RAW, &volt_raw);
    max17333_read(charger->regmap, REG_CURR_RAW, &curr_raw);
	max17333_read(charger->regmap, REG_HCONFIG, &hconfig);
	max17333_read(charger->regmap, REG_VFOCV, &vfocv);

    pr_info("[MAX17333][%s] HW: %04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh\n", max17333_battery_type[charger->battery_type],
			vg_trim_val, cg_trim_val, tg_trim_val, hconfig2, temp_raw, volt_raw, curr_raw, hconfig, vfocv);

	/* Clear alerts */
	ret = max17333_write(charger->regmap, REG_STATUS, staus & REG_STATUS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_STATUS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}
#endif

static int max17333_charger_initialize(struct max17333_charger_data *charger)
{
	int ret = 0;

	ret = max17333_set_chg_voltage(charger, 4200);
	ret |= max17333_set_chg_current(charger, 500);
	ret |= max17333_set_commsh_enable(charger, 1);
#if defined(CONFIG_SEC_FACTORY)
	max17333_set_chg_enable(charger, 0);
#else
	max17333_set_chg_enable(charger, 1);
#endif
	return ret;
}

static int max17333_charger_probe(struct platform_device *pdev)
{
	struct max17333_dev *max17333 = dev_get_drvdata(pdev->dev.parent);
	struct max17333_charger_data *charger;
	int ret = 0;
	struct power_supply_config charger_cfg = {};

	pr_info("%s: MAX17333 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->lock);

	charger->dev = &pdev->dev;
	charger->max17333 = max17333;
	charger->regmap = max17333->regmap_pmic;
	charger->regmap_shdw = max17333->regmap_shdw;
	charger->rsense = max17333->pdata->rsense;
	charger->cycles_reg_lsb_percent = 25;
	charger->battery_type = max17333->pdata->battery_type;
	charger->bc32_chg_block = false;
	charger->bc32_chg_curr = 200;

	platform_set_drvdata(pdev, charger);
	if (charger->battery_type) {
		pr_info("%s: max17333 charger main power supply register\n", __func__);
		charger->psy_chg_d.name		= "max17333-charger";
		charger->psy_chg_d.type		= POWER_SUPPLY_TYPE_UNKNOWN;
		charger->psy_chg_d.get_property	= max17333_charger_get_property;
		charger->psy_chg_d.set_property	= max17333_charger_set_property;
		charger->psy_chg_d.property_is_writeable   = max17333_chg_property_is_writeable;
		charger->psy_chg_d.properties	= max17333_charger_props;
		charger->psy_chg_d.num_properties	=
			ARRAY_SIZE(max17333_charger_props);
		charger_cfg.drv_data = charger;
		charger_cfg.supplied_to = chg_supplied_to;
		charger_cfg.of_node = max17333->dev->of_node;
		charger_cfg.num_supplicants = ARRAY_SIZE(chg_supplied_to);
	} else {
		pr_info("%s: max17333 charger sub power supply register\n", __func__);
		charger->psy_chg_d.name		= "max17333-charger-sub";
		charger->psy_chg_d.type		= POWER_SUPPLY_TYPE_UNKNOWN;
		charger->psy_chg_d.get_property	= max17333_charger_get_property;
		charger->psy_chg_d.set_property	= max17333_charger_set_property;
		charger->psy_chg_d.property_is_writeable   = max17333_chg_property_is_writeable;
		charger->psy_chg_d.properties	= max17333_charger_props;
		charger->psy_chg_d.num_properties	=
			ARRAY_SIZE(max17333_charger_props);
		charger_cfg.drv_data = charger;
		charger_cfg.supplied_to = chg_sub_supplied_to;
		charger_cfg.of_node = max17333->dev->of_node;
		charger_cfg.num_supplicants = ARRAY_SIZE(chg_sub_supplied_to);
	}

	charger->psy_chg =
		devm_power_supply_register(max17333->dev,
				&charger->psy_chg_d,
				&charger_cfg);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("Couldn't register psy_chg rc=%ld\n",
				PTR_ERR(charger->psy_chg));
		goto error;
	}

#ifdef USE_STATUS_ALERT
	if (max17333->irq > 0) {
		ret = devm_request_threaded_irq(max17333->dev, max17333->irq, NULL,
			max17333_alrt_irq_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED,
			"alrt-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto error_alrt_irq;
		}
		dev_info(&pdev->dev, "MAX17333 irq requested %d\n", max17333->irq);
	}

	if (max17333->irqc_intsrc) {
		charger->prot_irq = regmap_irq_get_virq(max17333->irqc_intsrc,
			MAX17333_FG_PROT_INT);
		dev_info(&pdev->dev, "MAX17333 prot irq %d\n", charger->prot_irq);
	}

	if (charger->prot_irq > 0) {
		ret = request_threaded_irq(charger->prot_irq, NULL,
			max17333_protalrt_irq_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED,
			"protalrt-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto error_protalrt_irq;
		}
	}

	/* Enable alert on fuel-gauge outputs */
	max17333_update_bits(charger->regmap, REG_CONFIG, BIT_CONFIG_ALRT_EN, BIT_CONFIG_ALRT_EN);

	/* Clear alerts */
	max17333_write(charger->regmap, REG_PROTALRTS, REG_PROTALRTS_MASK);
	max17333_write(charger->regmap, REG_STATUS, REG_STATUS_MASK);
#endif

	ret = max17333_chg_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(charger->dev,
			"%s : Failed to create_attrs\n", __func__);
	}

	pr_info("%s: Charger Initialize\n", __func__);
	ret = max17333_charger_initialize(charger);
	if (ret < 0) {
		dev_err(charger->dev, "Error: Initializing Charger\n");
		goto error_init;
	}

	pr_info("%s: MAX17333 Charger Driver Loaded\n", __func__);
	return 0;

error_init:
#ifdef USE_STATUS_ALERT
	if (charger->prot_irq)
		free_irq(charger->prot_irq, charger);

error_protalrt_irq:
	if (max17333->irq)
		free_irq(max17333->irq, charger);

error_alrt_irq:
#endif
	power_supply_unregister(charger->psy_chg);

error:
	kfree(charger);
	return ret;
}

static int max17333_charger_remove(struct platform_device *pdev)
{
	struct max17333_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);
	/* this model can have charger during power off without LPM */
	max17333_set_chg_enable(charger, 0);
	kfree(charger);

	return 0;
}

static const struct platform_device_id max17333_charger_id[] = {
	{ "max17333-charger", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_charger_id);

static struct platform_driver max17333_charger_driver = {
	.driver = {
		.name = "max17333-charger",
	},
	.probe = max17333_charger_probe,
	.remove = max17333_charger_remove,
	.id_table = max17333_charger_id,
};

static const struct platform_device_id max17333_charger_sub_id[] = {
	{ "max17333-charger-sub", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_charger_sub_id);

static struct platform_driver max17333_charger_sub_driver = {
	.driver = {
		.name = "max17333-charger-sub",
	},
	.probe = max17333_charger_probe,
	.remove = max17333_charger_remove,
	.id_table = max17333_charger_sub_id,
};

static int __init max17333_limiter_init(void)
{
	pr_info("%s:\n", __func__);
	platform_driver_register(&max17333_charger_driver);
	platform_driver_register(&max17333_charger_sub_driver);
	return 0;
}

static void __exit max17333_limiter_exit(void)
{
	platform_driver_unregister(&max17333_charger_driver);
	platform_driver_unregister(&max17333_charger_sub_driver);
}

module_init(max17333_limiter_init);
module_exit(max17333_limiter_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX17333 Charger");
MODULE_VERSION(MAX17333_RELEASE_VERSION);