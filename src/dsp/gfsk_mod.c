#include "gfsk_mod.h"
#include "gaussian_taps.h"
#include <errno.h>
#include <string.h>
#include "interp_fir_filter.h"
#include "frequency_modulator.h"

struct gfsk_mod_t {
    interp_fir_filter *filter;
    frequency_modulator *freq_mod;

    float *temp_input;
    size_t temp_input_len;
};

int gfsk_mod_convolve(float *x, size_t x_len, float *y, size_t y_len, float **out, size_t *out_len) {
    size_t result_len = x_len + y_len - 1;
    float *result = malloc(sizeof(float) * result_len);
    if (result == NULL) {
        return -ENOMEM;
    }
    float *temp = malloc(sizeof(float) * result_len);
    if (temp == NULL) {
        return -ENOMEM;
    }
    memset(temp, 0, sizeof(float) * result_len);
    memcpy(temp, x, sizeof(float) * x_len);
    for (size_t i = 0; i < result_len; i++) {
        float sum = 0.0;
        for (int j = 0, k = i; j < y_len && k >= 0; j++, k--) {
            sum += y[j] * temp[k];
        }
        result[i] = sum;
    }
    free(temp);

    *out = result;
    *out_len = result_len;
    return 0;
}

int gfsk_mod_create(float samples_per_symbol, float sensitivity, float bt, uint32_t max_input_buffer_length, gfsk_mod **mod) {
    struct gfsk_mod_t *result = malloc(sizeof(struct gfsk_mod_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct gfsk_mod_t) {0};
    result->temp_input_len = max_input_buffer_length * 8;
    result->temp_input = malloc(sizeof(float) * result->temp_input_len);
    if (result->temp_input == NULL) {
        gfsk_mod_destroy(result);
        return -ENOMEM;
    }

    size_t gaussian_taps_len = 4 * samples_per_symbol;
    float *gaussian_taps = NULL;
    int code = gaussian_taps_create(1.0F, samples_per_symbol, bt, gaussian_taps_len, &gaussian_taps);
    if (code != 0) {
        gfsk_mod_destroy(result);
        return code;
    }
    size_t square_wave_len = (int) samples_per_symbol;
    float *square_wave = malloc(sizeof(float) * square_wave_len);
    if (square_wave == NULL) {
        free(gaussian_taps);
        gfsk_mod_destroy(result);
        return -ENOMEM;
    }
    for (size_t i = 0; i < square_wave_len; i++) {
        square_wave[i] = 1.0F;
    }

    float *taps = NULL;
    size_t taps_len = 0;
    code = gfsk_mod_convolve(gaussian_taps, gaussian_taps_len, square_wave, square_wave_len, &taps, &taps_len);
    free(gaussian_taps);
    free(square_wave);
    if (code != 0) {
        gfsk_mod_destroy(result);
        return code;
    }

    code = interp_fir_filter_create(taps, taps_len, (int) samples_per_symbol, result->temp_input_len, &result->filter);
    if (code != 0) {
        free(taps);
        gfsk_mod_destroy(result);
        return code;
    }

    code = frequency_modulator_create(sensitivity, max_input_buffer_length, &result->freq_mod);
    if (code != 0) {
        gfsk_mod_destroy(result);
        return code;
    }

    *mod = result;
    return 0;
}

void gfsk_mod_process(uint8_t *input, size_t input_len, float complex **output, size_t *output_len, gfsk_mod *mod) {
    size_t temp_index = 0;
    for (size_t i = 0; i < input_len; i++) {
        for (int j = 0; j < 8; j++) {
            int bit = (input[i] >> (7 - j)) & 1;
            if (bit == 0) {
                mod->temp_input[temp_index] = -1.0F;
            } else {
                mod->temp_input[temp_index] = 1.0F;
            }
            temp_index++;
        }
    }

    float *filtered = NULL;
    size_t filtered_len = 0;
    interp_fir_filter_process(mod->temp_input, temp_index, &filtered, &filtered_len, mod->filter);

    float complex *modulated = NULL;
    size_t modulated_len = 0;
    frequency_modulator_process(filtered, filtered_len, &modulated, &modulated_len, mod->freq_mod);

    *output = modulated;
    *output_len = modulated_len;
}

void gfsk_mod_destroy(gfsk_mod *mod) {
    if (mod == NULL) {
        return;
    }
    if (mod->temp_input != NULL) {
        free(mod->temp_input);
    }
    if (mod->filter != NULL) {
        interp_fir_filter_destroy(mod->filter);
    }
    if (mod->freq_mod != NULL) {
        frequency_modulator_destroy(mod->freq_mod);
    }
    free(mod);
}