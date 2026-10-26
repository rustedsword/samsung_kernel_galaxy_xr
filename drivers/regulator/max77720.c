// SPDX-License-Identifier: GPL-2.0+
/*
 * max77720.c - REGULATOR device driver for MAX77720
 *
 * Copyright (C) 2019 Maxim Integrated. All rights reserved.
 *
 * Author:
 *  Daniel Jeong <daniel.jeong@maximintegrated.com>
 *	Maxim Opensource <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max77720.h>
#include <linux/regulator/of_regulator.h>
#if IS_ENABLED(CONFIG_REGULATOR_DEBUG_CONTROL)
#include <linux/regulator/debug-regulator.h>
#endif

#define DRIVER_NAME		"max77720"

struct max77720_chip {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
	struct mutex update_lock;
	u32 pwr_en_gpio;
	u32 driver_en_gpio;
};

struct max77720_chip *pchip_global;

static int max77720_get_status(struct regulator_dev *rdev)
{
    struct max77720_chip *pchip = rdev_get_drvdata(rdev);
    unsigned int data;
    int ret;

    ret = regmap_read(pchip->regmap, MAX77720_REG_STATUS, &data);
    if (ret < 0) {
        MAX_ERR("failed to read i2c: %d", ret);
        return ret;
    }
    ret = (data & MAX77720_MASK_ST);
    MAX_DBG("get status: data: %d, ret: %d", data, ret);
    return ret;
}

static int max77720_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
    struct max77720_chip *pchip = rdev_get_drvdata(rdev);
    int ret;
    MAX_DBG("set mode: %d", mode);
    if (mode != REGULATOR_MODE_NORMAL)
    {
        ret = regmap_update_bits(pchip->regmap,
        		MAX77720_REG_CONFIG1,
        		MAX77720_MASK_FPWM, 0);
    }
    else
    {
        ret = regmap_update_bits(pchip->regmap,
        		MAX77720_REG_CONFIG1,
        		MAX77720_MASK_FPWM, MAX77720_MASK_FPWM);
    }
    if (ret < 0) {
        MAX_ERR(
        	"failed to update i2c: %d @ function %s\n",
        	ret, __func__);
    }
    return ret;
}

static int max77720_write_regmap(struct regmap *regmap, int addr, int data)
{
    int ret = 0;
    ret = regmap_write(regmap, addr, data);
    if (ret < 0) {
        MAX_ERR("failed to write regmap: addr: 0x%x, data: 0x%x, ret:%d\n", addr, data, ret);
        return ret;
    } else {
        MAX_DBG("success to write regmap: addr: 0x%x, data: 0x%x, ret:%d\n", addr, data, ret);
        ret = regmap_read(regmap, addr, &data);
        if (ret < 0) {
            MAX_ERR("failed to read regmap: addr: 0x%x, data: 0x%x, ret:%d\n", addr, data, ret);
            return ret;
        }
        else
        {
            MAX_DBG("success to read regmap: addr: 0x%x, data: 0x%x, ret:%d\n", addr, data, ret);
        }
    }
    return ret;
}

EXPORT_SYMBOL(max77720_write_reg);

int max77720_write_reg(int addr, int data)
{
    if(pchip_global != NULL)
        return max77720_write_regmap(pchip_global->regmap, addr, data);
    else{
         MAX_ERR("pchip_global configuration failed!  addr = 0x%x data = 0x%x \n", addr, data);
    }
    return -1;
}

static unsigned int max77720_get_mode(struct regulator_dev *rdev)
{
    struct max77720_chip *pchip = rdev_get_drvdata(rdev);
    unsigned int rval;
    int ret;

    ret = regmap_read(pchip->regmap, MAX77720_REG_CONFIG1, &rval);
    if (ret < 0) {
        MAX_ERR(
        	"failed to read i2c: %d @ function %s\n",
        	ret, __func__);
    	return ret;
    }
    ret = (rval & MAX77720_MASK_FPWM)
    		? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;;
    MAX_DBG("get mode: rval: %d, ret: %d", rval, ret);
    return ret;
}

static struct regulator_ops max77720_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.get_status = max77720_get_status,
	.set_mode = max77720_set_mode,
	.get_mode = max77720_get_mode,
};

static const struct regmap_config max77720_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x41,
	.use_single_read = true,
	.use_single_write = true,
};

static struct regulator_desc max77720_reg_vout = {
	.name = "vout_dtof",
	.id = MAX77720_ID_VOUT,
	.ops = &max77720_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.enable_mask = MAX77720_MASK_BB_EN,
	.enable_reg = MAX77720_REG_CONFIG2,
	.min_uV = MAX77720_VOUT_MIN_UV,
	.uV_step = MAX77720_VOUT_STEP_UV,
	.n_voltages = MAX77720_MASK_VOUT + 1,
	.vsel_reg = MAX77720_REG_VOUT,
	.vsel_mask = MAX77720_MASK_VOUT,
	.active_discharge_off = MAX77720_AD_DISABLE,
	.active_discharge_on = MAX77720_MASK_AD,
	.active_discharge_mask = MAX77720_MASK_AD,
	.active_discharge_reg = MAX77720_REG_CONFIG1,
};

static int max77720_parse_dt(struct max77720_chip *pdata)
{
	int ret = 0;
	struct device_node *node = pdata->dev->of_node;

	if (!node) {
		pr_err("device tree info missing %d\n", ret);
		return -EINVAL;
	}
	return ret;
}


static int max77720_init_regulator(struct max77720_chip *pchip,
				   struct device_node *node)
{
	int ret;
	unsigned int rval;
	struct regulator_config config = { };

	config.regmap = pchip->regmap;
	config.driver_data = pchip;
	config.dev = pchip->dev;
	config.of_node = pchip->dev->of_node;

	ret = regmap_read(pchip->regmap, MAX77720_REG_STATUS, &rval);
	if (ret < 0) {
		MAX_ERR(
			"failed to read i2c: %d @ function %s\n", ret,
			__func__);
		return ret;
	}

	switch (rval & MAX77720_MASK_GPIO_CFG) {
	case MAX77720_SUB_A_F:
	case MAX77720_SUB_B:
	case MAX77720_SUB_D:
	case MAX77720_SUB_E:
		MAX_ERR( "MAX77720_SUB_E function [%s] version: %d\n", __func__,
			(rval & MAX77720_MASK_GPIO_CFG));
		pchip->regulator_desc = max77720_reg_vout;
		config.init_data = of_get_regulator_init_data(pchip->dev,
                              pchip->dev->of_node,
                              &max77720_reg_vout);
		pchip->regulator
			= devm_regulator_register(pchip->dev,
					&max77720_reg_vout, &config);
#if IS_ENABLED(CONFIG_REGULATOR_DEBUG_CONTROL)
		ret = devm_regulator_debug_register(pchip->dev, pchip->regulator);
		if (ret) {
			MAX_ERR( "failed to register debug regulator for bob rc=%d\n",ret);
		}
#endif
		break;
	case MAX77720_SUB_C:
		MAX_ERR( "MAX77720_SUB_C not supported %s\n", __func__);
		break;
	default:
		MAX_ERR( "MAX77720_default function %s\n", __func__);
		break;
	}

	if (IS_ERR(pchip->regulator)) {
		ret = PTR_ERR(pchip->regulator);
		return ret;
	}
	return 0;
}

static int max77720_regulator_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
    struct device_node *node = client->dev.of_node;
    struct device *dev = &client->dev;
    struct max77720_chip *pchip;
    int ret = 0;
    pchip_global = NULL;

    pchip = devm_kzalloc(dev, sizeof(struct max77720_chip), GFP_KERNEL);
    if (!pchip)
    	return -ENOMEM;

    i2c_set_clientdata(client, pchip);
    pchip->dev = dev;
    mutex_lock(&pchip->update_lock);

    ret = max77720_parse_dt(pchip);
    if (ret) {
        MAX_ERR( "failed to parse device tree, ret:%d\n", ret);
        goto fail_parse_dt;
    }
    mutex_unlock(&pchip->update_lock);

    pchip->regmap = devm_regmap_init_i2c(client, &max77720_regmap_config);
    if (IS_ERR(pchip->regmap)) {
        ret = PTR_ERR(pchip->regmap);
        MAX_ERR("failed to initialize regmap: %d\n", ret);
        return ret;
    }

    ret = max77720_init_regulator(pchip, node);
    if (ret < 0) {
        MAX_ERR("failed to register regulator: %d\n", ret);
        return ret;
    } else {
        MAX_DBG("success to register max77720 regulator: %d\n", ret);
        ret = max77720_write_regmap(pchip->regmap, 0x05, 0x0);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
        ret = max77720_write_regmap(pchip->regmap, 0x02, 0xFF);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
        ret = max77720_write_regmap(pchip->regmap, 0x30, 0x31);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
        ret = max77720_write_regmap(pchip->regmap, 0x31, 0x01);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
        ret = max77720_write_regmap(pchip->regmap, 0x32, 0x37);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
        ret = max77720_write_regmap(pchip->regmap, 0x40, 0x0);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
        ret = max77720_write_regmap(pchip->regmap, 0x41, 0x0);
        if (ret < 0) {
            MAX_ERR("failed to write reg map %d\n", ret);
            return ret;
        }
    }

    pchip_global = pchip;

    MAX_ERR( "max77720 init done\n");
    return 0;
    fail_parse_dt:
    i2c_set_clientdata(client, NULL);
    dev_set_drvdata(&client->dev, NULL);
    return ret;
}

static const struct i2c_device_id max77720_i2c_id[] = {
	{DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(i2c, max77720_i2c_id);

static const struct of_device_id max77720_of_match[] = {
	{.compatible = "maxim,max77720"},
	{},
};
MODULE_DEVICE_TABLE(of, max77720_of_match);

static struct i2c_driver max77720_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(max77720_of_match),
	},
	.probe	= max77720_regulator_probe,
	.id_table = max77720_i2c_id,
};
module_i2c_driver(max77720_driver);

MODULE_AUTHOR("Daniel Jeong <daniel.jeong@maximintegrated.com>");
MODULE_AUTHOR("Maxim Opensource <opensource@maximintegrated.com>");
MODULE_DESCRIPTION("Regulator Device Driver for Maxim MAX77720");
MODULE_LICENSE("GPL");
