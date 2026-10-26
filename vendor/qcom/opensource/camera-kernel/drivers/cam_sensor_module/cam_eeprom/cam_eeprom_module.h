/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _CAM_EEPROM_MODULE_H_
#define _CAM_EEPROM_MODULE_H_

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/cam_sensor.h>
#include "cam_eeprom_dev.h"
#include "cam_eeprom_core.h"

#define CONFIG_SAMSUNG_CAMERA_OTP
#define CAM_EEPROM_MODULE_UPDATE
#define CAM_EEPROM_DBG  1
//#define CAM_EEPROM_DBG_DUMP  1

#define OK 1
#define CRASH 0

#define SYSFS_FW_VER_SIZE                       40
#define SYSFS_MODULE_INFO_SIZE                  96
#define FROM_CAL_MAP_VERSION                    0x20
#define MODULE_VER_ON_PVR                       0x20
#define MODULE_VER_ON_SRA                       0x20
#define REAR_PAF_CAL_INFO_SIZE                  1024

#define FROM_MODULE_FW_INFO_SIZE                11
#define FROM_MTF_SIZE                           54
#define FROM_MODULE_ID_SIZE                     10

#define FROM_REAR_MTF_ADDR                      0x0160
#define FROM_REAR_MTF2_ADDR                     0x0196
#define FROM_REAR2_MTF_ADDR                     0x0160
#define FROM_REAR3_MTF_ADDR                     0x0160
#define FROM_REAR4_MTF_ADDR                     0x0160
#define FROM_REAR5_MTF_ADDR                     0x0160
#define FROM_REAR6_MTF_ADDR                     0x0160
#define FROM_REAR7_MTF_ADDR                     0x0160
#define FROM_REAR8_MTF_ADDR                     0x0160
#define FROM_REAR_TOF_MTF_ADDR                  0x0160
#define FROM_REAR_DTOF_MTF_ADDR                 0x0160
#define FROM_FRONT_MTF_ADDR                     0x0080
#define FROM_FRONT2_MTF_ADDR                    0x0080
#define FROM_FRONT3_MTF_ADDR                    0x0080
#define FROM_FRONT4_MTF_ADDR                    0x0080
#define FROM_FRONT5_MTF_ADDR                    0x0080

#define FRONT_FROM_CAL_MAP_VERSION              0x20
#define FRONT_MODULE_VER_ON_PVR                 0x20
#define FRONT_MODULE_VER_ON_SRA                 0x20

#define FROM_REAR_AF_CAL_SIZE                   10
#define FROM_SENSOR_ID_SIZE                     16

#define FROM_REAR_DUAL_CAL_SIZE                 2060
#define FROM_FRONT_DUAL_CAL_SIZE                1024

#define PAF_2PD_CAL_INFO_SIZE                   4096
#define PAF_SPARSEPD_CAL_INFO_SIZE              2048
#define PAF_CAL_ERR_CHECK_OFFSET                0x14

#define CAMERA_CAL_CRC_WIDE                     0x1FFF
#define FROM_REAR_HEADER_SIZE                   0x0200

#define HW_INFO_MAX_SIZE                        (6)
#define SW_INFO_MAX_SIZE                        (5)
#define VENDOR_INFO_MAX_SIZE                    (2)
#define PROCESS_INFO_MAX_SIZE                   (2)
#define PROJECT_CAL_TYPE_MAX_SIZE               (20)

#if defined(CONFIG_SEC_VST_PROJECT)
#define FROM_MODULE_ID_ADDR                     0x005E
#define FROM_MODULE_ID_SIZE                     10
#define FROM_REAR_SENSOR_ID_ADDR                0x0068

#define REAR_MODULE_FW_VERSION                  0x0030
#define REAR_CAM_MAP_VERSION_ADDR               0x0040
#define REAR_DLL_VERSION_ADDR                   0x0044

#define REAR2_MODULE_ID_ADDR                     0x005E
#define FROM_REAR2_SENSOR_ID_ADDR                0x0068
#define REAR2_MODULE_FW_VERSION                  0x0030
#define REAR2_CAM_MAP_VERSION_ADDR               0x0040
#define REAR2_DLL_VERSION_ADDR                   0x0044

#define REAR3_MODULE_ID_ADDR                     0x0A22
#define FROM_REAR3_SENSOR_ID_ADDR                0x0A2C
#define REAR3_MODULE_FW_VERSION                  0x0A05
#define REAR3_CAM_MAP_VERSION_ADDR               0x0A10
#define REAR3_DLL_VERSION_ADDR                   0x0044

#define REAR4_MODULE_ID_ADDR                     0x0A22
#define FROM_REAR4_SENSOR_ID_ADDR                0x0A2C
#define REAR4_MODULE_FW_VERSION                  0x0A05
#define REAR4_CAM_MAP_VERSION_ADDR               0x0A10
#define REAR4_DLL_VERSION_ADDR                   0x0044

#define REAR5_MODULE_ID_ADDR                     0x0A22
#define FROM_REAR5_SENSOR_ID_ADDR                0x0A2C
#define REAR5_MODULE_FW_VERSION                  0x0A05
#define REAR5_CAM_MAP_VERSION_ADDR               0x0A10
#define REAR5_DLL_VERSION_ADDR                   0x0044

#define REAR6_MODULE_ID_ADDR                     0x0A22
#define FROM_REAR6_SENSOR_ID_ADDR                0x0A2C
#define REAR6_MODULE_FW_VERSION                  0x0A05
#define REAR6_CAM_MAP_VERSION_ADDR               0x0A10
#define REAR6_DLL_VERSION_ADDR                   0x0044

#define REAR7_MODULE_ID_ADDR                     0x0A22
#define FROM_REAR7_SENSOR_ID_ADDR                0x0A2C
#define REAR7_MODULE_FW_VERSION                  0x0A05
#define REAR7_CAM_MAP_VERSION_ADDR               0x0A10
#define REAR7_DLL_VERSION_ADDR                   0x0044

#define REAR8_MODULE_ID_ADDR                     0x0A22
#define FROM_REAR8_SENSOR_ID_ADDR                0x0A2C
#define REAR8_MODULE_FW_VERSION                  0x0A05
#define REAR8_CAM_MAP_VERSION_ADDR               0x0A10
#define REAR8_DLL_VERSION_ADDR                   0x0044

#if defined(CONFIG_SAMSUNG_REAR_TOF)
#define REAR_TOF_FROM_CAL_MAP_VERSION            0x20
#define REAR_TOF_MODULE_VER_ON_PVR               0x20
#define REAR_TOF_MODULE_VER_ON_SRA               0x20
#define REAR_TOF_MODULE_ID_ADDR                  0x00AE
#define FROM_REAR_TOF_SENSOR_ID_ADDR             0x00B8
#define REAR_TOF_MODULE_FW_VERSION               0x0040
#define REAR_TOF_CAM_MAP_VERSION_ADDR            0x0050
#define REAR_TOF_DLL_VERSION_ADDR                0x0054

#define TOFCAL_START_ADDR                        0x0100
#define TOFCAL_END_ADDR                          0x18FC
#define TOFCAL_TOTAL_SIZE	                 (TOFCAL_END_ADDR - TOFCAL_START_ADDR + 1)
#define TOFCAL_SIZE                              (4096 - 1)
#define TOFCAL_EXTRA_SIZE                        (TOFCAL_TOTAL_SIZE - TOFCAL_SIZE)
#define TOFCAL_RESULT_ADDR                       0x00CA
#define TOF_DUAL_CAL_ADDR                        0x08E6
#define TOF_DUAL_CAL_END_ADDR                    0x0CE5
#define REAR_TOF_DUAL_CAL_SIZE                   (TOF_DUAL_CAL_END_ADDR - TOF_DUAL_CAL_ADDR + 1)
#define REAR_TOF_VALIDATION_500                  0x1930
#define REAR_TOF_VALIDATION_300                  0x196C

#define REAR_DTOF_FROM_CAL_MAP_VERSION           0x20
#define REAR_DTOF_MODULE_VER_ON_PVR              0x20
#define REAR_DTOF_MODULE_VER_ON_SRA              0x20
#define REAR_DTOF_MODULE_ID_ADDR                 0x00AE
#define FROM_REAR_DTOF_SENSOR_ID_ADDR            0x00B8
#define REAR_DTOF_MODULE_FW_VERSION              0x0040
#define REAR_DTOF_CAM_MAP_VERSION_ADDR           0x0050
#define REAR_DTOF_DLL_VERSION_ADDR               0x0054

#define DTOFCAL_START_ADDR                       0x0100
#define DTOFCAL_END_ADDR                         0x18FC
#define DTOFCAL_TOTAL_SIZE	                 (DTOFCAL_END_ADDR - DTOFCAL_START_ADDR + 1)
#define DTOFCAL_SIZE                             (4096 - 1)
#define DTOFCAL_EXTRA_SIZE                       (DTOFCAL_TOTAL_SIZE - DTOFCAL_SIZE)
#define DTOFCAL_RESULT_ADDR                      0x00CC
#define DTOF_DUAL_CAL_ADDR                       0x08E6
#define DTOF_DUAL_CAL_END_ADDR                   0x0CE5
#define REAR_DTOF_DUAL_CAL_SIZE                  (DTOF_DUAL_CAL_END_ADDR - DTOF_DUAL_CAL_ADDR + 1)
#define REAR_DTOF_VALIDATION_500                 0x1930
#define REAR_DTOF_VALIDATION_300                 0x196C

#endif

#define FROM_FRONT_MODULE_ID_ADDR                0x0A22
#define FROM_FRONT_MODULE_ID_SIZE                10
#define FROM_FRONT_SENSOR_ID_ADDR                0x0A2C
#define FRONT_MODULE_FW_VERSION                  0x0A05
#define FRONT_CAM_MAP_VERSION_ADDR               0x0A10
#define FRONT_DLL_VERSION_ADDR                   0x0044

#define FROM_FRONT2_MODULE_ID_ADDR                0x0A22
#define FROM_FRONT2_MODULE_ID_SIZE                10
#define FROM_FRONT2_SENSOR_ID_ADDR                0x0A2C
#define FRONT2_MODULE_FW_VERSION                  0x0A05
#define FRONT2_CAM_MAP_VERSION_ADDR               0x0A10
#define FRONT2_DLL_VERSION_ADDR                   0x0044

#define FROM_FRONT3_MODULE_ID_ADDR                0x0A22
#define FROM_FRONT3_MODULE_ID_SIZE                10
#define FROM_FRONT3_SENSOR_ID_ADDR                0x0A2C
#define FRONT3_FT_MODULE_FW_VERSION               0x7071
#define FRONT3_MODULE_FW_VERSION                  0x0A05
#define FRONT3_CAM_MAP_VERSION_ADDR               0x0A10
#define FRONT3_DLL_VERSION_ADDR                   0x0044

#define FROM_FRONT4_MODULE_ID_ADDR                0x0A22
#define FROM_FRONT4_MODULE_ID_SIZE                10
#define FROM_FRONT4_SENSOR_ID_ADDR                0x0A2C
#define FRONT4_FT_MODULE_FW_VERSION               0x7071
#define FRONT4_MODULE_FW_VERSION                  0x0A05
#define FRONT4_CAM_MAP_VERSION_ADDR               0x0A10
#define FRONT4_DLL_VERSION_ADDR                   0x0044

#define FROM_FRONT5_MODULE_ID_ADDR                0x0A22
#define FROM_FRONT5_MODULE_ID_SIZE                10
#define FROM_FRONT5_SENSOR_ID_ADDR                0x0A2C
#define FRONT5_MODULE_FW_VERSION                  0x7071
#define FRONT5_CAM_MAP_VERSION_ADDR               0x0A10
#define FRONT5_DLL_VERSION_ADDR                   0x0044

#define HW_INFO                                 ("M12QS")
#define SW_INFO                                 ("QCR6")
#define VENDOR_INFO                             ("S")
#define PROCESS_INFO                            ("A")
#define CRITERION_REV                           (0)

#define HW_INFO_VST2                            ("M12QS")
#define SW_INFO_VST2                            ("QCR6")
#define VENDOR_INFO_VST2                        ("S")
#define PROCESS_INFO_VST2                       ("A")
#define CRITERION_REV_VST2                      (0)

#define HW_INFO_HET1                            ("A04QL")
#define SW_INFO_HET1                            ("QDH0")
#define VENDOR_INFO_HET1                        ("S")
#define PROCESS_INFO_HET1                       ("A")
#define CRITERION_REV_HET1                      (0)

#define HW_INFO_HET2                            ("A04QL")
#define SW_INFO_HET2                            ("QDH0")
#define VENDOR_INFO_HET2                        ("S")
#define PROCESS_INFO_HET2                       ("A")
#define CRITERION_REV_HET2                      (0)

#define HW_INFO_HET3                            ("A04QL")
#define SW_INFO_HET3                            ("QDH0")
#define VENDOR_INFO_HET3                        ("S")
#define PROCESS_INFO_HET3                       ("A")
#define CRITERION_REV_HET3                      (0)

#define HW_INFO_HET4                            ("A04QL")
#define SW_INFO_HET4                            ("QDH0")
#define VENDOR_INFO_HET4                        ("S")
#define PROCESS_INFO_HET4                       ("A")
#define CRITERION_REV_HET4                      (0)

#define HW_INFO_HET5                            ("B04QL")
#define SW_INFO_HET5                            ("QDH0")
#define VENDOR_INFO_HET5                        ("S")
#define PROCESS_INFO_HET5                       ("A")
#define CRITERION_REV_HET5                      (0)

#define HW_INFO_HET6                            ("B04QL")
#define SW_INFO_HET6                            ("QDH0")
#define VENDOR_INFO_HET6                        ("S")
#define PROCESS_INFO_HET6                       ("A")
#define CRITERION_REV_HET6                      (0)

#if defined(CONFIG_SAMSUNG_REAR_TOF)
#define HW_INFO_TOF                            ("ET2QL")
#define SW_INFO_TOF                            ("QCT1")
#define VENDOR_INFO_TOF                        ("N")
#define PROCESS_INFO_TOF                       ("A")
#define CRITERION_REV_TOF                      (0)

#define HW_INFO_DTOF                           ("GT0QS")
#define SW_INFO_DTOF                           ("RCL0")
#define VENDOR_INFO_DTOF                       ("Z")
#define PROCESS_INFO_DTOF                      ("B")
#define CRITERION_REV_DTOF                     (0)
#endif

#define FRONT_HW_INFO                           ("C04QL")
#define FRONT_SW_INFO                           ("QDE0")
#define FRONT_VENDOR_INFO                       ("S")
#define FRONT_PROCESS_INFO                      ("A")
#define CRITERION_REV_FRONT                      (0)

#define FRONT2_HW_INFO                           ("E04QL")
#define FRONT2_SW_INFO                           ("QDE0")
#define FRONT2_VENDOR_INFO                       ("S")
#define FRONT2_PROCESS_INFO                      ("A")
#define CRITERION_REV_FRONT2                      (0)

#define FRONT3_HW_INFO                           ("D04QL")
#define FRONT3_SW_INFO                           ("QDE0")
#define FRONT3_VENDOR_INFO                       ("S")
#define FRONT3_PROCESS_INFO                      ("A")
#define CRITERION_REV_FRONT3                      (0)

#define FRONT4_HW_INFO                           ("F04QL")
#define FRONT4_SW_INFO                           ("QDE0")
#define FRONT4_VENDOR_INFO                       ("S")
#define FRONT4_PROCESS_INFO                      ("A")
#define CRITERION_REV_FRONT4                      (0)

#define FRONT5_HW_INFO                           ("F01QO")
#define FRONT5_SW_INFO                           ("QD")
#define FRONT5_VENDOR_INFO                       ("")
#define FRONT5_PROCESS_INFO                      ("")
#define CRITERION_REV_FRONT5                      (0)

#endif

#define AF_CAL_NEAR_OFFSET_FROM_AF                  0x0010
#define AF_CAL_FAR_OFFSET_FROM_AF                   0x0004
#define AF_CAL_M1_OFFSET_FROM_AF                    0x0008
#define AF_CAL_M2_OFFSET_FROM_AF                    0x000C

#define PAF_OFFSET_CAL_ERR_CHECK                    (0x0014)
#define PAF_MID_SIZE                                936
#define PAF_MID_OFFSET                              (0x0730)

#define PAF_FAR_SIZE                                234
#define PAF_FAR_OFFSET                              (0x0CD0)

#define MAX_AF_CAL_STR_SIZE                         256

#define REAR_DTOF_CAM_ZERO_DISTANCE_ADDR1           0x1E85
#define REAR_DTOF_CAM_ZERO_DISTANCE_ADDR2           0x1E86
extern int rear2_tof_zero_distance_value1, rear2_tof_zero_distance_value2, eeprom_subRef_radialDepth;

//extern int rear_af_cal[FROM_REAR_AF_CAL_SIZE + 1];
extern char rear_af_cal_str[MAX_AF_CAL_STR_SIZE];
extern char rear_sensor_id[FROM_SENSOR_ID_SIZE + 1];

extern char rear_paf_cal_data_far[PAF_2PD_CAL_INFO_SIZE];
extern char rear_paf_cal_data_mid[PAF_2PD_CAL_INFO_SIZE];
extern uint32_t paf_err_data_result;
extern char rear_f2_paf_cal_data_far[PAF_2PD_CAL_INFO_SIZE];
extern char rear_f2_paf_cal_data_mid[PAF_2PD_CAL_INFO_SIZE];
extern uint32_t f2_paf_err_data_result;

extern char module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear_mtf_exif[FROM_MTF_SIZE + 1];
extern char rear_mtf2_exif[FROM_MTF_SIZE + 1];

extern char cam_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam_fw_user_ver[SYSFS_FW_VER_SIZE];
extern char cal_crc[SYSFS_FW_VER_SIZE];

extern char rear2_af_cal_str[MAX_AF_CAL_STR_SIZE];
extern uint32_t rear2_paf_err_data_result;
extern uint8_t rear2_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1];

extern char module2_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear2_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear2_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear2_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam2_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam2_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam2_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam2_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char module3_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear3_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear3_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear3_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam3_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam3_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam3_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam3_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char module4_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear4_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear4_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear4_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam4_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam4_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam4_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam4_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char module5_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear5_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear5_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear5_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam5_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam5_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam5_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam5_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char module6_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear6_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear6_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear6_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam6_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam6_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam6_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam6_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char module7_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear7_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear7_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear7_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam7_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam7_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam7_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam7_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char module8_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear8_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear8_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear8_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam8_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam8_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam8_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam8_fw_user_ver[SYSFS_FW_VER_SIZE];

#if defined(CONFIG_SAMSUNG_REAR_TOF)
extern char rear_tof_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear_tof_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear_tof_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear_tof_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam_tof_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam_tof_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam_tof_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam_tof_fw_user_ver[SYSFS_FW_VER_SIZE];

extern int rear_tof_validation_500;
extern int rear_tof_validation_300;
extern uint8_t rear_tof_cal[TOFCAL_SIZE + 1];
extern uint8_t rear_tof_cal_extra[TOFCAL_EXTRA_SIZE + 1];
extern uint8_t rear_tof_cal_result;
extern uint8_t rear_tof_dual_cal[REAR_TOF_DUAL_CAL_SIZE + 1];

extern char rear_dtof_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char rear_dtof_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char rear_dtof_module_id[FROM_MODULE_ID_SIZE + 1];
extern char rear_dtof_mtf_exif[FROM_MTF_SIZE + 1];
extern char cam_dtof_fw_ver[SYSFS_FW_VER_SIZE];
extern char cam_dtof_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char cam_dtof_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char cam_dtof_fw_user_ver[SYSFS_FW_VER_SIZE];

extern int rear_dtof_validation_500;
extern int rear_dtof_validation_300;
extern uint8_t rear_dtof_cal[DTOFCAL_SIZE + 1];
extern uint8_t rear_dtof_cal_extra[DTOFCAL_EXTRA_SIZE + 1];
extern uint8_t rear_dtof_cal_result;
extern uint8_t rear_dtof_dual_cal[REAR_DTOF_DUAL_CAL_SIZE + 1];
#endif

extern char front_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char front_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char front_module_id[FROM_MODULE_ID_SIZE + 1];
extern char front_mtf_exif[FROM_MTF_SIZE + 1];
extern char front_cam_fw_ver[SYSFS_FW_VER_SIZE];
extern char front_cam_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char front_cam_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char front_cam_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char front2_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char front2_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char front2_module_id[FROM_MODULE_ID_SIZE + 1];
extern char front2_mtf_exif[FROM_MTF_SIZE + 1];
extern char front2_cam_fw_ver[SYSFS_FW_VER_SIZE];
extern char front2_cam_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char front2_cam_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char front2_cam_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char front3_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char front3_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char front3_module_id[FROM_MODULE_ID_SIZE + 1];
extern char front3_mtf_exif[FROM_MTF_SIZE + 1];
extern char front3_cam_fw_ver[SYSFS_FW_VER_SIZE];
extern char front3_cam_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char front3_cam_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char front3_cam_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char front4_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char front4_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char front4_module_id[FROM_MODULE_ID_SIZE + 1];
extern char front4_mtf_exif[FROM_MTF_SIZE + 1];
extern char front4_cam_fw_ver[SYSFS_FW_VER_SIZE];
extern char front4_cam_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char front4_cam_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char front4_cam_fw_user_ver[SYSFS_FW_VER_SIZE];

extern char front5_module_info[SYSFS_MODULE_INFO_SIZE + 1];
extern char front5_sensor_id[FROM_SENSOR_ID_SIZE + 1];
extern char front5_module_id[FROM_MODULE_ID_SIZE + 1];
extern char front5_mtf_exif[FROM_MTF_SIZE + 1];
extern char front5_cam_fw_ver[SYSFS_FW_VER_SIZE];
extern char front5_cam_fw_full_ver[SYSFS_FW_VER_SIZE];
extern char front5_cam_fw_factory_ver[SYSFS_FW_VER_SIZE];
extern char front5_cam_fw_user_ver[SYSFS_FW_VER_SIZE];

#define TEW_SIZE 100 //TOF_MAX_EEPROM_WRITE_SIZE
extern int tofEepromWriteAddr[TEW_SIZE];
extern int tofEepromWriteData[TEW_SIZE];
extern int tofEepromWriteAck;
extern int tofEepromLastWriteSize;

typedef struct _cam_eeprom_dual_tilt_t {
	int x;
	int y;
	int z;
	int sx;
	int sy;
	int range;
	int max_err;
	int avg_err;
	int dll_ver;
	char project_cal_type[PROJECT_CAL_TYPE_MAX_SIZE];
} DualTilt_t;

extern DualTilt_t rear2_dual;

extern uint32_t rear3_paf_err_data_result;

extern char front_af_cal_str[MAX_AF_CAL_STR_SIZE];
extern uint32_t front_paf_err_data_result;

/* phone fw info */
extern uint32_t CAMERA_NORMAL_CAL_CRC;

typedef enum{
	EEPROM_FW_VER = 1,
	PHONE_FW_VER,
	LOAD_FW_VER
} cam_eeprom_fw_version_idx;

typedef enum{
	CAM_EEPROM_IDX_VST_LEFT = 0,
	CAM_EEPROM_IDX_VST_RIGHT = 1,
	CAM_EEPROM_IDX_HETFRONT_LEFT = 2,
	CAM_EEPROM_IDX_HETFRONT_RIGHT = 3,
	CAM_EEPROM_IDX_HETDOWN_LEFT = 4,
	CAM_EEPROM_IDX_HETDOWN_RIGHT = 5,
	CAM_EEPROM_IDX_ET_LEFT = 6,
	CAM_EEPROM_IDX_ET_RIGHT = 7,
	CAM_EEPROM_IDX_FT_LEFT = 8,
	CAM_EEPROM_IDX_FT_RIGHT = 9,
	CAM_EEPROM_IDX_FT_MID = 10,
	CAM_EEPROM_IDX_BACK_TOF = 11,
	CAM_EEPROM_IDX_HET_MID_L = 2,
	CAM_EEPROM_IDX_HET_MID_R = 3,
	CAM_EEPROM_IDX_HET_BTM_L = 4,
	CAM_EEPROM_IDX_HET_BTM_R = 5,
	CAM_EEPROM_IDX_HET_TOP_L = 6,
	CAM_EEPROM_IDX_HET_TOP_R = 7,
	CAM_EEPROM_IDX_ET_LD = 8,
	CAM_EEPROM_IDX_ET_RD = 9,
	CAM_EEPROM_IDX_ET_LU = 10,
	CAM_EEPROM_IDX_ET_RU = 11,
	CAM_EEPROM_IDX_DTOF = 12,
	CAM_EEPROM_IDX_ITOF = 13,
	CAM_EEPROM_IDX_ET_LD_FACTORY = 14,
	CAM_EEPROM_IDX_ET_RD_FACTORY = 15,
	CAM_EEPROM_IDX_ET_LU_FACTORY = 16,
	CAM_EEPROM_IDX_ET_RU_FACTORY = 17,
	CAM_EEPROM_IDX_MAX = 18
} cam_eeprom_idx_type;

typedef enum{
	CAM_SENSOR_TYPE_IDX_IMX564 = 1,
	CAM_SENSOR_TYPE_IDX_S5K931 = 2,
	CAM_SENSOR_TYPE_IDX_S5K931_F = 3,
	CAM_SENSOR_TYPE_IDX_OVOG0TB = 4,
	CAM_SENSOR_TYPE_IDX_S5K63D = 5,
	CAM_SENSOR_TYPE_IDX_IMX611 = 6,
} cam_sensor_idx_type;

typedef enum{
	CAM_SLAVE_IDX_IMX564 = 0x66,
	CAM_SLAVE_IDX_S5K931 = 0x5A,
	CAM_SLAVE_IDX_OVOG0TB = 0x6C,
	CAM_SLAVE_IDX_S5K63D = 0x20,
	CAM_SLAVE_IDX_IMX611 = 0x20,
} cam_slave_idx_type;

int cam_eeprom_update_module_info(struct cam_eeprom_ctrl_t *e_ctrl);

void cam_eeprom_update_sysfs_fw_version(const char *update_fw_ver, cam_eeprom_fw_version_idx update_fw_index, cam_eeprom_idx_type idx, cam_sensor_idx_type id_type, uint32_t hw_version);

int32_t cam_eeprom_check_firmware_cal(uint32_t camera_cal_crc, uint8_t cal_map_version, uint32_t hw_version, cam_eeprom_idx_type idx, cam_sensor_idx_type id_type);

/**
 * cam_eeprom_verify_sum - verify crc32 checksum
 * @mem:			data buffer
 * @size:			size of data buffer
 * @sum:			expected checksum
 * @rev_endian:	compare reversed endian (0:little, 1:big)
 *
 * Returns 0 if checksum match, -EINVAL otherwise.
 */
int cam_eeprom_verify_sum(const char *mem, uint32_t size, uint32_t sum, uint32_t rev_endian);

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
uint32_t cam_eeprom_match_crc(struct cam_eeprom_memory_block_t *data, uint32_t subdev_id, uint32_t subdev_type, uint32_t hw_version);

#if defined(CONFIG_SAMSUNG_CAMERA_OTP)
/**
  * cam_otp_read_memory() - read map data into buffer
  * @e_ctrl:	otp control struct
  * @block:	block to be read
  *
  * This function iterates through blocks stored in block->map, reads each
  * region and concatenate them into the pre-allocated block->mapdata
  */
int cam_otp_read_memory(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_eeprom_memory_block_t *block);
#endif

#ifdef CAM_EEPROM_DBG_DUMP
int cam_eeprom_dump(uint32_t subdev_id, uint8_t *mapdata, uint32_t addr, uint32_t size);
#endif

/**
 * cam_eeprom_calc_calmap_size - Calculate cal array size based on the cal map
 * @e_ctrl:       ctrl structure
 *
 * Returns size of cal array
 */
int32_t cam_eeprom_calc_calmap_size(struct cam_eeprom_ctrl_t *e_ctrl);

/**
 * cam_eeprom_get_phone_ver - parse the userspace IO config and
 *                            read phone version at eebindriver.bin
 * @e_ctrl:     ctrl structure
 * @csl_packet: csl packet received
 *
 * Returns success or failure
 */
int32_t cam_eeprom_get_phone_ver(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_packet *csl_packet);

int cam_eeprom_retry(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_packet *csl_packet, struct cam_eeprom_soc_private *soc_private);

#endif /*_CAM_EEPROM_MODULE_H_ */
