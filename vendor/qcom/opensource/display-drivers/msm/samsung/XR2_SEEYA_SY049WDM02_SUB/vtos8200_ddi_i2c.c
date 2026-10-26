/*
 * vtos8200_pmic.c - Platform data for vtos8200 pmic driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include "ss_dsi_panel_common.h"

#define MAX_DDI_CNT 2

struct ddi_i2c_message {
	char *address;
	char *data;
	int size;
};

struct vtos8200_platform_data {	
	struct ddi_i2c_message disp_on_data;
};

struct vtos8200_info {
	struct i2c_client			*client;
	struct vtos8200_platform_data	pdata;
};

static struct vtos8200_info ddi_info[MAX_DDI_CNT];

static int ss_ddi_vtos8200_i2c_write(struct i2c_client *client, u16 reg,  u16 val, bool is_left)
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
	LCD_DEBUG("%s 0x%04x, 0x%04x\n",is_left?"LEFT":"RIGHT", reg, val);

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0) {
//			mutex_unlock(&data->lock);
			return 0;
		}
		LCD_INFO("fail(%d)\n", retry);
		usleep_range(1000, 1100);
	}
//	mutex_unlock(&data->lock);
	return ret;
}

int ss_ddi_vtos8200_i2c_read(u16 reg, u8 *val, bool is_left)
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
		} else {	
			LCD_DEBUG("write fail(%d)\n", retry);
			usleep_range(10000, 11000);
		}
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
			LCD_DEBUG("0x%02x, 0x%02x\n", val[0], val[1]);
			return 0;
		} else {
			LCD_DEBUG("fail(data read)(%d)\n", retry);
			usleep_range(10000, 11000);
		}
		retry--;
	}

//	mutex_unlock(&data->lock);

	return ret;
}

int ss_ddi_vtos8200_configure(struct samsung_display_driver_data *vdd, u16 data[][2], int size, bool is_left)
{
	struct i2c_client *client = ddi_info[is_left].client;
	int ret = 0;
	int i;

	if (!client) {
		LCD_INFO_ONCE("%s: i2c is not prepared\n", __func__);
		return -ENODEV;
	}

	if (ub_con_det_status(vdd->ndx)) {
		return ret;
	}
		
	for (i = 0; i < size; i++) {
		u16 reg = data[i][0];
		u16 val = data[i][1];

		ss_ddi_vtos8200_i2c_write(client, reg, val, is_left);
#if 0 //DEBUG
	{
		unsigned char check[2] = {0,}; //DEBUG
		ss_ddi_vtos8200_i2c_read(reg, check, is_left);
		LCD_DEBUG("0x%x, 0x%x\n", check[0], check[1]);
	}
#endif
	}

	LCD_ERR("vtos8200 config done\n");

	return ret;
}

int ss_ddi_vtos8200_parse_dt(struct device *dev,
			struct vtos8200_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	const char *data;
	int  data_offset, len = 0 , i = 0;
	struct ddi_i2c_message *disp_on_data = &pdata->disp_on_data;

	/* parse i2c init_data */
	data = of_get_property(np, "init_data", &len);
	if (!data) {
		LCD_ERR("fail to parse init_data\n");
		return -EINVAL;
	}

	if ((len % 2) != 0) {
		LCD_ERR("invalid length for init_data\n");
		return -EINVAL;
	}

	disp_on_data->size = len / 2;

	disp_on_data->address = kzalloc(disp_on_data->size, GFP_KERNEL);
	if (!disp_on_data->address) {
		LCD_ERR("fail to allocate address\n");
		return -ENOMEM;
	}

	disp_on_data->data = kzalloc(disp_on_data->size, GFP_KERNEL);
	if (!disp_on_data->data) {
		LCD_ERR("fail to allocate data\n");
		disp_on_data->size = 0;
		kfree(disp_on_data->address);
		return -ENOMEM;
	}

	data_offset = 0;
	for (i = 0 ; i < disp_on_data->size; i++) {
		disp_on_data->address[i] = data[data_offset++];
		disp_on_data->data[i] = data[data_offset++];

		LCD_INFO("init_data: %02X 0x%02X\n",
				disp_on_data->address[i], disp_on_data->data[i]);
	}
	return 0;
}

static int ss_ddi_vtos8200_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct vtos8200_platform_data *pdata;

	int error = 0;

	static int idx = 0;

	if (!strcmp(id->name,"vtos8200_l"))
		LCD_INFO("left++\n");
	else if (!strcmp(id->name,"vtos8200_r"))
		LCD_INFO("right++\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		LCD_ERR("i2c_check_functionality is failed.. \n");
		return -EIO;
	}

	LCD_INFO("[%d] client[%s] address [%02X]\n", idx, client->name, client->addr);

	ddi_info[idx].client = client;
	pdata = &ddi_info[idx].pdata;

	if (!client->dev.of_node) {
		LCD_ERR("error: of_node is null\n");
		return -ENODEV;
	}

	ss_ddi_vtos8200_parse_dt(&client->dev, pdata);

	i2c_set_clientdata(client, ddi_info);

	idx++;

	LCD_INFO("--\n");

	return error;
}

static int ss_ddi_vtos8200_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id vtos8200_id[MAX_DDI_CNT] = {
	{"vtos8200_left", 0},
	{"vtos8200_right", 0},
};
MODULE_DEVICE_TABLE(i2c, vtos8200_id);

static struct of_device_id vtos8200_match_table[MAX_DDI_CNT] = {
	{ .compatible = "sec,vtos8200_left",},
	{ .compatible = "sec,vtos8200_right",},
};

struct i2c_driver ss_ddi_vtos8200_driver = {
	.probe = ss_ddi_vtos8200_probe,
	.remove = ss_ddi_vtos8200_remove,
	.driver = {
		.name = "vtos8200",
		.owner = THIS_MODULE,
		.of_match_table = vtos8200_match_table,
	},
	.id_table = vtos8200_id,
};

static int __init ss_ddi_vtos8200_init(void)
{
	int ret = 0;

	LCD_INFO("+++ \n");

	ret = i2c_add_driver(&ss_ddi_vtos8200_driver);
	if (ret) {
		LCD_ERR("vtos8200_init registration failed. ret= %d\n", ret);
	}

	LCD_INFO("--- \n");
	return ret;
}

static void __exit ss_ddi_vtos8200_exit(void)
{
	i2c_del_driver(&ss_ddi_vtos8200_driver);
}

module_init(ss_ddi_vtos8200_init);
module_exit(ss_ddi_vtos8200_exit);

MODULE_DESCRIPTION("vtos8200 ddi driver");
MODULE_LICENSE("GPL");

