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
#define VENDOR_NAME	"ABOV"
#define CHIP_ID "A96T3XX_SUB"

static ssize_t sub_grip_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sub_grip_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t sensor_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "ADSP");
}

/*static ssize_t sub_grip_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "Self test is not implemented yet");
}*/

static ssize_t sub_grip_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t grip_thd[2] = {0, };

	uint8_t cnt = 0;

	adsp_unicast(NULL, 0, MSG_GRIP_SUB, 0, MSG_TYPE_GET_THRESHOLD);

	while (!(data->ready_flag[MSG_TYPE_GET_THRESHOLD] &
		1 << MSG_GRIP_SUB) && cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_THRESHOLD] &= ~(1 << MSG_GRIP_SUB);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
	} else {
		memcpy(grip_thd, &data->msg_buf[MSG_GRIP_SUB][0], sizeof(int32_t)*2);
	}
#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: Press:ch2 %d, Release:ch2 %d\n", __func__, grip_thd[0], grip_thd[1]);
#endif
	return snprintf(buf, PAGE_SIZE, "%d,%d\n", grip_thd[0], grip_thd[1]);
}

static ssize_t sub_grip_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;
	int32_t msg_buf[2];
	int ret;

	pr_info("%s, %s", __func__, buf);

	ret = sscanf(buf, "%d,%d\n", &msg_buf[0], &msg_buf[1]);
	if (ret != 2) {
		pr_info("fail to read buf is %s\n", buf);
		return size;
	}

	adsp_unicast(&msg_buf, sizeof(msg_buf), MSG_GRIP_SUB, 0, MSG_TYPE_SET_THRESHOLD);

	while (!(data->ready_flag[MSG_TYPE_SET_THRESHOLD] & 1 << MSG_GRIP_SUB) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_THRESHOLD] &= ~(1 << MSG_GRIP_SUB);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return size;
	}

	pr_info("[FACTORY] %s: Set threshold done [%d,%d]\n", __func__,	data->msg_buf[MSG_GRIP_SUB][0], data->msg_buf[MSG_GRIP_SUB][1]);

	return size;
}

static ssize_t sub_grip_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t  grip_check[1] = {0};

	uint8_t cnt = 0;

	adsp_unicast(NULL, 0, MSG_GRIP_SUB, 0, MSG_TYPE_OPTION_DEFINE);

	while (!(data->ready_flag[MSG_TYPE_OPTION_DEFINE] &
		1 << MSG_GRIP_SUB) && cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_OPTION_DEFINE] &= ~(1 << MSG_GRIP_SUB);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
	} else {
		grip_check[0] = data->msg_buf[MSG_GRIP_SUB][0];
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d\n", __func__, grip_check[0]);
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", grip_check[0]);
}

static ssize_t sub_grip_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t raw_data[1] = {0};

	uint8_t cnt = 0;

	adsp_unicast(NULL, 0, MSG_GRIP_SUB, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] &
		1 << MSG_GRIP_SUB) && cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_GRIP_SUB);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
	} else {
		raw_data[0] = data->msg_buf[MSG_GRIP_SUB][0];
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d\n", __func__, raw_data[0]);
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", raw_data[0]);
}

static ssize_t sub_grip_sw_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;
	int32_t msg_buf[1];
	int ret;

	pr_info("%s, %s", __func__, buf);

	ret = sscanf(buf, "%d\n", &msg_buf[0]);
	if (ret != 1) {
		pr_info("fail to read buf is %s\n", buf);
		return size;
	}

	if (!(msg_buf[0] == 1)) {
		pr_err("wrong cmd %d\n", msg_buf[0]);
		return size;
	}

	adsp_unicast(&msg_buf, sizeof(msg_buf), MSG_GRIP_SUB, 0, MSG_TYPE_SET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_SET_CAL_DATA] & 1 << MSG_GRIP_SUB) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_CAL_DATA] &= ~(1 << MSG_GRIP_SUB);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return size;
	}
	if(data->msg_buf[MSG_GRIP_SUB][0] == 1)
		pr_info("[FACTORY] %s: Sub Grip SW Reset done successfully [%d]\n", __func__, data->msg_buf[MSG_GRIP_SUB][0]);
	else 
		pr_info("[FACTORY] %s: Sub Grip SW Reset Failed [%d]\n", __func__, data->msg_buf[MSG_GRIP_SUB][0]);

	return size;
}

static ssize_t sub_grip_sw_reset_ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t grip_reset = 0;

	uint8_t cnt = 0;

	adsp_unicast(NULL, 0, MSG_GRIP_SUB, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] &
		1 << MSG_GRIP_SUB) && cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << MSG_GRIP_SUB);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
	} else {
		if(data->msg_buf[MSG_GRIP_SUB][0] == 0)
			grip_reset = 1;
#ifdef CONFIG_SEC_FACTORY
		pr_info("[FACTORY] %s: %d, %d\n", __func__, grip_reset, data->msg_buf[MSG_GRIP_SUB][0]);
#endif
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", grip_reset);
}


static DEVICE_ATTR(name, 0444, sub_grip_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, sub_grip_vendor_show, NULL);
static DEVICE_ATTR(type, 0444, sensor_type_show, NULL);
//static DEVICE_ATTR(selftest, 0440, sub_grip_selftest_show, NULL);
static DEVICE_ATTR(grip_threshold, 0664, sub_grip_threshold_show, sub_grip_threshold_store);
static DEVICE_ATTR(grip_raw, 0444, sub_grip_raw_show, NULL);
static DEVICE_ATTR(grip_check, 0444, sub_grip_check_show, NULL);
static DEVICE_ATTR(grip_sw_reset, 0220, NULL, sub_grip_sw_reset_store);
static DEVICE_ATTR(grip_sw_reset_ready, 0444, sub_grip_sw_reset_ready_show, NULL);

static struct device_attribute *grip_sensor_attributes[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_type,
//	&dev_attr_selftest,
	&dev_attr_grip_threshold,
	&dev_attr_grip_raw,
	&dev_attr_grip_check,
	&dev_attr_grip_sw_reset,
	&dev_attr_grip_sw_reset_ready,
	NULL,
};

static int __init a96t3xx_sub_grip_factory_init(void)
{
	adsp_factory_register(MSG_GRIP_SUB, grip_sensor_attributes);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit a96t3xx_sub_grip_factory_exit(void)
{
	adsp_factory_unregister(MSG_GRIP_SUB);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(a96t3xx_sub_grip_factory_init);
module_exit(a96t3xx_sub_grip_factory_exit);
