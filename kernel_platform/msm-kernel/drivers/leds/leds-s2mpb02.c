/*
 * LED driver for Samsung S2MPB02
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver is based on leds-max77804.c
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mfd/samsung/s2mpb02.h>
#include <linux/mfd/samsung/s2mpb02-regulator.h>
#include <linux/leds-s2mpb02.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

struct device *s2mpb02_led_dev;
struct s2mpb02_led_data **global_led_datas;
struct s2mpb02_led_data** right_led_datas;
struct s2mpb02_led_data** left_led_datas;
bool sysfs_flash_op;
bool flash_config_factory;

#ifdef S2MPB02_FLED_CHANNEL_1
#define S2MPB02_REG_FLED_CTRL S2MPB02_REG_FLED_CTRL1
#define S2MPB02_REG_FLED_CUR S2MPB02_REG_FLED_CUR1
#define S2MPB02_REG_FLED_TIME S2MPB02_REG_FLED_TIME1
#else
#define S2MPB02_REG_FLED_CTRL S2MPB02_REG_FLED_CTRL2
#define S2MPB02_REG_FLED_CUR S2MPB02_REG_FLED_CUR2
#define S2MPB02_REG_FLED_TIME S2MPB02_REG_FLED_TIME2
#endif

//#define DEBUG_READ_REGISTER //To dump register values with /sys/kernel/debug/s2mpb02-led-regs
//#define DEBUG_WRITE_REGISTER //To write register with sysfs

struct s2mpb02_led_data {
	struct led_classdev led;
	struct s2mpb02_dev *s2mpb02;
	struct s2mpb02_led *data;
	struct i2c_client *i2c;
	struct work_struct work;
	struct mutex lock;
	spinlock_t value_lock;
	int brightness;
	int test_brightness;
	int pattern;
};

static u8 leds_mask[S2MPB02_LED_MAX] = {
	S2MPB02_FLASH_MASK,
	S2MPB02_TORCH_MASK,
};

static u8 leds_shift[S2MPB02_LED_MAX] = {
	4,
	0,
};

#if defined(DEBUG_READ_REGISTER)
#if 0
static void print_all_reg_value(struct i2c_client *client)
{
	int ret;
	u8 value;
	u8 i;

	for (i = 0; i <= S2MPB02_REG_LDO_DSCH3; i++) {
		ret = s2mpb02_read_reg(client, i, &value);
		if (unlikely(ret < 0))
			SLED_ERR("[s2mpb02-LED] read failed");
		SLED_ERR("[s2mpb02-LED] register(%x) = %x\n", i, value);
		value = 0;
	}
}
#endif
static int s2mpb02_debugfs_show(struct seq_file *s, void *data)
{
	struct s2mpb02_dev *ldata = s->private;
	u8 reg;
	u8 reg_data;
	int ret;

	seq_printf(s, "s2mpb02 IF PMIC :\n");
	seq_printf(s, "=============\n");
	for (reg = 0; reg <= S2MPB02_REG_LDO_DSCH3; reg++) {
		ret = s2mpb02_read_reg(ldata->i2c, reg, &reg_data);
		if (unlikely(ret < 0)) {
			SLED_ERR("[s2mpb02-LED] read failed");
		}
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}
	//print_all_reg_value(ldata->i2c);
	seq_printf(s, "\n");

	return 0;
}

static int s2mpb02_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, s2mpb02_debugfs_show, inode->i_private);
}

static const struct file_operations s2mpb02_debugfs_fops = {
	.open			= s2mpb02_debugfs_open,
	.read			= seq_read,
	.llseek 		= seq_lseek,
	.release		= single_release,
};
#endif

static int s2mpb02_set_bits(struct i2c_client *client, const u8 reg,
				 const u8 mask, const u8 inval)
{
	int ret;
	u8 value;

	ret = s2mpb02_read_reg(client, reg, &value);
	if (unlikely(ret < 0))
		return ret;

	value = (value & ~mask) | (inval & mask);

	ret = s2mpb02_write_reg(client, reg, value);

	return ret;
}

static int s2mpb02_direct_set_bits(struct i2c_client *client, const u8 reg,
				 const u8 mask, const u8 inval)
{
	int ret;
	/*u8 value;

	ret = s2mpb02_read_reg(client, reg, &value);
	if (unlikely(ret < 0))
		return ret;

	value = (value & ~mask) | (inval & mask);*/

	ret = s2mpb02_write_reg(client, reg, inval);

	return ret;
}

static int s2mpb02_led_get_en_value(struct s2mpb02_led_data *led_data, int on)
{
	if (on) {
		if (led_data->data->id == S2MPB02_FLASH_LED_1)
			return ((S2MPB02_FLED_ENABLE << S2MPB02_FLED_ENABLE_SHIFT) |
				(S2MPB02_FLED_FLASH_MODE << S2MPB02_FLED_MODE_SHIFT));
				/* Turn on FLASH by I2C */
		else
			return ((S2MPB02_FLED_ENABLE << S2MPB02_FLED_ENABLE_SHIFT) |
				(S2MPB02_FLED_TORCH_MODE << S2MPB02_FLED_MODE_SHIFT));
				/* Turn on TORCH by I2C */
	}
	else {
		if (led_data->data->id == S2MPB02_FLASH_LED_1)
			return ((S2MPB02_FLED_DISABLE << S2MPB02_FLED_ENABLE_SHIFT) |
				(S2MPB02_FLED_FLASH_MODE << S2MPB02_FLED_MODE_SHIFT));
		/* Turn on FLASH by I2C */
		else
			return ((S2MPB02_FLED_DISABLE << S2MPB02_FLED_ENABLE_SHIFT) |
				(S2MPB02_FLED_TORCH_MODE << S2MPB02_FLED_MODE_SHIFT));
	}
		//return (S2MPB02_FLED_DISABLE << S2MPB02_FLED_ENABLE_SHIFT);
				/* controlled by GPIO */
}

static void s2mpb02_led_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	unsigned long flags;
	struct s2mpb02_led_data *led_data
		= container_of(led_cdev, struct s2mpb02_led_data, led);

	SLED_DBG("[LED] %s\n", __func__);

	spin_lock_irqsave(&led_data->value_lock, flags);
	led_data->test_brightness = min_t(int, (int)value, (int)led_cdev->max_brightness);
	spin_unlock_irqrestore(&led_data->value_lock, flags);

	schedule_work(&led_data->work);
}

static void led_set(struct s2mpb02_led_data *led_data, enum s2mpb02_led_turn_way turn_way)
{
	int ret;
	struct s2mpb02_led *data = led_data->data;
	int id = data->id;
	int value = 0, i = 0;

	if (turn_way == S2MPB02_LED_TURN_WAY_GPIO) 
	{
		/* Turn way LED by GPIO */
		value = s2mpb02_led_get_en_value(led_data, 0);
		ret = s2mpb02_direct_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CTRL1, S2MPB02_FLED_ENABLE_MODE_MASK, value);
		if (unlikely(ret))
			goto error_set_bits;

		if (led_data->test_brightness == LED_OFF) 
		{
			/* set current */
			ret = s2mpb02_direct_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR1,
					  leds_mask[id], data->brightness << leds_shift[id]);
			if (unlikely(ret))
				goto error_set_bits;

#if defined(CONFIG_SEC_VST_PROJECT)
			if ((led_data->data) && 
				(led_data->data->gpio_count > 0) && 
				(led_data->data->gpio > 0))
			{
				gpio_request(led_data->data->gpio, NULL);
				gpio_direction_output(led_data->data->gpio, 0);
				gpio_free(led_data->data->gpio);
				if(led_data->data->gpio_count > 1 && led_data->data->gpio1 > 0)
				{
					gpio_request(led_data->data->gpio1, NULL);
					gpio_direction_output(led_data->data->gpio1, 0);
					gpio_free(led_data->data->gpio1);
				}
			}
#else
			for (i = 0; i < S2MPB02_LED_MAX; i++) {
				if (global_led_datas[i] && global_led_datas[i]->data) {
					gpio_request(global_led_datas[i]->data->gpio, NULL);
					gpio_direction_output(global_led_datas[i]->data->gpio, 0);
					gpio_free(global_led_datas[i]->data->gpio);
				}
			}
#endif
			//value = 0x31;
			value = 0;
			ret = s2mpb02_direct_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CUR1, 0xFF, value);
			if (unlikely(ret))
				goto error_set_bits;

			ret = s2mpb02_direct_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CUR2, 0xFF, value);
			if (unlikely(ret))
				goto error_set_bits;
		}
		else  // LED_ON
		{
			/* set current */
			ret = s2mpb02_direct_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR1,
					  leds_mask[id], data->brightness << leds_shift[id]);
			if (unlikely(ret))
				goto error_set_bits;
#if defined(CONFIG_SEC_VST_PROJECT)
			if ((led_data->data) && 
				(led_data->data->gpio_count > 0) && 
				(led_data->data->gpio > 0))
			{
				gpio_request(led_data->data->gpio, NULL);
				gpio_direction_output(led_data->data->gpio, ((led_data->pattern & 0x02) >> 1));
				gpio_free(led_data->data->gpio);
				if(led_data->data->gpio_count > 1 && led_data->data->gpio1 > 0)
				{
					gpio_request(led_data->data->gpio1, NULL);
					gpio_direction_output(led_data->data->gpio1, (led_data->pattern & 0x01));
					gpio_free(led_data->data->gpio1);
				}
			}
#else
			if (global_led_datas[id] && global_led_datas[id]->data) {
				gpio_request(global_led_datas[id]->data->gpio, NULL);
				gpio_direction_output(global_led_datas[id]->data->gpio, 1);
				gpio_free(global_led_datas[id]->data->gpio);
			}
#endif
			value = 0x31;
			if ((led_data->pattern & 0x02) >> 1 == 1)
			{
				ret = s2mpb02_direct_set_bits(led_data->i2c,
					S2MPB02_REG_FLED_CUR1, 0xFF, value | (data->brightness & leds_mask[id]));
				if (unlikely(ret))
					goto error_set_bits;
			}
			else {
				ret = s2mpb02_direct_set_bits(led_data->i2c,
					S2MPB02_REG_FLED_CUR1, 0xFF, value);
				if (unlikely(ret))
					goto error_set_bits;
			}

			if ((led_data->pattern & 0x01) == 1)
			{
				ret = s2mpb02_direct_set_bits(led_data->i2c,
					S2MPB02_REG_FLED_CUR2, 0xFF, value | (data->brightness & leds_mask[id]));
				if (unlikely(ret))
					goto error_set_bits;
			}
			else
			{
				ret = s2mpb02_direct_set_bits(led_data->i2c,
					S2MPB02_REG_FLED_CUR2, 0xFF, value);
				if (unlikely(ret))
					goto error_set_bits;
			}
		}
	}
	else
	{
		/* Turn way LED by I2C */
		if (led_data->test_brightness == LED_OFF)
		{
			value = 0x76;
			ret = s2mpb02_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CTRL2, 0xFF, value);
			if (unlikely(ret))
				goto error_set_bits;

			value = 0x73;
			ret = s2mpb02_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CTRL1, 0xFF, value);
			if (unlikely(ret))
				goto error_set_bits;

			//value = 0x31;
			value = 0;
			ret = s2mpb02_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CUR1, 0xFF, value);
			if (unlikely(ret))
				goto error_set_bits;

			ret = s2mpb02_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CUR2, 0xFF, value);
			if (unlikely(ret))
				goto error_set_bits;
		}
		else // LED_ON
		{
			// set current
			if ((led_data->pattern & 0x01) == 1)
			{
				ret = s2mpb02_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR1,
					leds_mask[id], data->brightness << leds_shift[id]);
				if (unlikely(ret))
					goto error_set_bits;
			}
			else
			{
				ret = s2mpb02_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR1,
					leds_mask[id], 0);
				if (unlikely(ret))
					goto error_set_bits;
			}

			if ((led_data->pattern & 0x02) >> 1 == 1)
			{
				ret = s2mpb02_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR2,
					leds_mask[id], data->brightness << leds_shift[id]);
				if (unlikely(ret))
					goto error_set_bits;
			}
			else
			{
				ret = s2mpb02_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR2,
					leds_mask[id], 0);
				if (unlikely(ret))
					goto error_set_bits;
			}

			value = (0xF6 & (led_data->pattern & 0x02) <<6);
			value = value | ((led_data->data->id == S2MPB02_FLASH_LED_1) ? (S2MPB02_FLED_FLASH_MODE << S2MPB02_FLED_MODE_SHIFT) : (S2MPB02_FLED_TORCH_MODE << S2MPB02_FLED_MODE_SHIFT));
			ret = s2mpb02_set_bits(led_data->i2c,
				S2MPB02_REG_FLED_CTRL2, S2MPB02_FLED_ENABLE_MODE_MASK, value);
			if (unlikely(ret))
				goto error_set_bits;

			value = (0xF3 & (led_data->pattern & 0x01) << 7);
			value = value | ((led_data->data->id == S2MPB02_FLASH_LED_1) ? (S2MPB02_FLED_FLASH_MODE << S2MPB02_FLED_MODE_SHIFT) : (S2MPB02_FLED_TORCH_MODE << S2MPB02_FLED_MODE_SHIFT));
			ret = s2mpb02_set_bits(led_data->i2c,
						S2MPB02_REG_FLED_CTRL1, S2MPB02_FLED_ENABLE_MODE_MASK, value);
			if (unlikely(ret))
				goto error_set_bits;
		}
	}

	return;

error_set_bits:
	SLED_ERR("%s: can't set led level %d\n", __func__, ret);

	return;
}

static void s2mpb02_led_work(struct work_struct *work)
{
	struct s2mpb02_led_data *led_data
		= container_of(work, struct s2mpb02_led_data, work);

	SLED_DBG("[LED] %s\n", __func__);

	mutex_lock(&led_data->lock);
	led_set(led_data, S2MPB02_LED_TURN_WAY_I2C);
	mutex_unlock(&led_data->lock);
}

static int s2mpb02_led_setup(struct s2mpb02_led_data *led_data)
{
	int ret = 0;
	struct s2mpb02_led *data = led_data->data;
	int id = data->id;
	int value;
	SLED_DBG("enter setup: id: %d\n", id);

	/* Disable Low Voltage operating mode control */
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL1,
				S2MPB02_FLED_CTRL1_LV_DISABLE, S2MPB02_FLED_CTRL1_LV_EN_MASK);

	/* set operating minimum voltage */
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL1,
				S2MPB02_LV_SEL_VOLT(3000), S2MPB02_LV_SEL_VOUT_MASK);

	/* set current & timeout */
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_CUR1,
				data->brightness << leds_shift[id], leds_mask[id]);
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_TIME1,
				data->timeout << leds_shift[id], leds_mask[id]);

	value = s2mpb02_led_get_en_value(led_data, 0);
	ret |= s2mpb02_update_reg(led_data->i2c,
				S2MPB02_REG_FLED_CTRL1, value, S2MPB02_FLED_ENABLE_MODE_MASK);

	value = 0x76;
	ret |= s2mpb02_update_reg(led_data->i2c,
		S2MPB02_REG_FLED_CTRL2, value, 0xFF);

	value = 0x73;
	ret |= s2mpb02_update_reg(led_data->i2c,
		S2MPB02_REG_FLED_CTRL1, value, 0xFF);

	value = 0x3F;
	ret |= s2mpb02_update_reg(led_data->i2c,
		S2MPB02_REG_FLED_TIME1, value, 0xFF);

	value = 0x30;
	ret |= s2mpb02_update_reg(led_data->i2c,
		S2MPB02_REG_FLED_CUR1, value, 0xFF);

	ret |= s2mpb02_update_reg(led_data->i2c,
		S2MPB02_REG_FLED_CUR2, value, 0xFF);

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
	ret |= s2mpb02_ir_led_init();
#endif

	return ret;
}

void s2mpb02_led_get_status(struct led_classdev *led_cdev, bool status, bool onoff)
{
	int ret = 0;
	u8 value[6] = {0, };
	struct s2mpb02_led_data *led_data
		= container_of(led_cdev, struct s2mpb02_led_data, led);

	ret = s2mpb02_read_reg(led_data->i2c, 0x12, &value[0]); //Fled_ctrl1
	ret |= s2mpb02_read_reg(led_data->i2c, 0x13, &value[1]); //Fled_ctrl2
	ret |= s2mpb02_read_reg(led_data->i2c, 0x14, &value[2]); //Fled_cur1
	ret |= s2mpb02_read_reg(led_data->i2c, 0x15, &value[3]); //Fled_time1
	ret |= s2mpb02_read_reg(led_data->i2c, 0x16, &value[4]); //Fled_cur2
	ret |= s2mpb02_read_reg(led_data->i2c, 0x17, &value[5]); //Fled_time2
	if (unlikely(ret < 0)) {
		printk("%s : error to get dt node\n", __func__);
	}

	printk("%s[%d, %d] : Fled_ctrl1 = 0x%12x, Fled_ctrl2 = 0x%13x, Fled_cur1 = 0x%14x, "
		"Fled_time1 = 0x%15x, Fled_cur2 = 0x%16x, Fled_time2 = 0x%17x\n",
		__func__, status, onoff, value[0], value[1], value[2], value[3], value[4], value[5]);
}

int s2mpb02_led_en(int led_type, int mode, int onoff, int pattern, enum s2mpb02_led_turn_way turn_way)
{
	int ret = 0;
	int i = 0;
	int brightness = 0;
	int test_brightness;
	if (sysfs_flash_op) {
		pr_warn("%s : The camera led control is not allowed"
			"because sysfs led control already used it\n", __FUNCTION__);
		return 0; //no error
	}

	if (global_led_datas == NULL) {
		SLED_ERR("<%s> global_led_datas is NULL\n", __func__);
		return -1;
	}
	
	for (i = 0; i < S2MPB02_LED_MAX; i++) {
		if (global_led_datas[i] == NULL) {
			SLED_ERR("<%s> global_led_datas[%d] is NULL\n", __func__, i);
			return -1;
		}
	}
	SLED_INFO("en: %d, md: %d ptr: 0x%x way: %d\n",
		onoff, mode, pattern, turn_way);
	if (onoff > 0) {/* enable */
		if (mode == S2MPB02_TORCH_LED_1) {
			if (onoff >= S2MPB02_TORCH_OUT_I_MAX)
				onoff = S2MPB02_TORCH_OUT_I_MAX-1;
		} else if (mode == S2MPB02_FLASH_LED_1) {
			if (onoff >= S2MPB02_FLASH_OUT_I_MAX)
				onoff = S2MPB02_FLASH_OUT_I_MAX-1;
		} else {
			SLED_ERR("<%s> mode %d is invalid\n", __func__, mode);
			return -1;
		}
		brightness = onoff;
		test_brightness = LED_FULL;
		global_led_datas[mode]->data->brightness = onoff;
		global_led_datas[mode]->test_brightness = LED_FULL;
	} else {/* disable */
		global_led_datas[mode]->test_brightness = LED_OFF;
		test_brightness = LED_OFF;
	}

	if (led_type == 0) {
		left_led_datas[mode]->data->brightness = brightness;
		left_led_datas[mode]->test_brightness = test_brightness;
		left_led_datas[mode]->pattern = pattern;
		led_set(left_led_datas[mode], turn_way);
	}
	else if (led_type == 1) {
		right_led_datas[mode]->data->brightness = brightness;
		right_led_datas[mode]->test_brightness = test_brightness;
		right_led_datas[mode]->pattern = pattern;
		led_set(right_led_datas[mode], turn_way);
	}
	else if (led_type == 2) {
		right_led_datas[mode]->data->brightness = brightness;
		right_led_datas[mode]->test_brightness = test_brightness;
		right_led_datas[mode]->pattern = pattern;
		led_set(right_led_datas[mode], turn_way);

		left_led_datas[mode]->data->brightness = brightness;
		left_led_datas[mode]->test_brightness = test_brightness;
		left_led_datas[mode]->pattern = pattern;
		led_set(left_led_datas[mode], turn_way);
	}
	
	return ret;
}
EXPORT_SYMBOL(s2mpb02_led_en);

struct device* cam_dev_flash_copy(void)
{
	return s2mpb02_led_dev;
}
EXPORT_SYMBOL(cam_dev_flash_copy);

#ifdef DEBUG_WRITE_REGISTER
ssize_t s2mpb02_write(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	unsigned int value	= 0;
	unsigned char reg = 0;
	unsigned char data = 0;

	if (buf == NULL || kstrtouint(buf, 16, &value)) {
		SLED_ERR("[%s] error buf is NULL\n", __func__);
		return -1;
	}

	reg = value >> 8;
	data = value & 0xFF;
	SLED_INFO("[%s] reg: %x, data: %x\n", __func__, reg, data);

	s2mpb02_write_reg(global_led_datas[0]->i2c, reg, data);

	return count;
}
static DEVICE_ATTR(s2mpb02_cam_reg, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	NULL, s2mpb02_write);
#endif

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
int s2mpb02_ir_led_init(void)
{
	int ret = 0;

	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CTRL2, 0x38);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CUR2, 0xAF);

	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_TIME2, 0x34);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_TIME2, 0x35);

	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON1, 0x19);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON2, 0x0B);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRD1, 0x00);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRD2, 0X2C);

#if 1 //TEMP_845
    s2mpb02_ir_led_current(5);
    s2mpb02_ir_led_pulse_width(240);
    s2mpb02_ir_led_pulse_delay(0);
    s2mpb02_ir_led_max_time(0);
#endif

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_init);

int s2mpb02_ir_led_current(int32_t current_value)
{
	int ret = 0;
	unsigned int value = 0;
	unsigned char data = 0;

	if (current_value > 0)
		value  = current_value - 1;

	SLED_INFO(" led current value : %u [current_value::%d]\n", value, current_value);

	data = ((value & 0x0F) << 4) | 0x0F;

	ret = s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CUR2, data);
	if (ret < 0)
		SLED_ERR("[%s] i2c write error", __func__);

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_current);

int s2mpb02_ir_led_pulse_width(int32_t width)
{
	unsigned int value	= width;
	unsigned char iron1 = 0;
	unsigned char iron2 = 0;
	int ret = 0;

	SLED_INFO("led pulse_width value : %u\n", value);

	iron1 = (value >> 2) & 0xFF;
	iron2 = ((value & 0x03) << 6) | 0x0B;

	SLED_INFO(" IRON1(0x%02x), IRON2(0x%02x)\n" , iron1, iron2);

	/* set 0x18, 0x19 */
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON1, iron1);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON2, iron2);
	if (ret < 0)
		SLED_ERR("[%s] i2c write error", __func__);

	return ret;

}
EXPORT_SYMBOL(s2mpb02_ir_led_pulse_width);

int s2mpb02_ir_led_pulse_delay(int32_t delay)
{
	unsigned int value	= delay;
	unsigned char ird1 = 0;
	unsigned char ird2 = 0;
	int ret = 0;

	SLED_INFO("led pulse_delay value : %u\n", value);

	ird1 = (value >> 2) & 0xFF;
	ird2 = ((value & 0x03) << 6) | 0x2C; /* value 0x2C means RSVD[5:0] Reserved */

	SLED_INFO("IRD1(0x%02x), IRD2(0x%02x)\n", ird1, ird2);

	/* set 0x18, 0x19 */
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRD1, ird1);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRD2, ird2);
	if (ret < 0)
		SLED_ERR("[%s] i2c write error", __func__);

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_pulse_delay);

int s2mpb02_ir_led_max_time(int32_t max_time)
{
	int ret = 0;

	SLED_INFO(" led max_time value : %u\n", max_time);

	ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CTRL2,
		S2MPB02_FLED2_MAX_TIME_CLEAR_MASK, 0x00);
	ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_TIME2,
		S2MPB02_FLED2_MAX_TIME_EN_MASK, 0x00);
	if (max_time > 0) {
		ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_TIME2,
			S2MPB02_FLED2_MAX_TIME_EN_MASK, 0x01);

		ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON2,
			S2MPB02_FLED2_MAX_TIME_MASK, (u8) max_time - 1);
	}

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_max_time);
#endif

void s2mpb02_set_brightness(int led_type, int onoff, int pattern, enum s2mpb02_led_turn_way turn_way)
{
	SLED_DBG(" sysfs torch/flash led_type: %d value %d pattern: 0x%x turn_way %d\n",
		led_type, onoff, pattern, turn_way);
	if (onoff <= 0)
		onoff = 0;
	else if(onoff >= 100)
		onoff = S2MPB02_TORCH_OUT_I_240MA;
	else if(onoff >= 90)
		onoff = S2MPB02_TORCH_OUT_I_200MA;
	else if(onoff >= 80)
		onoff = S2MPB02_TORCH_OUT_I_180MA;
	else if(onoff >= 70)
		onoff = S2MPB02_TORCH_OUT_I_160MA;
	else if(onoff >= 60)
		onoff = S2MPB02_TORCH_OUT_I_140MA;
	else if(onoff >= 50)
		onoff = S2MPB02_TORCH_OUT_I_120MA;
	else if(onoff >= 40)
		onoff = S2MPB02_TORCH_OUT_I_100MA;
	else if(onoff >= 30)
		onoff = S2MPB02_TORCH_OUT_I_80MA;
	else if(onoff >= 20)
		onoff = S2MPB02_TORCH_OUT_I_60MA;
	else if(onoff >= 10)
		onoff = S2MPB02_TORCH_OUT_I_40MA;
	else if(onoff >= 1)
		onoff = S2MPB02_TORCH_OUT_I_20MA;

	s2mpb02_led_en(led_type, S2MPB02_TORCH_LED_1, onoff, pattern, turn_way);

	if (onoff)
		sysfs_flash_op = 1;
}

EXPORT_SYMBOL(s2mpb02_led_ctrl);
int s2mpb02_led_ctrl(int ledType,int brightness, int pattern, int strobe)
{
	int i = 0;
	sysfs_flash_op = 0;
	if (ledType == 0) {
		if (left_led_datas == NULL) {
			SLED_ERR("<%s> left_led_datas is NULL\n", __func__);
			return -1;
		}
		for (i = 0; i < S2MPB02_LED_MAX; i++)
		{
			if (left_led_datas[i] == NULL) {
				SLED_ERR("<%s> left_led_datas[%d] is NULL\n", __func__, i);
				return -1;
			}
		}
	}
	else if (ledType == 1) {
		if (right_led_datas == NULL) {
			SLED_ERR("<%s> right_led_datas is NULL\n", __func__);
			return -1;
		}
		for (i = 0; i < S2MPB02_LED_MAX; i++)
		{
			if (right_led_datas[i] == NULL) {
				SLED_ERR("<%s> right_led_datas[%d] is NULL\n", __func__, i);
				return -1;
			}
		}
	}
	else if (ledType == 2) {
		if (left_led_datas == NULL) {
			SLED_ERR("<%s> left_led_datas is NULL\n", __func__);
			return -1;
		}
		if (right_led_datas == NULL) {
			SLED_ERR("<%s> right_led_datas is NULL\n", __func__);
			return -1;
		}
		for (i = 0; i < S2MPB02_LED_MAX; i++)
		{
			if (right_led_datas[i] == NULL) {
				SLED_ERR("<%s> right_led_datas[%d] is NULL\n", __func__, i);
				return -1;
			}
			if (left_led_datas[i] == NULL) {
				SLED_ERR("<%s> left_led_datas[%d] is NULL\n", __func__, i);
				return -1;
			}
		}
	}
	else {
		SLED_ERR("Invalid ledType value: %d", ledType);
		return -1;
	}

	SLED_ERR("Value of led_data received: brightness: 0x%x pattern: 0x%x ledType 0x%x strobe %d", brightness, pattern, ledType, strobe);
	s2mpb02_set_brightness(ledType, brightness, pattern, strobe);
	
	return 0;
}

ssize_t s2mpb02_store_left(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int i = 0;
	int onoff = -1, brightness = 0, pattern = 3;
	sysfs_flash_op = 0;

	if (buf == NULL || kstrtouint(buf, 10, &onoff))
		return -1;

	if (left_led_datas == NULL) {
		SLED_ERR("<%s> left_led_datas is NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < S2MPB02_LED_MAX; i++) {
		if (left_led_datas[i] == NULL) {
			SLED_ERR("<%s> left_led_datas[%d] is NULL\n", __func__, i);
			return -1;
		}
	}

	brightness = (onoff & 0x0ff);
	pattern = ((onoff >> 8) & 0xf);
	s2mpb02_set_brightness(S2MPB02_LED_LEFT, brightness, pattern, S2MPB02_LED_TURN_WAY_I2C);
	
	return count;
}

ssize_t s2mpb02_show_left(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	u8 value;
	u8 i;
	u8 pseudo_reg[S2MPB02_REG_LDO_DSCH3] = { 0, };
	int brightness = 0;
	if (left_led_datas == NULL) {
		SLED_ERR("<%s> left_led_datas is NULL\n", __func__);
		return sprintf(buf, "%d\n", -1);
	} 

	for (i = 0; i < S2MPB02_LED_MAX; i++) {
		if (left_led_datas[i] == NULL) {
			SLED_ERR("<%s> left_led_datas[%d] is NULL\n", __func__, i);
			return sprintf(buf, "%d\n", -1);
		}
	}
	if (left_led_datas[S2MPB02_TORCH_LED_1]->test_brightness == LED_OFF) {
		brightness = 0;
	} else {
		brightness = left_led_datas[S2MPB02_TORCH_LED_1]->data->brightness;
	}
	for (i = 0; i <= S2MPB02_REG_LDO_DSCH3; i++) {
		ret = s2mpb02_read_reg(left_led_datas[S2MPB02_TORCH_LED_1]->i2c, i, &value);
		if (unlikely(ret < 0))
			SLED_ERR("[s2mpb02-LED] read failed");
		SLED_ERR("[s2mpb02-LED] register(%x) = %x\n", i, value);
		pseudo_reg[i] = value;
		value = 0;
	}
	return sprintf(buf, \
			"SUCCESS\nBrightess: %d\nmode_reg[0x12 ~ 0x17] = 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X \n", \
			brightness,
			pseudo_reg[0x12], pseudo_reg[0x13], pseudo_reg[0x14], pseudo_reg[0x15], pseudo_reg[0x16], pseudo_reg[0x17]);
}

static DEVICE_ATTR(et_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	s2mpb02_show_left, s2mpb02_store_left);

ssize_t s2mpb02_store_right(struct device* dev,
	struct device_attribute* attr, const char* buf,
	size_t count)
{
	int i = 0;
	int onoff = -1, brightness = 0, pattern=3;
	sysfs_flash_op = 0;

	if (buf == NULL || kstrtouint(buf, 10, &onoff))
		return -1;

	if (right_led_datas == NULL) {
		SLED_ERR("<%s> right_led_datas is NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < S2MPB02_LED_MAX; i++) {
		if (right_led_datas[i] == NULL) {
			SLED_ERR("<%s> right_led_datas[%d] is NULL\n", __func__, i);
			return -1;
		}
	}

	brightness = (onoff & 0x0ff);
	pattern = (((onoff >> 8) & 0xf)<<4);
	s2mpb02_set_brightness(S2MPB02_LED_RIGHT, brightness, pattern, S2MPB02_LED_TURN_WAY_I2C);

	return count;
}

ssize_t s2mpb02_show_right(struct device* dev,
	struct device_attribute* attr, char* buf)
{
	int ret;
	u8 value;
	u8 i;
	u8 pseudo_reg[S2MPB02_REG_LDO_DSCH3] = { 0, };
	int brightness = 0;
	if (right_led_datas == NULL) {
		SLED_ERR("<%s> left_led_datas is NULL\n", __func__);
		return sprintf(buf, "%d\n", -1);
	} 

	for (i = 0; i < S2MPB02_LED_MAX; i++) {
		if (right_led_datas[i] == NULL) {
			SLED_ERR("<%s> left_led_datas[%d] is NULL\n", __func__, i);
			return sprintf(buf, "%d\n", -1);
		}
	}
	if (right_led_datas[S2MPB02_TORCH_LED_1]->test_brightness == LED_OFF) {
		brightness = 0;
	} else {
		brightness = right_led_datas[S2MPB02_TORCH_LED_1]->data->brightness;
	}
	for (i = 0; i <= S2MPB02_REG_LDO_DSCH3; i++) {
		ret = s2mpb02_read_reg(right_led_datas[S2MPB02_TORCH_LED_1]->i2c, i, &value);
		if (unlikely(ret < 0))
			SLED_ERR("[s2mpb02-LED] read failed");
		SLED_ERR("[s2mpb02-LED] register(%x) = %x\n", i, value);
		pseudo_reg[i] = value;
		value = 0;
	}
	return sprintf(buf, \
			"SUCCESS\nBrightess: %d\nmode_reg[0x12 ~ 0x17] = 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X \n", \
			brightness,
			pseudo_reg[0x12], pseudo_reg[0x13], pseudo_reg[0x14], pseudo_reg[0x15], pseudo_reg[0x16], pseudo_reg[0x17]);
}

static DEVICE_ATTR(et2_flash, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
	s2mpb02_show_right, s2mpb02_store_right);

#if defined(CONFIG_OF)
static int of_s2mpb02_led_dt(struct s2mpb02_dev *iodev,
					struct s2mpb02_led_platform_data *pdata)
{
	struct device_node *led_np, *np, *c_np;
	int ret;
	u32 temp;
	const char *temp_str;
	int index;

	led_np = iodev->dev->of_node;
	if (!led_np) {
		SLED_ERR("<%s> could not find led sub-node\n", __func__);
		return -ENODEV;
	}

	np = of_find_node_by_name(led_np, "torch");
	if (!np) {
		SLED_ERR("<%s> could not find led sub-node\n",
								__func__);
		return -EINVAL;
	}

	pdata->num_leds = of_get_child_count(np);

	for_each_child_of_node(np, c_np) {
		ret = of_property_read_u32(c_np, "id", &temp);
		if (ret < 0)
			goto dt_err;
		index = temp;
		pdata->leds[index].id = temp;

		ret = of_property_read_string(c_np, "ledname", &temp_str);
		if (ret < 0)
			goto dt_err;
		pdata->leds[index].name = temp_str;

		ret = of_property_read_u32(c_np, "brightness", &temp);
		if (ret < 0)
			goto dt_err;
		if (temp > S2MPB02_FLASH_TORCH_CURRENT_MAX)
			temp = S2MPB02_FLASH_TORCH_CURRENT_MAX;
		pdata->leds[index].brightness = temp;

		ret = of_property_read_u32(c_np, "timeout", &temp);
		if (ret < 0)
			goto dt_err;
		if (temp > S2MPB02_TIMEOUT_MAX)
			temp = S2MPB02_TIMEOUT_MAX;
		pdata->leds[index].timeout = temp;

		ret = of_property_read_string(c_np, "default-trigger", &temp_str);
		if (ret < 0)
			goto dt_err;
		pdata->leds[index].default_trigger = temp_str;

		temp = of_gpio_count(c_np);
		pdata->leds[index].gpio_count = 0;
		if (temp > 0 && temp < S2MPB02_GPIO_MAX) {
			pdata->leds[index].gpio_count = temp;
			pdata->leds[index].gpio = of_get_gpio(c_np, 0);
			if (temp > 1) {
				pdata->leds[index].gpio1 = of_get_gpio(c_np, 1);
			}
		}
	}
	of_node_put(led_np);

	return 0;
dt_err:
	SLED_ERR("%s failed to get a dt info\n", __func__);
	return ret;
}
#endif /* CONFIG_OF */

static int s2mpb02_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct s2mpb02_dev *s2mpb02 = dev_get_drvdata(pdev->dev.parent);
#ifndef CONFIG_OF
	struct s2mpb02_platform_data *s2mpb02_pdata
		= dev_get_platdata(s2mpb02->dev);
#endif
	struct s2mpb02_led_platform_data *pdata;
	struct s2mpb02_led_data *led_data;
	struct s2mpb02_led *data;
	struct s2mpb02_led_data **led_datas;

#ifdef CONFIG_OF    // defined
	pdata = kzalloc(sizeof(struct s2mpb02_led_platform_data), GFP_KERNEL);
	if (!pdata) {
		SLED_ERR("%s: failed to allocate driver data\n", __func__);
		return -ENOMEM;
	}
	ret = of_s2mpb02_led_dt(s2mpb02, pdata);
	if (ret < 0) {
		SLED_ERR("s2mpb02-torch : %s not found torch dt! ret[%d]\n",
				 __func__, ret);
		kfree(pdata);
		return ret;
	}
#else
	pdata = s2mpb02_pdata->led_data;
	if (pdata == NULL) {
		SLED_ERR("[LED] no platform data for this led is found\n");
		return -EFAULT;
	}
#endif

	sysfs_flash_op = 0; //default off
	global_led_datas = kzalloc(sizeof(struct s2mpb02_led_data *)*S2MPB02_LED_MAX, GFP_KERNEL);
	if (left_led_datas == NULL) {
		left_led_datas = kzalloc(sizeof(struct s2mpb02_led_data*) * S2MPB02_LED_MAX, GFP_KERNEL);
	}
	if (right_led_datas == NULL) {
		right_led_datas = kzalloc(sizeof(struct s2mpb02_led_data*) * S2MPB02_LED_MAX, GFP_KERNEL);
	}

	led_datas = kzalloc(sizeof(struct s2mpb02_led_data *)
				* S2MPB02_LED_MAX, GFP_KERNEL);
	if (unlikely(!led_datas)) {
		SLED_ERR("[LED] memory allocation error %s", __func__);
		kfree(pdata);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, led_datas);

	SLED_DBG("number of leds: %d \n", pdata->num_leds);

	for (i = 0; i != pdata->num_leds; ++i) {
		SLED_INFO("led[%d]  ledname: %s, led_id: %d, setup\n", i, pdata->leds[i].name, pdata->leds[i].id);

		data = kzalloc(sizeof(struct s2mpb02_led), GFP_KERNEL);
		if (unlikely(!data)) {
			SLED_ERR("[LED] memory allocation error %s\n", __func__);
			ret = -ENOMEM;
			continue;
		}

		memcpy(data, &(pdata->leds[i]), sizeof(struct s2mpb02_led));

		led_data = kzalloc(sizeof(struct s2mpb02_led_data), GFP_KERNEL);

		global_led_datas[i] = led_data;
		led_datas[i] = led_data;
		if (unlikely(!led_data)) {
			SLED_ERR("[LED] memory allocation error %s\n", __func__);
			ret = -ENOMEM;
			kfree(data);
			continue;
		}

		led_data->s2mpb02 = s2mpb02;
		led_data->i2c = s2mpb02->i2c;
		led_data->data = data;
		led_data->led.name = data->name;
		led_data->led.brightness_set = s2mpb02_led_set;
		led_data->led.brightness = LED_OFF;
		led_data->brightness = data->brightness;
		led_data->led.flags = 0;
		led_data->led.max_brightness = LED_FULL;
		led_data->led.default_trigger = data->default_trigger;

		if (strcmp(led_data->led.name, LEFT_LEDS_NAME) == 0 || strcmp(led_data->led.name, LEFT_TORCH_NAME) == 0)
		{
			left_led_datas[i] = led_data;
			SLED_DBG("left_led_datas[%d] initialized, led_data value=%p",i, led_data);
		}
		else if (strcmp(led_data->led.name, RIGHT_LEDS_NAME) == 0 || strcmp(led_data->led.name, RIGHT_TORCH_NAME) == 0)
		{
			right_led_datas[i] = led_data;
			SLED_DBG("right_led_datas[%d] initialized, led_data value=%p", i, led_data);
		}

		mutex_init(&led_data->lock);
		spin_lock_init(&led_data->value_lock);
		INIT_WORK(&led_data->work, s2mpb02_led_work);

		ret = led_classdev_register(&pdev->dev, &led_data->led);
		if (unlikely(ret)) {
			SLED_ERR("unable to register LED\n");
			cancel_work_sync(&led_data->work);
			mutex_destroy(&led_data->lock);
			kfree(data);
			kfree(led_data);
			led_datas[i] = NULL;
			ret = -EFAULT;
			continue;
		}

		ret = s2mpb02_led_setup(led_data);
		if (unlikely(ret)) {
			SLED_ERR("unable to register LED\n");
			cancel_work_sync(&led_data->work);
			mutex_destroy(&led_data->lock);
			led_classdev_unregister(&led_data->led);
			kfree(data);
			kfree(led_data);
			led_datas[i] = NULL;
			ret = -EFAULT;
		}
	}

#if defined(DEBUG_READ_REGISTER)
	(void)debugfs_create_file("s2mpb02-led-regs", S_IRUGO, NULL,
					(void *)s2mpb02, &s2mpb02_debugfs_fops);
#endif

#ifdef CONFIG_OF
	kfree(pdata);
#endif
	SLED_INFO("done. ret: %d \n", ret);
	return ret;
}

static int s2mpb02_led_remove(struct platform_device *pdev)
{
	struct s2mpb02_led_data **led_datas = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i != S2MPB02_LED_MAX; ++i) {
		if (led_datas[i] == NULL)
			continue;

		cancel_work_sync(&led_datas[i]->work);
		mutex_destroy(&led_datas[i]->lock);
		led_classdev_unregister(&led_datas[i]->led);
		kfree(led_datas[i]->data);
		kfree(led_datas[i]);
	}
	kfree(led_datas);

	if (global_led_datas != NULL)
		kfree(global_led_datas);

	if (left_led_datas != NULL)
		kfree(left_led_datas);

	if (right_led_datas != NULL)
		kfree(right_led_datas);

	device_remove_file(s2mpb02_led_dev, &dev_attr_et_flash);
	device_remove_file(s2mpb02_led_dev, &dev_attr_et2_flash);

#ifdef DEBUG_WRITE_REGISTER
	device_remove_file(s2mpb02_led_dev, &dev_attr_s2mpb02_cam_reg);
#endif

	return 0;
}

static struct platform_driver s2mpb02_led_driver = {
	.probe		= s2mpb02_led_probe,
	.remove		= s2mpb02_led_remove,
	.driver		= {
		.name	= "s2mpb02-led",
		.owner	= THIS_MODULE,
		.suppress_bind_attrs = true,
	},
};

static int __init s2mpb02_led_init(void)
{
	return platform_driver_register(&s2mpb02_led_driver);
}
module_init(s2mpb02_led_init);

static void __exit s2mpb02_led_exit(void)
{
	platform_driver_unregister(&s2mpb02_led_driver);
}
module_exit(s2mpb02_led_exit);

MODULE_DESCRIPTION("S2MPB02 LED driver");
MODULE_LICENSE("GPL");
