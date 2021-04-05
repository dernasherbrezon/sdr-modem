#include <errno.h>
#include <volk/volk.h>
#include <string.h>

#include "lpf_complex.h"
#include "lpf.h"

struct lpf_complex_t {

	float **taps;
	size_t aligned_taps_len;
	size_t alignment;
	size_t taps_len;
	float *original_taps;

	float *working_buffer;
	size_t history_offset;
	size_t working_len_total;
	float complex *volk_output;

	float complex *output;
	size_t output_len;
};

int create_aligned_taps(float *original_taps, size_t taps_len, lpf_complex *filter) {
	size_t alignment = volk_get_alignment();
	size_t number_of_aligned = fmax((size_t ) 1, alignment / sizeof(float));
	// Make a set of taps at all possible alignments
	float **result = malloc(number_of_aligned * sizeof(float*));
	if (result == NULL) {
		return -1;
	}
	for (int i = 0; i < number_of_aligned; i++) {
		size_t aligned_taps_len = taps_len + number_of_aligned - 1;
		result[i] = (float*) volk_malloc(aligned_taps_len * sizeof(float), alignment);
		// some taps will be longer than original, but
		// since they contain zeros, multiplication on an input will produce 0
		// there is a tradeoff: multiply unaligned input or
		// multiply aligned input but with additional zeros
		for (size_t j = 0; j < aligned_taps_len; j++) {
			result[i][j] = 0;
		}
		for (size_t j = 0; j < taps_len; j++) {
			result[i][i + j] = original_taps[j];
		}
	}
	filter->taps = result;
	filter->aligned_taps_len = number_of_aligned;
	filter->alignment = alignment;
	return 0;
}

int lpf_complex_create(uint32_t sampling_freq, uint32_t cutoff_freq, uint32_t transition_width, size_t output_len, lpf_complex **filter) {
	struct lpf_complex_t *result = malloc(sizeof(struct lpf_complex_t));
	if (result == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*result = (struct lpf_complex_t ) { 0 };

	float *taps = NULL;
	size_t len;
	int code = create_low_pass_filter(1.0, sampling_freq, cutoff_freq, transition_width, &taps, &len);
	if (code != 0) {
		lpf_complex_destroy(result);
		return code;
	}
	result->original_taps = taps;
	result->taps_len = len;
	result->history_offset = len - 1;
	code = create_aligned_taps(taps, len, result);
	if (code != 0) {
		lpf_complex_destroy(result);
		return code;
	}
	result->working_len_total = output_len + result->history_offset;
	result->working_buffer = volk_malloc(sizeof(float complex) * result->working_len_total, result->alignment);
	if (result->working_buffer == NULL) {
		lpf_complex_destroy(result);
		return -ENOMEM;
	}
	result->output_len = output_len;
	result->output = malloc(sizeof(float complex) * output_len);
	if (result->output == NULL) {
		lpf_complex_destroy(result);
		return -ENOMEM;
	}

	result->volk_output = volk_malloc(1 * sizeof(float complex), result->alignment);
	if (result->volk_output == NULL) {
		lpf_complex_destroy(result);
		return -ENOMEM;
	}

	*filter = result;
	return 0;
}

void lpf_complex_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len, lpf_complex *filter) {
	memcpy(filter->working_buffer + filter->history_offset, input, input_len * sizeof(float complex));
	size_t i = 0;
	for (; i < input_len; i++) {
		const lv_32fc_t *buf = (const lv_32fc_t*) (filter->working_buffer + i);

		const lv_32fc_t *aligned_buffer = (const lv_32fc_t*) ((size_t) buf & ~(filter->alignment - 1));
		unsigned align_index = buf - aligned_buffer;

		volk_32fc_32f_dot_prod_32fc_a(filter->volk_output, aligned_buffer, filter->taps[align_index], filter->taps_len + align_index);
		filter->output[i] = *filter->volk_output;
	}

	*output = filter->output;
	*output_len = i;
}

int lpf_complex_destroy(lpf_complex *filter) {
	if (filter == NULL) {
		return 0;
	}
	if (filter->taps != NULL) {
		for (size_t i = 0; i < filter->aligned_taps_len; i++) {
			volk_free(filter->taps[i]);
		}
		free(filter->taps);
	}
	if (filter->original_taps != NULL) {
		free(filter->original_taps);
	}
	if (filter->output != NULL) {
		free(filter->output);
	}
	if (filter->working_buffer != NULL) {
		volk_free(filter->working_buffer);
	}
	if (filter->volk_output != NULL) {
		volk_free(filter->volk_output);
	}
	if (filter->output != NULL) {
		free(filter->output);
	}
	free(filter);
	return 0;
}

