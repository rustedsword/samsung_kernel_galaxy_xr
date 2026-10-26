#include <linux/module.h>
#include "flicker_anti_aliasing.h"
#define NYQUIST_FOLD		10
#define MIN_TEMP_FREQ		70000
#define CHECK_MARGIN 		4000

#define CHECK_DISTANCE(A, B)	((A) > (B))? (((A)-(B)) <= CHECK_MARGIN) : (((B)-(A)) <= CHECK_MARGIN)

#define N 4  // the number of valid freq list

#define UFM_MAX 4

int g_block_size = 0;

static bool __check_margin2(uint32_t A, uint32_t B)
{
	if (A == 0 && B == 0)
		return false;
	if(A > B)
		return ((A - B) <= CHECK_MARGIN);
	else
		return ((B - A) <= CHECK_MARGIN);
}

static bool __check_margin3(uint32_t A, uint32_t B, uint32_t C)
{
	uint32_t max = A;
	uint32_t min = A;

	if (A == 0 && B == 0 && C == 0)
		return false;
	if(B > max)
		max = B;
	if(C > max)
		max = C;
	if(B < min)
		min = B;
	if(C < min)
		min = C;
	return ((max - min) <= CHECK_MARGIN);
}

struct mode_step_data multi_result_data[3] = {
	{}, {}, {}
};

struct sample_rate_data g_samplerates[3];

struct global_step_data g_data = {
	.cur_mode = MODE_1,
};

struct freq_t {
	uint freq;
	uint value;
	bool valid;
};

struct peak_list {
	uint count;
	struct freq_t freq[N];
	uint min_index;
};

struct peak_list valid;

void remove_min_valid(void)
{
	int index;
	int i;
	uint32_t min;

	if (valid.count < N)
		index = valid.count;
	else
	 	index = valid.min_index;

	for (i = index; i > 0; i--)
		valid.freq[i] = valid.freq[i - 1];

	if (valid.count < N) {
		valid.min_index = valid.count; // just insert.
	} else {
		// find min_index
		for (i = 0; i < N; i++) {
			min = valid.freq[valid.min_index].value;
			if (valid.freq[i].value < min) {
				valid.min_index = i;
			}
		}
	}
}

void insert_valid(uint32_t data, uint freq)
{
	remove_min_valid();

	if (valid.count < N)
		valid.count++;

	valid.freq[0].freq =  freq;
	valid.freq[0].value = data;

	if (data < valid.freq[valid.min_index].value)
		valid.min_index = 0;
}

uint local_max_index(uint32_t *data, int start, int end, bool get_list)
{
	uint i_max = 0;
	uint32_t val_max = 0;
	int i;
	uint32_t freq;

	for (i = start; i <= end; i++) {
		if(data[i] > data[i - 1] && data[i] >= data[i + 1]) {
			freq = i * g_samplerates[g_data.cur_sr_index].resolution;
			if (data[i] > val_max) {
				i_max = i;
				val_max = data[i];
			}

			if (get_list && (valid.count < N || data[i] > valid.freq[valid.min_index].value))
				insert_valid(data[i], freq);
		}
	}
	return i_max;
}

void get_average(uint32_t *data, int start, int end, uint64_t *avg)
{
	int i;

	for (i = start; i <= end; i++) {
		*avg += data[i];
	}
	*avg /= end - start + 1;
}

uint32_t compare_nyquist(bool check_boundary, bool debug)
{
	uint32_t freq_list[3][NYQUIST_FOLD] = {0, }; // NYQUIST_FOLD
	uint32_t freq_candits[3];
	int boundary_idx = -1;
	uint32_t nyq_hz[3];
	uint32_t freq[3];
	bool found = false;
	int ret = 0;
	int i;
	int j;
	int k;
	uint32_t idx;
	uint32_t freq_idx;
	uint32_t _nyq_hz;
	uint32_t _freq;

	retry:
	for (i = 0; i< 3; i++) {
		nyq_hz[i] = g_samplerates[i].nyquist_Hz;
		freq[i] = multi_result_data[i].freq_mHz;
	}

	if (check_boundary) {
		boundary_idx++;
		if (boundary_idx >= 3) {
			printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ COMPARE_NYQUIST boundary check, Not found");
			return 0;
		}
		freq[boundary_idx] = 0;
	}

	/* freq_list[0] <= cur
	 * freq_list[1] <= prev
	 * freq_list[2] <= prev prev
	 */
	for (idx = 0; idx < g_data.cur_mode; idx++) {
		freq_idx = (idx + g_data.cur_sr_index) % 3;
		_nyq_hz = nyq_hz[freq_idx];
		_freq = freq[freq_idx];
		for(i = 0; i < NYQUIST_FOLD; i++) {
			if (i&0x1) { // odd
				freq_list[idx][i] = (i + 1) * _nyq_hz - _freq;
			} else {
				freq_list[idx][i] = i * _nyq_hz + _freq;
			}
		}
		if (debug)
			printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ COMPARE_NYQUIST, index, nyquist freq, %d, %d (mHz)", idx, _nyq_hz);

		if(freq_list[idx][0] < MIN_TEMP_FREQ) { // Skip when less than 70Hz
			if (debug || g_data.cur_mode == MODE_1)
				printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ COMPARE_NYQUIST, less than min_temp_freq, %d < %d (mHz)", freq_list[idx][0], MIN_TEMP_FREQ);
			freq_list[idx][0] = 0;
		}
	}

	if (debug)
		printk(KERN_INFO "Flicker_Multi: " "DEBUG_FLICKER check list[0] %8d %8d %8d cur_sr %d, mode %d", freq_list[0][0], freq_list[1][0], freq_list[2][0], g_data.cur_sr_index, g_data.cur_mode);

	for(i = 0; i < NYQUIST_FOLD; i++) {
		if (debug && i + 1 < NYQUIST_FOLD) {
			printk(KERN_INFO "Flicker_Multi: " "DEBUG_FLICKER check list[%d] %8d %8d %8d cur_sr %d, mode %d", i + 1, freq_list[0][i + 1], freq_list[1][i + 1], freq_list[2][i + 1], g_data.cur_sr_index, g_data.cur_mode);
		}

		switch (g_data.cur_mode) {
			case MODE_1:
				printk(KERN_ERR "Flicker_Multi: invalid state");
				break;
			case MODE_2:
				for (j = (i - 1 >= 0)?(i - 1):i; j <= i + 1 && j < NYQUIST_FOLD; j++) {
					if (__check_margin2(freq_list[0][i], freq_list[1][j])) {
						found = true;
						ret = (freq_list[0][i] + freq_list[1][j]) / 2;
						break;
					}
				}
				break;
			case MODE_3:
				for (j = (i - 1 >= 0)?(i - 1):i; j <= i + 1 && j < NYQUIST_FOLD; j++) {
					for (k = (i - 1 >= 0)?(i - 1):i; k <= i + 1 && k < NYQUIST_FOLD; k++) {
						if (__check_margin3(freq_list[0][i], freq_list[1][j], freq_list[2][k])) {
							found = true;
							ret = (freq_list[0][i] + freq_list[1][j] + freq_list[2][k]) / 3;
							break;
						}
					}
					if (found)
						break;
				}
				break;
		}

		if (found)
			break;
	}

	if (found) {
		if (check_boundary && boundary_idx == g_data.cur_sr_index)
			multi_result_data[g_data.cur_sr_index].thd_valid = multi_result_data[(g_data.cur_sr_index + 1)%3].thd_valid; // special case

		return ret;
	}

	if (check_boundary)
		goto retry;

	if (debug)
		printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ COMPARE_NYQUIST, Not found");
	return 0; // not found
}

void find_maxfreq(uint32_t* data, unsigned int size, bool debug, void (*func)(int32_t*, uint, bool))
{
	int cur_spl_index = g_data.cur_sr_index;
	uint resolution = g_samplerates[cur_spl_index].resolution;
	struct mode_step_data *curr_mode = &multi_result_data[cur_spl_index];
	uint i_min_temp_freq = (MIN_TEMP_FREQ) / resolution;
	uint i_max_freq = size / 2;
	uint i_max = 0;
	uint i_temp_max = 0;
	int i;

	curr_mode->average = 0;
	curr_mode->temp_average = 0;
	curr_mode->max = 0;
	curr_mode->temp_max = 0;

	get_average(data, 2, i_max_freq, &curr_mode->average);
	get_average(data, i_min_temp_freq, i_max_freq, &curr_mode->temp_average);

	if (func != NULL)
		func(data, size, debug); 	// if it is needed, update data

	i_max = local_max_index(data, 2, i_max_freq, true);
	i_temp_max = local_max_index(data, i_min_temp_freq, i_max_freq, false);

	if(i_max == 0) {
		printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ, Not found");
		return;	// not found
	}

	curr_mode->freq_mHz = i_max * resolution;
	curr_mode->temp_freq_mHz = i_temp_max * resolution;

	curr_mode->max = data[i_max];
	curr_mode->temp_max = data[i_temp_max];

	curr_mode->thd_valid = ((curr_mode->average << 10) / curr_mode->max) < FLICKER_AVGMAX_THD;
	curr_mode->temp_thd_valid = ((curr_mode->temp_average << 10) / curr_mode->temp_max) < FLICKER_AVGMAX_THD;
	for (i = 0; i < valid.count; i++) {
		valid.freq[i].valid = ((curr_mode->temp_average << 10) / valid.freq[i].value) < FLICKER_AVGMAX_THD;
	}

	if (debug) {
		printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ mode=%d spl_index=%d nyq_mHz=%d res_mHz=%d freq_mHz=%d temp_freq_mHz=%d",
				g_data.cur_mode, cur_spl_index, g_samplerates[cur_spl_index].nyquist_Hz, g_samplerates[cur_spl_index].resolution, curr_mode->freq_mHz, curr_mode->temp_freq_mHz);
		printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER FIND_FREQ avg=%llu temp_avg=%llu max=%u temp_max=%u thd_valid=%d temp_thd_valid=%d",
				curr_mode->average, curr_mode->temp_average, data[i_max], data[i_temp_max], curr_mode->thd_valid, curr_mode->temp_thd_valid);
	}
}

void process_aliasing(bool debug) {
	uint32_t freq_mHz = 0;
	int cur_spl_index = g_data.cur_sr_index;
	struct mode_step_data *curr_mode = &multi_result_data[cur_spl_index];

	switch(g_data.cur_mode) {
		case MODE_1:
			g_data.final_freq = curr_mode->temp_freq_mHz;
			return;
		case MODE_2:
			freq_mHz = compare_nyquist(false, debug);
			if(freq_mHz > 0) {
				g_data.final_freq = freq_mHz;
				return;
			} else {
				g_data.final_freq = 0;
				return;
			}
			break;
		case MODE_3:
			freq_mHz = compare_nyquist(false, debug);
			if(freq_mHz > 0) {
				g_data.final_freq = freq_mHz;
				return;
			}

			freq_mHz = compare_nyquist(true, debug);
			if(freq_mHz > 0) {
				g_data.final_freq = freq_mHz;
				return;
			} else {
				g_data.cur_mode = MODE_2;
				printk(KERN_INFO "Flicker_Multi: ""DEBUG_FLICKER retry mode 2");
				return process_aliasing(debug);
			}
	}
}

void flicker_anti_aliasing_init(uint32_t data[], uint block_size)
{
	int i;

	printk(KERN_INFO "Flicker_Multi: init samplerate");
	for (i = 0; i < 3; i++) {
		g_samplerates[i].nyquist_Hz = NYQUIST_HZ(data[i]);
		g_samplerates[i].resolution = RESOLUTION(data[i], block_size);
	}
	g_data.cur_mode = MODE_1;
	g_block_size = block_size;
}
EXPORT_SYMBOL(flicker_anti_aliasing_init);

void flicker_anti_aliasing_set_index(unsigned int idx)
{
	g_data.cur_sr_index = idx;
}
EXPORT_SYMBOL(flicker_anti_aliasing_set_index);

uint nyq_folding(uint freq)
{
	uint32_t nyq_freq = g_samplerates[g_data.cur_sr_index].nyquist_Hz;
	uint32_t q = freq / nyq_freq;
	uint32_t r = freq - q * nyq_freq;

	if ((freq / nyq_freq) & 0x01) { // ODD
		return nyq_freq - r;
	} else {
		return r;
	}
}

uint32_t find_ufm(uint32_t freq, bool folding) {
	int div;
	int i;
	uint32_t ufm_freq;

	for (div = UFM_MAX; div > 0 ; --div) {
		ufm_freq = (folding)? nyq_folding(freq / div):(freq / div);
		if (ufm_freq < MIN_TEMP_FREQ)
			continue;
		for (i = valid.count - 1; i >= 0; --i) {
			if (!valid.freq[i].valid)
				continue;
 			if (valid.freq[i].freq < MIN_TEMP_FREQ)
				break;

			if (CHECK_DISTANCE(ufm_freq, valid.freq[i].freq))
				return freq / div;
			if (ufm_freq < valid.freq[i].freq)
				break;
		}
	}
	return freq;
}

int32_t process_harmonic(uint32_t fMSR, bool debug)
{
	int i;
	uint32_t t_freq;

	if (debug) {
		printk(KERN_INFO "Flicker_Multi:""fMSR %u", fMSR);
		printk(KERN_INFO "Flicker_Multi:""valid freq (cnt:%d) %d(%d) %d(%d) %d(%d) %d(%d)", valid.count,
				valid.freq[0].freq, valid.freq[0].valid, valid.freq[1].freq, valid.freq[1].valid,
				valid.freq[2].freq, valid.freq[2].valid, valid.freq[3].freq, valid.freq[3].valid);
	}
	switch(g_data.cur_mode) {
		case MODE_1:
			if (g_data.fHar_old == 0) {
				return find_ufm(fMSR, false);
			} else {
				t_freq = nyq_folding(g_data.fHar_old);
				for (i = 0; i < valid.count; i++) {
					if (!valid.freq[i].valid)
						continue;
					if (CHECK_DISTANCE(t_freq, valid.freq[i].freq))
						return g_data.fHar_old;
					if (t_freq < valid.freq[i].freq)
						break;
				}
				return fMSR;
			}
		case MODE_2:
		case MODE_3:
			return find_ufm(fMSR, true);
		default:
			return fMSR;
	}
}

#define MAGNITUDE_RATIO(result) ((result).average << 10) / ((result).max)
#define MAGNITUDE_TEMP_RATIO(result) ((result).average << 10) / ((result).temp_max)

uint32_t flicker_anti_aliasing_process(uint32_t* data, bool debug, void (*func)(int32_t*, uint, bool))
{
	static int count = 0;
	uint32_t freq = 0;
	int idx = g_data.cur_sr_index;
	enum mode_step next_mode;

	valid.freq[0] = (struct freq_t){};
	valid.count = 0;

	FFT(data, g_block_size, debug);
	find_maxfreq(data, g_block_size, debug, func);

	switch(g_data.cur_mode) {
		case MODE_2:
		case MODE_3:
			process_aliasing(debug);
			if (g_data.final_freq != 0 && multi_result_data[idx].thd_valid) {
				freq = g_data.final_freq;
				next_mode = MODE_3;
				break;
			} else {
				//next_mode = MODE_1;
				/* fall through */
			}
		case MODE_1:
			if (multi_result_data[idx].temp_thd_valid) {
				freq = multi_result_data[idx].temp_freq_mHz;
				next_mode = MODE_2;
			} else {
				freq = 0;
				if (multi_result_data[idx].thd_valid)
					next_mode = MODE_2;
				else
					next_mode = MODE_1;
			}
			break;
	}

	g_data.fHar_old = g_data.fHar;
	if (freq != 0) {
		g_data.fHar = process_harmonic(freq, (debug|| count > 13));
	} else {
	 	g_data.fHar = 0;
	}

	if (g_data.fHar != 0) {
		freq = ((g_data.fHar + 1000) / 2000) * 2;
	}
	g_data.cur_mode = next_mode;

	if (debug || count++ > 13) {
		printk(KERN_INFO "Flicker_Multi:" "final freq %d", freq);
		count = 0;
	}

	return freq;
}
EXPORT_SYMBOL(flicker_anti_aliasing_process);
MODULE_LICENSE("GPL");
