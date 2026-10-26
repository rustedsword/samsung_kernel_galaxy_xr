/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 */

#include <linux/init.h>
#include <linux/module.h>
#include "adsp.h"
#define VENDOR "AKM"
#define CHIP_ID "AK09973"
#define FAIL (-1)
#define FAIL_SPECOUT (-2)
#define FAIL_STUCK (-3)
#define ST_SPEC_MASK 0xE000
#define DIGITAL_HALL_RAW_DATA_CNT		3

#define D_HALL_RAW_DATA_ERR_VALUE		65536

static ssize_t sub2_digital_hall_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t sub2_digital_hall_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t sub2_digital_hall_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;

	pr_info("[FACTORY] %s - start", __func__);

	mutex_lock(&data->digital_hall_mutex);
	adsp_unicast(NULL, 0, MSG_DIGITAL_HALL_SUB2, 0, MSG_TYPE_ST_SHOW_DATA);

	while (!(data->ready_flag[MSG_TYPE_ST_SHOW_DATA] & (1 << MSG_DIGITAL_HALL_SUB2)) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->ready_flag[MSG_TYPE_ST_SHOW_DATA] &= ~(1 << MSG_DIGITAL_HALL_SUB2);
	mutex_unlock(&data->digital_hall_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
	}
/*
 * Meaning of data->msg_buf[MSG_DIGITAL_HALL_SUB2][14] value. Refer sns_ak0997x_hal.h
 *
 * TLIMIT_NO_INVALID_ID                        (1 << 1)
 * TLIMIT_NO_RESET                             (1 << 2)
 * TLIMIT_NO_SET_SELFTEST                      (1 << 3)
 * TLIMIT_NO_CNT_CNTL2                         (1 << 4)
 * TLIMIT_NO_CNT_WAIT                          (1 << 5)
 * TLIMIT_NO_CNT_READ                          (1 << 6)
 * TLIMIT_NO_DATA_CHANGED                      (1 << 7)
 * TLIMIT_NO_SLF_RVHX                          (1 << 13)
 * TLIMIT_NO_SLF_RVHY                          (1 << 14)
 * TLIMIT_NO_SLF_RVHZ                          (1 << 15)
 * TLIMIT_NO_SLF_ST2                           (1 << 16)
 */
	pr_info("[FACTORY] I2C_ERROR: %d, ST_RES: %d, min: %d/%d/%d, max: %d/%d/%d, avg: %d/%d/%d, st: %d/%d/%d, err: %d\n",
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][0], data->msg_buf[MSG_DIGITAL_HALL_SUB2][1],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][2], data->msg_buf[MSG_DIGITAL_HALL_SUB2][3],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][4], data->msg_buf[MSG_DIGITAL_HALL_SUB2][5],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][6], data->msg_buf[MSG_DIGITAL_HALL_SUB2][7],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][8], data->msg_buf[MSG_DIGITAL_HALL_SUB2][9],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][10], data->msg_buf[MSG_DIGITAL_HALL_SUB2][11],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][12], data->msg_buf[MSG_DIGITAL_HALL_SUB2][13],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][14]);

	if (data->msg_buf[MSG_DIGITAL_HALL_SUB2][1] == FAIL_STUCK)
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][1] = FAIL;

	if ((data->msg_buf[MSG_DIGITAL_HALL_SUB2][14] & ST_SPEC_MASK) != 0)
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][1] = FAIL_SPECOUT;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][0], data->msg_buf[MSG_DIGITAL_HALL_SUB2][1],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][2], data->msg_buf[MSG_DIGITAL_HALL_SUB2][3],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][4], data->msg_buf[MSG_DIGITAL_HALL_SUB2][5],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][6], data->msg_buf[MSG_DIGITAL_HALL_SUB2][7],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][11], data->msg_buf[MSG_DIGITAL_HALL_SUB2][12],
		data->msg_buf[MSG_DIGITAL_HALL_SUB2][13]);
}

static ssize_t sub2_digital_hall_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t raw_data[DIGITAL_HALL_RAW_DATA_CNT] = {D_HALL_RAW_DATA_ERR_VALUE, D_HALL_RAW_DATA_ERR_VALUE, D_HALL_RAW_DATA_ERR_VALUE};
	uint8_t cnt = 0;

	pr_info("[FACTORY] %s - start", __func__);

	mutex_lock(&data->digital_hall_mutex);
	adsp_unicast(NULL, 0, MSG_DIGITAL_HALL_SUB2, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & (1 << MSG_DIGITAL_HALL_SUB2)) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_DIGITAL_HALL_SUB2);
	mutex_unlock(&data->digital_hall_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", raw_data[0], raw_data[1], raw_data[2]);
	}

	memcpy(raw_data, &data->msg_buf[MSG_DIGITAL_HALL_SUB2][0], sizeof(int32_t) * 3);

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d, %d, %d\n", __func__,
		raw_data[0], raw_data[1], raw_data[2]);
#endif

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", raw_data[0], raw_data[1], raw_data[2]);
}

static DEVICE_ATTR(name, 0444, sub2_digital_hall_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, sub2_digital_hall_vendor_show, NULL);
static DEVICE_ATTR(selftest, 0440, sub2_digital_hall_selftest_show, NULL);
static DEVICE_ATTR(raw_data, 0444, sub2_digital_hall_raw_data_show, NULL);

static struct device_attribute *digital_hall_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_raw_data,
	NULL,
};

static int __init ak09970_sub2_factory_init(void)
{
	adsp_factory_register(MSG_DIGITAL_HALL_SUB2, digital_hall_attrs);
	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit ak09970_sub2_factory_exit(void)
{
	adsp_factory_unregister(MSG_DIGITAL_HALL_SUB2);
	pr_info("[FACTORY] %s\n", __func__);
}

module_init(ak09970_sub2_factory_init);
module_exit(ak09970_sub2_factory_exit);
