/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#ifndef S5K931_OTP_H
#define S5K931_OTP_H

#define SENSOR_HI1336C_OTP_CONTROL_REGISTER				0x0A00
#define SENSOR_HI1336C_OTP_ERROR_FLAG_REGISTER			        0x6f
#define SENSOR_HI1336C_OTP_PAGE_SELECT_REGISTER			        0xfe
#define SENSOR_S5K931_OTP_BANK_SELECT_REGISTER			        0x0A04
#define SENSOR_HI1336C_OTP_PAGE_START_REGISTER			        0x0404
#define SENSOR_HI1336C_OTP_PAGE_END_REGISTER	    	                0x08f3
/*============== Load setfile =====================*/


//static int cam_otp_s5k931_read_memory(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_eeprom_memory_block_t *block);

struct cam_sensor_i2c_reg_array load_sensor_s5k931_otp_setfile_reg[] = {
	//external CLK & PLL setting
	{0x0136, 0x1333},
	{0x0304, 0x0003},
	{0x0306, 0x00E1},
	{0x030E, 0x0003},
	{0x0310, 0x00B0},
	{0x0312, 0x0001},
};

struct cam_sensor_i2c_reg_array s5k931_otp_init_reg1[] = {
	//sensor stream on setting	
	{0x0100, 0x0100, 0},
};

struct cam_sensor_i2c_reg_array s5k931_otp_finish_reg1[] = {
	//make initial state
	{0x0A00, 0x0000, 0},
};


struct cam_sensor_i2c_reg_array load_sensor_hi1336c_otp_setfile_reg[] = {
	//sensor global setting
	{0x0808, 0x0000},
	{0x2000, 0x0021},

	//mode setting
	{0x3250, 0xa060},
	//sensor streaming on
	{0x0808, 0x0001},
};



struct cam_sensor_i2c_reg_array hi1336c_otp_init_reg1[] = {
	{0x0B02, 0x01, 0},
	{0x0809, 0x00, 0},
	{0x0B00, 0x00, 0},
};

struct cam_sensor_i2c_reg_array hi1336c_otp_init_reg2[] = {
	{0x0260, 0x10, 0},
	{0x0809, 0x01, 0},
	{0x0B00, 0x01, 0},
};

struct cam_sensor_i2c_reg_array hi1336c_otp_finish_reg1[] = {
	{0x0809, 0x00, 0},
	{0x0B00, 0x00, 0},
};

struct cam_sensor_i2c_reg_array hi1336c_otp_finish_reg2[] = {
	{0x0260, 0x00, 0},
	{0x0809, 0x01, 0},
	{0x0B00, 0x01, 0},
};

#endif /* S5K931_OTP_H */

