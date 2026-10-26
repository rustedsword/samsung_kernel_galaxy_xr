/*
 * ss_s6ulm1_tcon_i2c.c - Platform data for s6tulm1 pmic driver
 *
 * Copyright (C) 2023 Samsung Electronics
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
	 
#include "ss_s6tulm1_tcon_i2c.h"


#define MAX_DDI_CNT 2

struct ddi_i2c_message {
	char *address;
	char *data;
	int size;
};

struct s6tulm1_platform_data {
	struct ddi_i2c_message disp_on_data;
};

struct s6tulm1_info {
	struct i2c_client			*client;
	struct s6tulm1_platform_data	pdata;
};

static struct s6tulm1_info ddi_info[MAX_DDI_CNT];

static int ss_s6tulm1_tcon_i2c_write(struct i2c_client *client, u16 reg,  u16 val, bool is_left)
{
	struct i2c_msg msg[1];
	unsigned char buf[4];
	int ret;
	int retry = 1;

//	mutex_lock(&data->lock);
	buf[0] = (reg & 0xFF00) >> 8;
	buf[1] = (reg & 0xFF);
	buf[2] = (val & 0xFF00) >> 8;
	buf[3] = (val & 0xFF);

	msg->addr = client->addr;
	msg->flags = 0;//WRITE
	msg->len = 4;
	msg->buf = buf;
	pr_err("%s 0x%04x, 0x%04x\n", is_left?"LEFT":"RIGHT", reg, val);

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0) {
//			mutex_unlock(&data->lock);
			return 0;
		}
		pr_err("fail(%d)\n", retry);
		usleep_range(1000, 1100);
	}
//	mutex_unlock(&data->lock);
	return ret;
}

int ss_s6tulm1_tcon_i2c_read(u16 reg, u8 *val, bool is_left)
{
	struct i2c_client *client = ddi_info[is_left].client;
	struct i2c_msg msg;
	unsigned char buf[2];
	unsigned char read_buf[2];
	int ret;
	int retry = 3;

	buf[0] = (reg & 0xFF00) >> 8;
	buf[1] = (reg & 0xFF);

	msg.addr = client->addr;
	msg.flags = 0;//I2C_M_WR;
	msg.len = 2;
	msg.buf = buf;
	while (retry > 0) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			break;
		}
		pr_err("write fail(%d)\n", retry);
		usleep_range(10000, 11000);
		retry--;
	}

	msg.addr = client->addr;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = 2;
	msg.buf = read_buf;
	retry = 3;
	while (retry > 0) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			memcpy(val, read_buf, sizeof(read_buf));
			pr_err("0x%02x, 0x%02x\n", val[0], val[1]);
			return 0;
		}
		pr_err("fail(data read)(%d)\n", retry);
		usleep_range(10000, 11000);
		retry--;
	}
//	mutex_unlock(&data->lock);

	return ret;
}

int ss_s6tulm1_tcon_configure(struct samsung_display_driver_data *vdd, u16 data[][2], int size, bool is_left)
{
	struct i2c_client *client = ddi_info[is_left].client;
	int ret = 0;
	int i;

	if (!client) {
		pr_err("%s: i2c is not prepared\n", __func__);
		return -ENODEV;
	}

	if (ub_con_det_status(vdd->ndx))
		return ret;

	for (i = 0; i < size; i++) {
		u16 reg = data[i][0];
		u16 val = data[i][1];

		ss_s6tulm1_tcon_i2c_write(client, reg, val, is_left);
		{
			unsigned char check[2] = {0,}; //DEBUG

			ss_s6tulm1_tcon_i2c_read(reg, check, is_left);
			pr_err("0x%x, 0x%x\n", check[0], check[1]);
		}
	}

	pr_err("s6tulm1 config done\n");

	return ret;
}

int ss_s6tulm1_tcon_parse_dt(struct device *dev,
			struct s6tulm1_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	const char *data;
	int  data_offset, len = 0, i = 0;
	struct ddi_i2c_message *disp_on_data = &pdata->disp_on_data;

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

	disp_on_data->size = len / 2;

	disp_on_data->address = kzalloc(disp_on_data->size, GFP_KERNEL);
	if (!disp_on_data->address) {
		pr_err("fail to allocate address\n");
		return -ENOMEM;
	}

	disp_on_data->data = kzalloc(disp_on_data->size, GFP_KERNEL);
	if (!disp_on_data->data) {
		pr_err("fail to allocate data\n");
		disp_on_data->size = 0;
		kfree(disp_on_data->address);
		return -ENOMEM;
	}

	data_offset = 0;
	for (i = 0 ; i < disp_on_data->size; i++) {
		disp_on_data->address[i] = data[data_offset++];
		disp_on_data->data[i] = data[data_offset++];

		pr_err("init_data: %02X 0x%02X\n",
				disp_on_data->address[i], disp_on_data->data[i]);
	}
	return 0;
}

static int ss_s6tulm1_tcon_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct s6tulm1_platform_data *pdata;

	int error = 0;

	static int idx;

	pr_err("%s ++\n", __func__);

	if (!strcmp(id->name, "s6tulm1_l"))
		pr_err("%s left++\n", __func__);
	else if (!strcmp(id->name, "s6tulm1_r"))
		pr_err("%s right++\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality is failed\n");
		return -EIO;
	}

	pr_err("%s [%d] client[%s] address [%02X]\n", __func__, idx, client->name, client->addr);

	ddi_info[idx].client = client;
	pdata = &ddi_info[idx].pdata;

	if (!client->dev.of_node) {
		pr_err("error: of_node is null\n");
		return -ENODEV;
	}

	ss_s6tulm1_tcon_parse_dt(&client->dev, pdata);

	i2c_set_clientdata(client, ddi_info);

	idx++;

	pr_err("%s --\n", __func__);

	return error;
}

static int ss_s6tulm1_tcon_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id s6tulm1_id[] = {
	{"s6tulm1_left", 0},
	{"s6tulm1_right", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, s6tulm1_id);

static const struct of_device_id s6tulm1_match_table[] = {
	{ .compatible = "sdc,s6tulm1_left",},
	{ .compatible = "sdc,s6tulm1_right",},
	{}
};

struct i2c_driver ss_s6tulm1_tcon_driver = {
	.probe = ss_s6tulm1_tcon_probe,
	.remove = ss_s6tulm1_tcon_remove,
	.driver = {
		.name = "s6tulm1",
		.owner = THIS_MODULE,
		.of_match_table = s6tulm1_match_table,
	},
	.id_table = s6tulm1_id,
};

int ss_s6tulm1_tcon_init(void)
{
	int ret = 0;

	pr_err("%s +++++++++\n", __func__);

	ret = i2c_add_driver(&ss_s6tulm1_tcon_driver);
	if (ret)
		pr_err("s6tulm1_init registration failed. ret= %d\n", ret);

	pr_err("%s ------------\n", __func__);
	return ret;
}

void ss_s6tulm1_tcon_exit(void)
{
	i2c_del_driver(&ss_s6tulm1_tcon_driver);
}
