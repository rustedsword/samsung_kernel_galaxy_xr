// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "[sec_fan] MAX31760:" fmt

#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include "sec_fan.h"

/*
 * MAX31760 registers.
 * Indentation helps identify how these constants apply:
 *      Register number.
 *          Per-register bit fields.
 *              Values for multi-bit fields.
 */
#define MAX31760_REG_CR1   0x00 /* Control Register 1 */
#define     MAX31760_CR1_TIS    0x01 /* Temperature Index Source */
#define     MAX31760_CR1_MTI    0x02 /* Maximum Temperature Index */
#define     MAX31760_CR1_PPS    0x04 /* PWM Polarity */
#define     MAX31760_CR1_DRV    0x18 /* PWM Frequency */
#define         MAX31760_DRV_33HZ   0x00
#define         MAX31760_DRV_150HZ  0x08
#define         MAX31760_DRV_1500HZ 0x10
#define         MAX31760_DRV_25KHZ  MAX31760_CR1_DRV
#define     MAX31760_CR1_HYST   0x20 /* Lookup Table Hysteresis: 2C or 4C */
#define     MAX31760_CR1_POR    0x40 /* Software Power-On Reset */
#define     MAX31760_CR1_ALTMSK 0x80 /* Alert Mask */
#define MAX31760_REG_CR2   0x01 /* Control Register 2 */
#define     MAX31760_CR2_DFC    0x01 /* Direct Fan Control */
#define     MAX31760_CR2_FSST   0x02 /* Fan Sense Signal Type */
#define     MAX31760_CR2_RDPS   0x04 /* RD Polarity Selection */
#define     MAX31760_CR2_FSEN   0x08 /* FS Input Enable */
#define     MAX31760_CR2_FFMODE 0x10 /* FF Functionality Selection */
#define     MAX31760_CR2_SPEN   0x20 /* Spin-up Enable */
#define     MAX31760_CR2_ALERTS 0x40 /* Alerts Functionality Selection */
#define     MAX31760_CR2_STBY   0x80 /* Standby Mode Enable */
#define MAX31760_REG_CR3   0x02 /* Control Register 3 */
#define     MAX31760_CR3_TACH1E 0x01 /* Tachometer 1 Enable */
#define     MAX31760_CR3_TACH2E 0x02 /* Tachometer 2 Enable */
#define     MAX31760_CR3_PSEN   0x04 /* Pulse Stretch Enable */
#define     MAX31760_CR3_TACHFL 0x08 /* Fan Fail When 100% Duty Cycle Only */
#define     MAX31760_CR3_RAMP   0x30 /* PWM Duty-Cycle Ramp Rate */
#define         MAX31760_RAMP_SLOW 0x00
#define         MAX31760_RAMP_SMED 0x10
#define         MAX31760_RAMP_MEDF 0x20
#define         MAX31760_RAMP_FAST MAX31760_CR3_RAMP
#define     MAX31760_CR3_FF_0   0x40 /* 0 Duty-Cycle Fan-Fail Detection */
#define     MAX31760_CR3_CLR_FF 0x80 /* Clear Fan Fail */
#define MAX31760_REG_FFDC  0x03 /* Fan Fault Duty Cycle */
#define MAX31760_REG_MASK  0x04 /* Alert Mask Register */
#define 	MAX31760_REG_MASK_DEFAULT 0xC0 /* Alert Mask Default */
#define     MAX31760_MASK_TACH1AM 0x01 /* TACH1 Alarm Mask */
#define     MAX31760_MASK_TACH2AM 0x02 /* TACH2 Alarm Mask */
#define     MAX31760_MASK_ROTAM   0x04 /* Remote Overtemperature Alarm Mask */
#define     MAX31760_MASK_RHAM    0x08 /* Remote High Temperature Alarm Mask */
#define     MAX31760_MASK_LOTAM   0x10 /* Local Overtemperature Alarm Mask */
#define     MAX31760_MASK_LHAM    0x20 /* Local High Temperature Alarm Mask */
#define MAX31760_REG_IFR   0x05 /* Ideality Factor Register */
#define     MAX31760_IFR_MASK 0x3f /* Mask for value of the IFR */
#define MAX31760_REG_RHSH  0x06 /* Remote High Set-point MSB */
#define MAX31760_REG_RHSL  0x07 /* Remote High Set-point LSB */
#define MAX31760_REG_LOTSH 0x08 /* Local Overtemperature Set-point MSB */
#define MAX31760_REG_LOTSL 0x09 /* Local Overtemperature Set-point LSB */
#define MAX31760_REG_ROTSH 0x0a /* Remote Overtemperature Set-point MSB */
#define MAX31760_REG_ROTSL 0x0b /* Remote Overtemperature Set-point LSB */
#define MAX31760_REG_LHSH  0x0c /* Local High Set-point MSB */
#define MAX31760_REG_LHSL  0x0d /* Local High Set-point LSB */
#define MAX31760_REG_TCTH  0x0e /* TACH Count Threshold Register, MSB */
#define MAX31760_REG_TCTL  0x0f /* TACH Count Threshold Register, LSB */
#define MAX31760_REG_USER  0x10 /* 8 bytes General Purpose User Memory */
#define MAX31760_REG_USER0 0x10 /* Custom Control Register USER0 */
#define     MAX31760_USER0_PULSE1 0x07 /* Fan1 Pulses per revolution */
#define     MAX31760_USER0_PULSE2 0x38 /* Fan2 Pulses per revolution */
#define MAX31760_REG_LUT   0x20 /* 48-Byte Lookup Table (LUT) */
#define     MAX31760_LUT_COUNT 48
#define MAX31760_REG_PWMR  0x50 /* Direct Duty-Cycle Control Register */

#define MAX31760_REG_PWMV  0x51 /* Current PWM Duty-Cycle Register */
#define MAX31760_REG_TC1H  0x52 /* TACH1 Count Register, MSB */
#define MAX31760_REG_TC1L  0x53 /* TACH1 Count Register, LSB */
#define MAX31760_REG_TC2H  0x54 /* TACH2 Count Register, MSB */
#define MAX31760_REG_TC2L  0x55 /* TACH2 Count Register, LSB */
#define MAX31760_REG_RTH   0x56 /* Remote Temperature Reading Register, MSB */
#define MAX31760_REG_RTL   0x57 /* Remote Temperature Reading Register, LSB */
#define MAX31760_REG_LTH   0x58 /* Local Temperature Reading Register, MSB */
#define MAX31760_REG_LTL   0x59 /* Local Temperature Reading Register, LSB */
#define MAX31760_REG_SR	   0x5a /* Status Register */
#define     MAX31760_SR_TACH1A 0x01 /* TACH1 Alarm */
#define     MAX31760_SR_TACH2A 0x02 /* TACH2 Alarm */
#define     MAX31760_SR_ROTA   0x04 /* Remote Overtemperature Alarm */
#define     MAX31760_SR_RHA    0x08 /* Remote High Temperature Alarm */
#define     MAX31760_SR_LOTA   0x10 /* Local Overtemperature Alarm */
#define     MAX31760_SR_LHA    0x20 /* Local High Temperature Alarm */
#define     MAX31760_SR_RDFA   0x40 /* Remote Diode Fault Alarm */
#define     MAX31760_SR_PC     0x80 /* Program Corrupt Bit */

#define MAX31760_REG_EEX   0x5b /* Load EEPROM to RAM; Write RAM to EEPROM */
#define     MAX31760_EEX_LW    0x80 /* Load from or write to EEPROM */
#define     MAX31760_EEX_BLKS  0x1F /* Blocks to load/write */

#define MAX31760_TEMP_MIN_MC -40000   /* Minimum Millicelcius */
#define MAX31760_TEMP_MAX_MC 127875   /* Maximum Millicelcius */
#define MAX31760_TEMP_HIGH_HYST 1000  /* 1C hysteresis on high temp alarms. */
#define MAX31760_TEMP_OVER_HYST 10000 /* 10C hysteresis on over temp alarms. */
#define MAX31760_LUT_HYST_CLEAR 2000  /* LUT hysteresis: bit clear. */
#define MAX31760_LYT_HYST_THRESH 3000 /* LUT hysteresis: store threshold. */
#define MAX31760_LUT_HYST_SET 4000    /* LUT hysteresis: bit set. */

#define MAX31760_NUM_TEMPS 2          /* Number of temperature sensors. */
#define MAX31760_NUM_FANS 2           /* Number of fans. */
#define MAX31760_FAN_PULSES_DEF 2     /* Default number of fan pulses. */
#define MAX31760_FAN_PULSES_MAX 8     /* Maximum number of fan pulses. */
#define MAX31760_PWM_ENABLE_FULL 0    /* pwmX_enable: Set PWM at full power. */
#define MAX31760_PWM_ENABLE_MANUAL 1  /* pwmX_enable: Set manual mode. */
#define MAX31760_PWM_ENABLE_AUTO 2    /* pwmX_enable: Set automatic mode. */

#define MAX31760_LUT_AUTO_ATTRS 3     /* Number of LUT auto-point attributes. */
#define MAX31760_LUT_AUTO_ATTR_COUNT (MAX31760_LUT_COUNT * \
				      MAX31760_LUT_AUTO_ATTRS)
#define MAX31760_LUT_NAME_SIZE 32     /* Fit: pwm1_auto_pointXX_temp_hyst\0. */

#define I2C_RETRY_CNT	3

/* Use the below flag to test your driver before the board arrives */
//#define IC_NAME_DUMMY_I2C_TESTING

struct max31760_drvdata {
	struct sec_fan_drvdata sec_fan_ddata;
	struct i2c_client *client;
	struct regulator *fan_rotor_supply;
	struct regulator *fan_drv_chip_supply;
	struct mutex i2c_lock;
	u32 fan_error_irq;
	int fan_pulses;
	int irq_gpio;
	int buck_en;
	bool init_done;
	struct delayed_work	debug_work;
};

#ifndef IC_NAME_DUMMY_I2C_TESTING
static inline int write_u8_to_i2c(struct i2c_client *client, u8 i2c_reg, u8 input)
{
	struct max31760_drvdata *max31760 = i2c_get_clientdata(client);
	int ret, i;

	mutex_lock(&max31760->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_byte_data(client, i2c_reg, input);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				 __func__, i2c_reg, ret, i + 1, I2C_RETRY_CNT);
		// Add 10ms delay to ensure reliability
		usleep_range(10000, 11000);
	}
	mutex_unlock(&max31760->i2c_lock);
	if (ret < 0)
		pr_err("%s reg(0x%x) failed.\n", __func__, i2c_reg);

	return ret;
}

static int read_u8_from_i2c(struct i2c_client *client, u8 i2c_reg, u8 *output)
{
	struct max31760_drvdata *max31760 = i2c_get_clientdata(client);
	int ret, i;

	mutex_lock(&max31760->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(client, i2c_reg);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), i2c_retry_cnt(%d/%d)\n",
				__func__, i2c_reg, i + 1, I2C_RETRY_CNT);
		// Add 10ms delay to ensure reliability
		usleep_range(10000, 11000);
	}
	mutex_unlock(&max31760->i2c_lock);
	if (ret < 0) {
		pr_err("%s reg(0x%x) failed.\n", __func__, i2c_reg);
		return ret;
	}
	ret &= 0xff;
	*output = ret;
	return 0;
}

static int read_u16_from_i2c(struct i2c_client *client, u8 i2c_reg,
				u16 *output)
{
	struct max31760_drvdata *max31760 = i2c_get_clientdata(client);
	u16 val;
	int i;

	mutex_lock(&max31760->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		val = i2c_smbus_read_word_swapped(client,  i2c_reg);
		if (val >= 0)
			break;
		pr_info("%s reg(0x%x), val(0x%x), i2c_retry_cnt(%d/%d)\n",
			__func__, i2c_reg, val, i + 1, I2C_RETRY_CNT);
		// Add 10ms delay to ensure reliability
		usleep_range(10000, 11000);
	}
	if (val < 0) {
		pr_err("%s reg(0x%x), val(0x%x)\n",	__func__, i2c_reg, val);
		mutex_unlock(&max31760->i2c_lock);
		return val;
	}
	mutex_unlock(&max31760->i2c_lock);

	*output = val;
	return 0;
}
#else // dummy I2C Read
static inline int write_u8_to_i2c(struct i2c_client *client, u8 i2c_reg, u8 input)
{
	pr_info("%s: i2c_reg=%d, input=%d\n", __func__, i2c_reg, input);
}

static int read_u8_from_i2c(struct i2c_client *client, u8 i2c_reg, u8 *output)
{
	*output = 1;
	pr_info("%s: i2c_reg=%d\n", __func__, i2c_reg);
	return 0;
}

static void read_fan_from_i2c(struct i2c_client *client, u16 *output,
				u8 hi_addr, u8 lo_addr)
{
	*output = 1;
	pr_info("%s: hi_addr=%d, lo_addr=%d\n", __func__, hi_addr, lo_addr);
}
#endif //IC_NAME_DUMMY_I2C_TESTING

int max31760_set_step(struct device *dev, int step, int step_data)
{
	struct max31760_drvdata *max31760 = dev_get_drvdata(dev);
	int err = 0;
	pr_info("%s: new_step = %d and step_data = %d\n", __func__, step, step_data);

	if (step_data) {
		err = write_u8_to_i2c(max31760->client, MAX31760_REG_CR2, MAX31760_CR2_DFC);
		if (err  < 0) {
			pr_err("%s: failed MAX31760_REG_CR2 disable standby mode\n", __func__);
			return err;
		}
	}

	err = write_u8_to_i2c(max31760->client, MAX31760_REG_PWMR, step_data);
	if (err < 0) {
		pr_err("%s: failed MAX31760_REG_PWMR set step_data = %d\n", __func__, step_data);
		return err;
	}

	if (!step_data) {
		err = write_u8_to_i2c(max31760->client, MAX31760_REG_CR2, MAX31760_CR2_DFC | MAX31760_CR2_STBY);
		if (err  < 0) {
			pr_err("%s: failed MAX31760_REG_CR2 enable standby mode\n", __func__);
			return err;
		}
	}

	cancel_delayed_work_sync(&(max31760->debug_work));
	schedule_delayed_work(&(max31760->debug_work), msecs_to_jiffies(8000));
	pr_info("%s done.\n", __func__);
	return err;
}

void max31760_enable_irq(struct device *dev)
{
	struct max31760_drvdata *max31760 = dev_get_drvdata(dev);
	pr_info("set %s\n", __func__);
	enable_irq(max31760->fan_error_irq);

}

void max31760_disable_irq(struct device *dev)
{
	struct max31760_drvdata *max31760 = dev_get_drvdata(dev);
	pr_info("set %s\n", __func__);
	disable_irq(max31760->fan_error_irq);
}


int max31760_set_power_off(struct device *dev)
{
	struct max31760_drvdata *max31760 = dev_get_drvdata(dev);

	int err = 0;

	pr_info("%s +++\n", __func__);

	if(max31760->fan_rotor_supply) {
		if(regulator_is_enabled(max31760->fan_rotor_supply)) {
			err = regulator_disable(max31760->fan_rotor_supply);
			if (err) {
				pr_err("%s Fan Rotor Supply regulator disable failed\n", __func__);
				return err;
			}
		} else
			pr_info("%s Fan Rotor Supply regulator already disabled.\n", __func__);
	}
	gpio_set_value(max31760->buck_en, 0);
	if(regulator_is_enabled(max31760->fan_drv_chip_supply)) {
		err = regulator_disable(max31760->fan_drv_chip_supply);
		if (err) {
			pr_err("%s Fan Drv IC Supply regulator disable failed, %d\n", __func__);
			return err;
		}
	} else
		pr_info("%s Fan Drv IC Supply regulator already disabled.\n", __func__);

	pr_info("%s --- done\n", __func__);
	return err;
}

/*
 * max31760_init - max31760 Boot time configurations
 */
int max31760_init(struct device *dev)
{
	struct max31760_drvdata *max31760 = dev_get_drvdata(dev);

	int err = 0;

	pr_info("%s +++\n", __func__);

	err = regulator_enable(max31760->fan_drv_chip_supply);
	if (err) {
		pr_err("%s Fan Drv IC Supply regulator enable failed\n", __func__);
		goto init_err;
	}
	// NOTE: when the regulator is enabled, it can create a glitch in PWM signal, provides a enough delay to prevent the motor stall
	msleep(100);
	gpio_set_value(max31760->buck_en, 1);
	if(max31760->fan_rotor_supply) {
		err = regulator_enable(max31760->fan_rotor_supply);
		if (err) {
			pr_err("%s Fan Rotor Supply regulator enable failed\n", __func__);
			goto init_err;
		}
	}
	msleep(100);
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_CR1, MAX31760_CR1_DRV | MAX31760_CR1_ALTMSK);
	if (err  < 0) {
		pr_err("%s Failed to write to CR1 register\n", __func__);
		goto init_err;
	}
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_PWMR, 0x0);
	if (err  < 0) {
		pr_err("%s Failed to write to PWMR register\n", __func__);
		goto init_err;
	}
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_CR2, MAX31760_CR2_DFC | MAX31760_CR2_STBY);
	if (err  < 0) {
		pr_err("%s Failed to write to CR2 register\n", __func__);
		goto init_err;
	}
#if defined(CONFIG_SEC_FACTORY)
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_CR3, MAX31760_CR3_TACH1E | MAX31760_CR3_TACH2E | MAX31760_RAMP_FAST);
#else
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_CR3, MAX31760_CR3_TACH1E | MAX31760_CR3_TACH2E | MAX31760_RAMP_MEDF);
#endif

	if (err  < 0) {
		pr_err("%s Failed to write to CR2 register\n", __func__);
		goto init_err;
	}
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_FFDC, max31760->sec_fan_ddata.pdata->step_map[SEC_FAN_STEP_DEFAULT]);
	if (err  < 0) {
		pr_err("%s Failed to write to FFDC register\n", __func__);
		goto init_err;
	}
	err = write_u8_to_i2c(max31760->client, MAX31760_REG_MASK, MAX31760_REG_MASK_DEFAULT | MAX31760_MASK_ROTAM | MAX31760_MASK_LOTAM | MAX31760_MASK_LHAM);
	if (err  < 0) {
		pr_err("%s Failed to write to MASK register\n", __func__);
		goto init_err;
	}
	err = max31760_set_step(dev, SEC_FAN_STEP_DEFAULT, max31760->sec_fan_ddata.pdata->step_map[SEC_FAN_STEP_DEFAULT]);
	if(err)
		pr_err("%s step %d setting failed\n", __func__, SEC_FAN_STEP_DEFAULT);
	else
		pr_info("%s step %d setting done\n", __func__, SEC_FAN_STEP_DEFAULT);

	pr_info("%s --- done\n", __func__);
	return err;

init_err:
	pr_err("%s initialization failed\n", __func__);
	return err;
}

/**
 * max31760_get_rpm - calculate rpm with given tachometer reading
 **/
long max31760_calc_rpm(u16 tach, int pulses)
{
	return 60L * 100000L / (long)tach / (long)pulses;
}

/**
 * max31760_get_rpm - reads tachometer and calculate the rpm
 **/
int max31760_get_rpm1(struct max31760_drvdata *ddata)
{
	int rpm = 0;
	u16 tach = 0;
	int err = 0;

	err = read_u16_from_i2c(ddata->client, MAX31760_REG_TC1H, &tach);
	if (err < 0)
		return err;
	if (tach == 0) {
		pr_err("%s: Can't read RPM, tach=%hu\n", __func__, tach);
		return -EAGAIN;
	}
	rpm = max31760_calc_rpm(tach, ddata->fan_pulses);

	pr_info("%s: rpm=%d, tach=%hu, fan_pulses=%d\n", __func__, rpm, tach, ddata->fan_pulses);

	return rpm;
}

int max31760_get_curr_rpm1(struct device *dev)
{
	return max31760_get_rpm1(dev_get_drvdata(dev));
}


int max31760_get_rpm2(struct max31760_drvdata *ddata)
{
	int rpm = 0;
	u16 tach = 0;
	int err = 0;

	err = read_u16_from_i2c(ddata->client, MAX31760_REG_TC2H, &tach);
	if (err < 0)
		return err;
	if (tach == 0) {
		pr_err("%s: Can't read RPM, tach=%hu\n", __func__, tach);
		return -EAGAIN;
	}
	rpm = max31760_calc_rpm(tach, ddata->fan_pulses);

	pr_info("%s: rpm=%d, tach=%hu, fan_pulses=%d\n", __func__, rpm, tach, ddata->fan_pulses);

	return rpm;
}

int max31760_get_curr_rpm2(struct device *dev)
{
	return max31760_get_rpm2(dev_get_drvdata(dev));
}

int max31760_get_pwm(struct device *dev)
{
	struct sec_fan_drvdata *sec_fan_ddata;
	struct max31760_drvdata *max31760;
	int ret = 0;
	u8 pwm_V = 0;

	if (!dev) {
		pr_err("%s: device is NULL\n", __func__);
		return -EINVAL;
	}

	sec_fan_ddata = dev_get_drvdata(dev);
	if (!sec_fan_ddata) {
		pr_err("%s: sec_fan_drvdata is NULL\n", __func__);
		return -EINVAL;
	}

	max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	if (!max31760->client) {
		pr_err("%s: i2c client is NULL\n", __func__);
		return -EINVAL;
	}

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_PWMV, &pwm_V);
	if (ret < 0) {
		pr_err("%s error in reading pwmv register\n", __func__);
		return ret;
	}
	pr_info("%s: pwm_V=%d\n", __func__, pwm_V);
	return pwm_V;
		
}

struct sec_fan_ops max31760_sec_fan_ops = {
	.set_step = max31760_set_step,
	.get_curr_rpm = max31760_get_curr_rpm1,
	.get_curr_rpm2 = max31760_get_curr_rpm2,
	.suspend = max31760_set_power_off,
	.resume = max31760_init,
	.enable_irq = max31760_enable_irq,
	.disable_irq = max31760_disable_irq,
	.get_pwm = max31760_get_pwm,
};

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static u8 reg_arr[] = {
	MAX31760_REG_CR1,
	MAX31760_REG_CR2,
	MAX31760_REG_CR3,
	MAX31760_REG_FFDC,
	MAX31760_REG_MASK,
	MAX31760_REG_PWMR,
	MAX31760_REG_PWMV,
	MAX31760_REG_TC1H,
	MAX31760_REG_TC1L,
	MAX31760_REG_TC2H,
	MAX31760_REG_TC2L,
	MAX31760_REG_SR,
};

/**
 * reg_show - sysfs read interface to show all register value
 **/
static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);
	int i, len = sprintf(buf, "Register   Value\n");

	for (i = 0; i < ARRAY_SIZE(reg_arr); i++) {
		read_u8_from_i2c(max31760->client, reg_arr[i], &val);
		len += sprintf(buf + len, "  0x%x      0x%x\n", reg_arr[i], val);
	}

	return len;
}

/**
 * reg_store - sysfs write interface to update given register by
 *      desired value.
 *      Accessible only on non-ship binaries.
 **/
static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int reg = 0, regval = 0;
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	if (sscanf(buf, "%x %x", &reg, &regval) != 2)
		return -EINVAL;
	if (reg > 256 || regval > 256)
		return -EINVAL;

	write_u8_to_i2c(max31760->client, (u8)reg, (u8)regval);

	return len;
}
static DEVICE_ATTR_RW(reg);


/**
 * tune_pwm_duty_ratio_show - sysfs read interface to show current pwm duty ratio
 **/
static ssize_t tune_pwm_duty_ratio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val_R = 0, val_V = 0;
	int ret = 0;
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_PWMR, &val_R);
	if (ret < 0) {
		pr_err("%s error in reading pwm register PWMR\n", __func__);
		return ret;
	}

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_PWMV, &val_V);
	if (ret < 0) {
		pr_err("%s error in reading pwm register PWMV\n", __func__);
		return ret;
	}

	return sprintf(buf, "duty ratio : %d(%d)\n", val_R, val_V);
}

/**
 * tune_pwm_duty_ratio_store - sysfs write interface to update duty ratio
 *      Accessible only on non-ship binaries.
 **/
static ssize_t tune_pwm_duty_ratio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int duty_ratio = 0;
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	if (kstrtoint(buf, 0, &duty_ratio))
		return -EINVAL;

	if (duty_ratio > 256)
		return -EINVAL;

	write_u8_to_i2c(max31760->client, MAX31760_REG_PWMR, (u8)(duty_ratio * 0xFF / 100));

	return len;
}
static DEVICE_ATTR_RW(tune_pwm_duty_ratio);

/**
 * fan_rotor_regulator_show - sysfs read interface to show current set voltage.
 * Accessible only on non-ship binaries.
 **/
static ssize_t fan_rotor_regulator_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	return sprintf(buf, "fan rotor regulator: %duV\n", regulator_get_voltage(max31760->fan_rotor_supply));
}

/**
 * fan_rotor_regulator_store - sysfs write interface to update fan rotor voltage.
 *      Accessible only on non-ship binaries.
 **/
static ssize_t fan_rotor_regulator_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret, rotor_reg = 0;
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	if (kstrtoint(buf, 0, &rotor_reg))
		return -EINVAL;

	if (rotor_reg < 2600000 || rotor_reg > 5140000)
		return -EINVAL;
	ret = regulator_set_voltage(max31760->fan_rotor_supply, rotor_reg, rotor_reg);
	if (ret) {
		pr_err("%s error in setting voltage\n", __func__);
		return ret;
	}
	return len;
}
static DEVICE_ATTR_RW(fan_rotor_regulator);

/**
 * speed_show - sysfs read interface to show fan speed related parameters
 **/
static ssize_t speed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 pwm_R = 0, pwm_V = 0;
	u16 tach1, tach2 = 0;
	int ret = 0, rpm1, rpm2 = 0;
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);

	pr_info("start! %s\n", __func__);

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_PWMR, &pwm_R);
	if (ret < 0) {
		pr_err("%s error in reading pwmr register\n", __func__);
		return ret;
	}

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_PWMV, &pwm_V);
	if (ret < 0) {
		pr_err("%s error in reading pwmv register\n", __func__);
		return ret;
	}

	ret = read_u16_from_i2c(max31760->client, MAX31760_REG_TC1H, &tach1);
	if (ret < 0) {
		pr_err("%s error in reading tach1 register\n", __func__);
		return ret;
	}

	rpm1 = max31760_calc_rpm(tach1, max31760->fan_pulses);

	ret = read_u16_from_i2c(max31760->client, MAX31760_REG_TC2H, &tach2);
	if (ret < 0) {
		pr_err("%s error in reading tach2 register\n", __func__);
		return ret;
	}

	rpm2 = max31760_calc_rpm(tach2, max31760->fan_pulses);

	return sprintf(buf, "tach1_count=%d, rpm1=%d, tach2_count=%d, rpm2=%d, fan_pulses=%d, pwm=%d(%d)\n",
		tach1, rpm1, tach2, rpm2, max31760->fan_pulses, pwm_R, pwm_V);
}
static DEVICE_ATTR_RO(speed);

/**
 * pwm_store - sysfs write interface to update pwm value
 *      Accessible only on non-ship binaries.
 **/
static ssize_t pwm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);
	int pwm = 0;

	if (kstrtoint(buf, 0, &pwm))
		return -EINVAL;

	if (pwm > 256)
		return -EINVAL;

	write_u8_to_i2c(max31760->client, MAX31760_REG_PWMR, pwm);
	pr_info("%s:pwm = %d\n", __func__, pwm);

	return len;
}
static DEVICE_ATTR_WO(pwm);

static ssize_t regulator_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);
	int en, err = 0;

	if (kstrtoint(buf, 0, &en))
		return -EINVAL;

	if (en > 1)
		return -EINVAL;
	pr_info("%s:regulator = %d\n", __func__, en);
	if(en==1) {
		err = regulator_enable(max31760->fan_drv_chip_supply);
		if (err) {
			pr_err("%s Fan Drv IC Supply regulator enable failed\n", __func__);
		}
		if(max31760->fan_rotor_supply) {
			err = regulator_enable(max31760->fan_rotor_supply);
			if (err) {
				pr_err("%s Fan Rotor Supply regulator enable failed\n", __func__);
			}
		}
	}

	else {
		err = regulator_disable(max31760->fan_drv_chip_supply);
		if (err) {
			pr_err("%s Fan Drv IC Supply regulator disable failed\n", __func__);
		}
		if(max31760->fan_rotor_supply) {
			err = regulator_disable(max31760->fan_rotor_supply);
			if (err) {
				pr_err("%s Fan Rotor Supply regulator disable failed\n", __func__);
			}
		}
	}
	return len;
}

static DEVICE_ATTR_WO(regulator);

static ssize_t gpio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct sec_fan_drvdata *sec_fan_ddata = dev_get_drvdata(dev);
	struct max31760_drvdata *max31760 = container_of(sec_fan_ddata, struct max31760_drvdata, sec_fan_ddata);
	int en = 0;

	if (kstrtoint(buf, 0, &en))
		return -EINVAL;

	if (en > 1)
		return -EINVAL;

	if(en==1) {
		gpio_set_value(max31760->buck_en, 1);
	}

	else {
		gpio_set_value(max31760->buck_en, 0);
	}

	return len;
}

static DEVICE_ATTR_WO(gpio);


static struct attribute *max31760_debug_dev_attrs[] = {
	&dev_attr_reg.attr,
	&dev_attr_tune_pwm_duty_ratio.attr,
	&dev_attr_fan_rotor_regulator.attr,
	&dev_attr_speed.attr,
	&dev_attr_pwm.attr,
	&dev_attr_regulator.attr,
	&dev_attr_gpio.attr,
	NULL,
};

static struct attribute_group max31760_debug_dev_attr_group = {
	.name = "max31760",
	.attrs = max31760_debug_dev_attrs,
};
#endif //CONFIG_SAMSUNG_PRODUCT_SHIP

static struct attribute_group *max31760_dev_attr_groups[] = {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	&max31760_debug_dev_attr_group,
#endif //CONFIG_SAMSUNG_PRODUCT_SHIP
	NULL
};

static void max31760_print_reg_log(struct work_struct *work)
{
	struct max31760_drvdata *max31760 = container_of(work, struct max31760_drvdata, debug_work.work);

	u8 val = 0;

	if (!max31760->init_done)
		pr_info("max31760_init is not done\n");

	read_u8_from_i2c(max31760->client, MAX31760_REG_CR2, &val);
	pr_info("CR2(0x%x) = 0x%x\n", MAX31760_REG_CR2, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_CR3, &val);
	pr_info("CR3(0x%x) = 0x%x\n", MAX31760_REG_CR3, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_PWMR, &val);
	pr_info("PWMR(0x%x) = 0x%x\n", MAX31760_REG_PWMR, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_PWMV, &val);
	pr_info("PWMV(0x%x) = 0x%x\n", MAX31760_REG_PWMV, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_TC1H, &val);
	pr_info("TC1H(0x%x) = 0x%x\n", MAX31760_REG_TC1H, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_TC1L, &val);
	pr_info("TC1L(0x%x) = 0x%x\n", MAX31760_REG_TC1L, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_TC2H, &val);
	pr_info("TC2H(0x%x) = 0x%x\n", MAX31760_REG_TC2H, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_TC2L, &val);
	pr_info("TC2L(0x%x) = 0x%x\n", MAX31760_REG_TC2L, val);

	read_u8_from_i2c(max31760->client, MAX31760_REG_SR, &val);
	pr_info("SR(0x%x) = 0x%x\n", MAX31760_REG_SR, val);

	schedule_delayed_work(&(max31760->debug_work),	msecs_to_jiffies(60000));
}

/**
 * max31760_parse_dt - parses hw details from dts
 **/
static int max31760_parse_dt(struct device *dev, struct max31760_drvdata *ddata)
{
	struct device_node *np;
	int ret = 0;

	pr_info("%s +++\n", __func__);
	np = dev->of_node;
	if (unlikely(!np)) {
		pr_err("%s err: could not parse dt node\n", __func__);
		return -ENODEV;
	}
	/* Parse HW configurations */
	ddata->fan_rotor_supply = regulator_get_exclusive(dev, "vout");
	if (IS_ERR(ddata->fan_rotor_supply)) {
		ret = PTR_ERR(ddata->fan_rotor_supply);
		pr_info("%s This device does not use a Fan Rotor Supply regulator.\n", __func__);
		ddata->fan_rotor_supply = NULL;
	}
	else {
		ret = regulator_set_voltage(ddata->fan_rotor_supply, 3600000, 3600000);
		if (unlikely(ret)) {
			pr_err("%s error in setting voltage in Rotor Supply regulator, 3600000\n", __func__);
			goto err_reg;
		}
	}
	ddata->fan_drv_chip_supply = regulator_get(dev, "pmxr2230_l17");
	if (IS_ERR(ddata->fan_drv_chip_supply)) {
		ret = PTR_ERR(ddata->fan_drv_chip_supply);
		pr_err("%s FAN DRV CHIP regulator_get failed\n", __func__);
		goto err_reg;
	}

	ret = regulator_set_voltage(ddata->fan_drv_chip_supply, 3300000, 3300000);
	if (unlikely(ret)) {
		pr_err("%s error in setting voltage in Fan Drv Chip regulator, 3300000\n", __func__);
		goto err_reg;
	}

	ddata->irq_gpio = of_get_named_gpio(np, "irq_gpio", 0);
	if (!gpio_is_valid(ddata->irq_gpio))
		pr_err("%s: FAN_FF_N gpio is not specified\n", __func__);
	gpio_direction_input(ddata->irq_gpio);
	if (gpio_is_valid(ddata->irq_gpio)) {
		ddata->fan_error_irq = gpio_to_irq(ddata->irq_gpio);
		if (ddata->fan_error_irq < 0) {
			pr_err("%s irq failed: gpio_int(%d) irq(%d)\n", __func__,
				ddata->irq_gpio, ddata->fan_error_irq);
			goto err_reg;
		} else {
			pr_info("%s irq ok!!: gpio_int(%d) irq(%d)\n", __func__,
				ddata->irq_gpio, ddata->fan_error_irq);
		}
	}

	ddata->buck_en= of_get_named_gpio(np, "buck-en", 0);
	if (!gpio_is_valid(ddata->buck_en))
		pr_info("%s: buck_en gpio is not specified\n", __func__);
	gpio_request(ddata->buck_en, "fan_buck_en");
	gpio_direction_output(ddata->buck_en, 0);
	// NOTE: a glitch in buck_en can cause an unexpected bump in FAN BUCK 4V, which would cause a fan stall. See b/361614773 for the detail.

	ret = of_property_read_u32(np, "fan_pulses", &ddata->fan_pulses);
	if (ret)
		pr_err("%s: not able to read fan_pulses\n", __func__);

	pr_info("dt parsing - done! %s\n", __func__);
	return 0;

err_reg:
	pr_err("dt parsing failed %s: %d", __func__, ret);
	return ret;
}

static irqreturn_t max31760_fan_recovery(int irq, void *data)
{
	struct max31760_drvdata *max31760 = (struct max31760_drvdata *)data;

	int ret = 0;
	u8 status = 0;

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_SR, &status);
	if (ret < 0) {
		pr_err("%s error in reading status register\n", __func__);
		return IRQ_NONE;
	}

	pr_info("%s FF irq detected!!: status register(0x%x) : 0x%x\n", __func__, MAX31760_REG_SR, status);
	if(status == 0x42)
		pr_info("%s Error was detected from FAN 2\n", __func__);

	else if(status == 0x41)
		pr_info("%s Error was detected from FAN 1\n", __func__);

	ret = read_u8_from_i2c(max31760->client, MAX31760_REG_CR3, &status);
	if (ret < 0) {
		pr_err("[SEC_FAN] error in reading control register3, %d\n", ret);
		return IRQ_NONE;
	}
	status = status | 0x80;
	ret =  write_u8_to_i2c(max31760->client, MAX31760_REG_CR3, status);
	if (ret < 0) {
		pr_err("[SEC_FAN] error in writing control register3, %d\n", ret);
		return IRQ_NONE;
	}

	pr_info("%s --- done\n", __func__);

	return IRQ_HANDLED;
}

/**
 * max31760_i2c_probe - Enumerate max31760 device.
 **/
static int max31760_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct max31760_drvdata *ddata = NULL;
	int ret = 0;

	pr_info("%s +++\n", __func__);
	//Check i2c function here if required
	if (!is_sec_fan_init_required())
		return 0;

	ddata = devm_kzalloc(&client->dev, sizeof(struct max31760_drvdata), GFP_KERNEL);
	if (unlikely(!ddata))
		return -ENOMEM;

	ddata->sec_fan_ddata.ops = &max31760_sec_fan_ops;
	ddata->sec_fan_ddata.chip_dev = &client->dev;
	ddata->sec_fan_ddata.vendor_dev_attr_groups = max31760_dev_attr_groups;
	i2c_set_clientdata(client, ddata);
	ddata->client = client;
	mutex_init(&ddata->i2c_lock);

	ret = max31760_parse_dt(&client->dev, ddata);
	if (ret) {
		pr_err("%s EPROBE_DEFER(%d)\n", __func__, __LINE__);
		return -EPROBE_DEFER;
	}

	INIT_DELAYED_WORK(&(ddata->debug_work),	max31760_print_reg_log);

	ret = sec_fan_register(&ddata->sec_fan_ddata);
	if (ret) {
		pr_err("%s EPROBE_DEFER(%d)\n", __func__, __LINE__);
		return -EPROBE_DEFER;
	}

	if(!max31760_init(&client->dev))
		ddata->init_done = true;

	ret = request_threaded_irq(ddata->fan_error_irq, NULL, max31760_fan_recovery,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT , "max31760_fan_recovery_irq", ddata);
	if (ret)
		pr_err("%s request irq failed: gpio_int(%d) irq(%d)\n", __func__, ddata->irq_gpio, ddata->fan_error_irq);

	pr_info("%s --- done\n", __func__);
	return 0;
}

static int max31760_i2c_remove(struct i2c_client *i2c)
{
	struct max31760_drvdata *max31760 = i2c_get_clientdata(i2c);

	mutex_destroy(&max31760->i2c_lock);

	return 0;
}
/**
 * max31760_of_match structure
 **/
static const struct of_device_id max31760_of_match[] = {
	{ .compatible = "maxim,max31760" },
	{ }
};

/**
 * max31760_id_i2c
 **/
static const struct i2c_device_id max31760_id_i2c[] = {
	{"max31760", 0},
	{}
};

/**
 * max31760_i2c_driver structure
 **/
static struct i2c_driver max31760_i2c_driver = {
	.driver = {
		.name = "max31760_sec_fan",
		.of_match_table = max31760_of_match,
		.owner = THIS_MODULE,
	},
	.id_table = max31760_id_i2c,
	.probe = max31760_i2c_probe,
	.remove = max31760_i2c_remove,
};

/**
 * Register MAX31760_i2c_driver module
 **/
 /*
module_i2c_driver(max31760_i2c_driver);
*/
static int __init max31760_i2c_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&max31760_i2c_driver);
}
late_initcall(max31760_i2c_init);

static void __exit max31760_i2c_exit(void)
{
	i2c_del_driver(&max31760_i2c_driver);
}
module_exit(max31760_i2c_exit);


MODULE_AUTHOR("Samsung Electronics <ravi.sharma@samsung.com>");
MODULE_DESCRIPTION("Maxim, MAX31760 SEC FAN Driver");
MODULE_LICENSE("GPL");
