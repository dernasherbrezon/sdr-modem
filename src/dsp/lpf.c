#include "lpf.h"
#include <errno.h>
#include <volk/volk.h>
#include <string.h>
#include "fir_filter.h"
#include "lpf_taps.h"

struct lpf_t {
    fir_filter *filter;
};

int lpf_create(uint8_t decimation, uint32_t sampling_freq, uint32_t cutoff_freq, uint32_t transition_width,
               size_t max_input_buffer_length, size_t num_bytes, lpf **filter) {
    struct lpf_t *result = malloc(sizeof(struct lpf_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct lpf_t) {0};

    float *taps = NULL;
    size_t taps_len = 0;
    int code = create_low_pass_filter(1.0F, sampling_freq, cutoff_freq, transition_width, &taps, &taps_len);
    if (code != 0) {
        lpf_destroy(result);
        return code;
    }
    code = fir_filter_create(decimation, taps, taps_len, max_input_buffer_length, num_bytes, &result->filter);
    if (code != 0) {
        lpf_destroy(result);
        return code;
    }
    *filter = result;
    return 0;
}


void lpf_process(const void *input, size_t input_len, void **output, size_t *output_len, lpf *filter) {
    fir_filter_process(input, input_len, output, output_len, filter->filter);
}

void lpf_destroy(lpf *filter) {
    if (filter == NULL) {
        return;
    }
    if (filter->filter != NULL) {
        fir_filter_destroy(filter->filter);
    }
    free(filter);
}

