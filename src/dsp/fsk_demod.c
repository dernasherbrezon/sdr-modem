#include "fsk_demod.h"
#include "quadrature_demod.h"
#include "clock_recovery_mm.h"
#include "dc_blocker.h"
#include <math.h>
#include <errno.h>

#include "lpf.h"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

struct fsk_demod_t {

	lpf *lpf1;
	lpf *lpf2;
	quadrature_demod *quad_demod;
	dc_blocker *dc;
	clock_mm *clock;

	int8_t *output;
	size_t output_len;
};

int create_fsk_demod(uint32_t sampling_freq, int baud_rate, float deviation, int decimation, uint32_t transition_width, bool use_dc_block, uint32_t max_input_buffer_length, fsk_demod **demod) {
	struct fsk_demod_t *result = malloc(sizeof(struct fsk_demod_t));
	if (result == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*result = (struct fsk_demod_t ) { 0 };

	float carson_cutoff = fabs(deviation) + baud_rate / 2.0f;
	int code = lpf_create(sampling_freq, carson_cutoff, 0.1 * carson_cutoff, max_input_buffer_length, sizeof(float complex), &result->lpf1);
	if (code != 0) {
		destroy_fsk_demod(result);
		return code;
	}
	code = quadrature_demod_create((sampling_freq / (2 * M_PI * deviation)), max_input_buffer_length, &result->quad_demod);
	if (code != 0) {
		destroy_fsk_demod(result);
		return code;
	}
	code = lpf_create(sampling_freq, (float) baud_rate / 2, transition_width, max_input_buffer_length, sizeof(float), &result->lpf2);
	if (code != 0) {
		destroy_fsk_demod(result);
		return code;
	}

	float sps = sampling_freq / (float) baud_rate;

	if (use_dc_block) {
		code = dc_blocker_create(ceilf(sps * 32), &result->dc);
		if (code != 0) {
			destroy_fsk_demod(result);
			return code;
		}
	}

	code = clock_mm_create(sps, (sps * M_PI) / 100, 0.5f, 0.5f / 8.0f, 0.01f, &result->clock);
	if (code != 0) {
		destroy_fsk_demod(result);
		return code;
	}

	result->output_len = max_input_buffer_length;
	result->output = malloc(sizeof(int8_t) * result->output_len);
	if (result->output == NULL) {
		destroy_fsk_demod(result);
		return -ENOMEM;
	}

	return 0;
}

int8_t fsk_clamp(int8_t d, int8_t min, int8_t max) {
	const int8_t t = d < min ? min : d;
	return t > max ? max : t;
}

void fsk_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, fsk_demod *demod) {
	float complex *lpf_output = NULL;
	size_t lpf_output_len = 0;
	lpf_process(input, input_len, (void**) &lpf_output, &lpf_output_len, demod->lpf1);

	float *qd_output = NULL;
	size_t qd_output_len = 0;
	quadrature_demod_process(lpf_output, lpf_output_len, &qd_output, &qd_output_len, demod->quad_demod);

	float *lpf2_output = NULL;
	size_t lpf2_output_len = 0;
	lpf_process(qd_output, qd_output_len, (void**) &lpf2_output, &lpf2_output_len, demod->lpf2);

	float *dc_output = NULL;
	size_t dc_output_len = 0;
	if (demod->dc != NULL) {
		dc_blocker_process(lpf2_output, lpf2_output_len, &dc_output, &dc_output_len, demod->dc);
	} else {
		dc_output = qd_output;
		dc_output_len = qd_output_len;
	}

	float *clock_output = NULL;
	size_t clock_output_len = 0;
	clock_mm_process(dc_output, dc_output_len, &clock_output, &clock_output_len, demod->clock);

	for (size_t i = 0; i < clock_output_len; i++) {
		demod->output[i] = fsk_clamp((int8_t) (clock_output[i] * 127), -128, 127);
	}

	*output = demod->output;
	*output_len = clock_output_len;
}

int destroy_fsk_demod(fsk_demod *demod) {
	if (demod == NULL) {
		return 0;
	}
	if (demod->lpf1 != NULL) {
		lpf_destroy(demod->lpf1);
	}
	if (demod->quad_demod != NULL) {
		quadrature_demod_destroy(demod->quad_demod);
	}
	if (demod->lpf2 != NULL) {
		lpf_destroy(demod->lpf2);
	}
	if (demod->dc != NULL) {
		dc_blocker_destroy(demod->dc);
	}
	if (demod->clock != NULL) {
		clock_mm_destroy(demod->clock);
	}
	if (demod->output != NULL) {
		free(demod->output);
	}
	return 0;
}
