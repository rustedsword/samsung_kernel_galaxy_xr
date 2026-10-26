/* abov_touchkey.c -- Linux driver for abov chip as touchkey
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
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
#include <linux/sec_class.h>
#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "abov_tk_t396.h"
#include "abov_tk_fw.h"

#define BIT_DOWN	5
#define BIT_MOVE	6
#define BIT_UP		7

#define	bit_test(val, n)	((val) & (1<<(n)))

/* registers */
#define ABOV_BTNSTATUS		0x00
#define ABOV_FW_VER			0x01
#define ABOV_MODEL_NUMBER	0x02

#define ABOV_SW_RESET		0x06
#define ABOV_INT_TEST		0x08

#define ABOV_COORD		0x10

#define ABOV_THRESHOLD		0x24
#define ABOV_DIFFDATA		0x3C
#define ABOV_RAWDATA		0x30

#define ABOV_BOOT_DELAY		45
#define ABOV_RESET_DELAY	250

#define ABOV_FLASH_MODE		0x90

//static struct device *sec_touchkey;

#define TK_FW_PATH_BIN "abov/abov_noble.fw"
#define TK_FW_PATH_SDCARD "/sdcard/abov_fw.bin"

#define I2C_M_WR 0		/* for i2c */

enum {
	BUILT_IN = 0,
	SDCARD,
};

#define ABOV_ISP_FIRMUP_ROUTINE	0

#define TOUCHPAD_SWIPE_FORWARD	531
#define TOUCHPAD_SWIPE_BACKWARD	532

extern unsigned int system_rev;
extern struct class *sec_class;
static int touchkey_keycode[] = {
	TOUCHPAD_SWIPE_FORWARD, TOUCHPAD_SWIPE_BACKWARD
};

struct abov_tk_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct device *dev;
	struct abov_touchkey_platform_data *pdata;

	struct mutex lock;

	const struct firmware *firm_data_bin;
	const u8 *firm_data_ums;
	char phys[32];
	long firm_size;
	int irq;
	int (*power) (bool on);
	void (*input_event)(void *data);
	int touchkey_count;
	u8 fw_ver;
	u8 fw_ver_bin;
	u8 fw_model_number;
	u8 checksum_h;
	u8 checksum_h_bin;
	u8 checksum_l;
	u8 checksum_l_bin;
	u16 ch0_thd;
	u16 ch1_thd;
	u16 ch2_thd;
	unsigned int comm_err_count;
	bool enabled;
	unsigned int key_num;
	atomic_t irq_test_state;
#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
	struct sec_cmd_data sec;
#endif
};

#ifdef USE_OPEN_CLOSE
static int abov_tk_input_open(struct input_dev *dev);
static void abov_tk_input_close(struct input_dev *dev);
#endif /* USE_OPEN_CLOSE */

static int abov_tk_i2c_read_checksum(struct abov_tk_info *info);
static void abov_tk_reset(struct abov_tk_info *info);

static int abov_tk_fw_mode_check(struct abov_tk_info *info);
static int touchkey_threshold(struct abov_tk_info *info);

#if ABOV_ISP_FIRMUP_ROUTINE
static void abov_config_gpio_i2c(struct abov_tk_info *info, int onoff)
{
	struct device *i2c_dev = info->client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c;

	if (onoff) {
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR(pinctrl_i2c))
			input_err(true, &info->client->dev, "%s: Failed to configure i2c pin\n", __func__);
	} else {
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
		if (IS_ERR(pinctrl_i2c))
			input_err(true, &info->client->dev, "%s: Failed to configure i2c pin\n", __func__);
	}
}
#endif

static int abov_tk_i2c_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	msg.addr = client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = &reg;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;

		input_err(true, &client->dev, "%s fail(address set)(%d)\n",
			__func__, retry);
		msleep(10);
	}
	if (ret <= 0) {
		input_err(true, &client->dev, "%s fail(address set) ret:(%d)\n",
			__func__, ret);
		info->comm_err_count++;
		mutex_unlock(&info->lock);
		return ret;
	}
	retry = 3;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		input_err(true, &client->dev, "%s fail(data read)(%d)\n",
			__func__, retry);
		msleep(10);
	}
	info->comm_err_count++;
	mutex_unlock(&info->lock);
	return ret;
}

static int abov_tk_i2c_read_data(struct i2c_client *client, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	msg.addr = client->addr;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		dev_err(&client->dev, "%s fail(data read)(%d)\n",
			__func__, retry);
		msleep(10);
	}
	info->comm_err_count++;
	mutex_unlock(&info->lock);
	return ret;
}

static int abov_tk_i2c_write(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg[1];
	unsigned char data[2];
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	data[0] = reg;
	data[1] = *val;
	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		input_err(true, &client->dev, "%s fail(%d)\n",
			__func__, retry);
		msleep(10);
	}
	info->comm_err_count++;
	mutex_unlock(&info->lock);
	return ret;
}

static void release_all_fingers(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	int i;

	input_info(true, &client->dev, "%s called (touchkey_count=%d)\n", __func__,info->touchkey_count);

	for (i = 1; i < info->touchkey_count; i++) {
		input_report_key(info->input_dev,
			touchkey_keycode[i], 0);
	}
	input_sync(info->input_dev);
}

static int abov_tk_reset_for_bootmode(struct abov_tk_info *info)
{
	int ret=0;

	info->pdata->power(info, false);
	msleep(100);
	info->pdata->power(info, true);

	return ret;

}

static void abov_tk_reset(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;

	if (info->enabled == false)
		return;

	input_info(true,&client->dev, "%s start\n", __func__);
	disable_irq_nosync(info->irq);

	info->enabled = false;

	release_all_fingers(info);

	abov_tk_reset_for_bootmode(info);
	msleep(ABOV_RESET_DELAY);

	info->enabled = true;

	enable_irq(info->irq);
	input_info(true,&client->dev, "%s end\n", __func__);
}

static irqreturn_t abov_tk_interrupt(int irq, void *dev_id)
{
	struct abov_tk_info *info = dev_id;
	struct i2c_client *client = info->client;
	int ret, retry;
	u8 r_buf[2] = { 0, };
	int coord;

	ret = abov_tk_i2c_read(client, ABOV_COORD, r_buf, 2);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			input_err(true, &client->dev, "%s read fail(%d)\n",
				__func__, retry);
			ret = abov_tk_i2c_read(client, ABOV_COORD, r_buf, 2);
			if (ret == 0)
				break;
			else
				msleep(10);
		}
		if (retry <= 0) {
			abov_tk_reset(info);
			return IRQ_HANDLED;
		}
	}

//	input_info(true, &client->dev, "%s buf = 0x%02x 0x%02x\n", __func__, r_buf[0], r_buf[1]);

	if (atomic_read(&info->irq_test_state)) {
		atomic_set(&info->irq_test_state, false);
		return IRQ_HANDLED;
	}

	coord = ((r_buf[0] & 0x0f) << 8) | r_buf[1];

	if (bit_test(r_buf[0], BIT_DOWN) || bit_test(r_buf[0], BIT_MOVE)) {
		input_mt_slot(info->input_dev, 0);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, coord);
		input_sync(info->input_dev);

		if (bit_test(r_buf[0], BIT_DOWN))
			input_info(true, &client->dev, "%s: [P] %d\n", __func__, coord);
//		else if (bit_test(r_buf[0], BIT_MOVE))
//			input_info(true, &client->dev, "%s: [M] %d\n", __func__, coord);
	} else if (bit_test(r_buf[0], BIT_UP)) {
		input_mt_slot(info->input_dev, 0);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_sync(info->input_dev);

		input_info(true, &client->dev, "%s: [R] %d\n", __func__, coord);
	} else
		input_info(true, &client->dev, "%s: wrong %d\n", __func__, coord);

	return IRQ_HANDLED;

}

static int touchkey_threshold(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	u8 r_buf[6] = { 0, };
	int ret;

	ret = abov_tk_i2c_read(client, ABOV_THRESHOLD, r_buf, 6);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		return -1;
	}

	info->ch0_thd = (r_buf[0] << 8) | r_buf[1];
	info->ch1_thd = (r_buf[2] << 8) | r_buf[3];
	info->ch2_thd = (r_buf[4] << 8) | r_buf[5];

	input_info(true, &client->dev, "%s: %d, %d, %d\n", __func__,
				info->ch0_thd, info->ch1_thd, info->ch2_thd);

	return 0;
}

static int get_tk_fw_version(struct abov_tk_info *info, bool bootmode)
{
	struct i2c_client *client = info->client;
	u8 buf;
	int ret;
	int retry = 10;

	ret = abov_tk_i2c_read(client, ABOV_FW_VER, &buf, 1);
	if (ret < 0) {
		while (retry--) {
			input_err(true, &client->dev, "%s read fail(%d)\n",
				__func__, retry);
			if (!bootmode)
				abov_tk_reset(info);
			else
				return -1;
			ret = abov_tk_i2c_read(client, ABOV_FW_VER, &buf, 1);
			if (ret == 0)
				break;
		}
		msleep(20);
		if (retry <= 0)
			return -1;
	}

	info->fw_ver = buf;

	input_info(true, &client->dev, "%s : 0x%x\n", __func__, buf);

	touchkey_threshold(info);

	return 0;
}

static ssize_t read_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	int ret;

	ret = get_tk_fw_version(info, false);
	if (ret < 0) {
		input_err(true, &client->dev, "%s read fail\n", __func__);
		info->fw_ver = 0;
	}

	return sprintf(buf, "0x%02x\n", info->fw_ver);
}

static int abov_load_fw(struct abov_tk_info *info, u8 cmd)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	switch(cmd) {
	case BUILT_IN:
			/* Header info
			* 0x00 0x91 : model info,
			* 0x00 0x00 : module info (Rev 0.0),
			* 0x00 0xF3 : F/W
			* 0x00 0x00 0x17 0x10 : checksum
			* ~ 22byte 0x00 */
		if (info->pdata->support_fw_header) {
			input_info(true, &client->dev, "%s : support_fw_header\n", __func__);
			info->fw_model_number = tsp_firmware[1];
			info->fw_ver_bin = tsp_firmware[5];
			info->checksum_h_bin = tsp_firmware[8];
			info->checksum_l_bin = tsp_firmware[9];
			info->firm_size = TSP_FIRMWARE_SIZE;

			input_info(true, &client->dev, "%s, bin version:%2X,%2X,%2X   crc:%2X,%2X\n", __func__, \
				tsp_firmware[1], tsp_firmware[3], info->fw_ver_bin, \
				info->checksum_h_bin, info->checksum_l_bin);
		} else {
			ret = request_firmware(&info->firm_data_bin,
				info->pdata->fw_path, &client->dev);
			if (ret) {
				input_err(true, &client->dev,
					"%s request_firmware fail(%d)\n", __func__, cmd);
				return ret;
			}

			info->fw_model_number = info->firm_data_bin->data[1];
			info->fw_ver_bin = info->firm_data_bin->data[5];
			info->checksum_h_bin = info->firm_data_bin->data[8];
			info->checksum_l_bin = info->firm_data_bin->data[9];
			info->firm_size = info->firm_data_bin->size;

			input_info(true, &client->dev, "%s, bin version:%2X,%2X,%2X   crc:%2X,%2X\n", __func__, \
				info->firm_data_bin->data[1], info->firm_data_bin->data[3], info->fw_ver_bin, \
				info->checksum_h_bin, info->checksum_l_bin);
		}
		break;
	default:
		ret = -1;
		break;
	}
	input_info(true, &client->dev, "fw_size : %lu\n", info->firm_size);
	input_info(true, &client->dev, "%s success\n", __func__);
	return ret;
}

#if ABOV_ISP_FIRMUP_ROUTINE
void abov_i2c_start(int scl, int sda)
{
	gpio_direction_output(sda, 1);
	gpio_direction_output(scl, 1);
	usleep_range(15, 17);
	gpio_direction_output(sda, 0);
	usleep_range(10, 12);
	gpio_direction_output(scl, 0);
	usleep_range(10, 12);
}

void abov_i2c_stop(int scl, int sda)
{
	gpio_direction_output(scl, 0);
	usleep_range(10, 12);
	gpio_direction_output(sda, 0);
	usleep_range(10, 12);
	gpio_direction_output(scl, 1);
	usleep_range(10, 12);
	gpio_direction_output(sda, 1);
}

void abov_testdelay(void)
{
	u8 i;
	u8 delay;

	/* 120nms */
	for (i = 0; i < 15; i++)
		delay = 0;
}


void abov_byte_send(u8 data, int scl, int sda)
{
	u8 i;
	for (i = 0x80; i != 0; i >>= 1) {
		gpio_direction_output(scl, 0);
		usleep_range(1,1);

		if (data & i)
			gpio_direction_output(sda, 1);
		else
			gpio_direction_output(sda, 0);

		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);
	}
	usleep_range(1,1);

	gpio_direction_output(scl, 0);
	gpio_direction_input(sda);
	usleep_range(1,1);

	gpio_direction_output(scl, 1);
	usleep_range(1,1);

	gpio_get_value(sda);
	abov_testdelay();

	gpio_direction_output(scl, 0);
	gpio_direction_output(sda, 0);
	usleep_range(20,20);
}

u8 abov_byte_read(bool type, int scl, int sda)
{
	u8 i;
	u8 data = 0;
	u8 index = 0x7;

	gpio_direction_output(scl, 0);
	gpio_direction_input(sda);
	usleep_range(1,1);

	for (i = 0; i < 8; i++) {
		gpio_direction_output(scl, 0);
		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);

		data = data | (u8)(gpio_get_value(sda) << index);
		index -= 1;
	}
		usleep_range(1,1);
	gpio_direction_output(scl, 0);

	gpio_direction_output(sda, 0);
		usleep_range(1,1);

	if (type) { /*ACK */
		gpio_direction_output(sda, 0);
		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);
		gpio_direction_output(scl, 0);
		usleep_range(1,1);
	} else { /* NAK */
		gpio_direction_output(sda, 1);
		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);
		gpio_direction_output(scl, 0);
		usleep_range(1,1);
		gpio_direction_output(sda, 0);
		usleep_range(1,1);
	}
	usleep_range(20,20);

	return data;
}

void abov_enter_mode(int scl, int sda)
{
	abov_i2c_start(scl, sda);
	abov_testdelay();
	abov_byte_send(ABOV_ID, scl, sda);
	abov_byte_send(0xAC, scl, sda);
	abov_byte_send(0x5B, scl, sda);
	abov_byte_send(0x2D, scl, sda);
	abov_i2c_stop(scl, sda);
}

void abov_firm_write(const u8 *fw_data, int block, int scl, int sda)
{
	int i, j;
	u16 pos = 0x20;
	u8 addr[2];
	u8 data[32] = {0, };

	addr[0] = 0x10;
	addr[1] = 0x00;
	for (i = 0; i < (block - 0x20); i++) {
		if (i % 8 == 0) {
			addr[0] = 0x10 + i/8;
			addr[1] = 0;
		} else
			addr[1] = addr[1] + 0x20;
		memcpy(data, fw_data + pos, 32);
		abov_i2c_start(scl, sda);
		abov_testdelay();
		abov_byte_send(ABOV_ID, scl, sda);
		abov_byte_send(0xAC, scl, sda);
		abov_byte_send(0x7A, scl, sda);
		abov_byte_send(addr[0], scl, sda);
		abov_byte_send(addr[1], scl, sda);
		for (j = 0; j < 32; j++)
			abov_byte_send(data[j], scl, sda);
		abov_i2c_stop(scl, sda);

		pos += 0x20;

		usleep_range(3000,3000);; //usleep(2000); //msleep(2);
	}
}

void abov_read_address_set(int scl, int sda)
{
		abov_i2c_start(scl, sda);
		abov_testdelay();
		abov_byte_send(ABOV_ID, scl, sda);
		abov_byte_send(0xAC, scl, sda);
		abov_byte_send(0x9E, scl, sda);
		abov_byte_send(0x10, scl, sda); /* start addr H */
		abov_byte_send(0x00, scl, sda); /* start addr L */
		abov_byte_send(0x3F, scl, sda); /* end addr H  */
		abov_byte_send(0xFF, scl, sda); /* end addr L  */
		abov_i2c_stop(scl, sda);
}

void abov_checksum(struct abov_tk_info *info, int scl, int sda)
{
	struct i2c_client *client = info->client;

	u8 status;
	u8 bootver;
	u8 firmver;
	u8 checksumh;
	u8 checksuml;

	abov_read_address_set(scl, sda);
	msleep(5);

	abov_i2c_start(scl, sda);
	abov_testdelay();
	abov_byte_send(ABOV_ID, scl, sda);
	abov_byte_send(0x00, scl, sda);

	abov_i2c_start(scl, sda); /* restart */
	abov_testdelay();
	abov_byte_send(ABOV_ID + 1, scl, sda);
	status = abov_byte_read(true, scl, sda);
	bootver = abov_byte_read(true, scl, sda);
	firmver = abov_byte_read(true, scl, sda);
	checksumh = abov_byte_read(true, scl, sda);
	checksuml = abov_byte_read(false, scl, sda);
	abov_i2c_stop(scl, sda);
	msleep(3);

	info->checksum_h = checksumh;
	info->checksum_l = checksuml;

	input_dbg(true, &client->dev,
		"%s status(0x%x), boot(0x%x), firm(0x%x), cs_h(0x%x), cs_l(0x%x)\n",
		__func__, status, bootver, firmver, checksumh, checksuml);
}

void abov_exit_mode(int scl, int sda)
{
	abov_i2c_start(scl, sda);
	abov_testdelay();
	abov_byte_send(ABOV_ID, scl, sda);
	abov_byte_send(0xAC, scl, sda);
	abov_byte_send(0x5B, scl, sda);
	abov_byte_send(0xE1, scl, sda);
	abov_i2c_stop(scl, sda);
}

static int abov_fw_update(struct abov_tk_info *info,
				const u8 *fw_data, int block, int scl, int sda)
{
	input_dbg(true, &info->client->dev, "%s start (%d)\n", __func__, __LINE__);
	abov_config_gpio_i2c(info, 0);
	msleep(ABOV_BOOT_DELAY);
	abov_enter_mode(scl, sda);
	msleep(1100); //msleep(600);
	abov_firm_write(fw_data, block, scl, sda);
	abov_checksum(info, scl, sda);
	abov_config_gpio_i2c(info, 1);
	input_dbg(true, &info->client->dev, "%s end (%d)\n", __func__, __LINE__);
	return 0;
}
#endif

static int abov_tk_check_busy(struct abov_tk_info *info)
{
	int ret, count = 0;
	unsigned char val = 0x00;

	do {
		ret = i2c_master_recv(info->client, &val, sizeof(val));

		if (val & 0x01) {
			count++;
			if (count > 1000)
				input_err(true, &info->client->dev,
					"%s: busy %d\n", __func__, count);
				return ret;
		} else {
			break;
		}

	} while(1);

	return ret;
}

#define ABOV_TSP_FIRM_CHECKSUM_MSB_OFFSET    0x08
#define ABOV_TSP_FIRM_CHECKSUM_LSB_OFFSET    0x09

static int abov_tk_i2c_read_checksum(struct abov_tk_info *info)
{
	unsigned char data[6] = {0xAC, 0x9E, 0x04, 0x00, 0x37, 0xFF};
	unsigned char data2[1] = {0x00};
	unsigned char checksum[6] = {0, };
	int ret;

	i2c_master_send(info->client, data, 6);

	usleep_range(5000, 5000);

	i2c_master_send(info->client, data2, 1);
	usleep_range(5000, 5000);

	ret = abov_tk_i2c_read_data(info->client, checksum, 6);

	input_info(true, &info->client->dev, "%s: ret:%d [%X][%X][%X][%X][%X]\n",
			__func__, ret, checksum[0], checksum[1], checksum[2]
			, checksum[4], checksum[5]);
	info->checksum_h = checksum[4];
	info->checksum_l = checksum[5];
	return 0;
}

static int abov_tk_fw_write(struct abov_tk_info *info, unsigned char *addrH,
						unsigned char *addrL, unsigned char *val)
{
	int length = 36, ret = 0;
	unsigned char data[36];

	data[0] = 0xAC;
	data[1] = 0x7A;
	memcpy(&data[2], addrH, 1);
	memcpy(&data[3], addrL, 1);
	memcpy(&data[4], val, 32);

	ret = i2c_master_send(info->client, data, length);
	if (ret != length) {
		input_err(true, &info->client->dev,
			"%s: write fail[%x%x], %d\n", __func__, *addrH, *addrL, ret);
		return ret;
	}

	usleep_range(3000, 3000);

	abov_tk_check_busy(info);

	return 0;
}

static int abov_tk_fw_mode_enter(struct abov_tk_info *info)
{
	unsigned char data[2] = {0xAC, 0x5B};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 2);
	if (ret != 2) {
		input_err(true, &info->client->dev, "%s: write fail\n", __func__);
		return -1;
	}

	return 0;

}


static int abov_tk_fw_mode_check(struct abov_tk_info *info)
{
	unsigned char buf[1] = {0};
	int ret;

	ret = abov_tk_i2c_read_data(info->client, buf, 1);
	if(ret<0){
		pr_err("%s: write fail\n", __func__);
		return 0;
	}

	dev_info(&info->client->dev, "%s: ret:%02X\n", __func__, buf[0]);

	if(buf[0] == ABOV_FLASH_MODE)	
		return 1;

	pr_err("%s: value is same same,  %X, %X \n", __func__, ABOV_FLASH_MODE, buf[0] );

	return 0;
}

static int abov_tk_flash_erase(struct abov_tk_info *info)
{
	unsigned char data[2] = {0xAC, 0x2D};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 2);
	if (ret != 2) {
		pr_err("%s: write fail\n", __func__);
		return -1;
	}

	return 0;

}

static int abov_tk_fw_mode_exit(struct abov_tk_info *info)
{
	unsigned char data[2] = {0xAC, 0xE1};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 2);
	if (ret != 2) {
		pr_err("%s: write fail\n", __func__);
		return -1;
	}

	return 0;

}

static int abov_tk_fw_update(struct abov_tk_info *info, u8 cmd)
{
	int ret, ii = 0;
	int count;
	unsigned short address;
	unsigned char addrH, addrL;
	unsigned char data[32] = {0, };


	input_info(true, &info->client->dev, "%s start\n", __func__);

	count = info->firm_size / 32;
	address = 0x400;

	input_info(true, &info->client->dev, "%s reset\n", __func__);
	abov_tk_reset_for_bootmode(info);
	msleep(ABOV_BOOT_DELAY);
	ret = abov_tk_fw_mode_enter(info);
	if(ret < 0){
		input_err(true, &info->client->dev,
			"%s:abov_tk_fw_mode_enter fail\n", __func__);
		return ret;
	}
	usleep_range(5 * 1000, 5 * 1000);
	input_info(true, &info->client->dev, "%s fw_mode_cmd sended\n", __func__);

	if(abov_tk_fw_mode_check(info) != 1)
	{
		pr_err("%s: err, flash mode is not: %d\n", __func__, ret);
		return 0;
	}

	ret = abov_tk_flash_erase(info);
	msleep(1400);
	input_info(true, &info->client->dev, "%s fw_write start\n", __func__);

	for (ii = 1; ii < count; ii++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		if (cmd == BUILT_IN) {
			if (info->pdata->support_fw_header)
				memcpy(data, &tsp_firmware[ii * 32], 32);
			else
				memcpy(data, &info->firm_data_bin->data[ii * 32], 32);
		}

		ret = abov_tk_fw_write(info, &addrH, &addrL, data);
		if (ret < 0) {
			input_err(true, &info->client->dev,
				"%s: err, no device : %d\n", __func__, ret);
			return ret;
		}

		address += 0x20;

		memset(data, 0, 32);
	}
	ret = abov_tk_i2c_read_checksum(info);
	input_dbg(true, &info->client->dev, "%s checksum readed\n", __func__);

	ret = abov_tk_fw_mode_exit(info);
	input_info(true, &info->client->dev, "%s fw_write end\n", __func__);

	return ret;
}

static void abov_release_fw(struct abov_tk_info *info, u8 cmd)
{
	switch(cmd) {
	case BUILT_IN:
		if (!info->pdata->support_fw_header)
			release_firmware(info->firm_data_bin);
		break;
	default:
		break;
	}
}

static int abov_flash_fw(struct abov_tk_info *info, bool probe, u8 cmd)
{
	struct i2c_client *client = info->client;
	int retry = 2;
	int ret;
	int block_count;
	const u8 *fw_data;

	switch(cmd) {
	case BUILT_IN:
		if (info->pdata->support_fw_header)
			fw_data = tsp_firmware;
		else
			fw_data = info->firm_data_bin->data;
		break;
	default:
		return -1;
		break;
	}

	block_count = (int)(info->firm_size / 32);

	while (retry--) {
		ret = abov_tk_fw_update(info, cmd);
		if (ret < 0)
			break;
#if ABOV_ISP_FIRMUP_ROUTINE
		abov_tk_reset_for_bootmode(info);
		abov_fw_update(info, fw_data, block_count,
			info->pdata->gpio_scl, info->pdata->gpio_sda);
#endif

		if ((info->checksum_h != info->checksum_h_bin) ||
			(info->checksum_l != info->checksum_l_bin)) {
			input_err(true, &client->dev,
				"%s checksum fail.(0x%x,0x%x),(0x%x,0x%x) retry:%d\n",
				__func__, info->checksum_h, info->checksum_l,
				info->checksum_h_bin, info->checksum_l_bin, retry);
			ret = -1;
			continue;
		} else
			input_info(true, &client->dev, "%s checksum successed.\n", __func__);

		abov_tk_reset_for_bootmode(info);
		msleep(ABOV_RESET_DELAY);
		ret = get_tk_fw_version(info, true);
		if (ret) {
			input_err(true, &client->dev, "%s fw version read fail\n", __func__);
			ret = -1;
			continue;
		}

		if (info->fw_ver == 0) {
			input_err(true, &client->dev, "%s fw version fail (0x%x)\n",
				__func__, info->fw_ver);
//			ret = -1;
//			continue;
		}

		if ((cmd == BUILT_IN) && (info->fw_ver != info->fw_ver_bin)){
			input_err(true, &client->dev, "%s fw version fail 0x%x, 0x%x\n",
				__func__, info->fw_ver, info->fw_ver_bin);
//			ret = -1;
//			continue;
		}		
		ret = 0;
		break;
	}

	return ret;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	char result[16] = {0};
	int ret;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		ret = abov_load_fw(info, BUILT_IN);
		if (ret) {
			input_err(true, &client->dev, "%s fw load fail\n", __func__);
			goto err;
		}

		disable_irq(info->irq);
		info->enabled = false;

		ret = abov_flash_fw(info, true, BUILT_IN);

		info->enabled = true;
		enable_irq(info->irq);

		if (ret) {
			input_err(true, &client->dev, "failed to abov_flash_fw (%d)\n", ret);
			goto err;
		} else
			input_info(true, &client->dev, "fw update success\n");

		info->enabled = true;
		enable_irq(info->irq);

		abov_release_fw(info, BUILT_IN);
		break;
	default:
		goto err;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(result, sizeof(result), "%s", "OK");
	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
	input_info(true, &client->dev, "%s: \"%s\"\n", __func__, sec->cmd_result);
	return;
err:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(result, sizeof(result), "%s", "NG");
	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
	input_info(true, &client->dev, "%s: \"%s\"\n", __func__, sec->cmd_result);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buff, sizeof(buff), "%X", info->fw_ver_bin);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	ret = get_tk_fw_version(info, false);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s read fail\n", __func__);
		info->fw_ver = 0;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buff, sizeof(buff), "%X", info->fw_ver);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);

	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	strncpy(buff, "A96T396", sizeof(buff));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_touchkey_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	touchkey_threshold(info);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buff, sizeof(buff), "%d,%d,%d", info->ch0_thd, info->ch1_thd, info->ch2_thd);

	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void run_touchkey_raw_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 r_buf[MAX_KEY_NUM * 2] = { 0, };
	u16 ch[MAX_KEY_NUM] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	ret = abov_tk_i2c_read(client, ABOV_RAWDATA, r_buf, 6);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	ch[0] = (r_buf[0] << 8) | r_buf[1];
	ch[1] = (r_buf[2] << 8) | r_buf[3];
	ch[2] = (r_buf[4] << 8) | r_buf[5];

	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(buff, sizeof(buff), "%d,%d,%d", ch[0], ch[1], ch[2]);
out:
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "RAW");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void run_touchkey_diff_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 r_buf[MAX_KEY_NUM * 2] = { 0, };
	u16 ch[MAX_KEY_NUM] = { 0 };
	int ret, i, min, max;

	sec_cmd_set_default_result(sec);

	ret = abov_tk_i2c_read(client, ABOV_DIFFDATA, r_buf, 6);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	ch[0] = (r_buf[0] << 8) | r_buf[1];
	ch[1] = (r_buf[2] << 8) | r_buf[3];
	ch[2] = (r_buf[4] << 8) | r_buf[5];

	input_info(true, &client->dev, "%s: %d, %d, %d\n", __func__, ch[0], ch[1], ch[2]);

	min = max = ch[0];
	for (i = 1; i < MAX_KEY_NUM; i++) {
		if (ch[i] < min)
			min = ch[i];
		if (ch[i] > max)
			max = ch[i];
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
out:
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DIFF");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void run_interrupt_gpio_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 cmd_data = 0xff;
	int ret;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s\n", __func__);

	atomic_set(&info->irq_test_state, true);

	ret = abov_tk_i2c_write(info->client, ABOV_INT_TEST, &cmd_data, 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s fail(%d)\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	sec_delay(50);

	if (atomic_read(&info->irq_test_state)) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "0");
	}

out:
	atomic_set(&info->irq_test_state, false);
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "INT_GPIO");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}


static void run_drawing_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 r_buf[MAX_KEY_NUM * 2] = { 0, };
	int ret, ch0, ch1, ch2;

	sec_cmd_set_default_result(sec);

	if (!info->ch0_thd || !info->ch1_thd || !info->ch2_thd)
		touchkey_threshold(info);

	ret = abov_tk_i2c_read(client, ABOV_DIFFDATA, r_buf, 6);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "0,0,0");
		goto out;
	}

	ch0 = (r_buf[0] << 8) | r_buf[1];
	ch1 = (r_buf[2] << 8) | r_buf[3];
	ch2 = (r_buf[4] << 8) | r_buf[5];

	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buff, sizeof(buff), "%d,%d,%d", (ch0 > info->ch0_thd) ? 1 : 0,
									(ch1 > info->ch1_thd) ? 1 : 0,
									(ch2 > info->ch2_thd) ? 1 : 0);
out:
	input_info(true, &client->dev, "%s: %s (%d,%d,%d)\n", __func__,
				buff, info->ch0_thd, info->ch1_thd, info->ch2_thd);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_touchkey_raw_data(sec);
	run_touchkey_diff_data(sec);
	run_interrupt_gpio_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %d %s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static ssize_t touchkey_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);

	touchkey_threshold(info);

	return sprintf(buf, "%d,%d,%d\n", info->ch0_thd, info->ch1_thd, info->ch2_thd);
}

static ssize_t touchkey_raw_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	u8 r_buf[6] = { 0, };
	int ret, ch0, ch1, ch2;

	ret = abov_tk_i2c_read(client, ABOV_RAWDATA, r_buf, 6);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		return ret;
	}

	ch0 = (r_buf[0] << 8) | r_buf[1];
	ch1 = (r_buf[2] << 8) | r_buf[3];
	ch2 = (r_buf[4] << 8) | r_buf[5];

	input_info(true, &client->dev, "%s: %d, %d, %d\n", __func__, ch0, ch1, ch2);

	return sprintf(buf, "%d,%d,%d\n", ch0, ch1, ch2);
}

static ssize_t touchkey_diff_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;
	u8 r_buf[6] = { 0, };
	int ret, ch0, ch1, ch2;

	ret = abov_tk_i2c_read(client, ABOV_DIFFDATA, r_buf, 6);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		return ret;
	}

	ch0 = (r_buf[0] << 8) | r_buf[1];
	ch1 = (r_buf[2] << 8) | r_buf[3];
	ch2 = (r_buf[4] << 8) | r_buf[5];

	input_info(true, &client->dev, "%s: %d, %d, %d\n", __func__, ch0, ch1, ch2);

	return sprintf(buf, "%d,%d,%d\n", ch0, ch1, ch2);
}

static ssize_t touchkey_chip_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;

	input_dbg(true, &client->dev, "%s\n", __func__);
#ifdef CONFIG_KEYBOARD_ABOV_TOUCH_T316
	return sprintf(buf, "A96T316\n");
#else
	return sprintf(buf, "FT1804\n");
#endif
}

static ssize_t bin_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	struct i2c_client *client = info->client;

	input_dbg(true, &client->dev, "fw version bin : 0x%x\n", info->fw_ver_bin);

	return sprintf(buf, "0x%02x\n", info->fw_ver_bin);
}

/* read param */
static ssize_t hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);
	char buff[516];
	char tbuff[128];

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TCOM\":\"%d\"", info->comm_err_count);
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, info->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

/* clear param */
static ssize_t hw_param_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct abov_tk_info *info = container_of(sec, struct abov_tk_info, sec);

	info->comm_err_count = 0;

	return count;
}

static DEVICE_ATTR(touchkey_chip_name, S_IRUGO, touchkey_chip_name, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO, bin_fw_ver, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO, read_fw_ver, NULL);
static DEVICE_ATTR_RO(touchkey_threshold);
static DEVICE_ATTR_RO(touchkey_raw_data);
static DEVICE_ATTR_RO(touchkey_diff_data);
static DEVICE_ATTR_RW(hw_param);

static struct attribute *sec_touchkey_attributes[] = {
	&dev_attr_touchkey_chip_name.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_raw_data.attr,
	&dev_attr_touchkey_diff_data.attr,
	&dev_attr_hw_param.attr,
	NULL,
};

static struct attribute_group sec_touchkey_attr_group = {
	.attrs = sec_touchkey_attributes,
};

extern int get_samsung_lcd_attached(void);

static int abov_tk_fw_check(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	int ret, fw_update=0;
	u8 buf;

	ret = abov_load_fw(info, BUILT_IN);
	if (ret) {
		input_err(true, &client->dev,
			"%s fw load fail\n", __func__);
		return ret;
	}

	ret = get_tk_fw_version(info, true);
	//Check Model No.
	ret = abov_tk_i2c_read(client, ABOV_MODEL_NUMBER, &buf, 1);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
	}
	if(info->fw_model_number != buf){
		input_info(true, &client->dev, "fw model number = %x ic model number = %x \n", info->fw_model_number, buf);
		fw_update = 1;
		goto flash_fw;
	}

	if ((info->fw_ver == 0) || info->fw_ver < info->fw_ver_bin){
		input_dbg(true, &client->dev, "excute tk firmware update (0x%x -> 0x%x\n",
			info->fw_ver, info->fw_ver_bin);
		fw_update = 1;
	}

	if (info->pdata->bringup == 1) {
		input_info(true, &client->dev, "%s: firmware update skip, bring up\n", __func__);
		fw_update = 0;
	} else if ((info->pdata->bringup == 3) && (info->fw_ver != info->fw_ver_bin)) {
		input_info(true, &client->dev, "%s: force firmware update\n", __func__);
		fw_update = 1;
	}

flash_fw:

	if(fw_update){
		ret = abov_flash_fw(info, true, BUILT_IN);
		if (ret) {
			input_err(true, &client->dev,
				"failed to abov_flash_fw (%d)\n", ret);
		} else {
			input_info(true, &client->dev,
				"fw update success\n");
		}
	}

	abov_release_fw(info, BUILT_IN);

	return ret;
}

static int abov_power_regulator_ctrl(void *data, bool on)
{
	struct abov_tk_info *info = (struct abov_tk_info *)data;
	struct i2c_client *client = info->client;
	int ret = 0;

	if (on) {
		if (info->pdata->dvdd_vreg) {
			ret = regulator_enable(info->pdata->dvdd_vreg);
			if(ret){
				input_err(true, &client->dev, "%s : dvdd reg enable fail\n", __func__);
				return ret;
			}
		}
	} else {
		if (info->pdata->dvdd_vreg) {
			ret = regulator_disable(info->pdata->dvdd_vreg);
			if(ret){
				input_err(true, &client->dev, "%s : dvdd reg disable fail\n", __func__);
				return ret;
			}
		}
	}

	input_info(true, &client->dev, "%s %s\n", __func__, on ? "on" : "off");

	return ret;
}

static int abov_power_gpio_ctrl(void *data, bool on)
{
	struct abov_tk_info *info = (struct abov_tk_info *)data;
	
	gpio_set_value(info->pdata->gpio_en, on);

	input_info(true, &info->client->dev, "%s %s\n", __func__, on ? "on" : "off");

	return 0;
}

#ifdef CONFIG_OF
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	if(of_property_read_bool(np, "abov,power-gpioen")) {
		pdata->gpio_en = of_get_named_gpio(np, "abov,power-gpio", 0);
		if(pdata->gpio_en < 0){
			input_err(true, dev, "unable to get gpio_int\n");
			return pdata->gpio_en;
		}
		
		if (gpio_is_valid(pdata->gpio_en)) {
			ret = gpio_request(pdata->gpio_en, "tkey_power_en");
			if(ret < 0){
				input_err(true, dev,
					"unable to request gpio_en\n");
				return ret;
			}
			
			ret = gpio_direction_output(pdata->gpio_en, 0);
			if (ret) {
				input_err(true, dev, "%s: unable to set direction for gpio [%d]\n",
						__func__, pdata->gpio_en);
			}
		}
		
		/* Set power control function */
		pdata->power = abov_power_gpio_ctrl;
		
		input_info(true, dev, "%s: power control by gpio (%d) \n", __func__, pdata->gpio_en);
	} else {
		// TO DO : Regulator get if any
		pdata->dvdd_vreg = devm_regulator_get(dev, "abov_vddo");
		if (IS_ERR(pdata->dvdd_vreg))
			input_err(true, dev, "%s : dvdd_vreg get error, ignoring\n", __func__);

		pdata->power = abov_power_regulator_ctrl;
		
		input_info(true, dev, "%s: power control by regulator\n", __func__);
	}

	pdata->gpio_int = of_get_named_gpio(np, "abov,irq-gpio", 0);
	if(pdata->gpio_int < 0){
		input_err(true, dev, "unable to get gpio_int\n");
		return pdata->gpio_int;
	} else {
		if (gpio_is_valid(pdata->gpio_int)) {
			ret = gpio_request(pdata->gpio_int, "tkey_gpio_int");
			if(ret < 0){
				input_err(true, dev,
					"unable to request gpio_int\n");
				return ret;
			}
			
			ret = gpio_direction_input(pdata->gpio_int);
			if (ret) {
				input_err(true, dev, "%s: unable to set direction for gpio [%d]\n",
						__func__, pdata->gpio_int);
			}
		}
	}

	pdata->gpio_scl = of_get_named_gpio(np, "abov,scl-gpio", 0);
	if(pdata->gpio_scl < 0){
		input_err(true, dev, "unable to get gpio_scl\n");
		return pdata->gpio_scl;
	}

	pdata->gpio_sda = of_get_named_gpio(np, "abov,sda-gpio", 0);
	if(pdata->gpio_sda < 0){
		input_err(true, dev, "unable to get gpio_sda\n");
		return pdata->gpio_sda;
	}

	ret = of_property_read_string(np, "abov,fw_path", (const char **)&pdata->fw_path);

	if (ret) {
		input_err(true, dev, "touchkey:failed to read fw_path %d\n", ret);
		pdata->fw_path = TK_FW_PATH_BIN;
	}
	input_info(true, dev, "%s: fw path %s\n", __func__, pdata->fw_path);

	pdata->support_fw_header = of_property_read_bool(np, "support_fw_header");

	pdata->boot_on_ldo = of_property_read_bool(np, "abov,boot-on-ldo");
	if (of_property_read_u32(np, "abov,bringup", &pdata->bringup) < 0)
		pdata->bringup = 0;

	pdata->ta_notifier = of_property_read_bool(np, "abov,ta-notifier");

	input_info(true, dev, "%s: gpio_int:%d, gpio_scl:%d, gpio_sda:%d bringup:%d\n",
			__func__, pdata->gpio_int, pdata->gpio_scl,
			pdata->gpio_sda, pdata->bringup);
	return 0;
}
#else
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* CONFIG_OF */

struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_threshold", get_touchkey_threshold),},
	{SEC_CMD("run_raw_data", run_touchkey_raw_data),},
	{SEC_CMD("run_diff_data", run_touchkey_diff_data),},
	{SEC_CMD("run_drawing_test", run_drawing_test),},
	{SEC_CMD("run_interrupt_gpio_test", run_interrupt_gpio_test),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

void abov_input_device(struct abov_tk_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, 1, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, 512, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, 512, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
}

static int abov_tk_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct abov_tk_info *info;
	struct input_dev *input_dev;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev,
			"i2c_check_functionality fail\n");
		return -EIO;
	}

	info = kzalloc(sizeof(struct abov_tk_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev,
			"Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;

	if (client->dev.of_node) {
		struct abov_touchkey_platform_data *pdata;
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct abov_touchkey_platform_data), GFP_KERNEL);
		if (!pdata) {
			input_err(true, &client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_config;
		}

		ret = abov_parse_dt(&client->dev, pdata);
		if (ret){
			input_err(true, &client->dev, "failed to abov_parse_dt\n");
			ret = -ENOMEM;
			goto err_config;
		}

		info->pdata = pdata;
	} else
		info->pdata = client->dev.platform_data;

	if (info->pdata == NULL) {
		input_err(true, &client->dev, "failed to get platform data\n");
		goto err_config;
	}

	if (info->pdata->power)
		info->pdata->power(info, true);

	if(!info->pdata->boot_on_ldo)
		msleep(ABOV_RESET_DELAY);

	info->enabled = true;
	info->irq = -1;
	client->irq = gpio_to_irq(info->pdata->gpio_int);

	mutex_init(&info->lock);

	info->input_event = info->pdata->input_event;
	info->touchkey_count = sizeof(touchkey_keycode) / sizeof(int);
	i2c_set_clientdata(client, info);

	ret = abov_tk_fw_check(info);
	if (ret) {
		input_err(true, &client->dev,
			"failed to firmware check (%d)\n", ret);
		goto err_reg_input_dev;
	}

	ret = get_tk_fw_version(info, false);
	if (ret < 0) {
		input_err(true, &client->dev, "%s read fail\n", __func__);
		goto err_reg_input_dev;
	}

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchkey";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;
	input_dev->id.vendor = 0x00AB;
	input_dev->id.product = 0x00E1;
#ifdef USE_OPEN_CLOSE
	input_dev->open = abov_tk_input_open;
	input_dev->close = abov_tk_input_close;
#endif /* USE_OPEN_CLOSE */

/*
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(TOUCHPAD_SWIPE_FORWARD, input_dev->keybit);
	set_bit(TOUCHPAD_SWIPE_BACKWARD, input_dev->keybit);
*/
	abov_input_device(info);

	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		input_err(true, &client->dev, "failed to register input dev (%d)\n",
			ret);
		goto err_reg_input_dev;
	}

	if (!info->pdata->irq_flag) {
		input_err(true, &client->dev, "no irq_flag\n");
		ret = request_threaded_irq(client->irq, NULL, abov_tk_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ABOV_TK_NAME, info);
	} else {
		ret = request_threaded_irq(client->irq, NULL, abov_tk_interrupt,
			info->pdata->irq_flag, ABOV_TK_NAME, info);
	}
	if (ret < 0) {
		input_err(true, &client->dev, "Failed to register interrupt\n");
		goto err_req_irq;
	}
	info->irq = client->irq;

#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
	ret = sec_cmd_init(&info->sec, info->dev, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TKEY, &sec_touchkey_attr_group);
	if (ret < 0)
		input_err(true, info->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
#else
	info->dev = sec_device_create(info, "sec_touchkey");
	if (IS_ERR(info->dev))
		input_err(true, &client->dev,
		"Failed to create device for the touchkey sysfs\n");

	ret = sysfs_create_group(&info->dev ->kobj,
		&sec_touchkey_attr_group);
	if (ret)
		input_err(true, &client->dev, "Failed to create sysfs group\n");

	ret = sysfs_create_link(&info->dev ->kobj,
		&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &client->dev,
			"%s: Failed to create input symbolic link\n",
			__func__);
	}
#endif

	input_err(true, &client->dev, "%s done\n", __func__);
	return 0;

err_req_irq:
	input_unregister_device(input_dev);
err_reg_input_dev:
	mutex_destroy(&info->lock);
	gpio_free(info->pdata->gpio_int);
	if (info->pdata->power)
		info->pdata->power(info, false);
err_config:
	input_free_device(input_dev);
err_input_alloc:
	kfree(info);
err_alloc:
	input_err(true, &client->dev, "%s fail\n", __func__);
	return ret;

}

static int abov_tk_remove(struct i2c_client *client)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);

/*	if (info->enabled)
		info->pdata->power(0);
*/
	info->enabled = false;
	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	kfree(info);

	return 0;
}

static void abov_tk_shutdown(struct i2c_client *client)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);

	input_info(true, &client->dev, "Inside abov_tk_shutdown \n");

	if (info->enabled){
		disable_irq(info->irq);
		info->pdata->power(info, false);
	}
	info->enabled = false;
}

#if defined(CONFIG_PM)
static int abov_tk_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct abov_tk_info *info = i2c_get_clientdata(client);

	if (!info->enabled) {
		input_info(true, &client->dev, "%s: already power off\n", __func__);
		return 0;
	}

	input_info(true, &client->dev, "%s: users=%d\n", __func__,
		   info->input_dev->users);

	disable_irq(info->irq);
	info->enabled = false;
	release_all_fingers(info);

	if (info->pdata->power)
		info->pdata->power(info, false);
	return 0;
}

static int abov_tk_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct abov_tk_info *info = i2c_get_clientdata(client);

	if (info->enabled) {
		input_info(true, &client->dev, "%s: already power on\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s: users=%d\n", __func__,
		   info->input_dev->users);

	if (info->pdata->power) {
		info->pdata->power(info, true);
		msleep(ABOV_RESET_DELAY);
	} else /* touchkey on by i2c */
		get_tk_fw_version(info, true);

	info->enabled = true; 	
	enable_irq(info->irq);

	return 0;
}
#endif

#ifdef USE_OPEN_CLOSE
static int abov_tk_input_open(struct input_dev *dev)
{
	struct abov_tk_info *info = input_get_drvdata(dev);

	input_dbg(true, &info->client->dev, "%s %d \n", __func__, __LINE__);
	input_info(true, &info->client->dev, "%s: users=%d, v:0x%02x\n", __func__,
		info->input_dev->users, info->fw_ver);

	abov_tk_resume(&info->client->dev);

	return 0;
}
static void abov_tk_input_close(struct input_dev *dev)
{
	struct abov_tk_info *info = input_get_drvdata(dev);

	input_dbg(true, &info->client->dev, "%s %d \n", __func__, __LINE__);
	input_info(true, &info->client->dev, "%s: users=%d\n", __func__,
		   info->input_dev->users);

	abov_tk_suspend(&info->client->dev);
}
#endif /* USE_OPEN_CLOSE */
 
#if defined(CONFIG_PM) &&!defined(USE_OPEN_CLOSE)
static const struct dev_pm_ops abov_tk_pm_ops = {
	.suspend = abov_tk_suspend,
	.resume = abov_tk_resume,
};
#endif

static const struct i2c_device_id abov_tk_id[] = {
	{ABOV_TK_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, abov_tk_id);

#ifdef CONFIG_OF
static struct of_device_id abov_match_table[] = {
	{ .compatible = "abov,a96t396xx",},
	{ },
};
#else
#define abov_match_table NULL
#endif

static struct i2c_driver abov_tk_driver = {
	.probe = abov_tk_probe,
	.remove = abov_tk_remove,
	.shutdown = abov_tk_shutdown,
	.driver = {
		   .name = ABOV_TK_NAME,
		   .of_match_table = abov_match_table,		   
#if defined(CONFIG_PM) && !defined(USE_OPEN_CLOSE)
		   .pm = &abov_tk_pm_ops,
#endif 
	},
	.id_table = abov_tk_id,
};

static int __init touchkey_init(void)
{
	return i2c_add_driver(&abov_tk_driver);
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&abov_tk_driver);
}

module_init(touchkey_init);
module_exit(touchkey_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Touchkey driver for Abov MF18xx chip");
MODULE_LICENSE("GPL");
