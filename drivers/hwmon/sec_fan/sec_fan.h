/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Samsung Electronics
 */

#ifndef SEC_FAN_H
#define SEC_FAN_H

#include <linux/device.h>

#define SEC_FAN_STEP_DEFAULT	1
#define SEC_FAN_STEP_OFF		0

struct sec_fan_ops {
	//int (*enable)(struct device *dev, bool en);  //ENabled required
	int (*set_step)(struct device *dev, int step, int step_data);
	//int (*get_step)(struct device *dev);
	int (*get_curr_rpm)(struct device *dev);
	int (*get_curr_rpm2)(struct device *dev);
	int (*get_pwm)(struct device *dev);
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	void (*enable_irq)(struct device *dev);
	void (*disable_irq)(struct device *dev);
};

struct sec_fan_platform_data {
	bool probe_done;
	int steps_cnt;
	u32 *step_map;
	void *ddata;
};

struct sec_fan_drvdata {
	int new_step;
	struct mutex mutex;
	struct work_struct step_work;
	struct work_struct fan_resume_work;
	struct device *sec_fan_dev;
	const struct sec_fan_ops *ops;
	struct device *chip_dev;  // for chip data
	struct attribute_group **vendor_dev_attr_groups;
	bool is_registered;
	struct sec_fan_platform_data *pdata;  // To do : Put the pdata at right place //TSN
};

extern int sec_fan_register(struct sec_fan_drvdata *ddata);
extern void sec_fan_unregister(struct sec_fan_drvdata *ddata);
extern bool is_sec_fan_init_required(void);
#endif /* SEC_FAN_H */
