// SPDX-License-Identifier: GPL-2.0
/*
 * driver/../s2mm006_fw.c - S2MM006 fw function
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#define pr_fmt(fmt) "[PDIC] " fmt

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/usb/typec/common/pdic_sysfs.h>
#include <linux/usb_notify.h>
#include "s2mm006_ext.h"
#include "S2MM006_Factory_FW.h"
#include "S2MM006_Main_FW.h"

#define	S2MM006_FIRMWARE_PATH	"usbpd/s2mm006.bin"

#define FW_CHECK_RETRY 5
#define VALID_FW_BOOT_VERSION(fw_boot) (fw_boot == 0x8)
#define VALID_FW_MAIN_VERSION(fw_main) \
	(!((fw_main[0] == 0xff) && (fw_main[1] == 0xff)) \
		&& !((fw_main[0] == 0x00) && (fw_main[1] == 0x00)))

//#define ADDR_CTRL	0x92
#define ADDR_CTRL			0x49 /* 0x92 */
#define ADDR_DMA_AREA1		0x31 /* 0x62 */
#define ADDR_DMA_AREA2		0x32 /* 0x64 */
#define ADDR_DEVICE			0x34 /* 0x68 */
#define ADDR_SRAM			0x33 /* 0x66 */

typedef enum {
	OM_HIGH,
	OM_LOW,
	OM_RESET
} t_om_ctrl;

uint8_t s2mm006_get_header_fw_ver(int id)
{
	if (id == S2MM006_MAIN)
		return BOOT_FLASH_FW_MAIN[1];
	else if (id == S2MM006_MAIN_TEST)
		return BOOT_FLASH_FW_MAIN[2];
	else if (id == S2MM006_FACTORY_BOOT)
		return BOOT_FLASH_FW[0];
	else
		return BOOT_FLASH_FW_MAIN[1];
}

uint8_t s2mm006_get_bin_fw_ver(int id, const uint8_t *fw)
{
	if (id == S2MM006_MAIN)
		return fw[1];
	else if (id == S2MM006_MAIN_TEST)
		return fw[2];
	else if (id == S2MM006_FACTORY_BOOT)
		return fw[0];
	else
		return fw[1];
}

const char *flashmode_to_string(u32 mode)
{
	switch (mode) {
	case FLASH_MODE_NORMAL:
		return "FLASH_MODE_NORMAL";
	case FLASH_MODE_FLASH:
		return "FLASH_MODE_FLASH";
	case FLASH_MODE_ERASE:
		return "FLASH_MODE_ERASE";
	default:
		return "?";
	}
}

int s2mm006_fw_write_ctrl_4bytes(const struct i2c_client *i2c, u8 addr, u16 reg, u8 *val)
{
	int ret, i2c_retry; u8 buf[258] = {0,};
	u16 size = 4;
	struct i2c_msg msg[1];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	mutex_lock(&usbpd_data->i2c_mutex);
	i2c_retry = 0;
	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = size+2;
	msg[0].buf = buf;

	buf[0] = (reg & 0xFF00) >> 8;
	buf[1] = (reg & 0xFF);
	memcpy(&buf[2], val, size);

	do {
		ret = i2c_transfer(i2c->adapter, msg, 1);
	} while (ret < 0 &&  i2c_retry++ < 5);

	if (ret < 0)
		pr_err("i2c write fail reg:0x%x error %d\n", reg, ret);

	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}

int s2mm006_fw_write_ctrl_byte(const struct i2c_client *i2c, u8 addr, u16 reg, u8 *val)
{
	int ret, i2c_retry; u8 buf[258] = {0,};
	u16 size = 1;
	struct i2c_msg msg[1];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	mutex_lock(&usbpd_data->i2c_mutex);
	i2c_retry = 0;
	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = size+2;
	msg[0].buf = buf;

	buf[0] = (reg & 0xFF00) >> 8;
	buf[1] = (reg & 0xFF);
	memcpy(&buf[2], val, size);

	do {
		ret = i2c_transfer(i2c->adapter, msg, 1);
	} while (ret < 0 &&  i2c_retry++ < 5);

	if (ret < 0)
		pr_err("i2c write fail reg:0x%x error %d\n", reg, ret);

	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}

int s2mm006_fw_read_ctrl_byte(const struct i2c_client *i2c, u8 addr, u16 reg, u8 *val, u16 size)
{
	int ret, i2c_retry; u8 wbuf[2];
	struct i2c_msg msg[2];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&usbpd_data->i2c_mutex);
	i2c_retry = 0;
	msg[0].addr = addr;
	msg[0].flags = i2c->flags;
	msg[0].len = 2;
	msg[0].buf = wbuf;
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = val;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	do {
		ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
	} while (ret < 0 &&  i2c_retry++ < 5);

	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		pr_err("i2c read16 fail reg:0x%x error %d\n", reg, ret);
	}
	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}


int s2mm006_fw_write_data_byte(const struct i2c_client *i2c, u8 addr, u16 reg, u8 *val, u16 size)
{
	int ret, i2c_retry; u8 buf[258] = {0,};
	struct i2c_msg msg[1];
	struct s2mm006_data *usbpd_data = i2c_get_clientdata(i2c);

	mutex_lock(&usbpd_data->i2c_mutex);
	i2c_retry = 0;
	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = size+2;
	msg[0].buf = buf;

	buf[0] = (reg & 0xFF00) >> 8;
	buf[1] = (reg & 0xFF);
	memcpy(&buf[2], val, size);

	do {
		ret = i2c_transfer(i2c->adapter, msg, 1);
	} while (ret < 0 &&  i2c_retry++ < 5);

	if (ret < 0)
		pr_err("i2c write fail reg:0x%x error %d\n", reg, ret);

	mutex_unlock(&usbpd_data->i2c_mutex);

	return ret;
}

int s2mm006_fw_set_om(struct s2mm006_data *usbpd_data, t_om_ctrl om_ctrl)
{
	/* Set OM pin GPIO */
	/* TODO : GPIO Control implementation */
	switch (om_ctrl) {
	case OM_HIGH:
		gpio_direction_output(usbpd_data->s2mm006_om, 1);
		msleep(30);
		break;
	case OM_LOW:
		gpio_direction_output(usbpd_data->s2mm006_om, 0);
		msleep(80);
		break;
	case OM_RESET:
		s2mm006_om_reset(usbpd_data);
		break;
	default:
		break;
	}
	pr_info("%s:(%s)after hrst_om gpio value %d\n", __func__,
			(om_ctrl == OM_HIGH)?"HIGH":((om_ctrl == OM_LOW)?"LOW":"RESET"),
			gpio_get_value(usbpd_data->s2mm006_om));
	return 0;
}

void s2mm006_main_fw_flash_DMA_without_erase(struct s2mm006_data *usbpd_data)
{
	u8 wdata[4] = {0,};
	u32 *ldata;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint32_t j = 0;
	uint16_t t_size = 0;
	uint32_t offset = 0;
	uint8_t *pBuf;
	const uint8_t *fw_buf = (void *)BOOT_FLASH_FW_MAIN;
	size_t fw_size = ARRAY_SIZE(BOOT_FLASH_FW_MAIN);

	if (fw_size < 0x10) {
		pr_err("FW_UPDATE fail, abnormal size: (%lu)\n", fw_size);
		return;
	}

	ldata = (u32 *)wdata;

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	pr_info("Flash Current State = 0x%02X\n", wdata[0]);
	msleep(200);

	pBuf = (uint8_t *)&fw_buf[0xC];
	j = 1;
	offset = 0;
	t_size = 0;
	do {
		if ((j++ * 256) > (fw_size - 0x10)) {
			/* Pad */
			t_size = (fw_size - 0x10) - offset;
			if (offset >= (0x10000-0x3000)) {
				/* 0x64 */
				s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2, (offset - 0xD000), pBuf, t_size);
			} else {
				/* 0x62 */
				s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1, 0x3000 + offset, pBuf, t_size);
			}
			break;
		}
		t_size = 256;
		if (offset >= (0x10000-0x3000)) {
			/* 0x64 */
			s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2, (offset - 0xD000), pBuf, t_size);
		} else {
			/* 0x62 */
			s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1, 0x3000 + offset, pBuf, t_size);
		}

		offset += t_size;
		pBuf += t_size;
	} while (1);

	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,
					0x1FF0, (u8 *)&fw_buf[0], 4);
	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,
					0x1FF4, (u8 *)&fw_buf[8], 4);
	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,
					0x1FF8, (u8 *)&fw_buf[fw_size - 0x10 + 0xC], 4);
	usleep_range(100, 110);
	*ldata = 0x01;
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,	0x1FFC, wdata, 4);

	usleep_range(100, 110);
}

int s2mm006_fw_factory_flash_DMA(struct s2mm006_data *usbpd_data, void *buf, size_t size)
{
	u8 wdata[4] = {0,};
	u32 *ldata;
	u8 wait_cnt = 0;
	struct i2c_client *i2c = usbpd_data->i2c;
	int ret = 0;
	uint32_t j = 0;
	uint16_t t_size = 0;
	uint32_t offset = 0;
	uint8_t *pBuf;
	const uint8_t *fw_buf = buf;
	size_t fw_size = size;

	if (fw_size < 0x10) {
		ret = -1;
		pr_err("FW_UPDATE fail, abnormal size: (%lu)\n", fw_size);
		goto END_OF_FW_UPDATE;
	}

	ldata = (u32 *)wdata;
	s2mm006_fw_set_om(usbpd_data, OM_HIGH);

	wdata[0] = 0x09;
	wdata[1] = 0x00;
	wdata[2] = 0x00;
	wdata[3] = 0x00;

	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x2000, wdata);
	usleep_range(10, 11);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DEVICE, 0x2000, wdata, 4);
	pr_info("Reset Core Setting 0x2000 = 0x%02X%02X%02X%02X\n",
						wdata[3], wdata[2], wdata[1], wdata[0]);

	*ldata = 0x40000000;
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x1030, wdata);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DEVICE, 0x1030, wdata, 4);
	pr_info("Reset DMA 0x1030 = 0x%02X%02X%02X%02X\n",
						wdata[3], wdata[2], wdata[1], wdata[0]);

	*ldata = 0x1F;
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x101C, wdata);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DEVICE, 0x101C, wdata, 4);
	pr_info("DMA Flash Enable 0x101C = 0x%02X%02X%02X%02X\n",
						wdata[3], wdata[2], wdata[1], wdata[0]);

	while (wait_cnt++ < 3) {
		msleep(50);
		s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
		pr_info("Flash Current State = 0x%02X\n", wdata[0]);

		if (wdata[0] == 0x01)
			break;
	}
	if (wait_cnt >= 3) {
		ret = -1;
		goto END_OF_FLASH;
	}

	wait_cnt = 0;

	usleep_range(100, 110);
	pr_info("Reset Halt On sequence start\n");

	wdata[0] = 0x01;
	s2mm006_fw_write_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata);

	usleep_range(100, 110);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata, 1);
	pr_info("Reset Halt On 0xF004 = 0x%02X\n", wdata[0]);

	while (wait_cnt++ < 3) {
		msleep(50);
		s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
		pr_info("Flash Current State = 0x%02X\n", wdata[0]);
		if (wdata[0] == 0x02)
			break;
	}

	if (wait_cnt >= 3) {
		ret = -1;
		goto END_OF_FLASH;
	}

	usleep_range(100, 110);

	/* Flash Erase Start */
	*ldata = 0x40; //FLASH_AREA_SECTOR_ALL
	ret = s2mm006_fw_write_data_byte(i2c, ADDR_CTRL, 0xF008, wdata, 1);
	if (ret < 0) {
		pr_info("%s, FLASH Erase Fail\n", __func__);
		ret = -1;
		goto END_OF_FLASH;
	} else 
		ret = 0;

	msleep(50);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF008, wdata, 1);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	if (wdata[0] != 4) {
		pr_info("%s, FLASH Erase Fail\n", __func__);
		ret = -1;
		goto END_OF_FLASH;
	}

	pr_info("Flash Erase Done\n");

	usleep_range(100, 110);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	pr_info("Flash Current State = 0x%02X\n", wdata[0]);
	msleep(200);

	pBuf = (uint8_t *)&fw_buf[0xC];
	j = 1;
	offset = 0;
	t_size = 0;
	do {
		if ((j++ * 256) > (fw_size - 0x10)) {
			/* Pad */
			t_size = (fw_size - 0x10) - offset;
			/* 0x62 */
			s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1, offset, pBuf, t_size);
			break;
		}
		t_size = 256;
		/* 0x62 */
		s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1, offset, pBuf, t_size);
		offset += t_size;
		pBuf += t_size;
	} while (1);

	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1,
					0x2FF0, (u8 *)&fw_buf[0], 4);
	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1,
					0x2FF4, (u8 *)&fw_buf[8], 4);
	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1,
					0x2FF8, (u8 *)&fw_buf[fw_size - 0x10 + 0xC], 4);

	usleep_range(100, 110);
	*ldata = 0x01;
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1,	0x2FFC, wdata, 4);

	usleep_range(100, 110);

	/* MAIN FW Update */
	s2mm006_main_fw_flash_DMA_without_erase(usbpd_data);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF00C, wdata, 1);
	pr_info("CRC8 Out : 0x%02X\n", wdata[0]);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FF0, wdata, 4); //read version
	pr_info("Flash : VERSION : 0x%04X\n", *ldata);
	pr_info("BIN : VERSION: 0x%02X %02X\n", BOOT_FLASH_FW_MAIN[1], BOOT_FLASH_FW_MAIN[0]);

	if((BOOT_FLASH_FW_MAIN[1] != wdata[1]) || (BOOT_FLASH_FW_MAIN[0] != wdata[0])) {
		pr_info("[ERROR] USER F/W Download Fail !!! ver_err\n");
		ret = -1;
	}
	
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FF4, wdata, 4); //read BIN SIZE
	pr_info("Flash : SIZE : 0x%08X\n", *ldata);
	pr_info("BIN : SIZE : 0x%02X %02X %02X %02X\n", BOOT_FLASH_FW_MAIN[11], BOOT_FLASH_FW_MAIN[10], BOOT_FLASH_FW_MAIN[9], BOOT_FLASH_FW_MAIN[8]);
	if ((BOOT_FLASH_FW_MAIN[11] != wdata[3]) || (BOOT_FLASH_FW_MAIN[10] != wdata[2]) || (BOOT_FLASH_FW_MAIN[9] != wdata[1]) || (BOOT_FLASH_FW_MAIN[8] != wdata[0])) {
		pr_info("[ERROR] USER F/W Donwlaod Fail !!! size_err\n\r");
		ret = -1;
	}

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FF8, wdata, 1); //read CHECKSUM
	pr_info("Flash : CHECKSUM : 0x%02X\n", wdata[0]);
	pr_info("BIN : CHECKSUM: 0x%02X\n", BOOT_FLASH_FW_MAIN[sizeof(BOOT_FLASH_FW_MAIN) - 0x10 + 0xC]);

	if (BOOT_FLASH_FW_MAIN[sizeof(BOOT_FLASH_FW_MAIN) - 0x10 + 0xC] != wdata[0]) {
		pr_info("[ERROR] USER F/W Donwlaod Fail !!! checksum_err\n");
		ret = -1;
	}

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FFC, wdata, 4); //read FINISH FLAG
	pr_info("Flash : FINISH : 0x%08X\n", *ldata);
	if (*ldata != 1) {
		pr_info("[ERROR] USER F/W Donwlaod Fail !!! finish_err\n");
		ret = -1;
	}

	usleep_range(100, 110);
	wdata[0] = 0;
	s2mm006_fw_write_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata);

	usleep_range(100, 110);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	pr_info("Flash Current State = 0x%02X\n", wdata[0]);

	/* Reset Core */
	*ldata = 0;
	usleep_range(100, 110);
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x2000, wdata);
	usleep_range(10, 11);

END_OF_FLASH:
	s2mm006_fw_set_om(usbpd_data, OM_RESET);

	msleep(300);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_SRAM, 0x01, wdata, 1);
	pr_info("%s, FW Ver : %02X\n", __func__, wdata[0]);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_SRAM, 0x18, wdata, 1);
	pr_info("%s, Integrity Check : %s\n", __func__, ((wdata[0] & 0x10) == 0) ? "OK":"FAIL");


END_OF_FW_UPDATE:
	return ret;
/*
	usleep_range(100, 110);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF00C, wdata, 1);
	pr_info("CRC8 Out : 0x%02X\n", wdata[0]);

	usleep_range(100, 110);
	wdata[0] = 0;
	s2mm006_fw_write_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata);

	usleep_range(100, 110);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	pr_info("Flash Current State = 0x%02X\n", wdata[0]);
*/

	/* Reset Core */
/*
	*ldata = 0;
	usleep_range(100, 110);
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x2000, wdata);
	usleep_range(10, 11);

END_OF_FLASH:
	s2mm006_fw_set_om(usbpd_data, OM_RESET);

	msleep(300);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_SRAM, 0x01, wdata, 1);
	pr_info("%s, FW Ver : %x\n", __func__, wdata[0]);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_SRAM, 0x18, wdata, 1);
	pr_info("%s, Integrity Check : %s\n", __func__, ((wdata[0] & 0x10) == 0) ? "OK":"FAIL");


	return ret;
*/
}

int s2mm006_fw_flash_DMA(struct s2mm006_data *usbpd_data, void *buf, size_t size)
{
	u8 wdata[4] = {0,};
	u32 *ldata;
	u8 wait_cnt = 0, i = 0;
	struct i2c_client *i2c = usbpd_data->i2c;
	int ret = 0;
	uint32_t j = 0;
	uint16_t t_size = 0;
	uint32_t offset = 0;
	uint8_t *pBuf;
	const uint8_t *fw_buf = buf;
	size_t fw_size = size;

	if (fw_size < 0x10) {
		ret = -1;
		pr_err("FW_UPDATE fail, abnormal size: (%lu)\n", fw_size);
		goto END_OF_FW_UPDATE;
	}

	ldata = (u32 *)wdata;
	s2mm006_fw_set_om(usbpd_data, OM_HIGH);

	wdata[0] = 0x09;
	wdata[1] = 0x00;
	wdata[2] = 0x00;
	wdata[3] = 0x00;

	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x2000, wdata);
	usleep_range(10, 11);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DEVICE, 0x2000, wdata, 4);
	pr_info("Reset Core Setting 0x2000 = 0x%02X%02X%02X%02X\n",
						wdata[3], wdata[2], wdata[1], wdata[0]);

	*ldata = 0x40000000;
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x1030, wdata);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DEVICE, 0x1030, wdata, 4);
	pr_info("Reset DMA 0x1030 = 0x%02X%02X%02X%02X\n",
						wdata[3], wdata[2], wdata[1], wdata[0]);

	*ldata = 0x1F;
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x101C, wdata);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DEVICE, 0x101C, wdata, 4);
	pr_info("DMA Flash Enable 0x101C = 0x%02X%02X%02X%02X\n",
						wdata[3], wdata[2], wdata[1], wdata[0]);

	while (wait_cnt++ < 3) {
		msleep(50);
		s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
		pr_info("Flash Current State = 0x%02X\n", wdata[0]);

		if (wdata[0] == 0x01)
			break;
	}
	if (wait_cnt >= 3) {
		ret = -1;
		goto END_OF_FLASH;
	}

	wait_cnt = 0;

	usleep_range(100, 110);
	pr_info("Reset Halt On sequence start\n");

	wdata[0] = 0x01;
	s2mm006_fw_write_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata);

	usleep_range(100, 110);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata, 1);
	pr_info("Reset Halt On 0xF004 = 0x%02X\n", wdata[0]);

	while (wait_cnt++ < 3) {
		msleep(50);
		s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
		pr_info("Flash Current State = 0x%02X\n", wdata[0]);
		if (wdata[0] == 0x02)
			break;
	}

	if (wait_cnt >= 3) {
		ret = -1;
		goto END_OF_FLASH;
	}

	usleep_range(100, 110);

	/* Flash Erase Start */

	for(j = 0; j < 5; ++j) {
		for (i = 4; i <= 18; i++) {
			*ldata = i;
			s2mm006_fw_write_ctrl_byte(i2c, ADDR_CTRL, 0xF008, wdata);
			usleep_range(20000, 21000);
			s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF008, wdata, 1);
			s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
			if (wdata[0] != 4) {
				pr_info("flash erase failed, i(%d)\n", i);
				ret = -1;
				goto END_OF_FLASH;
			}
		}
	}
	pr_info("Flash Erase Done\n");

	usleep_range(100, 110);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	pr_info("Flash Current State = 0x%02X\n", wdata[0]);
	msleep(200);

	pBuf = (uint8_t *)&fw_buf[0xC];
	j = 1;
	offset = 0;
	t_size = 0;
	do {
		if ((j++ * 256) > (fw_size - 0x10)) {
			/* Pad */
			t_size = (fw_size - 0x10) - offset;
			if (offset >= (0x10000-0x3000)) {
				/* 0x64 */
				s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2, (offset - 0xD000), pBuf, t_size);
			} else {
				/* 0x62 */
				s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1, 0x3000 + offset, pBuf, t_size);
			}
			break;
		}
		t_size = 256;
		if (offset >= (0x10000-0x3000)) {
			/* 0x64 */
			s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2, (offset - 0xD000), pBuf, t_size);
		} else {
			/* 0x62 */
			s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA1, 0x3000 + offset, pBuf, t_size);
		}

		offset += t_size;
		pBuf += t_size;
	} while (1);

	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,
					0x1FF0, (u8 *)&fw_buf[0], 4);
	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,
					0x1FF4, (u8 *)&fw_buf[8], 4);
	usleep_range(100, 110);
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,
					0x1FF8, (u8 *)&fw_buf[fw_size - 0x10 + 0xC], 4);
	usleep_range(100, 110);
	*ldata = 0x01;
	s2mm006_fw_write_data_byte(i2c, ADDR_DMA_AREA2,	0x1FFC, wdata, 4);

	usleep_range(100, 110);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF00C, wdata, 1);
	pr_info("CRC8 Out : 0x%02X\n", wdata[0]);

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FF0, wdata, 4); //read version
	pr_info("Flash : VERSION : 0x%04X\n", *ldata);
	pr_info("BIN : VERSION: 0x%02X %02X\n", fw_buf[1], fw_buf[0]);

	if((fw_buf[1] != wdata[1]) || (fw_buf[0] != wdata[0])) {
		pr_info("[ERROR] USER F/W Download Fail !!! ver_err\n");
		ret = -1;
	}
	
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FF4, wdata, 4); //read BIN SIZE
	pr_info("Flash : SIZE : 0x%08X\n", *ldata);
	pr_info("BIN : SIZE : 0x%02X %02X %02X %02X\n", fw_buf[11], fw_buf[10], fw_buf[9], fw_buf[8]);
	if ((fw_buf[11] != wdata[3]) || (fw_buf[10] != wdata[2]) || (fw_buf[9] != wdata[1]) || (fw_buf[8] != wdata[0])) {
		pr_info("[ERROR] USER F/W Donwlaod Fail !!! size_err\n\r");
	}

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FF8, wdata, 1); //read CHECKSUM
	pr_info("Flash : CHECKSUM : 0x%02X\n", wdata[0]);
	pr_info("BIN : CHECKSUM: 0x%02X\n", fw_buf[size - 0x10 + 0xC]);

	if (fw_buf[size - 0x10 + 0xC] != wdata[0])
	{
		pr_info("[ERROR] USER F/W Donwlaod Fail !!! checksum_err\n");
		ret = -1;
	}

	s2mm006_fw_read_ctrl_byte(i2c, ADDR_DMA_AREA2,	0x1FFC, wdata, 4); //read FINISH FLAG
	pr_info("Flash : FINISH : 0x%08X\n", *ldata);
	if (*ldata != 1) {
		pr_info("[ERROR] USER F/W Donwlaod Fail !!! finish_err\n");
		ret = -1;
	}

	usleep_range(100, 110);
	wdata[0] = 0;
	s2mm006_fw_write_ctrl_byte(i2c, ADDR_CTRL, 0xF004, wdata);

	usleep_range(100, 110);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_CTRL, 0xF000, wdata, 1);
	pr_info("Flash Current State = 0x%02X\n", wdata[0]);

	/* Reset Core */
	*ldata = 0;
	s2mm006_fw_write_ctrl_4bytes(i2c, ADDR_DEVICE, 0x2000, wdata);

END_OF_FLASH:
	s2mm006_fw_set_om(usbpd_data, OM_RESET);

	msleep(300);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_SRAM, 0x01, wdata, 1);
	pr_info("%s, FW Ver : %02X\n", __func__, wdata[0]);
	s2mm006_fw_read_ctrl_byte(i2c, ADDR_SRAM, 0x18, wdata, 1);
	pr_info("%s, Integrity Check : %s\n", __func__, ((wdata[0] & 0x10) == 0) ? "OK":"FAIL");

END_OF_FW_UPDATE:
	return ret;
}

int s2mm006_flash_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type)
{
	int ret = -1;
	int retry_cnt = 0;

	pr_info("%s type=%s\n", __func__, (type == FW_BUILT_IN) ?
			"built-in" : ((type == FW_ADB) ? "ADB" : "SPU or etc"));

	switch (type) {
	case FW_BUILT_IN:
		while (retry_cnt++ < 3) {
			s2mm006_fw_factory_flash_DMA(usbpd_data,
					(void *)BOOT_FLASH_FW, ARRAY_SIZE(BOOT_FLASH_FW));
			if (!s2mm006_fw_flash_DMA(usbpd_data,
					(void *)BOOT_FLASH_FW_MAIN, ARRAY_SIZE(BOOT_FLASH_FW_MAIN))) {
				ret = 0;
				break;
			}
			pr_err("FW_UPDATE fail, cnt : (%d)\n", retry_cnt);
		}
		break;
	case FW_ADB:
		if (size > BOOT_FLASH_FW_SIZE/2 && size < ((BOOT_FLASH_FW_SIZE*3)/2)) {
			s2mm006_fw_factory_flash_DMA(usbpd_data, buf, size);
		} else {
			while (retry_cnt++ < 3) {
				if (!s2mm006_fw_flash_DMA(usbpd_data, buf, size)) {
					ret = 0;
					s2mm006_init_register(usbpd_data);
					break;
				}
				pr_err("FW_UPDATE fail, cnt : (%d)\n", retry_cnt);
			}
		}
		break;
	case FW_SPU:
		break;
	default:
		break;
	}

	return ret;
}

int s2mm006_flash_main_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type)
{
	int ret = -1;
	int retry_cnt = 0;

	pr_info("%s type=%s\n", __func__, (type == FW_BUILT_IN) ?
			"built-in" : ((type == FW_ADB) ? "ADB" : "SPU or etc"));

	switch (type) {
	case FW_BUILT_IN:
		while (retry_cnt++ < 3) {
			if (!s2mm006_fw_flash_DMA(usbpd_data,
					(void *)BOOT_FLASH_FW_MAIN, ARRAY_SIZE(BOOT_FLASH_FW_MAIN))) {
				ret = 0;
				break;
			}
			pr_err("FW_UPDATE fail, cnt : (%d)\n", retry_cnt);
		}
		break;
	case FW_ADB:
		if (size > BOOT_FLASH_FW_SIZE/2 && size < ((BOOT_FLASH_FW_SIZE*3)/2)) {
			s2mm006_fw_factory_flash_DMA(usbpd_data, buf, size);
		} else {
			while (retry_cnt++ < 3) {
				if (!s2mm006_fw_flash_DMA(usbpd_data, buf, size)) {
					ret = 0;
					s2mm006_init_register(usbpd_data);
					break;
				}
				pr_err("FW_UPDATE fail, cnt : (%d)\n", retry_cnt);
			}
		}
		break;
	case FW_SPU:
		break;
	default:
		break;
	}

	return ret;
}

int s2mm006_flash_factory_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type)
{
	int ret = -1;
	int retry_cnt = 0;

	pr_info("%s type=%s\n", __func__, (type == FW_BUILT_IN) ?
			"built-in" : ((type == FW_ADB) ? "ADB" : "SPU or etc"));

	switch (type) {
	case FW_BUILT_IN:
		while (retry_cnt++ < 3) {
			if (!s2mm006_fw_factory_flash_DMA(usbpd_data,
					(void *)BOOT_FLASH_FW, ARRAY_SIZE(BOOT_FLASH_FW))) {
				ret = 0;
				break;
			}
			pr_err("FW_UPDATE fail, cnt : (%d)\n", retry_cnt);
		}
		break;
	case FW_ADB:
		if (size > BOOT_FLASH_FW_SIZE/2 && size < ((BOOT_FLASH_FW_SIZE*3)/2)) {
			s2mm006_fw_factory_flash_DMA(usbpd_data, buf, size);
		} else {
			while (retry_cnt++ < 3) {
				if (!s2mm006_fw_flash_DMA(usbpd_data, buf, size)) {
					ret = 0;
					s2mm006_init_register(usbpd_data);
					break;
				}
				pr_err("FW_UPDATE fail, cnt : (%d)\n", retry_cnt);
			}
		}
		break;
	case FW_SPU:
		break;
	default:
		break;
	}

	return ret;
}

int s2mm006_firmware_update_callback(void *data,
		void *fw_data, size_t fw_size)
{
	struct s2mm006_data *usbpd_data = data;
	int ret = 0;

	ret = s2mm006_flash_fw(usbpd_data, fw_data, fw_size, FW_ADB);

	return ret;
}

int s2mm006_fw_sysfs_update(void *data, int mode) {
	struct s2mm006_data *usbpd_data = data;
	int ret = 0;
	void *buf;
	size_t size;
	const struct firmware *fw;
	u8 fw_ver = 0, fw_test_ver = 0, boot_ver = 0;

	switch (mode) {
	case 0:
		pr_info("%s, Update Factory FW\n", __func__);
		ret = request_firmware(&fw, "s2mm006_factory_fw.bin", usbpd_data->dev);
		break;
	case 1:
		pr_info("%s, Update Main FW\n", __func__);
		ret = request_firmware(&fw, usbpd_data->main_fw_name, usbpd_data->dev);
		break;
	default:
		pr_info("%s, mode(%d) is invlid!!\n", __func__, mode);
		return -EINVAL;
	}

	if (ret) {	
		pr_info("%s: firmware is not available %d\n", __func__, ret);
		return -EINVAL;
	}

	size = fw->size;
	buf = (void *)fw->data;

	switch (mode) {
	case 0:
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_DEVICE_BOOT_ID, &boot_ver);
		if (s2mm006_get_bin_fw_ver(S2MM006_FACTORY_BOOT, buf) == boot_ver) {
			pr_info("%s, fw_boot_ver(%02X) same, skip facotry FW update\n",
					__func__, boot_ver);
			goto exit;
		}
		break;
	case 1:
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_DEVICE_FW_ID, &fw_ver);
		s2mm006_read_byte(usbpd_data->i2c, S2MM006_REG_DEVICE_FW_TEST_ID, &fw_test_ver);
		if (s2mm006_get_bin_fw_ver(S2MM006_MAIN, buf) == fw_ver
				&& s2mm006_get_bin_fw_ver(S2MM006_MAIN_TEST, buf) == fw_test_ver) {
			pr_info("%s, fw_ver(%02X, %02X) same, skip main FW update\n",
					__func__, fw_ver, fw_test_ver);
			goto exit;
		}
		break;
	}

	ret = s2mm006_flash_fw(usbpd_data, buf, size, FW_ADB);
	if (ret) {
		pr_info("%s, Fail to Update FW through Sysfs\n", __func__);
		release_firmware(fw);
		return -EINVAL;
	}

exit:
	release_firmware(fw);

	return 0;
}

unsigned long s2mm006_get_firmware_size(void *data)
{
	unsigned long ret = 0;

	if (ARRAY_SIZE(BOOT_FLASH_FW) > ARRAY_SIZE(BOOT_FLASH_FW_MAIN))
		ret = ARRAY_SIZE(BOOT_FLASH_FW);
	else
		ret = ARRAY_SIZE(BOOT_FLASH_FW_MAIN);

	return ret;
}

