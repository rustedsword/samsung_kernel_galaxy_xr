#include <linux/types.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include "fft.h"

#define FLICKER_AVGMAX_THD		65LL
#define PSEC_PER_SEC			1000000000000LL

#define NYQUIST_HZ(sampling_time_usec) DIV_ROUND_CLOSEST(DIV_ROUND_CLOSEST(PSEC_PER_SEC, sampling_time_usec), 2);
#define RESOLUTION(sampling_time_usec, block_size) DIV_ROUND_CLOSEST(DIV_ROUND_CLOSEST(PSEC_PER_SEC, sampling_time_usec), block_size);

struct sample_rate_data {
	uint32_t nyquist_Hz;
	uint32_t resolution;
};

enum mode_step {
	MODE_1 = 1,
	MODE_2,
	MODE_3,
};

struct global_step_data {
	enum mode_step cur_mode;
	// bool is_final_temp;
	uint32_t final_freq;
	uint32_t cur_sr_index;
	uint32_t fHar_old;
	uint32_t fHar;
};

struct mode_step_data {
	uint32_t					freq_mHz;
	uint32_t					temp_freq_mHz;
	uint64_t					average;
	uint64_t					temp_average;
	uint32_t					max;
	uint32_t					temp_max;
	bool						thd_valid;
	bool						temp_thd_valid;
};

void flicker_anti_aliasing_init(uint32_t data[], uint block_size);
void flicker_anti_aliasing_set_index(unsigned int idx);
uint32_t flicker_anti_aliasing_process(uint32_t* data, bool debug, void (*func)(int32_t*, uint, bool));
