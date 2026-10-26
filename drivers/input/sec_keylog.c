/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list_sort.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
struct class *key_sec_class;


#define SECLOG "[sec_input]"

struct device *sec_key;
EXPORT_SYMBOL(sec_key);
struct device *sec_power_key;
EXPORT_SYMBOL(sec_power_key);

static atomic_t sec_keylog_acceptable_event[KEY_MAX] __read_mostly;

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)		(sizeof(a) / sizeof(a[0]))
#endif

static unsigned int __keys_log[] = {
	KEY_POWER,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_HOMEPAGE,
	KEY_BRIGHTNESSDOWN,
	KEY_BRIGHTNESSUP
};

int key_state;
int powerkey_state;

static ssize_t key_pressed_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (key_state)
		sprintf(buf, "PRESS");
	else
		sprintf(buf, "RELEASE");

	return strlen(buf);
}

static ssize_t key_pressed_power_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (powerkey_state)
		sprintf(buf, "PRESS");
	else
		sprintf(buf, "RELEASE");

	return strlen(buf);
}


static DEVICE_ATTR(sec_key_pressed, 0444, key_pressed_show, NULL);
static DEVICE_ATTR(sec_power_key_pressed, 0664, key_pressed_power_show, NULL);

static struct attribute *sec_key_attrs[] = {
	&dev_attr_sec_key_pressed.attr,
	NULL,
};
static struct attribute_group sec_key_attr_group = {
	.attrs = sec_key_attrs,
};

static struct attribute *sec_power_key_attrs[] = {
	&dev_attr_sec_power_key_pressed.attr,
	NULL,
};
static struct attribute_group sec_power_key_attr_group = {
	.attrs = sec_power_key_attrs,
};

static void inline update_acceptable_event(unsigned int event_code, bool is_add)
{
	if (is_add)
		atomic_inc(&(sec_keylog_acceptable_event[event_code]));
	else
		atomic_dec(&(sec_keylog_acceptable_event[event_code]));
}

static inline bool is_event_supported(unsigned int event_type,
		unsigned int event_code)
{
	bool ret;

	if (event_type != EV_KEY || event_code >= KEY_MAX)
		return false;

	ret = !!atomic_read(&(sec_keylog_acceptable_event[event_code]));

	return ret;
}

static void sec_keylog_event(struct input_handle *handle, unsigned int event_type,
		unsigned int event_code, int value)
{
	if (!is_event_supported(event_type, event_code))
		return;

	if (event_code == KEY_POWER) {
		powerkey_state = value;
	} else {
		key_state = value;
	}

	pr_info("%s key_event: %u, %d\n", SECLOG, event_code, value);
}

static int sec_keylog_connect(struct input_handler *handler, struct input_dev *dev,
		const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "sec_keylog";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

err_unregister_handle:
	input_unregister_handle(handle);
err_free_handle:
	kfree(handle);
	return error;
}

static void sec_keylog_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id sec_keylog_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};

static struct input_handler sec_keylog_handler = {
	.event		= sec_keylog_event,
	.connect	= sec_keylog_connect,
	.disconnect	= sec_keylog_disconnect,
	.name		= "sec_keylog",
	.id_table	= sec_keylog_ids,
};

static int __init sec_keylog_init(void)
{
	int err;
	size_t i;

	pr_info("%s %s\n", SECLOG, __func__);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_key = sec_device_create(NULL, "sec_key");
	if (IS_ERR(sec_key))
		pr_err("%s failed to create sec_key\n", __func__);
#else
	key_sec_class = class_create(THIS_MODULE, "key_sec");
	if (unlikely(IS_ERR(key_sec_class))) {
		pr_err("%s %s: Failed to create class(sec) %ld\n", SECLOG, __func__, PTR_ERR(key_sec_class));
		return PTR_ERR(key_sec_class);
	}
	sec_key = device_create(key_sec_class, NULL, 11, NULL, "%s", "sec_key");
#endif

	err = sysfs_create_group(&sec_key->kobj, &sec_key_attr_group);
	if (err)
		pr_info("%s Unable to export keys/switches, error: %d\n", SECLOG,
			err);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_power_key = sec_device_create(NULL, "sec_power_key");
#else
	sec_power_key = device_create(key_sec_class, NULL, 12, NULL, "%s", "sec_power_key");
#endif
	if (IS_ERR(sec_power_key))
		pr_err("%s failed to create sec_power_key\n", SECLOG);

	err = sysfs_create_group(&sec_power_key->kobj, &sec_power_key_attr_group);
	if (err)
		pr_err("%s Unable to export keys/switches, error: %d\n", SECLOG,
			err);

	for (i = 0; i < KEY_MAX; i++)
		atomic_set(&(sec_keylog_acceptable_event[i]), 0);

	for (i = 0; i < ARRAY_SIZE(__keys_log); i++)
		update_acceptable_event(__keys_log[i], true);

	err = input_register_handler(&sec_keylog_handler);

	return err;

}

static void __exit sec_keylog_exit(void)
{
	input_unregister_handler(&sec_keylog_handler);
}

module_init(sec_keylog_init);
module_exit(sec_keylog_exit);

MODULE_DESCRIPTION("Samsung tsp dumpkey driver");
MODULE_LICENSE("GPL");

