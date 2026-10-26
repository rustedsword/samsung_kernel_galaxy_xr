/*
 *  stk6d2x_fifo.c - Linux kernel modules for sensortek stk6d2x
 *  ambient light sensor (Algorithm)
 *
 *  Copyright (C) 2012~2018 Bk, sensortek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/input.h>
#include "stk6d2x.h"
#include "stk6d2x_sec.h"

#if IS_ENABLED(CONFIG_FLICKER_ANTI_ALIASING)
#include "flicker_anti_aliasing.h"
#else
extern void FFT(int32_t *data, uint size, bool debug);
#endif

#ifdef STK_FIFO_ENABLE
#ifdef SEC_FFT_FLICKER_1024

void remove_RS(int32_t*, uint, bool);
static int remove_rs_cnt = 0;
static uint32_t minClearGainLog = 0;

sample_rate_setting sr_table[] = {
	{   // 2000Hz
		.als_it = 0x0C,
		.als_it_ext = 0x1B,
		.IC_delay = 1308281, // /10 = us
		.buf_size = 1024,
		.sampling_time = 499985, //ns
		.sample_rate = 2000, //Hz
	},
	{   // 2048HZ
		.als_it = 0x0C,
		.als_it_ext = 0x0D,
		.IC_delay = 1308281, // /10 = us
		.buf_size = 1024,
		.sampling_time = 488319, //ns
		.sample_rate = 2048, //Hz
	},
	{   // 2098HZ
		.als_it = 0x0B,
		.als_it_ext = 0x1F,
		.IC_delay = 1308281, // /10 = us
		.buf_size = 1024,
		.sampling_time = 476652, //ns
		.sample_rate = 2098, //Hz
	},
};

uint32_t stk6d2x_als_get_max_gain_ratio(uint8_t pd_mode, uint8_t data_type)
{
	uint32_t value = STK6D2X_ALS_DGAIN_MULTI1024 * STK6D2X_ALS_AGAIN_MULTI4;
	if (data_type == STK6D2X_TYPE_ALS0 && (pd_mode == STK6D2X_ALS0_AGC_PDMODE0 || pd_mode == STK6D2X_ALS0_AGC_PDMODE1))
	{
		value *= STK6D2X_ALS_PD_REDUCE_MULTI4;
	}

	if (data_type == STK6D2X_TYPE_ALS1 && pd_mode == STK6D2X_ALS0_AGC_PDMODE0)
	{
		value *= STK6D2X_ALS_PD_REDUCE_MULTI2;
	}

	if (data_type == STK6D2X_TYPE_ALS1 && pd_mode == STK6D2X_ALS0_AGC_PDMODE1)
	{
		value *= STK6D2X_ALS_PD_REDUCE_MULTI3;
	}

	if (data_type == STK6D2X_TYPE_ALS2 && pd_mode == STK6D2X_ALS0_AGC_PDMODE0)
	{
		value *= STK6D2X_ALS_PD_REDUCE_MULTI2;
	}

	if (data_type == STK6D2X_TYPE_ALS2 && pd_mode == STK6D2X_ALS0_AGC_PDMODE1)
	{
		value *= STK6D2X_ALS_PD_REDUCE_MULTI1;
	}

	return value;
}

#if !IS_ENABLED(CONFIG_FLICKER_ANTI_ALIASING)
uint32_t get_single_freq(uint32_t* buf, bool als_info)
{
	int i = 0;

	static int log_cnt = 0;
	uint64_t average_thd = 0;
	uint64_t max_thd = 0;
	long long int ratio_thd = 0;

	uint resolution = DIV_ROUND_CLOSEST(1000000000, SAMPLING_TIME) / SEC_FFT_SIZE;
	uint min_freq = 70 / resolution;
	uint max_freq = 1000 / resolution;
	int imax = min_freq;

	uint16_t freq = 9999;

	uint64_t ratio_DC_max = 0;
	uint64_t ratio_max_avg = 0;

	// do fft
	FFT(buf, SEC_FFT_SIZE, als_info);

	// get average
	for (i = min_freq; i <= max_freq; ++i) {    // 70~1000
		average_thd += buf[i];   // 29+10=39
	}

	average_thd /= (max_freq - min_freq + 1); // 29

	remove_RS(buf, SEC_FFT_SIZE, als_info);

	// get peak index
	for (i = min_freq; i <= max_freq; ++i) {    // 70~1000
		// ALS_err("ccb_sw_bin4096_flicker_GetResult: buf[%d]=%10u\n", i, buf[i]);
		if (buf[i] > buf[imax]) {
			imax = i;
		}
	}

	max_thd = buf[imax];   // 29
	ratio_thd = (uint32_t)((average_thd << 10) / max_thd);   // 0+10-0, 0+10-29, 29+10-29

	ratio_DC_max = buf[0] / max_thd;
	ratio_max_avg = max_thd / average_thd;

	ALS_info("DEBUG_FLICKER DECISION_THD buf[%d]=%u, avg=%llu, max=%llu, DC=%u, DC/max=%llu, max/avg=%llu", imax, buf[imax], average_thd, max_thd, buf[0], ratio_DC_max, ratio_max_avg);

	// get valid threshold
	// thd = calc_thd(clear_average_fifo, clear_average, buf);
	ALS_info("DEBUG_FLICKER DC:%u\n", buf[0]);
	//ALS_info("DEBUG_FLICKER buf[%d]=%u, thd %lld", imax, buf[imax], thd);
	ALS_info("DEBUG_FLICKER buf[%d]=%u, avg/maxthd %lld\n", imax, buf[imax], ratio_thd);

	// valid peak check
	if (ratio_thd < FLICKER_AVGMAX_THD)
	{
		freq = imax * resolution;
		ALS_info("flicker_freq %d\n", freq);
	}
	else {
		freq = 0;
		ALS_info("flicker_freq is zero\n");
	}


	/*
	if (buf[imax] > thd) {
		freq = imax * resolution;
		ALS_info("flicker_freq %d", freq);
	}
	else {
		freq = 0;
		ALS_info("flicker_freq is zero");
	} */

	if (log_cnt++ > 10) {
		ALS_dbg("flicker_freq : %d ", freq);
		log_cnt = 0;
	} else {
		ALS_info("flicker_freq : %d ", freq);
	}

	return freq;
}
#endif

void SEC_fft_entry(struct stk6d2x_data *alps_data)
{
	stk6d2x_wrapper *stk_wrapper = container_of(alps_data, stk6d2x_wrapper, alps_data);
	int i = 0;
	static uint32_t clear_buffer[SEC_FFT_SIZE] = { 0 };
	static uint32_t uv_buffer[SEC_FFT_SIZE] = { 0 };
	static uint32_t ir_buffer[SEC_FFT_SIZE] = { 0 };
	static uint32_t clear_gain_log[SEC_FFT_SIZE] = { 0 };
	static uint32_t uv_gain_log[SEC_FFT_SIZE] = { 0 };
	static uint32_t ir_gain_log[SEC_FFT_SIZE] = { 0 };
	static bool     is_clear_sat[SEC_FFT_SIZE] = { false };
	static bool     is_uv_sat[SEC_FFT_SIZE] = { false };
	static bool     is_ir_sat[SEC_FFT_SIZE] = { false };
	static bool     is_ext_clk[SEC_FFT_SIZE] = { false };
	static uint32_t buf[SEC_FFT_SIZE] = { 0 };
	uint32_t extclkCnt = 0;

	uint32_t min_clear_gain_log = stk6d2x_als_get_max_gain_ratio(alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS0);
	uint32_t min_uv_gain_log = stk6d2x_als_get_max_gain_ratio(alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS1);
	uint32_t min_ir_gain_log = stk6d2x_als_get_max_gain_ratio(alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS2);

	uint32_t clear_average_fifo = 0;

	uint32_t clear_average = 0;
	uint32_t ir_average = 0;
	uint32_t uv_average = 0;
	int16_t clear_gain = 0;
	int16_t ir_gain = 0;
	int16_t uv_gain = 0;

	uint32_t freq = 0;

	/*
			***** read fifo *****
			*  ~~_buffer : raw fifo data(max 16bit!!!)
			*  ~~_gain_log : gain(log value)
			*               ex) gain 1   : 0
			*               ex) gain 2   : 1
			*               ex) gain 4   : 2
			*                     ...
			*               ex) gain 256 : 8
			*        This is an example, you can use it at an appropriate value according to STK68210
			*
	*/
	for (i = 0; i < SEC_FFT_SIZE; i++)
	{
		clear_buffer[i]    = alps_data->fifo_info.fft_buf[i];
		uv_buffer[i]       = alps_data->fifo_info.fft_uv_buf[i];
		ir_buffer[i]       = alps_data->fifo_info.fft_ir_buf[i];
		clear_gain_log[i]  = alps_data->fifo_info.fft_gain_clear[i];
		uv_gain_log[i]     = alps_data->fifo_info.fft_gain_uv[i];
		ir_gain_log[i]     = alps_data->fifo_info.fft_gain_ir[i];
		is_clear_sat[i]    = (alps_data->fifo_info.fft_xflag[i] & 0x1)? true : false;  //CL staturation flag
		is_uv_sat[i]       = (alps_data->fifo_info.fft_xflag[i] & 0x2)? true : false;  //UV staturation flag
		is_ir_sat[i]       = (alps_data->fifo_info.fft_xflag[i] & 0x4)? true : false;  //IR staturation flag
		is_ext_clk[i]      = (alps_data->fifo_info.fft_xflag[i] & 0x80)? true : false;  //External clock flag
		if (is_ext_clk[i])
			extclkCnt++;
	}

	ALS_info("DEBUG_FLICKER extclk count:%d, intclk count:%d", extclkCnt, SEC_FFT_SIZE - extclkCnt);

	// find minimum gain ratio
	for (i = 0; i < SEC_FFT_SIZE; i++)
	{
		if (clear_gain_log[i] < min_clear_gain_log)
			min_clear_gain_log = clear_gain_log[i];

		if (uv_gain_log[i] < min_uv_gain_log)
			min_uv_gain_log = uv_gain_log[i];

		if (ir_gain_log[i] < min_ir_gain_log)
			min_ir_gain_log = ir_gain_log[i];
	}

	// gain unification
	for (i = 0; i < SEC_FFT_SIZE; i++)
	{
		clear_average_fifo += clear_buffer[i];
		clear_buffer[i] = clear_buffer[i] >> (clear_gain_log[i] - min_clear_gain_log);
		ir_buffer[i]    = ir_buffer[i]    >> (ir_gain_log[i]    - min_ir_gain_log   );
		uv_buffer[i]    = uv_buffer[i]    >> (uv_gain_log[i]    - min_uv_gain_log   );
		buf[i] = clear_buffer[i]; // for FFT, max 16bit

		clear_average += clear_buffer[i]; // 16+11=27
		ir_average    += ir_buffer[i];
		uv_average    += uv_buffer[i];
	}

	// cal average
	clear_average_fifo = clear_average_fifo / SEC_FFT_SIZE;
	clear_average = clear_average / SEC_FFT_SIZE;
	ir_average = ir_average / SEC_FFT_SIZE;
	uv_average = uv_average / SEC_FFT_SIZE;

	clear_average = (clear_average << FLICKER_AVG_SHIFT) >> min_clear_gain_log;	// 16+14=30 -...
	ir_average    = (ir_average    << FLICKER_AVG_SHIFT) >> min_ir_gain_log; // 16+14=30 -...
	uv_average    = (uv_average    << FLICKER_AVG_SHIFT) >> min_uv_gain_log; // 16+14=30 -...

	clear_gain = 1<<(min_clear_gain_log);
	ir_gain    = 1<<(min_clear_gain_log);
	uv_gain    = 1<<(min_clear_gain_log);
	minClearGainLog = min_clear_gain_log;
#if IS_ENABLED(CONFIG_FLICKER_ANTI_ALIASING)
	freq = flicker_anti_aliasing_process(buf, als_info, remove_RS);
#else
	freq = get_single_freq(buf, als_info);
#endif //CONFIG_FLICKER_ANTI_ALIASING

	if (remove_rs_cnt > 30)
		remove_rs_cnt = 0;
	else
		remove_rs_cnt++;

	alps_data->flicker = freq;

	input_report_rel(stk_wrapper->als_input_dev, REL_RZ, alps_data->flicker + 1);
	input_sync(stk_wrapper->als_input_dev);

#if defined(CONFIG_SENSORS_FLICKER_SELF_TEST)
	als_eol_update_flicker(alps_data->flicker);
#endif
	/*
		return values (average, flicker... )
	*/
}
#endif

void stk6d2x_fifo_init(struct stk6d2x_data * alps_data)
{
	int32_t err;
	uint8_t flag_value;
	err = STK6D2X_REG_READ(alps_data, STK6D2X_REG_FIFO1, &flag_value);

	if (err < 0)
	{
		ALS_err("Read flag failed\n");
		return;
	}

	alps_data->fifo_info.data_type = (flag_value & STK6D2X_FIFO_SEL_MASK);
	ALS_info("data_type = %d\n", alps_data->fifo_info.data_type);

	switch (alps_data->fifo_info.data_type)
	{
		case STK6D2X_FIFO_ALS0:
		case STK6D2X_FIFO_ALS1:
		case STK6D2X_FIFO_ALS2:
			alps_data->fifo_info.frame_byte = 2;
			break;

		case STK6D2X_FIFO_ALS0_ALS1:
		case STK6D2X_FIFO_ALS0_ALS2:
		case STK6D2X_FIFO_ALS1_ALS2:
			alps_data->fifo_info.frame_byte = 4;
			break;

		case STK6D2X_FIFO_ALS0_ALS1_ALS2:
			alps_data->fifo_info.frame_byte = 6;
			break;

		case STK6D2X_FIFO_STA0_ALS0:
		case STK6D2X_FIFO_STA1_ALS1:
		case STK6D2X_FIFO_STA2_ALS2:
			alps_data->fifo_info.frame_byte = 4;
			break;

		case STK6D2X_FIFO_STA01_ALS0_ALS1:
		case STK6D2X_FIFO_STA02_ALS0_ALS2:
		case STK6D2X_FIFO_STA12_ALS1_ALS2:
			alps_data->fifo_info.frame_byte = 6;
			break;

		case STK6D2X_FIFO_STA01_STA2_ALS0_ALS1_ALS2:
			alps_data->fifo_info.frame_byte = 10;
			break;

		default:
			alps_data->fifo_info.frame_byte = 0xFF;
			ALS_err("Frame Type ERROR!\n");
			break;
	}

#if IS_ENABLED(CONFIG_FLICKER_ANTI_ALIASING)
	{
		uint32_t arr[] = {
			sr_table[0].sampling_time,
			sr_table[1].sampling_time,
			sr_table[2].sampling_time,
		};
		flicker_anti_aliasing_init(arr, SEC_FFT_SIZE);
	}
#endif

	alps_data->fifo_info.read_frame = STK_FIFO_I2C_READ_FRAME;
	alps_data->fifo_info.target_frame_count = STK_FIFO_I2C_READ_FRAME_TARGET;
	alps_data->fifo_info.read_max_data_byte = alps_data->fifo_info.frame_byte * STK_FIFO_I2C_READ_FRAME;
	alps_data->fifo_info.fifo_reading = false;
	alps_data->fifo_info.block_size = STK_FIFO_I2C_READ_FRAME_TARGET;
	alps_data->fifo_info.fft_buf_idx = 0;
	alps_data->fifo_info.ext_clk_chk = false;
	alps_data->fifo_info.pre_ext_clk_chk = false;
}

int32_t stk6d2x_enable_fifo(struct stk6d2x_data * alps_data, bool en)
{
	int32_t ret = 0;
	uint8_t fifo_status, i2c_data_reg[2] = {0};
	uint16_t max_frame_count = STK_FIFO_DATA_BUFFER_LEN / alps_data->fifo_info.frame_byte;

	if (alps_data->fifo_info.fifo_enable == en)
	{
		ALS_dbg("FIFO already set\n");
		return ret;
	}

	ret = STK6D2X_REG_READ(alps_data, STK6D2X_REG_FIFO1, &fifo_status);

	if (ret < 0)
	{
		return ret;
	}

	if (en)
	{
		fifo_status = (STK6D2X_FIFO_MODE_NORMAL | fifo_status);
		i2c_data_reg[0] = (max_frame_count >> 8) & 0x03;
		i2c_data_reg[1] = max_frame_count & 0xFF;
		alps_data->fifo_info.fft_buf_idx = 0;
	}
	else
	{
		fifo_status = (fifo_status & (~STK6D2X_FIFO_MODE_NORMAL));
	}

	ret = STK6D2X_REG_WRITE_BLOCK(alps_data,
								  STK6D2X_REG_FIFO1_WM_LV,
								  i2c_data_reg,
								  sizeof(i2c_data_reg) / sizeof(i2c_data_reg[0]));

	if (ret < 0)
	{
		ALS_err("fail, ret=%d\n", ret);
		return ret;
	}

	ret = STK6D2X_REG_READ_MODIFY_WRITE(alps_data,
										STK6D2X_REG_FIFO1,
										fifo_status,
										0xFF);

	if (ret < 0)
	{
		ALS_err("fail, ret=%d\n", ret);
		return ret;
	}

	alps_data->fifo_info.fifo_enable = en;
	return ret;
}

void SEC_local_average(struct stk6d2x_data* alps_data)
{
	uint64_t clear_local_average = 0;
	uint64_t uv_local_average = 0;
	uint64_t ir_local_average = 0;

	bool is_clear_local_sat = false;
	bool is_uv_local_sat = false;
	bool is_ir_local_sat = false;
	int i = 0;
	int n = 0;

	uint32_t min_clear_local_gain_log = alps_data->fifo_info.fft_gain_clear[alps_data->index_last];
	uint32_t min_uv_local_gain_log = alps_data->fifo_info.fft_gain_uv[alps_data->index_last];
	uint32_t min_ir_local_gain_log = alps_data->fifo_info.fft_gain_ir[alps_data->index_last];

	int local_length = 0;
	int index_last_temp = 0;

	if (alps_data->is_first)
	{
		n = (int)(alps_data->fifo_info.fft_buf_idx) - 1;
		if (n > -1)
		{
			clear_local_average += (alps_data->fifo_info.fft_buf[n]) >> ((alps_data->fifo_info.fft_gain_clear[n]) - (alps_data->fifo_info.fft_gain_clear[n]));
			uv_local_average += (alps_data->fifo_info.fft_uv_buf[n]) >> ((alps_data->fifo_info.fft_gain_uv[n]) - (alps_data->fifo_info.fft_gain_uv[n]));
			ir_local_average += (alps_data->fifo_info.fft_ir_buf[n]) >> ((alps_data->fifo_info.fft_gain_ir[n]) - (alps_data->fifo_info.fft_gain_ir[n]));
			if (alps_data->fifo_info.fft_xflag[n] & 0x1)
				is_clear_local_sat = true;
			if (alps_data->fifo_info.fft_xflag[n] & 0x2)
				is_uv_local_sat = true;
			if (alps_data->fifo_info.fft_xflag[n] & 0x4)
				is_ir_local_sat = true;
			alps_data->is_first = false;
		}
		else
		{
			return; // no data
		}
	}


	if ((alps_data->fifo_info.fft_buf_idx < alps_data->index_last + SEC_LOCAL_AVG_SIZE))
	{
		if (alps_data->fifo_info.fft_buf_idx == 0 && alps_data->index_last > 0)
		{

		}
		else
		{
			return; // not yet
		}
	}

	local_length = SEC_LOCAL_AVG_SIZE;
	index_last_temp = alps_data->index_last + SEC_LOCAL_AVG_SIZE;
	if (index_last_temp > SEC_FFT_SIZE)
	{
		index_last_temp = SEC_FFT_SIZE;
		local_length = index_last_temp - alps_data->index_last;
	}

	// find minimum gain ratio, check saturation
	for (i = alps_data->index_last; i < index_last_temp; i++)
	{
		if (alps_data->fifo_info.fft_gain_clear[i] < min_clear_local_gain_log)
			min_clear_local_gain_log = alps_data->fifo_info.fft_gain_clear[i];
		if (alps_data->fifo_info.fft_gain_uv[i] < min_uv_local_gain_log)
			min_uv_local_gain_log = alps_data->fifo_info.fft_gain_uv[i];
		if (alps_data->fifo_info.fft_gain_ir[i] < min_ir_local_gain_log)
			min_ir_local_gain_log = alps_data->fifo_info.fft_gain_ir[i];
		if (alps_data->fifo_info.fft_xflag[i] & 0x1)
			is_clear_local_sat = true;
		if (alps_data->fifo_info.fft_xflag[i] & 0x2)
			is_uv_local_sat = true;
		if (alps_data->fifo_info.fft_xflag[i] & 0x4)
			is_ir_local_sat = true;
	}

	// gain unification
	for (i = alps_data->index_last; i < index_last_temp; i++)
	{
		clear_local_average		+= (alps_data->fifo_info.fft_buf[i])	>> ((alps_data->fifo_info.fft_gain_clear[i]) - min_clear_local_gain_log);
		uv_local_average		+= (alps_data->fifo_info.fft_uv_buf[i]) >> ((alps_data->fifo_info.fft_gain_uv[i])	 - min_uv_local_gain_log);
		ir_local_average		+= (alps_data->fifo_info.fft_ir_buf[i]) >> ((alps_data->fifo_info.fft_gain_ir[i])	 - min_ir_local_gain_log);
	}

	// cal average

	clear_local_average /= local_length; // 16
	uv_local_average	/= local_length;
	ir_local_average	/= local_length;


	clear_local_average = (clear_local_average << FLICKER_AVG_SHIFT) >> min_clear_local_gain_log;	// 16+14=30 -...
	uv_local_average = (uv_local_average << FLICKER_AVG_SHIFT) >> min_uv_local_gain_log; // 16+14=30 -...
	ir_local_average = (ir_local_average << FLICKER_AVG_SHIFT) >> min_ir_local_gain_log; // 16+14=30 -...

	alps_data->clear_gain = 1<<(min_clear_local_gain_log);
	alps_data->ir_gain    = 1<<(min_ir_local_gain_log);
	alps_data->uv_gain    = 1<<(min_uv_local_gain_log);

	alps_data->clear_local_average = clear_local_average;
	alps_data->uv_local_average    = uv_local_average;
	alps_data->ir_local_average    = ir_local_average;

	alps_data->is_clear_local_sat = is_clear_local_sat;
	alps_data->is_uv_local_sat    = is_uv_local_sat;
	alps_data->is_ir_local_sat    = is_ir_local_sat;
	ALS_info("clear_local_average:%llu is_clear_local_sat:%d | uv_local_average:%llu is_uv_local_sat:%d | ir_local_average:%llu is_ir_local_sat:%d\n",
			clear_local_average, is_clear_local_sat, uv_local_average, is_uv_local_sat, ir_local_average, is_ir_local_sat);

	alps_data->index_last = index_last_temp;
	if (alps_data->index_last >= SEC_FFT_SIZE)
		alps_data->index_last = 0;

	alps_data->is_local_avg_update = true;
}

void stk6d2x_fifo_get_data(struct stk6d2x_data * alps_data, uint16_t frame_num)
{
	uint16_t offset;
	uint8_t raw_data[STK_FIFO_I2C_READ_FRAME_BUF_SIZE]; // alps_data->fifo_info.read_max_data_byte * sizeof(uint8_t)
	int16_t i, frame_count = 0, read_frame_num = 0;
	int32_t ret, read_bytes;
#ifdef STK_FIFO_DATA_SUMMATION
	uint64_t als0_sum = 0, als1_sum = 0, als2_sum = 0;
#endif

#ifdef STK_CHK_XFLG
	alps_data->xflag = 0;
	if (alps_data->rid != 1)
	{
		ret = STK6D2X_REG_READ(alps_data, STK6D2X_REG_XFLAG, &alps_data->xflag);

		if (ret < 0)
		{
			return;
		}
	}
#endif

	memset(alps_data->fifo_info.fifo_data0, 0, STK_FIFO_I2C_READ_FRAME_TARGET * sizeof(uint32_t));
	memset(alps_data->fifo_info.fifo_data1, 0, STK_FIFO_I2C_READ_FRAME_TARGET * sizeof(uint32_t));
	memset(alps_data->fifo_info.fifo_data2, 0, STK_FIFO_I2C_READ_FRAME_TARGET * sizeof(uint32_t));
	memset(alps_data->fifo_info.fifo_xflag, 0, STK_FIFO_I2C_READ_FRAME_TARGET * sizeof(uint8_t));

#ifdef STK_FIFO_DATA_SUMMATION
	if (alps_data->fifo_info.fifo_sum_cnt >= STK6D2X_ALS_SUMMATION_CNT)
	{
		alps_data->fifo_info.fifo_sum_cnt = 0;
	}
#endif

	for (frame_count = 0 ; frame_count < frame_num ; frame_count += (alps_data->fifo_info.read_frame))
	{
		read_frame_num = (int16_t)(frame_num - frame_count);

		if (read_frame_num >= alps_data->fifo_info.read_frame)
		{
			read_bytes = alps_data->fifo_info.read_max_data_byte;
			read_frame_num = alps_data->fifo_info.read_frame;
		}
		else
		{
			read_bytes = alps_data->fifo_info.frame_byte * read_frame_num;
		}

		memset(raw_data, 0, sizeof(raw_data));
		ret = STK6D2X_REG_BLOCK_READ(alps_data, STK6D2X_REG_FIFO_OUT, read_bytes, raw_data);

		if (ret < 0)
		{
			ALS_err("fail, err=0x%x\n", ret);
			return;
		}


		switch (alps_data->fifo_info.data_type)
		{
			case STK6D2X_FIFO_ALS0:
			case STK6D2X_FIFO_ALS1:
			case STK6D2X_FIFO_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset) << 8 | *(raw_data + offset + 1));
				}

				break;

			case STK6D2X_FIFO_ALS0_ALS1:
			case STK6D2X_FIFO_ALS0_ALS2:
			case STK6D2X_FIFO_ALS1_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset) << 8 | *(raw_data + offset + 1));
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
				}
				break;

			case STK6D2X_FIFO_ALS0_ALS1_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset) << 8 | *(raw_data + offset + 1));
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					*(alps_data->fifo_info.fifo_data2 + frame_count + i) = (*(raw_data + offset + 4) << 8 | *(raw_data + offset + 5));
				}
				break;

#ifdef STK_ALS_AGC
			case STK6D2X_FIFO_STA0_ALS0:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					if (alps_data->rid == 1)
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset + 1) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
						}
#endif
#ifdef STK_CHK_XFLG
						if ((*(raw_data + offset) & 0x80))
						{
							alps_data->xflag |= 0x1;
						}
#endif
					}
					else
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
						}
#endif
					}
					alps_data->als_info.als_cur_dgain[0] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[0] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[0] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS0);

					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					stk6d2x_get_als_ratio(alps_data);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[0];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als0_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;

			case STK6D2X_FIFO_STA1_ALS1:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					if (alps_data->rid == 1)
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset + 1) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
						}
#endif
#ifdef STK_CHK_XFLG
						if ((*(raw_data + offset) & 0x80))
						{
							alps_data->xflag |= 0x1;
						}
#endif
					}
					else
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
						}
#endif
					}
					alps_data->als_info.als_cur_dgain[1] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[1] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[1] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS1);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					stk6d2x_get_als_ratio(alps_data);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[1];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als1_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;
			case STK6D2X_FIFO_STA2_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					if (alps_data->rid == 1)
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset + 1) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
						}
#endif
#ifdef STK_CHK_XFLG
						if ((*(raw_data + offset) & 0x80))
						{
							alps_data->xflag |= 0x1;
						}
#endif
					}
					else
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
						}
#endif
					}
					alps_data->als_info.als_cur_dgain[2] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[2] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[2] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS2);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					stk6d2x_get_als_ratio(alps_data);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[2];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als2_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;
			case STK6D2X_FIFO_STA01_ALS0_ALS1:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
#ifdef STK_CHK_CLK_SRC
					if ((*(raw_data + offset) & 0x80) == 0)
					{
						alps_data->fifo_info.ext_clk_chk = false;
					}
					else
					{
						alps_data->fifo_info.ext_clk_chk = true;
					}
#endif
					alps_data->als_info.als_cur_dgain[0] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[0] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[0] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS0);
					alps_data->als_info.als_cur_dgain[1] = stk6d2x_cal_curDGain(*(raw_data + offset + 1) & 0x7);
					alps_data->als_info.als_cur_again[1] = stk6d2x_als_get_again_multiple((*(raw_data + offset + 1) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[1] = stk6d2x_als_get_pd_multiple((*(raw_data + offset + 1) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS1);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset + 4) << 8 | *(raw_data + offset + 5));
					stk6d2x_get_als_ratio(alps_data);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[0];
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) *= alps_data->als_info.als_cur_ratio[1];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als0_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						als1_sum += *(alps_data->fifo_info.fifo_data1 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;
			case STK6D2X_FIFO_STA02_ALS0_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
#ifdef STK_CHK_CLK_SRC
					if ((*(raw_data + offset) & 0x80) == 0)
					{
						alps_data->fifo_info.ext_clk_chk = false;
					}
					else
					{
						alps_data->fifo_info.ext_clk_chk = true;
					}
#endif
					alps_data->als_info.als_cur_dgain[0] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[0] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[0] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS0);
					alps_data->als_info.als_cur_dgain[2] = stk6d2x_cal_curDGain(*(raw_data + offset + 1) & 0x7);
					alps_data->als_info.als_cur_again[2] = stk6d2x_als_get_again_multiple((*(raw_data + offset + 1) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[2] = stk6d2x_als_get_pd_multiple((*(raw_data + offset + 1) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS2);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset + 4) << 8 | *(raw_data + offset + 5));
					stk6d2x_get_als_ratio(alps_data);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[0];
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) *= alps_data->als_info.als_cur_ratio[2];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als0_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						als2_sum += *(alps_data->fifo_info.fifo_data1 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;
			case STK6D2X_FIFO_STA12_ALS1_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
#ifdef STK_CHK_CLK_SRC
					if ((*(raw_data + offset) & 0x80) == 0)
					{
						alps_data->fifo_info.ext_clk_chk = false;
					}
					else
					{
						alps_data->fifo_info.ext_clk_chk = true;
					}
#endif
					alps_data->als_info.als_cur_dgain[1] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[1] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[1] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS1);
					alps_data->als_info.als_cur_dgain[2] = stk6d2x_cal_curDGain(*(raw_data + offset + 1) & 0x7);
					alps_data->als_info.als_cur_again[2] = stk6d2x_als_get_again_multiple((*(raw_data + offset + 1) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[2] = stk6d2x_als_get_pd_multiple((*(raw_data + offset + 1) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS2);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset + 4) << 8 | *(raw_data + offset + 5));
					stk6d2x_get_als_ratio(alps_data);
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[1];
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) *= alps_data->als_info.als_cur_ratio[2];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als1_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						als2_sum += *(alps_data->fifo_info.fifo_data1 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;

			case STK6D2X_FIFO_STA01_STA2_ALS0_ALS1_ALS2:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					if (alps_data->rid == 1)
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset + 3) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
#ifdef SEC_FFT_FLICKER_1024
							*(alps_data->fifo_info.fifo_xflag + frame_count + i) |= (1 << 7); //bit7 is cfalg
#endif
						}
#endif
#ifdef STK_CHK_XFLG
						if ((*(raw_data + offset) & 0x80))
						{
							alps_data->xflag |= 0x1;
#ifdef SEC_FFT_FLICKER_1024
							*(alps_data->fifo_info.fifo_xflag + frame_count + i) |= 0x1; //bit0 is xfalg for C
#endif
						}
						if ((*(raw_data + offset + 1) & 0x80))
						{
							alps_data->xflag |= 0x2;
#ifdef SEC_FFT_FLICKER_1024
							*(alps_data->fifo_info.fifo_xflag + frame_count + i) |= 0x2; //bit1 is xfalg for UV
#endif
						}
						if ((*(raw_data + offset + 2) & 0x80))
						{
							alps_data->xflag |= 0x4;
#ifdef SEC_FFT_FLICKER_1024
							*(alps_data->fifo_info.fifo_xflag + frame_count + i) |= 0x4; //bit2 is xfalg for IR
#endif
						}
#endif
					}
					else
					{
#ifdef STK_CHK_CLK_SRC
						if ((*(raw_data + offset) & 0x80) == 0)
						{
							alps_data->fifo_info.ext_clk_chk = false;
						}
						else
						{
							alps_data->fifo_info.ext_clk_chk = true;
#ifdef SEC_FFT_FLICKER_1024
							*(alps_data->fifo_info.fifo_xflag + frame_count + i) |= (1 << 7); //bit7 is cfalg
#endif

						}
#endif
					}
					alps_data->als_info.als_cur_dgain[0] = stk6d2x_cal_curDGain(*(raw_data + offset) & 0x7);
					alps_data->als_info.als_cur_again[0] = stk6d2x_als_get_again_multiple((*(raw_data + offset) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[0] = stk6d2x_als_get_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS0);
					alps_data->als_info.als_cur_dgain[1] = stk6d2x_cal_curDGain(*(raw_data + offset + 1) & 0x7);
					alps_data->als_info.als_cur_again[1] = stk6d2x_als_get_again_multiple((*(raw_data + offset + 1) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[1] = stk6d2x_als_get_pd_multiple((*(raw_data + offset + 1) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS1);
					alps_data->als_info.als_cur_dgain[2] = stk6d2x_cal_curDGain(*(raw_data + offset + 2) & 0x7);
					alps_data->als_info.als_cur_again[2] = stk6d2x_als_get_again_multiple((*(raw_data + offset + 2) & 0x18) >> 3);
					alps_data->als_info.als_cur_pd_reduce[2] = stk6d2x_als_get_pd_multiple((*(raw_data + offset + 2) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS2);

					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 4) << 8 | *(raw_data + offset + 5));
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset + 6) << 8 | *(raw_data + offset + 7));
					*(alps_data->fifo_info.fifo_data2 + frame_count + i) = (*(raw_data + offset + 8) << 8 | *(raw_data + offset + 9));
					stk6d2x_get_als_ratio(alps_data);
#ifdef SEC_FFT_FLICKER_1024
					*(alps_data->fifo_info.fifo_data_clear + frame_count + i) = *(alps_data->fifo_info.fifo_data0 + frame_count + i);
					*(alps_data->fifo_info.fifo_data_uv    + frame_count + i) = *(alps_data->fifo_info.fifo_data1 + frame_count + i);
					*(alps_data->fifo_info.fifo_data_ir    + frame_count + i) = *(alps_data->fifo_info.fifo_data2 + frame_count + i);

					*(alps_data->fifo_info.fifo_gain_clear + frame_count + i) = stk6d2x_sec_dgain(*(raw_data + offset) & 0x7) + \
																				stk6d2x_sec_again((*(raw_data + offset) & 0x18) >> 3) + \
																				stk6d2x_sec_pd_multiple((*(raw_data + offset) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS0);
					*(alps_data->fifo_info.fifo_gain_uv    + frame_count + i) = stk6d2x_sec_dgain(*(raw_data + offset + 1) & 0x7) + \
																				stk6d2x_sec_again((*(raw_data + offset + 1) & 0x18) >> 3) + \
																				stk6d2x_sec_pd_multiple((*(raw_data + offset + 1) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS1);
					*(alps_data->fifo_info.fifo_gain_ir    + frame_count + i) = stk6d2x_sec_dgain(*(raw_data + offset + 2) & 0x7) + \
																				stk6d2x_sec_again((*(raw_data + offset + 2) & 0x18) >> 3) + \
																				stk6d2x_sec_pd_multiple((*(raw_data + offset + 2) & 0x60) >> 5, \
																						alps_data->als_info.als_cur_pd_mode, STK6D2X_TYPE_ALS2);
#endif
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) *= alps_data->als_info.als_cur_ratio[0];
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) *= alps_data->als_info.als_cur_ratio[1];
					*(alps_data->fifo_info.fifo_data2 + frame_count + i) *= alps_data->als_info.als_cur_ratio[2];
#ifdef STK_FIFO_DATA_SUMMATION
					if (alps_data->fifo_info.fifo_sum_cnt < STK6D2X_ALS_SUMMATION_CNT)
					{
						als0_sum += *(alps_data->fifo_info.fifo_data0 + frame_count + i);
						als1_sum += *(alps_data->fifo_info.fifo_data1 + frame_count + i);
						als2_sum += *(alps_data->fifo_info.fifo_data2 + frame_count + i);
						alps_data->fifo_info.fifo_sum_cnt ++;
					}
#endif
				}
				break;
#endif
			default:
				ALS_err("unavailable!\n");
				break;
		}
	}
#ifdef STK_CHK_CLK_SRC
	if (!alps_data->fifo_info.ext_clk_chk)
	{
		/*
		alps_data->fifo_info.last_frame_count = 0;
		ALS_err("internal clock mode\n");
		alps_data->is_long_it = true;
		memset(alps_data->fifo_info.fft_buf, 0, FFT_BUF_SIZE);
		alps_data->fifo_info.fft_buf_idx = 0;
		return;
		*/
		ALS_info("internal clock mode");
	}
#endif

#ifdef STK_CHK_XFLG
    if ((alps_data->xflag & 0x1))
        ALS_err("XFLAG ALS0  Occur\n");
    if ((alps_data->xflag & 0x2))
        ALS_err("XFLAG ALS1  Occur\n");
    if ((alps_data->xflag & 0x4))
        ALS_err("XFLAG ALS2  Occur\n");
#endif
    if (alps_data->fifo_info.pre_ext_clk_chk != alps_data->fifo_info.ext_clk_chk)
    {
        ALS_info("clock change skip fifo data");
        alps_data->fifo_info.pre_ext_clk_chk = alps_data->fifo_info.ext_clk_chk;
        alps_data->fifo_info.fft_buf_idx = 0;
        return;
    }

#ifdef STK_FIFO_DATA_SUMMATION
#ifdef STK_CHK_XFLG
	//if (!(alps_data->xflag & 0x1))
#endif
	{
		alps_data->fifo_info.fifo_sum_als0 = als0_sum;
	}
#ifdef STK_CHK_XFLG
	//if (!(alps_data->xflag & 0x2))
#endif
	{
		alps_data->fifo_info.fifo_sum_als1 = als1_sum;
	}
#ifdef STK_CHK_XFLG
	//if (!(alps_data->xflag & 0x4))
#endif
	{
		alps_data->fifo_info.fifo_sum_als2 = als2_sum;
	}
#endif
	alps_data->fifo_info.last_frame_count = frame_num;

#ifdef STK_CHK_XFLG
	if (/*!(alps_data->xflag & 0x1) && */frame_num < STK_FIFO_I2C_READ_FRAME_TARGET)
#endif
	{
#ifdef STK_FFT_FLICKER
		for (i = 0; i < frame_num; i ++)
		{
			alps_data->fifo_info.fft_buf[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_data_clear[i];
			alps_data->fifo_info.fft_uv_buf[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_data_uv[i];
			alps_data->fifo_info.fft_ir_buf[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_data_ir[i];
			alps_data->fifo_info.fft_gain_clear[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_gain_clear[i];
			alps_data->fifo_info.fft_gain_uv[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_gain_uv[i];
			alps_data->fifo_info.fft_gain_ir[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_gain_ir[i];
			alps_data->fifo_info.fft_xflag[alps_data->fifo_info.fft_buf_idx] = alps_data->fifo_info.fifo_xflag[i];
			alps_data->fifo_info.fft_buf_idx ++;
			if (alps_data->fifo_info.fft_buf_idx >= FFT_BUF_SIZE)
			{
				break;
			}
		}

		if (alps_data->fifo_info.fft_buf_idx >= FFT_BUF_SIZE)
		{
			alps_data->fifo_info.fft_buf_idx = 0;

#ifdef SEC_FFT_FLICKER_1024
			SEC_fft_entry(alps_data);
#if IS_ENABLED(CONFIG_FLICKER_ANTI_ALIASING)
			ALS_info("Current sampling rate: %d Hz", sr_table[alps_data->cur_sample_rate].sample_rate);
			if (alps_data->cur_sample_rate > Sample_Rate_MIN)
			{
				alps_data->cur_sample_rate --;
			}
			else
			{
				alps_data->cur_sample_rate = Sample_Rate_MAX;
			}
			stk6d2x_switch_sample_rate(alps_data);
			flicker_anti_aliasing_set_index(alps_data->cur_sample_rate);
#endif // CONFIG_FLICKER_ANTI_ALIASING
#endif // SEC_FFT_FLICKER_1024
		}
#endif
	}
#ifdef STK_CHK_XFLG
	else
	{
		alps_data->fifo_info.fft_buf_idx = 0;
	}
#endif
}

void stk6d2x_get_fifo_data_polling(struct stk6d2x_data *alps_data)
{
	int32_t ret;
	uint8_t raw_data[2] = {0};
	uint16_t frame_num;
	bool is_fifo_warning = false;

	if (alps_data->fifo_info.fifo_enable == false)
	{
		ALS_err("FIFO is disable\n");
		return;
	}

	alps_data->fifo_info.fifo_reading = true;
	alps_data->is_local_avg_update = false;

	ret = STK6D2X_REG_BLOCK_READ(alps_data, STK6D2X_REG_FIFO_FCNT1, 2, raw_data);

	if (ret < 0)
	{
		ALS_err("fail, err=0x%x\n", ret);
		return;
	}

	frame_num = (((raw_data[0] & 0x3) << 8) | raw_data[1]);
	ALS_info("frame_num = %d\n", frame_num);

	if (frame_num == 0)
	{
		return;
	}
	else
	{
		if (alps_data->fifo_info.data_type != STK6D2X_FIFO_STA01_STA2_ALS0_ALS1_ALS2)
		{
			if (frame_num >= 2 * STK_FIFO_I2C_READ_FRAME_TARGET)
			{
				frame_num = STK_FIFO_I2C_READ_FRAME_TARGET;
				is_fifo_warning = true;
			}
			else if(frame_num >= STK_FIFO_I2C_READ_FRAME_TARGET)
			{
				frame_num = STK_FIFO_I2C_READ_FRAME_TARGET;
			}
		}
		else
		{
			if (frame_num >= STK_FIFO_I2C_READ_FRAME_TARGET)
			{
				frame_num = STK_FIFO_I2C_READ_FRAME_TARGET;
				is_fifo_warning = true;
			}
		}
	}

	stk6d2x_fifo_get_data(alps_data, frame_num);

	if (alps_data->fifo_info.last_frame_count != 0)
		SEC_local_average(alps_data);

	if (is_fifo_warning)
	{
		ALS_info("is_fifo_warning\n");
		stk6d2x_fifo_get_data(alps_data, frame_num);
	}

	alps_data->fifo_info.fifo_reading = false;
	if (alps_data->fifo_info.fifo_enable == false)
		ALS_dbg("fifo read done");
}

void remove_RS(int32_t *data, uint size, bool debug)
{
	// int32_t* data : 16bit fifo data
	// enum fft_size : fft data size
	// for M project (S26)

	int f_rs_idx = 0;
	int32_t f_rs_val = 0;
	int f_max_idx = 300;
	int32_t f_max_val = 0;
	int len = size / 2;
	int i = 0;
	int64_t limitRS;
	int32_t k1, k2, k3;
	int32_t temp;

	for (i = 0; i < size; i++) {
		if (debug && remove_rs_cnt > 30) // print debug log
			ALS_info("DEBUG_FLICKER fft_mag[%d] => %d\n", i, data[i]);
	}

	for (i = 0; i < 3; i++) // 12Hz, 14Hz, 16Hz
	{
		if (data[i + 5] < data[i + 6] && data[i + 6] >= data[i + 7])
		{
			if (data[i + 6] >= f_rs_val)
			{
				f_rs_idx = i + 6;
				f_rs_val = data[i + 6];
			}
		}
	}

	if (f_rs_idx == 0)
	{
		// no rs peak
		return;
	}

	limitRS = ((int64_t)52000 << minClearGainLog) >> 15;

	if (f_rs_val > (int32_t)limitRS)
	{
		f_rs_val = (int32_t)limitRS;
	}

	f_max_val = f_rs_val / 4;

	for (i = 0; i < 8; i += 2)
	{
		temp = data[i] - f_rs_val;
		if (temp > 2)
			data[i] = temp;
		else
			data[i] = 2;
	}

	for (i = 1; i < 8; i += 2)
	{
		temp = data[i] - f_rs_val;
		if (temp > 1)
			data[i] = temp;
		else
			data[i] = 1;
	}

	for (i = 300; i <= len; i += 2)
	{
		temp = data[i] - f_max_val;
		if (temp > 2)
			data[i] = temp;
		else
			data[i] = 2;
	}

	for (i = 301; i <= len; i += 2)
	{
		temp = data[i] - f_max_val;
		if (temp > 1)
			data[i] = temp;
		else
			data[i] = 1;
	}

	k1 = f_max_val - f_rs_val;
	k2 = 300 - 8;
	k3 = k1 / k2;

	for (i = 8; i < 300; i += 2)
	{
		temp = data[i] - (k3 * (i - 8) + f_rs_val);
		if (temp > 2)
			data[i] = temp;
		else
			data[i] = 2;
	}

	for (i = 9; i < 300; i += 2)
	{
		temp = data[i] - (k3 * (i - 8) + f_rs_val);
		if (temp > 1)
			data[i] = temp;
		else
			data[i] = 1;
	}

	for (i = 0; i < size; i++) {
		if (debug && remove_rs_cnt > 30) // print debug log
			ALS_info("DEBUG_FLICKER fft_remove_RS[%d] => %d\n", i, data[i]);
	}
}
#endif
