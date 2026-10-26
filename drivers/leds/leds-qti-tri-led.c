// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/qpnp/qti-pwm.h>
#include <linux/leds-qti-tri-led.h>
#include <linux/backlight.h>
#ifdef CONFIG_SEC_SVCLED
#include <linux/sec_class.h>
#endif

#define TRILED_REG_TYPE			0x04
#define TRILED_REG_SUBTYPE		0x05
#define TRILED_REG_EN_CTL		0x46

/* TRILED_REG_EN_CTL */
#define TRILED_EN_CTL_MASK		GENMASK(7, 5)
#define TRILED_EN_CTL_MAX_BIT		7

#define TRILED_TYPE			0x19
#define TRILED_SUBTYPE_LED3H0L12	0x02
#define TRILED_SUBTYPE_LED2H0L12	0x03
#define TRILED_SUBTYPE_LED1H2L12	0x04

#define TRILED_NUM_MAX			3

#define PWM_PERIOD_DEFAULT_NS		1000000

#ifdef CONFIG_SEC_SVCLED
typedef enum {
	SVCLED_MIN = 0,
	SVCLED_R = SVCLED_MIN,	/* RED   */
	SVCLED_G,		/* GREEN */
	SVCLED_B,		/* BLUE  */
	SVCLED_MAX,
} e_svcled_t;

enum {
	PATTERN_OFF = 0,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
	POWERING_OFF,
};
#endif

struct pwm_setting {
	u64	pre_period_ns;
	u64	period_ns;
	u64	duty_ns;
};

struct led_setting {
	u64			on_ms;
	u64			off_ms;
	enum led_brightness	brightness;
	bool			blink;
	bool			breath;
};

struct qpnp_led_dev {
	struct led_classdev	cdev;
	struct pwm_device	*pwm_dev;
	struct pwm_setting	pwm_setting;
	struct led_setting	led_setting;
	struct qpnp_tri_led_chip	*chip;
	struct mutex		lock;
	const char		*label;
	const char		*default_trigger;
	u8			id;
#ifdef CONFIG_SEC_SVCLED
	int			br_ratio_normal;	/* normal mode ratio   */
	int			br_ratio_lpm;		/* lowpower mode ratio */
	int			ratio;			/* final using ratio   */
	int			blink_count;
	void			(*ori_blink_timer_function)(struct timer_list *tl);
#endif
	bool			blinking;
	bool			breathing;
};

struct qpnp_tri_led_chip {
	struct device		*dev;
	struct regmap		*regmap;
	struct qpnp_led_dev	*leds;
	struct nvmem_device	*pbs_nvmem;
	struct mutex		bus_lock;
#ifdef CONFIG_SEC_SVCLED
	struct device		*led_dev;
	u8			lpm;	/* low power mode */
	int			step;	/* partition step */
	int			mode; /* led pattern */
	int			blink_count;
	bool			is_sec_svcled;
	struct mutex		function_lock;
	struct delayed_work	svcled_bootani_work;
#endif
	int			num_leds;
	u16			reg_base;
	u8			subtype;
	u8			bitmap;
	int			ref_count;
	struct backlight_device *bl;
	const char *name;
};

typedef enum {
	SVCLED_CLIENT_NONE  = 0,
	SVCLED_CLIENT_CAM   = 1 << 0,
	SVCLED_CLIENT_LIGHT = 1 << 1,
	SVCLED_CLIENT_MAX,
} e_svcled_client_t;

static u32 led_client_type = SVCLED_CLIENT_NONE;
struct mutex led_client_lock;

static int qpnp_tri_led_read(struct qpnp_tri_led_chip *chip, u16 addr, u8 *val)
{
	int rc;
	unsigned int tmp;

	mutex_lock(&chip->bus_lock);
	rc = regmap_read(chip->regmap, chip->reg_base + addr, &tmp);
	if (rc < 0)
		dev_err(chip->dev, "Read addr 0x%x failed, rc=%d\n", addr, rc);
	else
		*val = (u8)tmp;
	mutex_unlock(&chip->bus_lock);

	return rc;
}

static int qpnp_tri_led_masked_write(struct qpnp_tri_led_chip *chip,
				u16 addr, u8 mask, u8 val)
{
	int rc;

	mutex_lock(&chip->bus_lock);
	rc = regmap_update_bits(chip->regmap, chip->reg_base + addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "Update addr 0x%x to val 0x%x with mask 0x%x failed, rc=%d\n",
					addr, val, mask, rc);
	mutex_unlock(&chip->bus_lock);

	return rc;
}

static int __tri_led_config_pwm(struct qpnp_led_dev *led,
				struct pwm_setting *pwm)
{
	struct pwm_state pstate;
	enum pwm_output_type output_type;
	int rc;

	pwm_get_state(led->pwm_dev, &pstate);
	pstate.enabled = !!(pwm->duty_ns != 0);
	pstate.period = pwm->period_ns;
	pstate.duty_cycle = pwm->duty_ns;
	output_type = led->led_setting.breath ?
		PWM_OUTPUT_MODULATED : PWM_OUTPUT_FIXED;

	rc = qpnp_lpg_pwm_set_output_type(led->pwm_dev, output_type);
	if (rc < 0) {
		dev_err(led->chip->dev, "Set output_type for %s led failed, rc=%d\n",
			led->cdev.name, rc);
		return rc;
	}

	rc = pwm_apply_state(led->pwm_dev, &pstate);
	if (rc < 0)
		dev_err(led->chip->dev, "Apply PWM state for %s led failed, rc=%d\n",
					led->cdev.name, rc);

	return rc;
}

#define PBS_ENABLE	1
#define PBS_DISABLE	2
#define PBS_ARG		0x42
#define PBS_TRIG_CLR	0xE6
#define PBS_TRIG_SET	0xE5
static int __tri_led_set(struct qpnp_led_dev *led)
{
	int rc = 0;
	u8 val = 0, mask = 0, pbs_val;
	u8 prev_bitmap;

	rc = __tri_led_config_pwm(led, &led->pwm_setting);
	if (rc < 0) {
		dev_err(led->chip->dev, "Configure PWM for %s led failed, rc=%d\n",
					led->cdev.name, rc);
		return rc;
	}

	mask |= 1 << (TRILED_EN_CTL_MAX_BIT - led->id);

	if (led->pwm_setting.duty_ns == 0)
		val = 0;
	else
		val = mask;

	if (led->chip->subtype == TRILED_SUBTYPE_LED2H0L12 &&
		led->chip->pbs_nvmem) {
		/*
		 * Control BOB_CONFIG_EXT_CTRL2_FORCE_EN for HR_LED through
		 * PBS trigger. PBS trigger for enable happens if any one of
		 * LEDs are turned on. PBS trigger for disable happens only
		 * if both LEDs are turned off.
		 */

		prev_bitmap = led->chip->bitmap;
		if (val)
			led->chip->bitmap |= (1 << led->id);
		else
			led->chip->bitmap &= ~(1 << led->id);

		if (!(led->chip->bitmap & prev_bitmap)) {
			pbs_val = led->chip->bitmap ? PBS_ENABLE : PBS_DISABLE;
			rc = nvmem_device_write(led->chip->pbs_nvmem, PBS_ARG,
				1, &pbs_val);
			if (rc < 0) {
				dev_err(led->chip->dev, "Couldn't set PBS_ARG, rc=%d\n",
					rc);
				return rc;
			}

			pbs_val = 1;
			rc = nvmem_device_write(led->chip->pbs_nvmem,
				PBS_TRIG_CLR, 1, &pbs_val);
			if (rc < 0) {
				dev_err(led->chip->dev, "Couldn't set PBS_TRIG_CLR, rc=%d\n",
					rc);
				return rc;
			}

			pbs_val = 1;
			rc = nvmem_device_write(led->chip->pbs_nvmem,
				PBS_TRIG_SET, 1, &pbs_val);
			if (rc < 0) {
				dev_err(led->chip->dev, "Couldn't set PBS_TRIG_SET, rc=%d\n",
					rc);
				return rc;
			}
		}
	}

	rc = qpnp_tri_led_masked_write(led->chip, TRILED_REG_EN_CTL,
							mask, val);
	if (rc < 0)
		dev_err(led->chip->dev, "Update addr 0x%x failed, rc=%d\n",
					TRILED_REG_EN_CTL, rc);

	return rc;
}

static int qpnp_tri_led_set(struct qpnp_led_dev *led)
{
	u64 on_ms, off_ms, period_ns, duty_ns;
	enum led_brightness brightness = led->led_setting.brightness;
	int rc = 0;

	if (led->led_setting.blink) {
		on_ms = led->led_setting.on_ms;
		off_ms = led->led_setting.off_ms;

		duty_ns = on_ms * NSEC_PER_MSEC;
		period_ns = (on_ms + off_ms) * NSEC_PER_MSEC;

		if (period_ns < duty_ns && duty_ns != 0)
			period_ns = duty_ns + 1;
	} else {
		/* Use initial period if no blinking is required */
		period_ns = led->pwm_setting.pre_period_ns;

		if (brightness == LED_OFF)
			duty_ns = 0;

		duty_ns = period_ns * brightness;
		do_div(duty_ns, LED_FULL);

		if (period_ns < duty_ns && duty_ns != 0)
			period_ns = duty_ns + 1;
	}
	dev_dbg(led->chip->dev, "PWM settings for %s led: period = %lluns, duty = %lluns\n",
				led->cdev.name, period_ns, duty_ns);

	led->pwm_setting.duty_ns = duty_ns;
	led->pwm_setting.period_ns = period_ns;

	rc = __tri_led_set(led);
	if (rc < 0) {
		dev_err(led->chip->dev, "__tri_led_set %s failed, rc=%d\n",
				led->cdev.name, rc);
		return rc;
	}

	if (led->led_setting.blink) {
		led->cdev.brightness = LED_FULL;
		led->blinking = true;
		led->breathing = false;
	} else if (led->led_setting.breath) {
		led->cdev.brightness = LED_FULL;
		led->blinking = false;
		led->breathing = true;
	} else {
		led->cdev.brightness = led->led_setting.brightness;
		led->blinking = false;
		led->breathing = false;
	}

	return rc;
}

static int qpnp_tri_led_set_brightness(struct led_classdev *led_cdev,
		enum led_brightness brightness)
{
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);
	int rc = 0;
	mutex_lock(&led->lock);
	if (brightness > LED_FULL)
		brightness = LED_FULL;

	if (brightness == led->led_setting.brightness &&
			!led->blinking && !led->breathing) {
		mutex_unlock(&led->lock);
		return 0;
	}

	led->led_setting.brightness = brightness;
	if (!!brightness)
		led->led_setting.off_ms = 0;
	else
		led->led_setting.on_ms = 0;
	led->led_setting.blink = false;
	led->led_setting.breath = false;

	dev_dbg(led->chip->dev, "prepared to set brightness %d for %s\n", led->led_setting.brightness, led->label);

	rc = qpnp_tri_led_set(led);
	if (rc)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

	mutex_unlock(&led->lock);

	return rc;
}

static enum led_brightness qpnp_tri_led_get_brightness(
			struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int qpnp_tri_led_set_blink(struct led_classdev *led_cdev,
		unsigned long *on_ms, unsigned long *off_ms)
{
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);
	int rc = 0;

	mutex_lock(&led->lock);
	if (led->blinking && *on_ms == led->led_setting.on_ms &&
			*off_ms == led->led_setting.off_ms) {
		dev_dbg(led_cdev->dev, "Ignore, on/off setting is not changed: on %lums, off %lums\n",
						*on_ms, *off_ms);
		mutex_unlock(&led->lock);
		return 0;
	}

	if (*on_ms == 0) {
		led->led_setting.blink = false;
		led->led_setting.breath = false;
		led->led_setting.brightness = LED_OFF;
	} else if (*off_ms == 0) {
		led->led_setting.blink = false;
		led->led_setting.breath = false;
		led->led_setting.brightness = led->cdev.brightness;
	} else {
		led->led_setting.on_ms = *on_ms;
		led->led_setting.off_ms = *off_ms;
		led->led_setting.blink = true;
		led->led_setting.breath = false;
	}

	rc = qpnp_tri_led_set(led);
	if (rc)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

	mutex_unlock(&led->lock);
	return rc;
}

static ssize_t breath_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", led->led_setting.breath);
}

static ssize_t breath_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int rc;
	bool breath;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	rc = kstrtobool(buf, &breath);
	if (rc < 0)
		return rc;

	cancel_work_sync(&led_cdev->set_brightness_work);

	mutex_lock(&led->lock);
	if (led->breathing == breath)
		goto unlock;

	led->led_setting.blink = false;
	led->led_setting.breath = breath;
	led->led_setting.brightness = breath ? LED_FULL : LED_OFF;
	rc = qpnp_tri_led_set(led);
	if (rc < 0)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

unlock:
	mutex_unlock(&led->lock);
	return (rc < 0) ? rc : count;
}

static DEVICE_ATTR_RW(breath);
static const struct attribute *breath_attrs[] = {
	&dev_attr_breath.attr,
	NULL
};

#ifdef CONFIG_SEC_SVCLED
/**
 * sysfs:sec_class service_led attribute control support
 */
/*calc br 1 per 0.1mA max 12mA*/
#if 0
static u8 calc_led_br(struct qpnp_tri_led_chip *chip, u8 ratio)
{
	u8 br_curr, br;

	if (!ratio || !chip->step)
		return 0;

	if (ratio >= chip->step)
		br = chip->step;
	else
		br = ratio;

	br_curr = br * 0xff / chip->step;
	br_curr = (br_curr > 0) ? br_curr : 1;

	return br_curr;
}
#endif

static int svcled_set(struct device *dev, e_svcled_t color,
	u8 brightness, u32 on_ms, u32 off_ms)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	struct qpnp_led_dev *led;
	int rc;

	if (!chip)
		return -EINVAL;

	led = &chip->leds[color];
	led->led_setting.breath = false;
	if (!off_ms) {
		led->led_setting.blink = false;
		led->led_setting.on_ms = 0;
		led->led_setting.off_ms = 0;
	} else {
		led->led_setting.blink = true;
		led->led_setting.on_ms = on_ms;
		led->led_setting.off_ms = off_ms;
	}
//	led->led_setting.brightness = calc_led_br(chip, brightness);
	led->led_setting.brightness = brightness;

	rc = qpnp_tri_led_set(led);
	if (rc < 0) {
		pr_err("%s: color:%s br:0x%x mode:%s failed to set,%d\n",
			__func__, led->label, brightness, (!off_ms ? "always" : "blink"), rc);
		goto end;
	}
	pr_info("%s: color:%s br:0x%x mode:%s\n", __func__, led->label,
		brightness, (!off_ms ? "always" : "blink"));
end:
	return rc;
}

static void svcled_reset(struct qpnp_tri_led_chip *chip)
{
	int color;
	struct qpnp_led_dev *led;
	struct led_classdev *led_cdev;
	mutex_lock(&led_client_lock);
	for (color = SVCLED_R; color <= SVCLED_B; color++) {
		if(SVCLED_R == color && (led_client_type & SVCLED_CLIENT_CAM))
			continue;
		led = &chip->leds[color];
		led->blink_count = 0;
		led_cdev = &led->cdev;
		led_cdev->brightness = led_cdev->new_blink_brightness = led_cdev->blink_brightness = 0;

		del_timer_sync(&led_cdev->blink_timer);
		clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
		clear_bit(LED_BLINK_ONESHOT, &led_cdev->work_flags);
		clear_bit(LED_BLINK_ONESHOT_STOP, &led_cdev->work_flags);

		svcled_set(chip->dev, color, LED_OFF, 0, 0);
	}
	mutex_unlock(&led_client_lock);
	led_cdev->blink_timer.function = led->ori_blink_timer_function;
}

static void svcled_pattern_powering(struct device *dev, e_svcled_t color, u8 brightness)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	int offset = 0;
	int current_brightness = 0;
	int msleep_gap = 50;
	int pattern[] = {800, 200, 800, 200};
	int pattern_brightness[4] = {0, };
	int pattern_cnt = 4;
	unsigned int set_val = 0;
	int i, j;


	for (i = 0; i < pattern_cnt; i++)
		pattern[i] /= msleep_gap;

	pattern_brightness[0] = brightness;

	if (pattern_brightness[0])
		pattern_brightness[0] *= 100;

	pattern_brightness[1] = pattern_brightness[0];

	if (pattern_brightness[0])
		pattern_brightness[2] = pattern_brightness[0] / 4;

	pattern_brightness[3] = 0;

	for (i = 0; i < pattern_cnt ; i++) {
		offset = pattern_brightness[i] - current_brightness;
		if (offset)
			offset /= pattern[i];

		for (j = 0; j < pattern[i]; j++) {
			if (offset)
				current_brightness += offset;

			set_val = current_brightness / 100;
			svcled_set(dev, color, set_val, 0, 0);
			msleep(msleep_gap - 1);
		}
	}

	svcled_reset(chip);
}

static ssize_t led_r_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	unsigned int brightness;
	int rc;

	rc = kstrtouint(buf, 0, &brightness);
	if (rc) {
		dev_err(dev, "%s: failed to get brightness,%d\n", __func__, rc);
		return count;
	}

	svcled_set(dev, SVCLED_R, brightness, 0, 0);
	return count;
}

static ssize_t led_g_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	unsigned int brightness;
	int rc;

	rc = kstrtouint(buf, 0, &brightness);
	if (rc) {
		dev_err(dev, "%s: failed to get brightness,%d\n", __func__, rc);
		return count;
	}

	svcled_set(dev, SVCLED_G, brightness, 0, 0);
	return count;
}

static ssize_t led_b_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	unsigned int brightness;
	int rc;

	rc = kstrtouint(buf, 0, &brightness);
	if (rc) {
		dev_err(dev, "%s: failed to get brightness,%d\n", __func__, rc);
		return count;
	}

	svcled_set(dev, SVCLED_B, brightness, 0, 0);
	return count;
}

static ssize_t led_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "red : %d green : %d blue : %d\n",
			chip->leds[SVCLED_R].led_setting.brightness,
			chip->leds[SVCLED_G].led_setting.brightness,
			chip->leds[SVCLED_B].led_setting.brightness);
}

static ssize_t led_brightness_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	u8 r_br, g_br, b_br;
	int rc;

	rc = sscanf(buf, "%hhu %hhu %hhu", &r_br, &g_br, &b_br);
	if (!rc) {
		dev_err(dev, "%s: failed to get brightness,%d\n", __func__, rc);
		return count;
	}

	chip->lpm = 0;
	dev_info(dev, "%s: 0x%x,0x%x,0x%x\n", __func__, r_br, g_br, b_br);
	chip->leds[SVCLED_R].led_setting.brightness = r_br;
	chip->leds[SVCLED_G].led_setting.brightness = g_br;
	chip->leds[SVCLED_B].led_setting.brightness = b_br;

	return count;
}

static ssize_t led_lowpower_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", chip->lpm);
}

static ssize_t led_lowpower_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	u8 temp;
	int rc, color;

	rc = kstrtou8(buf, 0, &temp);
	if (rc) {
		dev_err(dev, "%s : failed get lowpower,%d\n", __func__, rc);
		return count;
	}

	chip->lpm = temp;
	dev_info(dev, "%s: lowpower:%d\n", __func__, chip->lpm);

	if (temp) {	/* low power mode */
		for (color = SVCLED_MIN; color < SVCLED_MAX; color++)
			chip->leds[color].ratio = chip->leds[color].br_ratio_lpm;
	} else {	/* normal power mode */
		for (color = SVCLED_MIN; color < SVCLED_MAX; color++)
			chip->leds[color].ratio = chip->leds[color].br_ratio_normal;
	}

	return count;
}

static ssize_t led_blink_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	u32 brightness, t_on, t_off;
	u8 led[SVCLED_MAX];
	int rc, color;

	rc = sscanf(buf, "0x%8x %5d %5d", &brightness, &t_on, &t_off);
	if (!rc) {
		dev_err(dev, "%s: failed to get led_blink,%d\n", __func__, rc);
		return count;
	}

	led[SVCLED_R] = ((brightness & 0xFF0000) >> 16);
	led[SVCLED_G] = ((brightness & 0x00FF00) >> 8);
	led[SVCLED_B] = ((brightness & 0x0000FF) >> 0);
	dev_info(dev, "%s: RGB:0x%02x,0x%02x,0x%02x, on:%d, off:%d\n",
			__func__, led[SVCLED_R], led[SVCLED_G], led[SVCLED_B], t_on, t_off);

	mutex_lock(&chip->function_lock);
	svcled_reset(chip);

	if (!t_off) {
		/*constant on*/
		for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
			if (led[color])
				svcled_set(dev, color, led[color], 0, 0);
		}
	} else {
		/*blinking*/
		for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
			if (led[color])
				svcled_set(dev, color, led[color], t_on, t_off);
		}
	}

	mutex_unlock(&chip->function_lock);
	dev_info(dev, "%s: blink done\n", __func__);

	return count;
}

static ssize_t led_blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	struct qpnp_led_dev *led;
	int color;
	u8 led_power_status = 0;

	for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
		led = &chip->leds[color];

		if (led->led_setting.brightness && !led->led_setting.blink) {/* led on */
			led_power_status = 1;
			break;
		}
	}

	return sprintf(buf, "%u\n", led_power_status);
}

static void svcled_wrapper_blink_timer_function(struct timer_list *t)
{
	struct led_classdev *led_cdev = from_timer(led_cdev, t, blink_timer);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	if (led->blink_count > 0) {
		led->ori_blink_timer_function(t);
		led->blink_count--;
	} else {
		qpnp_tri_led_set_brightness(led_cdev, LED_OFF);
	}
}

static ssize_t led_control_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	struct qpnp_led_dev *led;
	struct led_classdev *led_cdev;
	unsigned long t_on, t_off;
	int blink_count;
	u8 led_b[SVCLED_MAX];
	int rc, color;
	int brightness;

	rc = sscanf(buf, "0x%6x %d %d %d", &brightness, &t_on, &t_off, &blink_count);
	if (!rc) {
		dev_err(dev, "%s: failed to get led_blink,%d\n", __func__, rc);
		return count;
	}

	led_b[SVCLED_R] = ((brightness & 0xFF0000) >> 16);
	led_b[SVCLED_G] = ((brightness & 0x00FF00) >> 8);
	led_b[SVCLED_B] = ((brightness & 0x0000FF) >> 0);
	pr_info("%s: RGB:0x%02x,0x%02x,0x%02x, on:%d, off:%d, blink_count:%d\n",
			__func__, led_b[SVCLED_R], led_b[SVCLED_G], led_b[SVCLED_B], t_on, t_off, blink_count);

	mutex_lock(&chip->function_lock);
	svcled_reset(chip);

	if (blink_count > 50)
		blink_count = 50;


	if (!t_on || !t_off || !blink_count) {
		/*constant on*/
		for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
			if (led_b[color])
				svcled_set(dev, color, led_b[color], 0, 0);
		}

		pr_info("%s: always on 0x%06x\n", __func__, brightness);
	} else {
		/*blinking with count*/
		if (blink_count > 0) {

			for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
				led = &chip->leds[color];
				led_cdev = &led->cdev;
				led_cdev->blink_timer.function = svcled_wrapper_blink_timer_function;

				if (led_b[color]) {
					led_cdev->brightness = led_cdev->new_blink_brightness
							= led_cdev->blink_brightness = led_b[color];
					led->blink_count = blink_count * 2;
					led_blink_set(led_cdev, &t_on, &t_off);
				}
			}

			pr_info("%s: blink %06x\n", __func__, brightness);
		} else {
			for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
				led = &chip->leds[color];
				led_cdev = &led->cdev;
				if (led_b[color]) {
					led_cdev->brightness = led_cdev->new_blink_brightness
							= led_cdev->blink_brightness = led_b[color];
					svcled_set(dev, color, led_b[color], (u32)t_on, (u32)t_off);
				}
			}
		}
	}

	mutex_unlock(&chip->function_lock);
	pr_info("%s: done\n", __func__);

	return count;
}

static ssize_t led_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if 0
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	struct qpnp_led_dev *led;
	int color;
	u8 led_power_status = 0;

	for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
		led = &chip->leds[color];

		if (led->led_setting.brightness && !led->led_setting.blink) {/* led on */
			led_power_status = 1;
			break;
		}
	}

	return sprintf(buf, "%u\n", led_power_status);
#endif
	return 0;
}


static ssize_t led_pattern_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);
	struct qpnp_led_dev *led[SVCLED_MAX];
	int rc, mode, color;

	rc = sscanf(buf, "%1d", &mode);
	if (!rc) {
		dev_err(dev, "%s: failed to get led_pattern,%d\n", __func__, rc);
		return count;
	}

	dev_info(dev, "%s++: %d\n", __func__, mode);
	mutex_lock(&chip->function_lock);
	svcled_reset(chip);

	for (color = SVCLED_MIN; color < SVCLED_MAX; color++) {
		led[color] = &chip->leds[color];
		led[color]->ratio = chip->lpm ? led[color]->br_ratio_lpm : led[color]->br_ratio_normal;
	}

	chip->mode = mode;

	switch (mode) {
	case PATTERN_OFF:
		break;
	case CHARGING:
		svcled_set(dev, SVCLED_R, led[SVCLED_R]->ratio, 0, 0);
		break;
	case CHARGING_ERR:
		svcled_set(dev, SVCLED_R, led[SVCLED_R]->ratio, 500, 500);
		break;
	case MISSED_NOTI:
		svcled_set(dev, SVCLED_B, led[SVCLED_B]->ratio, 500, 5000);
		break;
	case LOW_BATTERY:
		svcled_set(dev, SVCLED_R, led[SVCLED_R]->ratio, 100, 3900);
		break;
	case FULLY_CHARGED:
		svcled_set(dev, SVCLED_G, led[SVCLED_G]->ratio, 0, 0);
		break;
	case POWERING:
		svcled_pattern_powering(dev, SVCLED_B, led[SVCLED_B]->ratio);
		break;
	case POWERING_OFF:
		svcled_pattern_powering(dev, SVCLED_R, led[SVCLED_R]->ratio);
		break;
	default:
		break;
	}

	mutex_unlock(&chip->function_lock);
	dev_info(dev, "%s--: %d\n", __func__, mode);

	return count;
}

static ssize_t led_pattern_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", chip->mode);
}

/* SAMSUNG specific attribute nodes */
static DEVICE_ATTR_WO(led_r);
static DEVICE_ATTR_WO(led_g);
static DEVICE_ATTR_WO(led_b);
static DEVICE_ATTR_RW(led_pattern);
static DEVICE_ATTR_RW(led_blink);
static DEVICE_ATTR_RW(led_brightness);
static DEVICE_ATTR_RW(led_lowpower);
static DEVICE_ATTR_RW(led_control);

static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	&dev_attr_led_control.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif

static int qpnp_tri_led_register(struct qpnp_tri_led_chip *chip)
{
	struct qpnp_led_dev *led;
	int rc, i, j;

	mutex_init(&led_client_lock);

	for (i = 0; i < chip->num_leds; i++) {
		led = &chip->leds[i];
		mutex_init(&led->lock);
		led->cdev.name = led->label;
		led->cdev.max_brightness = LED_FULL;
		led->cdev.brightness_set_blocking = qpnp_tri_led_set_brightness;
		led->cdev.brightness_get = qpnp_tri_led_get_brightness;
		led->cdev.blink_set = qpnp_tri_led_set_blink;
#ifdef CONFIG_SEC_SVCLED
		/* assign null to use software blink */
		/* the reason why using software blink is to make blink count logic */
		if (chip->is_sec_svcled)
			led->cdev.blink_set = NULL;
#endif
		led->cdev.default_trigger = led->default_trigger;
		led->cdev.brightness = LED_OFF;

		rc = devm_led_classdev_register(chip->dev, &led->cdev);
		if (rc < 0) {
			dev_err(chip->dev, "%s led class device registering failed, rc=%d\n",
							led->label, rc);
			goto err_out;
		}
#ifdef CONFIG_SEC_SVCLED
		if (chip->is_sec_svcled)
			led->ori_blink_timer_function = led->cdev.blink_timer.function;
#endif

		rc = qpnp_lpg_pwm_get_output_types_supported(led->pwm_dev);
		if (rc > 0 && (rc & PWM_OUTPUT_MODULATED)) {
			rc = sysfs_create_files(&led->cdev.dev->kobj,
					breath_attrs);
			if (rc < 0) {
				dev_err(chip->dev, "Create breath file for %s led failed, rc=%d\n",
						led->label, rc);
				goto err_out;
			}
		}
	}

	return 0;

err_out:
	for (j = 0; j <= i; j++) {
		if (j < i)
			sysfs_remove_files(&chip->leds[j].cdev.dev->kobj,
					breath_attrs);
		mutex_destroy(&chip->leds[j].lock);
	}
	return rc;
}

static int qpnp_tri_led_hw_init(struct qpnp_tri_led_chip *chip)
{
	int rc = 0;
	u8 val;

	rc = qpnp_tri_led_read(chip, TRILED_REG_TYPE, &val);
	if (rc < 0) {
		dev_err(chip->dev, "Read REG_TYPE failed, rc=%d\n", rc);
		return rc;
	}

	if (val != TRILED_TYPE) {
		dev_err(chip->dev, "invalid subtype(%d)\n", val);
		return -ENODEV;
	}

	rc = qpnp_tri_led_read(chip, TRILED_REG_SUBTYPE, &val);
	if (rc < 0) {
		dev_err(chip->dev, "Read REG_SUBTYPE failed, rc=%d\n", rc);
		return rc;
	}

	chip->subtype = val;

	return 0;
}

static int qpnp_tri_led_bl_update_status(struct backlight_device* bl) 
{
	struct qpnp_tri_led_chip* svc_led_ctrl = bl_get_data(bl);
	int brightness = 0, rc = 0;
	mutex_lock(&led_client_lock);
	if(svc_led_ctrl)
	{
		brightness =  bl->props.brightness;
		if(brightness > 0)
		{
			svc_led_ctrl->ref_count++;
			if(1 == svc_led_ctrl->ref_count)
			{
				led_client_type = led_client_type | SVCLED_CLIENT_CAM;
				rc = qpnp_tri_led_set_brightness(&svc_led_ctrl->leds[0].cdev, brightness);
				mutex_unlock(&led_client_lock);
				return rc;
			}
		}
		else
		{
			svc_led_ctrl->ref_count--;
			if(0 > svc_led_ctrl->ref_count)
			{
				svc_led_ctrl->ref_count = 0;
			}
			if(0 == svc_led_ctrl->ref_count)
			{
				led_client_type = ~(~led_client_type | SVCLED_CLIENT_CAM);
				rc = qpnp_tri_led_set_brightness(&svc_led_ctrl->leds[0].cdev, brightness);
				mutex_unlock(&led_client_lock);
				return rc;
			}
		}
	}	
	else
	{
		pr_err("backlight_device is NULL\n");
	}
	mutex_unlock(&led_client_lock);
	return -1;
}

static const struct backlight_ops triLed_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = qpnp_tri_led_bl_update_status,
};

static int qpnp_tri_led_backlight_register(struct qpnp_tri_led_chip* svc_led_ctrl)
{
	struct backlight_device* bl;
	struct backlight_properties props;
	dev_dbg(svc_led_ctrl->dev,"BL_REGISTER svc_led_ctrl->name[%s] register sucess \n", svc_led_ctrl->name);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = LED_FULL;
	props.brightness = 0;

	
	bl = devm_backlight_device_register(svc_led_ctrl->dev, svc_led_ctrl->name, svc_led_ctrl->dev, svc_led_ctrl, &triLed_bl_ops, &props);
	
	if (IS_ERR(bl))
	{
		dev_err(svc_led_ctrl->dev,"register error \n");
		return PTR_ERR(bl);
	}

	svc_led_ctrl->bl = bl;
	dev_dbg(svc_led_ctrl->dev,"Backlight register success in leds-qti-tri-led\n");
	return 0;
}

static int qpnp_tri_led_parse_dt(struct qpnp_tri_led_chip *chip)
{
	struct device_node *node = chip->dev->of_node, *child_node;
	struct qpnp_led_dev *led;
	struct pwm_args pargs;
	const __be32 *addr;
	int rc = 0, id, i = 0;
#ifdef CONFIG_SEC_SVCLED
	int temp;
#endif

	addr = of_get_address(chip->dev->of_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Getting address failed\n");
		return -EINVAL;
	}
	chip->reg_base = be32_to_cpu(addr[0]);

	chip->num_leds = of_get_available_child_count(node);
	if (chip->num_leds == 0) {
		dev_err(chip->dev, "No led child node defined\n");
		return -ENODEV;
	}

	if (chip->num_leds > TRILED_NUM_MAX) {
		dev_err(chip->dev, "can't support %d leds(max %d)\n",
				chip->num_leds, TRILED_NUM_MAX);
		return -EINVAL;
	}

	if (of_find_property(chip->dev->of_node, "nvmem", NULL)) {
		chip->pbs_nvmem = devm_nvmem_device_get(chip->dev, "pbs_sdam");
		if (IS_ERR_OR_NULL(chip->pbs_nvmem)) {
			rc = PTR_ERR(chip->pbs_nvmem);
			if (rc != -EPROBE_DEFER) {
				dev_err(chip->dev, "Couldn't get nvmem device, rc=%d\n",
					rc);
				return -ENODEV;
			}
			chip->pbs_nvmem = NULL;
			return rc;
		}
	}

	chip->leds = devm_kcalloc(chip->dev, chip->num_leds,
			sizeof(struct qpnp_led_dev), GFP_KERNEL);
	if (!chip->leds)
		return -ENOMEM;

#ifdef CONFIG_SEC_SVCLED
	chip->is_sec_svcled = of_property_read_bool(chip->dev->of_node, "sec_svcled");
	if (chip->is_sec_svcled) {
		rc = of_property_read_u32(chip->dev->of_node, "step", &temp);
		if (!rc) {
			chip->step = temp;
			dev_info(chip->dev, "step=%d\n", chip->step);
		} else {
			chip->step = 120;
			dev_err(chip->dev, "Get step failed, rc=%d\n", rc);
		}
	}
#endif

	for_each_available_child_of_node(node, child_node) {
		rc = of_property_read_u32(child_node, "led-sources", &id);
		if (rc) {
			dev_err(chip->dev, "Get led-sources failed, rc=%d\n",
							rc);
			return rc;
		}

		if (id >= TRILED_NUM_MAX) {
			dev_err(chip->dev, "only support 0~%d current source\n",
					TRILED_NUM_MAX - 1);
			return -EINVAL;
		}

		led = &chip->leds[i++];
		led->chip = chip;
		led->id = id;
		led->label =
			of_get_property(child_node, "label", NULL) ? :
							child_node->name;

#ifdef CONFIG_SEC_SVCLED
		if (chip->is_sec_svcled) {
			rc = of_property_read_u32(child_node, "br_ratio_normal", &temp);
			if (!rc) {
				led->br_ratio_normal = temp & 0xff;
				dev_info(chip->dev, "rto=%d\n", led->br_ratio_normal);
			} else {
				led->br_ratio_normal = chip->step; /* default set max */
				dev_err(chip->dev, "Get br_ratio_normal failed, rc=%d\n",
						rc);
			}

			rc = of_property_read_u32(child_node, "br_ratio_lpm", &temp);
			if (!rc) {
				led->br_ratio_lpm = temp & 0xff;
				dev_info(chip->dev, "rto_low=%d\n", led->br_ratio_lpm);
			} else {
				led->br_ratio_lpm = chip->step / 2; /* default set half */
				dev_err(chip->dev, "Get br_ratio_lpm failed, rc=%d\n",
						rc);
			}
		}
#endif

		led->pwm_dev =
			devm_of_pwm_get(chip->dev, child_node, NULL);
		if (IS_ERR(led->pwm_dev)) {
			rc = PTR_ERR(led->pwm_dev);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Get pwm device for %s led failed, rc=%d\n",
							led->label, rc);
			return rc;
		}

		pwm_get_args(led->pwm_dev, &pargs);
		if (pargs.period == 0)
			led->pwm_setting.pre_period_ns = PWM_PERIOD_DEFAULT_NS;
		else
			led->pwm_setting.pre_period_ns = pargs.period;

		led->default_trigger = of_get_property(child_node,
				"linux,default-trigger", NULL);
	}

	return rc;
}

#ifdef CONFIG_SEC_SVCLED
static void svcled_bootani_work_func(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct qpnp_tri_led_chip *chip;

	if (dw == NULL) {
		pr_err("%s: dw is null\n", __func__);
		return;
	}

	chip = 	container_of(dw, struct qpnp_tri_led_chip, svcled_bootani_work);

	dev_info(chip->dev, "%s++:\n", __func__);
	led_pattern_store(chip->dev, NULL, "6", 2); /* POWERING */
	dev_info(chip->dev, "%s--\n", __func__);
}
#endif

static int qpnp_tri_led_probe(struct platform_device *pdev)
{
	struct qpnp_tri_led_chip *chip;
	int rc = 0;

	pr_info("[LED] %s\n", __func__);

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Getting regmap failed\n");
		return -EINVAL;
	}

	rc = qpnp_tri_led_parse_dt(chip);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "Devicetree properties parsing failed, rc=%d\n",
								rc);
		return rc;
	}
	chip->name = "backlight_svc";

	mutex_init(&chip->bus_lock);

	rc = qpnp_tri_led_hw_init(chip);
	if (rc) {
		dev_err(chip->dev, "HW initialization failed, rc=%d\n", rc);
		goto destroy;
	}

	dev_set_drvdata(chip->dev, chip);
	rc = qpnp_tri_led_register(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Registering LED class devices failed, rc=%d\n",
								rc);
		goto destroy;
	}
	dev_dbg(chip->dev, "Tri-led module with subtype 0x%x is detected\n",
					chip->subtype);
					
	chip->ref_count = 0;
	rc = qpnp_tri_led_backlight_register(chip);
	if (rc) {
		dev_err(chip->dev, "failed to register backlight. err: %d\n", rc);
		goto destroy;
	}

#ifdef CONFIG_SEC_SVCLED
	if (chip->is_sec_svcled) {
		chip->led_dev = sec_device_create(chip, "led");
		if (unlikely(!chip->led_dev)) {
			dev_err(chip->dev, "failed create sec_class led-dev\n");
			goto destroy;
		}

		rc = sysfs_create_group(&chip->led_dev->kobj, &sec_led_attr_group);
		if (rc) {
			dev_err(chip->dev, "failed create sysfs:sec_led_attr\n");
			goto free_device;
		}
		pr_info("[LED] %s create sec svcled node\n", __func__);
	}
	mutex_init(&chip->function_lock);
	INIT_DELAYED_WORK(&chip->svcled_bootani_work, svcled_bootani_work_func);
	schedule_delayed_work(&chip->svcled_bootani_work,
				msecs_to_jiffies(6000));
#endif
	return 0;

#ifdef CONFIG_SEC_SVCLED
free_device:
	sec_device_destroy(chip->led_dev->devt);
#endif
destroy:
	mutex_destroy(&chip->bus_lock);
	dev_set_drvdata(chip->dev, NULL);

	return rc;
}

static int qpnp_tri_led_remove(struct platform_device *pdev)
{
	int i;
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(&pdev->dev);

	mutex_destroy(&chip->bus_lock);
	mutex_destroy(&led_client_lock);
	for (i = 0; i < chip->num_leds; i++) {
		sysfs_remove_files(&chip->leds[i].cdev.dev->kobj, breath_attrs);
		mutex_destroy(&chip->leds[i].lock);
	}
#ifdef CONFIG_SEC_SVCLED
	if (chip->is_sec_svcled)
		sysfs_remove_group(&chip->led_dev->kobj, &sec_led_attr_group);
#endif
	dev_set_drvdata(chip->dev, NULL);
	return 0;
}

static const struct of_device_id qpnp_tri_led_of_match[] = {
	{ .compatible = "qcom,tri-led",},
	{ },
};

static struct platform_driver qpnp_tri_led_driver = {
	.driver		= {
		.name		= "leds-qti-tri-led",
		.of_match_table	= qpnp_tri_led_of_match,
	},
	.probe		= qpnp_tri_led_probe,
	.remove		= qpnp_tri_led_remove,
};
module_platform_driver(qpnp_tri_led_driver);

MODULE_DESCRIPTION("QTI TRI_LED driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: pwm-qti-lpg");
