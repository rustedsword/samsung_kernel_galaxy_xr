// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "[sec_fan] " fmt

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include "sec_fan.h"

/*
 * sec_fan_pdata - holding sec_fan common platform data requires
 *		to communicate with user space.
 */
static struct sec_fan_platform_data sec_fan_pdata;

/*
 * sec_fan_class - used for creating sec_fan class
 */
static struct class *sec_fan_class;

/**
 * sec_fan_resume_work - work handler to resume.
 **/
static void sec_fan_resume_work(struct work_struct *work)
{
	struct sec_fan_drvdata *ddata =
		container_of(work, struct sec_fan_drvdata, fan_resume_work);

	int ret;

	mutex_lock(&ddata->mutex);
	pr_info("%s +++\n", __func__);

	ret = ddata->ops->resume(ddata->chip_dev);
	if(ret)
		pr_err("%s driver ic init failed\n", __func__);

	ddata->ops->enable_irq(ddata->chip_dev);
	mutex_unlock(&ddata->mutex);
}

/**
 * sec_fan_step_work - work handler to execute HW access for setting step
 *    Note: If the step change is done in sysfs, it might give lagging so better to do it in this work.
 **/
static void sec_fan_step_work(struct work_struct *work)
{
	struct sec_fan_drvdata *ddata =
		container_of(work, struct sec_fan_drvdata, step_work);

	int new_step, ret;

	mutex_lock(&ddata->mutex);
	new_step = ddata->new_step;
	pr_info("%s step %d\n", __func__, new_step);
	ret = ddata->ops->set_step(ddata->chip_dev, new_step, sec_fan_pdata.step_map[new_step]);
	if(ret)
		pr_err("%s step %d setting failed\n", __func__, new_step);
	else
		pr_info("%s step %d setting done\n", __func__, new_step);

	mutex_unlock(&ddata->mutex);
}


/**
 * step_show - sysfs read interface to show
 *      currently configured step value of FAN.
 **/

/**
 * step_store - sysfs write interface to update fan speed in steps
 *      Its a main interface for controlling based on thermistor values.
 **/
static ssize_t step_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct sec_fan_drvdata *ddata = dev_get_drvdata(dev);
	int step;

	if (kstrtoint(buf, 0, &step))
		return -EINVAL;

	pr_info("start! %s: Step=%d\n", __func__, step);

	if (step >= ddata->pdata->steps_cnt || step < 0)
		return -EINVAL;

	cancel_work_sync(&ddata->step_work);
	ddata->new_step = step;
	schedule_work(&ddata->step_work);

	return len;
}
static DEVICE_ATTR_WO(step);

/**
 * curr_rpm_show - sysfs read interface to show
 *      current fan speed in rpm.
 * This will be used in factory test.
 **/
static ssize_t curr_rpm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rpm = 0;
	struct sec_fan_drvdata *ddata = dev_get_drvdata(dev);

	rpm = ddata->ops->get_curr_rpm(ddata->chip_dev);

	pr_info("%s: rpm=%d\n", __func__, rpm);
	return snprintf(buf, PAGE_SIZE, "%d\n", rpm);
}
static DEVICE_ATTR_RO(curr_rpm);

static ssize_t curr_rpm2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rpm2 = 0;
	struct sec_fan_drvdata *ddata = dev_get_drvdata(dev);

	if(ddata->ops->get_curr_rpm2) {
		rpm2 = ddata->ops->get_curr_rpm2(ddata->chip_dev);

		pr_info("%s: rpm2=%d\n", __func__, rpm2);
		return snprintf(buf, PAGE_SIZE, "%d\n", rpm2);
	}

	pr_err("%s: fan2 does not exist\n", __func__);
	return snprintf(buf, PAGE_SIZE, "fan2 does not exist\n");
}
static DEVICE_ATTR_RO(curr_rpm2);

static ssize_t get_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rpm1 = 0;
	int rpm2 = 0;
	int pwm = 0;
	int ret;

	struct sec_fan_drvdata *ddata;

	if (!dev) {
		pr_err("%s: device is NULL\n", __func__);
		return 0;
	}

	ddata = dev_get_drvdata(dev);
	if (!ddata) {
		pr_err("%s: sec_fan_drvdata is NULL\n", __func__);
		return 0;
	}

	if(ddata->ops->get_curr_rpm) {
		ret = ddata->ops->get_curr_rpm(ddata->chip_dev);
		if (ret < 0) {
			pr_err("%s: failed to read curr_rpm1: %d\n", __func__, ret);
			rpm1 = 0;
		} 
		else
			rpm1 = ret;
	}

	if (ddata->ops->get_curr_rpm2) {
	    ret = ddata->ops->get_curr_rpm2(ddata->chip_dev);
	    if (ret < 0) {
	        pr_err("%s: failed to read curr_rpm2: %d\n", __func__, ret);
	        rpm2 = 0;
	    } 
		else
	        rpm2 = ret;
	}

	if(ddata->ops->get_pwm) {
		ret = ddata->ops->get_pwm(ddata->chip_dev);
		if (ret < 0) {
			pr_err("%s: failed to read get_pwm: %d\n", __func__, ret);
			pwm = 0;
		} 
		else
			pwm = ret;
	}

	pr_info("%s: rpm1=%d, rpm2=%d, pwm=%d \n", __func__, rpm1, rpm2, pwm);

	return snprintf(buf, PAGE_SIZE, "rpm1=%d, rpm2=%d, pwm=%d\n", rpm1, rpm2, pwm);
}
static DEVICE_ATTR_RO(get_status);

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
/**
 * tune_steps_map_show - sysfs read interface to show
 *      currently tune maps. Steps & PWM.
 *      Accessible only on non-ship binaries.
 **/
static ssize_t tune_steps_map_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt, len;

	pr_info("%s start!\n", __func__);

	len = sprintf(buf, "Steps mapping:\n");
	for (cnt = SEC_FAN_STEP_DEFAULT; cnt < sec_fan_pdata.steps_cnt; cnt++)
		len += sprintf(buf + len, " %d: %d\n", cnt, sec_fan_pdata.step_map[cnt]);

	return len;
}
/**
 * tune_steps_map_store - helping functtion provides sysfs write interface to
 *      update fan speed in steps map.
 *      Accessible only on non-ship binaries.
 **/
static ssize_t tune_steps_map_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret = 0;

	pr_info("%s start!\n", __func__);

	if (sec_fan_pdata.steps_cnt == 4) {
		ret = sscanf(buf, "%d %d %d\n", &sec_fan_pdata.step_map[1],
		&sec_fan_pdata.step_map[2], &sec_fan_pdata.step_map[3]);
	}
	else if (sec_fan_pdata.steps_cnt == 6) {
		ret = sscanf(buf, "%d %d %d %d %d\n", &sec_fan_pdata.step_map[1],
		&sec_fan_pdata.step_map[2], &sec_fan_pdata.step_map[3],
		&sec_fan_pdata.step_map[4], &sec_fan_pdata.step_map[5]);
	}
	else if (sec_fan_pdata.steps_cnt == 8) {
		ret = sscanf(buf, "%d %d %d %d %d %d %d\n", &sec_fan_pdata.step_map[1],
		&sec_fan_pdata.step_map[2], &sec_fan_pdata.step_map[3],
		&sec_fan_pdata.step_map[4], &sec_fan_pdata.step_map[5],
		&sec_fan_pdata.step_map[6], &sec_fan_pdata.step_map[7]);
	}
	if (ret < sec_fan_pdata.steps_cnt)
		pr_err("%s all steps are not updated\n", __func__);

	return len;
}
static DEVICE_ATTR_RW(tune_steps_map);
#endif //CONFIG_SAMSUNG_PRODUCT_SHIP

static struct attribute *sec_fan_attributes[] = {
	&dev_attr_step.attr,
	&dev_attr_curr_rpm.attr,
	&dev_attr_curr_rpm2.attr,
	&dev_attr_get_status.attr,
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	/* List of nodes, created for non ship binary */
	&dev_attr_tune_steps_map.attr,  // To Do: Add sysfs for steps tuning via adb
#endif //CONFIG_SAMSUNG_PRODUCT_SHIP
	NULL,
};

static struct attribute_group sec_fan_attr_group = {
	.attrs = sec_fan_attributes,
};

/**
 * sec_fan_sysfs_init - regsiters sec_fan sysfs and Chip driver's sysfs
 **/
int sec_fan_sysfs_init(struct sec_fan_drvdata *ddata)
{
	int ret = 0;
	int vendor_attr_grp_cnt = 0;

	pr_info("%s +++\n", __func__);

	/* To Do:
	 * If multiple IC support requires, this needs to be a separate function to provide
	 * other ICs also able to register under sec_fan class.
	 *    - Reference can be taken from sec_device_create.
	 *    - The device number should be update properly (not by MKDEV)
	 */
	ddata->sec_fan_dev = device_create(sec_fan_class, NULL, MKDEV(0, 0), ddata, "fan");
	if (IS_ERR(ddata->sec_fan_dev)) {
		ret = PTR_ERR(ddata->sec_fan_dev);
		goto err_device_create;
	}

	ret = sysfs_create_group(&ddata->sec_fan_dev->kobj, &sec_fan_attr_group);
	if (ret) {
		pr_err("%s Failed to create sec_fan_attr_group\n", __func__);
		goto err_sysfs_sec_fan_attr;
	}

	if (ddata->vendor_dev_attr_groups) {
		pr_debug("%s : vendor_dev_attr_groups\n", __func__);
		for (; ddata->vendor_dev_attr_groups[vendor_attr_grp_cnt]; vendor_attr_grp_cnt++) {
			ret = sysfs_create_group(&ddata->sec_fan_dev->kobj,
				ddata->vendor_dev_attr_groups[vendor_attr_grp_cnt]);
			if (ret) {
				pr_err("%s Failed to create sysfs groups[%d]\n", __func__, vendor_attr_grp_cnt);
				goto err_sysfs_vendor_attr;
			}
			pr_debug("%s: -> %d\n", __func__, vendor_attr_grp_cnt);
		}
	}

	pr_info("%s - done\n", __func__);
	return 0;

err_sysfs_vendor_attr:
	sysfs_remove_group(&ddata->sec_fan_dev->kobj, &sec_fan_attr_group);
	if (ddata->vendor_dev_attr_groups) {
		for (--vendor_attr_grp_cnt; vendor_attr_grp_cnt >= 0; vendor_attr_grp_cnt--)
			sysfs_remove_group(&ddata->sec_fan_dev->kobj,
				ddata->vendor_dev_attr_groups[vendor_attr_grp_cnt]);
	}
err_sysfs_sec_fan_attr:
	/* TO DO: If Multiple Device suppor is added MKDEV has to be update */
	device_destroy(sec_fan_class, MKDEV(0, 0));
err_device_create:
	pr_err("%s - Failed\n", __func__);
	return ret;
}

static void sec_fan_register_thermal_cooling_device(void);

/**
 * sec_fan_register - Exported API for sec_fan chip driver to register themselves to sec_fan sub system
 **/
int sec_fan_register(struct sec_fan_drvdata *ddata)
{
	int ret = 0;

	pr_info("%s +++\n", __func__);

	if (!ddata) {
		pr_err("%s no ddata\n", __func__);
		return -ENODEV;
	}

	mutex_init(&ddata->mutex);
	INIT_WORK(&ddata->step_work, sec_fan_step_work);
	INIT_WORK(&ddata->fan_resume_work, sec_fan_resume_work);

	ddata->pdata = &sec_fan_pdata;
	sec_fan_pdata.ddata = ddata;

	pr_debug("%s: pdata parsed\n", __func__);

	ret = sec_fan_sysfs_init(ddata);
	if (ret) {
		pr_err("%s Failed to create inputff sysfs group: %d\n", __func__);
		return -EIO;
	}
	pr_debug("%s: sysfs initialized\n", __func__);

	// Register this after sec_fan_pdata.data is initialized.
	sec_fan_register_thermal_cooling_device();

	pr_info("%s ---\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(sec_fan_register);

/**
 * sec_fan_unregister - Exported API for sec_fan chip driver to unregister themselves from sec_fan sub system
 **/
void sec_fan_unregister(struct sec_fan_drvdata *ddata)
{
	pr_info("%s +++\n", __func__);

	if (!ddata) {
		pr_err("%s no ddata\n", __func__);
		return;
	}

	if (sec_fan_pdata.ddata) {
		int vendor_attr_grp_cnt = 0;

		mutex_destroy(&ddata->mutex);

		sysfs_remove_group(&ddata->sec_fan_dev->kobj, &sec_fan_attr_group);
		if (ddata->vendor_dev_attr_groups) {
			for (; vendor_attr_grp_cnt >= 0; vendor_attr_grp_cnt--)
				sysfs_remove_group(&ddata->sec_fan_dev->kobj,
					ddata->vendor_dev_attr_groups[vendor_attr_grp_cnt]);
		}

		device_destroy(sec_fan_class, MKDEV(0, 0));
	}

	sec_fan_pdata.ddata = NULL;
}
EXPORT_SYMBOL_GPL(sec_fan_unregister);

/**
 * sec_fan_parse_dt - parses hw details from dts
 **/
static int sec_fan_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sec_fan_platform_data *pdata = &sec_fan_pdata;
	int ret = 0;
	int i;

	pr_info("%s +++\n", __func__);

	if (unlikely(!np)) {
		pr_err("%s err: could not parse dt node\n", __func__);
		return -ENODEV;
	}

	/* To Do: Parse number of steps & steps tuning data */
	// Get number of steps  //
	pdata->steps_cnt = of_property_count_elems_of_size(np,
				"step-mapping", sizeof(u32));
	if (unlikely(pdata->steps_cnt)) {
		pdata->step_map = kmalloc_array(pdata->steps_cnt, sizeof(u32), GFP_KERNEL);
		if (unlikely(!pdata->step_map))
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "step-mapping",
			 pdata->step_map, pdata->steps_cnt);
		if (ret) {
			pr_err("%s error in geting steps\n", __func__);
			goto err_parse_steps;
		}

		pr_info("%s Step --> Mapping\n", __func__);
		for (i = 0; i < pdata->steps_cnt; i++)
			pr_info("%4d --> %d\n", i, pdata->step_map[i]);
	}

	pr_info("%s : done! ---\n", __func__);
	return 0;

err_parse_steps:
	kfree(pdata->step_map);

	return ret;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
extern int is_lpcharge_pdic_param(void);
#endif
/**
 * is_sec_fan_init_required - checks, if the fan needs to be enumerated or not
 **/
bool is_sec_fan_init_required(void)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	int lpcharge_pdic = is_lpcharge_pdic_param();

	pr_info("%s PDIC lpm function returned value = %d..\n", __func__, lpcharge_pdic);
	if (lpcharge_pdic) {
		pr_info("%s Skip loading - LPM Charging mode!!..\n", __func__);
		return 0;
	}
#endif
	return 1;
}
EXPORT_SYMBOL_GPL(is_sec_fan_init_required);

static int sec_fan_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = sec_fan_pdata.step_map[sec_fan_pdata.steps_cnt - 1];
	return 0;
}

static int sec_fan_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct sec_fan_drvdata *ddata = (struct sec_fan_drvdata *)sec_fan_pdata.ddata;
	*state = ddata->ops->get_curr_rpm(ddata->chip_dev);
	return 0;
}

static int sec_fan_get_cur_state1(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct sec_fan_drvdata *ddata = (struct sec_fan_drvdata *)sec_fan_pdata.ddata;
	*state = ddata->ops->get_curr_rpm2(ddata->chip_dev);
	return 0;
}

static int sec_fan_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	pr_err("Do not support set_cur_state.\n");
	return -EINVAL;
}

/* bind to generic thermal layer as cooling device*/
static struct thermal_cooling_device_ops sec_fan0_cooling_ops = {
	.get_max_state = sec_fan_get_max_state,
	.get_cur_state = sec_fan_get_cur_state,
	.set_cur_state = sec_fan_set_cur_state,
};

static struct thermal_cooling_device_ops sec_fan1_cooling_ops = {
	.get_max_state = sec_fan_get_max_state,
	.get_cur_state = sec_fan_get_cur_state1,
	.set_cur_state = sec_fan_set_cur_state,
};

static void sec_fan_register_thermal_cooling_device(void) {
	struct thermal_cooling_device *cdev;
        // Cooling device name should be matched with cdev_map in thermalCommon.cpp.
	cdev = thermal_cooling_device_register(
			"fan-0", &sec_fan_pdata, &sec_fan0_cooling_ops);
	if (IS_ERR(cdev)) {
		pr_err("Cdev register failed for FAN0, ret:%d\n", PTR_ERR(cdev));
	}
	cdev = thermal_cooling_device_register(
			"fan-1", &sec_fan_pdata, &sec_fan1_cooling_ops);
	if (IS_ERR(cdev)) {
		pr_err("Cdev register failed for FAN1, ret:%d\n", PTR_ERR(cdev));
	}
}

/**
 * sec_fan_probe - Enumerates service FAN resources.
 **/
static int sec_fan_probe(struct platform_device *pdev)
{
	pr_info("%s +++\n", __func__);

	//if (!is_sec_fan_init_required())  // To DO: If at init cant be done
	//	return 0;

	if (sec_fan_pdata.probe_done) {
		pr_err("%s Something went wrong!\n", __func__);
		return -EEXIST;
	}

	if (unlikely(sec_fan_parse_dt(pdev)))
		pr_err("%s: WARNING!>..parse dt failed\n", __func__);

	platform_set_drvdata(pdev, &sec_fan_pdata);

	sec_fan_pdata.probe_done = true;
	pr_info("%s : done! ---\n", __func__);

	return 0;
}

/**
 * sec_fan_suspend - on sleep, switch off fan.
 **/
static int sec_fan_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sec_fan_drvdata *ddata = (struct sec_fan_drvdata *)sec_fan_pdata.ddata;

	int ret;

	mutex_lock(&ddata->mutex);
	pr_info("%s +++\n", __func__);
	ddata->ops->disable_irq(ddata->chip_dev);
	ret = ddata->ops->suspend(ddata->chip_dev);
	if(ret)
		pr_err("%s driver ic power_off failed\n", __func__);

	mutex_unlock(&ddata->mutex);
	return ret;
}

/**
 * sec_fan_resume -  on resume, power ON fan with default speed.
 **/
static int sec_fan_resume(struct platform_device *pdev)
{
	struct sec_fan_drvdata *ddata = (struct sec_fan_drvdata *)sec_fan_pdata.ddata;

	pr_info("%s +++\n", __func__);

	cancel_work_sync(&ddata->fan_resume_work);
	schedule_work(&ddata->fan_resume_work);

	return 0;
}

/**
 * sec_fan_shutdown - Called during shutdown. fan must be set off during
 *		 shutdown to avoid fan is running after shutdown
 **/
void sec_fan_shutdown(struct platform_device *pdev)
{
	struct sec_fan_drvdata *ddata = (struct sec_fan_drvdata *)sec_fan_pdata.ddata;

	int ret;

	mutex_lock(&ddata->mutex);
	pr_info("%s +++\n", __func__);
	ddata->ops->disable_irq(ddata->chip_dev);
	ret = ddata->ops->suspend(ddata->chip_dev);
	if(ret)
		pr_err("%s driver ic power_off failed\n", __func__);

	mutex_unlock(&ddata->mutex);
}

/**
 * sec_fan_id - Holds sec_fan compatible ID
 **/
static const struct of_device_id sec_fan_id[] = {
	{ .compatible = "sec_fan" },
	{ }
};

/**
 * sec_fan_driver - Holds platform driver mapping for
 *       Basic FAN platform devices
 **/
static struct platform_driver sec_fan_driver = {
	.probe		= sec_fan_probe,
	.resume		= sec_fan_resume,
	.suspend	= sec_fan_suspend,
	.shutdown	= sec_fan_shutdown,
	.driver		= {
		.name	=  "sec_fan",
		.owner	= THIS_MODULE,
		.of_match_table = sec_fan_id,
	},
};

/**
 * Register sec_fan platform driver as a sub-system module
 **/
static int __init sec_fan_init(void)
{
	if (!is_sec_fan_init_required())
		return -ENODEV;

	/* create /sys/class/sec_fan/ */
	sec_fan_class = class_create(THIS_MODULE, "sec_fan");
	if (IS_ERR(sec_fan_class)) {
		pr_err("%s error in creating sec_fan class %d\n", __func__, PTR_ERR(sec_fan_class));
		return -EIO;
	}

	return platform_driver_register(&sec_fan_driver);
}
subsys_initcall(sec_fan_init);

/**
 * Unregister sec_fan platform driver module
 **/
static void __exit sec_fan_exit(void)
{
	platform_driver_unregister(&sec_fan_driver);
}
module_exit(sec_fan_exit);

MODULE_AUTHOR("Samsung Electronics <t.sankar@samsung.com>");
MODULE_DESCRIPTION("sec_fan sub-system");
MODULE_LICENSE("GPL");
