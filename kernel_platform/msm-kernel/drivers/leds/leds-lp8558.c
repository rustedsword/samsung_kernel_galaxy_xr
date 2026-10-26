// SPDX-License-Identifier: GPL-2.0-only
/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
 //#include <linux/platform_data/lp855x.h>
#include <linux/leds-lp8558.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>

/* LP8550/1/2/3/6 Registers */
#define LP855X_BRIGHTNESS_CTRL		        0x00
#define LP855X_DEVICE_CTRL		            0x01
#define LP855X_LED_ENABLE_CTRL              0x16
#define LP855X_EEPROM_START		            0xA0
#define LP855X_EEPROM_END		            0xA7
#define LP855X_VBOOST_RANGE                 0x9E
#define LP855X_PWM_DIRECT_MODE_ENABLE_CTRL  0xA5
#define LP855X_VBOOST_MAX                   0xA9
#define LP855X_LED_FAULT_TH_CTRL            0xAE
#define LP855X_PWM_DIRECT_MODE_CURRENT_CTRL 0xA0
#define LP855X_LED_CURRENT_CTRL             0xA1
#define LP855X_BL_ON_OFF                    0xA2
#define LP8556_EPROM_START		            0xA0
#define LP8556_EPROM_END		            0xAF

/* LP8555/7 Registers */
#define LP8557_BL_CMD			0x00
#define LP8557_BL_MASK			0x01
#define LP8557_BL_ON			0x01
#define LP8557_BL_OFF			0x00
#define LP8557_BRIGHTNESS_CTRL		0x04
#define LP8557_CONFIG			0x10
#define LP8555_EPROM_START		0x10
#define LP8555_EPROM_END		0x7A
#define LP8557_EPROM_START		0x10
#define LP8557_EPROM_END		0x1E

#define DEFAULT_BL_NAME		"lcd-backlight"
#define MAX_BRIGHTNESS		255
#define LED_ENABLE_RESET    0x3F
#define LP855X_BL_ON_OFF_MASK 0x08
#define LP855X_BL_ALL_ON_MASK 0xF
#define LP855X_BL_ALL_OFF_MASK 0x0

/*
 * struct lp855x_device_config
 * @pre_init_device: init device function call before updating the brightness
 * @reg_brightness: register address for brigthenss control
 * @reg_devicectrl: register address for device control
 * @post_init_device: late init device function call
 */
struct lp855x_device_config {
	int (*pre_init_device)(struct lp855x*);
	u8 reg_brightness;
	u8 reg_devicectrl;
	u8 reg_led_enable;
	u8 reg_vboostrange_ctrl;
	u8 reg_pwm_direct_mode_ctrl;
	u8 reg_vboost_max;
	u8 reg_led_fault_th;
	u8 reg_pwm_direct_current_ctrl;
	u8 reg_led_current_ctrl;
	u8 reg_led_b_on_off;
	int (*post_init_device)(struct lp855x*);
};

struct lp855x *g_lp855x_data, * prev_lp855x_data;

int probe_count = 0;

int lp855x_update_led_param(struct lp855x* data, u32 brightness, u32 pattern);

static int lp855x_write_byte(struct lp855x* lp, u8 reg, u8 data)
{
	int ret = 0;
	LED_DBG("reg[0x%x] data[0x%x] \n", reg, data);
	ret = i2c_smbus_write_byte_data(lp->client, reg, data);
	if (ret < 0) {
		LED_ERR("failed to write reg[0x%x] data[0x%x] ret[%d] \n", reg, data, ret);
	}
	return ret;
}

static int lp855x_update_bit(struct lp855x* lp, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;
	LED_DBG("reg[0x%x] mask[0x%x] data[0x%x] \n", reg, mask, data);

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		LED_ERR("failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return lp855x_write_byte(lp, reg, tmp);
}

static bool lp855x_is_valid_rom_area(struct lp855x* lp, u8 addr)
{
	u8 start, end;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
		start = LP855X_EEPROM_START;
		end = LP855X_EEPROM_END;
		break;
	case LP8556:
		start = LP8556_EPROM_START;
		end = LP8556_EPROM_END;
		break;
	case LP8555:
		start = LP8555_EPROM_START;
		end = LP8555_EPROM_END;
		break;
	case LP8557:
		start = LP8557_EPROM_START;
		end = LP8557_EPROM_END;
		break;
	default:
		return false;
	}

	return addr >= start && addr <= end;
}

static int lp8557_bl_off(struct lp855x* lp)
{
	/* BL_ON = 0 before updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
		LP8557_BL_OFF);
}

static int lp8557_bl_on(struct lp855x* lp)
{
	/* BL_ON = 1 after updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
		LP8557_BL_ON);
}

void lp8558_en_control(struct lp855x* lp, int enable)
{
	if(lp->pdata->driver_en_gpio > 0)
	{
		gpio_direction_output(lp->pdata->driver_en_gpio, enable);
		usleep_range(250, 260);
		LED_DBG("Gpio: [%d] set to [%d]", lp->pdata->driver_en_gpio, enable);
	}
}

static int lp855x_bl_update_brightness(struct lp855x* lp, u8 brightness_value)
{
	if (NULL != lp && lp->mode != 0xFF)
	{
		lp->bl->props.brightness = brightness_value;
		backlight_update_status(lp->bl);
		LED_DBG("LED updated brt_value: %d", brightness_value);
		return 0;
	}
	else
	{
		LED_ERR("bl not in proper state to turn on/off, brt_value: %d", brightness_value);
	}
	return -1;
}

#if 0
static int lp855x_bl_off(struct lp855x* lp)
{
	return lp855x_bl_update_brightness(lp, 0);
}
#endif

static int lp855x_bl_on(struct lp855x* lp)
{
	return lp855x_bl_update_brightness(lp, lp->brightness_data);
}



static struct lp855x_device_config lp855x_dev_cfg = {
	.reg_brightness = LP855X_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP855X_DEVICE_CTRL,
	.reg_led_enable = LP855X_LED_ENABLE_CTRL,
	.reg_vboostrange_ctrl = LP855X_VBOOST_RANGE,
	.reg_pwm_direct_mode_ctrl = LP855X_PWM_DIRECT_MODE_ENABLE_CTRL,
	.reg_vboost_max = LP855X_VBOOST_MAX,
	.reg_led_fault_th = LP855X_LED_FAULT_TH_CTRL,
	.reg_pwm_direct_current_ctrl = LP855X_PWM_DIRECT_MODE_CURRENT_CTRL,
	.reg_led_current_ctrl = LP855X_LED_CURRENT_CTRL,
	.reg_led_b_on_off = LP855X_BL_ON_OFF,
};

static struct lp855x_device_config lp8557_dev_cfg = {
	.reg_brightness = LP8557_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP8557_CONFIG,
	.pre_init_device = lp8557_bl_off,
	.post_init_device = lp8557_bl_on,
};

/*
 * Device specific configuration flow
 *
 *    a) pre_init_device(optional)
 *    b) update the brightness register
 *    c) update device control register
 *    d) update ROM area(optional)
 *    e) post_init_device(optional)
 *
 */
static int lp855x_configure(struct lp855x* lp)
{
	u8 val, addr;
	int i, ret;
	struct lp855x_platform_data* pd = lp->pdata;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
	case LP8556:
		lp->cfg = &lp855x_dev_cfg;
		break;
	case LP8555:
	case LP8557:
		lp->cfg = &lp8557_dev_cfg;
		break;
	default:
		return -EINVAL;
	}

	if (lp->cfg->pre_init_device) {
		ret = lp->cfg->pre_init_device(lp);
		if (ret) {
			LED_ERR("pre init device err: %d\n", ret);
			goto err;
		}
	}

	val = pd->initial_brightness;
	ret = lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
	if (ret < 0) {
		LED_ERR("failed to write initial_brightness val : %d", val);
		goto err;
	}

	val = pd->device_control;
	ret = lp855x_write_byte(lp, lp->cfg->reg_devicectrl, val);
	if (ret < 0) {
		LED_ERR("failed to write device_control val : %d", val);
		goto err;
	}

	if (pd->size_program > 0) {
		for (i = 0; i < pd->size_program; i++) {
			addr = pd->rom_data[i].addr;
			val = pd->rom_data[i].val;
			if (!lp855x_is_valid_rom_area(lp, addr))
				continue;

			ret = lp855x_write_byte(lp, addr, val);
			if (ret < 0) {
				LED_ERR("failed to write rom_data val : %d", val);
				goto err;
			}
		}
	}

	val = 0xA0; // PWM Bit Disable
	ret = lp855x_write_byte(lp, lp->cfg->reg_pwm_direct_mode_ctrl, val);
	if (ret < 0) {
		LED_ERR("failed to write PWM val : %d", val);
		goto err;
	}
	
	val = 0x0;
	ret = lp855x_update_bit(lp, lp->cfg->reg_led_b_on_off, LP855X_BL_ON_OFF_MASK, val);
	if (ret < 0) {
		LED_ERR("failed to write BL_ON_OFF val : %d", val);
		goto err;
	}

	if (lp->cfg->post_init_device) {
		ret = lp->cfg->post_init_device(lp);
		if (ret) {
			LED_ERR("post init device err: %d\n", ret);
			goto err;
		}
	}
	LED_INFO("cfg_name[%s] id[%d] configure success\n", lp->chipname, lp->chip_id);

	return 0;

err:
	return ret;
}

static void lp855x_pwm_ctrl(struct lp855x* lp, int br, int max_br)
{
	unsigned int period = lp->pdata->period_ns;
	unsigned int duty = br * period / max_br;
	struct pwm_device* pwm;

	/* request pwm device with the consumer name */
	if (!lp->pwm) {
		pwm = devm_pwm_get(lp->dev, lp->chipname);
		if (IS_ERR(pwm))
		{
			LED_INFO("pwm ctrl error \n");
			return;
		}

		lp->pwm = pwm;

		/*
		 * FIXME: pwm_apply_args() should be removed when switching to
		 * the atomic PWM API.
		 */
		pwm_apply_args(pwm);
	}

	pwm_config(lp->pwm, duty, period);
	if (duty)
		pwm_enable(lp->pwm);
	else
		pwm_disable(lp->pwm);
	LED_INFO("pwm mode en: %d \n", duty);
}

static int lp855x_bl_update_status(struct backlight_device* bl)
{
	struct lp855x* lp = bl_get_data(bl);
	int vboostrange_data, pwm_direct_mode_ctrl_data, vboost_max_data;
	int led_fault_th_data, pwm_direct_mode_current_data, led_current_data;
	int device_ctrl_data, bl_on_off_flag;

	int brightness = bl->props.brightness;
	int pattern = bl->props.pattern;
	lp->force_trigger = bl->props.strobe;

	if(lp->brightness_data == brightness)
	{
		return 0;
	}

	lp855x_update_led_param(lp, brightness, pattern);

	pattern = lp->pattern_data;
	vboostrange_data = lp->vboostrange_data;
	pwm_direct_mode_ctrl_data = lp->pwm_direct_mode_ctrl_data;
	vboost_max_data = lp->vboost_max_data;
	led_fault_th_data = lp->led_fault_th_data;
	pwm_direct_mode_current_data = lp->pwm_direct_mode_current_data;
	led_current_data = lp->led_current_data;
	device_ctrl_data = lp->device_ctrl_data;
	bl_on_off_flag = lp->bl_on_off_flag;

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
	{
		LED_DBG("[mode:%d][brt:0x%x][pat:0x%x][state:%d] Looks like suspended\n",
			lp->mode, brightness, pattern, bl->props.state);
		brightness = 0;
		pattern = LED_ENABLE_RESET;
	}
	LED_DBG("[bl-name:%s][mode:%d][state:%d] prepared to set [pat: 0x%x->0x%x, br: 0x%x(%d)]\n",
		lp->pdata->name, lp->mode, bl->props.state, bl->props.pattern, pattern, brightness, brightness);

	if (lp->mode == PWM_BASED)
		lp855x_pwm_ctrl(lp, brightness, bl->props.max_brightness);
	else if (lp->mode == REGISTER_BASED)
	{
		lp8558_en_control(lp, 1);
		lp855x_write_byte(lp, 0x9e, 0x1);
		lp855x_write_byte(lp, 0x9e, 0x1);
		lp855x_write_byte(lp, 0x9e, 0x1);
		lp855x_write_byte(lp, 0x9e, 0x1);
		lp855x_write_byte(lp, lp->cfg->reg_brightness, (u8)brightness);
		lp855x_write_byte(lp, lp->cfg->reg_devicectrl, (u8)device_ctrl_data);
		lp855x_write_byte(lp, lp->cfg->reg_led_enable, (u8)pattern);
		lp855x_write_byte(lp, lp->cfg->reg_vboostrange_ctrl, (u8)vboostrange_data);
		lp855x_write_byte(lp, lp->cfg->reg_pwm_direct_mode_ctrl, (u8)pwm_direct_mode_ctrl_data);
		lp855x_write_byte(lp, lp->cfg->reg_vboost_max, (u8)vboost_max_data);
		lp855x_write_byte(lp, lp->cfg->reg_led_fault_th, (u8)led_fault_th_data);
		lp855x_write_byte(lp, lp->cfg->reg_pwm_direct_current_ctrl, (u8)pwm_direct_mode_current_data);
		lp855x_write_byte(lp, lp->cfg->reg_led_current_ctrl, (u8)led_current_data);
		lp855x_write_byte(lp, lp->cfg->reg_led_b_on_off, bl_on_off_flag);
		if(0 == brightness)
			lp8558_en_control(lp, 0);
	}

	return 0;
}

/**
* et6312b_update_led_param - Animate ir_led by the config written
*		from upper layer
* returns 1 - register configuration updated.
* 		  0 - Register values to be updated through I2C.
**/
int lp855x_update_led_param(struct lp855x* data, u32 brightness, u32 pattern)
{
	u32 currentData;

	if (data == NULL)
		return -1;
	data->set_param = brightness<<8|pattern;
	data->led_param.srg_pattern = pattern & 0xFF;
	data->led_param.brow_pattern = pattern & 0xFF;
	data->led_param.reserved = 0;
	data->led_param.extpwm_off = 0;
	data->led_param.srg_brightness = brightness;
	data->led_param.brow_brightness = brightness;
	data->brightness_data = brightness;
	
	if (0 == pattern)
	{
		data->brightness_data = 0;
		data->device_ctrl_data = 0x84;
		data->bl_on_off_flag = 0x20;
	}
	else
	{
		data->device_ctrl_data = 0x85;
		data->bl_on_off_flag = 0x28;
	}

	if (data->force_trigger == 0)
	{
		data->vboostrange_data = 0x01;
		data->pwm_direct_mode_ctrl_data = 0xA0;
		data->vboost_max_data = 0x80;
		data->led_fault_th_data = 0x0A;
		currentData = min((data->brightness_data * 41),4095); // Approximate value
		data->pwm_direct_mode_current_data = (0x0FF & currentData);
		data->led_current_data = (0x60 | (currentData >> 8));
		//data->device_ctrl_data = 0x85;
	}
	else if (data->force_trigger == 1)
	{
		data->vboostrange_data = 0x21;
		data->pwm_direct_mode_ctrl_data = ((0 == data->led_param.brow_pattern) && (0 == data->led_param.srg_pattern)) ? 0xA0 : 0x14;
		data->vboost_max_data = 0xC0;
		data->led_fault_th_data = 0x0E;
		data->pwm_direct_mode_current_data = 0xFF;
		data->led_current_data = 0x6F;
		//data->device_ctrl_data = 0x85;
	}
	data->pattern_data = 0xF;
	if(pattern < LP8557_PATTERN_MODES)
		data->pattern_data = data->pdata->led_pattern_reg[pattern];

	LED_DBG("set_param_update = 0x%x, brow[pat: 0x%x, br: 0x%x(%d)], srg[pat: 0x%x, br: 0x%x(%d)], extpwm_off: %d final[pat: 0x%x -> 0x%x, br: 0x%x(%d)], vboost_range: 0x%x, pwm_direct_mode_ctrl: 0x%x, vboost_max: 0x%x, led_fault_th: 0x%x, pwm_direct_mode_current: 0x%x, led_current_data: 0x%x \n", \
		data->set_param, \
		data->led_param.brow_pattern, data->led_param.brow_brightness, data->led_param.brow_brightness, \
		data->led_param.srg_pattern, data->led_param.srg_brightness, data->led_param.srg_brightness, \
		data->led_param.extpwm_off, pattern, data->pattern_data, data->brightness_data, data->brightness_data, \
		data->vboostrange_data, data->pwm_direct_mode_ctrl_data, data->vboost_max_data, data->led_fault_th_data, \
		data->pwm_direct_mode_current_data, data->led_current_data);

	return 0;
}


/**
* lp855x_set_param_store - returns smart glow animation config on sysfs read.
**/

static ssize_t lp855x_set_param_right_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len)
{
	u32 brightness = 0;
	u32 pattern = 0;
	u32 force_trigger = 0;
	if (dev->parent == &g_lp855x_data->client->dev)
	{
		struct lp855x* lp = g_lp855x_data;
		LED_ERR("lp %p, chip_name[%s] chip_id[%d] mode[%d]\n", lp, lp->chipname, lp->chip_id, lp->mode);
		sscanf(buf, "%u %u %d", &brightness, &pattern, &force_trigger);
		LED_ERR("[ET_IR] Values Received by right are brightness: %u %x pattern %u %x force_trigger %d", 
			brightness, brightness, pattern, pattern, force_trigger);
		lp->force_trigger = force_trigger;

		mutex_lock(&g_lp855x_data->mutex);
		lp855x_update_led_param(lp, brightness, pattern);
		lp855x_bl_on(lp);
		mutex_unlock(&g_lp855x_data->mutex);
	}
	return len;
}

/**
* lp855x_set_param_show - returns smart glow animation config on sysfs read.
**/
static ssize_t lp855x_set_param_right_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	if (dev->parent == &g_lp855x_data->client->dev)
	{
		return sprintf(buf, "0x%X\n", g_lp855x_data->set_param);
	}
	return sprintf(buf, "FAIL");
}


/**
* lp855x_reg_store - get the animation configuration from user space.
**/
static ssize_t lp855x_right_reg_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len)
{
	LED_ERR(" lp855x_reg_store buf %s parent: dev:[0x%8x:%p] dev->parent: [0x%8x:%p]\n",
		buf, &g_lp855x_data->client->dev, &g_lp855x_data->client->dev, dev->parent, dev->parent);
	return len;
}

/**
* lp855x_reg_show - returns smart glow animation config on sysfs read.
**/
static ssize_t lp855x_right_reg_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret, i2c_fail = 0;
	u8 pseudo_reg[32] = { 0, };
	int read_addr[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x16, 0x9E, 0xA5, 0xA9, 0xAE, 0xA0, 0xA1, 0xA2 };
	char sPrintStr[256] = { 0 };
	int wCount = 0;
	int read_len = sizeof(read_addr) / sizeof(int);
	int idx;
	memset(sPrintStr, 0, sizeof(sPrintStr));
	if (dev->parent == &g_lp855x_data->client->dev)
	{
		for (idx = 0; idx < read_len; idx++)
		{
			if ((ret = i2c_smbus_read_i2c_block_data(g_lp855x_data->client, read_addr[idx], 1, &pseudo_reg[idx])) < 0)
			{
				LED_ERR("failure on i2c read block data at [%d][0x%X] \n", idx, read_addr[idx]);
				i2c_fail = 1;
				break;
			}
			wCount += sprintf(&sPrintStr[wCount], "reg[0x%X] = 0x%X\n", read_addr[idx], pseudo_reg[idx]);
		}
		LED_ERR("lp855x_reg_show: dev:[0x%8x:%p] dev->parent: [0x%8x:%p] i2c:[%d] str:[%s]\n",
			&g_lp855x_data->client->dev, &g_lp855x_data->client->dev, dev->parent, dev->parent, i2c_fail, sPrintStr);
		if (0 == i2c_fail)
		{
			return sprintf(buf, "SUCCESS\n %s", sPrintStr);
		}
	}
	return sprintf(buf, "FAIL");
}


static const struct backlight_ops lp855x_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lp855x_bl_update_status,
};

static int lp855x_backlight_register(struct lp855x* lp)
{
	struct backlight_device* bl;
	struct backlight_properties props;
	struct lp855x_platform_data* pdata = lp->pdata;
	
	LED_DBG("BL_REGISTER pdata->name[%s] id[%d] register sucess \n", pdata->name, lp->chip_id);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS;
	props.brightness = 0;

	
	bl = devm_backlight_device_register(lp->dev, pdata->name, lp->dev, lp, &lp855x_bl_ops, &props);
	
	if (IS_ERR(bl))
	{
		LED_ERR("register error \n");
		return PTR_ERR(bl);
	}

	lp->bl = bl;
	LED_DBG("register_name[%s] id[%d] register sucess \n", lp->chipname, lp->chip_id);
	return 0;
}

/**
* ir_led_set_brightness - Get the brightness value from upper
* 		layer through sysfs and schedule the work
**/
void ir_led_set_brightness(struct led_classdev* cdev,
	enum led_brightness brightness)
{
	LED_ERR("No function Yet!.");
	return;
}



static ssize_t lp855x_get_chip_id_right(struct device* dev,
	struct device_attribute* attr, char* buf)
{
	struct lp855x* lp = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", lp->chipname);
}



static ssize_t lp855x_get_bl_ctl_mode_right(struct device* dev,
	struct device_attribute* attr, char* buf)
{
	struct lp855x* lp = dev_get_drvdata(dev);
	char* strmode = NULL;

	if (lp->mode == PWM_BASED)
		strmode = "pwm based";
	else if (lp->mode == REGISTER_BASED)
		strmode = "register based";
	else
		strmode = "fail";
	return scnprintf(buf, PAGE_SIZE, "%s\n", strmode);
}



static DEVICE_ATTR(chip_id_right, S_IRUGO, lp855x_get_chip_id_right, NULL);
static DEVICE_ATTR(bl_ctl_mode_right, S_IRUGO, lp855x_get_bl_ctl_mode_right, NULL);
static DEVICE_ATTR(set_param_right, S_IRUGO | S_IWUSR | S_IWGRP, lp855x_set_param_right_show, lp855x_set_param_right_store);
static DEVICE_ATTR(reg_right, S_IRUGO | S_IWUSR | S_IWGRP, lp855x_right_reg_show, lp855x_right_reg_store);


static struct attribute* lp855x_attributes_right[] = {
	&dev_attr_chip_id_right.attr,
	&dev_attr_bl_ctl_mode_right.attr,
	&dev_attr_set_param_right.attr,
	&dev_attr_reg_right.attr,
	NULL,
};


static const struct attribute_group lp855x_attr_group_right = {
	.attrs = lp855x_attributes_right,
};



//EXPORT_SYMBOL(lp855x_led_mode_ctrl_et_right);
EXPORT_SYMBOL(lp855x_led_mode_ctrl);
int64_t lp855x_led_mode_ctrl(int ledBrightness, int ledPattern, int force_trigger)
{
	int ret = 0;
	LED_DBG("ET_RIGHT_IR_LED_START : E ledBrightness %d [0x%x] ledPattern %d [0x%x]", ledBrightness, ledBrightness, ledPattern, ledPattern);

	if (NULL == g_lp855x_data)
		return ret;
	g_lp855x_data->force_trigger = force_trigger;
	g_lp855x_data->slave_index = 1;
	mutex_lock(&g_lp855x_data->mutex);

	if ((ret = lp855x_update_led_param(g_lp855x_data, ledBrightness, ledPattern)) == 0)
	{
		if ((ret = lp855x_bl_on(g_lp855x_data)) >= 0)
		{
			LED_DBG("LED Updated successfully...");
		}
	}

	mutex_unlock(&g_lp855x_data->mutex);

	LED_DBG("ET_RIGHT_IR_LED_START : X [0x%x] ret [%d]", g_lp855x_data->set_param, ret);
	return ret;
}

int32_t lp855x_led_set_torch(int slaveIndex, int currentValue)
{
	LED_DBG("ET_IR_LED_START_%d: E param [0x%x]", slaveIndex, currentValue);
	return 0;
}


/**
* lp855x_register_ledclass - Registers LED class for smart glow.
**/
#if 0
static int lp855x_register_ledclass(struct lp855x* data)
{
	
	struct device* dev = &data->client->dev;
	int ret = 0;

	data->class_dev.brightness_set = ir_led_set_brightness;
	data->class_dev.brightness = 0;
	data->class_dev.max_brightness = 0xFF;
	data->class_dev.flags = 0;
	data->class_dev.name = "lp8558_et_right";

	/* Register LED Class device interface */
	if ((ret = led_classdev_register(dev, &data->class_dev)) < 0) {
		LED_ERR("can not register led : lp8558_et_right");
		return ret;
	}

	/* Create additional class attribute to expose blink
		related functionalities */
	if ((ret = sysfs_create_group(&data->class_dev.dev->kobj, &lp855x_attr_group_right)) < 0) {
		LED_ERR("can not register sysfs attribute : lp8558_et_right");
		goto ERR_SYSFS;
	}
	LED_ERR("register ledclass success [%s](X)", data->class_dev.name);

	return 0;

ERR_SYSFS:
	led_classdev_unregister(&data->class_dev);

	return ret;
}
#endif


#ifdef CONFIG_OF
static int lp855x_parse_dt(struct lp855x* lp)
{
	struct device* dev = lp->dev;
	struct device_node* node = dev->of_node;
	struct lp855x_platform_data* pdata;
	int rom_length, idx, count;
	int ret, temp;
	u8 val_u8;

	if (!node) {
		LED_ERR("no platform data\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		LED_ERR("failed to allocate driver data\n");
		return -ENOMEM;
	}

	/*bl-name*/
	ret = of_property_read_string(node, "bl-name", &pdata->name);
	if (ret < 0) {
		LED_ERR("Error in getting bl-name.\n");
	}
	else {
		LED_DBG("Success in getting bl-name (%s)\n", pdata->name);
	}

	/*dev-ctrl*/
	ret = of_property_read_u32(node, "dev-ctrl", &temp);
	if (ret < 0) {
		LED_ERR("Error in getting dev-ctrl.\n");
	}
	else {
		pdata->device_control = temp;
		LED_DBG("Success in getting dev-ctrl d_val[0x%x] val[0x%x]\n", temp, pdata->device_control);
	}

	/*init-brt*/
	ret = of_property_read_u32(node, "init-brt", &temp);
	if (ret < 0) {
		LED_DBG("Error in getting init-brt.\n");
	}
	else {
		//0x10 will make LED glow from begining only
		pdata->initial_brightness = temp;
		LED_DBG("Success in getting init-brt d_val[0x%x] val[0x%x]\n", temp, pdata->initial_brightness);
	}

	/*pwm-period*/
	ret = of_property_read_u32(node, "pwm-period", &temp);
	if (ret < 0) {
		LED_DBG("Error in getting pwm-period.\n");
	}
	else {
		pdata->period_ns = 0;
		LED_DBG("Success in getting pwm-period d_val[%d] overriding val[%d]\n", temp, pdata->period_ns);
	}

	for( idx = 0 ; idx < LP8557_PATTERN_MODES ; idx++ )
	{
		pdata->led_pattern_reg[idx] = 0xF;
	}
	count = of_property_count_u32_elems(node, "led-pattern-reg");

	for( idx = 0 ; idx < count && idx < LP8557_PATTERN_MODES ; idx++ )
	{
		ret = of_property_read_u32_index(node,"led-pattern-reg", idx, &pdata->led_pattern_reg[idx]);
		if (ret < 0) {
			LED_DBG("Error in getting led-pattern-reg[%d]\n",idx);
		}
		else{
			LED_DBG("led-pattern-reg[%d] = [0x%x]\n", idx, pdata->led_pattern_reg[idx]);
		}
	}

	temp = of_get_named_gpio(node, "lp,driver-en-gpio", 0);
	pdata->driver_en_gpio = 0;
	if (temp > 0) {	
		if (!gpio_is_valid(temp)) {
			LED_ERR("enable gpio not specified [%d]\n", temp);
		}
		else 
		{
			ret = gpio_request(temp, "lp_driver_en_gpio");
			if (ret) {
				LED_ERR("Enable gpio request failed, gpio: %d, ret:%d\n", temp, ret);
			}
			else
			{
				pdata->driver_en_gpio = temp;
				LED_DBG("Success in getting lp,driver-en-gpio[%d]\n",
					pdata->driver_en_gpio);
			}
		}	
	}
	else
	{
		LED_DBG("Error in getting lp,driver-en-gpio.\n");
	}

	/* Fill ROM platform data if defined */
	rom_length = of_get_child_count(node);
	if (rom_length > 0) {
		struct lp855x_rom_data* rom;
		struct device_node* child;
		int i = 0;

		rom = devm_kcalloc(dev, rom_length, sizeof(*rom), GFP_KERNEL);
		if (!rom) {
			LED_ERR("failed to allocate driver data\n");
			return -ENOMEM;
		}

		for_each_child_of_node(node, child) {
			ret = of_property_read_u8(child, "rom-addr", &val_u8);
			if (ret < 0) {
				LED_DBG("[%d] Error in getting rom-addr.\n", i);
			}
			else {
				if (val_u8 == 0x0)
				{
					rom[i].addr = 0x14;
				}
				else
				{
					rom[i].addr = val_u8;
				}


				LED_DBG("[%d] Success in getting rom-addr d_val[0x%x] val[0x%x]\n", i, val_u8, rom[i].addr);
			}
			ret = of_property_read_u8(child, "rom-val", &val_u8);
			if (ret < 0) {
				LED_DBG("[%d] Error in getting rom-val.\n", i);
			}
			else {
				if (val_u8 == 0x0)
				{
					rom[i].val = 0xcf;
				}
				else
				{
					rom[i].val = val_u8;
				}
				LED_DBG("[%d] Success in getting rom-val d_val[0x%x] val[0x%x]\n", i, val_u8, rom[i].val);
			}
			i++;
		}

		pdata->size_program = rom_length;
		pdata->rom_data = &rom[0];
	}
	else
	{
		LED_DBG("Error in getting ROM data or not defined!!!\n");
	}

	lp->pdata = pdata;
	LED_DBG("dt parsing success\n");
	LED_INFO("bl-name[%s] dev-ctrl[0x%x] init-brt[0x%x] gpio[%d] \n",
		lp->pdata->name, lp->pdata->device_control, lp->pdata->initial_brightness, lp->pdata->driver_en_gpio);
	return 0;
}
#else
static int lp855x_parse_dt(struct lp855x* lp)
{
	LED_ERR("dt parsing disabled\n");
	return -EINVAL;
}
#endif

static int lp855x_probe(struct i2c_client* cl, const struct i2c_device_id* id)
{
	struct lp855x* lp;
	struct lp855x state_save;
	int ret;
	LED_ERR("%s : probe enter lp8558 (E)", cl->name);


	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp855x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = cl;
	lp->dev = &cl->dev;
	lp->chipname = id->name;
	lp->chip_id = id->driver_data;
	lp->pdata = dev_get_platdata(&cl->dev);

	if (!lp->pdata) {
		ret = lp855x_parse_dt(lp);
		if (ret < 0)
			return ret;
	}

	//lp8558_en_control(lp, 1);

	if (lp->pdata->period_ns > 0)
		lp->mode = PWM_BASED;
	else
		lp->mode = REGISTER_BASED;

	lp->supply = devm_regulator_get(lp->dev, "power");
	if (IS_ERR(lp->supply)) {
		if (PTR_ERR(lp->supply) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		lp->supply = NULL;
	}

	lp->enable = devm_regulator_get_optional(lp->dev, "enable");
	if (IS_ERR(lp->enable)) {
		ret = PTR_ERR(lp->enable);
		if (ret == -ENODEV) {
			lp->enable = NULL;
		}
		else {
			if (ret != -EPROBE_DEFER)
				LED_ERR("error getting enable regulator: %d\n",
					ret);
			return ret;
		}
	}

	if (lp->supply) {
		ret = regulator_enable(lp->supply);
		if (ret < 0) {
			LED_ERR("failed to enable supply: %d\n", ret);
			return ret;
		}
	}

	if (lp->enable) {
		ret = regulator_enable(lp->enable);
		if (ret < 0) {
			LED_ERR("failed to enable vddio: %d\n", ret);
			goto disable_supply;
		}

		/*
		 * LP8555 datasheet says t_RESPONSE (time between VDDIO and
		 * I2C) is 1ms.
		 */
		usleep_range(1000, 2000);
	}

	
	g_lp855x_data = lp;

	state_save = *lp;
	prev_lp855x_data = &state_save;

	i2c_set_clientdata(cl, lp);

	ret = lp855x_configure(lp);
	if (ret) {
		LED_ERR("device config err: %d", ret);
		//goto disable_vddio;
		ret = 0;
	}

	ret = lp855x_backlight_register(lp);
	if (ret) {
		LED_ERR("failed to register backlight. err: %d\n", ret);
		goto disable_vddio;
	}
	mutex_init(&lp->mutex);

	//backlight_update_status(lp->bl);
	
	LED_ERR("%s : probe success ", cl->name);

	return 0;

disable_vddio:
	if (lp->enable)
		regulator_disable(lp->enable);
disable_supply:
	if (lp->supply)
		regulator_disable(lp->supply);
	mutex_destroy(&lp->mutex);

	return ret;
}

static int lp855x_remove(struct i2c_client* cl)
{
	struct lp855x* lp = i2c_get_clientdata(cl);

	lp->bl->props.brightness = 0;
	backlight_update_status(lp->bl);
	if (lp->enable)
		regulator_disable(lp->enable);
	if (lp->supply)
		regulator_disable(lp->supply);
	sysfs_remove_group(&lp->dev->kobj, &lp855x_attr_group_right);
	g_lp855x_data = NULL;
	prev_lp855x_data = NULL;

	led_classdev_unregister(&lp->class_dev);
	mutex_destroy(&lp->mutex);

	return 0;
}

static const struct of_device_id lp855x_dt_ids[] = {
	{.compatible = "ti,lp8550", },
	{.compatible = "ti,lp8551", },
	{.compatible = "ti,lp8552", },
	{.compatible = "ti,lp8553", },
	{.compatible = "ti,lp8555", },
	{.compatible = "ti,lp8556", },
	{.compatible = "ti,lp8557", },
	{ }
};
MODULE_DEVICE_TABLE(of, lp855x_dt_ids);

static const struct i2c_device_id lp855x_ids[] = {
	{"lp8550", LP8550},
	{"lp8551", LP8551},
	{"lp8552", LP8552},
	{"lp8553", LP8553},
	{"lp8555", LP8555},
	{"lp8556", LP8556},
	{"lp8557", LP8557},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static struct i2c_driver lp855x_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "lp855x",
		   .of_match_table = of_match_ptr(lp855x_dt_ids),
		   },
	.probe = lp855x_probe,
	.remove = lp855x_remove,
	.id_table = lp855x_ids,
};

module_i2c_driver(lp855x_driver);

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
