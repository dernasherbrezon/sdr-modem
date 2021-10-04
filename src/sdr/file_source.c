#include "file_source.h"
#include "../dsp/sig_source.h"
#include <stdio.h>
#include <errno.h>

struct file_device_t {
    uint32_t id;

    FILE *rx_file;
    FILE *tx_file;

    int64_t freq_offset;
    sig_source *signal;

    float complex *output;
    size_t output_len;
};

void file_source_stop(void *plugin) {
    // do nothing
}

int file_source_create(uint32_t id, const char *rx_filename, const char *tx_filename, uint64_t sampling_freq, int64_t freq_offset, uint32_t max_output_buffer_length, sdr_device **output) {
    struct file_device_t *device = malloc(sizeof(struct file_device_t));
    if (device == NULL) {
        return -ENOMEM;
    }
    *device = (struct file_device_t) {0};
    device->id = id;
    device->freq_offset = freq_offset;
    device->output_len = max_output_buffer_length;
    device->output = malloc(sizeof(float complex) * device->output_len);
    if (device->output == NULL) {
        file_source_destroy(device);
        return -ENOMEM;
    }

    if (freq_offset != 0) {
        int code = sig_source_create(1.0F, sampling_freq, max_output_buffer_length, &device->signal);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to create signal generator\n", device->id);
            file_source_destroy(device);
            return -1;
        }
    }

    if (rx_filename != NULL) {
        device->rx_file = fopen(rx_filename, "rb");
        if (device->rx_file == NULL) {
            fprintf(stderr, "<3>[%d] unable to open file for input: %s\n", device->id, rx_filename);
            file_source_destroy(device);
            return -1;
        }
    }

    if (tx_filename != NULL) {
        device->tx_file = fopen(tx_filename, "wb");
        if (device->tx_file == NULL) {
            fprintf(stderr, "<3>[%d] unable to open file for output: %s\n", device->id, tx_filename);
            file_source_destroy(device);
            return -1;
        }
    }

    struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
    if (result == NULL) {
        file_source_destroy(device);
        return -ENOMEM;
    }
    result->plugin = device;
    result->destroy = file_source_destroy;
    result->sdr_process_rx = file_source_process_rx;
    result->sdr_process_tx = file_source_process_tx;
    result->stop_rx = file_source_stop;

    *output = result;
    return 0;
}

int file_source_process_rx(float complex **output, size_t *output_len, void *plugin) {
    file_device *device = (file_device *) plugin;
    if (device->rx_file == NULL) {
        fprintf(stderr, "<3>[%d] rx file was not initialized\n", device->id);
        *output = NULL;
        *output_len = 0;
        return -1;
    }
    size_t actually_read = fread(device->output, sizeof(float complex), device->output_len, device->rx_file);
    if (actually_read <= 0) {
        *output = NULL;
        *output_len = 0;
        return -1;
    }
    if (device->signal != NULL) {
        float complex *sig_output = NULL;
        size_t sig_output_len = 0;
        sig_source_multiply(device->freq_offset, device->output, actually_read, &sig_output, &sig_output_len, device->signal);
        *output = sig_output;
        *output_len = sig_output_len;
    } else {
        *output = device->output;
        *output_len = actually_read;
    }
    return 0;
}

int file_source_process_tx(float complex *input, size_t input_len, void *plugin) {
    file_device *device = (file_device *) plugin;
    if (input_len > device->output_len) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, device->output_len);
        return -1;
    }
    if (device->tx_file == NULL) {
        fprintf(stderr, "<3>[%d] tx file was not initialized\n", device->id);
        return -1;
    }
    if (device->signal != NULL) {
        float complex *sig_output = NULL;
        size_t sig_output_len = 0;
        sig_source_multiply(device->freq_offset, input, input_len, &sig_output, &sig_output_len, device->signal);
        //ignore actually written
        fwrite(sig_output, sizeof(float complex), sig_output_len, device->tx_file);
    } else {
        //ignore actually written
        fwrite(input, sizeof(float complex), input_len, device->tx_file);
    }
    return 0;
}

void file_source_destroy(void *plugin) {
    if (plugin == NULL) {
        return;
    }
    file_device *device = (file_device *) plugin;
    if (device->rx_file != NULL) {
        fclose(device->rx_file);
    }
    if (device->tx_file != NULL) {
        fclose(device->tx_file);
    }
    if (device->output != NULL) {
        free(device->output);
    }
    if (device->signal != NULL) {
        sig_source_destroy(device->signal);
    }
    free(device);
}