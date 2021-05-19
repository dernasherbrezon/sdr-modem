#include "gfsk_mod.h"
#include <errno.h>

struct gfsk_mod_t {
    float *temp_input;
    size_t temp_input_len;
};

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
}

void gfsk_mod_destroy(gfsk_mod *mod) {
    if (mod == NULL) {
        return;
    }
    if (mod->temp_input != NULL) {
        free(mod->temp_input);
    }
    free(mod);
}