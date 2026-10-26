/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 */
#include <linux/leds.h>
#ifndef _LP855X_H
#define _LP855X_H

//#define LP8558_DEBUG_EN

#define LOG_TAG "ET_IR:"
#define LED_ERR(X, ...) pr_err("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)

#ifdef LP8558_DEBUG_EN
#define LED_INFO(X, ...) pr_err("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#define LED_DBG(X, ...) pr_err("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#else
#define LED_INFO(X, ...) pr_info("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#define LED_DBG(X, ...) pr_debug("%s %s : "X, LOG_TAG, __func__, ## __VA_ARGS__)
#endif

#define LP8558_LED_DEVICE_NAME	"lp8558"
#define IR_LED_SET_PARAM_DEFAULT 	0x4f4f0000	// (echo efef1155 1 > set_param), (echo efef04aa 1 > set_param), (echo 0 1 > set_param)
#define IR_LED_SET_PARAM_MASK 		0xffffffff

#define BL_CTL_SHFT	(0)
#define BRT_MODE_SHFT	(1)
#define BRT_MODE_MASK	(0x06)

/* Enable backlight. Only valid when BRT_MODE=10(I2C only) */
#define ENABLE_BL	(1)
#define DISABLE_BL	(0)

#define I2C_CONFIG(id)	id ## _I2C_CONFIG
#define PWM_CONFIG(id)	id ## _PWM_CONFIG

/* DEVICE CONTROL register - LP8550 */
#define LP8550_PWM_CONFIG	(LP8550_PWM_ONLY << BRT_MODE_SHFT)
#define LP8550_I2C_CONFIG	((ENABLE_BL << BL_CTL_SHFT) | \
				(LP8550_I2C_ONLY << BRT_MODE_SHFT))

/* DEVICE CONTROL register - LP8551 */
#define LP8551_PWM_CONFIG	LP8550_PWM_CONFIG
#define LP8551_I2C_CONFIG	LP8550_I2C_CONFIG

/* DEVICE CONTROL register - LP8552 */
#define LP8552_PWM_CONFIG	LP8550_PWM_CONFIG
#define LP8552_I2C_CONFIG	LP8550_I2C_CONFIG

/* DEVICE CONTROL register - LP8553 */
#define LP8553_PWM_CONFIG	LP8550_PWM_CONFIG
#define LP8553_I2C_CONFIG	LP8550_I2C_CONFIG

/* CONFIG register - LP8555 */
#define LP8555_PWM_STANDBY	BIT(7)
#define LP8555_PWM_FILTER	BIT(6)
#define LP8555_RELOAD_EPROM	BIT(3)	/* use it if EPROMs should be reset
					   when the backlight turns on */
#define LP8555_OFF_OPENLEDS	BIT(2)
#define LP8555_PWM_CONFIG	LP8555_PWM_ONLY
#define LP8555_I2C_CONFIG	LP8555_I2C_ONLY
#define LP8555_COMB1_CONFIG	LP8555_COMBINED1
#define LP8555_COMB2_CONFIG	LP8555_COMBINED2

/* DEVICE CONTROL register - LP8556 */
#define LP8556_PWM_CONFIG	(LP8556_PWM_ONLY << BRT_MODE_SHFT)
#define LP8556_COMB1_CONFIG	(LP8556_COMBINED1 << BRT_MODE_SHFT)
#define LP8556_I2C_CONFIG	((ENABLE_BL << BL_CTL_SHFT) | \
				(LP8556_I2C_ONLY << BRT_MODE_SHFT))
#define LP8556_COMB2_CONFIG	(LP8556_COMBINED2 << BRT_MODE_SHFT)
#define LP8556_FAST_CONFIG	BIT(7) /* use it if EPROMs should be maintained
					  when exiting the low power mode */

/* CONFIG register - LP8557 */
#define LP8557_PWM_STANDBY	BIT(7)
#define LP8557_PWM_FILTER	BIT(6)
#define LP8557_RELOAD_EPROM	BIT(3)	/* use it if EPROMs should be reset
					   when the backlight turns on */
#define LP8557_OFF_OPENLEDS	BIT(2)
#define LP8557_PWM_CONFIG	LP8557_PWM_ONLY
#define LP8557_I2C_CONFIG	LP8557_I2C_ONLY
#define LP8557_COMB1_CONFIG	LP8557_COMBINED1
#define LP8557_COMB2_CONFIG	LP8557_COMBINED2
#define LP8557_PATTERN_MODES 20

enum lp855x_chip_id {
	LP8550,
	LP8551,
	LP8552,
	LP8553,
	LP8555,
	LP8556,
	LP8557,
};

enum lp8550_brighntess_source {
	LP8550_PWM_ONLY,
	LP8550_I2C_ONLY = 2,
};

enum lp8551_brighntess_source {
	LP8551_PWM_ONLY = LP8550_PWM_ONLY,
	LP8551_I2C_ONLY = LP8550_I2C_ONLY,
};

enum lp8552_brighntess_source {
	LP8552_PWM_ONLY = LP8550_PWM_ONLY,
	LP8552_I2C_ONLY = LP8550_I2C_ONLY,
};

enum lp8553_brighntess_source {
	LP8553_PWM_ONLY = LP8550_PWM_ONLY,
	LP8553_I2C_ONLY = LP8550_I2C_ONLY,
};

enum lp8555_brightness_source {
	LP8555_PWM_ONLY,
	LP8555_I2C_ONLY,
	LP8555_COMBINED1,	/* Brightness register with shaped PWM */
	LP8555_COMBINED2,	/* PWM with shaped brightness register */
};

enum lp8556_brightness_source {
	LP8556_PWM_ONLY,
	LP8556_COMBINED1,	/* pwm + i2c before the shaper block */
	LP8556_I2C_ONLY,
	LP8556_COMBINED2,	/* pwm + i2c after the shaper block */
};

enum lp8557_brightness_source {
	LP8557_PWM_ONLY,
	LP8557_I2C_ONLY,
	LP8557_COMBINED1,	/* pwm + i2c after the shaper block */
	LP8557_COMBINED2,	/* pwm + i2c before the shaper block */
};

struct lp855x_rom_data {
	u8 addr;
	u8 val;
};

/**
 * struct lp855x_platform_data
 * @name : Backlight driver name. If it is not defined, default name is set.
 * @device_control : value of DEVICE CONTROL register
 * @initial_brightness : initial value of backlight brightness
 * @period_ns : platform specific pwm period value. unit is nano.
		Only valid when mode is PWM_BASED.
 * @size_program : total size of lp855x_rom_data
 * @rom_data : list of new eeprom/eprom registers
 */
struct lp855x_platform_data {
	const char *name;
	u8 device_control;
	u8 initial_brightness;
	unsigned int period_ns;
	int size_program;
	struct lp855x_rom_data *rom_data;
	unsigned int driver_en_gpio;
	int32_t led_pattern_reg[LP8557_PATTERN_MODES];
};

enum lp855x_brightness_ctrl_mode {
	PWM_BASED = 1,
	REGISTER_BASED,
};

struct t_led_param {
	u8 srg_pattern;
	u8 brow_pattern;
	u8 reserved;
	u8 extpwm_off;
	u8 srg_brightness;
	u8 brow_brightness;
};

struct lp855x {
	const char* chipname;
	enum lp855x_chip_id chip_id;
	enum lp855x_brightness_ctrl_mode mode;
	struct lp855x_device_config* cfg;
	struct i2c_client* client;
	struct backlight_device* bl;
	struct device* dev;
	struct lp855x_platform_data* pdata;
	struct pwm_device* pwm;
	struct regulator* supply;	/* regulator for VDD input */
	struct regulator* enable;	/* regulator for EN/VDDIO input */
	struct led_classdev	class_dev;
	u32 set_param;
	u32 force_trigger;
	struct t_led_param led_param;
	u8 slave_index;
	u8 pattern_data;
	u8 brightness_data;
	u8 vboostrange_data;
	u8 pwm_direct_mode_ctrl_data;
	u8 vboost_max_data;
	u8 led_fault_th_data;
	u8 pwm_direct_mode_current_data;
	u8 led_current_data;
	u8 device_ctrl_data;
	u8 bl_on_off_flag;
	struct mutex 		mutex;
}; 

// extern int64_t lp855x_led_mode_ctrl_et_left(int ledBrightness, int ledPattern, int force_trigger);
// extern int64_t lp855x_led_mode_ctrl_et_right(int ledBrightness, int ledPattern, int force_trigger);
extern int64_t lp855x_led_mode_ctrl(int ledBrightness, int ledPattern, int force_trigger);
extern int32_t lp855x_led_set_torch(int slaveIndex, int currentValue);

#endif // _LP855X_H
