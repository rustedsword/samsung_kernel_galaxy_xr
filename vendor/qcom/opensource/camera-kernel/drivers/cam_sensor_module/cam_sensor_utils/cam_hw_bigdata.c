// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_hw_bigdata.h"
#include "cam_sensor_soc.h"

#define REAR_OIS_X_Y_ERR_REG  0x0600
#define REAR3_OIS_X_Y_ERR_REG 0x1800
#define REAR4_OIS_X_Y_ERR_REG 0x6000

static struct hw_param_collector hwparam_collector;
static struct hw_param_collector hwparam_collector_static;

char hwparam_str[MAX_HW_PARAM_ID][MAX_HW_PARAM_INFO][MAX_HW_PARAM_STR_LEN] =
		{{ "CAMIR_ID", "I2CR_AF", "I2CR_OIS", "I2CR_SEN", "MIPIR_SEN", "MIPIR_INFO", "I2CR_EEP", "CRCR_EEP", "CAMR_CNT", "WIFIR_INFO", "AFR_FAIL", "AFR_INFO"},
		 { "CAMIR2_ID", "I2CR2_AF", "I2CR2_OIS", "I2CR2_SEN", "MIPIR2_SEN", "MIPIR2_INFO", "I2CR2_EEP", "CRCR2_EEP", "CAMR2_CNT", "WIFIR2_INFO", "AFR2_FAIL", "AFR2_INFO"},
		 { "CAMIR3_ID", "I2CR3_AF", "I2CR3_OIS", "I2CR3_SEN", "MIPIR3_SEN", "MIPIR3_INFO", "I2CR3_EEP", "CRCR3_EEP", "CAMR3_CNT", "WIFIR3_INFO", "AFR3_FAIL", "AFR3_INFO"},
		 { "CAMIR4_ID", "I2CR4_AF", "I2CR4_OIS", "I2CR4_SEN", "MIPIR4_SEN", "MIPIR4_INFO", "I2CR4_EEP", "CRCR4_EEP", "CAMR4_CNT", "WIFIR4_INFO", "AFR4_FAIL", "AFR4_INFO"},
		 { "CAMIR5_ID", "I2CR5_AF", "I2CR5_OIS", "I2CR5_SEN", "MIPIR5_SEN", "MIPIR5_INFO", "I2CR5_EEP", "CRCR5_EEP", "CAMR5_CNT", "WIFIR5_INFO", "AFR5_FAIL", "AFR5_INFO"},
		 { "CAMIR6_ID", "I2CR6_AF", "I2CR6_OIS", "I2CR6_SEN", "MIPIR6_SEN", "MIPIR6_INFO", "I2CR6_EEP", "CRCR6_EEP", "CAMR6_CNT", "WIFIR6_INFO", "AFR6_FAIL", "AFR6_INFO"},
		 { "CAMIR7_ID", "I2CR7_AF", "I2CR7_OIS", "I2CR7_SEN", "MIPIR7_SEN", "MIPIR7_INFO", "I2CR7_EEP", "CRCR7_EEP", "CAMR7_CNT", "WIFIR7_INFO", "AFR7_FAIL", "AFR7_INFO"},
		 { "CAMIR8_ID", "I2CR8_AF", "I2CR8_OIS", "I2CR8_SEN", "MIPIR8_SEN", "MIPIR8_INFO", "I2CR8_EEP", "CRCR8_EEP", "CAMR8_CNT", "WIFIR8_INFO", "AFR8_FAIL", "AFR8_INFO"},
		 { "CAMIF_ID", "I2CF_AF", "I2CF_OIS", "I2CF_SEN", "MIPIF_SEN", "MIPIF_INFO", "I2CF_EEP", "CRCF_EEP", "CAMF_CNT", "WIFIF_INFO", "AFF_FAIL", "AFF_INFO"},
		 { "CAMIF2_ID", "I2CF2_AF", "I2CF2_OIS", "I2CF2_SEN", "MIPIF2_SEN", "MIPIF2_INFO", "I2CF2_EEP", "CRCF2_EEP", "CAMF2_CNT", "WIFIF2_INFO", "AFF2_FAIL", "AFF2_INFO"},
		 { "CAMIF3_ID", "I2CF3_AF", "I2CF3_OIS", "I2CF3_SEN", "MIPIF3_SEN", "MIPIF3_INFO", "I2CF3_EEP", "CRCF3_EEP", "CAMF3_CNT", "WIFIF3_INFO", "AFF3_FAIL", "AFF3_INFO"},
		 { "CAMIF_4ID", "I2CF4_AF", "I2CF4_OIS", "I2CF4_SEN", "MIPIF4_SEN", "MIPIF4_INFO", "I2CF4_EEP", "CRCF4_EEP", "CAMF4_CNT", "WIFIF4_INFO", "AFF4_FAIL", "AFF4_INFO"},
		 { "CAMIR_TOF_ID", "I2CR_TOF_AF", "I2CR_TOF_OIS", "I2CR_TOF_SEN", "MIPIR_TOF_SEN", "MIPIR_TOF_INFO", "I2CR_TOF_EEP", "CRCR_TOF_EEP", "CAMR_TOF_CNT", "WIFIR_TOF_INFO", "AFR_TOF_FAIL", "AFR_TOF_INFO"},
		 { "CAMIR2_TOF_ID", "I2CR2_TOF_AF", "I2CR2_TOF_OIS", "I2CR2_TOF_SEN", "MIPIR2_TOF_SEN", "MIPIR2_TOF_INFO", "I2CR2_TOF_EEP", "CRCR2_TOF_EEP", "CAMR2_TOF_CNT", "WIFIR2_TOF_INFO", "AFR2_TOF_FAIL", "AFR2_TOF_INFO"},};

int hw_bigdata_get_hw_param_id(uint32_t camera_id)
{
	uint32_t hw_param_id = MAX_HW_PARAM_ID;

	switch (camera_id) {
	case SEC_VST_L:
		hw_param_id = HW_PARAM_REAR;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear");
		break;
	case SEC_VST_R:
		hw_param_id = HW_PARAM_REAR2;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear2");
		break;
	case SEC_HET_ML:
		hw_param_id = HW_PARAM_REAR3;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear3");
		break;
	case SEC_HET_MR:
		hw_param_id = HW_PARAM_REAR4;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear4");
		break;
	case SEC_HET_BL:
		hw_param_id = HW_PARAM_REAR5;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear5");
		break;
	case SEC_HET_BR:
		hw_param_id = HW_PARAM_REAR6;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear6");
		break;
	case SEC_HET_TL:
		hw_param_id = HW_PARAM_REAR7;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear7");
		break;
	case SEC_HET_TR:
		hw_param_id = HW_PARAM_REAR8;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear8");
		break;
	case SEC_ET_LD:
		hw_param_id = HW_PARAM_FRONT;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Front");
		break;
	case SEC_ET_RD:
		hw_param_id = HW_PARAM_FRONT2;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Front2");
		break;
	case SEC_ET_LU:
		hw_param_id = HW_PARAM_FRONT3;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Front3");
		break;
	case SEC_ET_RU:
		hw_param_id = HW_PARAM_FRONT4;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Front4");
		break;
	case SEC_TOF_I:
		hw_param_id = HW_PARAM_REAR_TOF;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear_TOF");
		break;
	case SEC_TOF_D:
		hw_param_id = HW_PARAM_REAR2_TOF;
		scnprintf(hwparam_collector.camera_info, sizeof(hwparam_collector.camera_info), "%s", "Rear2_TOF");
		break;

	default:
		break;
	}

	return hw_param_id;
}

int hw_bigdata_get_hw_param(struct cam_hw_param **hw_param, uint32_t hw_param_id)
{
	*hw_param = &hwparam_collector.hwparam[hw_param_id];
	return 0;
}

int hw_bigdata_get_hw_param_static(struct cam_hw_param **hw_param, uint32_t hw_param_id)
{
	*hw_param = &hwparam_collector_static.hwparam[hw_param_id];
	return 0;
}

void hw_bigdata_update_err_cnt(struct cam_hw_param *hw_param, uint32_t modules)
{
	hw_param->err_cnt[modules]++;
	hw_param->need_update_to_file = TRUE;
}

void hw_bigdata_init_mipi_param(struct cam_hw_param *hw_param)
{
	hw_param->mipi_chk = FALSE;
	hw_param->need_update_to_file = FALSE;
}

void hw_bigdata_init_mipi_param_sensor(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(s_ctrl->id);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		if (hw_param != NULL) {
			CAM_DBG(CAM_UTIL, "[%s][INIT] Init\n", hwparam_collector.camera_info);
			hw_bigdata_init_mipi_param(hw_param);
		}
	}
}

void hw_bigdata_deinit_mipi_param_sensor(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(s_ctrl->id);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		if (hw_param != NULL) {
			if (hw_param->need_update_to_file) {
				CAM_DBG(CAM_UTIL, "[%s][DEINIT] Update\n", hwparam_collector.camera_info);
				hw_bigdata_copy_err_cnt_to_file();
			}
			hw_bigdata_init_mipi_param(hw_param);
		}
	}
}

void hw_bigdata_i2c_from_sensor(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(s_ctrl->id);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		if (hw_param != NULL) {
			CAM_ERR(CAM_UTIL, "[HWB][%s][I2C]\n", hwparam_collector.camera_info);
			hw_bigdata_update_err_cnt(hw_param, I2C_SENSOR_ERROR);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
			hw_bigdata_get_rfinfo(hw_param);
#endif
		}
	}
}

void hw_bigdata_mipi_from_ife_csid_ver1(uint32_t hw_cam_position)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(hw_cam_position);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		if (hw_param != NULL && (hw_param->mipi_chk == FALSE)) {
			CAM_ERR(CAM_UTIL, "[HWB][%s][MIPI_S]\n", hwparam_collector.camera_info);
			hw_bigdata_update_err_cnt(hw_param, MIPI_SENSOR_ERROR);
			hw_param->mipi_chk = TRUE;
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
			hw_bigdata_get_rfinfo(hw_param);
#endif
		}
	}
}

void hw_bigdata_i2c_from_actuator(struct cam_actuator_ctrl_t *a_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(a_ctrl->soc_info.index);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		if (hw_param != NULL) {
			CAM_ERR(CAM_UTIL, "[HWB][%s][AF]\n", hwparam_collector.camera_info);
			hw_bigdata_update_err_cnt(hw_param, I2C_AF_ERROR);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
			hw_bigdata_get_rfinfo(hw_param);
#endif
		}
	}
}

void hw_bigdata_i2c_from_ois_status_reg(uint32_t hw_cam_position)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(hw_cam_position);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		if (hw_param != NULL) {
			CAM_ERR(CAM_UTIL, "[HWB][%s][OIS]\n", hwparam_collector.camera_info);
			hw_bigdata_update_err_cnt(hw_param, I2C_OIS_ERROR);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
			hw_bigdata_get_rfinfo(hw_param);
#endif
		}
	}
}

void hw_bigdata_i2c_from_ois_error_reg(uint32_t err_reg)
{
	struct cam_hw_param *hw_param = NULL;

	if ((err_reg & REAR_OIS_X_Y_ERR_REG) != 0) {
		if (!hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR)) {
			if (hw_param != NULL) {
				CAM_ERR(CAM_UTIL, "[HWB][R][OIS]\n");
				hw_bigdata_update_err_cnt(hw_param, I2C_OIS_ERROR);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
				hw_bigdata_get_rfinfo(hw_param);
#endif
			}
		}
	}
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	if ((err_reg & REAR3_OIS_X_Y_ERR_REG) != 0) {
		if (!hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR3)) {
			if (hw_param != NULL) {
				CAM_ERR(CAM_UTIL, "[HWB][R3][OIS]\n");
				hw_bigdata_update_err_cnt(hw_param, I2C_OIS_ERROR);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
				hw_bigdata_get_rfinfo(hw_param);
#endif

			}
		}
	}
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	if ((err_reg & REAR4_OIS_X_Y_ERR_REG) != 0) {
		if (!hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR4)) {
			if (hw_param != NULL) {
				CAM_ERR(CAM_UTIL, "[HWB][R4][OIS]\n");
				hw_bigdata_update_err_cnt(hw_param, I2C_OIS_ERROR);
				CAM_ERR(CAM_UTIL, "[HWB][R4][AF]\n");
				hw_param->err_cnt[I2C_AF_ERROR]++;
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
				hw_bigdata_get_rfinfo(hw_param);
#endif
			}
		}
	}
#endif
}
/*
int get_camera_id(int csiphy_num) {
	int cameraid[SEC_SENSOR_ID_MAX];


	for(int i = 0; i < SEC_SENSOR_ID_MAX; i++){
		if(csiphy_sharing[i] == csiphy_num)
			cameraid[i] = 1;
		else
			cameraid[i] = -1;
	}

	switch (csiphy_num) {
	case 4:
		cameraid = SEC_WIDE_SENSOR;
		break;

	case UW_CAM:
		cameraid = SEC_ULTRA_WIDE_SENSOR;
		break;
	case TELE1_CAM:
		cameraid = SEC_TELE_SENSOR;
		break;
	case TELE2_CAM:
		cameraid = SEC_TELE2_SENSOR;
		break;
	case FRONT_CAM:
		cameraid = SEC_FRONT_SENSOR;
		break;
	case COVER_CAM:
		cameraid = SEC_FRONT_TOP_SENSOR;
		break;
	case FRONT_AUX:
		cameraid = SEC_FRONT_AUX1_SENSOR;
		break;

	default:
		CAM_INFO(CAM_ISP, "[HWB] No cameraId found (csiphy %d)", csiphy_num);
		break;
	}

  return cameraid;
}
*/
void hw_bigdata_mipi_from_ife_csid_ver2(int csiphy_num)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;
	int i;
	CAM_DBG(CAM_UTIL, "[HWB]csiphy_num : %d \n", csiphy_num);
	for(i =0; i< SEC_SENSOR_ID_MAX; i++){

		if(csiphy_sharing[i] != csiphy_num)
			continue;
	
		hw_param_id = hw_bigdata_get_hw_param_id(i);
		if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
		{
			if (hw_param != NULL && (hw_param->mipi_chk == FALSE)) {
				CAM_ERR(CAM_UTIL, "[HWB][%s][MIPI_S]\n", hwparam_collector.camera_info);
				hw_bigdata_update_err_cnt(hw_param, MIPI_SENSOR_ERROR);
				hw_param->mipi_chk = TRUE;
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
				hw_bigdata_get_rfinfo(hw_param);
#endif
			}
		}
	}
}


#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
void hw_bigdata_get_rfinfo(struct cam_hw_param *hw_param)
{
	struct cam_cp_noti_cell_infos cell_infos;

	get_rf_info(&cell_infos);
	CAM_DBG(CAM_UTIL,
		"[RF_MIPI_DBG] rat : %d, band : %d, channel : %d",
		cell_infos.cell_list[0].rat, cell_infos.cell_list[0].band, cell_infos.cell_list[0].channel);
	hw_param->rf_rat = cell_infos.cell_list[0].rat;
	hw_param->rf_band = cell_infos.cell_list[0].band;
	hw_param->rf_channel = cell_infos.cell_list[0].channel;
}
#endif

void hw_bigdata_i2c_from_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	struct cam_hw_param *hw_param_eeprom = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(e_ctrl->soc_info.index);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id) &&
		!hw_bigdata_get_hw_param_static(&hw_param_eeprom, hw_param_id))
	{
		if (hw_param != NULL) {
			CAM_INFO(CAM_UTIL, "[HWB][%s][EEP]\n", hwparam_collector.camera_info);
			hw_bigdata_update_err_cnt(hw_param, I2C_EEPROM_ERROR);
			hw_param_eeprom->eeprom_i2c_chk = 1;
		}
	}
}

void hw_bigdata_crc_from_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	struct cam_hw_param *hw_param_eeprom = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(e_ctrl->soc_info.index);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id) &&
		!hw_bigdata_get_hw_param_static(&hw_param_eeprom, hw_param_id))
	{
		if (hw_param != NULL) {
			CAM_INFO(CAM_UTIL, "[HWB][%s][EEP]\n", hwparam_collector.camera_info);
			hw_bigdata_update_err_cnt(hw_param, CRC_EEPROM_ERROR);
			hw_param_eeprom->eeprom_crc_chk = 1;
		}
	}
}

void hw_bigdata_init_all_cnt(void)
{
	CAM_INFO(CAM_UTIL, "All_Init_Cnt\n");
	memset(&hwparam_collector, 0, sizeof(struct hw_param_collector));
}

void hw_bigdata_init_err_cnt_file(struct cam_hw_param *hw_param)
{
	if (hw_param != NULL) {
		CAM_INFO(CAM_UTIL, "Init_Cnt\n");

		memset(hw_param, 0, sizeof(struct cam_hw_param));
		hw_bigdata_copy_err_cnt_to_file();
	} else {
		CAM_INFO(CAM_UTIL, "NULL\n");
	}
}

void hw_bigdata_copy_err_cnt_to_file(void)
{
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nwrite = 0;
	int old_mask = 0;

	CAM_INFO(CAM_UTIL, "To_F\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	old_mask = sys_umask(0);

	fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
	if (IS_ERR_OR_NULL(fp)) {
		CAM_INFO(CAM_UTIL, "[HWB][To_F]\n");
		sys_umask(old_mask);
		set_fs(old_fs);
		return;
	}

	nwrite = vfs_write(fp, (char *)&hwparam_collector, sizeof(struct hw_param_collector), &fp->f_pos);

	filp_close(fp, NULL);
	fp = NULL;
	sys_umask(old_mask);
	set_fs(old_fs);
#endif
}

void hw_bigdata_copy_err_cnt_from_file(void)
{
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread = 0;
	int ret = 0;

	ret = hw_bigdata_file_exist(CAM_HW_ERR_CNT_FILE_PATH, HW_PARAMS_NOT_CREATED);
	if (ret == 1) {
		CAM_INFO(CAM_UTIL, "From_F\n");
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_RDONLY, 0660);
		if (IS_ERR_OR_NULL(fp)) {
			CAM_INFO(CAM_UTIL, "[HWB][From_F]\n");
			set_fs(old_fs);
			return;
		}

		nread = vfs_read(fp, (char *)&hwparam_collector, sizeof(struct hw_param_collector), &fp->f_pos);

		filp_close(fp, NULL);
		fp = NULL;
		set_fs(old_fs);
	} else {
		CAM_INFO(CAM_UTIL, "NoEx_F\n");
	}
#endif
}

int hw_bigdata_file_exist(char *filename, hw_params_check_type chktype)
{
	int ret = 0;
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nwrite = 0;
	int old_mask = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (sys_access(filename, 0) == 0) {
		CAM_INFO(CAM_UTIL, "Ex_F\n");
		ret = 1;
	} else {
		switch (chktype) {
		case HW_PARAMS_CREATED:
			CAM_INFO(CAM_UTIL, "Ex_Cr\n");
			hw_bigdata_init_all_cnt();

			old_mask = sys_umask(0);

			fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
			if (IS_ERR_OR_NULL(fp)) {
				CAM_ERR(CAM_UTIL, "[HWB][Ex_F] ERROR\n");
				ret = 0;
			} else {
				nwrite = vfs_write(fp, (char *)&hwparam_collector, sizeof(struct hw_param_collector), &fp->f_pos);

				filp_close(fp, current->files);
				fp = NULL;
				ret = 2;
			}
			sys_umask(old_mask);
			break;

		case HW_PARAMS_NOT_CREATED:
			CAM_INFO(CAM_UTIL, "Ex_NoCr\n");
			ret = 0;
			break;

		default:
			CAM_INFO(CAM_UTIL, "Ex_Err\n");
			ret = 0;
			break;
		}
	}

	set_fs(old_fs);
#endif

	return ret;
}

uint32_t hw_bigdata_get_error_cnt(struct cam_hw_param *hw_param, uint32_t modules)
{
	return hw_param->err_cnt[modules];
}

void hw_bigdata_update_eeprom_error_cnt(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	struct cam_hw_param *hw_param_eeprom = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(s_ctrl->id);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id) &&
		!hw_bigdata_get_hw_param_static(&hw_param_eeprom, hw_param_id))
	{
		if (hw_param_eeprom->eeprom_i2c_chk) {
			hw_bigdata_update_err_cnt(hw_param, I2C_EEPROM_ERROR);
		}
		if (hw_param_eeprom->eeprom_crc_chk) {
			hw_bigdata_update_err_cnt(hw_param, CRC_EEPROM_ERROR);
		}
	}
}

void hw_bigdata_update_cam_entrance_cnt(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_hw_param *hw_param = NULL;
	uint32_t hw_param_id;

	hw_param_id = hw_bigdata_get_hw_param_id(s_ctrl->id);

	if (!hw_bigdata_get_hw_param(&hw_param, hw_param_id))
	{
		hw_param->cam_entrance_cnt++;
	}
}

void hw_bigdata_debug_info(void)
{
	uint32_t err_cnt[MAX_HW_PARAM_ID][MAX_HW_PARAM_ERROR];
	uint8_t  is_avail_cam[MAX_HW_PARAM_ID];
	int i;

	struct cam_hw_param *hw_param = NULL;

	for (i = 0; i < 1; i++) {
		hw_bigdata_get_hw_param(&hw_param, i);
		err_cnt[i][I2C_AF_ERROR] = hw_param->err_cnt[I2C_AF_ERROR];
		err_cnt[i][I2C_OIS_ERROR] = hw_param->err_cnt[I2C_OIS_ERROR];
		err_cnt[i][I2C_SENSOR_ERROR] = hw_param->err_cnt[I2C_SENSOR_ERROR];
		err_cnt[i][MIPI_SENSOR_ERROR] = hw_param->err_cnt[MIPI_SENSOR_ERROR];
		err_cnt[i][I2C_EEPROM_ERROR] = hw_param->err_cnt[I2C_EEPROM_ERROR];
		err_cnt[i][CRC_EEPROM_ERROR] = hw_param->err_cnt[CRC_EEPROM_ERROR];

		hw_bigdata_get_hw_param_static(&hw_param, i);
		is_avail_cam[i] = hw_param->cam_available;
	}

	CAM_INFO(CAM_UTIL, "[R1:%c,%x,%x,%x,%x,%x,%x]", 
		(is_avail_cam[HW_PARAM_REAR] ? 'Y':'N'), err_cnt[HW_PARAM_REAR][I2C_AF_ERROR], err_cnt[HW_PARAM_REAR][I2C_OIS_ERROR], err_cnt[HW_PARAM_REAR][I2C_SENSOR_ERROR], 
		err_cnt[HW_PARAM_REAR][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_REAR][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_REAR][CRC_EEPROM_ERROR]);
}

/*
void hw_bigdata_debug_info(void)
{
	uint32_t err_cnt[MAX_HW_PARAM_ID][MAX_HW_PARAM_ERROR];
	uint8_t  is_avail_cam[MAX_HW_PARAM_ID];
	int i;

	struct cam_hw_param *hw_param = NULL;

	for (i = 0; i < 6; i++) {
		hw_bigdata_get_hw_param(&hw_param, i);
		err_cnt[i][I2C_AF_ERROR] = hw_param->err_cnt[I2C_AF_ERROR];
		err_cnt[i][I2C_OIS_ERROR] = hw_param->err_cnt[I2C_OIS_ERROR];
		err_cnt[i][I2C_SENSOR_ERROR] = hw_param->err_cnt[I2C_SENSOR_ERROR];
		err_cnt[i][MIPI_SENSOR_ERROR] = hw_param->err_cnt[MIPI_SENSOR_ERROR];
		err_cnt[i][I2C_EEPROM_ERROR] = hw_param->err_cnt[I2C_EEPROM_ERROR];
		err_cnt[i][CRC_EEPROM_ERROR] = hw_param->err_cnt[CRC_EEPROM_ERROR];

		hw_bigdata_get_hw_param_static(&hw_param, i);
		is_avail_cam[i] = hw_param->cam_available;
	}

	CAM_INFO(CAM_UTIL, "[R1:%c,%x,%x,%x,%x,%x,%x] [R2:%c,%x,%x,%x,%x,%x,%x] [R3:%c,%x,%x,%x,%x,%x,%x] "
		"[R4:%c,%x,%x,%x,%x,%x,%x] [F1:%c,%x,%x,%x,%x,%x,%x] [F2:%c,%x,%x,%x,%x,%x,%x]", 
		(is_avail_cam[HW_PARAM_REAR] ? 'Y':'N'), err_cnt[HW_PARAM_REAR][I2C_AF_ERROR], err_cnt[HW_PARAM_REAR][I2C_OIS_ERROR], err_cnt[HW_PARAM_REAR][I2C_SENSOR_ERROR], 
		err_cnt[HW_PARAM_REAR][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_REAR][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_REAR][CRC_EEPROM_ERROR],
		(is_avail_cam[HW_PARAM_REAR2] ? 'Y':'N'), err_cnt[HW_PARAM_REAR2][I2C_AF_ERROR], err_cnt[HW_PARAM_REAR2][I2C_OIS_ERROR], err_cnt[HW_PARAM_REAR2][I2C_SENSOR_ERROR],
		err_cnt[HW_PARAM_REAR2][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_REAR2][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_REAR2][CRC_EEPROM_ERROR],
		(is_avail_cam[HW_PARAM_REAR3] ? 'Y':'N'), err_cnt[HW_PARAM_REAR3][I2C_AF_ERROR], err_cnt[HW_PARAM_REAR3][I2C_OIS_ERROR], err_cnt[HW_PARAM_REAR3][I2C_SENSOR_ERROR],
		err_cnt[HW_PARAM_REAR3][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_REAR3][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_REAR3][CRC_EEPROM_ERROR],
		(is_avail_cam[HW_PARAM_REAR4] ? 'Y':'N'), err_cnt[HW_PARAM_REAR4][I2C_AF_ERROR], err_cnt[HW_PARAM_REAR4][I2C_OIS_ERROR], err_cnt[HW_PARAM_REAR4][I2C_SENSOR_ERROR],
		err_cnt[HW_PARAM_REAR4][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_REAR4][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_REAR4][CRC_EEPROM_ERROR],
		(is_avail_cam[HW_PARAM_FRONT] ? 'Y':'N'), err_cnt[HW_PARAM_FRONT][I2C_AF_ERROR], err_cnt[HW_PARAM_FRONT][I2C_OIS_ERROR], err_cnt[HW_PARAM_FRONT][I2C_SENSOR_ERROR],
		err_cnt[HW_PARAM_FRONT][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_FRONT][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_FRONT][CRC_EEPROM_ERROR],
		(is_avail_cam[HW_PARAM_FRONT2] ? 'Y':'N'), err_cnt[HW_PARAM_FRONT2][I2C_AF_ERROR], err_cnt[HW_PARAM_FRONT2][I2C_OIS_ERROR], err_cnt[HW_PARAM_FRONT2][I2C_SENSOR_ERROR],
		err_cnt[HW_PARAM_FRONT2][MIPI_SENSOR_ERROR], err_cnt[HW_PARAM_FRONT2][I2C_EEPROM_ERROR], err_cnt[HW_PARAM_FRONT2][CRC_EEPROM_ERROR]);

}
*/