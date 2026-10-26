/* Copyright (c) 2015-2023, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/backlight.h>
#include <linux/leds-lp8558.h>
#include "cam_sysfs_init.h"
#include "cam_ois_core.h"
#include "cam_eeprom_module.h"
#include "cam_actuator_core.h"
#include "cam_flash_dev.h"
#include <linux/regulator/max77720.h>
#if defined(CONFIG_LEDS_PMIC_QPNP)
#include "cam_flash_core.h"
#endif
#if defined(CONFIG_SAMSUNG_REAR_TOF)
#include "cam_sensor_dev.h"
#endif
#if defined(CONFIG_CAMERA_CDR_TEST)
#include "cam_clock_data_recovery.h"
#endif
#include "cam_res_mgr_api.h"

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
#include "cam_sensor_cmn_header.h"
#include "cam_debug_util.h"
#include "cam_hw_bigdata.h"
#endif

//#define CAM_SYS_DEBUG_EN // ONLY ENABLE FOR TESTING
#define GET16BIT_SIGNED(x) (((x ^ 0xFFFF)) + 0x0001)

#define SYS_TAG "[CAM_SYS]"
#define SYS_ERR(X, ...) pr_err("%s %s : "X, SYS_TAG, __func__, ## __VA_ARGS__)

#ifdef CAM_SYS_DEBUG_EN
#define SYS_INFO(X, ...) pr_err("%s %s : "X, SYS_TAG, __func__, ## __VA_ARGS__)
#define SYS_DBG(X, ...) pr_err("%s %s : "X, SYS_TAG, __func__, ## __VA_ARGS__)
#else
#define SYS_INFO(X, ...) pr_info("%s %s : "X, SYS_TAG, __func__, ## __VA_ARGS__)
#define SYS_DBG(X, ...) pr_debug("%s %s : "X, SYS_TAG, __func__, ## __VA_ARGS__)
#endif

#define FROM_SENSOR_ID_SIZE 16
#define TOF_TX_ERR_MASK_SIZE 20
#define DTOF_TX_ERR_MASK_SIZE 16
#define PD_RETRY_COUNT 3

#define GSCAM_FSYNC_GPIO_NUM 1209
#define ETCAM_FSYNC_GPIO_NUM 1211
#define FT_FPGA_RESET_GPIO_NUM 1154
#define ET_FPGA_RESET_GPIO_NUM 1146
#define HET_F_FPGA_RESET_GPIO_NUM 1132
#define HET_FSYNC_GPIO_NUM 1184

//GEN2 - gpio offset is 287 (ex: add 287 + gpio num)
#define ET_LEFT_RESET_GPIO_NUM 354
#define ET_RIGHT_RESET_GPIO_NUM 355

#define ET_IRLED_DRV_L_EN_GPIO_NUM 321
#define ET_IRLED_DRV_R_EN_GPIO_NUM 322

// 4 GPIO ports are using for ET IRLED control.
#define ET_IRLED_L_SEL0 1166
#define ET_IRLED_L_SEL1 1167
#define ET_IRLED_R_SEL0 1104
#define ET_IRLED_R_SEL1 1105


#define SYSFS_FW_VER_SIZE       40
#define SYSFS_MODULE_INFO_SIZE  96
/* #define FORCE_CAL_LOAD */
#define SYSFS_MAX_READ_SIZE     4096

extern struct kset *devices_kset;
struct class *camera_class;
extern struct device *is_dev;

#if defined(CONFIG_SAMSUNG_REAR_TOF)


extern int check_pd_ready;
extern char tof_freq[10];

extern void cam_sensor_i2c_write(uint32_t sensor_index, uint32_t addr, uint32_t data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);

extern void cam_sensor_i2c_read(uint32_t sensor_index, uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);

#endif

extern struct cam_sensor_ctrl_t *g_s_ctrl_info[20];
extern struct cam_flash_ctrl *g_flash_ctrl[MAX_NUMBER_OF_FLASH];

static int get_flash_index_by_led_type(int ledType, int ledIndex)
{
	int idx;
	for(idx = 0; idx < MAX_NUMBER_OF_FLASH; idx++)
	{
		if((NULL != g_flash_ctrl[idx]) && 
			(ledType == g_flash_ctrl[idx]->led_type) &&
			(ledIndex == g_flash_ctrl[idx]->led_index))
		{
			return idx;
		}
	}
	return -1;
} 

static int cam_ft_hat_flash_control(int soc_index, uint32_t curr)
{
	int i = 0;

	if( (g_flash_ctrl[soc_index]) && 
		(g_flash_ctrl[soc_index]->switch_trigger))
	{
		SYS_INFO("Sucess with flash control %d", soc_index);
	}
	else
	{
		SYS_INFO("g_flash_ctrl[%d] with flash control is NULL or Unsupported index!!!", soc_index);
		return 0;
	}
	if(curr)
	{
		// TORCH source with brightness value
		for (i = 0; i < g_flash_ctrl[soc_index]->torch_num_sources; i++) {
			cam_res_mgr_led_trigger_event(
				g_flash_ctrl[soc_index]->torch_trigger[i], curr);
		}
		// SWITCH ON
		cam_res_mgr_led_trigger_event(
			g_flash_ctrl[soc_index]->switch_trigger,
			(enum led_brightness)LED_SWITCH_ON);
	}
	else
	{
		// TORCH source with 0 brightness
		for (i = 0; i < g_flash_ctrl[soc_index]->torch_num_sources; i++) {
			cam_res_mgr_led_trigger_event(
				g_flash_ctrl[soc_index]->torch_trigger[i], 0);
		}
		// SWITCH OFF
		cam_res_mgr_led_trigger_event(g_flash_ctrl[soc_index]->switch_trigger,
			(enum led_brightness)LED_SWITCH_OFF);
	}
	return 0;
}

int et_left_pattern_value = 0xF;
int et_right_pattern_value = 0xF;

static ssize_t et_flash_left_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0, brightness=0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	SYS_INFO("value_u32 : %d\n", value_u32);

	brightness = value_u32 & 0x0FF;
	if(brightness > 100) brightness = 100;
	SYS_INFO("Et_flash_left brightness = %d, pattern = %d", brightness, et_left_pattern_value);

	soc_index = get_flash_index_by_led_type(LP855L_LED,0);
	if(soc_index >= 0)
	{
		if(g_flash_ctrl[soc_index]->bd)
				backlight_device_set_properties(g_flash_ctrl[soc_index]->bd, brightness, et_left_pattern_value, 1);
		else
			SYS_ERR("[%d] Backlight device not available to set brightness = %d, pattern = %d", soc_index, brightness, et_left_pattern_value);
	}
	return size;
}

static ssize_t et_flash_left_pattern_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	SYS_INFO("pattern value : %d -> %d \n", et_left_pattern_value, value_u32);
	et_left_pattern_value = value_u32;
	return size;
}

static ssize_t et_flash_current_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t et_flash_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, soc_index;
	int read_addr = 0xA1;
	u8 read_data = 0, current_mA = 0;
	struct lp855x* lp = NULL;

	soc_index = get_flash_index_by_led_type(LP855L_LED,0);
	if(soc_index >= 0)
	{
		lp =  bl_get_data(g_flash_ctrl[soc_index]->bd);
	}
	if(NULL != lp)
	{
		if ((ret = i2c_smbus_read_i2c_block_data(lp->client, read_addr, 1, &read_data)) < 0)
		{
			LED_ERR("failure on i2c read block data at [0x%X] \n", read_addr);
		}
		else
		{
			current_mA = (read_data & 0xf)*2;
			return sprintf(buf,"Current in mA =%u\n", current_mA);
		}
	}
	return sprintf(buf, "Failed to get current\n");
}

static ssize_t et2_flash_current_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t et2_flash_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, soc_index;
	int read_addr = 0xA1;
	u8 read_data = 0, current_mA = 0;
	struct lp855x* lp = NULL;

	soc_index = get_flash_index_by_led_type(LP855L_LED,1);
	if(soc_index >= 0)
	{
		lp =  bl_get_data(g_flash_ctrl[soc_index]->bd);
	}
	if(NULL != lp)
	{
		if ((ret = i2c_smbus_read_i2c_block_data(lp->client, read_addr, 1, &read_data)) < 0)
		{
			LED_ERR("failure on i2c read block data at [0x%X] \n", read_addr);
		}
		else
		{
			current_mA = (read_data & 0xf)*2;
			return sprintf(buf,"Current in mA =%u\n", current_mA);
		}
	}
	return sprintf(buf, "Failed to get current\n");
}
static ssize_t et_flash_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, i2c_fail = 0, soc_index;
	u8 pseudo_reg[32] = {0};
	int read_addr[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x16, 0x9E, 0xA5, 0xA9, 0xAE, 0xA0, 0xA1, 0xA2 };
	char sPrintStr[256] = { 0 };
	int wCount = 0;
	int read_len = sizeof(read_addr) / sizeof(int);
	int idx;
	struct lp855x* lp = NULL;
	memset(sPrintStr, 0, sizeof(sPrintStr));

	soc_index = get_flash_index_by_led_type(LP855L_LED, 0);
	if(soc_index >= 0 && NULL != g_flash_ctrl[soc_index]->bd)
	{
		lp =  bl_get_data(g_flash_ctrl[soc_index]->bd);
	}
	
	if(NULL != lp)
	{
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_L_EN_GPIO_NUM, 1);
		for (idx = 0; idx < read_len; idx++)
		{
			if ((ret = i2c_smbus_read_i2c_block_data(lp->client, read_addr[idx], 1, &pseudo_reg[idx])) < 0)
			{
				LED_ERR("failure on i2c read block data at [%d][0x%X] \n", idx, read_addr[idx]);
				i2c_fail = 1;
				break;
			}
			wCount += sprintf(&sPrintStr[wCount], "reg[0x%X] = 0x%X\n", read_addr[idx], pseudo_reg[idx]);
		}
		LED_ERR("lp855x_reg_show: dev:[0x%8x:%p] i2c:[%d] str:[%s]\n",
			&lp->client->dev, &lp->client->dev, i2c_fail, sPrintStr);
		if (0 == i2c_fail)
		{
			cam_res_mgr_gpio_set_value(ET_IRLED_DRV_L_EN_GPIO_NUM, 0);
			return sprintf(buf, "SUCCESS\n %s", sPrintStr);
		}
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_L_EN_GPIO_NUM, 0);
	}

	return sprintf(buf, "FAIL");
}
static ssize_t et_flash_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static ssize_t et_flash_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, i2c_fail = 0, soc_index;
	u8 pseudo_reg[32] = {0};
	int read_addr[] = { 0x3 };
	char sPrintStr[256] = { 0 };
	int wCount = 0;
	int read_len = sizeof(read_addr) / sizeof(int);
	int idx;
	struct lp855x* lp = NULL;
	memset(sPrintStr, 0, sizeof(sPrintStr));

	soc_index = get_flash_index_by_led_type(LP855L_LED, 0);
	if(soc_index >= 0 && NULL != g_flash_ctrl[soc_index]->bd)
	{
		lp =  bl_get_data(g_flash_ctrl[soc_index]->bd);
	}
	
	if(NULL != lp)
	{
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_L_EN_GPIO_NUM, 1);
		usleep_range(250, 260);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		usleep_range(2000, 2100);
		i2c_smbus_write_byte_data(lp->client, 0xa9, 0x80);
		i2c_smbus_write_byte_data(lp->client, 0x01, 0x80);
		i2c_smbus_write_byte_data(lp->client, 0x16, 0x0);
		usleep_range(2000, 2100);
		for (idx = 0; idx < read_len; idx++)
		{
			if ((ret = i2c_smbus_read_i2c_block_data(lp->client, read_addr[idx], 1, &pseudo_reg[idx])) < 0)
			{
				LED_ERR("failure on i2c read block data at [%d][0x%X] \n", idx, read_addr[idx]);
				i2c_fail = 1;
				break;
			}
			wCount += sprintf(&sPrintStr[wCount], "reg[0x%X] = 0x%X\n", read_addr[idx], pseudo_reg[idx]);
			usleep_range(2000, 2100);
		}
		LED_ERR("lp855x_reg_show: dev:[0x%8x:%p] i2c:[%d] str:[%s]\n",
			&lp->client->dev, &lp->client->dev, i2c_fail, sPrintStr);
		if (0 == i2c_fail)
		{
			cam_res_mgr_gpio_set_value(ET_IRLED_DRV_L_EN_GPIO_NUM, 0);
			return sprintf(buf, "SUCCESS\n");
		}
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_L_EN_GPIO_NUM, 0);
	}

	return sprintf(buf, "FAIL\n");
}
static ssize_t et_flash_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static ssize_t et2_flash_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, i2c_fail = 0, soc_index;
	u8 pseudo_reg[32] = {0};
	int read_addr[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x16, 0x9E, 0xA5, 0xA9, 0xAE, 0xA0, 0xA1, 0xA2 };
	char sPrintStr[256] = { 0 };
	int wCount = 0;
	int read_len = sizeof(read_addr) / sizeof(int);
	int idx;
	struct lp855x* lp = NULL;
	memset(sPrintStr, 0, sizeof(sPrintStr));

	soc_index = get_flash_index_by_led_type(LP855L_LED,1);
	if(soc_index >= 0 && NULL != g_flash_ctrl[soc_index]->bd)
	{
		lp =  bl_get_data(g_flash_ctrl[soc_index]->bd);
	}

	if(NULL != lp)
	{
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_R_EN_GPIO_NUM, 1);
		for (idx = 0; idx < read_len; idx++)
		{
			if ((ret = i2c_smbus_read_i2c_block_data(lp->client, read_addr[idx], 1, &pseudo_reg[idx])) < 0)
			{
				LED_ERR("failure on i2c read block data at [%d][0x%X] \n", idx, read_addr[idx]);
				i2c_fail = 1;
				break;
			}
			wCount += sprintf(&sPrintStr[wCount], "reg[0x%X] = 0x%X\n", read_addr[idx], pseudo_reg[idx]);
		}
		LED_ERR("lp855x_reg_show: dev:[0x%8x:%p] i2c:[%d] str:[%s]\n",
			&lp->client->dev, &lp->client->dev, i2c_fail, sPrintStr);
		if (0 == i2c_fail)
		{
			cam_res_mgr_gpio_set_value(ET_IRLED_DRV_R_EN_GPIO_NUM, 0);
			return sprintf(buf, "SUCCESS\n %s", sPrintStr);
		}
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_R_EN_GPIO_NUM, 0);
	}

	return sprintf(buf, "FAIL");
}
static ssize_t et2_flash_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static ssize_t et2_flash_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, i2c_fail = 0, soc_index;
	u8 pseudo_reg[32] = {0};
	int read_addr[] = { 0x3 };
	char sPrintStr[256] = { 0 };
	int wCount = 0;
	int read_len = sizeof(read_addr) / sizeof(int);
	int idx;
	struct lp855x* lp = NULL;
	memset(sPrintStr, 0, sizeof(sPrintStr));

	soc_index = get_flash_index_by_led_type(LP855L_LED,1);
	if(soc_index >= 0 && NULL != g_flash_ctrl[soc_index]->bd)
	{
		lp =  bl_get_data(g_flash_ctrl[soc_index]->bd);
	}

	if(NULL != lp)
	{
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_R_EN_GPIO_NUM, 1);
		usleep_range(250, 260);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		i2c_smbus_write_byte_data(lp->client, 0x9e, 0x1);
		usleep_range(2000, 2100);
		i2c_smbus_write_byte_data(lp->client, 0xa9, 0x80);
		i2c_smbus_write_byte_data(lp->client, 0x01, 0x80);
		i2c_smbus_write_byte_data(lp->client, 0x16, 0x0);
		usleep_range(2000, 2100);
		for (idx = 0; idx < read_len; idx++)
		{
			if ((ret = i2c_smbus_read_i2c_block_data(lp->client, read_addr[idx], 1, &pseudo_reg[idx])) < 0)
			{
				LED_ERR("failure on i2c read block data at [%d][0x%X] \n", idx, read_addr[idx]);
				i2c_fail = 1;
				break;
			}
			wCount += sprintf(&sPrintStr[wCount], "reg[0x%X] = 0x%X\n", read_addr[idx], pseudo_reg[idx]);
			usleep_range(2000, 2100);
		}
		LED_ERR("lp855x_reg_show: dev:[0x%8x:%p] i2c:[%d] str:[%s]\n",
			&lp->client->dev, &lp->client->dev, i2c_fail, sPrintStr);
		if (0 == i2c_fail)
		{
			cam_res_mgr_gpio_set_value(ET_IRLED_DRV_R_EN_GPIO_NUM, 0);
			return sprintf(buf, "SUCCESS\n");
		}
		cam_res_mgr_gpio_set_value(ET_IRLED_DRV_R_EN_GPIO_NUM, 0);
	}
	return sprintf(buf, "FAIL\n");
}
static ssize_t et2_flash_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t et_flash_left_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t et_flash_right_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0, brightness = 0 ;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	SYS_INFO("value_u32 : %d\n", value_u32);

	brightness = value_u32 & 0x0FF;
	if(brightness > 100) brightness = 100;
	SYS_INFO("Et_flash_right brightness = %d, pattern = %d", brightness, et_right_pattern_value);

	soc_index = get_flash_index_by_led_type(LP855L_LED,1);
	if(soc_index >= 0)
	{
		if(g_flash_ctrl[soc_index]->bd)
				backlight_device_set_properties(g_flash_ctrl[soc_index]->bd, brightness, et_right_pattern_value, 1);
		else
			SYS_ERR("[%d] Backlight device not available to set brightness = %d, pattern = %d", soc_index, brightness, et_right_pattern_value);
	}
	return size;
}

static ssize_t et_flash_right_pattern_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	SYS_INFO("pattern value : %d -> %d \n", et_right_pattern_value, value_u32);
	et_right_pattern_value = value_u32;
	return size;
}

static ssize_t et_flash_right_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t hat_lu_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("hat_lu_flash buf : %s Brightness : %d\n", buf, value_u32);

	soc_index = get_flash_index_by_led_type(S2MPB02_PMIC,0);
	if(soc_index >= 0)
	{
		s2mpb02_led_ctrl(g_flash_ctrl[soc_index]->led_index, value_u32, 0xF, S2MPB02_LED_TURN_WAY_I2C);
	}
	return size;
}

static ssize_t hat_lu_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}
static ssize_t hat_lu_flash_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t hat_lu_flash_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t hat_ru_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("hat_ru_flash buf : %s Brightness : %d\n", buf, value_u32);

	soc_index = get_flash_index_by_led_type(S2MPB02_PMIC,1);
	if(soc_index >= 0)
	{
		s2mpb02_led_ctrl(g_flash_ctrl[soc_index]->led_index, value_u32, 0xF, S2MPB02_LED_TURN_WAY_I2C);
	}
	return size;
}

static ssize_t hat_ru_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}
static ssize_t hat_ru_flash_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t hat_ru_flash_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t hat_ld_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("hat_ld_flash buf : %s brightness : %d\n", buf, value_u32);
	soc_index = get_flash_index_by_led_type(AP_PMIC_LED,0);
	if(soc_index >= 0)
	{
		cam_ft_hat_flash_control(soc_index, value_u32);
	}
	return size;
}

static ssize_t hat_ld_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}
static ssize_t hat_ld_flash_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t hat_ld_flash_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t hat_rd_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("hat_rd_flash buf : %s brightness : %d\n", buf, value_u32);
	soc_index = get_flash_index_by_led_type(AP_PMIC_LED,1);
	if(soc_index >= 0)
	{
		cam_ft_hat_flash_control(soc_index, value_u32);
	}
	return size;
}

static ssize_t hat_rd_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}
static ssize_t hat_rd_flash_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t hat_rd_flash_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t svc_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int soc_index;
	uint32_t value_u32, org_value;
	value_u32= 0;
	org_value = 0;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	org_value = value_u32;
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("buf : %s value_u32 : %d, org_value %d\n", buf, value_u32, org_value);
	
	soc_index = get_flash_index_by_led_type(SVC_LED,0);
	if(soc_index >= 0)
	{
		if(g_flash_ctrl[soc_index]->bd)
				backlight_device_set_brightness(g_flash_ctrl[soc_index]->bd, value_u32);
		else
			SYS_ERR("[%d] Backlight device not available", soc_index);
	}
	return size;
}

static ssize_t svc_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t ft_flash_left_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("ft_flash_left buf : %s brightness : %d\n", buf, value_u32);
	soc_index = get_flash_index_by_led_type(AP_PMIC_LED,0);
	if(soc_index >= 0)
	{
		cam_ft_hat_flash_control(soc_index, value_u32);
	}
	return size;
}

static ssize_t ft_flash_left_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t ft_flash_right_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("ft_flash_right buf : %s brightness : %d\n", buf, value_u32);
	soc_index = get_flash_index_by_led_type(AP_PMIC_LED,1);
	if(soc_index >= 0)
	{
		cam_ft_hat_flash_control(soc_index, value_u32);
	}
	return size;
}

static ssize_t ft_flash_right_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

static ssize_t ft_flash_mid_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value_u32 = 0;
	int soc_index;
	if ((buf == NULL) || kstrtouint(buf, 10, &value_u32))
	{
		SYS_INFO("Invalid Buffer");
		return size;
	}
	if(value_u32 > 100)
		value_u32 = 100;
	SYS_INFO("ft_flash_mid buf : %s brightness : %d\n", buf, value_u32);
	soc_index = get_flash_index_by_led_type(AP_PMIC_LED,2);
	if(soc_index >= 0)
	{
		cam_ft_hat_flash_control(soc_index, value_u32);
	}
	return size;
}

static ssize_t ft_flash_mid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d", ret);
	return ret;
}

char cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_INFO("[FW_DBG] cam_fw_ver : %s\n", cam_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_fw_ver, sizeof(cam_fw_ver), "%s", buf);

	return size;
}

static ssize_t rear_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SONY_IMX564\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);

	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "OV_OG0TB\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "OV_OG0TB\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front5_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "OV_OG0TB\n";
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_fw_user_ver : %s\n", cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_fw_user_ver, sizeof(cam_fw_user_ver), "%s", buf);

	return size;
}

char cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear_fw_factory_ver : %s\n", cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_fw_factory_ver, sizeof(cam_fw_factory_ver), "%s", buf);

	return size;
}

char cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear_fw_full_ver : %s\n", cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_fw_full_ver, sizeof(cam_fw_full_ver), "%s", buf);

	return size;
}

#if defined (CONFIG_SEC_VST_PROJECT)
static ssize_t gscam_fsync_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = GSCAM_FSYNC_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = 0;//cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gscam_fsync gpio_get_value() error!\n");
	}
	else
	{
		SYS_ERR("get gscam_fsync = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t gscam_fsync_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = GSCAM_FSYNC_GPIO_NUM;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);

	if(gpio_val < 0 || gpio_val > 2)
	{
		SYS_ERR("set gscam_fsync invalid input = %d (0:low, 1:high, 2:trigger)\n", gpio_val);
		return size;
	}

	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);

		if(ret < 0)
		{
			SYS_ERR("set gscam_fsync gpio_output() error! (val = %d)\n", gpio_val);
		}
		else
		{
			SYS_ERR("set gscam_fsync = %d (gpio %d)\n", gpio_val, gpio_num);
		}
	}
	else
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

		if(ret < 0)
		{
			SYS_ERR("set gscam_fsync gpio_trigger (init-low) error!\n");
		}

		msleep(1);

		ret = cam_res_mgr_gpio_set_value(gpio_num, 1);

		if(ret < 0)
		{
			SYS_ERR("set gscam_fsync gpio_trigger (high) error!\n");
		}
		else
		{
			msleep(1);

			ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

			if(ret < 0)
			{
				SYS_ERR("set gscam_fsync gpio_trigger (low) error!\n");
			}
			else
			{
				SYS_ERR("set gscam_fsync gpio_triggered (gpio %d)\n", gpio_num);
			}
		}
	}

	return size;
}

static ssize_t etcam_fsync_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ETCAM_FSYNC_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = 0;//cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get etcam_fsync gpio_get_value() error!\n");
	}
	else
	{
		SYS_ERR("get etcam_fsync = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t etcam_fsync_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = ETCAM_FSYNC_GPIO_NUM;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);

	if(gpio_val < 0 || gpio_val > 2)
	{
		SYS_ERR("set etcam_fsync invalid input = %d (0:low, 1:high, 2:trigger)\n", gpio_val);
		return size;
	}

	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);

		if(ret < 0)
		{
			SYS_ERR("set etcam_fsync gpio_output() error! (val = %d)\n", gpio_val);
		}
		else
		{
			SYS_ERR("set etcam_fsync = %d (gpio %d)\n", gpio_val, gpio_num);
		}
	}
	else
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

		if(ret < 0)
		{
			SYS_ERR("set etcam_fsync gpio_trigger (init-low) error!\n");
		}

		msleep(1);

		ret = cam_res_mgr_gpio_set_value(gpio_num, 1);

		if(ret < 0)
		{
			SYS_ERR("set etcam_fsync gpio_trigger (high) error!\n");
		}
		else
		{
			msleep(1);

			ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

			if(ret < 0)
			{
				SYS_ERR("set etcam_fsync gpio_trigger (low) error!\n");
			}
			else
			{
				SYS_ERR("set etcam_fsync gpio_triggered (gpio %d)\n", gpio_num);
			}
		}
	}

	return size;
}

int cam_boot_rev = 0;
static ssize_t cam_boot_rev_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[AR_DBG] SHOW cam_boot_rev : %d\n", cam_boot_rev);
	rc = scnprintf(buf, PAGE_SIZE, "%d", cam_boot_rev);
	if (rc)
		return rc;
	return 0;
}

static ssize_t cam_boot_rev_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int mode_val = -1;
	sscanf(buf, "%d", &mode_val);
	SYS_DBG("[AR_DBG] Not Storing cam_boot_rev : %d\n", mode_val);
	return size;
}

static void sec_gpio_control_set(int gpio_num, int gpio_val)
{
	int ret = 0;
	if(gpio_val < 0 || gpio_val > 2)
	{
		SYS_ERR("set sec_fpga_rst_pin_store invalid input = %d (0:low, 1:high, 2:trigger)\n", gpio_val);
		return;
	}

	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);

		if(ret < 0)
		{
			SYS_ERR("set gpio_output() error! (val = %d)\n", gpio_val);
		}
		else
		{
			SYS_INFO("set gpio value = %d (gpio %d) success!\n", gpio_val, gpio_num);
		}
	}
	else
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

		if(ret < 0)
		{
			SYS_ERR("set sec_fpga_rst_pin_store gpio_trigger (init-low) error!\n");
		}

		msleep(1);

		ret = cam_res_mgr_gpio_set_value(gpio_num, 1);

		if(ret < 0)
		{
			SYS_ERR("set gpio_trigger (high) error!\n");
		}
		else
		{
			msleep(1);

			ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

			if(ret < 0)
			{
				SYS_ERR("set gpio_trigger (low) error!\n");
			}
			else
			{
				SYS_INFO("set gpio_triggered (gpio %d)\n", gpio_num);
			}
		}
	}

}

static ssize_t sec_et_irled_l_sel0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_IRLED_L_SEL0;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_irled_l_sel0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gpio_num = ET_IRLED_L_SEL0;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);
	sec_gpio_control_set(gpio_num, gpio_val);
	
	return size;
}

static ssize_t sec_et_irled_l_sel1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_IRLED_L_SEL1;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_irled_l_sel1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gpio_num = ET_IRLED_L_SEL1;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);
	sec_gpio_control_set(gpio_num, gpio_val);
	
	return size;
}

static ssize_t sec_et_irled_r_sel0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_IRLED_R_SEL0;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_irled_r_sel0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gpio_num = ET_IRLED_R_SEL0;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);
	sec_gpio_control_set(gpio_num, gpio_val);
	
	return size;
}

static ssize_t sec_et_irled_r_sel1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_IRLED_R_SEL1;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_irled_r_sel1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gpio_num = ET_IRLED_R_SEL1;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);
	sec_gpio_control_set(gpio_num, gpio_val);
	
	return size;
}

static ssize_t sec_ft_fpga_rst_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = FT_FPGA_RESET_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_het_f_fpga_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = HET_F_FPGA_RESET_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_het_f_fpga_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gpio_num = HET_F_FPGA_RESET_GPIO_NUM;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);
	sec_gpio_control_set(gpio_num, gpio_val);
	
	return size;
}

static ssize_t sec_het_fsync_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = HET_FSYNC_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get gpio_get_value(%d) error!\n", gpio_num);
	}
	else
	{
		SYS_INFO("get value = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_het_fsync_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gpio_num = HET_FSYNC_GPIO_NUM;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);
	sec_gpio_control_set(gpio_num, gpio_val);
	
	return size;
}

static ssize_t sec_ft_fpga_rst_pin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = FT_FPGA_RESET_GPIO_NUM;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);

	if(gpio_val < 0 || gpio_val > 2)
	{
		SYS_ERR("set sec_fpga_rst_pin_store invalid input = %d (0:low, 1:high, 2:trigger)\n", gpio_val);
		return size;
	}

	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);

		if(ret < 0)
		{
			SYS_ERR("set sec_fpga_rst_pin_store gpio_output() error! (val = %d)\n", gpio_val);
		}
		else
		{
			SYS_ERR("set sec_fpga_rst_pin_store = %d (gpio %d) success!\n", gpio_val, gpio_num);
		}
	}
	else
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

		if(ret < 0)
		{
			SYS_ERR("set sec_fpga_rst_pin_store gpio_trigger (init-low) error!\n");
		}

		msleep(1);

		ret = cam_res_mgr_gpio_set_value(gpio_num, 1);

		if(ret < 0)
		{
			SYS_ERR("set sec_fpga_rst_pin_store gpio_trigger (high) error!\n");
		}
		else
		{
			msleep(1);

			ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

			if(ret < 0)
			{
				SYS_ERR("set sec_fpga_rst_pin_store gpio_trigger (low) error!\n");
			}
			else
			{
				SYS_ERR("set sec_fpga_rst_pin_store gpio_triggered (gpio %d)\n", gpio_num);
			}
		}
	}

	return size;
}

static ssize_t sec_et_fpga_rst_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_FPGA_RESET_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get sec_et_fpga_rst_pin_store gpio_get_value() error!\n");
	}
	else
	{
		SYS_ERR("get sec_et_fpga_rst_pin_store = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_fpga_rst_pin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = ET_FPGA_RESET_GPIO_NUM;
	int gpio_val = -1;

	sscanf(buf, "%d", &gpio_val);

	if(gpio_val < 0 || gpio_val > 2)
	{
		SYS_ERR("set sec_et_fpga_rst_pin_store invalid input = %d (0:low, 1:high, 2:trigger)\n", gpio_val);
		return size;
	}

	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);

		if(ret < 0)
		{
			SYS_ERR("set sec_et_fpga_rst_pin_store gpio_output() error! (val = %d)\n", gpio_val);
		}
		else
		{
			SYS_ERR("set sec_et_fpga_rst_pin_store = %d (gpio %d) success!\n", gpio_val, gpio_num);
		}
	}
	else
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

		if(ret < 0)
		{
			SYS_ERR("set sec_et_fpga_rst_pin_store gpio_trigger (init-low) error!\n");
		}

		msleep(1);

		ret = cam_res_mgr_gpio_set_value(gpio_num, 1);

		if(ret < 0)
		{
			SYS_ERR("set sec_et_fpga_rst_pin_store gpio_trigger (high) error!\n");
		}
		else
		{
			msleep(1);

			ret = cam_res_mgr_gpio_set_value(gpio_num, 0);

			if(ret < 0)
			{
				SYS_ERR("set sec_et_fpga_rst_pin_store gpio_trigger (low) error!\n");
			}
			else
			{
				SYS_ERR("set sec_et_fpga_rst_pin_store gpio_triggered (gpio %d)\n", gpio_num);
			}
		}
	}

	return size;
}

static ssize_t sec_et_left_rst_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_LEFT_RESET_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_INFO("get sec_et_left_rst_pin_show gpio_get_value() error!\n");
	}
	else
	{
		SYS_INFO("get sec_et_left_rst_pin_show = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_left_rst_pin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = ET_LEFT_RESET_GPIO_NUM;
	int gpio_val = -1;
	sscanf(buf, "%d", &gpio_val);
	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);
		SYS_INFO("gpio_set_value : num: %d, value: %d, ret: %d, success!!!", gpio_num, gpio_val, ret);
	}
	else
	{
		SYS_INFO("gpio_set_value : num: %d, value: %d wrong arg!!!", gpio_num, gpio_val);
	}

	return size;
}

static ssize_t sec_et_right_rst_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_RIGHT_RESET_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_INFO("get sec_et_right_rst_pin_show gpio_get_value() error!\n");
	}
	else
	{
		SYS_INFO("get sec_et_right_rst_pin_show = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_right_rst_pin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = ET_RIGHT_RESET_GPIO_NUM;
	int gpio_val = -1;
	sscanf(buf, "%d", &gpio_val);
	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);
		SYS_INFO("gpio_set_value : num: %d, value: %d, ret: %d, success!!!", gpio_num, gpio_val, ret);
	}
	else
	{
		SYS_INFO("gpio_set_value : num: %d, value: %d wrong arg!!!", gpio_num, gpio_val);
	}

	return size;
}

static ssize_t sec_et_irled_left_enable_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_IRLED_DRV_L_EN_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get sec_et_irled_left_enable_pin_show gpio_get_value() error!\n");
	}
	else
	{
		SYS_ERR("get sec_et_irled_left_enable_pin_show = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_irled_left_enable_pin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = ET_IRLED_DRV_L_EN_GPIO_NUM;
	int gpio_val = -1;
	sscanf(buf, "%d", &gpio_val);
	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);
		SYS_ERR("gpio_set_value : num: %d, value: %d, ret: %d, success!!!", gpio_num, gpio_val, ret);
	}
	else
	{
		SYS_ERR("gpio_set_value : num: %d, value: %d wrong arg!!!", gpio_num, gpio_val);
	}

	return size;
}

static ssize_t sec_et_irled_right_enable_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int gpio_num = ET_IRLED_DRV_R_EN_GPIO_NUM;
	int gpio_val = -1;

	gpio_val = cam_res_mgr_gpio_get_value(gpio_num);

	if(gpio_val < 0)
	{
		SYS_ERR("get sec_et_irled_right_enable_pin_show gpio_get_value() error!\n");
	}
	else
	{
		SYS_ERR("get sec_et_irled_right_enable_pin_show = %d (gpio %d)\n", gpio_val, gpio_num);
	}

	ret = sprintf(buf, "%d", gpio_val);

	return ret;
}

static ssize_t sec_et_irled_right_enable_pin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int gpio_num = ET_IRLED_DRV_R_EN_GPIO_NUM;
	int gpio_val = -1;
	sscanf(buf, "%d", &gpio_val);
	if(gpio_val == 0 || gpio_val == 1)
	{
		ret = cam_res_mgr_gpio_set_value(gpio_num, gpio_val);
		SYS_ERR("gpio_set_value : num: %d, value: %d, ret: %d, success!!!", gpio_num, gpio_val, ret);
	}
	else
	{
		SYS_ERR("gpio_set_value : num: %d, value: %d wrong arg!!!", gpio_num, gpio_val);
	}

	return size;
}

int fsync_mode = 1;
static ssize_t cam_fsync_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[AR_DBG] SHOW cam_fsync_mode : %d\n", fsync_mode);
	rc = scnprintf(buf, PAGE_SIZE, "%d", fsync_mode);
	if (rc)
		return rc;
	return 0;

}

static ssize_t cam_fsync_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int mode_val = -1;
    sscanf(buf, "%d", &mode_val);
    if(mode_val >= 0) 
        fsync_mode = mode_val;
    else
        fsync_mode = 0;
    SYS_DBG("[AR_DBG] STORE cam_fsync_mode : %d\n", fsync_mode);
	return size;
}

int fsync_sleep = 0;
static ssize_t cam_fsync_sleep_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[AR_DBG] SHOW cam_fsync_sleep : %d\n", fsync_sleep);
	rc = scnprintf(buf, PAGE_SIZE, "%d", fsync_sleep);
	if (rc)
		return rc;
	return 0;

}

static ssize_t cam_fsync_sleep_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int mode_val = -1;
    sscanf(buf, "%d", &mode_val);
    if(mode_val >= 0) 
        fsync_sleep = mode_val;
    else
        fsync_sleep = 0;
    SYS_DBG("[AR_DBG] STORE cam_fsync_sleep : %d\n", fsync_sleep);
	return size;
}

int gdbEnabled = 0;
static ssize_t cam_gdb_enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[AR_DBG] SHOW cam_gdb_enabled : %d\n", gdbEnabled);
	rc = scnprintf(buf, PAGE_SIZE, "%d", gdbEnabled);
	if (rc)
		return rc;
	return 0;
}

static ssize_t cam_gdb_enabled_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int gdb_val = -1;
    sscanf(buf, "%d", &gdb_val);
    if(gdb_val >= 0)
        gdbEnabled = gdb_val;
    else
        gdbEnabled = 0;
    SYS_DBG("[AR_DBG] STORE cam_gdb_enabled : %d\n", gdbEnabled);
	return size;
}

int fpgaEnabled = 1;
static ssize_t cam_fpga_enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[AR_DBG] SHOW cam_fpga_enabled : %d\n", fpgaEnabled);
	rc = scnprintf(buf, PAGE_SIZE, "%d", fpgaEnabled);
	if (rc)
		return rc;
	return 0;
}

static ssize_t cam_fpga_enabled_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int fpga_val = -1;
    sscanf(buf, "%d", &fpga_val);
    if(fpga_val >= 0)
        fpgaEnabled = fpga_val;
    else
        fpgaEnabled = 1;
    SYS_DBG("[AR_DBG] STORE cam_fpga_enabled : %d\n", fpgaEnabled);
	return size;
}

int wtCamCount = 0;
static ssize_t wt_cam_count_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[AR_DBG] SHOW wtCamCount : %d\n", wtCamCount);
	rc = scnprintf(buf, PAGE_SIZE, "%d", wtCamCount);
	if (rc)
		return rc;
	return 0;
}

static ssize_t wt_cam_count_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int camCount = -1;
    sscanf(buf, "%d", &camCount);
    if(camCount >= 0)
        wtCamCount = camCount;
    else
        wtCamCount = 0;
    SYS_DBG("[AR_DBG] STORE wtCamCount : %d\n", wtCamCount);
	return size;
}

#define TEW_SIZE 100 //TOF_MAX_EEPROM_WRITE_SIZE
int tofEepromWriteAddr[TEW_SIZE] = {0};
int tofEepromWriteData[TEW_SIZE] = {0};
int tofEepromWriteAck = 0;
int tofEepromLastWriteSize = 0;

static ssize_t cam_tof_eeprom_write_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0, idx = 0;
	for(idx = 0; idx < tofEepromLastWriteSize; idx++)
	{
		SYS_INFO("[TOF_DBG] SHOW tof_eeprom :[%d / %d]  addr: [0x%04X] data: [0x%02X] Ack[%d]\n",
			(idx+1), tofEepromLastWriteSize, tofEepromWriteAddr[idx], tofEepromWriteData[idx], tofEepromWriteAck);
	}
	
	if(0 == tofEepromWriteAck)
	{
		rc = scnprintf(buf, PAGE_SIZE, "[TOF_EEPRPOM] Write Init! addr: [0x%04X] data: [0x%02X] Ack[%d] Size[%d]\n", 
				tofEepromWriteAddr[0], tofEepromWriteData[0], tofEepromWriteAck, tofEepromLastWriteSize);
	}
	else if(1 == tofEepromWriteAck)
	{
		rc = scnprintf(buf, PAGE_SIZE, "[TOF_EEPRPOM] Write waiting! addr: [0x%04X] data: [0x%02X] Ack[%d] Size[%d]\n", 
				tofEepromWriteAddr[0], tofEepromWriteData[0], tofEepromWriteAck, tofEepromLastWriteSize);
	}
	else if(-1 == tofEepromWriteAck)
	{
		rc = scnprintf(buf, PAGE_SIZE, "[TOF_EEPRPOM] Write failure! addr: [0x%04X] data: [0x%02X] Ack[%d] Size[%d]\n", 
				tofEepromWriteAddr[0], tofEepromWriteData[0], tofEepromWriteAck, tofEepromLastWriteSize);
	}
	else if(2 == tofEepromWriteAck)
	{
		rc = scnprintf(buf, PAGE_SIZE, "[TOF_EEPRPOM] Write success! addr: [0x%04X] data: [0x%02X] Ack[%d] Size[%d]\n", 
				tofEepromWriteAddr[0], tofEepromWriteData[0], tofEepromWriteAck, tofEepromLastWriteSize);
	}
	return rc;
}

static ssize_t cam_tof_eeprom_write_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t size)
{
	int eepromAddr = -1;
	int eepromData = -1;
	int writeIndex = 0 , idx = 0;
	sscanf(buf, "%d 0x%x 0x%x", &writeIndex, &eepromAddr, &eepromData);
	SYS_INFO("[TOF_SCN] idx:[%d], ad:[0x%04X] dt:[0x%02X] ", writeIndex, eepromAddr, eepromData);
	if(writeIndex > 0 && writeIndex < TEW_SIZE && eepromAddr > 0 && eepromData > 0)
	{
		
		tofEepromWriteAddr[writeIndex-1] = eepromAddr;
		tofEepromWriteData[writeIndex-1] = eepromData;
		tofEepromWriteAck = 1;  // Write waiting!
		tofEepromLastWriteSize = writeIndex;
		idx = writeIndex - 1;
		SYS_INFO("[TOF_EEPRPOM] store! addr: [0x%04X] data: [0x%02X] Ack[%d] index[%d / %d]\n",
			tofEepromWriteAddr[idx], tofEepromWriteData[idx], tofEepromWriteAck, (idx+1), tofEepromLastWriteSize);
	}
	else
	{
		for(idx = 0; idx < TEW_SIZE; idx++)
		{
			tofEepromWriteAddr[idx] = 0;
			tofEepromWriteData[idx] = 0;
		}
		tofEepromWriteAck = 0;  // init!
		tofEepromLastWriteSize = 0;
		SYS_INFO("[TOF_EEPRPOM] store!reset ");
	}
	return size;
}

static ssize_t cam_vst_left_temperature_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	uint32_t reg_data1 = 0;
	int rc = 0;
	uint32_t sensor_index = VST_LEFT_CAM;

	if(g_s_ctrl_info[sensor_index] == NULL) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);

		if (SENSOR_ID_IMX564 == slave_info->sensor_id)
		{
			cam_sensor_i2c_read(sensor_index, 0x013A, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			value = reg_data1;
			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data1: 0x%x) in degrees Celcius \n", value, value, reg_data1);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else
		{
			SYS_ERR("[TEMP_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
			rc = scnprintf(buf, PAGE_SIZE, "NG\n");
		}
		
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t cam_vst_right_temperature_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	uint32_t reg_data1 = 0;
	int rc = 0;
	uint32_t sensor_index = VST_RIGHT_CAM;

	if(g_s_ctrl_info[sensor_index] == NULL) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);

		if (SENSOR_ID_IMX564 == slave_info->sensor_id)
		{
			cam_sensor_i2c_read(sensor_index, 0x013A, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			value = reg_data1;
			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data1: 0x%x) in degrees Celcius \n", value, value, reg_data1);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else
		{
			SYS_ERR("[TEMP_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
			rc = scnprintf(buf, PAGE_SIZE, "NG\n");
		}
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t cam_het_front_left_temperature_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	uint32_t reg_data1 = 0;
	int rc = 0;
	uint32_t sensor_index = HET_MID_L_CAM;

	if(g_s_ctrl_info[sensor_index] == NULL) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);

		if ((SENSOR_ID_S5K931 == slave_info->sensor_id) ||
			(SENSOR_ID_S5K931_V2 == slave_info->sensor_id))
		{
			cam_sensor_i2c_read(sensor_index, 0x0020, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
			value = reg_data1 / 256;
			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data1: 0x%x)\n", value, value, reg_data1);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else
		{
			SYS_ERR("[TEMP_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
			rc = scnprintf(buf, PAGE_SIZE, "NG\n");
		}
		
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_temperature_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	int32_t value = 0;
	uint32_t reg_data = 0;
	int rc = 0;
	uint32_t sensor_index = DTOF_SENSOR_CAM;

	if(g_s_ctrl_info[sensor_index] == NULL) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);

		if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
		{
			cam_sensor_i2c_read(sensor_index, 0x0180, &reg_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			value = reg_data - 128U;
			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data: 0x%x)\n", value, value, reg_data);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else
		{
			SYS_ERR("[TEMP_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
			rc = scnprintf(buf, PAGE_SIZE, "NG\n");
		}
		
	}
	if (rc)
		return rc;
	return 0;
}


static ssize_t rear2_tof_temperature_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	uint32_t reg_data = 0;
	uint32_t msb_data = 0;
	uint32_t lsb_data = 0;
	int rc = 0;
	uint32_t sensor_index = ITOF_SENSOR_CAM;

	if(g_s_ctrl_info[sensor_index] == NULL) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);

		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			cam_sensor_i2c_write(sensor_index, 0x602C, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(sensor_index, 0x602E, 0x0020, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 

			cam_sensor_i2c_read(sensor_index, 0x6F12, &reg_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
			msb_data = (reg_data >> 8) & 0x00FF;
			lsb_data = reg_data & 0x00FF;
			value = (msb_data << 8) | (lsb_data);
			value = value / 256;

			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data: 0x%x, msb_data: 0x%x,reg_data >> 8: 0x%x)\n", value, value, reg_data, msb_data , reg_data >> 8);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else
		{
			SYS_ERR("[TEMP_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
			rc = scnprintf(buf, PAGE_SIZE, "NG\n");
		}
		
	}
	if (rc)
		return rc;
	return 0;
}

uint32_t temp_check_sensor = 0;
static ssize_t cam_temperature_check_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = -1;
	sscanf(buf, "%d", &value);
	if(value >= 0) 
		temp_check_sensor = value;
	else
		temp_check_sensor = 0;
	SYS_INFO("[TEMP_DBG] stored sensor id: %d, recived: %d", temp_check_sensor, value);
	return size;
}

static ssize_t cam_temperature_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int32_t value = 0;
	uint32_t reg_data = 0;
	uint32_t msb_data = 0;
	uint32_t lsb_data = 0;
	uint32_t sensor_index = temp_check_sensor;
	struct cam_camera_slave_info *slave_info;

	if(sensor_index >= 20 || g_s_ctrl_info[sensor_index] == NULL) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);

		if ((SENSOR_ID_S5K931 == slave_info->sensor_id) ||
			(SENSOR_ID_S5K931_V2 == slave_info->sensor_id)) {
			cam_sensor_i2c_read(sensor_index, 0x0020, &reg_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
			value = reg_data / 256;
			SYS_ERR("[TEMP_DBG_%d] temp value: 0x%x(%d) (reg_data: 0x%x)\n", sensor_index, value, value, reg_data);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else if (SENSOR_ID_IMX564 == slave_info->sensor_id) {
			cam_sensor_i2c_read(sensor_index, 0x013A, &reg_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			value = reg_data;
			SYS_ERR("[TEMP_DBG_%d] temp value: 0x%x(%d) (reg_data: 0x%x) in degrees Celcius \n", sensor_index, value, value, reg_data);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
		{
			cam_sensor_i2c_read(sensor_index, 0x0180, &reg_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			value = reg_data - 128U;
			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data: 0x%x)\n", value, value, reg_data);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			cam_sensor_i2c_write(sensor_index, 0x602C, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(sensor_index, 0x602E, 0x0020, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 

			cam_sensor_i2c_read(sensor_index, 0x6F12, &reg_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
			msb_data = (reg_data >> 8) & 0x00FF;
			lsb_data = reg_data & 0x00FF;
			value = (msb_data << 8) | (lsb_data);
			value = value / 256;

			SYS_ERR("[TEMP_DBG] temp value: 0x%x(%d) (reg_data: 0x%x, msb_data: 0x%x,reg_data >> 8: 0x%x)\n", value, value, reg_data,msb_data ,reg_data >> 8);
			rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
		}
		else {
			SYS_ERR("[TEMP_DBG_%d] Unsupported sensor id: %d", slave_info, slave_info->sensor_id);
			rc = scnprintf(buf, PAGE_SIZE, "NG\n");
		}
	}
	if (rc)
		return rc;
	return 0;
}


#endif

char cam_load_fw[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t rear_firmware_load_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear_load_fw : %s\n", cam_load_fw);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_load_fw);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_load_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_load_fw, sizeof(cam_load_fw), "%s\n", buf);
	return size;
}

char cal_crc[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t rear_cal_data_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cal_crc : %s\n", cal_crc);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cal_crc);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cal_data_check_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cal_crc, sizeof(cal_crc), "%s", buf);

	return size;
}

char module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module_info : %s\n", module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module_info, sizeof(module_info), "%s", buf);

	return size;
}

char front_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t front_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front_module_info : %s\n", front_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_module_info, sizeof(front_module_info), "%s", buf);

	return size;
}

char front2_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t front2_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front2_module_info : %s\n", front2_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_module_info, sizeof(front2_module_info), "%s", buf);

	return size;
}

char front3_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t front3_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front3_module_info : %s\n", front3_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_module_info, sizeof(front3_module_info), "%s", buf);

	return size;
}

char front4_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t front4_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front4_module_info : %s\n", front4_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front4_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front4_module_info, sizeof(front4_module_info), "%s", buf);

	return size;
}

char front5_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t front5_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] front5_module_info : %s\n", front5_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front5_module_info);
	if (rc)
		return rc;
	return 0;
}
static ssize_t front5_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front5_module_info, sizeof(front5_module_info), "%s", buf);
	return size;
}

char isp_core[10];
static ssize_t rear_isp_core_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if 0// Power binning is used
	char cam_isp_core[] = "0.8V\n";

	return scnprintf(buf, sizeof(cam_isp_core), "%s", cam_isp_core);
#else
	int rc = 0;

	SYS_DBG("[FW_DBG] isp_core : %s\n", isp_core);
	rc = scnprintf(buf, PAGE_SIZE, "%s\n", isp_core);
	if (rc)
		return rc;
	return 0;
#endif
}

#if defined(CONFIG_CAMERA_CDR_TEST)
static ssize_t rear_cam_cdr_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_clock_data_recovery_get_value());
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cam_cdr_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	cam_clock_data_recovery_set_value(buf);

	return size;
}

static ssize_t rear_cam_cdr_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_clock_data_recovery_get_result());
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cam_cdr_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	cam_clock_data_recovery_reset_result(buf);

	return size;
}

char cdr_fastaec[5] = "";
static ssize_t rear_cam_cdr_fastaec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", cdr_fastaec);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cam_cdr_fastaec_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	scnprintf(cdr_fastaec, sizeof(cdr_fastaec), "%s", buf);

	return size;
}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
static int16_t is_hw_param_valid_module_id(char *moduleid)
{
	int i = 0;
	int32_t moduleid_cnt = 0;
	int16_t rc = MODULE_ID_VALID;

	if (moduleid == NULL) {
		SYS_ERR("MI_INVALID\n");
		return MODULE_ID_INVALID;
	}

	for (i = 0; i < FROM_MODULE_ID_SIZE; i++) {
		if (moduleid[i] == '\0') {
			moduleid_cnt = moduleid_cnt + 1;
		} else if ((i < 5)
				&& (!((moduleid[i] > 47 && moduleid[i] < 58)	// 0 to 9
						|| (moduleid[i] > 64 && moduleid[i] < 91)))) { // A to Z
			SYS_ERR("MIR_ERR_1\n");
			rc = MODULE_ID_ERR_CHAR;
			break;
		}
	}

	if (moduleid_cnt == FROM_MODULE_ID_SIZE) {
		SYS_ERR("MIR_ERR_0\n");
		rc = MODULE_ID_ERR_CNT_MAX;
	}

	return rc;
}

extern char hwparam_str[MAX_HW_PARAM_ID][MAX_HW_PARAM_INFO][MAX_HW_PARAM_STR_LEN];
char wifi_info[128] = "\0";

void camera_hw_param_check_avail_cam(void)
{
	struct cam_hw_param *hw_param = NULL;

	SYS_INFO("check available cam");
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR2);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR3);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR4);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR5);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR6);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT2);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT3);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT4);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR_TOF);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR2_TOF);
	hw_param->cam_available = 1;

}

char af_count_value[30] = { 0, };
char af_info_value[30] = { 0, };
char af_info_value_prev[30] = { 0, };

static char* get_af_hall_error_count(char *af_hall_info)
{
	int count_idx = 0;
	int pos = 0;
	int start_idx = 2;
	int break_cnt = sizeof(af_count_value);

	memset(af_count_value, 0, sizeof(af_count_value));
	SYS_INFO(" enter ");
	if (af_hall_info != NULL)
	{
		while (af_hall_info[start_idx+pos] != '|')
		{
			pos++;

			if (start_idx+pos == break_cnt)
				break;
		}
		pos++;

		while (af_hall_info[start_idx+pos] != '\0')
		{
			af_count_value[count_idx] = af_hall_info[start_idx+pos];
			count_idx++;
			pos++;
		}
	}
	return af_count_value;
}

static char* get_af_hall_error_info(char *af_hall_info)
{
	int count_idx = 0;
	int pos = 0;
	int start_idx = 2;
	int break_cnt = sizeof(af_info_value);

	memset(af_info_value, 0, sizeof(af_info_value));
	SYS_INFO(" enter ");
	if (af_hall_info != NULL)
	{
		if (af_hall_info[0] == 'F')
		{
			while (af_hall_info[start_idx+pos] != '|')
			{
				af_info_value[count_idx] = af_hall_info[start_idx+pos];
				count_idx++;
				pos++;
				
				if (start_idx+pos == break_cnt)
					break;
			}
			scnprintf(af_info_value_prev, sizeof(af_info_value_prev), "%s", af_info_value);
		}
		else
		{
			if (!strcmp(af_count_value, "0"))
			{
				scnprintf(af_info_value, sizeof(af_info_value), "%s", "0,0");
			}
			else
			{
				return af_info_value_prev;
			}
		}
	}
	return af_info_value;
}

ssize_t fill_hw_bigdata_sysfs_node(char *buf, struct cam_hw_param *ec_param, char *moduleid, char *af_str, uint32_t hw_param_id)
{
	SYS_INFO(" enter hw_param_id: %d", hw_param_id);
	if (is_hw_param_valid_module_id(moduleid) > 0) {
		return scnprintf(buf, PAGE_SIZE, "\"%s\":\"%c%c%c%c%cXX%02X%02X%02X\",\"%s\":\"%d\",\"%s\":\"%d\","
			"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d,%d,%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"\n",
			hwparam_str[hw_param_id][CAMI_ID], moduleid[0], moduleid[1], moduleid[2], moduleid[3],
			moduleid[4], moduleid[7], moduleid[8], moduleid[9],
			hwparam_str[hw_param_id][I2C_AF], ec_param->err_cnt[I2C_AF_ERROR],
			hwparam_str[hw_param_id][I2C_OIS], ec_param->err_cnt[I2C_OIS_ERROR],
			hwparam_str[hw_param_id][I2C_SEN], ec_param->err_cnt[I2C_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_SEN], ec_param->err_cnt[MIPI_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_INFO], ec_param->rf_rat, ec_param->rf_band, ec_param->rf_channel,
			hwparam_str[hw_param_id][I2C_EEPROM], ec_param->err_cnt[I2C_EEPROM_ERROR],
			hwparam_str[hw_param_id][CRC_EEPROM], ec_param->err_cnt[CRC_EEPROM_ERROR],
			hwparam_str[hw_param_id][CAM_USE_CNT], ec_param->cam_entrance_cnt,
			hwparam_str[hw_param_id][WIFI_INFO], wifi_info,
			hwparam_str[hw_param_id][AF_HALL], get_af_hall_error_count(af_str),
			hwparam_str[hw_param_id][AF_INFO], get_af_hall_error_info(af_str));
	} else {
		return scnprintf(buf, PAGE_SIZE, "\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%d\","
			"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d,%d,%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"\n",
			hwparam_str[hw_param_id][CAMI_ID], ((is_hw_param_valid_module_id(moduleid) == MODULE_ID_ERR_CHAR) ? "MIR_ERR" : "MI_NO"),
			hwparam_str[hw_param_id][I2C_AF], ec_param->err_cnt[I2C_AF_ERROR],
			hwparam_str[hw_param_id][I2C_OIS], ec_param->err_cnt[I2C_OIS_ERROR],
			hwparam_str[hw_param_id][I2C_SEN], ec_param->err_cnt[I2C_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_SEN], ec_param->err_cnt[MIPI_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_INFO], ec_param->rf_rat, ec_param->rf_band, ec_param->rf_channel,
			hwparam_str[hw_param_id][I2C_EEPROM], ec_param->err_cnt[I2C_EEPROM_ERROR],
			hwparam_str[hw_param_id][CRC_EEPROM], ec_param->err_cnt[CRC_EEPROM_ERROR],
			hwparam_str[hw_param_id][CAM_USE_CNT], ec_param->cam_entrance_cnt,
			hwparam_str[hw_param_id][WIFI_INFO], wifi_info,
			hwparam_str[hw_param_id][AF_HALL], get_af_hall_error_count(af_str),
			hwparam_str[hw_param_id][AF_INFO], get_af_hall_error_info(af_str));
	}
}

char hall_info_rear[30] = "N,0,0|0";
static ssize_t rear_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear_module_id, hall_info_rear, HW_PARAM_REAR);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear2_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR2);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear2_module_id, hall_info_rear, HW_PARAM_REAR2);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR2);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear3_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR3);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear3_module_id, hall_info_rear, HW_PARAM_REAR3);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR3);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear4_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR4);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear4_module_id, hall_info_rear, HW_PARAM_REAR4);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR4);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear5_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR5);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear5_module_id, hall_info_rear, HW_PARAM_REAR5);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR5);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear6_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR6);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear6_module_id, hall_info_rear, HW_PARAM_REAR6);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR6);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear7_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR7);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear7_module_id, hall_info_rear, HW_PARAM_REAR7);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear7_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR7);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear8_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR8);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear8_module_id, hall_info_rear, HW_PARAM_REAR8);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear8_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR8);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t front_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, front_module_id, hall_info_rear, HW_PARAM_FRONT);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t front2_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT2);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, front2_module_id, hall_info_rear, HW_PARAM_FRONT2);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT2);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t front3_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT3);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, front3_module_id, hall_info_rear, HW_PARAM_FRONT3);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT3);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t front4_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT4);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, front4_module_id, hall_info_rear, HW_PARAM_FRONT4);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT4);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear_tof_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR_TOF);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear_tof_module_id, hall_info_rear, HW_PARAM_REAR_TOF);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR_TOF);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear2_tof_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;
	SYS_INFO(" enter ");
	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR2_TOF);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, rear_dtof_module_id, hall_info_rear, HW_PARAM_REAR2_TOF);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	SYS_INFO("[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR2_TOF);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t rear_camera_wifi_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_INFO("wifi_info : %s\n", wifi_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", wifi_info);
	if (rc)
		return rc;
	return 0;
}
static ssize_t rear_camera_wifi_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{	
	SYS_INFO("[HWB] buf : %s\n", buf);
	scnprintf(wifi_info, sizeof(wifi_info), "%s", buf);

	return size;
}

static ssize_t common_camera_hw_param_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	int rc = 0, hw_param_id = 0, buf_idx=0, hw_param_idx=0, len=0;
    char hw_param_buf[PAGE_SIZE];
	struct cam_hw_param *ec_param = NULL;

    for(hw_param_id = HW_PARAM_REAR; hw_param_id<=HW_PARAM_REAR8 && buf_idx<(PAGE_SIZE-1); hw_param_id++)
    {
        ec_param = NULL;
        len = 0;
        hw_bigdata_get_hw_param(&ec_param, hw_param_id);
        if(ec_param == NULL)
        {
            continue;
        }
        switch(hw_param_id)
        {
            case HW_PARAM_REAR:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR2:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear2_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR3:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear3_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR4:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear4_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR5:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear5_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR6:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear6_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR7:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear7_module_id, hall_info_rear, hw_param_id);
                break;
            case HW_PARAM_REAR8:
                len = fill_hw_bigdata_sysfs_node(hw_param_buf, ec_param, rear8_module_id, hall_info_rear, hw_param_id);
                break;
            default:
                continue;
                
        }
        hw_param_idx = 0;
        while(hw_param_idx<len && buf_idx<(PAGE_SIZE-1))
        {
            buf[buf_idx] = hw_param_buf[hw_param_idx];
            hw_param_idx++;
            buf_idx++;
        }
        rc+=len;
            
    }
    buf[buf_idx] = '\0';
 
	if (rc)
		return rc;
	return 0;
}

static ssize_t common_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t camera_id = 0;
	kstrtouint(buf, 10, &camera_id);
	SYS_INFO("[SUPPORT_DBG] buf : [%s] camera_id[%d]\n", buf, camera_id);
	if(camera_id < SEC_SENSOR_ID_MAX)
	{
		hw_bigdata_i2c_from_sensor_id(camera_id);
		hw_bigdata_i2c_from_eeprom_id(camera_id);
	}
	return size;
}

#endif

#if defined(CONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE)
static ssize_t rear_af_hall_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", hall_info_rear);
	SYS_DBG("[FW_DBG] hall_info_rear : %s\n", hall_info_rear);
	if (rc)
		return rc;
	return 0;
}
static ssize_t rear_af_hall_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	scnprintf(hall_info_rear, sizeof(hall_info_rear), "%s", buf);
	SYS_DBG("[FW_DBG] hall_info_rear : %s\n", hall_info_rear);
	return size;
}
#endif


static ssize_t rear_isp_core_check_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(isp_core, sizeof(isp_core), "%s", buf);

	return size;
}

char rear_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t rear_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	SYS_DBG("rear_af_cal_str : 10 %s\n", rear_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "10 %s", rear_af_cal_str);
	if (rc)
		return rc;
#else
	SYS_DBG("rear_af_cal_str : 1 %s\n", rear_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "1 %s", rear_af_cal_str);
	if (rc)
		return rc;
#endif

	return 0;
}

char rear_paf_cal_data_far[PAF_2PD_CAL_INFO_SIZE] = {0,};
char rear_paf_cal_data_mid[PAF_2PD_CAL_INFO_SIZE] = {0,};

static ssize_t rear_paf_offset_mid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("rear_paf_cal_data : %s\n", rear_paf_cal_data_mid);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_paf_cal_data_mid);
	if (rc) {
		SYS_DBG("data size %d\n", rc);
		return rc;
	}
	return 0;
}
static ssize_t rear_paf_offset_far_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("rear_paf_cal_data : %s\n", rear_paf_cal_data_far);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_paf_cal_data_far);
	if (rc) {
		SYS_DBG("data size %d\n", rc);
		return rc;
	}
	return 0;
}

uint32_t paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("paf_cal_check : %u\n", paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

char rear_f2_paf_cal_data_far[PAF_2PD_CAL_INFO_SIZE] = {0,};
char rear_f2_paf_cal_data_mid[PAF_2PD_CAL_INFO_SIZE] = {0,};
static ssize_t rear_f2_paf_offset_mid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("rear_f2_paf_cal_data : %s\n", rear_f2_paf_cal_data_mid);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_f2_paf_cal_data_mid);
	if (rc) {
		SYS_DBG("data size %d\n", rc);
		return rc;
	}
	return 0;
}
static ssize_t rear_f2_paf_offset_far_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("rear_f2_paf_cal_data : %s\n", rear_f2_paf_cal_data_far);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_f2_paf_cal_data_far);
	if (rc) {
		SYS_DBG("data size %d\n", rc);
		return rc;
	}
	return 0;
}

uint32_t f2_paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear_f2_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("f2_paf_cal_check : %u\n", f2_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", f2_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

uint32_t rear3_paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear3_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("rear3_paf_err_data_result : %u\n", rear3_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", rear3_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

uint32_t front_paf_err_data_result = 0xFFFFFFFF;
static ssize_t front_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("front_paf_err_data_result : %u\n", front_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", front_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

char front_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t front_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front_cam_fw_ver : %s\n", front_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_ver, sizeof(front_cam_fw_ver), "%s", buf);

	return size;
}

char front_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
static ssize_t front_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front_cam_fw_full_ver : %s\n", front_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_full_ver, sizeof(front_cam_fw_full_ver), "%s", buf);
	return size;
}

char front_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front_cam_fw_user_ver : %s\n", front_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_user_ver, sizeof(front_cam_fw_user_ver), "%s", buf);

	return size;
}

char front_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_fw_ver : %s\n", front_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_factory_ver, sizeof(front_cam_fw_factory_ver), "%s", buf);

	return size;
}

char front2_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t front2_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front2_cam_fw_ver : %s\n", front2_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_ver, sizeof(front2_cam_fw_ver), "%s", buf);

	return size;
}

char front2_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
static ssize_t front2_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front_cam_fw_full_ver : %s\n", front2_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_full_ver, sizeof(front2_cam_fw_full_ver), "%s", buf);
	return size;
}

char front2_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front2_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front2_cam_fw_user_ver : %s\n", front2_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_user_ver, sizeof(front2_cam_fw_user_ver), "%s", buf);

	return size;
}

char front2_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front2_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_fw_ver : %s\n", front2_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_factory_ver, sizeof(front2_cam_fw_factory_ver), "%s", buf);

	return size;
}

char front3_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t front3_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front3_cam_fw_ver : %s\n", front3_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_ver, sizeof(front3_cam_fw_ver), "%s", buf);

	return size;
}

char front4_cam_fw_ver[SYSFS_FW_VER_SIZE] = "\0";
static ssize_t front4_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front4_cam_fw_ver : %s\n", front4_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front4_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front4_cam_fw_ver, sizeof(front4_cam_fw_ver), "%s", buf);

	return size;
}

char front5_cam_fw_ver[SYSFS_FW_VER_SIZE] = "\0";
static ssize_t front5_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] front5_cam_fw_ver : %s\n", front5_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front5_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}
static ssize_t front5_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front5_cam_fw_ver, sizeof(front5_cam_fw_ver), "%s", buf);
	return size;
}

char front3_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
static ssize_t front3_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front3_cam_fw_full_ver : %s\n", front3_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_full_ver, sizeof(front3_cam_fw_full_ver), "%s", buf);
	return size;
}

char front4_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
static ssize_t front4_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front4_cam_fw_full_ver : %s\n", front4_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front4_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front4_cam_fw_full_ver, sizeof(front4_cam_fw_full_ver), "%s", buf);
	return size;
}

char front5_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
static ssize_t front5_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] front5_cam_fw_full_ver : %s\n", front5_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front5_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}
static ssize_t front5_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front5_cam_fw_full_ver, sizeof(front5_cam_fw_full_ver), "%s", buf);
	return size;
}

char front3_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front3_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front3_cam_fw_user_ver : %s\n", front3_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_user_ver, sizeof(front3_cam_fw_user_ver), "%s", buf);

	return size;
}

char front4_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front4_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front4_cam_fw_user_ver : %s\n", front4_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front4_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front4_cam_fw_user_ver, sizeof(front4_cam_fw_user_ver), "%s", buf);

	return size;
}

char front5_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front5_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] front5_cam_fw_user_ver : %s\n", front5_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front5_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}
static ssize_t front5_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front5_cam_fw_user_ver, sizeof(front5_cam_fw_user_ver), "%s", buf);
	return size;
}

char front3_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front3_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front3_cam_fw_factory_ver : %s\n", front3_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_factory_ver, sizeof(front3_cam_fw_factory_ver), "%s", buf);

	return size;
}

char front4_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front4_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front4_cam_fw_factory_ver : %s\n", front4_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front4_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front4_cam_fw_factory_ver, sizeof(front4_cam_fw_factory_ver), "%s", buf);

	return size;
}

char front5_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t front5_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] front5_cam_fw_factory_ver : %s\n", front5_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front5_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}
static ssize_t front5_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(front5_cam_fw_factory_ver, sizeof(front5_cam_fw_factory_ver), "%s", buf);
	return size;
}

char rear_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_cam_info, sizeof(rear_cam_info), "%s", buf);

	return size;
}

char front_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", front_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front_cam_info, sizeof(front_cam_info), "%s", buf);

	return size;
}

char front2_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front2_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", front2_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_cam_info, sizeof(front2_cam_info), "%s", buf);

	return size;
}

char front3_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front3_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front3_cam_info : %s\n", front3_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front3_cam_info, sizeof(front3_cam_info), "%s", buf);

	return size;
}

char front4_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front4_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", front4_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front4_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front4_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front4_cam_info, sizeof(front4_cam_info), "%s", buf);

	return size;
}

char front5_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front5_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam_info : %s\n", front5_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front5_cam_info);
	if (rc)
		return rc;
	return 0;
}
static ssize_t front5_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front5_cam_info, sizeof(front5_cam_info), "%s", buf);
	return size;
}

char front2_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t front2_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front2_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3], front2_module_id[4],
	  front2_module_id[5], front2_module_id[6], front2_module_id[7], front2_module_id[8], front2_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3], front2_module_id[4],
	  front2_module_id[5], front2_module_id[6], front2_module_id[7], front2_module_id[8], front2_module_id[9]);
	return strlen(buf);
}

char front3_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t front3_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front3_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3], front3_module_id[4],
	  front3_module_id[5], front3_module_id[6], front3_module_id[7], front3_module_id[8], front3_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3], front3_module_id[4],
	  front3_module_id[5], front3_module_id[6], front3_module_id[7], front3_module_id[8], front3_module_id[9]);
	return strlen(buf);
}

char front4_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t front4_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front4_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front4_module_id[0], front4_module_id[1], front4_module_id[2], front4_module_id[3], front4_module_id[4],
	  front4_module_id[5], front4_module_id[6], front4_module_id[7], front4_module_id[8], front4_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front4_module_id[0], front4_module_id[1], front4_module_id[2], front4_module_id[3], front4_module_id[4],
	  front4_module_id[5], front4_module_id[6], front4_module_id[7], front4_module_id[8], front4_module_id[9]);
	return strlen(buf);
}

char front5_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t front5_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front5_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front5_module_id[0], front5_module_id[1], front5_module_id[2], front5_module_id[3], front5_module_id[4],
	  front5_module_id[5], front5_module_id[6], front5_module_id[7], front5_module_id[8], front5_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front5_module_id[0], front5_module_id[1], front5_module_id[2], front5_module_id[3], front5_module_id[4],
	  front5_module_id[5], front5_module_id[6], front5_module_id[7], front5_module_id[8], front5_module_id[9]);
	return strlen(buf);
}

uint32_t m_binary_mode = 0; // 1 - Normal , 0 - Factory
static ssize_t binary_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_INFO("[BIN_MODE] mode : [%d]\n", m_binary_mode);
	return scnprintf(buf, PAGE_SIZE, "mode: %d", m_binary_mode);
}

static ssize_t binary_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t bin_mode = 0;
	SYS_INFO("[BIN_MODE] buf : [%s]\n", buf);
	kstrtouint(buf, 10, &bin_mode);
	m_binary_mode = bin_mode;
	return size;
}

uint32_t supported_camera_ids = 0;
static ssize_t supported_camera_ids_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0, offset = 0, cnt = 0; 
	SYS_INFO("[SUPPORT_DBG] buf : [%s] supported_camera_ids: [%d] mode: [%d]\n", buf, supported_camera_ids, m_binary_mode);
	for (i = 0; i < supported_camera_ids; i++)
	{
		if(1 == m_binary_mode && 1 == i) // Skip front camera support in Normal Binary
		{
			continue;
		}
		if (supported_camera_ids == i + 1)
			cnt = scnprintf(buf + offset, PAGE_SIZE - offset, "%d\n", i);
		else
			cnt = scnprintf(buf + offset, PAGE_SIZE - offset, "%d ", i);
		offset += cnt;
	}
	return offset;
}

static ssize_t supported_camera_ids_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t num_camera_ids = 0;
	SYS_INFO("[SUPPORT_DBG] buf : [%s]\n", buf);
	kstrtouint(buf, 10, &num_camera_ids);
	if (num_camera_ids >= MAX_SENSOR_INDEX)
		return -EINVAL;
	supported_camera_ids = num_camera_ids;

	return size;
}

uint32_t dtof_pd_value = 0;
static ssize_t dtof_pd_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_INFO("[SUPPORT_DBG] buf : [%s] dtof_pd_value: [%d]\n", buf, dtof_pd_value);

	return 0;
}

static ssize_t dtof_pd_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t pd_value = 0;
	SYS_INFO("[SUPPORT_DBG] buf : [%s]\n", buf);
	kstrtouint(buf, 10, &pd_value);
	dtof_pd_value = pd_value;

	return size;
}

char rear_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_sensor_id : %s\n", rear_sensor_id);
	memcpy(buf, rear_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_sensor_id, sizeof(rear_sensor_id), "%s", buf);

	return size;
}

char front_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front_sensor_id : %s\n", front_sensor_id);
	memcpy(buf, front_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front_sensor_id, sizeof(front_sensor_id), "%s", buf);

	return size;
}

char front2_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front2_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front2_sensor_id : %s\n", front2_sensor_id);
	memcpy(buf, front2_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front2_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_sensor_id, sizeof(front2_sensor_id), "%s", buf);

	return size;
}

char front3_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front3_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front3_sensor_id : %s\n", front3_sensor_id);
	memcpy(buf, front3_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front3_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front3_sensor_id, sizeof(front3_sensor_id), "%s", buf);

	return size;
}

char front4_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front4_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front4_sensor_id : %s\n", front4_sensor_id);
	memcpy(buf, front4_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front4_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front4_sensor_id, sizeof(front4_sensor_id), "%s", buf);

	return size;
}

char front5_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front5_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front5_sensor_id : %s\n", front5_sensor_id);
	memcpy(buf, front5_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}
static ssize_t front5_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front5_sensor_id, sizeof(front5_sensor_id), "%s", buf);
	return size;
}

#define FROM_MTF_SIZE 54
char front_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front_mtf_exif : %s\n", front_mtf_exif);
	memcpy(buf, front_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t front_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front_mtf_exif, sizeof(front_mtf_exif), "%s", buf);

	return size;
}

char front2_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front2_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front2_mtf_exif : %s\n", front2_mtf_exif);
	memcpy(buf, front2_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t front2_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_mtf_exif, sizeof(front2_mtf_exif), "%s", buf);

	return size;
}

char front3_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front3_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front3_mtf_exif : %s\n", front3_mtf_exif);
	memcpy(buf, front3_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t front3_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front3_mtf_exif, sizeof(front3_mtf_exif), "%s", buf);

	return size;
}

char front4_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front4_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front4_mtf_exif : %s\n", front4_mtf_exif);
	memcpy(buf, front4_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t front4_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front4_mtf_exif, sizeof(front4_mtf_exif), "%s", buf);

	return size;
}

char front5_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front5_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front5_mtf_exif : %s\n", front5_mtf_exif);
	memcpy(buf, front5_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}
static ssize_t front5_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front5_mtf_exif, sizeof(front5_mtf_exif), "%s", buf);
	return size;
}

char rear_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_mtf_exif : %s\n", rear_mtf_exif);
	memcpy(buf, rear_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_mtf_exif, sizeof(rear_mtf_exif), "%s", buf);

	return size;
}

char rear_mtf2_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear_mtf2_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_mtf2_exif : %s\n", rear_mtf2_exif);
	memcpy(buf, rear_mtf2_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear_mtf2_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_mtf2_exif, sizeof(rear_mtf2_exif), "%s", buf);

	return size;
}

char rear_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
	  rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
	  rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
	return strlen(buf);
}

char front_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t front_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] front_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3], front_module_id[4],
	  front_module_id[5], front_module_id[6], front_module_id[7], front_module_id[8], front_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3], front_module_id[4],
	  front_module_id[5], front_module_id[6], front_module_id[7], front_module_id[8], front_module_id[9]);
	return strlen(buf);
}

#define SSRM_CAMERA_INFO_SIZE 256
char ssrm_camera_info[SSRM_CAMERA_INFO_SIZE + 1] = "\0";
static ssize_t ssrm_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_INFO("ssrm_camera_info : %s\n", ssrm_camera_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", ssrm_camera_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ssrm_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_INFO("ssrm_camera_info buf : %s\n", buf);
	scnprintf(ssrm_camera_info, sizeof(ssrm_camera_info), "%s", buf);

	return size;
}

char cam3_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear3_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam3_fw_ver : %s\n", cam3_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam3_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam3_fw_ver, sizeof(cam3_fw_ver), "%s", buf);

	return size;
}

char cam3_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear3_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam3_fw_full_ver : %s\n", cam3_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam3_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam3_fw_full_ver, sizeof(cam3_fw_full_ver), "%s", buf);

	return size;
}

char rear3_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t rear3_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	SYS_DBG("rear3_af_cal_str : 10 %s\n", rear3_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "10 %s", rear3_af_cal_str);
	if (rc)
		return rc;
#else
	SYS_DBG("rear3_af_cal_str : 1 %s\n", rear3_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "1 %s", rear3_af_cal_str);
	if (rc)
		return rc;
#endif

	return 0;
}

char rear3_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear3_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear3_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear3_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear3_cam_info, sizeof(rear3_cam_info), "%s", buf);

	return size;
}

static ssize_t rear3_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear3_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear3_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear3_mtf_exif : %s\n", rear3_mtf_exif);
	memcpy(buf, rear3_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear3_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear3_mtf_exif, sizeof(rear3_mtf_exif), "%s", buf);

	return size;
}

char rear3_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear3_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear3_sensor_id : %s\n", rear3_sensor_id);
	memcpy(buf, rear3_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear3_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear3_sensor_id, sizeof(rear3_sensor_id), "%s", buf);

	return size;
}

#define FROM_REAR_DUAL_CAL_SIZE 2060
uint8_t rear3_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear3_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear3_dual_cal : %s\n", rear3_dual_cal);

	if (FROM_REAR_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = FROM_REAR_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear3_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;

}

uint32_t rear3_dual_cal_size = FROM_REAR_DUAL_CAL_SIZE;
static ssize_t rear3_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear3_dual_cal_size : %d\n", rear3_dual_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear3_dual_cal_size);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t rear3_dual;
static ssize_t rear3_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] front dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s\n",
		rear3_dual.x, rear3_dual.y, rear3_dual.z, rear3_dual.sx, rear3_dual.sy,
		rear3_dual.range, rear3_dual.max_err, rear3_dual.avg_err, rear3_dual.dll_ver, rear3_dual.project_cal_type);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d %s\n", rear3_dual.x, rear3_dual.y,
			rear3_dual.z, rear3_dual.sx, rear3_dual.sy, rear3_dual.range,
			rear3_dual.max_err, rear3_dual.avg_err, rear3_dual.dll_ver, rear3_dual.project_cal_type);
	if (rc)
		return rc;
	return 0;
}

char rear3_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear3_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear3_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3], rear3_module_id[4],
	  rear3_module_id[5], rear3_module_id[6], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3], rear3_module_id[4],
	  rear3_module_id[5], rear3_module_id[6], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9]);
	return strlen(buf);
}

char module3_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear3_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module3_info : %s\n", module3_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module3_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module3_info, sizeof(module3_info), "%s", buf);

	return size;
}

char rear2_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear2_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear2_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear2_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear2_cam_info, sizeof(rear2_cam_info), "%s", buf);

	return size;
}

char rear2_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear2_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear2_mtf_exif : %s\n", rear2_mtf_exif);
	memcpy(buf, rear2_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear2_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear2_mtf_exif, sizeof(rear2_mtf_exif), "%s", buf);

	return size;
}

char rear2_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear2_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear2_sensor_id : %s\n", rear2_sensor_id);
	memcpy(buf, rear2_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear2_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear2_sensor_id, sizeof(rear2_sensor_id), "%s", buf);

	return size;
}

char module2_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear2_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module2_info : %s\n", module2_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module2_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module2_info, sizeof(module2_info), "%s", buf);

	return size;
}

char rear2_module_id[FROM_MODULE_ID_SIZE + 1]  = {'N', 'G', '\0'};
static ssize_t rear2_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear2_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3], rear2_module_id[4],
	  rear2_module_id[5], rear2_module_id[6], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3], rear2_module_id[4],
	  rear2_module_id[5], rear2_module_id[6], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9]);
	return strlen(buf);
}

char cam2_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear2_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam2_fw_ver : %s\n", cam2_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam2_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam2_fw_ver, sizeof(cam2_fw_ver), "%s", buf);

	return size;
}


char cam2_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear2_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam2_fw_user_ver : %s\n", cam2_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam2_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam2_fw_user_ver, sizeof(cam2_fw_user_ver), "%s", buf);

	return size;
}

char cam2_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear2_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam2_fw_factory_ver : %s\n", cam2_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam2_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam2_fw_factory_ver, sizeof(cam2_fw_factory_ver), "%s", buf);

	return size;
}

char cam2_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear2_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam2_fw_full_ver : %s\n", cam2_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam2_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam2_fw_full_ver, sizeof(cam2_fw_full_ver), "%s", buf);

	return size;
}

uint8_t rear2_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear2_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear2_dual_cal : %s\n", rear2_dual_cal);

	if (FROM_REAR_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = FROM_REAR_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear2_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;

}

uint32_t rear2_dual_cal_size = FROM_REAR_DUAL_CAL_SIZE;
static ssize_t rear2_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_INFO("[FW_DBG] rear2_dual_cal_size : %d\n", rear2_dual_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear2_dual_cal_size);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t rear2_dual;
static ssize_t rear2_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear2 dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s\n",
		rear2_dual.x, rear2_dual.y, rear2_dual.z, rear2_dual.sx, rear2_dual.sy,
		rear2_dual.range, rear2_dual.max_err, rear2_dual.avg_err, rear2_dual.dll_ver, rear2_dual.project_cal_type);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d %s\n", rear2_dual.x, rear2_dual.y,
			rear2_dual.z, rear2_dual.sx, rear2_dual.sy, rear2_dual.range,
			rear2_dual.max_err, rear2_dual.avg_err, rear2_dual.dll_ver, rear2_dual.project_cal_type);
	if (rc)
		return rc;
	return 0;
}


static ssize_t rear2_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SONY_IMX564\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear4_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear4_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear4_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear4_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear4_cam_info, sizeof(rear4_cam_info), "%s", buf);

	return size;
}

char rear4_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear4_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear4_mtf_exif : %s\n", rear4_mtf_exif);
	memcpy(buf, rear4_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear4_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear4_mtf_exif, sizeof(rear4_mtf_exif), "%s", buf);

	return size;
}

char rear4_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear4_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear4_sensor_id : %s\n", rear4_sensor_id);
	memcpy(buf, rear4_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear4_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear4_sensor_id, sizeof(rear4_sensor_id), "%s", buf);

	return size;
}

char module4_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear4_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module4_info : %s\n", module4_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module4_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module4_info, sizeof(module4_info), "%s", buf);

	return size;
}

char rear4_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear4_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear4_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear4_module_id[0], rear4_module_id[1], rear4_module_id[2], rear4_module_id[3], rear4_module_id[4],
	  rear4_module_id[5], rear4_module_id[6], rear4_module_id[7], rear4_module_id[8], rear4_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear4_module_id[0], rear4_module_id[1], rear4_module_id[2], rear4_module_id[3], rear4_module_id[4],
	  rear4_module_id[5], rear4_module_id[6], rear4_module_id[7], rear4_module_id[8], rear4_module_id[9]);
	return strlen(buf);
}

char cam4_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear4_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam4_fw_ver : %s\n", cam4_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam4_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam4_fw_ver, sizeof(cam4_fw_ver), "%s", buf);

	return size;
}


char cam4_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear4_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam4_fw_user_ver : %s\n", cam4_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam4_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam4_fw_user_ver, sizeof(cam4_fw_user_ver), "%s", buf);

	return size;
}

char cam4_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear4_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam4_fw_factory_ver : %s\n", cam4_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam4_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam4_fw_factory_ver, sizeof(cam4_fw_factory_ver), "%s", buf);

	return size;
}

char cam4_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear4_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam4_fw_full_ver : %s\n", cam4_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam4_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam4_fw_full_ver, sizeof(cam4_fw_full_ver), "%s", buf);

	return size;
}

static ssize_t rear4_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear5_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear5_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear5_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear5_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear5_cam_info, sizeof(rear5_cam_info), "%s", buf);

	return size;
}

char rear5_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear5_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear5_mtf_exif : %s\n", rear5_mtf_exif);
	memcpy(buf, rear5_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear5_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear5_mtf_exif, sizeof(rear5_mtf_exif), "%s", buf);

	return size;
}

char rear5_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear5_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear5_sensor_id : %s\n", rear5_sensor_id);
	memcpy(buf, rear5_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear5_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear5_sensor_id, sizeof(rear5_sensor_id), "%s", buf);

	return size;
}

char module5_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear5_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module5_info : %s\n", module5_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module5_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module5_info, sizeof(module5_info), "%s", buf);

	return size;
}

char rear5_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear5_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear5_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear5_module_id[0], rear5_module_id[1], rear5_module_id[2], rear5_module_id[3], rear5_module_id[4],
	  rear5_module_id[5], rear5_module_id[6], rear5_module_id[7], rear5_module_id[8], rear5_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear5_module_id[0], rear5_module_id[1], rear5_module_id[2], rear5_module_id[3], rear5_module_id[4],
	  rear5_module_id[5], rear5_module_id[6], rear5_module_id[7], rear5_module_id[8], rear5_module_id[9]);
	return strlen(buf);
}

char cam5_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear5_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam5_fw_ver : %s\n", cam5_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam5_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam5_fw_ver, sizeof(cam5_fw_ver), "%s", buf);

	return size;
}


char cam5_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear5_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam5_fw_user_ver : %s\n", cam5_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam5_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam5_fw_user_ver, sizeof(cam5_fw_user_ver), "%s", buf);

	return size;
}

char cam5_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear5_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam5_fw_factory_ver : %s\n", cam5_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam5_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam5_fw_factory_ver, sizeof(cam5_fw_factory_ver), "%s", buf);

	return size;
}

char cam5_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear5_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam5_fw_full_ver : %s\n", cam5_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam5_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear5_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam5_fw_full_ver, sizeof(cam5_fw_full_ver), "%s", buf);

	return size;
}

static ssize_t rear5_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear6_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear6_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear6_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear6_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear6_cam_info, sizeof(rear6_cam_info), "%s", buf);

	return size;
}

char rear6_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear6_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear6_mtf_exif : %s\n", rear6_mtf_exif);
	memcpy(buf, rear6_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear6_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear6_mtf_exif, sizeof(rear6_mtf_exif), "%s", buf);

	return size;
}

char rear6_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear6_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear6_sensor_id : %s\n", rear6_sensor_id);
	memcpy(buf, rear6_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear6_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear6_sensor_id, sizeof(rear6_sensor_id), "%s", buf);

	return size;
}

char module6_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear6_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module6_info : %s\n", module6_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module6_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module6_info, sizeof(module6_info), "%s", buf);

	return size;
}

char rear6_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear6_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear6_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear6_module_id[0], rear6_module_id[1], rear6_module_id[2], rear6_module_id[3], rear6_module_id[4],
	  rear6_module_id[5], rear6_module_id[6], rear6_module_id[7], rear6_module_id[8], rear6_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear6_module_id[0], rear6_module_id[1], rear6_module_id[2], rear6_module_id[3], rear6_module_id[4],
	  rear6_module_id[5], rear6_module_id[6], rear6_module_id[7], rear6_module_id[8], rear6_module_id[9]);
	return strlen(buf);
}

char cam6_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear6_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam6_fw_ver : %s\n", cam6_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam6_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam6_fw_ver, sizeof(cam6_fw_ver), "%s", buf);

	return size;
}


char cam6_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear6_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam6_fw_user_ver : %s\n", cam6_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam6_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam6_fw_user_ver, sizeof(cam6_fw_user_ver), "%s", buf);

	return size;
}

char cam6_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear6_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam6_fw_factory_ver : %s\n", cam6_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam6_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam6_fw_factory_ver, sizeof(cam6_fw_factory_ver), "%s", buf);

	return size;
}

char cam6_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear6_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam6_fw_full_ver : %s\n", cam6_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam6_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear6_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam6_fw_full_ver, sizeof(cam6_fw_full_ver), "%s", buf);

	return size;
}

static ssize_t rear6_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char cam7_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear7_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam7_fw_ver : %s\n", cam7_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam7_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear7_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam7_fw_ver, sizeof(cam7_fw_ver), "%s", buf);

	return size;
}

char cam7_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear7_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam7_fw_full_ver : %s\n", cam7_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam7_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear7_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam7_fw_full_ver, sizeof(cam7_fw_full_ver), "%s", buf);

	return size;
}

char rear7_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear7_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear7_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear7_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear7_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear7_cam_info, sizeof(rear7_cam_info), "%s", buf);

	return size;
}

static ssize_t rear7_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear7_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear7_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear7_mtf_exif : %s\n", rear7_mtf_exif);
	memcpy(buf, rear7_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear7_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear7_mtf_exif, sizeof(rear7_mtf_exif), "%s", buf);

	return size;
}

char rear7_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear7_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear7_sensor_id : %s\n", rear7_sensor_id);
	memcpy(buf, rear7_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear7_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear7_sensor_id, sizeof(rear7_sensor_id), "%s", buf);

	return size;
}

char rear7_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear7_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear7_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear7_module_id[0], rear7_module_id[1], rear7_module_id[2], rear7_module_id[3], rear7_module_id[4],
	  rear7_module_id[5], rear7_module_id[6], rear7_module_id[7], rear7_module_id[8], rear7_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear7_module_id[0], rear7_module_id[1], rear7_module_id[2], rear7_module_id[3], rear7_module_id[4],
	  rear7_module_id[5], rear7_module_id[6], rear7_module_id[7], rear7_module_id[8], rear7_module_id[9]);
	return strlen(buf);
}

char module7_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear7_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module7_info : %s\n", module7_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module7_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear7_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module7_info, sizeof(module7_info), "%s", buf);

	return size;
}


char cam8_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear8_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam8_fw_ver : %s\n", cam8_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam8_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear8_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam8_fw_ver, sizeof(cam8_fw_ver), "%s", buf);

	return size;
}

char cam8_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear8_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam8_fw_full_ver : %s\n", cam8_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam8_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear8_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam8_fw_full_ver, sizeof(cam8_fw_full_ver), "%s", buf);

	return size;
}

char rear8_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear8_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear8_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear8_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear8_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear8_cam_info, sizeof(rear8_cam_info), "%s", buf);

	return size;
}

static ssize_t rear8_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K931\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear8_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear8_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear8_mtf_exif : %s\n", rear8_mtf_exif);
	memcpy(buf, rear8_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear8_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear8_mtf_exif, sizeof(rear8_mtf_exif), "%s", buf);

	return size;
}

char rear8_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear8_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear8_sensor_id : %s\n", rear8_sensor_id);
	memcpy(buf, rear8_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear8_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear8_sensor_id, sizeof(rear8_sensor_id), "%s", buf);

	return size;
}

char rear8_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear8_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear8_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear8_module_id[0], rear8_module_id[1], rear8_module_id[2], rear8_module_id[3], rear8_module_id[4],
	  rear8_module_id[5], rear8_module_id[6], rear8_module_id[7], rear8_module_id[8], rear8_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear8_module_id[0], rear8_module_id[1], rear8_module_id[2], rear8_module_id[3], rear8_module_id[4],
	  rear8_module_id[5], rear8_module_id[6], rear8_module_id[7], rear8_module_id[8], rear8_module_id[9]);
	return strlen(buf);
}

char module8_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear8_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] module8_info : %s\n", module8_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module8_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear8_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(module8_info, sizeof(module8_info), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_REAR_TOF)
char rear_tof_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear_tof_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear_tof_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_tof_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_tof_cam_info, sizeof(rear_tof_cam_info), "%s", buf);

	return size;
}

char rear_tof_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear_tof_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_tof_mtf_exif : %s\n", rear_tof_mtf_exif);
	memcpy(buf, rear_tof_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear_tof_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_tof_mtf_exif, sizeof(rear_tof_mtf_exif), "%s", buf);

	return size;
}

char rear_tof_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear_tof_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_tof_sensor_id : %s\n", rear_tof_sensor_id);
	memcpy(buf, rear_tof_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear_tof_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_tof_sensor_id, sizeof(rear_tof_sensor_id), "%s", buf);

	return size;
}

char rear_tof_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear_tof_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear_tof_module_info : %s\n", rear_tof_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_tof_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_tof_module_info, sizeof(rear_tof_module_info), "%s", buf);

	return size;
}

char rear_tof_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear_tof_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear_tof_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_tof_module_id[0], rear_tof_module_id[1], rear_tof_module_id[2], rear_tof_module_id[3], rear_tof_module_id[4],
	  rear_tof_module_id[5], rear_tof_module_id[6], rear_tof_module_id[7], rear_tof_module_id[8], rear_tof_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_tof_module_id[0], rear_tof_module_id[1], rear_tof_module_id[2], rear_tof_module_id[3], rear_tof_module_id[4],
	  rear_tof_module_id[5], rear_tof_module_id[6], rear_tof_module_id[7], rear_tof_module_id[8], rear_tof_module_id[9]);
	return strlen(buf);
}

char cam_tof_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear_tof_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam_tof_fw_ver : %s\n", cam_tof_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_tof_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_tof_fw_ver, sizeof(cam_tof_fw_ver), "%s", buf);

	return size;
}

char cam_tof_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear_tof_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_tof_fw_user_ver : %s\n", cam_tof_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_tof_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_tof_fw_user_ver, sizeof(cam_tof_fw_user_ver), "%s", buf);

	return size;
}

char cam_tof_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear_tof_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_tof_fw_factory_ver : %s\n", cam_tof_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_tof_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_tof_fw_factory_ver, sizeof(cam_tof_fw_factory_ver), "%s", buf);

	return size;
}

char cam_tof_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear_tof_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_tof_fw_full_ver : %s\n", cam_tof_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_tof_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_tof_fw_full_ver, sizeof(cam_tof_fw_full_ver), "%s", buf);

	return size;
}

static ssize_t rear_tof_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SLSI_S5K63D\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char cam_tof_load_fw[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t rear_tof_firmware_load_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear_tof_load_fw : %s\n", cam_tof_load_fw);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_tof_load_fw);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_firmware_load_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_tof_load_fw, sizeof(cam_tof_load_fw), "%s\n", buf);
	return size;
}

int rear_tof_uid;
static ssize_t rear_tof_cal_uid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    int rc= 0;

	SYS_DBG("[FW_DBG] rear tof uid = %d\n", rear_tof_uid);

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", rear_tof_uid);
	if (rc)
		return rc;
	return 0;
}

uint8_t rear_tof_cal[TOFCAL_SIZE + 1] = "\0";
static ssize_t rear_tof_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear_tof_cal : %s\n", rear_tof_cal);

	if (TOFCAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = TOFCAL_SIZE;

	ret = memcpy(buf, rear_tof_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;
}

uint8_t rear_tof_cal_extra[TOFCAL_EXTRA_SIZE + 1] = "\0";
static ssize_t rear_tof_cal_extra_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear_tof_cal_extra : %s\n", rear_tof_cal_extra);

	if (TOFCAL_EXTRA_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = TOFCAL_EXTRA_SIZE;

	ret = memcpy(buf, rear_tof_cal_extra, copy_size);

	if (ret)
		return copy_size;

	return 0;
}


uint32_t rear_tof_cal_size = TOFCAL_TOTAL_SIZE;
static ssize_t rear_tof_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_INFO("[FW_DBG] rear_tof_cal_size : %d\n", rear_tof_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear_tof_cal_size);
	if (rc)
		return rc;
	return 0;
}

uint8_t rear_tof_cal_result = 0;
static ssize_t rear_tof_cal_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if (rear_tof_cal_result == 1)
		rc = scnprintf(buf, PAGE_SIZE, "%s", "OK\n");
	else
		rc = scnprintf(buf, PAGE_SIZE, "%s", "NG\n");
	if (rc)
		return rc;
	return 0;
}

uint8_t rear_tof_dual_cal[REAR_TOF_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear_tof_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear_tof_dual_cal : %s\n", rear_tof_dual_cal);

	if (REAR_TOF_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = REAR_TOF_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear_tof_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;
}

DualTilt_t rear_tof_dual;
static ssize_t rear_tof_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear tof tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d\n",
		rear_tof_dual.x, rear_tof_dual.y, rear_tof_dual.z, rear_tof_dual.sx, rear_tof_dual.sy,
		rear_tof_dual.range, rear_tof_dual.max_err, rear_tof_dual.avg_err, rear_tof_dual.dll_ver);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d\n", rear_tof_dual.x, rear_tof_dual.y,
			rear_tof_dual.z, rear_tof_dual.sx, rear_tof_dual.sy, rear_tof_dual.range,
			rear_tof_dual.max_err, rear_tof_dual.avg_err, rear_tof_dual.dll_ver);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t rear_dtof_dual;
static ssize_t rear2_tof_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear tof tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d\n",
		rear_dtof_dual.x, rear_dtof_dual.y, rear_dtof_dual.z, rear_dtof_dual.sx, rear_dtof_dual.sy,
		rear_dtof_dual.range, rear_dtof_dual.max_err, rear_dtof_dual.avg_err, rear_dtof_dual.dll_ver);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d\n", rear_dtof_dual.x, rear_dtof_dual.y,
			rear_dtof_dual.z, rear_dtof_dual.sx, rear_dtof_dual.sy, rear_dtof_dual.range,
			rear_dtof_dual.max_err, rear_dtof_dual.avg_err, rear_dtof_dual.dll_ver);
	if (rc)
		return rc;
	return 0;
}

int rear_tof_ld_value = 0;
static ssize_t rear2_tof_ld_onoff_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	int rc = 0, value = -1;
	uint32_t sensor_index = ITOF_SENSOR_CAM;
	if(g_s_ctrl_info[sensor_index] != NULL)
	{
		slave_info = &(g_s_ctrl_info[sensor_index]->sensordata->slave_info);
		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			value = rear_tof_ld_value;
			SYS_INFO("TOF_DBG rear2_tof_ld_value : %d \n", value);
		}
	}
	if( value != -1)
	{
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
	}
	else 
	{
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	}
	return rc;
}

static ssize_t rear2_tof_ld_onoff_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	int value = -1;
	int write_value = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. TOF Not running\n");
		return -1;
	}
	
	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);


	if (((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id)) &&
		((value == 0) ||
		(value == 1)))
	{
		rear_tof_ld_value = value;
		write_value = (value == 1)?0x8000:0;
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //page setup
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0xFB3A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, write_value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		SYS_INFO("ITOF ld on/off %d\n",value);
	} else {
		SYS_ERR("ITOF ld onoff not support, value : %d, slave_info->sensor_id : %d\n",value ,slave_info->sensor_id);
	}

	return size;
}

static ssize_t rear2_tof_ae_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t ae_value = 0;
	uint32_t reg_data3 = 0, reg_data2 = 0, reg_data1 = 0, reg_data0 = 0;
	int rc = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL)
		return 0;

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x2124, &reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x2125, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x2126, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x2127, &reg_data0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
	} else {
		SYS_INFO("TOF AE support not added\n");
	}

	SYS_INFO("value 0x%x reg_data3: 0x%2x, addr2: 0x%2x, addr1: 0x%2x, addr0: 0x%2x\n", ae_value, reg_data3,reg_data2, reg_data1,reg_data0);

	ae_value = (reg_data3<<24)|(reg_data2<<16)|(reg_data1<<8)|reg_data0;

	SYS_INFO("ae_value: 0x%x\n", ae_value);

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", ae_value/120);

	if (rc)
		return rc;

	return 0;
}

static ssize_t rear2_tof_ae_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	uint32_t reg_data3 = 0, reg_data2 = 0, reg_data1 = 0, reg_data0 = 0;
	uint32_t hmax = 0x02FE;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL)
		return 0;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	SYS_INFO("ae_value: 0x%x\n", value);

	value  = value * 120; // 120Mhz

	if(value >= hmax && value <= 0x1D4C0)
	{
		slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);
		reg_data3 = (value & 0xFF000000) >> 24;
		reg_data2 = (value & 0xFF0000) >> 16;
		reg_data1 = (value & 0xFF00) >> 8;
		reg_data0 = value & 0xFF;

		//SYS_INFO("value 0x%x reg_data3: 0x%2x, addr2: 0x%2x, addr1: 0x%2x, addr0: 0x%2x\n", value, reg_data3,reg_data2, reg_data1,reg_data0);
		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2124, reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2125, reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2126, reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2127, reg_data0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2128, reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2129, reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x212A, reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x212B, reg_data0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		} else {
			SYS_INFO("TOF AE support not added\n");
		}
	}

	return size;
}

static ssize_t rear2_tof_check_pd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0, value_A = 0, value_B = 0, retry_count = PD_RETRY_COUNT;
	uint32_t reg_data1, reg_data2 = 0;
	int rc = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if(check_pd_ready == 0) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			while(retry_count && !value_A)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_A = (reg_data1 & 0x30) >> 4;
				value_A = (value_A << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd value_A: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_A, value_A, reg_data1, reg_data2);
				retry_count--;
			}
			if(value_A){

				retry_count = PD_RETRY_COUNT;
				reg_data1 = 0, reg_data2 = 0;
				while(retry_count && !value_B)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_B = (reg_data1 & 0x30) >> 4;
				value_B = (value_B << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd value_B: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_B, value_B, reg_data1, reg_data2);
				retry_count--;
			}

			}
		}
		else
		{
			SYS_ERR("[TOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		}
		if(value_A || value_B)
			value = (value_A > value_B) ? value_A : value_B;
			
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_check_pd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
	{
		SYS_ERR("Please enter valid input\n");
		return -1;
	}

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		SYS_INFO("[TOF_DBG] PD store value: 0x%x(%d), ready: %d\n", value, value, check_pd_ready);
		if (check_pd_ready == 0)
			check_pd_ready = 1;

		//page setup - start
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		//page setup - end

		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x55E6, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //start address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //number of write bytes
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0008, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //register address of VCSEL Driver
		msleep(10);

		//data for 0x08 (0xE1 is 2700mA, 1st Current : 0xE1  (225*12=2700mA) 2nd current : 0x7D (125*12=1500mA))
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  
		msleep(223);
		
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x00FF, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Delay command
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x000F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Data for Delay, 13.3us
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Terminates command array
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5626, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //end address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // data at end address
		msleep(10);
		
	}
	else
	{
		SYS_INFO("[TOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear2_tof_check_pd_2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0, value_A = 0, value_B = 0, retry_count = PD_RETRY_COUNT;
	uint32_t reg_data1, reg_data2 = 0;
	int rc = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if(check_pd_ready == 0) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			while(retry_count && !value_A)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_A = (reg_data1 & 0x30) >> 4;
				value_A = (value_A << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd_2 value_A: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_A, value_A, reg_data1, reg_data2);
				retry_count--;
			}
			if(value_A){

				retry_count = PD_RETRY_COUNT;
				reg_data1 = 0, reg_data2 = 0;
				while(retry_count && !value_B)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_B = (reg_data1 & 0x30) >> 4;
				value_B = (value_B << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd_2 value_B: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_B, value_B, reg_data1, reg_data2);
				retry_count--;
			}

			}
		}
		else
		{
			SYS_ERR("[TOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		}
		if(value_A || value_B)
			value = (value_A > value_B) ? value_A : value_B;
			
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_check_pd_2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
	{
		SYS_ERR("Please enter valid input\n");
		return -1;
	}

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		SYS_INFO("[TOF_DBG] PD_2 store value: 0x%x(%d), ready: %d\n", value, value, check_pd_ready);
		if (check_pd_ready == 0)
			check_pd_ready = 1;

		//page setup - start
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		//page setup - end

		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x55E6, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //start address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //number of write bytes
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0008, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //register address of VCSEL Driver
		msleep(10);

		//data for 0x08 (0xE1 is 2700mA, 1st Current : 0xE1  (225*12=2700mA) 2nd current : 0x7D (125*12=1500mA))
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  
		msleep(223);
		
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x00FF, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Delay command
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x000F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Data for Delay, 13.3us
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Terminates command array
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5626, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //end address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // data at end address
		msleep(10);
		
	}
	else
	{
		SYS_INFO("[TOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear2_tof_check_pd_3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0, value_A = 0, value_B = 0, retry_count = PD_RETRY_COUNT;
	uint32_t reg_data1, reg_data2 = 0;
	int rc = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if(check_pd_ready == 0) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			while(retry_count && !value_A)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_A = (reg_data1 & 0x30) >> 4;
				value_A = (value_A << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd_3 value_A: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_A, value_A, reg_data1, reg_data2);
				retry_count--;
			}
			if(value_A){

				retry_count = PD_RETRY_COUNT;
				reg_data1 = 0, reg_data2 = 0;
				while(retry_count && !value_B)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_B = (reg_data1 & 0x30) >> 4;
				value_B = (value_B << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd_3 value_B: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_B, value_B, reg_data1, reg_data2);
				retry_count--;
			}

			}
		}
		else
		{
			SYS_ERR("[TOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		}
		if(value_A || value_B)
			value = (value_A > value_B) ? value_A : value_B;
			
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_check_pd_3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
	{
		SYS_ERR("Please enter valid input\n");
		return -1;
	}

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		SYS_INFO("[TOF_DBG] PD_3 store value: 0x%x(%d), ready: %d\n", value, value, check_pd_ready);
		if (check_pd_ready == 0)
			check_pd_ready = 1;

		//page setup - start
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		//page setup - end

		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x55E6, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //start address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //number of write bytes
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0008, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //register address of VCSEL Driver
		msleep(10);

		//data for 0x08 (0xE1 is 2700mA, 1st Current : 0xE1  (225*12=2700mA) 2nd current : 0x7D (125*12=1500mA))
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  
		msleep(223);
		
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x00FF, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Delay command
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x000F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Data for Delay, 13.3us
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Terminates command array
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5626, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //end address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // data at end address
		msleep(10);
		
	}
	else
	{
		SYS_INFO("[TOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear2_tof_check_pd_4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0, value_A = 0, value_B = 0, retry_count = PD_RETRY_COUNT;
	uint32_t reg_data1, reg_data2 = 0;
	int rc = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if(check_pd_ready == 0) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

		if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
			(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
		{
			while(retry_count && !value_A)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_A = (reg_data1 & 0x30) >> 4;
				value_A = (value_A << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd_4 value_A: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_A, value_A, reg_data1, reg_data2);
				retry_count--;
			}
			if(value_A){

				retry_count = PD_RETRY_COUNT;
				reg_data1 = 0, reg_data2 = 0;
				while(retry_count && !value_B)
			{
				//page setup
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0023, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  // //addr, CHECK_APC_IMOD[9:8], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
				msleep(10); 

				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0026, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // addr, CHECK_APC_IMOD[7:0], PD value
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
				msleep(10);
				cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

				msleep(223);

				value_B = (reg_data1 & 0x30) >> 4;
				value_B = (value_B << 8)|reg_data2;
				SYS_ERR("[TOF_DBG] pd_4 value_B: 0x%x(%d) (reg_data1: 0x%x, reg_data2: 0x%x)\n", value_B, value_B, reg_data1, reg_data2);
				retry_count--;
			}

			}
		}
		else
		{
			SYS_ERR("[TOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		}
		if(value_A || value_B)
			value = (value_A > value_B) ? value_A : value_B;
			
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_check_pd_4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
	{
		SYS_ERR("Please enter valid input\n");
		return -1;
	}

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		SYS_INFO("[TOF_DBG] PD_4 store value: 0x%x(%d), ready: %d\n", value, value, check_pd_ready);
		if (check_pd_ready == 0)
			check_pd_ready = 1;

		//page setup - start
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		msleep(10);
		//page setup - end

		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x55E6, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //start address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //number of write bytes
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0008, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //register address of VCSEL Driver
		msleep(10);

		//data for 0x08 (0xE1 is 2700mA, 1st Current : 0xE1  (225*12=2700mA) 2nd current : 0x7D (125*12=1500mA))
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);  
		msleep(223);
		
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x00FF, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Delay command
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x000F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Data for Delay, 13.3us
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //Terminates command array
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5626, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //end address
		msleep(10);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); // data at end address
		msleep(10);
		
	}
	else
	{
		SYS_INFO("[TOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear2_tof_laser_error_flag_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0,binary_value=0;
	char errorValueBinary[TOF_TX_ERR_MASK_SIZE+1];
	char* binaryValuePtr;
	uint32_t reg_data1 = 0, reg_data2 = 0, reg_data3 = 0;
	int rc = 0,i;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		//VCSEL Error Mask Register Read Status(1)
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 

			
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0027, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //addr, STATUS1 of DW9854
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

		//VCSEL Error Mask Register Read Status(2)
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 

			
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0028, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //addr, STATUS2 of DW9854
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

		//VCSEL Error Mask Register Read Status(3)
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 

			
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6028, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5628, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x562A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0029, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //addr, STATUS3 of DW9854
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602A, 0x5664, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x6F12, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602C, 0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x602E, 0x5650, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
		
		cam_sensor_i2c_read(ITOF_SENSOR_CAM, 0x6F12, &reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);

		//value = (reg_data1 & 0x30) >> 4;
		//value = (value << 8)|reg_data2;
		value = reg_data3;
		value = (value << 8) | reg_data2;
		value = (value << 8) | reg_data1;
		value = value & 0xFFFFF; // 20bit status result
		binary_value = value;
		errorValueBinary[TOF_TX_ERR_MASK_SIZE] = '\0';
		binaryValuePtr = errorValueBinary + (TOF_TX_ERR_MASK_SIZE - 1);
		for (i = (TOF_TX_ERR_MASK_SIZE - 1); i >= 0; i--)
		{
			*binaryValuePtr-- = (binary_value & 1) + '0';
			binary_value >>= 1;
		}
		SYS_ERR("[TOF_DBG] status reg value: 0x%x(%d)(%s) (status1: 0x%x, status2: 0x%x, status3: 0x%x)\n", value, value, errorValueBinary, reg_data1, reg_data2, reg_data3);
		rc = scnprintf(buf, PAGE_SIZE, "%s\n", errorValueBinary);
	}
	else
	{
		SYS_ERR("[TOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		rc = scnprintf(buf, PAGE_SIZE, "11111111111111111111\n");
	}
	if (rc)
		return rc;
	return 0;
}


static ssize_t rear2_tof_laser_error_flag_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL)
		return -1;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
		return -1;

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		SYS_ERR("[TOF_DBG] No functionality needed in store!!!");
	}
	else
	{
		SYS_INFO("[TOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear2_tof_fps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if (rc)
		return rc;

	return 0;
}

static ssize_t rear2_tof_fps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value = 0;
	struct cam_camera_slave_info *slave_info;

	if(g_s_ctrl_info[ITOF_SENSOR_CAM] == NULL){
		SYS_ERR("[TOF_DBG] Invalid state. TOF Not running");
		return 0;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[ITOF_SENSOR_CAM]->sensordata->slave_info);

	if ((TOF_SENSOR_ID_S5K63D == slave_info->sensor_id) ||
		(TOF_SENSOR_ID_S5K63D_V2 == slave_info->sensor_id))
	{
		SYS_INFO("[TOF] rear tof uid : %x\n", rear_tof_uid);

		if(value > 999) //VGA
		{
			value = value / 1000;

			switch (value) {
				case 24:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x13, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x6F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				case 20:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x19, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0xF1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				case 15:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x26, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0xF7, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				case 10:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x41, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x02, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				case 5:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x8F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x22, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				case 2:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x79, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x82, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				case 1:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x03, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x22, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] VGA fps : %d\n", value);
					break;

				default :
					SYS_INFO("[TOF] invalid fps : %d\n", value);
					break;
			}
		}
		else  // QVGA
		{
			switch (value) {
				case 30:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x13, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x78, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 24:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x19, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0xFB, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 20:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x20, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x7D, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 15:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x2D, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x83, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 10:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x47, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x8E, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 5:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x95, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0xAE, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 2:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x80, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0x0E, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				case 1:
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold On
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2110, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2111, 0x03, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2112, 0x06, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x2113, 0xAE, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
					cam_sensor_i2c_write(ITOF_SENSOR_CAM, 0x0104, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); //GroupHold Off
					SYS_INFO("[TOF] QVGA fps : %d\n", value);
					break;

				default :
					SYS_INFO("[TOF] Invalid fps : %d\n", value);
					break;
			}
		}
	}
	return size;
}

static ssize_t rear_tof_freq_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_ERR("[SETCAL_DBG] tof_freq : %s\n", tof_freq);
	rc = scnprintf(buf, PAGE_SIZE, "%s", tof_freq);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_freq_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_ERR("[SETCAL_DBG] buf : %s\n", buf);
	scnprintf(tof_freq, sizeof(tof_freq), "%s", buf);

	return size;
}

int rear_tof_validation_500;
int rear_tof_validation_300;
static ssize_t rear_tof_get_validation_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    int rc= 0;

	SYS_ERR("[SET_CAL] rear_tof_validation = %d, %d\n", rear_tof_validation_300, rear_tof_validation_500);

	rc = scnprintf(buf, PAGE_SIZE, "%d,%d\n", rear_tof_validation_300/100, rear_tof_validation_500/100);
	if (rc)
		return rc;
	return 0;
}


char rear_dtof_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear2_tof_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_info : %s\n", rear_dtof_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_dtof_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_dtof_cam_info, sizeof(rear_dtof_cam_info), "%s", buf);

	return size;
}

char rear_dtof_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear2_tof_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear2_tof_mtf_exif : %s\n", rear_dtof_mtf_exif);
	memcpy(buf, rear_dtof_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear2_tof_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_dtof_mtf_exif, sizeof(rear_dtof_mtf_exif), "%s", buf);

	return size;
}

char rear_dtof_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear2_tof_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear2_tof_sensor_id : %s\n", rear_dtof_sensor_id);
	memcpy(buf, rear_dtof_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear2_tof_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_dtof_sensor_id, sizeof(rear_dtof_sensor_id), "%s", buf);

	return size;
}

char rear_dtof_module_info[SYSFS_MODULE_INFO_SIZE + 1] = "NULL\n";
static ssize_t rear2_tof_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear2_tof_module_info : %s\n", rear_dtof_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_dtof_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_dtof_module_info, sizeof(rear_dtof_module_info), "%s", buf);

	return size;
}

char rear_dtof_module_id[FROM_MODULE_ID_SIZE + 1] = {'N', 'G', '\0'};
static ssize_t rear2_tof_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SYS_DBG("[FW_DBG] rear2_tof_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_dtof_module_id[0], rear_dtof_module_id[1], rear_dtof_module_id[2], rear_dtof_module_id[3], rear_dtof_module_id[4],
	  rear_dtof_module_id[5], rear_dtof_module_id[6], rear_dtof_module_id[7], rear_dtof_module_id[8], rear_dtof_module_id[9]);
	sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_dtof_module_id[0], rear_dtof_module_id[1], rear_dtof_module_id[2], rear_dtof_module_id[3], rear_dtof_module_id[4],
	  rear_dtof_module_id[5], rear_dtof_module_id[6], rear_dtof_module_id[7], rear_dtof_module_id[8], rear_dtof_module_id[9]);
	return strlen(buf);
}

char cam_dtof_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear2_tof_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	SYS_DBG("[FW_DBG] cam_dtof_fw_ver : %s\n", cam_dtof_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_dtof_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_dtof_fw_ver, sizeof(cam_dtof_fw_ver), "%s", buf);

	return size;
}

char cam_dtof_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear2_tof_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_dtof_fw_user_ver : %s\n", cam_dtof_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_dtof_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_dtof_fw_user_ver, sizeof(cam_dtof_fw_user_ver), "%s", buf);

	return size;
}

char cam_dtof_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear2_tof_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_dtof_fw_factory_ver : %s\n", cam_dtof_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_dtof_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_dtof_fw_factory_ver, sizeof(cam_dtof_fw_factory_ver), "%s", buf);

	return size;
}

char cam_dtof_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear2_tof_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] cam_dtof_fw_full_ver : %s\n", cam_dtof_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_dtof_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_dtof_fw_full_ver, sizeof(cam_dtof_fw_full_ver), "%s", buf);

	return size;
}

static ssize_t rear2_tof_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SONY_IMX611\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char cam_dtof_load_fw[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t rear2_tof_firmware_load_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_DBG("[FW_DBG] rear2_tof_load_fw : %s\n", cam_dtof_load_fw);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_dtof_load_fw);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_tof_firmware_load_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	SYS_DBG("[FW_DBG] buf : %s\n", buf);
	scnprintf(cam_dtof_load_fw, sizeof(cam_dtof_load_fw), "%s\n", buf);
	return size;
}

int rear_dtof_uid;
static ssize_t rear2_tof_cal_uid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    int rc= 0;

	SYS_DBG("[FW_DBG] rear dtof uid = %d\n", rear_dtof_uid);

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", rear_dtof_uid);
	if (rc)
		return rc;
	return 0;
}

uint8_t rear_dtof_cal[DTOFCAL_SIZE + 1] = "\0";
static ssize_t rear2_tof_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear_dtof_cal : %s\n", rear_dtof_cal);

	if (DTOFCAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = DTOFCAL_SIZE;

	ret = memcpy(buf, rear_dtof_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;
}

uint8_t rear_dtof_cal_extra[DTOFCAL_EXTRA_SIZE + 1] = "\0";
static ssize_t rear2_tof_cal_extra_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear_dtof_cal_extra : %s\n", rear_dtof_cal_extra);

	if (DTOFCAL_EXTRA_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = DTOFCAL_EXTRA_SIZE;

	ret = memcpy(buf, rear_dtof_cal_extra, copy_size);

	if (ret)
		return copy_size;

	return 0;
}


uint32_t rear_dtof_cal_size = DTOFCAL_TOTAL_SIZE;
static ssize_t rear2_tof_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	SYS_INFO("[FW_DBG] rear_dtof_cal_size : %d\n", rear_dtof_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear_dtof_cal_size);
	if (rc)
		return rc;
	return 0;
}

uint8_t rear_dtof_cal_result = 0;
static ssize_t rear2_tof_cal_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if (rear_dtof_cal_result == 1)
		rc = scnprintf(buf, PAGE_SIZE, "%s", "OK\n");
	else
		rc = scnprintf(buf, PAGE_SIZE, "%s", "NG\n");
	if (rc)
		return rc;
	return 0;
}

uint8_t rear_dtof_dual_cal[REAR_DTOF_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear2_tof_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	SYS_DBG("[FW_DBG] rear2_tof_dual_cal : %s\n", rear_dtof_dual_cal);

	if (REAR_DTOF_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = REAR_DTOF_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear_dtof_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;
}

int rear_dtof_ld_value = 0;
static ssize_t rear_tof_ld_onoff_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	int rc = 0, value = -1;
	if(g_s_ctrl_info[DTOF_SENSOR_CAM] != NULL)
	{
		slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);
		if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
		{
			value = rear_dtof_ld_value;
			SYS_INFO("DTOF_DBG rear_tof_ld_value : %d \n", value);
		}
	}
	if( value != -1)
	{
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);
	}
	else 
	{
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	}
	return rc;
}

static ssize_t rear_tof_ld_onoff_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	int value = -1;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}
		

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		if(value == 0) { //ld off
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //page setup

			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x602A, 0xFB3A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x6F12, 0x8000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
			SYS_INFO("DTOF ld off\n");
			rear_dtof_ld_value = 0;
		} else if(value == 1) { //ld on
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x6028, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x602A, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); //page setup

			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x602A, 0xFB3A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD); 
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x6F12, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
			SYS_INFO("DTOF ld on\n");
			rear_dtof_ld_value = 1;
		}
		else
		{
			SYS_ERR("Invalid value to handle!!!\n");
		}
	} else {
		SYS_ERR("DTOF ld onoff not support\n");
	}

	return size;
}

static ssize_t rear_tof_ae_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t ae_value = 0;
	uint32_t reg_data3 = 0, reg_data2 = 0, reg_data1 = 0, reg_data0 = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
		return 0;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x2124, &reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x2125, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x2126, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x2127, &reg_data0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
	} else {
		SYS_INFO("DTOF AE support not added\n");
	}

	SYS_INFO("value 0x%x reg_data3: 0x%2x, addr2: 0x%2x, addr1: 0x%2x, addr0: 0x%2x\n", ae_value, reg_data3,reg_data2, reg_data1,reg_data0);

	ae_value = (reg_data3<<24)|(reg_data2<<16)|(reg_data1<<8)|reg_data0;

	SYS_INFO("ae_value: 0x%x\n", ae_value);

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", ae_value/120);

	if (rc)
		return rc;

	return 0;
}

static ssize_t rear_tof_ae_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	uint32_t reg_data3 = 0, reg_data2 = 0, reg_data1 = 0, reg_data0 = 0;
	uint32_t hmax = 0x02FE;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
		return 0;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	SYS_INFO("ae_value: 0x%x\n", value);

	value  = value * 120; // 120Mhz

	if(value >= hmax && value <= 0x1D4C0)
	{
		slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);
		reg_data3 = (value & 0xFF000000) >> 24;
		reg_data2 = (value & 0xFF0000) >> 16;
		reg_data1 = (value & 0xFF00) >> 8;
		reg_data0 = value & 0xFF;

		//SYS_INFO("value 0x%x reg_data3: 0x%2x, addr2: 0x%2x, addr1: 0x%2x, addr0: 0x%2x\n", value, reg_data3,reg_data2, reg_data1,reg_data0);
		if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
		{
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x2124, reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x2125, reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x2126, reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x2127, reg_data0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x2128, reg_data3, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x2129, reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x212A, reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x212B, reg_data0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		} else {
			SYS_INFO("DTOF AE support not added\n");
		}
	}

	return size;
}

static ssize_t rear_tof_check_pd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t reg_data1 = 0, retry_count = 3;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	if(check_pd_ready == 0) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

		if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
		{
			while(retry_count && !reg_data1)
			{
				cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x055C, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

				SYS_ERR("[DTOF_DBG] pd value: 0x%x(%d)\n", reg_data1, reg_data1);
				retry_count--;
			}
		}
		else
		{
			SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		}
		rc = scnprintf(buf, PAGE_SIZE, "%d\n", reg_data1);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_check_pd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
	{
		SYS_ERR("Please enter valid input\n");
		return -1;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_INFO("[DTOF_DBG] PD store value: 0x%x(%d), ready: %d\n", value, value, check_pd_ready);
		if (check_pd_ready == 0)
			check_pd_ready = 1;

		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0503, 0x20, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0504, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0508, 0x80, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0564, 0x02, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0565, 0x3A, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0566, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0501, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0500, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0501, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear_tof_cg_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t reg_data1 = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{

		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x0340, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		SYS_ERR("[DTOF_DBG] CG CAL reg: %d", reg_data1);
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
	}
	rc = scnprintf(buf, PAGE_SIZE, "%d\n", reg_data1);
	
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_cg_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		
		SYS_INFO("[DTOF_DBG] CG Calibration ON/ OFF: %d\n", value);

		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0340, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}


static ssize_t rear_tof_ld_iout_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t reg_data1 = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x051D, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		SYS_ERR("[DTOF_DBG] VCSEL operating current: %d", reg_data1);
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
	}
	rc = scnprintf(buf, PAGE_SIZE, "%d\n", reg_data1);
	
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_ld_iout_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		
		SYS_INFO("[DTOF_DBG] VCSEL Operating Current(iout) change\n");

		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x051D, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

		
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}



int meas_subRef_radialDepth = -1;
int16_t zero_shift;
uint32_t value1, value2;
static ssize_t rear_tof_auto_zeroing_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	if(meas_subRef_radialDepth == -1) {
		rc = scnprintf(buf, PAGE_SIZE, "NG\n");
	} else {
		slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

		if (DTOF_SENSOR_ID_IMX611 != slave_info->sensor_id)
			SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);

		rc = scnprintf(buf, PAGE_SIZE, "%d\n", meas_subRef_radialDepth);

		SYS_INFO("[DTOF_DBG] meas_subRef_radialDepth: %d, eeprom_subRef_radialDepth: %d, zero shift: %d [0x%x]", 
			meas_subRef_radialDepth, eeprom_subRef_radialDepth, zero_shift, zero_shift);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_auto_zeroing_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		
		meas_subRef_radialDepth = value;
		zero_shift = ((meas_subRef_radialDepth - eeprom_subRef_radialDepth) * 20) / 3; // converted to pico second unit

		value1 = ((zero_shift >> 8) & 0x000000FF);
		value2 = (zero_shift & 0x000000FF);

		SYS_DBG("[DTOF_DBG] Auto Zeroing meas_subRef_radialDepth: %d, eeprom_subRef_radialDepth: %d, zero shift distance: %d[0x%x], value1: [0x%x], value2: [0x%x]\n", 
			value, eeprom_subRef_radialDepth, zero_shift, zero_shift, value1, value2);

		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x03A0, value1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); 
		cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x03A1, value2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); 
		
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear_tof_vrld_delta_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t reg_data1 = 0, reg_data2 = 0;
	uint16_t value = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x0150, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x0151, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		value = (reg_data1 & 0x00FF);
		value = (value << 8) | (reg_data2 & 0x00FF);
		SYS_ERR("[DTOF_DBG] VRLD_ABS: %d", value);
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
	}

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_vrld_delta_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_ERR("[DTOF_DBG] No functionality needed in store!!!");
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear_tof_vrld_abs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t reg_data1 = 0, reg_data2 = 0;
	uint16_t value = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x0154, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x0155, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		value = (reg_data1 & 0x00FF);
		value = (value << 8) | (reg_data2 & 0x00FF);
		SYS_DBG("[DTOF_DBG] VRLD_ABS: %d", value);
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
	}

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_vrld_abs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_ERR("[DTOF_DBG] No functionality needed in store!!!");
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear_tof_vrld_pmic_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint16_t value = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_ERR("[DTOF_DBG] No functionality needed in show");
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
	}

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", value);

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_vrld_pmic_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;
	int16_t  val1  = 0, val2  = 0;
	int      ret   = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		val1 = value & 0x00FF;
		val2 = (value >> 8) & 0x00FF;
		ret = max77720_write_reg(0x31, val2);
        if (ret < 0) {
            SYS_ERR("[DTOF_DBG] failed to write reg[0x31] map %d\n", ret);
            return ret;
        }
		ret = max77720_write_reg(0x32, val1);
        if (ret < 0) {
            SYS_ERR("[DTOF_DBG] failed to write reg[0x32] map %d\n", ret);
            return ret;
        }
		SYS_DBG("[DTOF_DBG] Updated MAX77720 to %d(0x%x, 0x%x)", value, val1, val2);
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear_tof_gbd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t reg_data1 = 0;
	int rc = 0;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_ERR("[DTOF_DBG] No functionality needed in store!!!");
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
	}
	rc = scnprintf(buf, PAGE_SIZE, "%d\n", reg_data1);
	
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_tof_gbd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;


	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_INFO("[DTOF_DBG]GBD register: 0x0518 value: %d \n", value);

		if(value)
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0518, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
		else
			cam_sensor_i2c_write(DTOF_SENSOR_CAM, 0x0518, 0x00, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

static ssize_t rear_tof_laser_error_flag_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cam_camera_slave_info *slave_info;
	uint16_t value = 0,binary_value=0;
	char errorValueBinary[DTOF_TX_ERR_MASK_SIZE+1];
	char* binaryValuePtr;
	uint32_t reg_data1 = 0, reg_data2 = 0;
	int rc = 0,i;

	if(g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return 0;
	}

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		//VCSEL Error Mask Register Read Status(1)

		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x055E, &reg_data1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

		//VCSEL Error Mask Register Read Status(2)
		
		cam_sensor_i2c_read(DTOF_SENSOR_CAM, 0x055F, &reg_data2, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

		//value = (reg_data1 & 0x30) >> 4;
		//value = (value << 8)|reg_data2;
		value = (reg_data2 & 0x00FF);
		value = (value << 8) | (reg_data1 & 0x00FF);
		binary_value = value;
		errorValueBinary[DTOF_TX_ERR_MASK_SIZE] = '\0';
		binaryValuePtr = errorValueBinary + (DTOF_TX_ERR_MASK_SIZE - 1);
		for (i = (DTOF_TX_ERR_MASK_SIZE - 1); i >= 0; i--)
		{
			*binaryValuePtr-- = (binary_value & 1) + '0';
			binary_value >>= 1;
		}
		SYS_ERR("[DTOF_DBG] status reg value: 0x%x(%d)(%s) (status1: 0x%x, status2: 0x%x)\n", value, value, errorValueBinary, reg_data1, reg_data2);
		rc = scnprintf(buf, PAGE_SIZE, "%s\n", errorValueBinary);
	}
	else
	{
		SYS_ERR("[DTOF_DBG] Unsupported sensor id: %d", slave_info->sensor_id);
		rc = scnprintf(buf, PAGE_SIZE, "11111111111111111111\n");
	}
	if (rc)
		return rc;
	return 0;
}


static ssize_t rear_tof_laser_error_flag_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_camera_slave_info *slave_info;
	uint32_t value = 0;

	if (g_s_ctrl_info[DTOF_SENSOR_CAM] == NULL)
	{
		SYS_ERR("Invalid state. DTOF Not running\n");
		return -1;
	}

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	if(value == 0)
		return -1;

	slave_info = &(g_s_ctrl_info[DTOF_SENSOR_CAM]->sensordata->slave_info);

	if (DTOF_SENSOR_ID_IMX611 == slave_info->sensor_id)
	{
		SYS_ERR("[DTOF_DBG] No functionality needed in store!!!");
	}
	else
	{
		SYS_INFO("[DTOF_DBG] sensor id not matched: %d", slave_info->sensor_id);
	}
	return size;
}

int rear_dtof_validation_500;
int rear_dtof_validation_300;
static ssize_t rear2_tof_get_validation_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    int rc= 0;

	SYS_ERR("[SET_CAL] rear2_tof_validation = %d, %d\n", rear_dtof_validation_300, rear_dtof_validation_500);

	rc = scnprintf(buf, PAGE_SIZE, "%d,%d\n", rear_dtof_validation_300/100, rear_dtof_validation_500/100);
	if (rc)
		return rc;
	return 0;
}

#endif
static DEVICE_ATTR(rear_camtype, S_IRUGO, rear_type_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_show, rear_firmware_store);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_user_show, rear_firmware_user_store);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_factory_show, rear_firmware_factory_store);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_full_show, rear_firmware_full_store);
static DEVICE_ATTR(rear_camfw_load, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_load_show, rear_firmware_load_store);
static DEVICE_ATTR(rear_calcheck, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cal_data_check_show, rear_cal_data_check_store);
static DEVICE_ATTR(rear_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_module_info_show, rear_module_info_store);
static DEVICE_ATTR(isp_core, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_isp_core_check_show, rear_isp_core_check_store);
static DEVICE_ATTR(rear_afcal, S_IRUGO, rear_afcal_show, NULL);
static DEVICE_ATTR(rear_paf_offset_far, S_IRUGO,
	rear_paf_offset_far_show, NULL);
static DEVICE_ATTR(rear_paf_offset_mid, S_IRUGO,
	rear_paf_offset_mid_show, NULL);
static DEVICE_ATTR(rear_paf_cal_check, S_IRUGO,
	rear_paf_cal_check_show, NULL);
static DEVICE_ATTR(rear_f2_paf_offset_far, S_IRUGO,
	rear_f2_paf_offset_far_show, NULL);
static DEVICE_ATTR(rear_f2_paf_offset_mid, S_IRUGO,
	rear_f2_paf_offset_mid_show, NULL);
static DEVICE_ATTR(rear_f2_paf_cal_check, S_IRUGO,
	rear_f2_paf_cal_check_show, NULL);
static DEVICE_ATTR(front_camtype, S_IRUGO, front_camera_type_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_firmware_show, front_camera_firmware_store);
static DEVICE_ATTR(front_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front_camera_firmware_full_show, front_camera_firmware_full_store);
static DEVICE_ATTR(front_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_firmware_user_show, front_camera_firmware_user_store);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_firmware_factory_show, front_camera_firmware_factory_store);
static DEVICE_ATTR(rear_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_info_show, rear_camera_info_store);
static DEVICE_ATTR(front_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_info_show, front_camera_info_store);
static DEVICE_ATTR(front_paf_cal_check, S_IRUGO,
	front_paf_cal_check_show, NULL);
static DEVICE_ATTR(front_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front_module_info_show, front_module_info_store);
static DEVICE_ATTR(rear_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_sensorid_exif_show, rear_sensorid_exif_store);
static DEVICE_ATTR(front_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front_sensorid_exif_show, front_sensorid_exif_store);
static DEVICE_ATTR(rear_moduleid, S_IRUGO, rear_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module, S_IRUGO, rear_moduleid_show, NULL);
static DEVICE_ATTR(front_moduleid, S_IRUGO, front_camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_front_module, S_IRUGO, front_camera_moduleid_show, NULL);
static DEVICE_ATTR(front_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front_mtf_exif_show, front_mtf_exif_store);
static DEVICE_ATTR(front2_camtype, S_IRUGO, front2_camera_type_show, NULL);
static DEVICE_ATTR(front2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_show, front2_camera_firmware_store);
static DEVICE_ATTR(front2_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front2_camera_firmware_full_show, front2_camera_firmware_full_store);
static DEVICE_ATTR(front2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_user_show, front2_camera_firmware_user_store);
static DEVICE_ATTR(front2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_factory_show, front2_camera_firmware_factory_store);
static DEVICE_ATTR(front2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_module_info_show, front2_module_info_store);
static DEVICE_ATTR(front2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_info_show, front2_camera_info_store);
static DEVICE_ATTR(front2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_sensorid_exif_show, front2_sensorid_exif_store);
static DEVICE_ATTR(front2_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_mtf_exif_show, front2_mtf_exif_store);

static DEVICE_ATTR(front2_moduleid, S_IRUGO, front2_camera_moduleid_show, NULL);

static DEVICE_ATTR(front3_camtype, S_IRUGO, front3_camera_type_show, NULL);
static DEVICE_ATTR(front3_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_firmware_show, front3_camera_firmware_store);
static DEVICE_ATTR(front3_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front3_camera_firmware_full_show, front3_camera_firmware_full_store);
static DEVICE_ATTR(front3_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_firmware_user_show, front3_camera_firmware_user_store);
static DEVICE_ATTR(front3_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_firmware_factory_show, front3_camera_firmware_factory_store);
static DEVICE_ATTR(front3_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_module_info_show, front3_module_info_store);
static DEVICE_ATTR(front3_moduleid, S_IRUGO, front3_camera_moduleid_show, NULL);
static DEVICE_ATTR(front3_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_info_show, front3_camera_info_store);
static DEVICE_ATTR(front3_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_sensorid_exif_show, front3_sensorid_exif_store);
static DEVICE_ATTR(front3_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_mtf_exif_show, front3_mtf_exif_store);

static DEVICE_ATTR(front4_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_camera_info_show, front4_camera_info_store);
static DEVICE_ATTR(front4_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_module_info_show, front4_module_info_store);
static DEVICE_ATTR(front4_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_mtf_exif_show, front4_mtf_exif_store);
static DEVICE_ATTR(front4_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_sensorid_exif_show, front4_sensorid_exif_store);
static DEVICE_ATTR(front4_moduleid, S_IRUGO,
	front4_camera_moduleid_show, NULL);
static DEVICE_ATTR(front4_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_camera_firmware_show, front4_camera_firmware_store);
static DEVICE_ATTR(front4_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_camera_firmware_user_show, front4_camera_firmware_user_store);
static DEVICE_ATTR(front4_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_camera_firmware_factory_show, front4_camera_firmware_factory_store);
static DEVICE_ATTR(front4_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_camera_firmware_full_show, front4_camera_firmware_full_store);
static DEVICE_ATTR(front4_camtype, S_IRUGO, front4_camera_type_show, NULL);

static DEVICE_ATTR(front5_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_camera_info_show, front5_camera_info_store);
static DEVICE_ATTR(front5_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_module_info_show, front5_module_info_store);
static DEVICE_ATTR(front5_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_mtf_exif_show, front5_mtf_exif_store);
static DEVICE_ATTR(front5_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_sensorid_exif_show, front5_sensorid_exif_store);
static DEVICE_ATTR(front5_moduleid, S_IRUGO,
	front5_camera_moduleid_show, NULL);
static DEVICE_ATTR(front5_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_camera_firmware_show, front5_camera_firmware_store);
static DEVICE_ATTR(front5_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_camera_firmware_user_show, front5_camera_firmware_user_store);
static DEVICE_ATTR(front5_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_camera_firmware_factory_show, front5_camera_firmware_factory_store);
static DEVICE_ATTR(front5_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	front5_camera_firmware_full_show, front5_camera_firmware_full_store);
static DEVICE_ATTR(front5_camtype, S_IRUGO, front5_camera_type_show, NULL);

static DEVICE_ATTR(supported_cameraIds, 0664,
	supported_camera_ids_show, supported_camera_ids_store);
static DEVICE_ATTR(binary_mode, 0664,
	binary_mode_show, binary_mode_store);
static DEVICE_ATTR(dtof_pd_value, 0664,
	dtof_pd_value_show, dtof_pd_value_store);

static DEVICE_ATTR(rear_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_mtf_exif_show, rear_mtf_exif_store);
static DEVICE_ATTR(rear_mtf2_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_mtf2_exif_show, rear_mtf2_exif_store);
static DEVICE_ATTR(ssrm_camera_info, S_IRUGO|S_IWUSR|S_IWGRP,
	ssrm_camera_info_show, ssrm_camera_info_store);

static DEVICE_ATTR(rear3_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_firmware_show, rear3_firmware_store);
static DEVICE_ATTR(rear3_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_firmware_full_show, rear3_firmware_full_store);
static DEVICE_ATTR(rear3_afcal, S_IRUGO, rear3_afcal_show, NULL);
static DEVICE_ATTR(rear3_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_camera_info_show, rear3_camera_info_store);
static DEVICE_ATTR(rear3_camtype, S_IRUGO, rear3_type_show, NULL);
static DEVICE_ATTR(rear3_moduleid, S_IRUGO,
	rear3_moduleid_show, NULL);
static DEVICE_ATTR(rear3_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_module_info_show, rear3_module_info_store);
static DEVICE_ATTR(rear3_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_mtf_exif_show, rear3_mtf_exif_store);
static DEVICE_ATTR(rear3_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_sensorid_exif_show, rear3_sensorid_exif_store);
static DEVICE_ATTR(rear3_dualcal, S_IRUGO, rear3_dual_cal_show, NULL);
static DEVICE_ATTR(rear3_dualcal_size, S_IRUGO, rear3_dual_cal_size_show, NULL);

static DEVICE_ATTR(rear3_tilt, S_IRUGO, rear3_tilt_show, NULL);
static DEVICE_ATTR(rear3_paf_cal_check, S_IRUGO,
	rear3_paf_cal_check_show, NULL);

static DEVICE_ATTR(rear2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_camera_info_show, rear2_camera_info_store);
static DEVICE_ATTR(rear2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_module_info_show, rear2_module_info_store);
static DEVICE_ATTR(rear2_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_mtf_exif_show, rear2_mtf_exif_store);
static DEVICE_ATTR(rear2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_sensorid_exif_show, rear2_sensorid_exif_store);
static DEVICE_ATTR(rear2_moduleid, S_IRUGO,
	rear2_moduleid_show, NULL);
static DEVICE_ATTR(rear2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_show, rear2_firmware_store);
static DEVICE_ATTR(rear2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_user_show, rear2_firmware_user_store);
static DEVICE_ATTR(rear2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_factory_show, rear2_firmware_factory_store);
static DEVICE_ATTR(rear2_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_full_show, rear2_firmware_full_store);
static DEVICE_ATTR(rear2_camtype, S_IRUGO, rear2_type_show, NULL);

static DEVICE_ATTR(rear2_dualcal, S_IRUGO, rear2_dual_cal_show, NULL);
static DEVICE_ATTR(rear2_dualcal_size, S_IRUGO, rear2_dual_cal_size_show, NULL);
static DEVICE_ATTR(rear2_tilt, S_IRUGO, rear2_tilt_show, NULL);

static DEVICE_ATTR(rear4_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_camera_info_show, rear4_camera_info_store);
static DEVICE_ATTR(rear4_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_module_info_show, rear4_module_info_store);
static DEVICE_ATTR(rear4_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_mtf_exif_show, rear4_mtf_exif_store);
static DEVICE_ATTR(rear4_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_sensorid_exif_show, rear4_sensorid_exif_store);
static DEVICE_ATTR(rear4_moduleid, S_IRUGO,
	rear4_moduleid_show, NULL);
static DEVICE_ATTR(rear4_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_show, rear4_firmware_store);
static DEVICE_ATTR(rear4_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_user_show, rear4_firmware_user_store);
static DEVICE_ATTR(rear4_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_factory_show, rear4_firmware_factory_store);
static DEVICE_ATTR(rear4_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_full_show, rear4_firmware_full_store);
static DEVICE_ATTR(rear4_camtype, S_IRUGO, rear4_type_show, NULL);

static DEVICE_ATTR(rear5_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_camera_info_show, rear5_camera_info_store);
static DEVICE_ATTR(rear5_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_module_info_show, rear5_module_info_store);
static DEVICE_ATTR(rear5_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_mtf_exif_show, rear5_mtf_exif_store);
static DEVICE_ATTR(rear5_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_sensorid_exif_show, rear5_sensorid_exif_store);
static DEVICE_ATTR(rear5_moduleid, S_IRUGO,
	rear5_moduleid_show, NULL);
static DEVICE_ATTR(rear5_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_firmware_show, rear5_firmware_store);
static DEVICE_ATTR(rear5_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_firmware_user_show, rear5_firmware_user_store);
static DEVICE_ATTR(rear5_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_firmware_factory_show, rear5_firmware_factory_store);
static DEVICE_ATTR(rear5_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_firmware_full_show, rear5_firmware_full_store);
static DEVICE_ATTR(rear5_camtype, S_IRUGO, rear5_type_show, NULL);

static DEVICE_ATTR(rear6_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_camera_info_show, rear6_camera_info_store);
static DEVICE_ATTR(rear6_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_module_info_show, rear6_module_info_store);
static DEVICE_ATTR(rear6_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_mtf_exif_show, rear6_mtf_exif_store);
static DEVICE_ATTR(rear6_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_sensorid_exif_show, rear6_sensorid_exif_store);
static DEVICE_ATTR(rear6_moduleid, S_IRUGO,
	rear6_moduleid_show, NULL);
static DEVICE_ATTR(rear6_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_firmware_show, rear6_firmware_store);
static DEVICE_ATTR(rear6_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_firmware_user_show, rear6_firmware_user_store);
static DEVICE_ATTR(rear6_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_firmware_factory_show, rear6_firmware_factory_store);
static DEVICE_ATTR(rear6_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_firmware_full_show, rear6_firmware_full_store);
static DEVICE_ATTR(rear6_camtype, S_IRUGO, rear6_type_show, NULL);

static DEVICE_ATTR(rear7_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_firmware_show, rear7_firmware_store);
static DEVICE_ATTR(rear7_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_firmware_full_show, rear7_firmware_full_store);
static DEVICE_ATTR(rear7_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_camera_info_show, rear7_camera_info_store);
static DEVICE_ATTR(rear7_camtype, S_IRUGO, rear7_type_show, NULL);
static DEVICE_ATTR(rear7_moduleid, S_IRUGO,
	rear7_moduleid_show, NULL);
static DEVICE_ATTR(rear7_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_module_info_show, rear7_module_info_store);
static DEVICE_ATTR(rear7_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_mtf_exif_show, rear7_mtf_exif_store);
static DEVICE_ATTR(rear7_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_sensorid_exif_show, rear7_sensorid_exif_store);
	
static DEVICE_ATTR(rear8_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_firmware_show, rear8_firmware_store);
static DEVICE_ATTR(rear8_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_firmware_full_show, rear8_firmware_full_store);
static DEVICE_ATTR(rear8_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_camera_info_show, rear8_camera_info_store);
static DEVICE_ATTR(rear8_camtype, S_IRUGO, rear8_type_show, NULL);
static DEVICE_ATTR(rear8_moduleid, S_IRUGO,
	rear8_moduleid_show, NULL);
static DEVICE_ATTR(rear8_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_module_info_show, rear8_module_info_store);
static DEVICE_ATTR(rear8_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_mtf_exif_show, rear8_mtf_exif_store);
static DEVICE_ATTR(rear8_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_sensorid_exif_show, rear8_sensorid_exif_store);	

#if defined(CONFIG_CAMERA_CDR_TEST)
static DEVICE_ATTR(cam_cdr_value, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cam_cdr_value_show, rear_cam_cdr_value_store);
static DEVICE_ATTR(cam_cdr_result, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cam_cdr_result_show, rear_cam_cdr_result_store);
static DEVICE_ATTR(cam_cdr_fastaec, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cam_cdr_fastaec_show, rear_cam_cdr_fastaec_store);
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
static DEVICE_ATTR(rear_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_hw_param_show, rear_camera_hw_param_store);
static DEVICE_ATTR(rear2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_camera_hw_param_show, rear2_camera_hw_param_store);
static DEVICE_ATTR(rear3_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_camera_hw_param_show, rear3_camera_hw_param_store);
static DEVICE_ATTR(rear4_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_camera_hw_param_show, rear4_camera_hw_param_store);
static DEVICE_ATTR(rear5_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear5_camera_hw_param_show, rear5_camera_hw_param_store);
static DEVICE_ATTR(rear6_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear6_camera_hw_param_show, rear6_camera_hw_param_store);
static DEVICE_ATTR(rear7_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear7_camera_hw_param_show, rear7_camera_hw_param_store);
static DEVICE_ATTR(rear8_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear8_camera_hw_param_show, rear8_camera_hw_param_store);
static DEVICE_ATTR(front_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_hw_param_show, front_camera_hw_param_store);
static DEVICE_ATTR(front2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_hw_param_show, front2_camera_hw_param_store);
static DEVICE_ATTR(front3_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_hw_param_show, front3_camera_hw_param_store);
static DEVICE_ATTR(front4_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front4_camera_hw_param_show, front4_camera_hw_param_store);
static DEVICE_ATTR(rear_tof_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_camera_hw_param_show, rear_tof_camera_hw_param_store);
static DEVICE_ATTR(rear2_tof_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_camera_hw_param_show, rear2_tof_camera_hw_param_store);
static DEVICE_ATTR(cam_wifi_info, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_wifi_info_show, rear_camera_wifi_info_store);

static DEVICE_ATTR(common_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	common_camera_hw_param_show, common_camera_hw_param_store);

#endif

#if defined(CONFIG_SAMSUNG_REAR_TOF)
static DEVICE_ATTR(rear_tof_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_camera_info_show, rear_tof_camera_info_store);
static DEVICE_ATTR(rear_tof_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_module_info_show, rear_tof_module_info_store);
static DEVICE_ATTR(rear_tof_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_mtf_exif_show, rear_tof_mtf_exif_store);
static DEVICE_ATTR(rear_tof_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_sensorid_exif_show, rear_tof_sensorid_exif_store);
static DEVICE_ATTR(rear_tof_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_firmware_user_show, rear_tof_firmware_user_store);
static DEVICE_ATTR(rear_tof_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_firmware_factory_show, rear_tof_firmware_factory_store);
static DEVICE_ATTR(rear_tof_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_firmware_full_show, rear_tof_firmware_full_store);
static DEVICE_ATTR(rear_tof_camtype, S_IRUGO, rear_tof_type_show, NULL);
static DEVICE_ATTR(rear_tof_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_camera_firmware_show, rear_tof_camera_firmware_store);
static DEVICE_ATTR(rear_tof_camfw_load, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_firmware_load_show, rear_tof_firmware_load_store);
static DEVICE_ATTR(rear_tof_moduleid, S_IRUGO, rear_tof_moduleid_show, NULL);
static DEVICE_ATTR(rear_tof_cal_uid, S_IRUGO, rear_tof_cal_uid_show, NULL);
static DEVICE_ATTR(rear_tof_cal, S_IRUGO, rear_tof_cal_show, NULL);
static DEVICE_ATTR(rear_tof_cal_extra, S_IRUGO, rear_tof_cal_extra_show, NULL);
static DEVICE_ATTR(rear_tof_cal_size, S_IRUGO, rear_tof_cal_size_show, NULL);
static DEVICE_ATTR(rear_tof_cal_result, S_IRUGO, rear_tof_cal_result_show, NULL);
static DEVICE_ATTR(rear_tof_tilt, S_IRUGO, rear_tof_tilt_show, NULL);
static DEVICE_ATTR(rear2_tof_tilt, S_IRUGO, rear2_tof_tilt_show, NULL);
static DEVICE_ATTR(rear_tof_dual_cal, S_IRUGO, rear_tof_dual_cal_show, NULL);
static DEVICE_ATTR(rear_tof_ld_onoff, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_ld_onoff_show, rear_tof_ld_onoff_store);
static DEVICE_ATTR(rear_tof_ae_value, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_ae_value_show, rear_tof_ae_value_store);
static DEVICE_ATTR(rear_tof_check_pd, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_check_pd_show, rear_tof_check_pd_store);
static DEVICE_ATTR(rear_tof_auto_zeroing, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_auto_zeroing_show, rear_tof_auto_zeroing_store);
static DEVICE_ATTR(rear_tof_ld_iout, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_ld_iout_show, rear_tof_ld_iout_store);
static DEVICE_ATTR(rear_tof_cg_cal, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_cg_cal_show, rear_tof_cg_cal_store);
static DEVICE_ATTR(rear_tof_vrld_delta, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_vrld_delta_show, rear_tof_vrld_delta_store);
static DEVICE_ATTR(rear_tof_vrld_abs, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_vrld_abs_show, rear_tof_vrld_abs_store);
static DEVICE_ATTR(rear_tof_vrld_pmic, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_vrld_pmic_show, rear_tof_vrld_pmic_store);
static DEVICE_ATTR(rear_tof_gbd, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_gbd_show, rear_tof_gbd_store);
static DEVICE_ATTR(rear_tof_error_flag, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_laser_error_flag_show, rear_tof_laser_error_flag_store);
static DEVICE_ATTR(rear_tof_freq, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_freq_show, rear_tof_freq_store);
static DEVICE_ATTR(rear_tof_get_validation, S_IRUGO,
	rear_tof_get_validation_show, NULL);

static DEVICE_ATTR(rear2_tof_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_camera_info_show, rear2_tof_camera_info_store);
static DEVICE_ATTR(rear2_tof_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_module_info_show, rear2_tof_module_info_store);
static DEVICE_ATTR(rear2_tof_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_mtf_exif_show, rear2_tof_mtf_exif_store);
static DEVICE_ATTR(rear2_tof_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_sensorid_exif_show, rear2_tof_sensorid_exif_store);
static DEVICE_ATTR(rear2_tof_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_firmware_user_show, rear2_tof_firmware_user_store);
static DEVICE_ATTR(rear2_tof_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_firmware_factory_show, rear2_tof_firmware_factory_store);
static DEVICE_ATTR(rear2_tof_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_firmware_full_show, rear2_tof_firmware_full_store);
static DEVICE_ATTR(rear2_tof_camtype, S_IRUGO, rear2_tof_type_show, NULL);
static DEVICE_ATTR(rear2_tof_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_camera_firmware_show, rear2_tof_camera_firmware_store);
static DEVICE_ATTR(rear2_tof_camfw_load, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_firmware_load_show, rear2_tof_firmware_load_store);
static DEVICE_ATTR(rear2_tof_moduleid, S_IRUGO, rear2_tof_moduleid_show, NULL);
static DEVICE_ATTR(rear2_tof_cal_uid, S_IRUGO, rear2_tof_cal_uid_show, NULL);
static DEVICE_ATTR(rear2_tof_cal, S_IRUGO, rear2_tof_cal_show, NULL);
static DEVICE_ATTR(rear2_tof_cal_extra, S_IRUGO, rear2_tof_cal_extra_show, NULL);
static DEVICE_ATTR(rear2_tof_cal_size, S_IRUGO, rear2_tof_cal_size_show, NULL);
static DEVICE_ATTR(rear2_tof_cal_result, S_IRUGO, rear2_tof_cal_result_show, NULL);
static DEVICE_ATTR(rear2_tof_dual_cal, S_IRUGO, rear2_tof_dual_cal_show, NULL);
static DEVICE_ATTR(rear2_tof_ld_onoff, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_ld_onoff_show, rear2_tof_ld_onoff_store);
static DEVICE_ATTR(rear2_tof_ae_value, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_ae_value_show, rear2_tof_ae_value_store);
static DEVICE_ATTR(rear2_tof_check_pd, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_check_pd_show, rear2_tof_check_pd_store);
static DEVICE_ATTR(rear2_tof_check_pd_2, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_check_pd_2_show, rear2_tof_check_pd_2_store);
static DEVICE_ATTR(rear2_tof_check_pd_3, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_check_pd_3_show, rear2_tof_check_pd_3_store);
static DEVICE_ATTR(rear2_tof_check_pd_4, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_check_pd_4_show, rear2_tof_check_pd_4_store);
static DEVICE_ATTR(rear2_tof_error_flag, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_laser_error_flag_show, rear2_tof_laser_error_flag_store);
static DEVICE_ATTR(rear2_tof_fps, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_fps_show, rear2_tof_fps_store);
static DEVICE_ATTR(rear2_tof_get_validation, S_IRUGO,
	rear2_tof_get_validation_show, NULL);
#endif
static DEVICE_ATTR(et_flash_current, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_current_show, et_flash_current_store);
static DEVICE_ATTR(et2_flash_current, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et2_flash_current_show, et2_flash_current_store);
static DEVICE_ATTR(et_flash_reg, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_reg_show, et_flash_reg_store);
static DEVICE_ATTR(et2_flash_reg, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et2_flash_reg_show, et2_flash_reg_store);
static DEVICE_ATTR(et_flash_status, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_status_show, et_flash_status_store);
static DEVICE_ATTR(et2_flash_status, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et2_flash_status_show, et2_flash_status_store);
static DEVICE_ATTR(et_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_left_show, et_flash_left_store);
static DEVICE_ATTR(et2_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_right_show, et_flash_right_store);
static DEVICE_ATTR(et_flash_pattern, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_left_show, et_flash_left_pattern_store);
static DEVICE_ATTR(et2_flash_pattern, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, et_flash_right_show, et_flash_right_pattern_store);

static DEVICE_ATTR(hat_lu_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_lu_flash_show, hat_lu_flash_store);
static DEVICE_ATTR(hat_ru_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_ru_flash_show, hat_ru_flash_store);
static DEVICE_ATTR(hat_ld_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_ld_flash_show, hat_ld_flash_store);
static DEVICE_ATTR(hat_rd_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_rd_flash_show, hat_rd_flash_store);
static DEVICE_ATTR(hat_lu_flash_status, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_lu_flash_status_show, hat_lu_flash_status_store);
static DEVICE_ATTR(hat_ru_flash_status, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_ru_flash_status_show, hat_ru_flash_status_store);
static DEVICE_ATTR(hat_ld_flash_status, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_ld_flash_status_show, hat_ld_flash_status_store);
static DEVICE_ATTR(hat_rd_flash_status, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, hat_rd_flash_status_show, hat_rd_flash_status_store);
static DEVICE_ATTR(svc_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, svc_flash_show, svc_flash_store);

static DEVICE_ATTR(et3_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ft_flash_left_show, ft_flash_left_store);
static DEVICE_ATTR(et4_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ft_flash_right_show, ft_flash_right_store);
static DEVICE_ATTR(et5_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ft_flash_mid_show, ft_flash_mid_store);

static DEVICE_ATTR(ft_flash_left, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ft_flash_left_show, ft_flash_left_store);
static DEVICE_ATTR(ft_flash_right, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ft_flash_right_show, ft_flash_right_store);
static DEVICE_ATTR(ft_flash_mid, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ft_flash_mid_show, ft_flash_mid_store);
static DEVICE_ATTR(gscam_fsync, S_IRUGO|S_IWUSR|S_IWGRP, gscam_fsync_show, gscam_fsync_store);
static DEVICE_ATTR(etcam_fsync, S_IRUGO|S_IWUSR|S_IWGRP, etcam_fsync_show, etcam_fsync_store);
static DEVICE_ATTR(cam_fsync_mode, S_IRUGO|S_IWUSR|S_IWGRP, cam_fsync_mode_show, cam_fsync_mode_store);
static DEVICE_ATTR(cam_fsync_sleep, S_IRUGO|S_IWUSR|S_IWGRP, cam_fsync_sleep_show, cam_fsync_sleep_store);
static DEVICE_ATTR(cam_boot_rev, S_IRUGO|S_IWUSR|S_IWGRP, cam_boot_rev_show, cam_boot_rev_store);
static DEVICE_ATTR(cam_gdb_enabled, S_IRUGO|S_IWUSR|S_IWGRP, cam_gdb_enabled_show, cam_gdb_enabled_store);
static DEVICE_ATTR(wt_cam_count, S_IRUGO|S_IWUSR|S_IWGRP, wt_cam_count_show, wt_cam_count_store);
static DEVICE_ATTR(sec_ft_fpga_rst_pin, S_IRUGO|S_IWUSR|S_IWGRP, sec_ft_fpga_rst_pin_show, sec_ft_fpga_rst_pin_store);
static DEVICE_ATTR(sec_et_fpga_rst_pin, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_fpga_rst_pin_show, sec_et_fpga_rst_pin_store);
static DEVICE_ATTR(sec_et_irled_l_sel0, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_irled_l_sel0_show, sec_et_irled_l_sel0_store);
static DEVICE_ATTR(sec_et_irled_l_sel1, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_irled_l_sel1_show, sec_et_irled_l_sel1_store);
static DEVICE_ATTR(sec_et_irled_r_sel0, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_irled_r_sel0_show, sec_et_irled_r_sel0_store);
static DEVICE_ATTR(sec_et_irled_r_sel1, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_irled_r_sel1_show, sec_et_irled_r_sel1_store);
static DEVICE_ATTR(sec_het_f_fpga_reset, S_IRUGO|S_IWUSR|S_IWGRP, sec_het_f_fpga_reset_show, sec_het_f_fpga_reset_store);
static DEVICE_ATTR(sec_het_fsync, S_IRUGO|S_IWUSR|S_IWGRP, sec_het_fsync_show, sec_het_fsync_store);
static DEVICE_ATTR(cam_fpga_enabled, 0664, cam_fpga_enabled_show, cam_fpga_enabled_store);
static DEVICE_ATTR(cam_tof_eeprom_write, S_IRUGO|S_IWUSR|S_IWGRP, cam_tof_eeprom_write_show, cam_tof_eeprom_write_store);
static DEVICE_ATTR(cam_vst_left_temperature_check, S_IRUGO|S_IWUSR|S_IWGRP,
	cam_vst_left_temperature_check_show, cam_temperature_check_store);
static DEVICE_ATTR(cam_vst_right_temperature_check, S_IRUGO|S_IWUSR|S_IWGRP,
	cam_vst_right_temperature_check_show, cam_temperature_check_store);
static DEVICE_ATTR(cam_het_front_left_temperature_check, S_IRUGO|S_IWUSR|S_IWGRP,
	cam_het_front_left_temperature_check_show, cam_temperature_check_store);
static DEVICE_ATTR(rear2_tof_temperature_check, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_tof_temperature_check_show, cam_temperature_check_store);
static DEVICE_ATTR(rear_tof_temperature_check, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_tof_temperature_check_show, cam_temperature_check_store);
static DEVICE_ATTR(cam_temperature_check, S_IRUGO|S_IWUSR|S_IWGRP,
	cam_temperature_check_show, cam_temperature_check_store);
static DEVICE_ATTR(sec_et_left_rst_pin, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_left_rst_pin_show, sec_et_left_rst_pin_store);
static DEVICE_ATTR(sec_et_right_rst_pin, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_right_rst_pin_show, sec_et_right_rst_pin_store);
static DEVICE_ATTR(sec_et_irled_left_enable_pin, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_irled_left_enable_pin_show, sec_et_irled_left_enable_pin_store);
static DEVICE_ATTR(sec_et_irled_right_enable_pin, S_IRUGO|S_IWUSR|S_IWGRP, sec_et_irled_right_enable_pin_show, sec_et_irled_right_enable_pin_store);

struct device		*cam_dev_flash;
struct device		*cam_dev_rear;
struct device		*cam_dev_front;

const struct device_attribute *flash_attrs[] = {
	&dev_attr_ft_flash_left,
	&dev_attr_ft_flash_right,
	&dev_attr_ft_flash_mid,
	&dev_attr_et_flash,
	&dev_attr_et_flash_pattern,
	&dev_attr_et2_flash,
	&dev_attr_et2_flash_pattern,
	&dev_attr_et3_flash,
	&dev_attr_et4_flash,
	&dev_attr_et5_flash,
	&dev_attr_et_flash_reg,
	&dev_attr_et2_flash_reg,
	&dev_attr_et_flash_status,
	&dev_attr_et2_flash_status,
	&dev_attr_et_flash_current,
	&dev_attr_et2_flash_current,
	&dev_attr_hat_lu_flash,
	&dev_attr_hat_ru_flash,
	&dev_attr_hat_ld_flash,
	&dev_attr_hat_rd_flash,
	&dev_attr_hat_lu_flash_status,
	&dev_attr_hat_ru_flash_status,
	&dev_attr_hat_ld_flash_status,
	&dev_attr_hat_rd_flash_status,
	&dev_attr_svc_flash,
	NULL, // DO NOT REMOVE
};

const struct device_attribute *rear_attrs[] = {
	&dev_attr_cam_boot_rev,
	&dev_attr_gscam_fsync,
	&dev_attr_etcam_fsync,
	&dev_attr_cam_fsync_mode,
	&dev_attr_cam_fsync_sleep,
	&dev_attr_cam_gdb_enabled,
	&dev_attr_wt_cam_count,
	&dev_attr_cam_fpga_enabled,
	&dev_attr_sec_ft_fpga_rst_pin,
	&dev_attr_sec_et_fpga_rst_pin,
	&dev_attr_sec_et_left_rst_pin,
	&dev_attr_sec_et_right_rst_pin,
	&dev_attr_sec_et_irled_left_enable_pin,
	&dev_attr_sec_et_irled_right_enable_pin,
	&dev_attr_cam_tof_eeprom_write,
	&dev_attr_cam_vst_left_temperature_check,
	&dev_attr_cam_vst_right_temperature_check,
	&dev_attr_cam_het_front_left_temperature_check,
	&dev_attr_rear2_tof_temperature_check,
	&dev_attr_rear_tof_temperature_check,
	&dev_attr_cam_temperature_check,
	&dev_attr_sec_et_irled_l_sel0,
	&dev_attr_sec_et_irled_l_sel1,
	&dev_attr_sec_et_irled_r_sel0,
	&dev_attr_sec_et_irled_r_sel1,
	&dev_attr_sec_het_f_fpga_reset,
	&dev_attr_sec_het_fsync,
	&dev_attr_rear_camtype,
	&dev_attr_rear_camfw,
	&dev_attr_rear_checkfw_user,
	&dev_attr_rear_checkfw_factory,
	&dev_attr_rear_camfw_full,
	&dev_attr_rear_camfw_load,
	&dev_attr_rear_calcheck,
	&dev_attr_rear_moduleinfo,
	&dev_attr_isp_core,
	&dev_attr_rear_caminfo,
	&dev_attr_rear_afcal,
	&dev_attr_rear_paf_offset_far,
	&dev_attr_rear_paf_offset_mid,
	&dev_attr_rear_paf_cal_check,
	&dev_attr_rear_f2_paf_offset_far,
	&dev_attr_rear_f2_paf_offset_mid,
	&dev_attr_rear_f2_paf_cal_check,
	&dev_attr_rear_sensorid_exif,
	&dev_attr_rear_moduleid,
	&dev_attr_rear_mtf_exif,
	&dev_attr_rear_mtf2_exif,
	&dev_attr_ssrm_camera_info,
	&dev_attr_supported_cameraIds,
	&dev_attr_binary_mode,
	&dev_attr_dtof_pd_value,
	&dev_attr_rear3_camfw,
	&dev_attr_rear3_camfw_full,
	&dev_attr_rear3_afcal,
	&dev_attr_rear3_tilt,
	&dev_attr_rear3_caminfo,
	&dev_attr_rear3_moduleid,
	&dev_attr_rear3_camtype,
	&dev_attr_rear3_moduleinfo,
	&dev_attr_rear3_mtf_exif,
	&dev_attr_rear3_sensorid_exif,
	&dev_attr_rear3_dualcal,
	&dev_attr_rear3_dualcal_size,
	&dev_attr_rear3_paf_cal_check,
	&dev_attr_rear2_caminfo,
	&dev_attr_rear2_moduleinfo,
	&dev_attr_rear2_sensorid_exif,
	&dev_attr_rear2_mtf_exif,
	&dev_attr_rear2_moduleid,
	&dev_attr_rear2_camfw,
	&dev_attr_rear2_checkfw_user,
	&dev_attr_rear2_checkfw_factory,
	&dev_attr_rear2_camfw_full,
	&dev_attr_rear2_dualcal,
	&dev_attr_rear2_dualcal_size,
	&dev_attr_rear2_tilt,
	&dev_attr_rear2_camtype,
	&dev_attr_rear4_caminfo,
	&dev_attr_rear4_moduleinfo,
	&dev_attr_rear4_sensorid_exif,
	&dev_attr_rear4_mtf_exif,
	&dev_attr_rear4_moduleid,
	&dev_attr_rear4_camfw,
	&dev_attr_rear4_checkfw_user,
	&dev_attr_rear4_checkfw_factory,
	&dev_attr_rear4_camfw_full,
	&dev_attr_rear4_camtype,
	&dev_attr_rear5_caminfo,
	&dev_attr_rear5_moduleinfo,
	&dev_attr_rear5_sensorid_exif,
	&dev_attr_rear5_mtf_exif,
	&dev_attr_rear5_moduleid,
	&dev_attr_rear5_camfw,
	&dev_attr_rear5_checkfw_user,
	&dev_attr_rear5_checkfw_factory,
	&dev_attr_rear5_camfw_full,
	&dev_attr_rear5_camtype,
	&dev_attr_rear6_caminfo,
	&dev_attr_rear6_moduleinfo,
	&dev_attr_rear6_sensorid_exif,
	&dev_attr_rear6_mtf_exif,
	&dev_attr_rear6_moduleid,
	&dev_attr_rear6_camfw,
	&dev_attr_rear6_checkfw_user,
	&dev_attr_rear6_checkfw_factory,
	&dev_attr_rear6_camfw_full,
	&dev_attr_rear6_camtype,
	&dev_attr_rear7_camfw,
	&dev_attr_rear7_camfw_full,
	&dev_attr_rear7_caminfo,
	&dev_attr_rear7_moduleid,
	&dev_attr_rear7_camtype,
	&dev_attr_rear7_moduleinfo,
	&dev_attr_rear7_mtf_exif,
	&dev_attr_rear7_sensorid_exif,
	&dev_attr_rear8_camfw,
	&dev_attr_rear8_camfw_full,
	&dev_attr_rear8_caminfo,
	&dev_attr_rear8_moduleid,
	&dev_attr_rear8_camtype,
	&dev_attr_rear8_moduleinfo,
	&dev_attr_rear8_mtf_exif,
	&dev_attr_rear8_sensorid_exif,
	&dev_attr_rear_tof_caminfo,
	&dev_attr_rear_tof_moduleinfo,
	&dev_attr_rear_tof_sensorid_exif,
	&dev_attr_rear_tof_mtf_exif,
	&dev_attr_rear_tof_camfw,
	&dev_attr_rear_tof_checkfw_user,
	&dev_attr_rear_tof_checkfw_factory,
	&dev_attr_rear_tof_camfw_full,
	&dev_attr_rear_tof_camtype,
	&dev_attr_rear_tof_camfw_load,
	&dev_attr_rear_tof_moduleid,
	&dev_attr_rear_tof_cal_uid,
	&dev_attr_rear_tof_cal,
	&dev_attr_rear_tof_cal_extra,
	&dev_attr_rear_tof_cal_size,
	&dev_attr_rear_tof_cal_result,
	&dev_attr_rear_tof_tilt,
	&dev_attr_rear_tof_dual_cal,
	&dev_attr_rear_tof_ld_onoff,
	&dev_attr_rear_tof_ae_value,
	&dev_attr_rear_tof_check_pd,
	&dev_attr_rear_tof_error_flag,
	&dev_attr_rear_tof_auto_zeroing,
	&dev_attr_rear_tof_ld_iout,
	&dev_attr_rear_tof_cg_cal,
	&dev_attr_rear_tof_vrld_abs,
	&dev_attr_rear_tof_vrld_delta,
	&dev_attr_rear_tof_vrld_pmic,
	&dev_attr_rear_tof_gbd,
	&dev_attr_rear_tof_freq,
	&dev_attr_rear_tof_get_validation,
	&dev_attr_rear2_tof_caminfo,
	&dev_attr_rear2_tof_moduleinfo,
	&dev_attr_rear2_tof_sensorid_exif,
	&dev_attr_rear2_tof_mtf_exif,
	&dev_attr_rear2_tof_camfw,
	&dev_attr_rear2_tof_checkfw_user,
	&dev_attr_rear2_tof_checkfw_factory,
	&dev_attr_rear2_tof_camfw_full,
	&dev_attr_rear2_tof_camtype,
	&dev_attr_rear2_tof_camfw_load,
	&dev_attr_rear2_tof_moduleid,
	&dev_attr_rear2_tof_cal_uid,
	&dev_attr_rear2_tof_cal,
	&dev_attr_rear2_tof_cal_extra,
	&dev_attr_rear2_tof_cal_size,
	&dev_attr_rear2_tof_cal_result,
	&dev_attr_rear2_tof_tilt,
	&dev_attr_rear2_tof_dual_cal,
	&dev_attr_rear2_tof_ld_onoff,
	&dev_attr_rear2_tof_ae_value,
	&dev_attr_rear2_tof_check_pd,
	&dev_attr_rear2_tof_check_pd_2,
	&dev_attr_rear2_tof_check_pd_3,
	&dev_attr_rear2_tof_check_pd_4,
	&dev_attr_rear2_tof_error_flag,
	&dev_attr_rear2_tof_fps,
	&dev_attr_rear2_tof_get_validation,
#if defined(CONFIG_CAMERA_CDR_TEST)
	&dev_attr_cam_cdr_value,
	&dev_attr_cam_cdr_result,
	&dev_attr_cam_cdr_fastaec,
#endif
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	&dev_attr_rear_hwparam,
	&dev_attr_rear2_hwparam,
	&dev_attr_rear3_hwparam,
	&dev_attr_rear4_hwparam,
	&dev_attr_rear5_hwparam,
	&dev_attr_rear6_hwparam,
	&dev_attr_rear7_hwparam,
	&dev_attr_rear8_hwparam,
	&dev_attr_rear_tof_hwparam,
	&dev_attr_rear2_tof_hwparam,
	&dev_attr_cam_wifi_info,
	&dev_attr_common_hwparam,
#endif
	NULL, // DO NOT REMOVE
};

const struct device_attribute *front_attrs[] = {
	&dev_attr_front_camtype,
	&dev_attr_front_camfw,
	&dev_attr_front_camfw_full,
	&dev_attr_front_checkfw_user,
	&dev_attr_front_checkfw_factory,
	&dev_attr_front_moduleinfo,
	&dev_attr_front_paf_cal_check,
	&dev_attr_front_caminfo,
	&dev_attr_front_sensorid_exif,
	&dev_attr_front_moduleid,
	&dev_attr_front_mtf_exif,
	&dev_attr_front2_camtype,
	&dev_attr_front2_camfw,
	&dev_attr_front2_camfw_full,
	&dev_attr_front2_checkfw_user,
	&dev_attr_front2_checkfw_factory,
	&dev_attr_front2_moduleinfo,
	&dev_attr_front2_caminfo,
	&dev_attr_front2_sensorid_exif,
	&dev_attr_front2_mtf_exif,
	&dev_attr_front2_moduleid,
	&dev_attr_front3_camtype,
	&dev_attr_front3_camfw,
	&dev_attr_front3_camfw_full,
	&dev_attr_front3_checkfw_user,
	&dev_attr_front3_checkfw_factory,
	&dev_attr_front3_moduleinfo,
	&dev_attr_front3_caminfo,
	&dev_attr_front3_moduleid,
	&dev_attr_front3_mtf_exif,
	&dev_attr_front3_sensorid_exif,
	&dev_attr_front4_caminfo,
	&dev_attr_front4_moduleinfo,
	&dev_attr_front4_sensorid_exif,
	&dev_attr_front4_mtf_exif,
	&dev_attr_front4_moduleid,
	&dev_attr_front4_camfw,
	&dev_attr_front4_checkfw_user,
	&dev_attr_front4_checkfw_factory,
	&dev_attr_front4_camfw_full,
	&dev_attr_front4_camtype,
	&dev_attr_front5_caminfo,
	&dev_attr_front5_moduleinfo,
	&dev_attr_front5_sensorid_exif,
	&dev_attr_front5_mtf_exif,
	&dev_attr_front5_moduleid,
	&dev_attr_front5_camfw,
	&dev_attr_front5_checkfw_user,
	&dev_attr_front5_checkfw_factory,
	&dev_attr_front5_camfw_full,
	&dev_attr_front5_camtype,
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	&dev_attr_front_hwparam,
	&dev_attr_front2_hwparam,
	&dev_attr_front3_hwparam,
	&dev_attr_front4_hwparam,
#endif
	NULL, // DO NOT REMOVE
};

static struct attribute *svc_cam_attrs[] = {
	&dev_attr_SVC_rear_module.attr,
	&dev_attr_SVC_front_module.attr,
	NULL, // DO NOT REMOVE
};

static struct attribute_group svc_cam_group = {
	.attrs = svc_cam_attrs,
};

static const struct attribute_group *svc_cam_groups[] = {
	&svc_cam_group,
	NULL, // DO NOT REMOVE
};

static void svc_cam_release(struct device *dev)
{
	kfree(dev);
}

int svc_cheating_prevent_device_file_create(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct device *dev;
	int err;

	/* To find SVC kobject */
	struct kobject *top_kobj = NULL;

	if(is_dev == NULL) {
		SYS_ERR("[SVC] Error! cam-cci-driver module does not exist\n");
		return -ENODEV;
	}

	top_kobj = &is_dev->kobj.kset->kobj;

	svc_sd = sysfs_get_dirent(top_kobj->sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", top_kobj);
		if (IS_ERR_OR_NULL(data)) {
			SYS_INFO("[SVC] Failed to create sys/devices/svc already exist svc : 0x%pK\n", data);
		} else {
			SYS_INFO("[SVC] Success to create sys/devices/svc svc : 0x%pK\n", data);
		}
	} else {
		data = (struct kobject *)svc_sd->priv;
		SYS_INFO("[SVC] Success to find svc_sd : 0x%pK SVC : 0x%pK\n", svc_sd, data);
	}

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev) {
		SYS_ERR("[SVC] Error allocating svc_ap device\n");
		return -ENOMEM;
	}

	err = dev_set_name(dev, "Camera");
	if (err < 0) {
		SYS_ERR("[SVC] Error dev_set_name\n");
		goto err_name;
	}

	dev->kobj.parent = data;
	dev->groups = svc_cam_groups;
	dev->release = svc_cam_release;

	err = device_register(dev);
	if (err < 0) {
		SYS_ERR("[SVC] Error device_register\n");
		goto err_dev_reg;
	}

	return 0;

err_dev_reg:
	put_device(dev);
err_name:
	kfree(dev);
	dev = NULL;
	return err;
}

int cam_device_create_files(struct device *device,
	const struct device_attribute **attrs)
{
	int ret = 0, i = 0;

	if (device == NULL) {
		SYS_ERR("device is null!\n");
		return ret;
	}

	for (i = 0; attrs[i]; i++) {
		if (device_create_file(device, attrs[i]) < 0) {
			SYS_ERR("Failed to create device file!(%s)!\n",
				attrs[i]->attr.name);
			ret = -ENODEV;
		}
	}
	return ret;
}

int cam_device_remove_file(struct device *device,
	const struct device_attribute **attrs)
{
	int ret = 0;

	if (device == NULL) {
		SYS_ERR("device is null!\n");
		return ret;
	}

	for (; *attrs; attrs++)
		device_remove_file(device, *attrs);
	return ret;
}

int cam_sysfs_init_module(void)
{
	int ret = 0;
	SYS_ERR("cam_sysfs_init start !\n");

	svc_cheating_prevent_device_file_create();

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class))
			SYS_ERR("failed to create device cam_dev_rear!\n");
	}

	cam_dev_flash = device_create(camera_class, NULL,
		0, NULL, "flash");
	ret |= cam_device_create_files(cam_dev_flash, flash_attrs);

	cam_dev_rear = device_create(camera_class, NULL,
		1, NULL, "rear");
	ret |= cam_device_create_files(cam_dev_rear, rear_attrs);

	cam_dev_front = device_create(camera_class, NULL,
		2, NULL, "front");
	ret |= cam_device_create_files(cam_dev_front, front_attrs);

	SYS_ERR("cam_sysfs_init done (ret=%d)!\n", ret);

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	camera_hw_param_check_avail_cam();
#endif
	return ret;
}

void cam_sysfs_exit_module(void)
{
	cam_device_remove_file(cam_dev_flash, flash_attrs);
	cam_device_remove_file(cam_dev_rear, rear_attrs);
	cam_device_remove_file(cam_dev_front, front_attrs);
}

MODULE_DESCRIPTION("CAM_SYSFS");
MODULE_LICENSE("GPL v2");
