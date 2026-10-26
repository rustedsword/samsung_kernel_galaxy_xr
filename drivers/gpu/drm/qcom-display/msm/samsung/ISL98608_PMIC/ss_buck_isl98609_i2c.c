/*
 * isl98608_hw_i2c.c - Platform data for isl98608 buck driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>

#include "ss_buck_isl98609_i2c.h"

#define MAX_LED_CNT 2

struct blic_message {
	char *addr;
	char *data;
	int size;
	char *check_data;
};

struct isl98608_platform_data {
	int gpio_enp;
	int gpio_enn;

	struct blic_message init_data;
};

struct isl98608_info {
	struct i2c_client			*client;
	struct isl98608_platform_data	pdata;
};

static struct isl98608_info info[MAX_LED_CNT];

static int ss_buck_i2c_read(struct i2c_client *client, u8 addr,  u8 *value)
{
	int retry = 3;
	u8 wr[] = {addr};

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = wr,
			.len = 1
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = value,
			.len = 1
		},
	};
	int ret;

	do {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (ret != 2)
			pr_err("%s : client->addr 0x%02x read_addr 0x%02x error (ret == %d)\n", __func__, client->addr, addr, ret);
		else
			break;
	} while (--retry);

	return ret;
}

static int ss_buck_i2c_write(struct i2c_client *client, u8 addr,  u8 value)
{
	int retry = 3;
	u8 wr[] = {addr, value};
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = wr,
			.len = 2
		}
	};
	int ret;

	do {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret != 1) {
			pr_err("%s : addr 0x%02x value 0x%02x error (ret == %d)\n", __func__,
				 addr, value, ret);
		} else
			break;
	} while (--retry);

	return ret;
}

int isl98608_parse_dt(struct device *dev,
			struct isl98608_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	const char *data;
	int  data_offset, len = 0 , i = 0;
	struct blic_message *init_data = &pdata->init_data;

	/* parse i2c init_data */
	data = of_get_property(np, "init_data", &len);
	if (!data) {
		pr_err("fail to parse init_data\n");
		return -EINVAL;
	}

	if ((len % 2) != 0) {
		pr_err("invalid length for init_data\n");
		return -EINVAL;
	}

	init_data->size = len / 2;

	init_data->addr = kvzalloc(init_data->size, GFP_KERNEL);
	if (!init_data->addr) {
		pr_err("fail to allocate addr\n");
		return -ENOMEM;
	}

	init_data->data = kvzalloc(init_data->size, GFP_KERNEL);
	if (!init_data->data) {
		pr_err("fail to allocate data\n");
		init_data->size = 0;
		kfree(init_data->addr);
		return -ENOMEM;
	}
	init_data->check_data = kvzalloc(init_data->size, GFP_KERNEL);
	if (!init_data->check_data) {
		pr_err("fail to allocate data\n");
		init_data->size = 0;
		kfree(init_data->check_data);
		return -ENOMEM;
	}

	data_offset = 0;
	for (i = 0 ; i < init_data->size; i++) {
		init_data->addr[i] = data[data_offset++];
		init_data->data[i] = data[data_offset++];

		pr_err("init_data 2 : %02X 0x%02X\n",
				init_data->addr[i], init_data->data[i]);
	}

	return 0;
}

static int ss_buck_isl98608_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct isl98608_platform_data *pdata;
	struct blic_message *init_data;
	static int isl98608_idx = 0;
	int i;

	if (!strcmp(id->name,"isl98608_left"))
		pr_info("ss_buck_isl98608_probe left\n");
	else if (!strcmp(id->name,"isl98608_right"))
		pr_info("ss_buck_isl98608_probe right\n");
	else {
		pr_info("wrong client %s\n", client->name);	
		return -ENODEV;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		pr_err("%s i2c check fail\n", __func__);
		return -EIO;
	}

	info[isl98608_idx].client = client;
	pdata = &info[isl98608_idx].pdata;
	init_data = &pdata->init_data;
	isl98608_parse_dt(&client->dev, pdata);

	i2c_set_clientdata(client, &info[isl98608_idx].client);

	for(i = 0; i < init_data->size ; i++) {
		ss_buck_i2c_write(info[isl98608_idx].client, init_data->addr[i], init_data->data[i]);
		ss_buck_i2c_read(info[isl98608_idx].client, init_data->addr[i], &init_data->check_data[i]);	
		if (init_data->data[i] != init_data->check_data[i])
			pr_err("[SDE]2 write 0x%x 0x%x fail\n", init_data->addr[i], init_data->check_data[i]);
		else
			pr_info("[SDE]2 write 0x%x 0x%x success\n", init_data->addr[i], init_data->check_data[i]);
	}

	isl98608_idx++;

	return 0;
}

static int ss_buck_isl98608_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ss_buck_isl98608_id[] = {
	{"isl98608_left", 0},
	{"isl98608_right", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ss_buck_isl98608_id);

static struct of_device_id ss_buck_isl98608_match_table[] = {
	{ .compatible = "sec,isl98608_left",},
	{ .compatible = "sec,isl98608_right",},
	{}
};

struct i2c_driver ss_buck_isl98608_driver = {
	.probe = ss_buck_isl98608_probe,
	.remove = ss_buck_isl98608_remove,
	.driver = {
		.name = "isl98608",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(ss_buck_isl98608_match_table),
#endif
		   },
	.id_table = ss_buck_isl98608_id,
};

int ss_buck_isl98608_init(void)
{

	int ret = 0;

	ret = i2c_add_driver(&ss_buck_isl98608_driver);
	if (ret)
		pr_err("%s 2: ss_buck_isl98608_init registration failed. ret= %d\n", __func__, ret);

	return ret;
}

void ss_buck_isl98608_exit(void)
{
	i2c_del_driver(&ss_buck_isl98608_driver);
}
