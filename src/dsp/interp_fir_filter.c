#include "interp_fir_filter.h"
#include <errno.h>
#include <string.h>
#include "fir_filter.h"

#include <stdio.h>

struct interp_fir_filter_t {
    float *taps;
    size_t taps_len;

    fir_filter **filters;
    size_t filters_len;

    float *output;
    size_t output_len;
};

int interp_fir_filter_roundup_to_interpolation(float *taps, size_t taps_len, uint8_t interpolation, float **result_taps, size_t *result_taps_len) {
    size_t len;
    // round up length to a multiple of the interpolation factor
    size_t n = taps_len % interpolation;
    if (n > 0) {
        n = interpolation - n;
        len = taps_len + n;
    } else {
        len = taps_len;
    }

    float *result = malloc(sizeof(float) * len);
    if (result == NULL) {
        return -ENOMEM;
    }
    memset(result, 0, sizeof(float) * len);
    memcpy(result, taps, sizeof(float) * taps_len);

    *result_taps = result;
    *result_taps_len = len;
    return 0;
}

float **interp_fir_filter_create_interleaved_taps(const float *taps, size_t taps_len, uint8_t interpolation) {
    // guaranteed to be integer. see interp_fir_filter_roundup_to_interpolation
    size_t filter_taps_len = taps_len / interpolation;
    float **interleaved_taps = malloc(sizeof(float *) * interpolation);
    if (interleaved_taps == NULL) {
        return NULL;
    }
    *interleaved_taps = (float *) {0};

    //init individual taps so that they can be freed by the corresponding filter
    int result_code = 0;
    for (uint8_t i = 0; i < interpolation; i++) {
        interleaved_taps[i] = malloc(sizeof(float) * filter_taps_len);
        if (interleaved_taps[i] == NULL) {
            result_code = -ENOMEM;
        }
    }
    if (result_code != 0) {
        for (uint8_t i = 0; i < interpolation; i++) {
            if (interleaved_taps[i] != NULL) {
                free(interleaved_taps[i]);
            }
        }
        free(interleaved_taps);
        return NULL;
    }

    for (size_t i = 0; i < taps_len; i++) {
        interleaved_taps[i % interpolation][i / interpolation] = taps[i];
    }
    return interleaved_taps;
}

int interp_fir_filter_create(float *taps, size_t taps_len, uint8_t interpolation, uint32_t max_input_buffer_length, interp_fir_filter **filter) {
    struct interp_fir_filter_t *result = malloc(sizeof(struct interp_fir_filter_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct interp_fir_filter_t) {0};

    result->output_len = max_input_buffer_length * interpolation;
    result->output = malloc(sizeof(float) * result->output_len);
    if (result->output == NULL) {
        interp_fir_filter_destroy(result);
        return -ENOMEM;
    }

    int code = interp_fir_filter_roundup_to_interpolation(taps, taps_len, interpolation, &result->taps, &result->taps_len);
    if (code != 0) {
        interp_fir_filter_destroy(result);
        return code;
    }

    float **interleaved_taps = interp_fir_filter_create_interleaved_taps(result->taps, result->taps_len, interpolation);
    if (interleaved_taps == NULL) {
        interp_fir_filter_destroy(result);
        return -ENOMEM;
    }

    result->filters_len = interpolation;
    result->filters = malloc(sizeof(fir_filter *) * result->filters_len);
    if (result->filters == NULL) {
        for (uint8_t i = 0; i < interpolation; i++) {
            free(interleaved_taps[i]);
        }
        free(interleaved_taps);
        interp_fir_filter_destroy(result);
        return -ENOMEM;
    }

    int result_code = 0;
    size_t filter_taps_len = result->taps_len / interpolation;
    for (size_t i = 0; i < result->filters_len; i++) {
        fir_filter *curFilter = NULL;
        code = fir_filter_create(1, interleaved_taps[i], filter_taps_len, max_input_buffer_length, sizeof(float), &curFilter);
        //init all filters anyway
        if (code != 0) {
            free(interleaved_taps[i]);
            result_code = code;
            result->filters[i] = NULL;
        } else {
            result->filters[i] = curFilter;
        }
    }
    //release array of pointers and not taps themselves
    free(interleaved_taps);
    if (result_code != 0) {
        interp_fir_filter_destroy(result);
        return result_code;
    }

    free(taps);
    *filter = result;
    return 0;
}

void interp_fir_filter_process(float *input, size_t input_len, float **output, size_t *output_len, interp_fir_filter *filter) {
    size_t result_len = 0;
    for (size_t i = 0; i < filter->filters_len; i++) {
        float *cur_output = NULL;
        size_t cur_output_len = 0;
        fir_filter_process(input, input_len, (void **) &cur_output, &cur_output_len, filter->filters[i]);
        result_len = filter->filters_len * cur_output_len;
        //de-interleave results
        for (size_t j = i, k = 0; j < result_len && k < cur_output_len; j += filter->filters_len, k++) {
            filter->output[j] = cur_output[k];
        }
    }

    *output_len = result_len;
    *output = filter->output;
}

void interp_fir_filter_destroy(interp_fir_filter *filter) {
    if (filter == NULL) {
        return;
    }
    if (filter->taps != NULL) {
        free(filter->taps);
    }
    if (filter->filters != NULL) {
        for (size_t i = 0; i < filter->filters_len; i++) {
            fir_filter_destroy(filter->filters[i]);
        }
        free(filter->filters);
    }
    if (filter->output != NULL) {
        free(filter->output);
    }
    free(filter);
}