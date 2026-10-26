/*
 *  Copyright (C) 2023, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#define IPD_RAW_DATA_CNT	14
#define IPD_CAL_TABLE_CNT	37 // 36 values + 1 flag
#define IPD_SAVE_CAL_DATA_CNT	13

#define IPD_RAW_DATA_ERR_VALUE		65536

#define IPD_CAL_DATA_FILE_PATH   "/efs/FactoryApp/ipd_cal"

static int ipd_read_cal_data(int *ipd_cal)
{
	struct file *cal_data_filp = NULL;
	int ret = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_data_filp = filp_open(IPD_CAL_DATA_FILE_PATH, O_RDONLY, 0440);
	if (IS_ERR(cal_data_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_data_filp);
		pr_err("[FACTORY] %s: open fail ipd cal:%d\n", __func__, ret);
		return ret;
	}

	ret = vfs_read(cal_data_filp, (char *)ipd_cal, sizeof(int) * IPD_CAL_TABLE_CNT, &cal_data_filp->f_pos);
	if (ret < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, ret);
		filp_close(cal_data_filp, current->files);
		set_fs(old_fs);
		return ret;
	}

	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			ipd_cal[0], ipd_cal[1], ipd_cal[2], ipd_cal[3], ipd_cal[4], ipd_cal[5],
			ipd_cal[6], ipd_cal[7], ipd_cal[8], ipd_cal[9], ipd_cal[10], ipd_cal[11],
			ipd_cal[12], ipd_cal[13], ipd_cal[14], ipd_cal[15], ipd_cal[16], ipd_cal[17],
			ipd_cal[18], ipd_cal[19], ipd_cal[20], ipd_cal[21], ipd_cal[22], ipd_cal[23],
			ipd_cal[24], ipd_cal[25], ipd_cal[26], ipd_cal[27], ipd_cal[28], ipd_cal[29],
			ipd_cal[30], ipd_cal[31], ipd_cal[32], ipd_cal[33], ipd_cal[34], ipd_cal[35], ipd_cal[36]);

	filp_close(cal_data_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static int ipd_write_cal_data(int *cal_data)
{
	struct file *cal_data_filp = NULL;
	int ret = 0;
	int flag = 0;
	umode_t mode = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	flag = O_TRUNC | O_RDWR | O_CREAT;
	mode = 0660;

	cal_data_filp = filp_open(IPD_CAL_DATA_FILE_PATH, flag, mode);
	if (IS_ERR(cal_data_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_data_filp);
		pr_err("[FACTORY] %s: Can't open cal data file(%d)\n", __func__, ret);
		return ret;
	}

	ret = vfs_write(cal_data_filp, (char *)cal_data, sizeof(int) * IPD_CAL_TABLE_CNT, &cal_data_filp->f_pos);
	if (ret < 0)
		pr_err("[FACTORY] %s: Can't write cal data to file(%d)\n", __func__, ret);

	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
		cal_data[0], cal_data[1], cal_data[2], cal_data[3], cal_data[4], cal_data[5],
		cal_data[6], cal_data[7], cal_data[8], cal_data[9], cal_data[10], cal_data[11],
		cal_data[12], cal_data[13], cal_data[14], cal_data[15], cal_data[16], cal_data[17],
		cal_data[18], cal_data[19], cal_data[20], cal_data[21], cal_data[22], cal_data[23],
		cal_data[24], cal_data[25], cal_data[26], cal_data[27], cal_data[28], cal_data[29],
		cal_data[30], cal_data[31], cal_data[32], cal_data[33], cal_data[34], cal_data[35], cal_data[36]);

	filp_close(cal_data_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static void ipd_send_cal_data(struct adsp_data *data)
{
	int ret = 0;
	int32_t cnt = 0;
	int cal_table[IPD_CAL_TABLE_CNT] = {0,};

	ret = ipd_read_cal_data(cal_table);
	if (ret == -ENOENT || ret == -ENXIO) {
		pr_info("[FACTORY] %s : no ipd cal file\n", __func__);
		return;
	} else if (ret < 0) {
		pr_err("[FACTORY]: %s - filp_open error\n", __func__);
		return;
	}

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(&cal_table[1], sizeof(int) * (IPD_CAL_TABLE_CNT - 1), MSG_IPD, 0, MSG_TYPE_SET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_SET_CAL_DATA] &
		1 << MSG_IPD) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1050);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	data->ready_flag[MSG_TYPE_SET_CAL_DATA] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	pr_info("[FACTORY] %s: Done.\n", __func__);
}

static bool save_cal_data(struct adsp_data *data, int step, int *cal_table) {
	int cnt = 0;

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(&step, sizeof(int), MSG_IPD, 0, MSG_TYPE_SAVE_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_SAVE_CAL_DATA] & 1 << MSG_IPD)
			&& cnt++ < TIMEOUT_CNT) {
		usleep_range(1000, 1050);
	}

	data->ready_flag[MSG_TYPE_SAVE_CAL_DATA] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s(%d): Timeout!!!\n", __func__, step);
		return false;
	}

	memcpy(cal_table, &data->msg_buf[MSG_IPD][0], sizeof(int32_t) * IPD_SAVE_CAL_DATA_CNT);

	return true;
}

static ssize_t ipd_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t ipd_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t ipd_location_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t raw_data[IPD_RAW_DATA_CNT] = {IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE,
											IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE,
											IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE,
											IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE,
											IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE, IPD_RAW_DATA_ERR_VALUE};

	int32_t left_distance = 0;
	int32_t right_distance = 0;
	int32_t left_float_position = 0;
	int32_t right_float_postion = 0;
	int cnt = 0;

	bool left_positive = true;
	bool right_positive = true;

	pr_info("[FACTORY] %s - start", __func__);

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(NULL, 0, MSG_IPD, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << MSG_IPD)
			&& cnt++ < TIMEOUT_CNT) {
		usleep_range(1000, 1050);
	}

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "L:-100,R:-100\n");
	}

	memcpy(raw_data, &data->msg_buf[MSG_IPD][0], sizeof(int32_t) * IPD_RAW_DATA_CNT);

	if (raw_data[0] < 0) {
		left_positive = false;
		left_distance = (raw_data[0] / 1000) * -1;
		left_float_position = (raw_data[0] % 1000) * -1;
	} else {
		left_distance = (raw_data[0] / 1000);
		left_float_position = (raw_data[0] % 1000);
	}

	if (raw_data[1] < 0) {
		right_positive = false;
		right_distance = (raw_data[1] / 1000) * -1;
		right_float_postion = (raw_data[1] % 1000) * -1;
	} else {
		right_distance = (raw_data[1] / 1000);
		right_float_postion = (raw_data[1] % 1000);
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", __func__,
			raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6],
			raw_data[7], raw_data[8], raw_data[9], raw_data[10], raw_data[11], raw_data[12], raw_data[13]);
#endif

	if (left_positive && right_positive) {
		return snprintf(buf, PAGE_SIZE, "L:%d.%d,R:%d.%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			left_distance, left_float_position, right_distance, right_float_postion,
			raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7],
			raw_data[8], raw_data[9], raw_data[10], raw_data[11], raw_data[12], raw_data[13]);
	} else if (left_positive && !right_positive) {
		return snprintf(buf, PAGE_SIZE, "L:%d.%d,R:-%d.%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			left_distance, left_float_position, right_distance, right_float_postion,
			raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7],
			raw_data[8], raw_data[9], raw_data[10], raw_data[11], raw_data[12], raw_data[13]);
	} else if (!left_positive && right_positive) {
		return snprintf(buf, PAGE_SIZE, "L:-%d.%d,R:%d.%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			left_distance, left_float_position, right_distance, right_float_postion,
			raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7],
			raw_data[8], raw_data[9], raw_data[10], raw_data[11], raw_data[12], raw_data[13]);
	} else {
		return snprintf(buf, PAGE_SIZE, "L:-%d.%d,R:-%d.%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			left_distance, left_float_position, right_distance, right_float_postion,
			raw_data[2], raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7],
			raw_data[8], raw_data[9], raw_data[10], raw_data[11], raw_data[12], raw_data[13]);
	}
}

static ssize_t ipd_cal_save_0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int step = 0;
	int cal_table[IPD_SAVE_CAL_DATA_CNT] = {0,};
	bool ret = false;
	pr_info("[FACTORY] %s - start", __func__);

	ret = save_cal_data(data, step, cal_table);

	if (!ret) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11], cal_table[12]);
#endif

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
		cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11], cal_table[12]);
}

static ssize_t ipd_cal_save_1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int step = 1;
	int cal_table[IPD_SAVE_CAL_DATA_CNT] = {0,};
	bool ret = false;
	pr_info("[FACTORY] %s - start", __func__);

	ret = save_cal_data(data, step, cal_table);

	if (!ret) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11], cal_table[12]);
#endif

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
		cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11], cal_table[12]);
}

static ssize_t ipd_cal_save_2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int step = 2;
	int cal_table[IPD_SAVE_CAL_DATA_CNT] = {0,};
	bool ret = false;
	pr_info("[FACTORY] %s - start", __func__);

	ret = save_cal_data(data, step, cal_table);

	if (!ret) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11], cal_table[12]);
#endif

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
		cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11], cal_table[12]);
}

static ssize_t ipd_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cal_table[IPD_CAL_TABLE_CNT] = {0,};
	int cnt = 0;
	int ret = 1;

	pr_info("[FACTORY] %s - start", __func__);

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(NULL, 0, MSG_IPD, 0, MSG_TYPE_START_CAL);

	while (!(data->ready_flag[MSG_TYPE_START_CAL] & 1 << MSG_IPD)
			&& cnt++ < TIMEOUT_CNT) {
		usleep_range(1000, 1050);
	}

	data->ready_flag[MSG_TYPE_START_CAL] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout in start cal!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	memcpy(&ret, &data->msg_buf[MSG_IPD][0], sizeof(int32_t));

	if (ret != 0) {// driver calibration result 0 - Success, 1 - Fail
		pr_err("[FACTORY] %s: driver calibration failed\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	cnt = 0;

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(NULL, 0, MSG_IPD, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << MSG_IPD)
			&& cnt++ < TIMEOUT_CNT) {
		usleep_range(1000, 1050);
	}

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout in get cal data!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	memcpy(cal_table, &data->msg_buf[MSG_IPD][0], sizeof(int32_t) * IPD_CAL_TABLE_CNT);

	ret = ipd_write_cal_data(cal_table);
	if (ret < 0) {
		pr_err("[FACTORY] %s: failed\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	return snprintf(buf, PAGE_SIZE, "0\n",	ret);
}

static ssize_t ipd_cal_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int cal_table[IPD_CAL_TABLE_CNT] = {0,};

	ret = ipd_read_cal_data(cal_table);
	if (ret == -ENOENT || ret == -ENXIO) {
		pr_info("[FACTORY] %s : no ipd cal file\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	} else if (ret < 0) {
		pr_err("[FACTORY]: %s - filp_open error\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	}

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11],
			cal_table[12], cal_table[13], cal_table[14], cal_table[15], cal_table[16], cal_table[17],
			cal_table[18], cal_table[19], cal_table[20], cal_table[21], cal_table[22], cal_table[23],
			cal_table[24], cal_table[25], cal_table[26], cal_table[27], cal_table[28], cal_table[29],
			cal_table[30], cal_table[31], cal_table[32], cal_table[33], cal_table[34], cal_table[35], cal_table[36]);
#endif

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11],
			cal_table[12], cal_table[13], cal_table[14], cal_table[15], cal_table[16], cal_table[17],
			cal_table[18], cal_table[19], cal_table[20], cal_table[21], cal_table[22], cal_table[23],
			cal_table[24], cal_table[25], cal_table[26], cal_table[27], cal_table[28], cal_table[29],
			cal_table[30], cal_table[31], cal_table[32], cal_table[33], cal_table[34], cal_table[35], cal_table[36]);
}

static ssize_t ipd_cal_reset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int cnt = 0;
	int ret = 1;

	pr_info("[FACTORY] %s - start", __func__);

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(NULL, 0, MSG_IPD, 0, MSG_TYPE_RESET_CAL);

	while (!(data->ready_flag[MSG_TYPE_RESET_CAL] & 1 << MSG_IPD)
			&& cnt++ < TIMEOUT_CNT) {
		usleep_range(1000, 1050);
	}

	data->ready_flag[MSG_TYPE_RESET_CAL] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	memcpy(&ret, &data->msg_buf[MSG_IPD][0], sizeof(int32_t));

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d\n", __func__, ret);
#endif

	return snprintf(buf, PAGE_SIZE, "%d\n",	ret);
}

static ssize_t in_use_cal_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cal_table[IPD_CAL_TABLE_CNT] = {0,};
	int cnt = 0;

	pr_info("[FACTORY] %s - start", __func__);

	mutex_lock(&data->ipd_mutex);
	adsp_unicast(NULL, 0, MSG_IPD, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << MSG_IPD)
			&& cnt++ < TIMEOUT_CNT) {
		usleep_range(1000, 1050);
	}

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << MSG_IPD);
	mutex_unlock(&data->ipd_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	}

	memcpy(cal_table, &data->msg_buf[MSG_IPD][0], sizeof(int32_t) * IPD_CAL_TABLE_CNT);

#ifdef CONFIG_SEC_FACTORY
	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11],
			cal_table[12], cal_table[13], cal_table[14], cal_table[15], cal_table[16], cal_table[17],
			cal_table[18], cal_table[19], cal_table[20], cal_table[21], cal_table[22], cal_table[23],
			cal_table[24], cal_table[25], cal_table[26], cal_table[27], cal_table[28], cal_table[29],
			cal_table[30], cal_table[31], cal_table[32], cal_table[33], cal_table[34], cal_table[35], cal_table[36]);
#endif

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			cal_table[0], cal_table[1], cal_table[2], cal_table[3], cal_table[4], cal_table[5],
			cal_table[6], cal_table[7], cal_table[8], cal_table[9], cal_table[10], cal_table[11],
			cal_table[12], cal_table[13], cal_table[14], cal_table[15], cal_table[16], cal_table[17],
			cal_table[18], cal_table[19], cal_table[20], cal_table[21], cal_table[22], cal_table[23],
			cal_table[24], cal_table[25], cal_table[26], cal_table[27], cal_table[28], cal_table[29],
			cal_table[30], cal_table[31], cal_table[32], cal_table[33], cal_table[34], cal_table[35], cal_table[36]);
}

void ipd_cal_read_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, ipd_cal_work);

	ipd_send_cal_data(data);
}

void ipd_cal_init_work(struct adsp_data *data)
{
	schedule_delayed_work(&data->ipd_cal_work, msecs_to_jiffies(3000));
}

static DEVICE_ATTR(name, 0644, ipd_name_show, NULL);
static DEVICE_ATTR(vendor, 0644, ipd_vendor_show, NULL);
static DEVICE_ATTR(location, 0644, ipd_location_show, NULL);
static DEVICE_ATTR(cal_save_0, 0644, ipd_cal_save_0_show, NULL);
static DEVICE_ATTR(cal_save_1, 0644, ipd_cal_save_1_show, NULL);
static DEVICE_ATTR(cal_save_2, 0644, ipd_cal_save_2_show, NULL);
static DEVICE_ATTR(calibration, 0644, ipd_calibration_show, NULL);
static DEVICE_ATTR(cal_table, 0644, ipd_cal_table_show, NULL);
static DEVICE_ATTR(cal_reset, 0644, ipd_cal_reset_show, NULL);
static DEVICE_ATTR(in_use_cal_table, 0644, in_use_cal_table_show, NULL);

static struct device_attribute *ipd_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_location,
	&dev_attr_cal_save_0,
	&dev_attr_cal_save_1,
	&dev_attr_cal_save_2,
	&dev_attr_calibration,
	&dev_attr_cal_table,
	&dev_attr_cal_reset,
	&dev_attr_in_use_cal_table,
	NULL,
};

static int __init ak09970_ipd_factory_init(void)
{
	adsp_factory_register(MSG_IPD, ipd_attrs);
	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit ak09970_ipd_factory_exit(void)
{
	adsp_factory_unregister(MSG_IPD);
	pr_info("[FACTORY] %s\n", __func__);
}

module_init(ak09970_ipd_factory_init);
module_exit(ak09970_ipd_factory_exit);
