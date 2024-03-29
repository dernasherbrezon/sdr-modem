#include "fir_filter.h"
#include <errno.h>
#include <volk/volk.h>
#include <string.h>
#include <complex.h>
#include <stdio.h>

int create_aligned_taps(const float *original_taps, size_t taps_len, fir_filter *filter) {
    size_t number_of_aligned = fmax((size_t) 1, (float) filter->alignment / sizeof(float));
    // Make a set of taps at all possible alignments
    float **result = malloc(number_of_aligned * sizeof(float *));
    if (result == NULL) {
        return -1;
    }
    for (int i = 0; i < number_of_aligned; i++) {
        size_t aligned_taps_len = taps_len + number_of_aligned - 1;
        result[i] = (float *) volk_malloc(aligned_taps_len * sizeof(float), filter->alignment);
        // some taps will be longer than original, but
        // since they contain zeros, multiplication on an input will produce 0
        // there is a tradeoff: multiply unaligned input or
        // multiply aligned input but with additional zeros
        for (size_t j = 0; j < aligned_taps_len; j++) {
            result[i][j] = 0;
        }
        for (size_t j = 0; j < taps_len; j++) {
            //reverse original taps
            result[i][i + j] = original_taps[taps_len - j - 1];
        }
    }
    filter->taps = result;
    filter->aligned_taps_len = number_of_aligned;
    return 0;
}

int fir_filter_create(uint8_t decimation, float *taps, size_t taps_len,
                      size_t max_input_buffer_length, size_t num_bytes, fir_filter **filter) {
    struct fir_filter_t *result = malloc(sizeof(struct fir_filter_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct fir_filter_t) {0};

    char *alignment_override = getenv("VOLK_ALIGNMENT");
    if (alignment_override != NULL) {
        result->alignment = strtol(alignment_override, (char **) NULL, 10);
        if (errno == ERANGE || result->alignment == 0) {
            fprintf(stderr, "<3>invalid VOLK_ALIGNMENT specified: %s\n", alignment_override);
            fir_filter_destroy(result);
            return -1;
        }
    } else {
        result->alignment = volk_get_alignment();
    }

    result->decimation = decimation;
    result->num_bytes = num_bytes;
    result->original_taps = taps;
    result->taps_len = taps_len;
    result->history_offset = taps_len - 1;
    int code = create_aligned_taps(taps, taps_len, result);
    if (code != 0) {
        fir_filter_destroy(result);
        return code;
    }
    result->max_input_buffer_length = max_input_buffer_length;
    result->working_len_total = max_input_buffer_length + result->history_offset;
    result->working_buffer = volk_malloc(num_bytes * result->working_len_total, result->alignment);
    if (result->working_buffer == NULL) {
        fir_filter_destroy(result);
        return -ENOMEM;
    }
    memset(result->working_buffer, 0, result->working_len_total * num_bytes);

    // +1 for case when round-up needed.
    result->output_len = max_input_buffer_length / decimation + 1;
    result->output = malloc(num_bytes * max_input_buffer_length);
    if (result->output == NULL) {
        fir_filter_destroy(result);
        return -ENOMEM;
    }

    result->volk_output = volk_malloc(1 * num_bytes, result->alignment);
    if (result->volk_output == NULL) {
        fir_filter_destroy(result);
        return -ENOMEM;
    }

    *filter = result;
    return 0;
}

void fir_filter_process_float(const float *input, size_t input_len, float *working_buffer, void **output, size_t *output_len,
                              fir_filter *filter) {
    memcpy(working_buffer + filter->history_offset, input, input_len * filter->num_bytes);
    size_t working_len = filter->history_offset + input_len;
    size_t i = 0;
    size_t produced = 0;
    float *output_pointer = (float *) filter->output;
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; i < max_index; i += filter->decimation, produced++) {
        volk_32f_x2_dot_prod_32f_u(filter->volk_output, working_buffer + i, filter->taps[0],
                                   filter->taps_len);
        *output_pointer = *(float *) filter ->volk_output;
        output_pointer++;
    }
    filter->history_offset = working_len - i;
    if (i > 0) {
        memmove(working_buffer, working_buffer + i, filter->num_bytes * filter->history_offset);
    }

    *output = filter->output;
    *output_len = produced;
}

float fir_filter_process_float_single(const float *input, fir_filter *filter) {
    const float *aligned_buffer = (const float *) ((size_t) input & ~(filter->alignment - 1));
    size_t align_index = input - aligned_buffer;
    volk_32f_x2_dot_prod_32f_a(filter->volk_output, aligned_buffer, filter->taps[align_index], (unsigned int) (filter->taps_len + align_index));
    return *(float *) filter->volk_output;
}

void fir_filter_process_complex(const float complex *input, size_t input_len, float complex *working_buffer, void **output,
                                size_t *output_len, fir_filter *filter) {
    memcpy(working_buffer + filter->history_offset, input, input_len * filter->num_bytes);
    size_t working_len = filter->history_offset + input_len;
    size_t i = 0;
    size_t produced = 0;
    float complex *output_pointer = (float complex *) filter->output;
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; i < max_index; i += filter->decimation, produced++) {
        volk_32fc_32f_dot_prod_32fc_u(filter->volk_output, working_buffer + i, filter->taps[0],
                                      filter->taps_len);
        *output_pointer = *(float complex *) filter->volk_output;
        output_pointer++;
    }
    filter->history_offset = working_len - i;
    if (i > 0) {
        memmove(working_buffer, working_buffer + i, filter->num_bytes * filter->history_offset);
    }

    *output = filter->output;
    *output_len = produced;
}

void fir_filter_process(const void *input, size_t input_len, void **output, size_t *output_len, fir_filter *filter) {
    if (input_len > filter->max_input_buffer_length) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, filter->max_input_buffer_length);
        *output = NULL;
        *output_len = 0;
        return;
    }
    if (filter->num_bytes == sizeof(float complex)) {
        fir_filter_process_complex((const float complex *) input, input_len, (float complex *) filter->working_buffer, output,
                                   output_len, filter);
    } else if (filter->num_bytes == sizeof(float)) {
        fir_filter_process_float((const float *) input, input_len, (float *) filter->working_buffer, output, output_len, filter);
    }
}

void fir_filter_destroy(fir_filter *filter) {
    if (filter == NULL) {
        return;
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
    free(filter);
}

