/*
 *  stk68210.c - Linux kernel modules for sensortek stk6d2x
 *  ambient light sensor (driver)
 *
 *  Copyright (C) 2012~2018 Bk, sensortek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stk68210.h>
#include <linux/delay.h>


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/pwm.h>


static int stk6d2x_parse_dt(struct stk6d2x* lp)
{
	struct device* dev = lp->dev;
	struct device_node* node = dev->of_node;
	struct stk6d2x_platform_data* pdata;
	int ret, temp;
	u8 val_u8;

	ALS_ERR("%s : stk6d2x_parse_dt stk6d2x (E)");

	if (!node) {
		ALS_ERR("no platform data\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ALS_ERR("failed to allocate driver data\n");
		return -ENOMEM;
	}

	/*bl-name*/
	ret = of_property_read_string(node, "bl-name", &pdata->name);
	if (ret < 0) {
		ALS_ERR("Error in getting bl-name.\n");
	}
	else {
		ALS_DBG("Success in getting bl-name (%s)\n", pdata->name);
	}
	return 0;
}

static int stk6d2x_probe(struct i2c_client* cl, const struct i2c_device_id* id)
{
	struct stk6d2x* lp;	
	int ret = 0;
	ALS_ERR("%s : probe enter stk6d2x (E)", cl->name);

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct stk6d2x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = cl;
	lp->dev = &cl->dev;
	lp->chipname = id->name;
	//lp->chip_id = id->driver_data;
	lp->pdata = dev_get_platdata(&cl->dev);

	if (!lp->pdata) {
		ret = stk6d2x_parse_dt(lp);
		if (ret < 0)
			return ret;
	}
	ALS_ERR("%s : parse success for stk6d2x (E) ret %d", cl->name, ret);
	
	return ret;
}

static int stk6d2x_remove(struct i2c_client* cl)
{
	//struct stk6d2x* lp = i2c_get_clientdata(cl);


	return 0;
}

static const struct of_device_id stk6d2x_dt_ids[] = {
	{ .compatible = "stk,stk6d2x", },
	{ },
};
MODULE_DEVICE_TABLE(of, stk6d2x_dt_ids);

static const struct i2c_device_id stk6d2x_ids[] = {
	{"stk68210", STK68210},
	{"stk68310", STK68310},
	{ }
};
MODULE_DEVICE_TABLE(i2c, stk6d2x_ids);

static struct i2c_driver stk_als_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = STK6D2X_DEV_NAME,
		   .of_match_table = of_match_ptr(stk6d2x_dt_ids),
		   },
	.probe = stk6d2x_probe,
	.remove = stk6d2x_remove,
	.id_table = stk6d2x_ids,
};

module_i2c_driver(stk_als_driver);

MODULE_DESCRIPTION("Sensortek stk6d2x ambient Light Sensor driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: sensors_core");
