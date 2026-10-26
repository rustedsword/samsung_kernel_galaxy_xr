// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>

#include "cam_eeprom_module.h"

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
#include "cam_hw_bigdata.h"
#endif

int rear2_tof_zero_distance_value1 = 0;
int rear2_tof_zero_distance_value2 = 0;
int eeprom_subRef_radialDepth = 0;
uint32_t otp_data_s5k931_valid_bank = 1;

#if defined(CONFIG_SAMSUNG_CAMERA_OTP)
#include "s5k931_otp.h"
#include "ovog0tb_otp.h"

//VST
struct cam_sensor_i2c_reg_setting load_s5k931_otp_setfile = {
	load_sensor_s5k931_otp_setfile_reg,
	sizeof(load_sensor_s5k931_otp_setfile_reg)/sizeof(load_sensor_s5k931_otp_setfile_reg[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	50
};

struct cam_sensor_i2c_reg_setting s5k931_otp_init_setting1 = {
	s5k931_otp_init_reg1,
	sizeof(s5k931_otp_init_reg1)/sizeof(s5k931_otp_init_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	10
};

struct cam_sensor_i2c_reg_setting s5k931_otp_finish_setting1 = {
	s5k931_otp_finish_reg1,
	sizeof(s5k931_otp_finish_reg1)/sizeof(s5k931_otp_finish_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	10
};

struct cam_sensor_i2c_reg_setting load_ovog0tb_otp_setfile = {
	load_sensor_ovog0tb_otp_setfile_reg,
	sizeof(load_sensor_ovog0tb_otp_setfile_reg)/sizeof(load_sensor_ovog0tb_otp_setfile_reg[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	50
};

struct cam_sensor_i2c_reg_setting ovog0tb_otp_init_setting1 = {
	ovog0tb_otp_init_reg1,
	sizeof(ovog0tb_otp_init_reg1)/sizeof(ovog0tb_otp_init_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	10
};

struct cam_sensor_i2c_reg_setting ovog0tb_otp_finish_setting1 = {
	ovog0tb_otp_finish_reg1,
	sizeof(ovog0tb_otp_finish_reg1)/sizeof(ovog0tb_otp_finish_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	10
};

#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
char rear2_cam_cal_check[SYSFS_FW_VER_SIZE] = "NULL";
#endif

static unsigned int system_rev __read_mostly;

/*static int __init sec_hw_rev_setup(char *p)
{
	int ret;

	ret = kstrtouint(p, 0, &system_rev);
	if (unlikely(ret < 0)) {
		pr_warn("androidboot.revision is malformed (%s)\n", p);
		return -EINVAL;
	}

	pr_info("androidboot.revision %x\n", system_rev);

	return 0;
}*/
//early_param("androidboot.revision", sec_hw_rev_setup);

static unsigned int sec_hw_rev(void)
{
	return system_rev;
}

unsigned int board_rev;

/* rear */
char hw_phone_info[HW_INFO_MAX_SIZE] = HW_INFO;
char sw_phone_info[SW_INFO_MAX_SIZE] = SW_INFO;
char vendor_phone_info[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO;
char process_phone_info[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO;

/* rear2 */
char hw_phone_info_vst2[HW_INFO_MAX_SIZE] = HW_INFO_VST2;
char sw_phone_info_vst2[SW_INFO_MAX_SIZE] = SW_INFO_VST2;
char vendor_phone_info_vst2[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_VST2;
char process_phone_info_vst2[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_VST2;

/* rear3 */
char hw_phone_info_het1[HW_INFO_MAX_SIZE] = HW_INFO_HET1;
char sw_phone_info_het1[SW_INFO_MAX_SIZE] = SW_INFO_HET1;
char vendor_phone_info_het1[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_HET1;
char process_phone_info_het1[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_HET1;

/* rear4 */
char hw_phone_info_het2[HW_INFO_MAX_SIZE] = HW_INFO_HET2;
char sw_phone_info_het2[SW_INFO_MAX_SIZE] = SW_INFO_HET2;
char vendor_phone_info_het2[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_HET2;
char process_phone_info_het2[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_HET2;

/* rear5 */
char hw_phone_info_het3[HW_INFO_MAX_SIZE] = HW_INFO_HET3;
char sw_phone_info_het3[SW_INFO_MAX_SIZE] = SW_INFO_HET3;
char vendor_phone_info_het3[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_HET3;
char process_phone_info_het3[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_HET3;

/* rear6 */
char hw_phone_info_het4[HW_INFO_MAX_SIZE] = HW_INFO_HET4;
char sw_phone_info_het4[SW_INFO_MAX_SIZE] = SW_INFO_HET4;
char vendor_phone_info_het4[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_HET4;
char process_phone_info_het4[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_HET4;

/* rear7 */
char hw_phone_info_het5[HW_INFO_MAX_SIZE] = HW_INFO_HET5;
char sw_phone_info_het5[SW_INFO_MAX_SIZE] = SW_INFO_HET5;
char vendor_phone_info_het5[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_HET5;
char process_phone_info_het5[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_HET5;

/* rear8 */
char hw_phone_info_het6[HW_INFO_MAX_SIZE] = HW_INFO_HET6;
char sw_phone_info_het6[SW_INFO_MAX_SIZE] = SW_INFO_HET6;
char vendor_phone_info_het6[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_HET6;
char process_phone_info_het6[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_HET6;

#if defined(CONFIG_SAMSUNG_REAR_TOF)
/* rear_tof */
char hw_phone_info_tof[HW_INFO_MAX_SIZE] = HW_INFO_TOF;
char sw_phone_info_tof[SW_INFO_MAX_SIZE] = SW_INFO_TOF;
char vendor_phone_info_tof[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_TOF;
char process_phone_info_tof[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_TOF;

/* rear_dtof */
char hw_phone_info_dtof[HW_INFO_MAX_SIZE] = HW_INFO_DTOF;
char sw_phone_info_dtof[SW_INFO_MAX_SIZE] = SW_INFO_DTOF;
char vendor_phone_info_dtof[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO_DTOF;
char process_phone_info_dtof[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO_DTOF;
#endif

/* Module Manufacturer information  */
char rear_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_cam_cal_check[SYSFS_FW_VER_SIZE] = "NULL";

char rear2_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear2_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear2_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char rear3_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear3_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear3_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char rear4_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear4_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear4_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char rear5_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear5_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear5_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char rear6_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear6_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear6_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char rear7_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear7_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear7_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char rear8_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear8_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear8_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char front_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char front2_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front2_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front2_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char front3_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front3_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front3_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char front4_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front4_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front4_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

char front5_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front5_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char front5_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";

#if defined(CONFIG_SAMSUNG_REAR_TOF)
char rear_tof_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_tof_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_tof_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_tof_cam_cal_check[SYSFS_FW_VER_SIZE] = "NULL";
uint8_t result1, result2, result3, result4;

char rear_dtof_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_dtof_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_dtof_load_fw_ver[FROM_MODULE_FW_INFO_SIZE + 1] = "\0";
char rear_dtof_cam_cal_check[SYSFS_FW_VER_SIZE] = "NULL";
uint8_t d_result1, d_result2, d_result3, d_result4;
#endif

char front_cam_cal_check[SYSFS_FW_VER_SIZE] = "NULL";

/*front */
char front_hw_phone_info[HW_INFO_MAX_SIZE] = FRONT_HW_INFO;
char front_sw_phone_info[SW_INFO_MAX_SIZE] = FRONT_SW_INFO;
char front_vendor_phone_info[VENDOR_INFO_MAX_SIZE] = FRONT_VENDOR_INFO;
char front_process_phone_info[PROCESS_INFO_MAX_SIZE] = FRONT_PROCESS_INFO;

/*front2 */
char front2_hw_phone_info[HW_INFO_MAX_SIZE] = FRONT2_HW_INFO;
char front2_sw_phone_info[SW_INFO_MAX_SIZE] = FRONT2_SW_INFO;
char front2_vendor_phone_info[VENDOR_INFO_MAX_SIZE] = FRONT2_VENDOR_INFO;
char front2_process_phone_info[PROCESS_INFO_MAX_SIZE] = FRONT2_PROCESS_INFO;

/*front3 */
char front3_hw_phone_info[HW_INFO_MAX_SIZE] = FRONT3_HW_INFO;
char front3_sw_phone_info[SW_INFO_MAX_SIZE] = FRONT3_SW_INFO;
char front3_vendor_phone_info[VENDOR_INFO_MAX_SIZE] = FRONT3_VENDOR_INFO;
char front3_process_phone_info[PROCESS_INFO_MAX_SIZE] = FRONT3_PROCESS_INFO;

/*front4 */
char front4_hw_phone_info[HW_INFO_MAX_SIZE] = FRONT4_HW_INFO;
char front4_sw_phone_info[SW_INFO_MAX_SIZE] = FRONT4_SW_INFO;
char front4_vendor_phone_info[VENDOR_INFO_MAX_SIZE] = FRONT4_VENDOR_INFO;
char front4_process_phone_info[PROCESS_INFO_MAX_SIZE] = FRONT4_PROCESS_INFO;

/*front5 */
char front5_hw_phone_info[HW_INFO_MAX_SIZE] = FRONT5_HW_INFO;
char front5_sw_phone_info[SW_INFO_MAX_SIZE] = FRONT5_SW_INFO;
char front5_vendor_phone_info[VENDOR_INFO_MAX_SIZE] = FRONT5_VENDOR_INFO;
char front5_process_phone_info[PROCESS_INFO_MAX_SIZE] = FRONT5_PROCESS_INFO;

#ifdef CAM_EEPROM_DBG_DUMP
int cam_eeprom_dump(uint32_t subdev_id, uint8_t *mapdata, uint32_t addr, uint32_t size)
{
	int rc = 0;
	int j;

	if (mapdata == NULL) {
		CAM_ERR(CAM_EEPROM, "mapdata is NULL");
		return -1;
	}
	if (size == 0) {
		CAM_ERR(CAM_EEPROM, "size is 0");
		return -1;
	}

	CAM_DBG(CAM_EEPROM, "subdev_id: %d, eeprom dump addr = 0x%04X, total read size = %d", subdev_id, addr, size);
	for (j = 0; j < size; j++)
		CAM_DBG(CAM_EEPROM, "addr = 0x%04X, data = 0x%02X", (addr+j), mapdata[addr+j]);

	return rc;
}
#endif

int cam_eeprom_update_module_info(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int rc = 0;
	uint8_t map_ver = 0;
	int i = 0;

	uint8_t loadfrom = 'N';
	char cal_ver[12] = "";
	uint8_t sensor_ver[2] = {0,};
	uint8_t dll_ver[2] = {0,};
	char ideal_ver[12] = "";

	uint32_t normal_is_supported = 0;
	uint8_t  normal_cri_rev = 0;
	uint8_t  bVerNull = FALSE;
	board_rev = sec_hw_rev();

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	rc = of_property_read_u32(e_ctrl->soc_info.dev->of_node, "boot_revision", &e_ctrl->soc_info.hw_version);
	CAM_DBG(CAM_EEPROM, "hw_boot_version %d ", e_ctrl->soc_info.hw_version);

	normal_is_supported = CAMERA_NORMAL_CAL_CRC;
	CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x normal_is_supported=0x%x board_rev=0x%x",
		e_ctrl->soc_info.index, map_ver, normal_is_supported, board_rev);

    if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX564 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_VST_LEFT) {
        map_ver = e_ctrl->cal_data.mapdata[REAR_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX564 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_VST_RIGHT) {
        map_ver = e_ctrl->cal_data.mapdata[REAR2_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETFRONT_LEFT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_TOP_L))) {
        map_ver = e_ctrl->cal_data.mapdata[REAR3_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETFRONT_RIGHT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_TOP_R))) {
        map_ver = e_ctrl->cal_data.mapdata[REAR4_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETDOWN_LEFT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_MID_L))) {
        map_ver = e_ctrl->cal_data.mapdata[REAR5_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETDOWN_RIGHT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_MID_R))) {
        map_ver = e_ctrl->cal_data.mapdata[REAR6_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_BTM_L) {
        map_ver = e_ctrl->cal_data.mapdata[REAR7_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_BTM_R) {
        map_ver = e_ctrl->cal_data.mapdata[REAR8_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
#if defined(CONFIG_SAMSUNG_REAR_TOF)
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K63D && e_ctrl->soc_info.index == CAM_EEPROM_IDX_BACK_TOF) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K63D && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ITOF)) {
        map_ver = e_ctrl->cal_data.mapdata[REAR_TOF_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX611 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_DTOF) {
        map_ver = e_ctrl->cal_data.mapdata[REAR_DTOF_CAM_MAP_VERSION_ADDR];
        if(map_ver >= 80 || !isalnum(map_ver)) {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
            map_ver = '0';
        } else {
            CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
        }
    }
#endif
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LEFT) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LU) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LU_FACTORY)) {// Front cam
		map_ver = e_ctrl->cal_data.mapdata[FRONT_CAM_MAP_VERSION_ADDR];
		if(map_ver >= 80 || !isalnum(map_ver)) {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
			map_ver = '0';
		} else {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
		}
	}
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RIGHT) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RU) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RU_FACTORY)) {
		map_ver = e_ctrl->cal_data.mapdata[FRONT2_CAM_MAP_VERSION_ADDR];
		if(map_ver >= 80 || !isalnum(map_ver)) {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
			map_ver = '0';
		} else {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
		}
	}
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD_FACTORY)) {
		map_ver = e_ctrl->cal_data.mapdata[FRONT3_CAM_MAP_VERSION_ADDR];
		if(map_ver >= 80 || !isalnum(map_ver)) {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
			map_ver = '0';
		} else {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
		}
	}
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD_FACTORY)) {
		map_ver = e_ctrl->cal_data.mapdata[FRONT4_CAM_MAP_VERSION_ADDR];
		if(map_ver >= 80 || !isalnum(map_ver)) {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
			map_ver = '0';
		} else {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
		}
	}
	else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && e_ctrl->soc_info.index == CAM_EEPROM_IDX_FT_MID) {
		map_ver = e_ctrl->cal_data.mapdata[FRONT5_CAM_MAP_VERSION_ADDR];
		if(map_ver >= 80 || !isalnum(map_ver)) {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", e_ctrl->soc_info.index, map_ver);
			map_ver = '0';
		} else {
			CAM_DBG(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", e_ctrl->soc_info.index, map_ver, map_ver);
		}
	}

    if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX564 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_VST_LEFT) {  //Vst Sensor
    /* rear sensor id */
        memcpy(rear_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_sensor_id[0], rear_sensor_id[1], rear_sensor_id[2], rear_sensor_id[3],
            rear_sensor_id[4], rear_sensor_id[5], rear_sensor_id[6], rear_sensor_id[7],
            rear_sensor_id[8], rear_sensor_id[9], rear_sensor_id[10], rear_sensor_id[11],
            rear_sensor_id[12], rear_sensor_id[13], rear_sensor_id[14], rear_sensor_id[15]);

    /* rear module id */
        memcpy(rear_module_id, &e_ctrl->cal_data.mapdata[FROM_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
            rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
            rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
#endif
    /* rear mtf exif */
        memcpy(rear_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR_MTF_ADDR], FROM_MTF_SIZE);
        rear_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear mtf exif = %s", rear_mtf_exif);

    /* rear mtf2 exif */
        memcpy(rear_mtf2_exif, &e_ctrl->cal_data.mapdata[FROM_REAR_MTF2_ADDR], FROM_MTF_SIZE);
        rear_mtf2_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear mtf2 exif = %s", rear_mtf2_exif);

    /* rear manufacturer info */
        memcpy(rear_fw_ver, &e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_fw_ver[0], rear_fw_ver[1], rear_fw_ver[2], rear_fw_ver[3], rear_fw_ver[4],
            rear_fw_ver[5], rear_fw_ver[6], rear_fw_ver[7], rear_fw_ver[8], rear_fw_ver[9],
            rear_fw_ver[10]);

        snprintf(rear_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info, sw_phone_info, vendor_phone_info, process_phone_info);
        rear_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_phone_fw_ver[0], rear_phone_fw_ver[1], rear_phone_fw_ver[2], rear_phone_fw_ver[3], rear_phone_fw_ver[4],
            rear_phone_fw_ver[5], rear_phone_fw_ver[6], rear_phone_fw_ver[7], rear_phone_fw_ver[8], rear_phone_fw_ver[9],
            rear_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear_phone_fw_ver[0], rear_phone_fw_ver[1], rear_phone_fw_ver[2], rear_phone_fw_ver[3], rear_phone_fw_ver[4],
            rear_phone_fw_ver[5], rear_phone_fw_ver[6], rear_phone_fw_ver[7], rear_phone_fw_ver[8], rear_phone_fw_ver[9],
            rear_phone_fw_ver[10]);
#endif

        if (strncmp(rear_phone_fw_ver, rear_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear_load_fw_ver, rear_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear_load_fw_ver, rear_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + i];
            }
            if(rear_phone_fw_ver[i] >= 0x80
                || !isalnum(rear_phone_fw_ver[i]))
                rear_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV;
        strcpy(ideal_ver, rear_phone_fw_ver);
        if(rear_fw_ver[9] < 0x80 && isalnum(rear_fw_ver[9])) {
            ideal_ver[9] = rear_fw_ver[9];
        }
        if(rear_fw_ver[10] < 0x80 && isalnum(rear_fw_ver[10])) {
            ideal_ver[10] = rear_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear info = %s", module_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam_fw_ver, "%s %s\n", rear_fw_ver, rear_load_fw_ver); //need check phone version
        sprintf(cam_fw_full_ver, "%s %s %s\n", rear_fw_ver, rear_phone_fw_ver, rear_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear_fw_ver[0], rear_fw_ver[1], rear_fw_ver[2], rear_fw_ver[3], rear_fw_ver[4],
        rear_fw_ver[5], rear_fw_ver[6], rear_fw_ver[7], rear_fw_ver[8], rear_fw_ver[9],
        rear_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX564 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_VST_RIGHT) {  //Vst Sensor
    /* rear2 sensor id */
        memcpy(rear2_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR2_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear2_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear2 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear2_sensor_id[0], rear2_sensor_id[1], rear2_sensor_id[2], rear2_sensor_id[3],
            rear2_sensor_id[4], rear2_sensor_id[5], rear2_sensor_id[6], rear2_sensor_id[7],
            rear2_sensor_id[8], rear2_sensor_id[9], rear2_sensor_id[10], rear2_sensor_id[11],
            rear2_sensor_id[12], rear2_sensor_id[13], rear2_sensor_id[14], rear2_sensor_id[15]);

	/* rear2 module id */
        memcpy(rear2_module_id, &e_ctrl->cal_data.mapdata[REAR2_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear2_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear2_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3], rear2_module_id[4],
            rear2_module_id[5], rear2_module_id[6], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear2_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3], rear2_module_id[4],
            rear2_module_id[5], rear2_module_id[6], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9]);
#endif
    /* rear2 mtf exif */
        memcpy(rear2_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR2_MTF_ADDR], FROM_MTF_SIZE);
        rear2_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear2 mtf exif = %s", rear2_mtf_exif);

    /* rear2 manufacturer info */
        memcpy(rear2_fw_ver, &e_ctrl->cal_data.mapdata[REAR2_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear2_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear2 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear2_fw_ver[0], rear2_fw_ver[1], rear2_fw_ver[2], rear2_fw_ver[3], rear2_fw_ver[4],
            rear2_fw_ver[5], rear2_fw_ver[6], rear2_fw_ver[7], rear2_fw_ver[8], rear2_fw_ver[9],
            rear2_fw_ver[10]);

        snprintf(rear2_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_vst2, sw_phone_info_vst2, vendor_phone_info_vst2, process_phone_info_vst2);
        rear2_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear2_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear2_phone_fw_ver[0], rear2_phone_fw_ver[1], rear2_phone_fw_ver[2], rear2_phone_fw_ver[3], rear2_phone_fw_ver[4],
            rear2_phone_fw_ver[5], rear2_phone_fw_ver[6], rear2_phone_fw_ver[7], rear2_phone_fw_ver[8], rear2_phone_fw_ver[9],
            rear2_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear2_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear2_phone_fw_ver[0], rear2_phone_fw_ver[1], rear2_phone_fw_ver[2], rear2_phone_fw_ver[3], rear2_phone_fw_ver[4],
            rear2_phone_fw_ver[5], rear2_phone_fw_ver[6], rear2_phone_fw_ver[7], rear2_phone_fw_ver[8], rear2_phone_fw_ver[9],
            rear2_phone_fw_ver[10]);
#endif

        if (strncmp(rear2_phone_fw_ver, rear2_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear2_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear2_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear2_load_fw_ver, rear2_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear2_load_fw_ver, rear2_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR2_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR2_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR2_MODULE_FW_VERSION + i];
            }
            if(rear2_phone_fw_ver[i] >= 0x80
                || !isalnum(rear2_phone_fw_ver[i]))
                rear2_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear2_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR2_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_VST2;
        strcpy(ideal_ver, rear2_phone_fw_ver);
        if(rear2_fw_ver[9] < 0x80 && isalnum(rear2_fw_ver[9])) {
            ideal_ver[9] = rear2_fw_ver[9];
        }
        if(rear2_fw_ver[10] < 0x80 && isalnum(rear2_fw_ver[10])) {
            ideal_ver[10] = rear2_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module2_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear2 info = %s", module2_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam2_fw_ver, "%s %s\n", rear2_fw_ver, rear2_load_fw_ver); //need check phone version
        sprintf(cam2_fw_full_ver, "%s %s %s\n", rear2_fw_ver, rear2_phone_fw_ver, rear2_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear2 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear2_fw_ver[0], rear2_fw_ver[1], rear2_fw_ver[2], rear2_fw_ver[3], rear2_fw_ver[4],
        rear2_fw_ver[5], rear2_fw_ver[6], rear2_fw_ver[7], rear2_fw_ver[8], rear2_fw_ver[9],
        rear2_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETFRONT_LEFT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_TOP_L))) {  //HET Sensor
    /* rear3 otp data valid bank check */
        if (!otp_data_s5k931_valid_bank)
        {
            snprintf(rear3_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
            rear3_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
            snprintf(rear3_module_id, FROM_MODULE_ID_SIZE, "FF");
            rear3_module_id[FROM_MODULE_ID_SIZE] = '\0';
            snprintf(module3_info, SYSFS_MODULE_INFO_SIZE, "FF");
            module3_info[SYSFS_MODULE_INFO_SIZE] = '\0';
            CAM_INFO(CAM_EEPROM, "rear3 has invalid OTP bank data");
            return rc;
        }

    /* rear3 sensor id */
        memcpy(rear3_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR3_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear3_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear3 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear3_sensor_id[0], rear3_sensor_id[1], rear3_sensor_id[2], rear3_sensor_id[3],
            rear3_sensor_id[4], rear3_sensor_id[5], rear3_sensor_id[6], rear3_sensor_id[7],
            rear3_sensor_id[8], rear3_sensor_id[9], rear3_sensor_id[10], rear3_sensor_id[11],
            rear3_sensor_id[12], rear3_sensor_id[13], rear3_sensor_id[14], rear3_sensor_id[15]);

	/* rear3 module id */
        memcpy(rear3_module_id, &e_ctrl->cal_data.mapdata[REAR3_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear3_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear3_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3], rear3_module_id[4],
            rear3_module_id[5], rear3_module_id[6], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear3_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3], rear3_module_id[4],
            rear3_module_id[5], rear3_module_id[6], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9]);
#endif
    /* rear3 mtf exif */
        memcpy(rear3_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR3_MTF_ADDR], FROM_MTF_SIZE);
        rear3_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear3 mtf exif = %s", rear3_mtf_exif);

    /* rear3 manufacturer info */
        memcpy(rear3_fw_ver, &e_ctrl->cal_data.mapdata[REAR3_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear3_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear3 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear3_fw_ver[0], rear3_fw_ver[1], rear3_fw_ver[2], rear3_fw_ver[3], rear3_fw_ver[4],
            rear3_fw_ver[5], rear3_fw_ver[6], rear3_fw_ver[7], rear3_fw_ver[8], rear3_fw_ver[9],
            rear3_fw_ver[10]);

        snprintf(rear3_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_het1, sw_phone_info_het1, vendor_phone_info_het1, process_phone_info_het1);
        rear3_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear3_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear3_phone_fw_ver[0], rear3_phone_fw_ver[1], rear3_phone_fw_ver[2], rear3_phone_fw_ver[3], rear3_phone_fw_ver[4],
            rear3_phone_fw_ver[5], rear3_phone_fw_ver[6], rear3_phone_fw_ver[7], rear3_phone_fw_ver[8], rear3_phone_fw_ver[9],
            rear3_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear3_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear3_phone_fw_ver[0], rear3_phone_fw_ver[1], rear3_phone_fw_ver[2], rear3_phone_fw_ver[3], rear3_phone_fw_ver[4],
            rear3_phone_fw_ver[5], rear3_phone_fw_ver[6], rear3_phone_fw_ver[7], rear3_phone_fw_ver[8], rear3_phone_fw_ver[9],
            rear3_phone_fw_ver[10]);
#endif

        if (strncmp(rear3_phone_fw_ver, rear3_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear3_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear3_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear3_load_fw_ver, rear3_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear3_load_fw_ver, rear3_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR3_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR3_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR3_MODULE_FW_VERSION + i];
            }
            if(rear3_phone_fw_ver[i] >= 0x80
                || !isalnum(rear3_phone_fw_ver[i]))
                rear3_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear3_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR3_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_HET1;
        strcpy(ideal_ver, rear3_phone_fw_ver);
        if(rear3_fw_ver[9] < 0x80 && isalnum(rear3_fw_ver[9])) {
            ideal_ver[9] = rear3_fw_ver[9];
        }
        if(rear3_fw_ver[10] < 0x80 && isalnum(rear3_fw_ver[10])) {
            ideal_ver[10] = rear3_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module3_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear3 info = %s", module3_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam3_fw_ver, "%s %s\n", rear3_fw_ver, rear3_load_fw_ver); //need check phone version
        sprintf(cam3_fw_full_ver, "%s %s %s\n", rear3_fw_ver, rear3_phone_fw_ver, rear3_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear3 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear3_fw_ver[0], rear3_fw_ver[1], rear3_fw_ver[2], rear3_fw_ver[3], rear3_fw_ver[4],
        rear3_fw_ver[5], rear3_fw_ver[6], rear3_fw_ver[7], rear3_fw_ver[8], rear3_fw_ver[9],
        rear3_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETFRONT_RIGHT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_TOP_R))) {  //HET Sensor
    /* rear4 otp data valid bank check */
        if (!otp_data_s5k931_valid_bank)
        {
            snprintf(rear4_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
            rear4_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
            snprintf(rear4_module_id, FROM_MODULE_ID_SIZE, "FF");
            rear4_module_id[FROM_MODULE_ID_SIZE] = '\0';
            snprintf(module4_info, SYSFS_MODULE_INFO_SIZE, "FF");
            module4_info[SYSFS_MODULE_INFO_SIZE] = '\0';
            CAM_INFO(CAM_EEPROM, "rear4 has invalid OTP bank data");
            return rc;
        }

    /* rear4 sensor id */
        memcpy(rear4_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR4_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear4_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear4 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear4_sensor_id[0], rear4_sensor_id[1], rear4_sensor_id[2], rear4_sensor_id[3],
            rear4_sensor_id[4], rear4_sensor_id[5], rear4_sensor_id[6], rear4_sensor_id[7],
            rear4_sensor_id[8], rear4_sensor_id[9], rear4_sensor_id[10], rear4_sensor_id[11],
            rear4_sensor_id[12], rear4_sensor_id[13], rear4_sensor_id[14], rear4_sensor_id[15]);

	/* rear4 module id */
        memcpy(rear4_module_id, &e_ctrl->cal_data.mapdata[REAR4_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear4_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear4_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear4_module_id[0], rear4_module_id[1], rear4_module_id[2], rear4_module_id[3], rear4_module_id[4],
            rear4_module_id[5], rear4_module_id[6], rear4_module_id[7], rear4_module_id[8], rear4_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear4_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear4_module_id[0], rear4_module_id[1], rear4_module_id[2], rear4_module_id[3], rear4_module_id[4],
            rear4_module_id[5], rear4_module_id[6], rear4_module_id[7], rear4_module_id[8], rear4_module_id[9]);
#endif
    /* rear4 mtf exif */
        memcpy(rear4_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR4_MTF_ADDR], FROM_MTF_SIZE);
        rear4_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear4 mtf exif = %s", rear4_mtf_exif);

    /* rear4 manufacturer info */
        memcpy(rear4_fw_ver, &e_ctrl->cal_data.mapdata[REAR4_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear4_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear4 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear4_fw_ver[0], rear4_fw_ver[1], rear4_fw_ver[2], rear4_fw_ver[3], rear4_fw_ver[4],
            rear4_fw_ver[5], rear4_fw_ver[6], rear4_fw_ver[7], rear4_fw_ver[8], rear4_fw_ver[9],
            rear4_fw_ver[10]);

        snprintf(rear4_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_het2, sw_phone_info_het2, vendor_phone_info_het2, process_phone_info_het2);
        rear4_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear4_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear4_phone_fw_ver[0], rear4_phone_fw_ver[1], rear4_phone_fw_ver[2], rear4_phone_fw_ver[3], rear4_phone_fw_ver[4],
            rear4_phone_fw_ver[5], rear4_phone_fw_ver[6], rear4_phone_fw_ver[7], rear4_phone_fw_ver[8], rear4_phone_fw_ver[9],
            rear4_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear4_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear4_phone_fw_ver[0], rear4_phone_fw_ver[1], rear4_phone_fw_ver[2], rear4_phone_fw_ver[3], rear4_phone_fw_ver[4],
            rear4_phone_fw_ver[5], rear4_phone_fw_ver[6], rear4_phone_fw_ver[7], rear4_phone_fw_ver[8], rear4_phone_fw_ver[9],
            rear4_phone_fw_ver[10]);
#endif

        if (strncmp(rear4_phone_fw_ver, rear4_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear4_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear4_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear4_load_fw_ver, rear4_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear4_load_fw_ver, rear4_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR4_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR4_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR4_MODULE_FW_VERSION + i];
            }
            if(rear4_phone_fw_ver[i] >= 0x80
                || !isalnum(rear4_phone_fw_ver[i]))
                rear4_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear4_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR4_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_HET2;
        strcpy(ideal_ver, rear4_phone_fw_ver);
        if(rear4_fw_ver[9] < 0x80 && isalnum(rear4_fw_ver[9])) {
            ideal_ver[9] = rear4_fw_ver[9];
        }
        if(rear4_fw_ver[10] < 0x80 && isalnum(rear4_fw_ver[10])) {
            ideal_ver[10] = rear4_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module4_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear4 info = %s", module4_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam4_fw_ver, "%s %s\n", rear4_fw_ver, rear4_load_fw_ver); //need check phone version
        sprintf(cam4_fw_full_ver, "%s %s %s\n", rear4_fw_ver, rear4_phone_fw_ver, rear4_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear4 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear4_fw_ver[0], rear4_fw_ver[1], rear4_fw_ver[2], rear4_fw_ver[3], rear4_fw_ver[4],
        rear4_fw_ver[5], rear4_fw_ver[6], rear4_fw_ver[7], rear4_fw_ver[8], rear4_fw_ver[9],
        rear4_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETDOWN_LEFT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_MID_L))) {  //HET Sensor
    /* rear5 otp data valid bank check */
        if (!otp_data_s5k931_valid_bank)
        {
            snprintf(rear5_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
            rear5_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
            snprintf(rear5_module_id, FROM_MODULE_ID_SIZE, "FF");
            rear5_module_id[FROM_MODULE_ID_SIZE] = '\0';
            snprintf(module5_info, SYSFS_MODULE_INFO_SIZE, "FF");
            module5_info[SYSFS_MODULE_INFO_SIZE] = '\0';
            CAM_INFO(CAM_EEPROM, "rear5 has invalid OTP bank data");
            return rc;
        }

    /* rear5 sensor id */
        memcpy(rear5_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR5_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear5_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear5 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear5_sensor_id[0], rear5_sensor_id[1], rear5_sensor_id[2], rear5_sensor_id[3],
            rear5_sensor_id[4], rear5_sensor_id[5], rear5_sensor_id[6], rear5_sensor_id[7],
            rear5_sensor_id[8], rear5_sensor_id[9], rear5_sensor_id[10], rear5_sensor_id[11],
            rear5_sensor_id[12], rear5_sensor_id[13], rear5_sensor_id[14], rear5_sensor_id[15]);

	/* rear5 module id */
        memcpy(rear5_module_id, &e_ctrl->cal_data.mapdata[REAR5_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear5_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear5_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear5_module_id[0], rear5_module_id[1], rear5_module_id[2], rear5_module_id[3], rear5_module_id[4],
            rear5_module_id[5], rear5_module_id[6], rear5_module_id[7], rear5_module_id[8], rear5_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear5_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear5_module_id[0], rear5_module_id[1], rear5_module_id[2], rear5_module_id[3], rear5_module_id[4],
            rear5_module_id[5], rear5_module_id[6], rear5_module_id[7], rear5_module_id[8], rear5_module_id[9]);
#endif
    /* rear5 mtf exif */
        memcpy(rear5_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR5_MTF_ADDR], FROM_MTF_SIZE);
        rear5_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear5 mtf exif = %s", rear5_mtf_exif);

    /* rear5 manufacturer info */
        memcpy(rear5_fw_ver, &e_ctrl->cal_data.mapdata[REAR5_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear5_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear5 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear5_fw_ver[0], rear5_fw_ver[1], rear5_fw_ver[2], rear5_fw_ver[3], rear5_fw_ver[4],
            rear5_fw_ver[5], rear5_fw_ver[6], rear5_fw_ver[7], rear5_fw_ver[8], rear5_fw_ver[9],
            rear5_fw_ver[10]);

        snprintf(rear5_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_het3, sw_phone_info_het3, vendor_phone_info_het3, process_phone_info_het3);
        rear5_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear5 rear5_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear5_phone_fw_ver[0], rear5_phone_fw_ver[1], rear5_phone_fw_ver[2], rear5_phone_fw_ver[3], rear5_phone_fw_ver[4],
            rear5_phone_fw_ver[5], rear5_phone_fw_ver[6], rear5_phone_fw_ver[7], rear5_phone_fw_ver[8], rear5_phone_fw_ver[9],
            rear5_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear5 rear5_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear5_phone_fw_ver[0], rear5_phone_fw_ver[1], rear5_phone_fw_ver[2], rear5_phone_fw_ver[3], rear5_phone_fw_ver[4],
            rear5_phone_fw_ver[5], rear5_phone_fw_ver[6], rear5_phone_fw_ver[7], rear5_phone_fw_ver[8], rear5_phone_fw_ver[9],
            rear5_phone_fw_ver[10]);
#endif

        if (strncmp(rear5_phone_fw_ver, rear5_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear5_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear5_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear5_load_fw_ver, rear5_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear5_load_fw_ver, rear5_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR5_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR5_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR5_MODULE_FW_VERSION + i];
            }
            if(rear5_phone_fw_ver[i] >= 0x80
                || !isalnum(rear5_phone_fw_ver[i]))
                rear5_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear5_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR5_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_HET3;
        strcpy(ideal_ver, rear5_phone_fw_ver);
        if(rear5_fw_ver[9] < 0x80 && isalnum(rear5_fw_ver[9])) {
            ideal_ver[9] = rear5_fw_ver[9];
        }
        if(rear5_fw_ver[10] < 0x80 && isalnum(rear5_fw_ver[10])) {
            ideal_ver[10] = rear5_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module5_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear5 info = %s", module5_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam5_fw_ver, "%s %s\n", rear5_fw_ver, rear5_load_fw_ver); //need check phone version
        sprintf(cam5_fw_full_ver, "%s %s %s\n", rear5_fw_ver, rear5_phone_fw_ver, rear5_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear5 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear5_fw_ver[0], rear5_fw_ver[1], rear5_fw_ver[2], rear5_fw_ver[3], rear5_fw_ver[4],
        rear5_fw_ver[5], rear5_fw_ver[6], rear5_fw_ver[7], rear5_fw_ver[8], rear5_fw_ver[9],
        rear5_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETDOWN_RIGHT) || (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_MID_R))) {  //HET Sensor
    /* rear6 otp data valid bank check */
        if (!otp_data_s5k931_valid_bank)
        {
            snprintf(rear6_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
            rear6_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
            snprintf(rear6_module_id, FROM_MODULE_ID_SIZE, "FF");
            rear6_module_id[FROM_MODULE_ID_SIZE] = '\0';
            snprintf(module6_info, SYSFS_MODULE_INFO_SIZE, "FF");
            module6_info[SYSFS_MODULE_INFO_SIZE] = '\0';
            CAM_INFO(CAM_EEPROM, "rear6 has invalid OTP bank data");
            return rc;
        }

    /* rear6 sensor id */
        memcpy(rear6_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR6_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear6_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear6 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear6_sensor_id[0], rear6_sensor_id[1], rear6_sensor_id[2], rear6_sensor_id[3],
            rear6_sensor_id[4], rear6_sensor_id[5], rear6_sensor_id[6], rear6_sensor_id[7],
            rear6_sensor_id[8], rear6_sensor_id[9], rear6_sensor_id[10], rear6_sensor_id[11],
            rear6_sensor_id[12], rear6_sensor_id[13], rear6_sensor_id[14], rear6_sensor_id[15]);

	/* rear6 module id */
        memcpy(rear6_module_id, &e_ctrl->cal_data.mapdata[REAR6_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear6_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear6_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear6_module_id[0], rear6_module_id[1], rear6_module_id[2], rear6_module_id[3], rear6_module_id[4],
            rear6_module_id[5], rear6_module_id[6], rear6_module_id[7], rear6_module_id[8], rear6_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear6_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear6_module_id[0], rear6_module_id[1], rear6_module_id[2], rear6_module_id[3], rear6_module_id[4],
            rear6_module_id[5], rear6_module_id[6], rear6_module_id[7], rear6_module_id[8], rear6_module_id[9]);
#endif
    /* rear6 mtf exif */
        memcpy(rear6_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR6_MTF_ADDR], FROM_MTF_SIZE);
        rear6_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear6 mtf exif = %s", rear6_mtf_exif);

    /* front manufacturer info */
        memcpy(rear6_fw_ver, &e_ctrl->cal_data.mapdata[REAR6_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear6_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear6 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear6_fw_ver[0], rear6_fw_ver[1], rear6_fw_ver[2], rear6_fw_ver[3], rear6_fw_ver[4],
            rear6_fw_ver[5], rear6_fw_ver[6], rear6_fw_ver[7], rear6_fw_ver[8], rear6_fw_ver[9],
            rear6_fw_ver[10]);

        snprintf(rear6_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_het4, sw_phone_info_het4, vendor_phone_info_het4, process_phone_info_het4);
        rear6_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear6_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear6_phone_fw_ver[0], rear6_phone_fw_ver[1], rear6_phone_fw_ver[2], rear6_phone_fw_ver[3], rear6_phone_fw_ver[4],
            rear6_phone_fw_ver[5], rear6_phone_fw_ver[6], rear6_phone_fw_ver[7], rear6_phone_fw_ver[8], rear6_phone_fw_ver[9],
            rear6_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear6_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear6_phone_fw_ver[0], rear6_phone_fw_ver[1], rear6_phone_fw_ver[2], rear6_phone_fw_ver[3], rear6_phone_fw_ver[4],
            rear6_phone_fw_ver[5], rear6_phone_fw_ver[6], rear6_phone_fw_ver[7], rear6_phone_fw_ver[8], rear6_phone_fw_ver[9],
            rear6_phone_fw_ver[10]);
#endif

        if (strncmp(rear6_phone_fw_ver, rear6_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear6_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear6_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear6_load_fw_ver, rear6_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear6_load_fw_ver, rear6_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR6_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR6_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR6_MODULE_FW_VERSION + i];
            }
            if(rear6_phone_fw_ver[i] >= 0x80
                || !isalnum(rear6_phone_fw_ver[i]))
                rear6_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear6_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR6_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_HET4;
        strcpy(ideal_ver, rear6_phone_fw_ver);
        if(rear6_fw_ver[9] < 0x80 && isalnum(rear6_fw_ver[9])) {
            ideal_ver[9] = rear6_fw_ver[9];
        }
        if(rear6_fw_ver[10] < 0x80 && isalnum(rear6_fw_ver[10])) {
            ideal_ver[10] = rear6_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module6_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear6 info = %s", module6_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam6_fw_ver, "%s %s\n", rear6_fw_ver, rear6_load_fw_ver); //need check phone version
        sprintf(cam6_fw_full_ver, "%s %s %s\n", rear6_fw_ver, rear6_phone_fw_ver, rear6_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear6 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear6_fw_ver[0], rear6_fw_ver[1], rear6_fw_ver[2], rear6_fw_ver[3], rear6_fw_ver[4],
        rear6_fw_ver[5], rear6_fw_ver[6], rear6_fw_ver[7], rear6_fw_ver[8], rear6_fw_ver[9],
        rear6_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_BTM_L) {  //HET Sensor
    /* rear7 otp data valid bank check */
        if (!otp_data_s5k931_valid_bank)
        {
            snprintf(rear7_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
            rear7_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
            snprintf(rear7_module_id, FROM_MODULE_ID_SIZE, "FF");
            rear7_module_id[FROM_MODULE_ID_SIZE] = '\0';
            snprintf(module7_info, SYSFS_MODULE_INFO_SIZE, "FF");
            module7_info[SYSFS_MODULE_INFO_SIZE] = '\0';
            CAM_INFO(CAM_EEPROM, "rear7 has invalid OTP bank data");
            return rc;
        }

    /* rear7 sensor id */
        memcpy(rear7_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR7_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear7_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear7 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear7_sensor_id[0], rear7_sensor_id[1], rear7_sensor_id[2], rear7_sensor_id[3],
            rear7_sensor_id[4], rear7_sensor_id[5], rear7_sensor_id[6], rear7_sensor_id[7],
            rear7_sensor_id[8], rear7_sensor_id[9], rear7_sensor_id[10], rear7_sensor_id[11],
            rear7_sensor_id[12], rear7_sensor_id[13], rear7_sensor_id[14], rear7_sensor_id[15]);

	/* rear7 module id */
        memcpy(rear7_module_id, &e_ctrl->cal_data.mapdata[REAR7_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear7_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear7_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear7_module_id[0], rear7_module_id[1], rear7_module_id[2], rear7_module_id[3], rear7_module_id[4],
            rear7_module_id[5], rear7_module_id[6], rear7_module_id[7], rear7_module_id[8], rear7_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear7_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear7_module_id[0], rear7_module_id[1], rear7_module_id[2], rear7_module_id[3], rear7_module_id[4],
            rear7_module_id[5], rear7_module_id[6], rear7_module_id[7], rear7_module_id[8], rear7_module_id[9]);
#endif
    /* rear7 mtf exif */
        memcpy(rear7_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR7_MTF_ADDR], FROM_MTF_SIZE);
        rear7_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear7 mtf exif = %s", rear7_mtf_exif);

    /* front manufacturer info */
        memcpy(rear7_fw_ver, &e_ctrl->cal_data.mapdata[REAR7_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear7_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear7 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear7_fw_ver[0], rear7_fw_ver[1], rear7_fw_ver[2], rear7_fw_ver[3], rear7_fw_ver[4],
            rear7_fw_ver[5], rear7_fw_ver[6], rear7_fw_ver[7], rear7_fw_ver[8], rear7_fw_ver[9],
            rear7_fw_ver[10]);

        snprintf(rear7_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_het5, sw_phone_info_het5, vendor_phone_info_het5, process_phone_info_het5);
        rear7_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear7_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear7_phone_fw_ver[0], rear7_phone_fw_ver[1], rear7_phone_fw_ver[2], rear7_phone_fw_ver[3], rear7_phone_fw_ver[4],
            rear7_phone_fw_ver[5], rear7_phone_fw_ver[6], rear7_phone_fw_ver[7], rear7_phone_fw_ver[8], rear7_phone_fw_ver[9],
            rear7_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear7_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear7_phone_fw_ver[0], rear7_phone_fw_ver[1], rear7_phone_fw_ver[2], rear7_phone_fw_ver[3], rear7_phone_fw_ver[4],
            rear7_phone_fw_ver[5], rear7_phone_fw_ver[6], rear7_phone_fw_ver[7], rear7_phone_fw_ver[8], rear7_phone_fw_ver[9],
            rear7_phone_fw_ver[10]);
#endif

        if (strncmp(rear7_phone_fw_ver, rear7_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear7_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear7_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear7_load_fw_ver, rear7_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear7_load_fw_ver, rear7_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR7_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR7_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR7_MODULE_FW_VERSION + i];
            }
            if(rear7_phone_fw_ver[i] >= 0x80
                || !isalnum(rear7_phone_fw_ver[i]))
                rear7_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear7_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR7_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_HET5;
        strcpy(ideal_ver, rear7_phone_fw_ver);
        if(rear7_fw_ver[9] < 0x80 && isalnum(rear7_fw_ver[9])) {
            ideal_ver[9] = rear7_fw_ver[9];
        }
        if(rear7_fw_ver[10] < 0x80 && isalnum(rear7_fw_ver[10])) {
            ideal_ver[10] = rear7_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module7_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear7 info = %s", module6_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam7_fw_ver, "%s %s\n", rear7_fw_ver, rear7_load_fw_ver); //need check phone version
        sprintf(cam7_fw_full_ver, "%s %s %s\n", rear7_fw_ver, rear7_phone_fw_ver, rear7_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear7 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear7_fw_ver[0], rear7_fw_ver[1], rear7_fw_ver[2], rear7_fw_ver[3], rear7_fw_ver[4],
        rear7_fw_ver[5], rear7_fw_ver[6], rear7_fw_ver[7], rear7_fw_ver[8], rear7_fw_ver[9],
        rear7_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_BTM_R) {  //HET Sensor
    /* rear8 otp data valid bank check */
        if (!otp_data_s5k931_valid_bank)
        {
            snprintf(rear8_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
            rear8_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
            snprintf(rear8_module_id, FROM_MODULE_ID_SIZE, "FF");
            rear8_module_id[FROM_MODULE_ID_SIZE] = '\0';
            snprintf(module8_info, SYSFS_MODULE_INFO_SIZE, "FF");
            module8_info[SYSFS_MODULE_INFO_SIZE] = '\0';
            CAM_INFO(CAM_EEPROM, "rear8 has invalid OTP bank data");
            return rc;
        }

    /* rear8 sensor id */
        memcpy(rear8_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR8_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear8_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear8 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear8_sensor_id[0], rear8_sensor_id[1], rear8_sensor_id[2], rear8_sensor_id[3],
            rear8_sensor_id[4], rear8_sensor_id[5], rear8_sensor_id[6], rear8_sensor_id[7],
            rear8_sensor_id[8], rear8_sensor_id[9], rear8_sensor_id[10], rear8_sensor_id[11],
            rear8_sensor_id[12], rear8_sensor_id[13], rear8_sensor_id[14], rear8_sensor_id[15]);

	/* rear8 module id */
        memcpy(rear8_module_id, &e_ctrl->cal_data.mapdata[REAR8_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear8_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear8_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear8_module_id[0], rear8_module_id[1], rear8_module_id[2], rear8_module_id[3], rear8_module_id[4],
            rear8_module_id[5], rear8_module_id[6], rear8_module_id[7], rear8_module_id[8], rear8_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear8_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear8_module_id[0], rear8_module_id[1], rear8_module_id[2], rear8_module_id[3], rear8_module_id[4],
            rear8_module_id[5], rear8_module_id[6], rear8_module_id[7], rear8_module_id[8], rear8_module_id[9]);
#endif
    /* rear8 mtf exif */
        memcpy(rear8_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR8_MTF_ADDR], FROM_MTF_SIZE);
        rear8_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear8 mtf exif = %s", rear8_mtf_exif);

    /* front manufacturer info */
        memcpy(rear8_fw_ver, &e_ctrl->cal_data.mapdata[REAR8_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear8_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear8 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear8_fw_ver[0], rear8_fw_ver[1], rear8_fw_ver[2], rear8_fw_ver[3], rear8_fw_ver[4],
            rear8_fw_ver[5], rear8_fw_ver[6], rear8_fw_ver[7], rear8_fw_ver[8], rear8_fw_ver[9],
            rear8_fw_ver[10]);

        snprintf(rear8_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_het6, sw_phone_info_het6, vendor_phone_info_het6, process_phone_info_het6);
        rear8_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear8_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear8_phone_fw_ver[0], rear8_phone_fw_ver[1], rear8_phone_fw_ver[2], rear8_phone_fw_ver[3], rear8_phone_fw_ver[4],
            rear8_phone_fw_ver[5], rear8_phone_fw_ver[6], rear8_phone_fw_ver[7], rear8_phone_fw_ver[8], rear8_phone_fw_ver[9],
            rear8_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear8_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear8_phone_fw_ver[0], rear8_phone_fw_ver[1], rear8_phone_fw_ver[2], rear8_phone_fw_ver[3], rear8_phone_fw_ver[4],
            rear8_phone_fw_ver[5], rear8_phone_fw_ver[6], rear8_phone_fw_ver[7], rear8_phone_fw_ver[8], rear8_phone_fw_ver[9],
            rear8_phone_fw_ver[10]);
#endif

        if (strncmp(rear8_phone_fw_ver, rear8_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear8_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear8_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear8_load_fw_ver, rear8_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear8_load_fw_ver, rear8_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR8_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR8_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR8_MODULE_FW_VERSION + i];
            }
            if(rear8_phone_fw_ver[i] >= 0x80
                || !isalnum(rear8_phone_fw_ver[i]))
                rear8_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear8_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR8_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_HET6;
        strcpy(ideal_ver, rear8_phone_fw_ver);
        if(rear8_fw_ver[9] < 0x80 && isalnum(rear8_fw_ver[9])) {
            ideal_ver[9] = rear8_fw_ver[9];
        }
        if(rear8_fw_ver[10] < 0x80 && isalnum(rear8_fw_ver[10])) {
            ideal_ver[10] = rear8_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(module8_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear8 info = %s", module6_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam8_fw_ver, "%s %s\n", rear8_fw_ver, rear8_load_fw_ver); //need check phone version
        sprintf(cam8_fw_full_ver, "%s %s %s\n", rear8_fw_ver, rear8_phone_fw_ver, rear8_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear8 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear8_fw_ver[0], rear8_fw_ver[1], rear8_fw_ver[2], rear8_fw_ver[3], rear8_fw_ver[4],
        rear8_fw_ver[5], rear8_fw_ver[6], rear8_fw_ver[7], rear8_fw_ver[8], rear8_fw_ver[9],
        rear8_fw_ver[10]);
#endif
    }
#if defined(CONFIG_SAMSUNG_REAR_TOF)
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K63D && e_ctrl->soc_info.index == CAM_EEPROM_IDX_BACK_TOF) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K63D && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ITOF)) {  //TOF Sensor
    /* rear tof dual cal */
        memcpy(rear_dtof_dual_cal, &e_ctrl->cal_data.mapdata[TOF_DUAL_CAL_ADDR], REAR_TOF_DUAL_CAL_SIZE);
        rear_dtof_dual_cal[REAR_TOF_DUAL_CAL_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_tof dual cal = %s", rear_dtof_dual_cal);

        memcpy(rear_dtof_cal, &e_ctrl->cal_data.mapdata[TOFCAL_START_ADDR], TOFCAL_SIZE);
        rear_dtof_cal[TOFCAL_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_tof cal = %s", rear_dtof_cal);

        memcpy(rear_dtof_cal_extra, &e_ctrl->cal_data.mapdata[TOFCAL_START_ADDR+TOFCAL_SIZE], TOFCAL_EXTRA_SIZE);
        rear_dtof_cal_extra[TOFCAL_EXTRA_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_tof cal extra = %s", rear_dtof_cal_extra);

    /* rear tof cal result */
        memcpy(&result1, &e_ctrl->cal_data.mapdata[TOFCAL_RESULT_ADDR], 1);
        memcpy(&result2, &e_ctrl->cal_data.mapdata[TOFCAL_RESULT_ADDR + 2], 1);
        memcpy(&result3, &e_ctrl->cal_data.mapdata[TOFCAL_RESULT_ADDR + 4], 1);
        memcpy(&result4, &e_ctrl->cal_data.mapdata[TOFCAL_RESULT_ADDR + 6], 1);
        CAM_DBG(CAM_EEPROM, "rear_tof cal result1 = %d, result2 = %d, result3 = %d, result4 = %d", result1, result2, result3, result4);

        if (result1 == 0x11 && result2 == 0x11 && result3 == 0x11 && result4 == 0x11)
        {
            rear_dtof_cal_result = 1;
        }

    /* rear tof validation_500 */
        memcpy(&rear_dtof_validation_500, &e_ctrl->cal_data.mapdata[REAR_TOF_VALIDATION_500], 2);
        CAM_DBG(CAM_EEPROM, "rear_tof 500mm validation data = 0x%04X", rear_dtof_validation_500);

    /* rear tof validation_300 */
        memcpy(&rear_dtof_validation_300, &e_ctrl->cal_data.mapdata[REAR_TOF_VALIDATION_300], 2);
        CAM_DBG(CAM_EEPROM, "rear_tof 300mm validation data = 0x%04X", rear_dtof_validation_300);

    /* rear_tof sensor id */
        memcpy(rear_dtof_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR_TOF_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear_dtof_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear_tof sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_dtof_sensor_id[0], rear_dtof_sensor_id[1], rear_dtof_sensor_id[2], rear_dtof_sensor_id[3],
            rear_dtof_sensor_id[4], rear_dtof_sensor_id[5], rear_dtof_sensor_id[6], rear_dtof_sensor_id[7],
            rear_dtof_sensor_id[8], rear_dtof_sensor_id[9], rear_dtof_sensor_id[10], rear_dtof_sensor_id[11],
            rear_dtof_sensor_id[12], rear_dtof_sensor_id[13], rear_dtof_sensor_id[14], rear_dtof_sensor_id[15]);

	/* rear_tof module id */
        memcpy(rear_dtof_module_id, &e_ctrl->cal_data.mapdata[REAR_TOF_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear_dtof_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_tof_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear_dtof_module_id[0], rear_dtof_module_id[1], rear_dtof_module_id[2], rear_dtof_module_id[3], rear_dtof_module_id[4],
            rear_dtof_module_id[5], rear_dtof_module_id[6], rear_dtof_module_id[7], rear_dtof_module_id[8], rear_dtof_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_tof_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear_dtof_module_id[0], rear_dtof_module_id[1], rear_dtof_module_id[2], rear_dtof_module_id[3], rear_dtof_module_id[4],
            rear_dtof_module_id[5], rear_dtof_module_id[6], rear_dtof_module_id[7], rear_dtof_module_id[8], rear_dtof_module_id[9]);
#endif
    /* rear_tof mtf exif */
        //memcpy(rear_dtof_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR_TOF_MTF_ADDR], FROM_MTF_SIZE);
        rear_dtof_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_tof mtf exif = %s", rear_dtof_mtf_exif);

    /* tof manufacturer info */
        memcpy(rear_dtof_fw_ver, &e_ctrl->cal_data.mapdata[REAR_TOF_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear_dtof_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear_tof manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_dtof_fw_ver[0], rear_dtof_fw_ver[1], rear_dtof_fw_ver[2], rear_dtof_fw_ver[3], rear_dtof_fw_ver[4],
            rear_dtof_fw_ver[5], rear_dtof_fw_ver[6], rear_dtof_fw_ver[7], rear_dtof_fw_ver[8], rear_dtof_fw_ver[9],
            rear_dtof_fw_ver[10]);

        snprintf(rear_dtof_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_tof, sw_phone_info_tof, vendor_phone_info_tof, process_phone_info_tof);
        rear_dtof_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear_tof_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_dtof_phone_fw_ver[0], rear_dtof_phone_fw_ver[1], rear_dtof_phone_fw_ver[2], rear_dtof_phone_fw_ver[3], rear_dtof_phone_fw_ver[4],
            rear_dtof_phone_fw_ver[5], rear_dtof_phone_fw_ver[6], rear_dtof_phone_fw_ver[7], rear_dtof_phone_fw_ver[8], rear_dtof_phone_fw_ver[9],
            rear_dtof_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear_tof_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear_dtof_phone_fw_ver[0], rear_dtof_phone_fw_ver[1], rear_dtof_phone_fw_ver[2], rear_dtof_phone_fw_ver[3], rear_dtof_phone_fw_ver[4],
            rear_dtof_phone_fw_ver[5], rear_dtof_phone_fw_ver[6], rear_dtof_phone_fw_ver[7], rear_dtof_phone_fw_ver[8], rear_dtof_phone_fw_ver[9],
            rear_dtof_phone_fw_ver[10]);
#endif

        if (strncmp(rear_dtof_phone_fw_ver, rear_dtof_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear_dtof_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear_dtof_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear_dtof_load_fw_ver, rear_dtof_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear_dtof_load_fw_ver, rear_dtof_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR_TOF_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR_TOF_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR_TOF_MODULE_FW_VERSION + i];
            }
            if(rear_dtof_phone_fw_ver[i] >= 0x80
                || !isalnum(rear_dtof_phone_fw_ver[i]))
                rear_dtof_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear_dtof_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR_TOF_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_TOF;
        strcpy(ideal_ver, rear_dtof_phone_fw_ver);
        if(rear_dtof_fw_ver[9] < 0x80 && isalnum(rear_dtof_fw_ver[9])) {
            ideal_ver[9] = rear_dtof_fw_ver[9];
        }
        if(rear_dtof_fw_ver[10] < 0x80 && isalnum(rear_dtof_fw_ver[10])) {
            ideal_ver[10] = rear_dtof_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(rear_dtof_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_tof info = %s", rear_dtof_module_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam_dtof_fw_ver, "%s %s\n", rear_dtof_fw_ver, rear_dtof_load_fw_ver); //need check phone version
        sprintf(cam_dtof_fw_full_ver, "%s %s %s\n", rear_dtof_fw_ver, rear_dtof_phone_fw_ver, rear_dtof_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_tof manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear_dtof_fw_ver[0], rear_dtof_fw_ver[1], rear_dtof_fw_ver[2], rear_dtof_fw_ver[3], rear_dtof_fw_ver[4],
        rear_dtof_fw_ver[5], rear_dtof_fw_ver[6], rear_dtof_fw_ver[7], rear_dtof_fw_ver[8], rear_dtof_fw_ver[9],
        rear_dtof_fw_ver[10]);
#endif
    }
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX611 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_DTOF) {  //DTOF Sensor
    /* rear dtof dual cal */
        memcpy(rear_tof_dual_cal, &e_ctrl->cal_data.mapdata[DTOF_DUAL_CAL_ADDR], REAR_DTOF_DUAL_CAL_SIZE);
        rear_tof_dual_cal[REAR_DTOF_DUAL_CAL_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_dtof dual cal = %s", rear_dtof_dual_cal);

        memcpy(rear_tof_cal, &e_ctrl->cal_data.mapdata[DTOFCAL_START_ADDR], DTOFCAL_SIZE);
        rear_tof_cal[DTOFCAL_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_dtof cal = %s", rear_dtof_cal);

        memcpy(rear_tof_cal_extra, &e_ctrl->cal_data.mapdata[DTOFCAL_START_ADDR+DTOFCAL_SIZE], DTOFCAL_EXTRA_SIZE);
        rear_tof_cal_extra[DTOFCAL_EXTRA_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_dtof cal extra = %s", rear_dtof_cal_extra);

    /* rear dtof cal result */
        memcpy(&d_result1, &e_ctrl->cal_data.mapdata[DTOFCAL_RESULT_ADDR], 1);
        CAM_DBG(CAM_EEPROM, "rear_dtof cal d_result1 = %d", d_result1);

        if (d_result1 == 0x01)
        {
            rear_tof_cal_result = 1;
        }

    /* rear dtof validation_500 */
        memcpy(&rear_tof_validation_500, &e_ctrl->cal_data.mapdata[REAR_DTOF_VALIDATION_500], 2);
        CAM_DBG(CAM_EEPROM, "rear_dtof 500mm validation data = 0x%04X", rear_dtof_validation_500);

    /* rear dtof validation_300 */
        memcpy(&rear_tof_validation_300, &e_ctrl->cal_data.mapdata[REAR_DTOF_VALIDATION_300], 2);
        CAM_DBG(CAM_EEPROM, "rear_dtof 300mm validation data = 0x%04X", rear_dtof_validation_300);

    /* rear_dtof sensor id */
        memcpy(rear_tof_sensor_id, &e_ctrl->cal_data.mapdata[FROM_REAR_DTOF_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
        rear_tof_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear_dtof sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_tof_sensor_id[0], rear_tof_sensor_id[1], rear_tof_sensor_id[2], rear_tof_sensor_id[3],
            rear_tof_sensor_id[4], rear_tof_sensor_id[5], rear_tof_sensor_id[6], rear_tof_sensor_id[7],
            rear_tof_sensor_id[8], rear_tof_sensor_id[9], rear_tof_sensor_id[10], rear_tof_sensor_id[11],
            rear_tof_sensor_id[12], rear_tof_sensor_id[13], rear_tof_sensor_id[14], rear_tof_sensor_id[15]);

	/* rear_dtof module id */
        memcpy(rear_tof_module_id, &e_ctrl->cal_data.mapdata[REAR_DTOF_MODULE_ID_ADDR], FROM_MODULE_ID_SIZE);
        rear_tof_module_id[FROM_MODULE_ID_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_dtof_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            rear_tof_module_id[0], rear_tof_module_id[1], rear_tof_module_id[2], rear_tof_module_id[3], rear_tof_module_id[4],
            rear_tof_module_id[5], rear_tof_module_id[6], rear_tof_module_id[7], rear_tof_module_id[8], rear_tof_module_id[9]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_dtof_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
            rear_tof_module_id[0], rear_tof_module_id[1], rear_tof_module_id[2], rear_tof_module_id[3], rear_tof_module_id[4],
            rear_tof_module_id[5], rear_tof_module_id[6], rear_tof_module_id[7], rear_tof_module_id[8], rear_tof_module_id[9]);
#endif
    /* rear_dtof mtf exif */
        //memcpy(rear_tof_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_REAR_DTOF_MTF_ADDR], FROM_MTF_SIZE);
        rear_tof_mtf_exif[FROM_MTF_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM, "rear_dtof mtf exif = %s", rear_tof_mtf_exif);

    /* dtof manufacturer info */
        memcpy(rear_tof_fw_ver, &e_ctrl->cal_data.mapdata[REAR_DTOF_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
        rear_tof_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear_dtof manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_tof_fw_ver[0], rear_tof_fw_ver[1], rear_tof_fw_ver[2], rear_tof_fw_ver[3], rear_tof_fw_ver[4],
            rear_tof_fw_ver[5], rear_tof_fw_ver[6], rear_tof_fw_ver[7], rear_tof_fw_ver[8], rear_tof_fw_ver[9],
            rear_tof_fw_ver[10]);

        snprintf(rear_tof_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", hw_phone_info_dtof, sw_phone_info_dtof, vendor_phone_info_dtof, process_phone_info_dtof);
        rear_tof_phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
        CAM_DBG(CAM_EEPROM,
            "rear rear_dtof_phone_fw_ver info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rear_tof_phone_fw_ver[0], rear_tof_phone_fw_ver[1], rear_tof_phone_fw_ver[2], rear_tof_phone_fw_ver[3], rear_tof_phone_fw_ver[4],
            rear_tof_phone_fw_ver[5], rear_tof_phone_fw_ver[6], rear_tof_phone_fw_ver[7], rear_tof_phone_fw_ver[8], rear_tof_phone_fw_ver[9],
            rear_tof_phone_fw_ver[10]);

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear rear_dtof_phone_fw_ver info = %c %c %c %c %c %c %c %c %c %c %c",
            rear_tof_phone_fw_ver[0], rear_tof_phone_fw_ver[1], rear_tof_phone_fw_ver[2], rear_tof_phone_fw_ver[3], rear_tof_phone_fw_ver[4],
            rear_tof_phone_fw_ver[5], rear_tof_phone_fw_ver[6], rear_tof_phone_fw_ver[7], rear_tof_phone_fw_ver[8], rear_tof_phone_fw_ver[9],
            rear_tof_phone_fw_ver[10]);
#endif

        if (strncmp(rear_tof_phone_fw_ver, rear_tof_fw_ver, HW_INFO_MAX_SIZE-1) == 0
            && strncmp(&rear_tof_phone_fw_ver[HW_INFO_MAX_SIZE-1], &rear_tof_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0) {
            CAM_DBG(CAM_EEPROM, "Load from phone");
            strcpy(rear_tof_load_fw_ver, rear_tof_phone_fw_ver);
            loadfrom = 'P';
        } else {
            CAM_DBG(CAM_EEPROM, "Load from EEPROM");
            strcpy(rear_tof_load_fw_ver, rear_tof_fw_ver);
            loadfrom = 'E';
        }

        bVerNull = FALSE;
        for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
            if(e_ctrl->cal_data.mapdata[REAR_DTOF_MODULE_FW_VERSION + i] >= 0x80
            || !isalnum(e_ctrl->cal_data.mapdata[REAR_DTOF_MODULE_FW_VERSION + i])) {
            cal_ver[i] = ' ';
            bVerNull = TRUE;
            } else {
                cal_ver[i] = e_ctrl->cal_data.mapdata[REAR_DTOF_MODULE_FW_VERSION + i];
            }
            if(rear_tof_phone_fw_ver[i] >= 0x80
                || !isalnum(rear_tof_phone_fw_ver[i]))
                rear_tof_phone_fw_ver[i] = ' ';
        }

        sensor_ver[0] = rear_tof_sensor_id[8];
        dll_ver[0] = e_ctrl->cal_data.mapdata[REAR_DTOF_DLL_VERSION_ADDR] - '0';
        sensor_ver[1] = 0;
        dll_ver[1] = 0;
        normal_cri_rev = CRITERION_REV_DTOF;
        strcpy(ideal_ver, rear_tof_phone_fw_ver);
        if(rear_tof_fw_ver[9] < 0x80 && isalnum(rear_tof_fw_ver[9])) {
            ideal_ver[9] = rear_tof_fw_ver[9];
        }
        if(rear_tof_fw_ver[10] < 0x80 && isalnum(rear_tof_fw_ver[10])) {
            ideal_ver[10] = rear_tof_fw_ver[10];
        }
        if(board_rev < normal_cri_rev && bVerNull == TRUE)
        {
            strcpy(cal_ver, ideal_ver);
            loadfrom = 'P';
            CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
        }

        snprintf(rear_tof_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
            loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
            board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
            ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_dtof info = %s", rear_tof_module_info);
#endif

    /* update EEPROM fw version on sysfs */
        sprintf(cam_tof_fw_ver, "%s %s\n", rear_tof_fw_ver, rear_tof_load_fw_ver); //need check phone version
        sprintf(cam_tof_fw_full_ver, "%s %s %s\n", rear_tof_fw_ver, rear_tof_phone_fw_ver, rear_tof_load_fw_ver);// needed check phone version.

#ifdef CAM_EEPROM_DBG
        CAM_DBG(CAM_EEPROM, "rear_dtof manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
        rear_tof_fw_ver[0], rear_tof_fw_ver[1], rear_tof_fw_ver[2], rear_tof_fw_ver[3], rear_tof_fw_ver[4],
        rear_tof_fw_ver[5], rear_tof_fw_ver[6], rear_tof_fw_ver[7], rear_tof_fw_ver[8], rear_tof_fw_ver[9],
        rear_tof_fw_ver[10]);
#endif
    }
#endif
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LEFT) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LU) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LU_FACTORY)) { //Front ET Cam
		/* front otp data valid bank check */
		if (!otp_data_s5k931_valid_bank)
		{
			snprintf(front_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
			front_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
			snprintf(front_module_id, FROM_FRONT_MODULE_ID_SIZE, "FF");
			front_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
			snprintf(front_module_info, SYSFS_MODULE_INFO_SIZE, "FF");
			front_module_info[SYSFS_MODULE_INFO_SIZE] = '\0';
			CAM_INFO(CAM_EEPROM, "front has invalid OTP bank data");
			return rc;
		}

		/* front sensor id */
		memcpy(front_sensor_id, &e_ctrl->cal_data.mapdata[FROM_FRONT_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
		front_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front_sensor_id[0], front_sensor_id[1], front_sensor_id[2], front_sensor_id[3],
			front_sensor_id[4], front_sensor_id[5], front_sensor_id[6], front_sensor_id[7],
			front_sensor_id[8], front_sensor_id[9], front_sensor_id[10], front_sensor_id[11],
			front_sensor_id[12], front_sensor_id[13], front_sensor_id[14], front_sensor_id[15]);

		/* front module id */
		memcpy(front_module_id, &e_ctrl->cal_data.mapdata[FROM_FRONT_MODULE_ID_ADDR], FROM_FRONT_MODULE_ID_SIZE);
		front_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3],
			front_module_id[4], front_module_id[5], front_module_id[6], front_module_id[7],
			front_module_id[8], front_module_id[9]);

#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
			front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3],
			front_module_id[4], front_module_id[5], front_module_id[6], front_module_id[7],
			front_module_id[8], front_module_id[9]);
#endif
        /* front mtf exif */
		memcpy(front_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_FRONT_MTF_ADDR], FROM_MTF_SIZE);
		front_mtf_exif[FROM_MTF_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front mtf exif = %s", front_mtf_exif);

		/* front manufacturer info */
		memcpy(front_fw_ver, &e_ctrl->cal_data.mapdata[FRONT_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
		front_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front_fw_ver[0], front_fw_ver[1], front_fw_ver[2], front_fw_ver[3], front_fw_ver[4],
			front_fw_ver[5], front_fw_ver[6], front_fw_ver[7], front_fw_ver[8], front_fw_ver[9],
			front_fw_ver[10]);

		/* temp front phone version */
		snprintf(front_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", front_hw_phone_info, front_sw_phone_info, front_vendor_phone_info, front_process_phone_info);

		/* temp front load version */
		CAM_DBG(CAM_EEPROM, "Load from front EEPROM");
		strcpy(front_load_fw_ver, front_fw_ver);
		loadfrom = 'E';

		bVerNull = FALSE;
		for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
			if(e_ctrl->cal_data.mapdata[FRONT_MODULE_FW_VERSION + i] >= 0x80
				|| !isalnum(e_ctrl->cal_data.mapdata[FRONT_MODULE_FW_VERSION + i])) {
				cal_ver[i] = ' ';
				bVerNull = TRUE;
			} else {
				cal_ver[i] = e_ctrl->cal_data.mapdata[FRONT_MODULE_FW_VERSION + i];
			}

			if(front_phone_fw_ver[i] >= 0x80 || !isalnum(front_phone_fw_ver[i]))
				front_phone_fw_ver[i] = ' ';
		}
		sensor_ver[0] = front_sensor_id[8];
		dll_ver[0] = e_ctrl->cal_data.mapdata[FRONT_DLL_VERSION_ADDR] - '0';

		sensor_ver[1] = 0;
		dll_ver[1] = 0;

		normal_cri_rev = CRITERION_REV_FRONT;
		strcpy(ideal_ver, front_phone_fw_ver);
		if(front_phone_fw_ver[9] < 0x80 && isalnum(front_phone_fw_ver[9])) {
			ideal_ver[9] = front_phone_fw_ver[9];
		}
		if(front_phone_fw_ver[10] < 0x80 && isalnum(front_phone_fw_ver[10])) {
			ideal_ver[10] = front_phone_fw_ver[10];
		}

		if(board_rev < normal_cri_rev && bVerNull == TRUE)
		{
			strcpy(cal_ver, ideal_ver);
			CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
			e_ctrl->is_supported = normal_is_supported;
		}

		snprintf(front_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
			loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
			board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
			ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front info = %s", front_module_info);
#endif

		/* update EEPROM fw version on sysfs */
		sprintf(front_cam_fw_ver, "%s %s\n", front_fw_ver, front_load_fw_ver);
		sprintf(front_cam_fw_full_ver, "%s %s %s\n", front_fw_ver, front_phone_fw_ver, front_load_fw_ver);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
			front_fw_ver[0], front_fw_ver[1], front_fw_ver[2], front_fw_ver[3], front_fw_ver[4],
			front_fw_ver[5], front_fw_ver[6], front_fw_ver[7], front_fw_ver[8], front_fw_ver[9],
			front_fw_ver[10]);
#endif
	}
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RIGHT) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RU) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RU_FACTORY)) { //Front ET Cam
		/* front2 otp data valid bank check */
		if (!otp_data_s5k931_valid_bank)
		{
			snprintf(front2_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
			front2_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
			snprintf(front2_module_id, FROM_FRONT_MODULE_ID_SIZE, "FF");
			front2_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
			snprintf(front2_module_info, SYSFS_MODULE_INFO_SIZE, "FF");
			front2_module_info[SYSFS_MODULE_INFO_SIZE] = '\0';
			CAM_INFO(CAM_EEPROM, "front2 has invalid OTP bank data");
			return rc;
		}

		/* front2 sensor id */
		memcpy(front2_sensor_id, &e_ctrl->cal_data.mapdata[FROM_FRONT2_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
		front2_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front2 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front2_sensor_id[0], front2_sensor_id[1], front2_sensor_id[2], front2_sensor_id[3],
			front2_sensor_id[4], front2_sensor_id[5], front2_sensor_id[6], front2_sensor_id[7],
			front2_sensor_id[8], front2_sensor_id[9], front2_sensor_id[10], front2_sensor_id[11],
			front2_sensor_id[12], front2_sensor_id[13], front2_sensor_id[14], front2_sensor_id[15]);

		/* front2 module id */
		memcpy(front2_module_id, &e_ctrl->cal_data.mapdata[FROM_FRONT2_MODULE_ID_ADDR], FROM_FRONT_MODULE_ID_SIZE);
		front2_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front2_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3],
			front2_module_id[4], front2_module_id[5], front2_module_id[6], front2_module_id[7],
			front2_module_id[8], front2_module_id[9]);

#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front2_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
			front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3],
			front2_module_id[4], front2_module_id[5], front2_module_id[6], front2_module_id[7],
			front2_module_id[8], front2_module_id[9]);
#endif
        /* front2 mtf exif */
		memcpy(front2_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_FRONT2_MTF_ADDR], FROM_MTF_SIZE);
		front2_mtf_exif[FROM_MTF_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front2 mtf exif = %s", front2_mtf_exif);

		/* front2 manufacturer info */
		memcpy(front2_fw_ver, &e_ctrl->cal_data.mapdata[FRONT2_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
		front2_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front2 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front2_fw_ver[0], front2_fw_ver[1], front2_fw_ver[2], front2_fw_ver[3], front2_fw_ver[4],
			front2_fw_ver[5], front2_fw_ver[6], front2_fw_ver[7], front2_fw_ver[8], front2_fw_ver[9],
			front2_fw_ver[10]);

		/* temp front2 phone version */
		snprintf(front2_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", front2_hw_phone_info, front2_sw_phone_info, front2_vendor_phone_info, front2_process_phone_info);

		/* temp front2 load version */
		CAM_DBG(CAM_EEPROM, "Load from front2 EEPROM");
		strcpy(front2_load_fw_ver, front2_fw_ver);
		loadfrom = 'E';

		bVerNull = FALSE;
		for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
			if(e_ctrl->cal_data.mapdata[FRONT2_MODULE_FW_VERSION + i] >= 0x80
				|| !isalnum(e_ctrl->cal_data.mapdata[FRONT2_MODULE_FW_VERSION + i])) {
				cal_ver[i] = ' ';
				bVerNull = TRUE;
			} else {
				cal_ver[i] = e_ctrl->cal_data.mapdata[FRONT2_MODULE_FW_VERSION + i];
			}

			if(front2_phone_fw_ver[i] >= 0x80 || !isalnum(front2_phone_fw_ver[i]))
				front2_phone_fw_ver[i] = ' ';
		}
		sensor_ver[0] = front2_sensor_id[8];
		dll_ver[0] = e_ctrl->cal_data.mapdata[FRONT2_DLL_VERSION_ADDR] - '0';

		sensor_ver[1] = 0;
		dll_ver[1] = 0;

		normal_cri_rev = CRITERION_REV_FRONT2;
		strcpy(ideal_ver, front2_phone_fw_ver);
		if(front2_phone_fw_ver[9] < 0x80 && isalnum(front2_phone_fw_ver[9])) {
			ideal_ver[9] = front2_phone_fw_ver[9];
		}
		if(front2_phone_fw_ver[10] < 0x80 && isalnum(front2_phone_fw_ver[10])) {
			ideal_ver[10] = front2_phone_fw_ver[10];
		}

		if(board_rev < normal_cri_rev && bVerNull == TRUE)
		{
			strcpy(cal_ver, ideal_ver);
			CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
			e_ctrl->is_supported = normal_is_supported;
		}

		snprintf(front2_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
			loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
			board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
			ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front2 info = %s", front2_module_info);
#endif

		/* update EEPROM fw version on sysfs */
		sprintf(front2_cam_fw_ver, "%s %s\n", front2_fw_ver, front2_load_fw_ver);
		sprintf(front2_cam_fw_full_ver, "%s %s %s\n", front2_fw_ver, front2_phone_fw_ver, front2_load_fw_ver);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front2 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
			front2_fw_ver[0], front2_fw_ver[1], front2_fw_ver[2], front2_fw_ver[3], front2_fw_ver[4],
			front2_fw_ver[5], front2_fw_ver[6], front2_fw_ver[7], front2_fw_ver[8], front2_fw_ver[9],
			front2_fw_ver[10]);
#endif
	}
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD_FACTORY)) { //Front FT Cam
		/* front3 otp data valid bank check */
		if (!otp_data_s5k931_valid_bank)
		{
			snprintf(front3_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
			front3_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
			snprintf(front3_module_id, FROM_FRONT_MODULE_ID_SIZE, "FF");
			front3_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
			snprintf(front3_module_info, SYSFS_MODULE_INFO_SIZE, "FF");
			front3_module_info[SYSFS_MODULE_INFO_SIZE] = '\0';
			CAM_INFO(CAM_EEPROM, "front3 has invalid OTP bank data");
			return rc;
		}

		/* front3 sensor id */
		memcpy(front3_sensor_id, &e_ctrl->cal_data.mapdata[FROM_FRONT3_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
		front3_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front3 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front3_sensor_id[0], front3_sensor_id[1], front3_sensor_id[2], front3_sensor_id[3],
			front3_sensor_id[4], front3_sensor_id[5], front3_sensor_id[6], front3_sensor_id[7],
			front3_sensor_id[8], front3_sensor_id[9], front3_sensor_id[10], front3_sensor_id[11],
			front3_sensor_id[12], front3_sensor_id[13], front3_sensor_id[14], front3_sensor_id[15]);

		/* front3 module id */
		memcpy(front3_module_id, &e_ctrl->cal_data.mapdata[FROM_FRONT3_MODULE_ID_ADDR], FROM_FRONT_MODULE_ID_SIZE);
		front3_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front3_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3],
			front3_module_id[4], front3_module_id[5], front3_module_id[6], front3_module_id[7],
			front3_module_id[8], front3_module_id[9]);

#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front3_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
			front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3],
			front3_module_id[4], front3_module_id[5], front3_module_id[6], front3_module_id[7],
			front3_module_id[8], front3_module_id[9]);
#endif
        /* front3 mtf exif */
		memcpy(front3_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_FRONT3_MTF_ADDR], FROM_MTF_SIZE);
		front3_mtf_exif[FROM_MTF_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front3 mtf exif = %s", front3_mtf_exif);

		/* front3 manufacturer info */
		memcpy(front3_fw_ver, &e_ctrl->cal_data.mapdata[FRONT3_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
		front3_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front3 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front3_fw_ver[0], front3_fw_ver[1], front3_fw_ver[2], front3_fw_ver[3], front3_fw_ver[4],
			front3_fw_ver[5], front3_fw_ver[6], front3_fw_ver[7], front3_fw_ver[8], front3_fw_ver[9],
			front3_fw_ver[10]);

		/* temp front3 phone version */
		snprintf(front3_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", front3_hw_phone_info, front3_sw_phone_info, front3_vendor_phone_info, front3_process_phone_info);

		/* temp front3 load version */
		CAM_DBG(CAM_EEPROM, "Load from front3 EEPROM");
		strcpy(front3_load_fw_ver, front3_fw_ver);
		loadfrom = 'E';

		bVerNull = FALSE;
		for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
			if (e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD || e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD_FACTORY){
				if(e_ctrl->cal_data.mapdata[FRONT3_MODULE_FW_VERSION + i] >= 0x80
					|| !isalnum(e_ctrl->cal_data.mapdata[FRONT3_MODULE_FW_VERSION + i])) {
					cal_ver[i] = ' ';
					bVerNull = TRUE;
				} else {
					cal_ver[i] = e_ctrl->cal_data.mapdata[FRONT3_MODULE_FW_VERSION + i];
				}
			}

			if(front3_phone_fw_ver[i] >= 0x80 || !isalnum(front3_phone_fw_ver[i]))
				front3_phone_fw_ver[i] = ' ';
		}
		sensor_ver[0] = front3_sensor_id[8];
		dll_ver[0] = e_ctrl->cal_data.mapdata[FRONT3_DLL_VERSION_ADDR] - '0';

		sensor_ver[1] = 0;
		dll_ver[1] = 0;

		normal_cri_rev = CRITERION_REV_FRONT3;
		strcpy(ideal_ver, front3_phone_fw_ver);
		if(front3_phone_fw_ver[9] < 0x80 && isalnum(front3_phone_fw_ver[9])) {
			ideal_ver[9] = front3_phone_fw_ver[9];
		}
		if(front3_phone_fw_ver[10] < 0x80 && isalnum(front3_phone_fw_ver[10])) {
			ideal_ver[10] = front3_phone_fw_ver[10];
		}

		if(board_rev < normal_cri_rev && bVerNull == TRUE)
		{
			strcpy(cal_ver, ideal_ver);
			CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
			e_ctrl->is_supported = normal_is_supported;
		}

		snprintf(front3_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
			loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
			board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
			ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front3 info = %s", front3_module_info);
#endif

		/* update EEPROM fw version on sysfs */
		sprintf(front3_cam_fw_ver, "%s %s\n", front3_fw_ver, front3_load_fw_ver);
		sprintf(front3_cam_fw_full_ver, "%s %s %s\n", front3_fw_ver, front3_phone_fw_ver, front3_load_fw_ver);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front3 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
			front3_fw_ver[0], front3_fw_ver[1], front3_fw_ver[2], front3_fw_ver[3], front3_fw_ver[4],
			front3_fw_ver[5], front3_fw_ver[6], front3_fw_ver[7], front3_fw_ver[8], front3_fw_ver[9],
			front3_fw_ver[10]);
#endif
	}
    else if ((e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD) || 
			(e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD_FACTORY)) { //Front FT Cam
		/* front4 otp data valid bank check */
		if (!otp_data_s5k931_valid_bank)
		{
			snprintf(front4_sensor_id, FROM_SENSOR_ID_SIZE, "FF");
			front4_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
			snprintf(front4_module_id, FROM_FRONT_MODULE_ID_SIZE, "FF");
			front4_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
			snprintf(front4_module_info, SYSFS_MODULE_INFO_SIZE, "FF");
			front4_module_info[SYSFS_MODULE_INFO_SIZE] = '\0';
			CAM_INFO(CAM_EEPROM, "front4 has invalid OTP bank data");
			return rc;
		}

		/* front4 sensor id */
		memcpy(front4_sensor_id, &e_ctrl->cal_data.mapdata[FROM_FRONT4_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
		front4_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front4 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front4_sensor_id[0], front4_sensor_id[1], front4_sensor_id[2], front4_sensor_id[3],
			front4_sensor_id[4], front4_sensor_id[5], front4_sensor_id[6], front4_sensor_id[7],
			front4_sensor_id[8], front4_sensor_id[9], front4_sensor_id[10], front4_sensor_id[11],
			front4_sensor_id[12], front4_sensor_id[13], front4_sensor_id[14], front4_sensor_id[15]);

		/* front4 module id */
		memcpy(front4_module_id, &e_ctrl->cal_data.mapdata[FROM_FRONT4_MODULE_ID_ADDR], FROM_FRONT_MODULE_ID_SIZE);
		front4_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front4_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			front4_module_id[0], front4_module_id[1], front4_module_id[2], front4_module_id[3],
			front4_module_id[4], front4_module_id[5], front4_module_id[6], front4_module_id[7],
			front4_module_id[8], front4_module_id[9]);

#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front4_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
			front4_module_id[0], front4_module_id[1], front4_module_id[2], front4_module_id[3],
			front4_module_id[4], front4_module_id[5], front4_module_id[6], front4_module_id[7],
			front4_module_id[8], front4_module_id[9]);
#endif
        /* front4 mtf exif */
		memcpy(front4_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_FRONT4_MTF_ADDR], FROM_MTF_SIZE);
		front4_mtf_exif[FROM_MTF_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front4 mtf exif = %s", front4_mtf_exif);

		/* front4 manufacturer info */
		memcpy(front4_fw_ver, &e_ctrl->cal_data.mapdata[FRONT4_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
		front4_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front4 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front4_fw_ver[0], front4_fw_ver[1], front4_fw_ver[2], front4_fw_ver[3], front4_fw_ver[4],
			front4_fw_ver[5], front4_fw_ver[6], front4_fw_ver[7], front4_fw_ver[8], front4_fw_ver[9],
			front4_fw_ver[10]);

		/* temp front4 phone version */
		snprintf(front4_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", front4_hw_phone_info, front4_sw_phone_info, front4_vendor_phone_info, front4_process_phone_info);

		/* temp front4 load version */
		CAM_DBG(CAM_EEPROM, "Load from front4 EEPROM");
		strcpy(front4_load_fw_ver, front4_fw_ver);
		loadfrom = 'E';

		bVerNull = FALSE;
		for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
			if (e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD || e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD_FACTORY) {
				if(e_ctrl->cal_data.mapdata[FRONT4_MODULE_FW_VERSION + i] >= 0x80
					|| !isalnum(e_ctrl->cal_data.mapdata[FRONT4_MODULE_FW_VERSION + i])) {
					cal_ver[i] = ' ';
					bVerNull = TRUE;
				} else {
					cal_ver[i] = e_ctrl->cal_data.mapdata[FRONT4_MODULE_FW_VERSION + i];
				}
			}

			if(front4_phone_fw_ver[i] >= 0x80 || !isalnum(front4_phone_fw_ver[i]))
				front4_phone_fw_ver[i] = ' ';
		}
		sensor_ver[0] = front4_sensor_id[8];
		dll_ver[0] = e_ctrl->cal_data.mapdata[FRONT4_DLL_VERSION_ADDR] - '0';

		sensor_ver[1] = 0;
		dll_ver[1] = 0;

		normal_cri_rev = CRITERION_REV_FRONT4;
		strcpy(ideal_ver, front4_phone_fw_ver);
		if(front4_phone_fw_ver[9] < 0x80 && isalnum(front4_phone_fw_ver[9])) {
			ideal_ver[9] = front4_phone_fw_ver[9];
		}
		if(front4_phone_fw_ver[10] < 0x80 && isalnum(front4_phone_fw_ver[10])) {
			ideal_ver[10] = front4_phone_fw_ver[10];
		}

		if(board_rev < normal_cri_rev && bVerNull == TRUE)
		{
			strcpy(cal_ver, ideal_ver);
			CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
			e_ctrl->is_supported = normal_is_supported;
		}

		snprintf(front4_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
			loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
			board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
			ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front info = %s", front4_module_info);
#endif

		/* update EEPROM fw version on sysfs */
		sprintf(front4_cam_fw_ver, "%s %s\n", front4_fw_ver, front4_load_fw_ver);
		sprintf(front4_cam_fw_full_ver, "%s %s %s\n", front4_fw_ver, front4_phone_fw_ver, front4_load_fw_ver);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front4 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
			front4_fw_ver[0], front4_fw_ver[1], front4_fw_ver[2], front4_fw_ver[3], front4_fw_ver[4],
			front4_fw_ver[5], front4_fw_ver[6], front4_fw_ver[7], front4_fw_ver[8], front4_fw_ver[9],
			front4_fw_ver[10]);
#endif
	}
	else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && e_ctrl->soc_info.index == CAM_EEPROM_IDX_FT_MID) { //Front FT Cam
		/* front5 sensor id */
		memcpy(front5_sensor_id, &e_ctrl->cal_data.mapdata[FROM_FRONT5_SENSOR_ID_ADDR], FROM_SENSOR_ID_SIZE);
		front5_sensor_id[FROM_SENSOR_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front5 sensor id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front5_sensor_id[0], front5_sensor_id[1], front5_sensor_id[2], front5_sensor_id[3],
			front5_sensor_id[4], front5_sensor_id[5], front5_sensor_id[6], front5_sensor_id[7],
			front5_sensor_id[8], front5_sensor_id[9], front5_sensor_id[10], front5_sensor_id[11],
			front5_sensor_id[12], front5_sensor_id[13], front5_sensor_id[14], front5_sensor_id[15]);
		/* front5 module id */
		memcpy(front5_module_id, &e_ctrl->cal_data.mapdata[FROM_FRONT5_MODULE_ID_ADDR], FROM_FRONT_MODULE_ID_SIZE);
		front5_module_id[FROM_FRONT_MODULE_ID_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front5_module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			front5_module_id[0], front5_module_id[1], front5_module_id[2], front5_module_id[3],
			front5_module_id[4], front5_module_id[5], front5_module_id[6], front5_module_id[7],
			front5_module_id[8], front5_module_id[9]);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front5_module_id = %c %c %c %c %c %02X %02X %02X %02X %02X",
			front5_module_id[0], front5_module_id[1], front5_module_id[2], front5_module_id[3],
			front5_module_id[4], front5_module_id[5], front5_module_id[6], front5_module_id[7],
			front5_module_id[8], front5_module_id[9]);
#endif
        /* front5 mtf exif */
		memcpy(front5_mtf_exif, &e_ctrl->cal_data.mapdata[FROM_FRONT5_MTF_ADDR], FROM_MTF_SIZE);
		front5_mtf_exif[FROM_MTF_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "front5 mtf exif = %s", front5_mtf_exif);
		/* front5 manufacturer info */
		memcpy(front5_fw_ver, &e_ctrl->cal_data.mapdata[FRONT5_MODULE_FW_VERSION], FROM_MODULE_FW_INFO_SIZE);
		front5_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"front5 manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			front5_fw_ver[0], front5_fw_ver[1], front5_fw_ver[2], front5_fw_ver[3], front5_fw_ver[4],
			front5_fw_ver[5], front5_fw_ver[6], front5_fw_ver[7], front5_fw_ver[8], front5_fw_ver[9],
			front5_fw_ver[10]);
		/* temp front5 phone version */
		snprintf(front5_phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s", front5_hw_phone_info, front5_sw_phone_info, front5_vendor_phone_info, front5_process_phone_info);
		/* temp front5 load version */
		CAM_DBG(CAM_EEPROM, "Load from front5 EEPROM");
		strcpy(front5_load_fw_ver, front5_fw_ver);
		loadfrom = 'E';
		bVerNull = FALSE;
		for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
			if(e_ctrl->cal_data.mapdata[FRONT5_MODULE_FW_VERSION + i] >= 0x80
				|| !isalnum(e_ctrl->cal_data.mapdata[FRONT5_MODULE_FW_VERSION + i])) {
				cal_ver[i] = ' ';
				bVerNull = TRUE;
			} else {
				cal_ver[i] = e_ctrl->cal_data.mapdata[FRONT5_MODULE_FW_VERSION + i];
			}
			if(front5_phone_fw_ver[i] >= 0x80 || !isalnum(front5_phone_fw_ver[i]))
				front5_phone_fw_ver[i] = ' ';
		}
		sensor_ver[0] = front5_sensor_id[8];
		dll_ver[0] = e_ctrl->cal_data.mapdata[FRONT5_DLL_VERSION_ADDR] - '0';
		sensor_ver[1] = 0;
		dll_ver[1] = 0;
		normal_cri_rev = CRITERION_REV_FRONT5;
		strcpy(ideal_ver, front5_phone_fw_ver);
		if(front5_phone_fw_ver[9] < 0x80 && isalnum(front5_phone_fw_ver[9])) {
			ideal_ver[9] = front5_phone_fw_ver[9];
		}
		if(front5_phone_fw_ver[10] < 0x80 && isalnum(front5_phone_fw_ver[10])) {
			ideal_ver[10] = front5_phone_fw_ver[10];
		}
		if(board_rev < normal_cri_rev && bVerNull == TRUE)
		{
			strcpy(cal_ver, ideal_ver);
			CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
			e_ctrl->is_supported = normal_is_supported;
		}
		snprintf(front5_module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
			loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
			board_rev & 0xFF, map_ver, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
			ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front info = %s", front5_module_info);
#endif
		/* update EEPROM fw version on sysfs */
		sprintf(front5_cam_fw_ver, "%s %s\n", front5_fw_ver, front5_load_fw_ver);
		sprintf(front5_cam_fw_full_ver, "%s %s %s\n", front5_fw_ver, front5_phone_fw_ver, front5_load_fw_ver);
#ifdef CAM_EEPROM_DBG
		CAM_DBG(CAM_EEPROM, "front5 manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
			front5_fw_ver[0], front5_fw_ver[1], front5_fw_ver[2], front5_fw_ver[3], front5_fw_ver[4],
			front5_fw_ver[5], front5_fw_ver[6], front5_fw_ver[7], front5_fw_ver[8], front5_fw_ver[9],
			front5_fw_ver[10]);
#endif
	}

    /*rear cal check*/
    if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX564 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_VST_LEFT)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_VST_LEFT, CAM_SENSOR_TYPE_IDX_IMX564);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX564 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_VST_RIGHT)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_VST_RIGHT, CAM_SENSOR_TYPE_IDX_IMX564);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETFRONT_LEFT))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HETFRONT_LEFT, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETFRONT_RIGHT))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HETFRONT_RIGHT, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETDOWN_LEFT))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HETDOWN_LEFT, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HETDOWN_RIGHT))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HETDOWN_RIGHT, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_TOP_L))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HET_TOP_L, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_TOP_R))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HET_TOP_R, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_MID_L))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HET_MID_L, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && (e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_MID_R))
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HET_MID_R, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_BTM_L)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HET_BTM_L, CAM_SENSOR_TYPE_IDX_S5K931);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_HET_BTM_R)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_HET_BTM_R, CAM_SENSOR_TYPE_IDX_S5K931);
#if defined(CONFIG_SAMSUNG_REAR_TOF)
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K63D && e_ctrl->soc_info.index == CAM_EEPROM_IDX_BACK_TOF)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_BACK_TOF, CAM_SENSOR_TYPE_IDX_S5K63D);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K63D && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ITOF)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ITOF, CAM_SENSOR_TYPE_IDX_S5K63D);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX611 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_DTOF)
        rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_DTOF, CAM_SENSOR_TYPE_IDX_IMX611);
#endif
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LEFT)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_LEFT, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RIGHT)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_RIGHT, CAM_SENSOR_TYPE_IDX_S5K931_F);
	else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && e_ctrl->soc_info.hw_version < 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_FT_MID)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_FT_MID, CAM_SENSOR_TYPE_IDX_OVOG0TB);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_LD, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_RD, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LU)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_LU, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RU)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_RU, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LD_FACTORY)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_LD_FACTORY, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RD_FACTORY)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_RD_FACTORY, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_LU_FACTORY)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_LU_FACTORY, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_S5K931_F && e_ctrl->soc_info.hw_version >= 5 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_ET_RU_FACTORY)
		rc = cam_eeprom_check_firmware_cal(e_ctrl->is_supported, map_ver, e_ctrl->soc_info.hw_version, CAM_EEPROM_IDX_ET_RU_FACTORY, CAM_SENSOR_TYPE_IDX_S5K931_F);
    else {
        CAM_ERR(CAM_EEPROM,"Invalid index %d", e_ctrl->soc_info.index);
        rc = -EINVAL;
    }

    return rc;
}

void cam_eeprom_update_sysfs_fw_version(
	const char *update_fw_ver, cam_eeprom_fw_version_idx update_fw_index, cam_eeprom_idx_type idx, cam_sensor_idx_type id_type, uint32_t hw_version)
{
	char *pCAM_fw_version = "", *pCAM_fw_full_version = "";
	char *pEEPROM_fw_version = "", *pPHONE_fw_version = "", *pLOAD_fw_version = "";

	CAM_DBG(CAM_EEPROM, "camera_idx: %d, update_fw_ver: %s, update_fw_index: %d",
		idx, update_fw_ver, update_fw_index);

	if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && idx == CAM_EEPROM_IDX_VST_LEFT) {
		pEEPROM_fw_version = rear_fw_ver;
		pPHONE_fw_version = rear_phone_fw_ver;
		pLOAD_fw_version = rear_load_fw_ver;
		pCAM_fw_version = cam_fw_ver;
		pCAM_fw_full_version = cam_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && idx == CAM_EEPROM_IDX_VST_RIGHT) {
		pEEPROM_fw_version = rear2_fw_ver;
		pPHONE_fw_version = rear2_phone_fw_ver;
		pLOAD_fw_version = rear2_load_fw_ver;
		pCAM_fw_version = cam2_fw_ver;
		pCAM_fw_full_version = cam2_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETFRONT_LEFT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_TOP_L))) {
		pEEPROM_fw_version = rear3_fw_ver;
		pPHONE_fw_version = rear3_phone_fw_ver;
		pLOAD_fw_version = rear3_load_fw_ver;
		pCAM_fw_version = cam3_fw_ver;
		pCAM_fw_full_version = cam3_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETFRONT_RIGHT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_TOP_R))) {
		pEEPROM_fw_version = rear4_fw_ver;
		pPHONE_fw_version = rear4_phone_fw_ver;
		pLOAD_fw_version = rear4_load_fw_ver;
		pCAM_fw_version = cam4_fw_ver;
		pCAM_fw_full_version = cam4_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETDOWN_LEFT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_MID_L))) {
		pEEPROM_fw_version = rear5_fw_ver;
		pPHONE_fw_version = rear5_phone_fw_ver;
		pLOAD_fw_version = rear5_load_fw_ver;
		pCAM_fw_version = cam5_fw_ver;
		pCAM_fw_full_version = cam5_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETDOWN_RIGHT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_MID_R))) {
		pEEPROM_fw_version = rear6_fw_ver;
		pPHONE_fw_version = rear6_phone_fw_ver;
		pLOAD_fw_version = rear6_load_fw_ver;
		pCAM_fw_version = cam6_fw_ver;
		pCAM_fw_full_version = cam6_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && idx == CAM_EEPROM_IDX_HET_BTM_L) {
		pEEPROM_fw_version = rear7_fw_ver;
		pPHONE_fw_version = rear7_phone_fw_ver;
		pLOAD_fw_version = rear7_load_fw_ver;
		pCAM_fw_version = cam7_fw_ver;
		pCAM_fw_full_version = cam7_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && idx == CAM_EEPROM_IDX_HET_BTM_R) {
		pEEPROM_fw_version = rear8_fw_ver;
		pPHONE_fw_version = rear8_phone_fw_ver;
		pLOAD_fw_version = rear8_load_fw_ver;
		pCAM_fw_version = cam8_fw_ver;
		pCAM_fw_full_version = cam8_fw_full_ver;
	}
#if defined(CONFIG_SAMSUNG_REAR_TOF)
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF)) {
		pEEPROM_fw_version = rear_dtof_fw_ver;
		pPHONE_fw_version = rear_dtof_phone_fw_ver;
		pLOAD_fw_version = rear_dtof_load_fw_ver;
		pCAM_fw_version = cam_dtof_fw_ver;
		pCAM_fw_full_version = cam_dtof_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF) {
		pEEPROM_fw_version = rear_tof_fw_ver;
		pPHONE_fw_version = rear_tof_phone_fw_ver;
		pLOAD_fw_version = rear_tof_load_fw_ver;
		pCAM_fw_version = cam_tof_fw_ver;
		pCAM_fw_full_version = cam_tof_fw_full_ver;
	}
#endif
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && idx == CAM_EEPROM_IDX_ET_LEFT) || (hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_LU || idx == CAM_EEPROM_IDX_ET_LU_FACTORY)))) {
		pEEPROM_fw_version = front_fw_ver;
		pPHONE_fw_version = front_phone_fw_ver;
		pLOAD_fw_version = front_load_fw_ver;
		pCAM_fw_version = front_cam_fw_ver;
		pCAM_fw_full_version = front_cam_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && idx == CAM_EEPROM_IDX_ET_RIGHT) || (hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_RU || idx == CAM_EEPROM_IDX_ET_RU_FACTORY)))) {
		pEEPROM_fw_version = front2_fw_ver;
		pPHONE_fw_version = front2_phone_fw_ver;
		pLOAD_fw_version = front2_load_fw_ver;
		pCAM_fw_version = front2_cam_fw_ver;
		pCAM_fw_full_version = front2_cam_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_LD || idx == CAM_EEPROM_IDX_ET_LD_FACTORY)) {
		pEEPROM_fw_version = front3_fw_ver;
		pPHONE_fw_version = front3_phone_fw_ver;
		pLOAD_fw_version = front3_load_fw_ver;
		pCAM_fw_version = front3_cam_fw_ver;
		pCAM_fw_full_version = front3_cam_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_RD || idx == CAM_EEPROM_IDX_ET_RD_FACTORY)) {
		pEEPROM_fw_version = front4_fw_ver;
		pPHONE_fw_version = front4_phone_fw_ver;
		pLOAD_fw_version = front4_load_fw_ver;
		pCAM_fw_version = front4_cam_fw_ver;
		pCAM_fw_full_version = front4_cam_fw_full_ver;
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && hw_version < 5 && idx == CAM_EEPROM_IDX_FT_MID) {
		pEEPROM_fw_version = front5_fw_ver;
		pPHONE_fw_version = front5_phone_fw_ver;
		pLOAD_fw_version = front5_load_fw_ver;
		pCAM_fw_version = front5_cam_fw_ver;
		pCAM_fw_full_version = front5_cam_fw_full_ver;
	}

	if (update_fw_index == EEPROM_FW_VER)
		strlcpy(pEEPROM_fw_version, update_fw_ver, FROM_MODULE_FW_INFO_SIZE + 1);
	else if (update_fw_index == PHONE_FW_VER)
		strlcpy(pPHONE_fw_version, update_fw_ver, FROM_MODULE_FW_INFO_SIZE + 1);
	else
		strlcpy(pLOAD_fw_version, update_fw_ver, FROM_MODULE_FW_INFO_SIZE + 1);

	sprintf(pCAM_fw_version, "%s %s\n", pEEPROM_fw_version, pLOAD_fw_version);
	sprintf(pCAM_fw_full_version, "%s %s %s\n", pEEPROM_fw_version, pPHONE_fw_version, pLOAD_fw_version);

	CAM_DBG(CAM_EEPROM, "camera_idx: %d, pCAM_fw_full_version: %s", idx, pCAM_fw_full_version);
}

int32_t cam_eeprom_check_firmware_cal(uint32_t camera_cal_crc, uint8_t cal_map_version, uint32_t hw_version, cam_eeprom_idx_type idx, cam_sensor_idx_type id_type)
{
	int rc = 0;
	char final_cmd_ack[SYSFS_FW_VER_SIZE] = "NG_";
	char rear_cam_cal_ack[SYSFS_FW_VER_SIZE] = "NULL";
	char front_cal_ack[SYSFS_FW_VER_SIZE] = "NULL";
	uint8_t isNeedUpdate = TRUE;
	uint8_t version_isp = 0, version_module_maker_ver = 0;
	uint8_t isValid_EEPROM_data = TRUE;
	uint8_t isQCmodule = TRUE;
	uint8_t camera_cal_ack = OK;
	uint8_t camera_fw_crc = OK;
	uint8_t camera_fw_ack = OK;
#if defined(CONFIG_SAMSUNG_REAR_TOF)
	char rear_tof_cal_ack[SYSFS_FW_VER_SIZE] = "NULL";
	char rear_dtof_cal_ack[SYSFS_FW_VER_SIZE] = "NULL";
#endif

	if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && idx == CAM_EEPROM_IDX_VST_LEFT) {
		version_isp = rear_fw_ver[3];
		version_module_maker_ver = rear_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && idx == CAM_EEPROM_IDX_VST_RIGHT) {
		version_isp = rear2_fw_ver[3];
		version_module_maker_ver = rear2_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETFRONT_LEFT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_TOP_L))) {
		version_isp = rear3_fw_ver[3];
		version_module_maker_ver = rear3_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETFRONT_RIGHT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_TOP_R))) {
		version_isp = rear4_fw_ver[3];
		version_module_maker_ver = rear4_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETDOWN_LEFT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_MID_L))) {
		version_isp = rear5_fw_ver[3];
		version_module_maker_ver = rear5_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETDOWN_RIGHT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_MID_R))) {
		version_isp = rear6_fw_ver[3];
		version_module_maker_ver = rear6_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && idx == CAM_EEPROM_IDX_HET_BTM_L) {
		version_isp = rear7_fw_ver[3];
		version_module_maker_ver = rear7_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && idx == CAM_EEPROM_IDX_HET_BTM_R) {
		version_isp = rear8_fw_ver[3];
		version_module_maker_ver = rear8_fw_ver[10];
	}
#if defined(CONFIG_SAMSUNG_REAR_TOF)
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF)) {
		version_isp = rear_dtof_fw_ver[3];
		version_module_maker_ver = rear_dtof_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF) {
		version_isp = rear_tof_fw_ver[3];
		version_module_maker_ver = rear_tof_fw_ver[10];
	}
#endif
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && idx == CAM_EEPROM_IDX_ET_LEFT) || (hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_LU || idx == CAM_EEPROM_IDX_ET_LU_FACTORY)))) {
		version_isp = front_fw_ver[3];
		version_module_maker_ver = front_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && idx == CAM_EEPROM_IDX_ET_RIGHT) || (hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_RU || idx == CAM_EEPROM_IDX_ET_RU_FACTORY)))) {
		version_isp = front2_fw_ver[3];
		version_module_maker_ver = front2_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_LD || idx == CAM_EEPROM_IDX_ET_LD_FACTORY)) {
		version_isp = front3_fw_ver[3];
		version_module_maker_ver = front3_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_RD || idx == CAM_EEPROM_IDX_ET_RD_FACTORY)) {
		version_isp = front4_fw_ver[3];
		version_module_maker_ver = front4_fw_ver[10];
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && hw_version < 5 && idx == CAM_EEPROM_IDX_FT_MID) {
		version_isp = front5_fw_ver[3];
		version_module_maker_ver = front5_fw_ver[10];
	}
	else {
		CAM_DBG(CAM_EEPROM,"Invalid id %d",idx);
		return -EINVAL;
	}

	if (version_isp == 0xff || version_module_maker_ver == 0xff) {
		CAM_ERR(CAM_EEPROM, "invalid eeprom data");
		isValid_EEPROM_data = FALSE;
		cam_eeprom_update_sysfs_fw_version("NULL", EEPROM_FW_VER, idx, id_type, hw_version);
	}

	/* 1. check camera firmware and cal data */
	CAM_DBG(CAM_EEPROM, "camera_cal_crc: 0x%x, camera_fw_crc: 0x%x", camera_cal_crc, camera_fw_crc);

	if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && (idx == CAM_EEPROM_IDX_VST_LEFT || idx == CAM_EEPROM_IDX_VST_RIGHT)) {
		if (camera_fw_crc == OK)
			camera_fw_ack = OK;
		else {
			camera_fw_ack = CRASH;
			strncat(final_cmd_ack, "FW", 2);
		}
	} else
		camera_fw_ack = OK;

	if ((id_type == CAM_SENSOR_TYPE_IDX_IMX564 && (idx == CAM_EEPROM_IDX_VST_LEFT || idx == CAM_EEPROM_IDX_VST_RIGHT)) || (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && (idx == CAM_EEPROM_IDX_HETFRONT_LEFT || idx == CAM_EEPROM_IDX_HETFRONT_RIGHT || idx == CAM_EEPROM_IDX_HETDOWN_LEFT || idx == CAM_EEPROM_IDX_HETDOWN_RIGHT 
			|| idx == CAM_EEPROM_IDX_HET_TOP_L || idx == CAM_EEPROM_IDX_HET_TOP_R || idx == CAM_EEPROM_IDX_HET_MID_L || idx == CAM_EEPROM_IDX_HET_MID_R || idx == CAM_EEPROM_IDX_HET_BTM_L || idx == CAM_EEPROM_IDX_HET_BTM_R))) {
		if (camera_cal_crc == CAMERA_NORMAL_CAL_CRC) {
			camera_cal_ack = OK;
			strncpy(rear_cam_cal_ack, "Normal", SYSFS_FW_VER_SIZE);
		} else {
			camera_cal_ack = CRASH;
			strncpy(rear_cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		}
	}
#if defined(CONFIG_SAMSUNG_REAR_TOF)
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF)) {
		if (camera_cal_crc == CAMERA_NORMAL_CAL_CRC) {
			camera_cal_ack = OK;
			strncpy(rear_dtof_cal_ack, "Normal", SYSFS_FW_VER_SIZE);
		} else {
			camera_cal_ack = CRASH;
			strncpy(rear_dtof_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
			strncat(final_cmd_ack, "CD3", 3);
		}
	}
	else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF) {
		if (camera_cal_crc == CAMERA_NORMAL_CAL_CRC) {
			camera_cal_ack = OK;
			strncpy(rear_tof_cal_ack, "Normal", SYSFS_FW_VER_SIZE);
		} else {
			camera_cal_ack = CRASH;
			strncpy(rear_tof_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
			strncat(final_cmd_ack, "CD3", 3);
		}
	}
#endif
	else {
		if (camera_cal_crc == CAMERA_NORMAL_CAL_CRC) {
			camera_cal_ack = OK;
			strncpy(front_cal_ack, "Normal", SYSFS_FW_VER_SIZE);
		} else {
			camera_cal_ack = CRASH;
			strncpy(front_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
			strncat(final_cmd_ack, "CD3", 3);
		}
    }

	/* 3-1. all success case: display LOAD FW */
	if (camera_fw_ack && camera_cal_ack)
		isNeedUpdate = FALSE;

	/* 3-2. fail case: update CMD_ACK on sysfs (load fw) */
	// If not QC module, return NG.
	if(version_isp >= 0x80 || !isalnum(version_isp))
		CAM_DBG(CAM_EEPROM, "ISP Ver : 0x%x", version_isp);
	else
		CAM_DBG(CAM_EEPROM, "ISP Ver : %c", version_isp);

	if (version_isp != 'Q' && version_isp != 'U' && version_isp != 'A' && version_isp != 'X') {
		CAM_ERR(CAM_EEPROM, "This is not Qualcomm module!");

		if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && (idx == CAM_EEPROM_IDX_VST_LEFT || idx == CAM_EEPROM_IDX_VST_RIGHT)) {
			strncpy(final_cmd_ack, "NG_FWCD", SYSFS_FW_VER_SIZE);
			strncpy(rear_cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		}
#if defined(CONFIG_SAMSUNG_REAR_TOF)
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF)) {
			strncpy(final_cmd_ack, "NG_CD3_L", SYSFS_FW_VER_SIZE);
			strncpy(rear_dtof_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		}
		else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF) {
			strncpy(final_cmd_ack, "NG_CD3_L", SYSFS_FW_VER_SIZE);
			strncpy(rear_tof_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		}
#endif
		else {
			strncpy(final_cmd_ack, "NG_CD3_L", SYSFS_FW_VER_SIZE);
			strncpy(front_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		}
		isNeedUpdate = TRUE;
		isQCmodule = FALSE;
		camera_cal_ack = CRASH;
	}

	if (isNeedUpdate) {
		CAM_ERR(CAM_EEPROM, "final_cmd_ack : %s", final_cmd_ack);
		cam_eeprom_update_sysfs_fw_version(final_cmd_ack, LOAD_FW_VER, idx, id_type, hw_version);
	} else {
		// just display success fw version log
		CAM_DBG(CAM_EEPROM, "final_cmd_ack : %s", final_cmd_ack);
		memset(final_cmd_ack, 0, sizeof(final_cmd_ack));
		if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && idx == CAM_EEPROM_IDX_VST_LEFT)
			strcpy(final_cmd_ack, cam_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && idx == CAM_EEPROM_IDX_VST_RIGHT)
			strcpy(final_cmd_ack, cam2_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETFRONT_LEFT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_TOP_L)))
			strcpy(final_cmd_ack, cam3_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETFRONT_RIGHT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_TOP_R)))
			strcpy(final_cmd_ack, cam4_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETDOWN_LEFT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_MID_L)))
			strcpy(final_cmd_ack, cam5_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && idx == CAM_EEPROM_IDX_HETDOWN_RIGHT) || (hw_version >= 5 && idx == CAM_EEPROM_IDX_HET_MID_R)))
			strcpy(final_cmd_ack, cam6_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && idx == CAM_EEPROM_IDX_HET_BTM_L)
			strcpy(final_cmd_ack, cam7_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && idx == CAM_EEPROM_IDX_HET_BTM_R)
			strcpy(final_cmd_ack, cam8_fw_full_ver);
#if defined(CONFIG_SAMSUNG_REAR_TOF)
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF))
			strcpy(final_cmd_ack, cam_dtof_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF)
			strcpy(final_cmd_ack, cam_tof_fw_full_ver);
#endif
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && idx == CAM_EEPROM_IDX_ET_LEFT) || (hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_LU || idx == CAM_EEPROM_IDX_ET_LU_FACTORY))))
			strcpy(final_cmd_ack, front_cam_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && idx == CAM_EEPROM_IDX_ET_RIGHT) || (hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_RU || idx == CAM_EEPROM_IDX_ET_RU_FACTORY))))
			strcpy(final_cmd_ack, front2_cam_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_LD || idx == CAM_EEPROM_IDX_ET_LD_FACTORY))
			strcpy(final_cmd_ack, front3_cam_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (idx == CAM_EEPROM_IDX_ET_RD || idx == CAM_EEPROM_IDX_ET_RD_FACTORY))
			strcpy(final_cmd_ack, front4_cam_fw_full_ver);
		else if (id_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && idx == CAM_EEPROM_IDX_FT_MID)
			strcpy(final_cmd_ack, front5_cam_fw_full_ver);

		CAM_DBG(CAM_EEPROM, "final_cmd_ack : %s", final_cmd_ack);
	}

	/* 4. update CAL check ack on sysfs rear_calcheck */
	if ((id_type == CAM_SENSOR_TYPE_IDX_IMX564 && (idx == CAM_EEPROM_IDX_VST_LEFT || idx == CAM_EEPROM_IDX_VST_RIGHT)) || (id_type == CAM_SENSOR_TYPE_IDX_S5K931 && (idx == CAM_EEPROM_IDX_HETFRONT_LEFT || idx == CAM_EEPROM_IDX_HETFRONT_RIGHT || idx == CAM_EEPROM_IDX_HETDOWN_LEFT || idx == CAM_EEPROM_IDX_HETDOWN_RIGHT 
			 || idx == CAM_EEPROM_IDX_HET_TOP_L || idx == CAM_EEPROM_IDX_HET_TOP_R || idx == CAM_EEPROM_IDX_HET_MID_L || idx == CAM_EEPROM_IDX_HET_MID_R || idx == CAM_EEPROM_IDX_HET_BTM_L || idx == CAM_EEPROM_IDX_HET_BTM_R)))
		strlcpy(rear_cam_cal_check, rear_cam_cal_ack, SYSFS_FW_VER_SIZE);
#if defined(CONFIG_SAMSUNG_REAR_TOF)
	else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF))
		strlcpy(rear_dtof_cam_cal_check, rear_dtof_cal_ack, SYSFS_FW_VER_SIZE);
	else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF)
		strlcpy(rear_tof_cam_cal_check, rear_tof_cal_ack, SYSFS_FW_VER_SIZE);
#endif
	else
		strlcpy(front_cam_cal_check, front_cal_ack, SYSFS_FW_VER_SIZE);

	snprintf(cal_crc, SYSFS_FW_VER_SIZE, "%s %s\n", rear_cam_cal_check, front_cam_cal_check);

	/* 5. update module maker ver check on sysfs checkfw_user, checkfw_factory*/

	if (id_type == CAM_SENSOR_TYPE_IDX_IMX564 && (idx == CAM_EEPROM_IDX_VST_LEFT || idx == CAM_EEPROM_IDX_VST_RIGHT)) {
		CAM_DBG(CAM_EEPROM,
			"version_module_maker: 0x%x, MODULE_VER_ON_PVR: 0x%x, MODULE_VER_ON_SRA: 0x%x",
			version_module_maker_ver, MODULE_VER_ON_PVR, MODULE_VER_ON_SRA);
		CAM_DBG(CAM_EEPROM,
			"cal_map_version: 0x%x vs FROM_CAL_MAP_VERSION: 0x%x",
			cal_map_version, FROM_CAL_MAP_VERSION);

		if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < FROM_CAL_MAP_VERSION)
			|| (version_module_maker_ver < MODULE_VER_ON_PVR))) {
			strncpy(cam_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
		} else {
			if (camera_cal_ack == CRASH)
				strncpy(cam_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
			else
				strncpy(cam_fw_user_ver, "OK", SYSFS_FW_VER_SIZE);
		}

		if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < FROM_CAL_MAP_VERSION)
			|| (version_module_maker_ver < MODULE_VER_ON_SRA))) {
			strncpy(cam_fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
		} else {
			if (camera_cal_ack == CRASH)
				strncpy(cam_fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
			else
				strncpy(cam_fw_factory_ver, "OK", SYSFS_FW_VER_SIZE);
		}
	}
#if defined(CONFIG_SAMSUNG_REAR_TOF)
    else if (id_type == CAM_SENSOR_TYPE_IDX_S5K63D && (idx == CAM_EEPROM_IDX_BACK_TOF || idx == CAM_EEPROM_IDX_ITOF)) {
        CAM_DBG(CAM_EEPROM,
            "rear_tof_version_module_maker: 0x%x, REAR_TOF_MODULE_VER_ON_PVR: 0x%x, REAR_TOF_MODULE_VER_ON_SRA: 0x%x",
            version_module_maker_ver, REAR_TOF_MODULE_VER_ON_PVR, REAR_TOF_MODULE_VER_ON_SRA);
        CAM_DBG(CAM_EEPROM,
            "rear_tof_cal_map_version: 0x%x vs REAR_TOF_FROM_CAL_MAP_VERSION: 0x%x",
            cal_map_version, REAR_TOF_FROM_CAL_MAP_VERSION);

        if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < REAR_TOF_FROM_CAL_MAP_VERSION)
            || (version_module_maker_ver < REAR_TOF_MODULE_VER_ON_PVR))) {
            strncpy(cam_dtof_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
        } else {
            if (camera_cal_ack == CRASH)
                strncpy(cam_dtof_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
            else
                strncpy(cam_dtof_fw_user_ver, "OK", SYSFS_FW_VER_SIZE);
        }

        if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < REAR_TOF_FROM_CAL_MAP_VERSION)
            || (version_module_maker_ver < REAR_TOF_MODULE_VER_ON_SRA))) {
            strncpy(cam_dtof_fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
        } else {
            if (camera_cal_ack == CRASH)
                strncpy(cam_dtof_fw_factory_ver, "NG_CRC", SYSFS_FW_VER_SIZE);
            else
                strncpy(cam_dtof_fw_factory_ver, "OK", SYSFS_FW_VER_SIZE);
        }
    }
    else if (id_type == CAM_SENSOR_TYPE_IDX_IMX611 && idx == CAM_EEPROM_IDX_DTOF) {
        CAM_DBG(CAM_EEPROM,
            "rear_dtof_version_module_maker: 0x%x, REAR_DTOF_MODULE_VER_ON_PVR: 0x%x, REAR_DTOF_MODULE_VER_ON_SRA: 0x%x",
            version_module_maker_ver, REAR_DTOF_MODULE_VER_ON_PVR, REAR_DTOF_MODULE_VER_ON_SRA);
        CAM_DBG(CAM_EEPROM,
            "rear_dtof_cal_map_version: 0x%x vs REAR_DTOF_FROM_CAL_MAP_VERSION: 0x%x",
            cal_map_version, REAR_DTOF_FROM_CAL_MAP_VERSION);

        if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < REAR_DTOF_FROM_CAL_MAP_VERSION)
            || (version_module_maker_ver < REAR_DTOF_MODULE_VER_ON_PVR))) {
            strncpy(cam_tof_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
        } else {
            if (camera_cal_ack == CRASH)
                strncpy(cam_tof_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
            else
                strncpy(cam_tof_fw_user_ver, "OK", SYSFS_FW_VER_SIZE);
        }

        if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < REAR_DTOF_FROM_CAL_MAP_VERSION)
            || (version_module_maker_ver < REAR_DTOF_MODULE_VER_ON_SRA))) {
            strncpy(cam_tof_fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
        } else {
            if (camera_cal_ack == CRASH)
                strncpy(cam_tof_fw_factory_ver, "NG_CRC", SYSFS_FW_VER_SIZE);
            else
                strncpy(cam_tof_fw_factory_ver, "OK", SYSFS_FW_VER_SIZE);
        }
    }
#endif
	else {
		CAM_DBG(CAM_EEPROM,
			"front_version_module_maker: 0x%x, FRONT_MODULE_VER_ON_PVR: 0x%x, FRONT_MODULE_VER_ON_SRA: 0x%x",
			version_module_maker_ver, FRONT_MODULE_VER_ON_PVR, FRONT_MODULE_VER_ON_SRA);
		CAM_DBG(CAM_EEPROM,
			"front_cal_map_version: 0x%x vs FRONT_FROM_CAL_MAP_VERSION: 0x%x",
			cal_map_version, FRONT_FROM_CAL_MAP_VERSION);

		if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < FRONT_FROM_CAL_MAP_VERSION)
			|| (version_module_maker_ver < FRONT_MODULE_VER_ON_PVR))) {
			strncpy(front_cam_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
		} else {
			if (camera_cal_ack == CRASH)
				strncpy(front_cam_fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
			else
				strncpy(front_cam_fw_user_ver, "OK", SYSFS_FW_VER_SIZE);
		}

		if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (cal_map_version < FRONT_FROM_CAL_MAP_VERSION)
			|| (version_module_maker_ver < FRONT_MODULE_VER_ON_SRA))) {
			strncpy(front_cam_fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
		} else {
			strncpy(front_cam_fw_factory_ver, "OK", SYSFS_FW_VER_SIZE);
		}
	}

	return rc;
}

/**
 * cam_eeprom_verify_sum - verify crc32 checksum
 * @mem:			data buffer
 * @size:			size of data buffer
 * @sum:			expected checksum
 * @rev_endian:	compare reversed endian (0:little, 1:big)
 *
 * Returns 0 if checksum match, -EINVAL otherwise.
 */
int cam_eeprom_verify_sum(const char *mem, uint32_t size, uint32_t sum, uint32_t rev_endian)
{
	uint32_t crc = ~0;
	uint32_t cmp_crc = 0;

	/* check overflow */
	if (size > crc - sizeof(uint32_t))
		return -EINVAL;

	crc = crc32_le(crc, mem, size);

	crc = ~crc;
	if (rev_endian == 1) {
		cmp_crc = (((crc) & 0xFF) << 24)
				| (((crc) & 0xFF00) << 8)
				| (((crc) >> 8) & 0xFF00)
				| ((crc) >> 24);
	} else {
		cmp_crc = crc;
	}
	CAM_DBG(CAM_EEPROM, "endian %d, expect 0x%x, result 0x%x", rev_endian, sum, cmp_crc);

	if (cmp_crc != sum) {
		CAM_ERR(CAM_EEPROM, "endian %d, expect 0x%x, result 0x%x", rev_endian, sum, cmp_crc);
		return -EINVAL;
	} else {
		CAM_DBG(CAM_EEPROM, "checksum pass 0x%x", sum);
		return 0;
	}
}

/**
 * cam_eeprom_match_crc - verify multiple regions using crc
 * @data:	data block to be verified
 *
 * Iterates through all regions stored in @data.  Regions with odd index
 * are treated as data, and its next region is treated as checksum.  Thus
 * regions of even index must have valid_size of 4 or 0 (skip verification).
 * Returns a bitmask of verified regions, starting from LSB.  1 indicates
 * a checksum match, while 0 indicates checksum mismatch or not verified.
 */
uint32_t cam_eeprom_match_crc(struct cam_eeprom_memory_block_t *data, uint32_t subdev_id, uint32_t subdev_type, uint32_t hw_version)
{
	int j, rc;
	uint32_t *sum;
	uint32_t ret = 0;
	uint8_t *memptr, *memptr_crc;
	struct cam_eeprom_memory_map_t *map;
	uint8_t map_ver = 0;

	if (!data) {
		CAM_ERR(CAM_EEPROM, "data is NULL");
		return -EINVAL;
	}
	map = data->map;

	if (subdev_type == CAM_SENSOR_TYPE_IDX_IMX564 && subdev_id == CAM_EEPROM_IDX_VST_LEFT) {
		map_ver = data->mapdata[REAR_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_IMX564 && subdev_id == CAM_EEPROM_IDX_VST_RIGHT) {
		map_ver = data->mapdata[REAR2_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && subdev_id == CAM_EEPROM_IDX_HETFRONT_LEFT) || (hw_version >= 5 && subdev_id == CAM_EEPROM_IDX_HET_TOP_L))) {
		map_ver = data->mapdata[REAR3_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && subdev_id == CAM_EEPROM_IDX_HETFRONT_RIGHT) || (hw_version >= 5 && subdev_id == CAM_EEPROM_IDX_HET_TOP_R))) {
		map_ver = data->mapdata[REAR4_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && subdev_id == CAM_EEPROM_IDX_HETDOWN_LEFT) || (hw_version >= 5 && subdev_id == CAM_EEPROM_IDX_HET_MID_L))) {
		map_ver = data->mapdata[REAR5_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931 && ((hw_version < 5 && subdev_id == CAM_EEPROM_IDX_HETDOWN_RIGHT) || (hw_version >= 5 && subdev_id == CAM_EEPROM_IDX_HET_MID_R))) {
		map_ver = data->mapdata[REAR6_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931 && subdev_id == CAM_EEPROM_IDX_HET_BTM_L) {
		map_ver = data->mapdata[REAR7_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931 && subdev_id == CAM_EEPROM_IDX_HET_BTM_R) {
		map_ver = data->mapdata[REAR8_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && subdev_id == CAM_EEPROM_IDX_ET_LEFT) || (hw_version >= 5 && (subdev_id == CAM_EEPROM_IDX_ET_LU || subdev_id == CAM_EEPROM_IDX_ET_LU_FACTORY)))) {
		map_ver = data->mapdata[FRONT_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931_F && ((hw_version < 5 && subdev_id == CAM_EEPROM_IDX_ET_RIGHT) || (hw_version >= 5 && (subdev_id == CAM_EEPROM_IDX_ET_RU || subdev_id == CAM_EEPROM_IDX_ET_RU_FACTORY)))) {
		map_ver = data->mapdata[FRONT2_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (subdev_id == CAM_EEPROM_IDX_ET_LD || subdev_id == CAM_EEPROM_IDX_ET_LD_FACTORY)) {
		map_ver = data->mapdata[FRONT3_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K931_F && hw_version >= 5 && (subdev_id == CAM_EEPROM_IDX_ET_RD || subdev_id == CAM_EEPROM_IDX_ET_RD_FACTORY)) {
		map_ver = data->mapdata[FRONT4_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_OVOG0TB && hw_version < 5 && subdev_id == CAM_EEPROM_IDX_FT_MID) {
		map_ver = data->mapdata[FRONT5_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_S5K63D && (subdev_id == CAM_EEPROM_IDX_BACK_TOF || subdev_id == CAM_EEPROM_IDX_ITOF)) {
		map_ver = data->mapdata[REAR_TOF_CAM_MAP_VERSION_ADDR];
	}
	else if (subdev_type == CAM_SENSOR_TYPE_IDX_IMX611 && subdev_id == CAM_EEPROM_IDX_DTOF) {
		map_ver = data->mapdata[REAR_DTOF_CAM_MAP_VERSION_ADDR];
	}
	else
	{
		CAM_DBG(CAM_EEPROM, "Invalid subdev_id %d",subdev_id);
		return -EINVAL;
	}

	if(map_ver >= 0x80 || !isalnum(map_ver))
		CAM_DBG(CAM_EEPROM, "map subdev_id = %d, version = 0x%x", subdev_id, map_ver);
	else
		CAM_DBG(CAM_EEPROM, "map subdev_id = %d, version = %c [0x%x]", subdev_id, map_ver, map_ver);

	//  idx 0 is the actual reading section (whole data)
	//  from idx 1, start to compare CRC checksum
	//  (1: CRC area for header, 2: CRC value)
	for (j = 1; j + 1 < data->num_map; j += 2) {
		memptr = data->mapdata + map[j].mem.addr;
		memptr_crc = data->mapdata + map[j+1].mem.addr;

		/* empty table or no checksum */
		if (!map[j].mem.valid_size || !map[j+1].mem.valid_size) {
			CAM_ERR(CAM_EEPROM, "continue");
			continue;
		}

		if (map[j+1].mem.valid_size < sizeof(uint32_t)) {
			CAM_ERR(CAM_EEPROM, "[%d : size 0x%X] malformatted data mapping", j+1, map[j+1].mem.valid_size);
			return -EINVAL;
		}
		CAM_DBG(CAM_EEPROM, "[%d] memptr 0x%x, memptr_crc 0x%x", j, map[j].mem.addr, map[j + 1].mem.addr);
		sum = (uint32_t *) (memptr_crc + map[j+1].mem.valid_size - sizeof(uint32_t));
		rc = cam_eeprom_verify_sum(memptr, map[j].mem.valid_size, *sum, 0);

		if (!rc)
			ret |= 1 << (j/2);
	}

	CAM_DBG(CAM_EEPROM, "CRC result = 0x%08X", ret);
	return ret;
}

#if defined(CONFIG_SAMSUNG_CAMERA_OTP)
/**
 * cam_otp_hi1336c_init() - init for hi1336C OTP
 * @io_master_info:	otp io struct
 *
 * This function is used for initilize hi1336c OTP to read/write data from OTP
 */

static int cam_otp_s5k931_init( struct camera_io_master *io_master_info)
{
    int	rc = 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    /* load otp global setfile */
	rc = camera_io_dev_single_write(io_master_info, 0xFCFC, 0x4000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x0136, 0x1333, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x0304, 0x0003, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x0306, 0x00E1, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x030E, 0x0003, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x0310, 0x00B0, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x0312, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);

	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "load otp globle setfile failed" );
	    return(rc);
	}

	/* OTP initial setting1 write */
	//rc = camera_io_dev_write( io_master_info, &s5k931_otp_init_setting1 );
	rc = camera_io_dev_single_write(io_master_info, 0x0100, 0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "load otp initial setfile1 failed" );
	    return(rc);
	}
	/* delay 10ms */
	msleep(10);

	CAM_DBG(CAM_EEPROM, "load otp init setting done!");
	return rc;
}

/**
 * cam_otp_s5k931_setpage() - read map data into buffer for hi1336c
 * @io_master_info:	otp io struct
 * @addr:	memory address to be read
 * @memptr	memory address to be stored
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */

static int cam_otp_s5k931_setpage( struct camera_io_master *io_master_info, uint32_t addr,
                                uint8_t *memptr )
{
	int rc = 0;
	int retry_cnt = 25;
	struct cam_sensor_i2c_reg_setting	i2c_reg_settings;
	struct cam_sensor_i2c_reg_array		i2c_reg_array;
	enum camera_sensor_i2c_type		addr_type	= CAMERA_SENSOR_I2C_TYPE_WORD;
	enum camera_sensor_i2c_type		data_type	= CAMERA_SENSOR_I2C_TYPE_BYTE;
	uint32_t read_value = 0, valid_flag = 0, otp_page = 0;

	if ( !io_master_info )
	{
	    CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
	    return(-EINVAL);
	}

	i2c_reg_settings.addr_type	= addr_type;
	i2c_reg_settings.data_type	= data_type;
	i2c_reg_settings.size		= 1;
	i2c_reg_settings.delay		= 4;
	i2c_reg_array.delay		= 4;

	/* Page Select (1 page) */
	i2c_reg_array.reg_addr		= 0x0A02;
	i2c_reg_array.reg_data		= 0x003F;
	i2c_reg_settings.reg_setting	= &i2c_reg_array;

	//rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
	rc = camera_io_dev_single_write(io_master_info, 0x0A02, 0x003F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
		CAM_ERR( CAM_EEPROM, "continuous read failed" );
		goto err;
	}
	i2c_reg_array.reg_addr		= 0x0A00;
	i2c_reg_array.reg_data		= 0x0100;
	i2c_reg_settings.reg_setting	= &i2c_reg_array;

	//rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
	rc = camera_io_dev_single_write(io_master_info, 0x0A00, 0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
		CAM_ERR( CAM_EEPROM, "continuous read failed" );
		goto err;
	}
	/* delay 10ms */
	msleep(10);
	addr = 0x0A00; read_value = 0;

	/* OTP data verify */
	do {
		rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
			break;
		}
		if(read_value == 0x0)
		{
			CAM_DBG(CAM_EEPROM, "Success to read addr=0x%x read_value=0x%x", addr, read_value );
			retry_cnt = 0;
			break;
		}
		else
		{
			CAM_DBG(CAM_EEPROM, "Not ready. addr=0x%x read_value=0x%x. Retry: %d", addr, read_value, retry_cnt);
			retry_cnt--;
			msleep(5);
		}
	} while (retry_cnt > 0);

	addr = 0x0A02; otp_page = 0;
	rc = camera_io_dev_read( io_master_info, addr, &otp_page, addr_type, addr_type );
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
	}

	/* OTP data read */
	//rc = camera_io_dev_read_seq( io_master_info, 0x0A04, memptr, addr_type, data_type, 1 );
	addr = 0x0A04; read_value = 0;
	rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
	}
	CAM_DBG(CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_value, *memptr );
	valid_flag = read_value >> 8;
	CAM_DBG(CAM_EEPROM, "OTP_Page: [0x%x] Valid_flag value = [0x%x]", otp_page, valid_flag);

	if(valid_flag != 0x11)
	{
		CAM_DBG( CAM_EEPROM, "1st OTP bank [0x%x] has invalid data, valid_flag value: [0x%x]. Now check 2nd OTP bank data", otp_page, valid_flag);
		/* Page Select (2 page) */
		rc = camera_io_dev_single_write(io_master_info, 0x0A02, 0x003E, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "continuous read failed" );
			goto err;
		}

		rc = camera_io_dev_single_write(io_master_info, 0x0A00, 0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "continuous read failed" );
			goto err;
		}
		/* delay 10ms */
		msleep(10);
		addr = 0x0A00; read_value = 0; retry_cnt = 25;

		/* OTP data verify */
		do {
			rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
			if ( rc < 0 )
			{
				CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
				break;
			}
			if(read_value == 0x0)
			{
				CAM_DBG(CAM_EEPROM, "Success to read addr=0x%x read_value=0x%x", addr, read_value );
				retry_cnt = 0;
				break;
			}
			else
			{
				CAM_DBG(CAM_EEPROM, "Not ready. addr=0x%x read_value=0x%x. Retry: %d", addr, read_value, retry_cnt);
				retry_cnt--;
				msleep(5);
			}
		} while (retry_cnt > 0);

		addr = 0x0A02; otp_page = 0;
		rc = camera_io_dev_read( io_master_info, addr, &otp_page, addr_type, addr_type );
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
		}

		addr = 0x0A04; read_value = 0;
		rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
		}
		CAM_DBG(CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_value, *memptr );
		valid_flag = read_value >> 8;
		CAM_DBG(CAM_EEPROM, "2nd OTP_Page: [0x%x] Valid_flag value = [0x%x]", otp_page, valid_flag);
		if(valid_flag != 0x11)
		{
			otp_data_s5k931_valid_bank = 0;
			CAM_ERR( CAM_EEPROM, "2nd OTP bank [0x%x] has also invalid data, valid_flag value: [0x%x]. No valid bank available", otp_page, valid_flag);
		}
	}

	addr = 0x0A06; read_value = 0;
	rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
	}
	CAM_DBG(CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_value, *memptr );
	addr = 0x0A08; read_value = 0;
	rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
	}
	CAM_DBG(CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_value, *memptr );
	addr = 0x0A0A; read_value = 0;
	rc = camera_io_dev_read( io_master_info, addr, &read_value, addr_type, addr_type );
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
	}
	CAM_DBG(CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_value, *memptr );

	err:
	return(rc);
}

/**
 * cam_otp_s5k931_burst_read() - read map data into buffer with burst mode
 * @io_master_info:	otp io struct
 * @addr: start memory for reading
 * @memptr:	block to be read
 * @read_size: buffer count to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */
#if 0
static int cam_otp_s5k931_burst_read( struct camera_io_master *io_master_info, uint32_t addr,
                                      uint8_t *memptr, uint32_t read_size )
{
    int					rc = 0;
#if 0
    struct cam_sensor_i2c_reg_setting	i2c_reg_settings;
    struct cam_sensor_i2c_reg_array		i2c_reg_array;
    enum camera_sensor_i2c_type		addr_type	= CAMERA_SENSOR_I2C_TYPE_WORD;
    enum camera_sensor_i2c_type		data_type	= CAMERA_SENSOR_I2C_TYPE_BYTE;
    uint32_t				read_addr		= 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    i2c_reg_settings.addr_type	= addr_type;
    i2c_reg_settings.data_type	= data_type;
    i2c_reg_settings.size		= 1;
    i2c_reg_settings.delay		= 4;
    i2c_reg_array.delay		= 4;

    /* high address */
    i2c_reg_array.reg_addr		= 0x030a;
    i2c_reg_array.reg_data		= (addr >> 8) & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write high address failed" );
        goto err;
    }

    /* low address */
    i2c_reg_array.reg_addr		= 0x030b;
    i2c_reg_array.reg_data		= addr & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write low address failed" );
        goto err;
    }

    /* OTP read mode */
    i2c_reg_array.reg_addr		= 0x0302;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data verify*/
    rc = camera_io_dev_read( io_master_info, 0x030a, &read_addr, addr_type, addr_type );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    if(read_addr != addr)
		CAM_DBG(CAM_EEPROM, "addr=0x%x read_addr=0x%x", addr, read_addr );

    /* burst read on */
    i2c_reg_array.reg_addr		= 0x0712;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data burst read */
    rc = camera_io_dev_read_seq( io_master_info, 0x0308, memptr, addr_type, data_type, read_size );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    /* burst read off */
    i2c_reg_array.reg_addr		= 0x0712;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }
#endif
	if ( !io_master_info )
	{
	    CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
	    return(-EINVAL);
	}

	/* OTP data burst read */
	rc = camera_io_dev_read_seq( io_master_info, addr, memptr, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, read_size );
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
	}

//err:
    return(rc);
}
#endif

/**
 * cam_otp_s5k931_read_memory() - read map data into buffer
 * @e_ctrl:	otp control struct
 * @block:	block to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */

static int cam_otp_s5k931_read_memory( struct cam_eeprom_ctrl_t *e_ctrl,
                                       struct cam_eeprom_memory_block_t *block )

{
    struct cam_eeprom_memory_map_t	*emap	= block->map;
    struct cam_eeprom_soc_private	*eb_info;
    uint32_t	addr		= 0;
    uint32_t	read_size	= 0;
    uint32_t	offset = 0;
    uint8_t		OTP_Bank	= 0;
    uint8_t				*memptr = block->mapdata;
    int		read_bytes	= 0;
    int		rc	= 0;
    int		j	= 0;

    if ( !e_ctrl )
    {
        CAM_ERR( CAM_EEPROM, "e_ctrl is NULL" );
        return(-EINVAL);
    }

    eb_info = (struct cam_eeprom_soc_private *) e_ctrl->soc_info.soc_private;

    rc = cam_otp_s5k931_init(&e_ctrl->io_master_info);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "OTP init failed" );
        goto err;
    }

    /* select bank */
    rc = cam_otp_s5k931_setpage( &e_ctrl->io_master_info, SENSOR_S5K931_OTP_BANK_SELECT_REGISTER, &OTP_Bank );

    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read data failed" );
        goto err;
    }
    CAM_DBG(CAM_EEPROM, "current OTP_Bank: %d", OTP_Bank );

    switch ( OTP_Bank )
    {
    /* Refer to OTP document */
    case 0:
		offset = 0x00;
        break;
    case 1:
        offset = 0x0400;
        break;

    case 3:
        offset = 0x0900;
        break;

    case 7:
        offset = 0x0E00;
        break;

    case 0xF:
        offset = 0x1300;
        break;

    default:
        CAM_DBG(CAM_EEPROM, "Bank error : Bank(%d)", OTP_Bank );
        return EINVAL;
    }
    CAM_DBG(CAM_EEPROM, "read OTP offset: 0x%x", offset );

    for ( j = 1; j < block->num_map; j++ )
    {
        read_size	= emap[j].mem.valid_size;
        memptr		= block->mapdata + emap[j].mem.addr;
        addr		= emap[j].mem.addr + offset;

		CAM_DBG(CAM_EEPROM, "emap[%d / %d].mem.addr=0x%x OTP addr=0x%x read_size=0x%x mapdata=%pK memptr=%pK subdev=%d type=%d",
                  j, block->num_map, emap[j].mem.addr, addr, read_size, block->mapdata, memptr, e_ctrl->soc_info.index, e_ctrl->eeprom_device_type );

        rc = camera_io_dev_read_seq( &e_ctrl->io_master_info, addr, memptr, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, read_size);
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "OTP read dev failed.!!!" );
			break;
		}
		memptr		+= read_size;
	}
	CAM_DBG(CAM_EEPROM, "read data done memptr=%pK data: 0x%x VR:: End read_bytes=0x%x\n", memptr, *memptr, read_bytes );

    /* OTP finish setting1 write */
	rc = camera_io_dev_single_write(&e_ctrl->io_master_info, 0x0A00, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
		CAM_ERR( CAM_EEPROM, "load otp finish setfile1 failed" );
		return(rc);
	}
	mdelay(2);
	rc = camera_io_dev_single_write(&e_ctrl->io_master_info, 0x0100, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
		CAM_ERR( CAM_EEPROM, "load otp finish setfile1 failed" );
		return(rc);
	}
	mdelay(2);
err:
    return(rc);
}

/**
 * cam_otp_ovog0tb_init() - init for ovog0tb OTP
 * @io_master_info:	otp io struct
 *
 * This function is used for initilize ovog0tb OTP to read/write data from OTP
 */

static int cam_otp_ovog0tb_init( struct camera_io_master *io_master_info)
{
    int	rc = 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    /* load otp global setfile */
	rc = camera_io_dev_single_write(io_master_info, 0xFCFC, 0x3D84, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x3D84, 0x40, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x3D88, 0x70, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x3D89, 0x70, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x3D8a, 0x70, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x3D8b, 0x7F, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
		return(rc);
	rc = camera_io_dev_single_write(io_master_info, 0x3D81, 0x01, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);

	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "load otp globle setfile failed" );
	    return(rc);
	}

	/* OTP initial setting1 write */
	//rc = camera_io_dev_write( io_master_info, &ovog0tb_otp_init_setting1 );
	rc = camera_io_dev_single_write(io_master_info, 0x0100, 0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
	    CAM_ERR( CAM_EEPROM, "load otp initial setfile1 failed" );
	    return(rc);
	}
	/* delay 10ms */
	msleep(10);

	CAM_DBG(CAM_EEPROM, "load otp init setting done!");
	return rc;
}

/**
 * cam_otp_ovog0tb_read_memory() - read map data into buffer
 * @e_ctrl:	otp control struct
 * @block:	block to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */

static int cam_otp_ovog0tb_read_memory( struct cam_eeprom_ctrl_t *e_ctrl,
                                       struct cam_eeprom_memory_block_t *block )

{
    struct cam_eeprom_memory_map_t	*emap	= block->map;
    struct cam_eeprom_soc_private	*eb_info;
    uint32_t	addr		= 0;
    uint32_t	read_size	= 0;
    uint32_t	offset = 0;
    uint8_t				*memptr = block->mapdata;
    int		read_bytes	= 0;
    int		rc	= 0;
    int		j	= 0;

    if ( !e_ctrl )
    {
        CAM_ERR( CAM_EEPROM, "e_ctrl is NULL" );
        return(-EINVAL);
    }

    eb_info = (struct cam_eeprom_soc_private *) e_ctrl->soc_info.soc_private;

    rc = cam_otp_ovog0tb_init(&e_ctrl->io_master_info);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "OTP init failed" );
        goto err;
    }

    for ( j = 1; j < block->num_map; j++ )
    {
        read_size	= emap[j].mem.valid_size;
        memptr		= block->mapdata + emap[j].mem.addr;
        addr		= emap[j].mem.addr + offset;

		CAM_DBG(CAM_EEPROM, "emap[%d / %d].mem.addr=0x%x OTP addr=0x%x read_size=0x%x mapdata=%pK memptr=%pK subdev=%d type=%d",
                  j, block->num_map, emap[j].mem.addr, addr, read_size, block->mapdata, memptr, e_ctrl->soc_info.index, e_ctrl->eeprom_device_type );

        rc = camera_io_dev_read_seq( &e_ctrl->io_master_info, addr, memptr, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, read_size);
		if ( rc < 0 )
		{
			CAM_ERR( CAM_EEPROM, "OTP read dev failed.!!!" );
			break;
		}
		memptr		+= read_size;
	}
	CAM_DBG(CAM_EEPROM, "read data done memptr=%pK data: 0x%x VR:: End read_bytes=0x%x\n", memptr, *memptr, read_bytes );

    /* OTP finish setting1 write */
	rc = camera_io_dev_single_write(&e_ctrl->io_master_info, 0x3D80, 0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
		CAM_ERR( CAM_EEPROM, "load otp finish setfile1 failed" );
		return(rc);
	}
	mdelay(2);
	rc = camera_io_dev_single_write(&e_ctrl->io_master_info, 0x0100, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 20);
	if ( rc < 0 )
	{
		CAM_ERR( CAM_EEPROM, "load otp finish setfile1 failed" );
		return(rc);
	}
	mdelay(2);
err:
    return(rc);
}

/**
  * cam_otp_read_memory() - read map data into buffer
  * @e_ctrl:	otp control struct
  * @block:	block to be read
  *
  * This function iterates through blocks stored in block->map, reads each
  * region and concatenate them into the pre-allocated block->mapdata
  */
int cam_otp_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
                               struct cam_eeprom_memory_block_t *block)
{
	int rc = 0;
	struct cam_eeprom_soc_private  *soc_private;

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;

	if(e_ctrl->soc_info.is_otp_sensor == 1 && (soc_private->i2c_info.slave_addr == 0x5A || soc_private->i2c_info.slave_addr == 0xAC)) {
		CAM_DBG(CAM_EEPROM, "It is Het-ET otp sensor with slave_addr 0x%x", soc_private->i2c_info.slave_addr);
		rc = cam_otp_s5k931_read_memory(e_ctrl, block);
	} else if(e_ctrl->soc_info.is_otp_sensor == 1 && soc_private->i2c_info.slave_addr == 0x6C) {
		CAM_DBG(CAM_EEPROM, "It is FT otp sensor with slave_addr 0x6C");
		rc = cam_otp_ovog0tb_read_memory(e_ctrl, block);
	}

	return rc;
}
#endif

/**
 * cam_eeprom_calc_calmap_size - Calculate cal array size based on the cal map
 * @e_ctrl:       ctrl structure
 *
 * Returns size of cal array
 */
int32_t cam_eeprom_calc_calmap_size(struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_eeprom_memory_map_t    *map = NULL;
	uint32_t minMap, maxMap, minLocal, maxLocal;
	int32_t i;
	int32_t calmap_size = 0;

	if (e_ctrl == NULL ||
		(e_ctrl->cal_data.num_map == 0) ||
		(e_ctrl->cal_data.map == NULL)) {
		CAM_DBG(CAM_EEPROM, "cal size is wrong");
		return calmap_size;
	}

	map = e_ctrl->cal_data.map;
	minMap = minLocal = 0xFFFFFFFF;
	maxMap = maxLocal = 0x00;

	for (i = 0; i < e_ctrl->cal_data.num_map; i++) {
		minLocal = map[i].mem.addr;
		maxLocal = minLocal + map[i].mem.valid_size;

		if (minMap > minLocal)
		{
			minMap = minLocal;
		}

		if (maxMap < maxLocal)
		{
			maxMap = maxLocal;
		}

		CAM_DBG(CAM_EEPROM, "[%d / %d] minLocal = 0x%X, minMap = 0x%X, maxLocal = 0x%X, maxMap = 0x%X",
			i+1, e_ctrl->cal_data.num_map, minLocal, minMap, maxLocal, maxMap);
	}
	calmap_size = maxMap - minMap;

	CAM_DBG(CAM_EEPROM, "calmap_size = 0x%X, minMap = 0x%X, maxMap = 0x%X",
		calmap_size, minMap, maxMap);

	return calmap_size;
}

/**
 * cam_eeprom_get_phone_ver - parse the userspace IO config and
 *                            read phone version at eebindriver.bin
 * @e_ctrl:     ctrl structure
 * @csl_packet: csl packet received
 *
 * Returns success or failure
 */

int32_t cam_eeprom_get_phone_ver(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_buf_io_cfg *io_cfg;
	uint32_t              i = 0, j = 0;
	int                   rc = 0;
	uintptr_t              buf_addr;
	size_t                buf_size;
	uint8_t               *read_buffer;

	int                   nVer = 0;
	uint8_t               *pBuf = NULL;
	uint8_t	              bVerNormal = TRUE;

	char                  tmp_hw_info[HW_INFO_MAX_SIZE] = HW_INFO;
	char                  tmp_sw_info[SW_INFO_MAX_SIZE] = SW_INFO;
	char                  tmp_vendor_info[VENDOR_INFO_MAX_SIZE] = VENDOR_INFO;
	char                  tmp_process_info[PROCESS_INFO_MAX_SIZE] = PROCESS_INFO;
	unsigned int          tmp_rev = 0;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
		&csl_packet->payload +
		csl_packet->io_configs_offset);

	CAM_DBG(CAM_EEPROM, "number of IO configs: %d:",
		csl_packet->num_io_configs);

	for (i = 0; i < csl_packet->num_io_configs; i++) {
		CAM_DBG(CAM_EEPROM, "Direction: %d:", io_cfg->direction);
		if (io_cfg->direction == CAM_BUF_OUTPUT) {
			rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
				&buf_addr, &buf_size);
			CAM_DBG(CAM_EEPROM, "buf_addr : %pK, buf_size : %zu",
				(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_EEPROM,
					"invalid buffer to copy data");
				return -EINVAL;
			}
			read_buffer += io_cfg->offsets[0];

			if (buf_size < e_ctrl->cal_data.num_data) {
				CAM_ERR(CAM_EEPROM,
					"failed to copy, Invalid size");
				return -EINVAL;
			}

			CAM_DBG(CAM_EEPROM, "copy the data, len:%d, read_buffer[0] = %d, read_buffer[4] = %d",
				e_ctrl->cal_data.num_data, read_buffer[0], read_buffer[4]);

			pBuf = read_buffer;
			memcpy(&nVer, pBuf, sizeof(int));
			pBuf += sizeof(int);

			memcpy(&tmp_rev, pBuf, sizeof(int));
			pBuf += sizeof(int);

			bVerNormal = TRUE;
			for(j = 0; j < FROM_MODULE_FW_INFO_SIZE; j ++) {
				CAM_DBG(CAM_EEPROM, "mapdata[0x%04X] = 0x%02X",
					REAR_MODULE_FW_VERSION + j,
					e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + j]);

				if(e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + j] >= 0x80
					|| !isalnum(e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + j] & 0xFF)) {
					CAM_ERR(CAM_EEPROM, "Invalid Version");
					bVerNormal = FALSE;
					break;
				}
			}

			if(bVerNormal == TRUE) {
				memcpy(hw_phone_info, &e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION],
					HW_INFO_MAX_SIZE);
				hw_phone_info[HW_INFO_MAX_SIZE-1] = '\0';
				CAM_DBG(CAM_EEPROM, "hw_phone_info: %s", hw_phone_info);
			} else {
				memcpy(hw_phone_info, HW_INFO, HW_INFO_MAX_SIZE);
				memcpy(sw_phone_info, SW_INFO, SW_INFO_MAX_SIZE);
				memcpy(vendor_phone_info, VENDOR_INFO, VENDOR_INFO_MAX_SIZE);
				memcpy(process_phone_info, PROCESS_INFO, PROCESS_INFO_MAX_SIZE);
				CAM_DBG(CAM_EEPROM, "Set Ver : %s %s %s %s",
					hw_phone_info, sw_phone_info,
					vendor_phone_info, process_phone_info);
			}

			CAM_DBG(CAM_EEPROM, "hw_phone_info: %s", hw_phone_info);

			for (i = 0; i < nVer; i++) {
				memcpy(tmp_hw_info, pBuf, HW_INFO_MAX_SIZE);
				pBuf += HW_INFO_MAX_SIZE;

				memcpy(tmp_sw_info, pBuf, SW_INFO_MAX_SIZE);
				pBuf += SW_INFO_MAX_SIZE;

				memcpy(tmp_vendor_info, pBuf, VENDOR_INFO_MAX_SIZE);
				tmp_vendor_info[VENDOR_INFO_MAX_SIZE-1] = '\0';
				pBuf += VENDOR_INFO_MAX_SIZE-1;

				memcpy(tmp_process_info, pBuf, PROCESS_INFO_MAX_SIZE);
				tmp_process_info[PROCESS_INFO_MAX_SIZE-1] = '\0';
				pBuf += PROCESS_INFO_MAX_SIZE;

				CAM_DBG(CAM_EEPROM, "[temp %d/%d] : %s %s %s %s",
					i, nVer, tmp_hw_info, tmp_sw_info,
					tmp_vendor_info, tmp_process_info);

				if (strcmp(hw_phone_info, tmp_hw_info) == 0) {
					memcpy(sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_DBG(CAM_EEPROM, "rear [%d] : %s %s %s %s",
						i, hw_phone_info, sw_phone_info,
						vendor_phone_info, process_phone_info);
				} else if (strcmp(front_hw_phone_info, tmp_hw_info) == 0) {
					memcpy(front_sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(front_vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(front_process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_DBG(CAM_EEPROM, "front [%d] : %s %s %s %s",
						i, front_hw_phone_info, front_sw_phone_info,
						front_vendor_phone_info, front_process_phone_info);
				} else {
					CAM_DBG(CAM_EEPROM, "invalid hwinfo: %s", tmp_hw_info);
				}
			}
			memset(read_buffer, 0x00, e_ctrl->cal_data.num_data);
		} else {
			CAM_ERR(CAM_EEPROM, "Invalid direction");
			rc = -EINVAL;
		}
	}
	return rc;
}

int cam_eeprom_retry(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_packet *csl_packet, struct cam_eeprom_soc_private *soc_private)
{
	int rc = 0;
	int32_t retryCount = 3;
	int32_t isRetry = 0;
	
retry:
	memset(e_ctrl->cal_data.mapdata, 0xFF, e_ctrl->cal_data.num_data);

	rc = cam_eeprom_power_up(e_ctrl,
		&soc_private->power_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed rc %d", rc);
		goto memdata_free;
	}

	e_ctrl->cam_eeprom_state = CAM_EEPROM_CONFIG;

	{
		int i;
		int normal_crc_value = 0;

		if (e_ctrl->eeprom_device_type == MSM_CAMERA_SPI_DEVICE) {
			rc = cam_eeprom_match_id(e_ctrl);
			if (rc) {
				CAM_DBG(CAM_EEPROM, "eeprom not matching %d", rc);
			}
		}

		normal_crc_value = 0;
		for (i = 0; i < e_ctrl->cal_data.num_map>>1; i++)
			normal_crc_value |= (1 << i);

		CAMERA_NORMAL_CAL_CRC = normal_crc_value;
		CAM_DBG(CAM_EEPROM, "num_map = %d, CAMERA_NORMAL_CAL_CRC = 0x%X soc_info.index=%d(0x%X) cal_data.num_data=0x%x",
			e_ctrl->cal_data.num_map, CAMERA_NORMAL_CAL_CRC, e_ctrl->soc_info.index, soc_private->i2c_info.slave_addr, e_ctrl->cal_data.num_data);

#if defined(CONFIG_SAMSUNG_CAMERA_OTP)
		if ((e_ctrl->soc_info.is_otp_sensor == 1) &&
			(soc_private->i2c_info.slave_addr == 0x5A || 
			soc_private->i2c_info.slave_addr == 0xAC || 
			soc_private->i2c_info.slave_addr == 0x6C)){
			rc = cam_otp_read_memory(e_ctrl, &e_ctrl->cal_data);
		}
		else
#endif
		rc = cam_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
			hw_bigdata_i2c_from_eeprom(e_ctrl);
#endif
			isRetry = 1;
			goto power_down;
		}
		else {
			CAM_DBG(CAM_EEPROM, "read_eeprom_memory success!!!");
		}

		if (1 < e_ctrl->cal_data.num_map) {
#if defined(CONFIG_SAMSUNG_CAMERA_OTP)
			if ((e_ctrl->soc_info.is_otp_sensor == 1) &&
				(soc_private->i2c_info.slave_addr == 0x5A || 
				soc_private->i2c_info.slave_addr == 0xAC || 
				soc_private->i2c_info.slave_addr == 0x6C)){
				e_ctrl->is_supported = normal_crc_value;
			}
			else
#endif
			e_ctrl->is_supported = normal_crc_value;
			if (e_ctrl->is_supported != normal_crc_value) {
				e_ctrl->is_supported = CAMERA_NORMAL_CAL_CRC;
				CAM_WARN(CAM_EEPROM, "Any CRC values at F-ROM are not matched.");
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
				hw_bigdata_crc_from_eeprom(e_ctrl);
#endif
			}
			else {
				CAM_DBG(CAM_EEPROM, "All CRC values are matched.");
			}

			if (e_ctrl->soc_info.is_sensor_type == CAM_SENSOR_TYPE_IDX_IMX611 && e_ctrl->soc_info.index == CAM_EEPROM_IDX_DTOF)
			{
				rear2_tof_zero_distance_value1 = e_ctrl->cal_data.mapdata[REAR_DTOF_CAM_ZERO_DISTANCE_ADDR1];
				rear2_tof_zero_distance_value2 = e_ctrl->cal_data.mapdata[REAR_DTOF_CAM_ZERO_DISTANCE_ADDR2];
				eeprom_subRef_radialDepth = rear2_tof_zero_distance_value1 + 256*rear2_tof_zero_distance_value2;
				CAM_DBG(CAM_EEPROM, "rear2_tof_zero_distance_value1: %d, rear2_tof_zero_distance_value2: %d, eeprom_radialDepth :%d",
					rear2_tof_zero_distance_value1, rear2_tof_zero_distance_value2, eeprom_subRef_radialDepth);
			}

			rc = cam_eeprom_update_module_info(e_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "cam_eeprom_update_module_info failed");
				isRetry = 1;
				goto power_down;
			}
#ifdef CAM_EEPROM_DBG_DUMP
			rc = cam_eeprom_dump(e_ctrl->soc_info.index,
				e_ctrl->cal_data.mapdata, e_ctrl->cal_data.map[0].mem.addr, e_ctrl->cal_data.num_data);
#endif
		}
		else if (e_ctrl->cal_data.num_map == 1 &&
			e_ctrl->cal_data.num_data == FROM_REAR_HEADER_SIZE &&
			e_ctrl->eeprom_device_type == MSM_CAMERA_SPI_DEVICE) {
			// run this on eebin check
			rc = cam_eeprom_get_phone_ver(e_ctrl, csl_packet);
		}
		else
		{
			CAM_DBG(CAM_EEPROM, "No calmap read!. map: %d", e_ctrl->cal_data.num_map);
		}
	}
	return rc;

power_down:
	cam_eeprom_power_down(e_ctrl);
	
	if(isRetry == 1 && retryCount > 0) {
		retryCount--;
		goto retry;
	}
memdata_free:
	vfree(e_ctrl->cal_data.mapdata);
	return rc;
}
