#include "iio_plugin.h"
#include <dlfcn.h>
#include <iio.h>
#include <stdio.h>
#include <errno.h>

struct iio_plugin_t {
    uint32_t id;

    struct iio_context *ctx;
    struct iio_device *tx;
    struct iio_channel *tx0_i;
    struct iio_channel *tx0_q;
    struct iio_buffer *tx_buffer;

    struct iio_device *rx;
    struct iio_channel *rx0_i;
    struct iio_channel *rx0_q;
    struct iio_buffer *rx_buffer;

    float complex *output;
    size_t output_len;

    struct stream_cfg *rx_config;
    struct stream_cfg *tx_config;
};

enum iio_direction {
    RX, TX
};

void iio_plugin_process_rx(float complex **output, size_t *output_len, iio_plugin *iio) {
    if (iio->rx_buffer == NULL) {
        fprintf(stderr, "rx was not initialized\n");
        *output = NULL;
        *output_len = 0;
        return;
    }
    ssize_t ret = iio_buffer_refill(iio->rx_buffer);
    if (ret < 0) {
        *output = NULL;
        *output_len = 0;
        return;
    }

    size_t len = 0;
    ptrdiff_t p_inc = iio_buffer_step(iio->rx_buffer);
    char *p_end = iio_buffer_end(iio->rx_buffer);
    for (char *p_dat = (char *) iio_buffer_first(iio->rx_buffer, iio->rx0_i); p_dat < p_end; p_dat += p_inc) {
        const int16_t i = ((int16_t *) p_dat)[0]; // Real (I)
        const int16_t q = ((int16_t *) p_dat)[1]; // Imag (Q)
        iio->output[len] = i / 2048.0F + I * (q / 2048.0F);
        len++;
    }

    *output = iio->output;
    *output_len = len;
}

static char tmpstr[64];

static struct iio_device *iio_get_ad9361_stream_dev(struct iio_context *ctx, enum iio_direction d) {
    switch (d) {
        case TX:
            return iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
        case RX:
            return iio_context_find_device(ctx, "cf-ad9361-lpc");
        default:
            return NULL;
    }
}

static char *iio_get_channel_name(const char *type, int id) {
    snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
    return tmpstr;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static struct iio_channel *iio_get_phy_channel(struct iio_context *ctx, enum iio_direction d, int chid) {
    switch (d) {
        case RX:
            return iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), iio_get_channel_name("voltage", chid), false);
        case TX:
            return iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), iio_get_channel_name("voltage", chid), true);
        default:
            return NULL;
    }
}

static int error_check(int v, const char *what) {
    if (v < 0) {
        fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what);
        return v;
    }
    return 0;
}

static int wr_ch_str(struct iio_channel *chn, const char *what, const char *str) {
    return error_check(iio_channel_attr_write(chn, what, str), what);
}

static int wr_ch_lli(struct iio_channel *chn, const char *what, long long val) {
    return error_check(iio_channel_attr_write_longlong(chn, what, val), what);
}

static struct iio_channel *iio_get_lo_chan(struct iio_context *ctx, enum iio_direction d) {
    switch (d) {
        // LO chan is always output, i.e. true
        case RX:
            return iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), iio_get_channel_name("altvoltage", 0), true);
        case TX:
            return iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), iio_get_channel_name("altvoltage", 1), true);
        default:
            return NULL;
    }
}

static struct iio_channel *iio_get_ad9361_stream_channel(__notused struct iio_context *ctx, enum iio_direction d, struct iio_device *dev, int chid) {
    struct iio_channel *chn = iio_device_find_channel(dev, iio_get_channel_name("voltage", chid), d == TX);
    if (chn == NULL) {
        chn = iio_device_find_channel(dev, iio_get_channel_name("altvoltage", chid), d == TX);
    }
    return chn;
}

int iio_configure_ad9361_streaming_channel(struct iio_context *ctx, struct stream_cfg *cfg, enum iio_direction type, int chid) {
    struct iio_channel *chn = iio_get_phy_channel(ctx, type, chid);
    if (chn == NULL) {
        return -1;
    }
    int code = wr_ch_str(chn, "rf_port_select", cfg->rfport);
    if (code != 0) {
        return code;
    }
    code = wr_ch_lli(chn, "rf_bandwidth", cfg->bw_hz);
    if (code != 0) {
        return code;
    }
    code = wr_ch_lli(chn, "sampling_frequency", cfg->fs_hz);
    if (code != 0) {
        return code;
    }

    chn = iio_get_lo_chan(ctx, type);
    if (chn == NULL) {
        return -1;
    }
    return wr_ch_lli(chn, "frequency", cfg->center_freq);
}

int iio_plugin_create(uint32_t id, struct stream_cfg *rx_config, struct stream_cfg *tx_config, unsigned int timeout_ms, uint32_t max_input_buffer_length, iio_plugin **output) {
    if (rx_config == NULL && tx_config == NULL) {
        fprintf(stderr, "configuration is missing\n");
        return -1;
    }
    struct iio_plugin_t *result = malloc(sizeof(struct iio_plugin_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct iio_plugin_t) {0};
    result->rx_config = rx_config;
    result->tx_config = tx_config;
    result->id = id;

    struct iio_scan_context *scan_ctx = iio_create_scan_context(NULL, 0);
    if (scan_ctx == NULL) {
        perror("unable to scan");
        iio_plugin_destroy(result);
        return -1;
    }
    struct iio_context_info **info = NULL;
    ssize_t code = iio_scan_context_get_info_list(scan_ctx, &info);
    if (code < 0) {
        fprintf(stderr, "unable to scan contexts: %zd\n", code);
        iio_scan_context_destroy(scan_ctx);
        iio_plugin_destroy(result);
        return -1;
    }
    if (code == 0) {
        fprintf(stderr, "no iio contexts found\n");
        iio_scan_context_destroy(scan_ctx);
        iio_plugin_destroy(result);
        return -1;
    }
    const char *uri = iio_context_info_get_uri(info[0]);
    fprintf(stdout, "using context uri: %s\n", uri);
    result->ctx = iio_create_context_from_uri(uri);
    iio_context_info_list_free(info);
    iio_scan_context_destroy(scan_ctx);
    if (result->ctx == NULL) {
        perror("unable to setup context");
        iio_plugin_destroy(result);
        return -1;
    }
    code = iio_context_set_timeout(result->ctx, timeout_ms);
    if (code < 0) {
        fprintf(stderr, "unable to setup timeout: %zd\n", code);
        iio_plugin_destroy(result);
        return -1;
    }

    if (tx_config != NULL) {
        result->tx = iio_get_ad9361_stream_dev(result->ctx, TX);
        if (result->tx == NULL) {
            fprintf(stderr, "unable to find tx device\n");
            iio_plugin_destroy(result);
            return -1;
        }
        code = iio_configure_ad9361_streaming_channel(result->ctx, tx_config, TX, 0);
        if (code < 0) {
            fprintf(stderr, "unable to configure tx channel: %zd\n", code);
            iio_plugin_destroy(result);
            return -1;
        }
        result->tx0_i = iio_get_ad9361_stream_channel(result->ctx, TX, result->tx, 0);
        if (result->tx0_i == NULL) {
            fprintf(stderr, "unable to find tx I channel\n");
            iio_plugin_destroy(result);
            return -1;
        }
        result->tx0_q = iio_get_ad9361_stream_channel(result->ctx, TX, result->tx, 1);
        if (result->tx0_q == NULL) {
            fprintf(stderr, "unable to find tx Q channel\n");
            iio_plugin_destroy(result);
            return -1;
        }
        iio_channel_enable(result->tx0_i);
        iio_channel_enable(result->tx0_q);
        result->tx_buffer = iio_device_create_buffer(result->tx, max_input_buffer_length, false);
        if (result->tx_buffer == NULL) {
            perror("unable to create tx buffer");
            iio_plugin_destroy(result);
            return -1;
        }
    }

    if (rx_config != NULL) {
        result->rx = iio_get_ad9361_stream_dev(result->ctx, RX);
        if (result->rx == NULL) {
            fprintf(stderr, "unable to find rx device\n");
            iio_plugin_destroy(result);
            return -1;
        }
        code = iio_configure_ad9361_streaming_channel(result->ctx, rx_config, RX, 0);
        if (code < 0) {
            fprintf(stderr, "unable to configure rx channel: %zd\n", code);
            iio_plugin_destroy(result);
            return -1;
        }
        result->rx0_i = iio_get_ad9361_stream_channel(result->ctx, RX, result->rx, 0);
        if (result->rx0_i == NULL) {
            fprintf(stderr, "unable to find rx I channel\n");
            iio_plugin_destroy(result);
            return -1;
        }
        result->rx0_q = iio_get_ad9361_stream_channel(result->ctx, RX, result->rx, 1);
        if (result->rx0_q == NULL) {
            fprintf(stderr, "unable to find rx Q channel\n");
            iio_plugin_destroy(result);
            return -1;
        }
        iio_channel_enable(result->rx0_i);
        iio_channel_enable(result->rx0_q);
        result->rx_buffer = iio_device_create_buffer(result->rx, max_input_buffer_length, false);
        if (result->rx_buffer == NULL) {
            perror("unable to create rx buffer");
            iio_plugin_destroy(result);
            return -1;
        }
        result->output_len = max_input_buffer_length;
        result->output = malloc(sizeof(float complex) * result->output_len);
        if (result->output == NULL) {
            iio_plugin_destroy(result);
            return -ENOMEM;
        }
    }

    *output = result;
    return 0;
}

void iio_plugin_destroy(iio_plugin *iio) {
    if (iio == NULL) {
        return;
    }
    if (iio->tx_buffer != NULL) {
        iio_buffer_destroy(iio->tx_buffer);
    }
    if (iio->tx0_i != NULL) {
        iio_channel_disable(iio->tx0_i);
    }
    if (iio->tx0_q != NULL) {
        iio_channel_disable(iio->tx0_q);
    }
    if (iio->rx_buffer != NULL) {
        iio_buffer_destroy(iio->rx_buffer);
    }
    if (iio->rx0_i != NULL) {
        iio_channel_disable(iio->rx0_i);
    }
    if (iio->rx0_q != NULL) {
        iio_channel_disable(iio->rx0_q);
    }
    if (iio->output != NULL) {
        free(iio->output);
    }
    if (iio->ctx != NULL) {
        iio_context_destroy(iio->ctx);
    }
    if (iio->rx_config != NULL) {
        free(iio->rx_config);
    }
    if (iio->tx_config != NULL) {
        free(iio->tx_config);
    }
    free(iio);
}
