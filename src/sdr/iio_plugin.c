#include "iio_plugin.h"
#include <dlfcn.h>
#include <iio.h>
#include <stdio.h>
#include <errno.h>
#include <volk/volk.h>
#include <stdbool.h>

#define FIR_BUF_SIZE    8192

static char tmpstr[256];
static const size_t tmpstr_len = 256;
static const uint32_t MIN_NO_FIR_FILTER = (uint32_t) ((double) 25000000 / 12 + 1);
static const uint32_t MIN_FIR_FILTER_2 = MIN_NO_FIR_FILTER / 2 + 1;
static const uint32_t MIN_FIR_FILTER = MIN_NO_FIR_FILTER / 4 + 1;
static int16_t fir_128_4[] = {
        -15, -27, -23, -6, 17, 33, 31, 9, -23, -47, -45, -13, 34, 69, 67, 21, -49, -102, -99, -32, 69, 146, 143, 48, -96, -204, -200, -69, 129, 278, 275, 97, -170,
        -372, -371, -135, 222, 494, 497, 187, -288, -654, -665, -258, 376, 875, 902, 363, -500, -1201, -1265, -530, 699, 1748, 1906, 845, -1089, -2922, -3424,
        -1697, 2326, 7714, 12821, 15921, 15921, 12821, 7714, 2326, -1697, -3424, -2922, -1089, 845, 1906, 1748, 699, -530, -1265, -1201, -500, 363, 902, 875,
        376, -258, -665, -654, -288, 187, 497, 494, 222, -135, -371, -372, -170, 97, 275, 278, 129, -69, -200, -204, -96, 48, 143, 146, 69, -32, -99, -102, -49, 21,
        67, 69, 34, -13, -45, -47, -23, 9, 31, 33, 17, -6, -23, -27, -15};

static int16_t fir_128_2[] = {
        -0, 0, 1, -0, -2, 0, 3, -0, -5, 0, 8, -0, -11, 0, 17, -0, -24, 0, 33, -0, -45, 0, 61, -0, -80, 0, 104, -0, -134, 0, 169, -0,
        -213, 0, 264, -0, -327, 0, 401, -0, -489, 0, 595, -0, -724, 0, 880, -0, -1075, 0, 1323, -0, -1652, 0, 2114, -0, -2819, 0, 4056, -0, -6883, 0, 20837, 32767,
        20837, 0, -6883, -0, 4056, 0, -2819, -0, 2114, 0, -1652, -0, 1323, 0, -1075, -0, 880, 0, -724, -0, 595, 0, -489, -0, 401, 0, -327, -0, 264, 0, -213, -0,
        169, 0, -134, -0, 104, 0, -80, -0, 61, 0, -45, -0, 33, 0, -24, -0, 17, 0, -11, -0, 8, 0, -5, -0, 3, 0, -2, -0, 1, 0, -0, 0};

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

static inline float clip(float i) {
    if (i > 1.0) {
        i = 1.0F;
    } else if (i < -1.0) {
        i = -1.0F;
    }
    return i;
}

int iio_plugin_process_tx(float complex *input, size_t input_len, iio_plugin *iio) {
    if (iio->tx_buffer == NULL) {
        fprintf(stderr, "tx was not initialized\n");
        return -1;
    }
    ptrdiff_t p_inc = iio_buffer_step(iio->tx_buffer);
    char *p_end = iio_buffer_end(iio->tx_buffer);
    size_t cur_index = 0;
    for (char *p_dat = (char *) iio_buffer_first(iio->tx_buffer, iio->tx0_i); p_dat < p_end && cur_index < input_len; p_dat += p_inc, cur_index++) {
        float i = clip(crealf(input[cur_index]));
        float q = clip(cimagf(input[cur_index]));
        // 12-bit sample needs to be MSB aligned so shift by 4
        ((int16_t *) p_dat)[0] = (int16_t) (i * 2048) << 4; // Real (I)
        ((int16_t *) p_dat)[1] = (int16_t) (q * 2048) << 4; // Imag (Q)
    }
    ssize_t nbytes_tx = iio_buffer_push(iio->tx_buffer);
    if (nbytes_tx < 0) {
        return -1;
    }
    return 0;
}

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

    char *p_end = iio_buffer_end(iio->rx_buffer);
    char *p_dat = (char *) iio_buffer_first(iio->rx_buffer, iio->rx0_i);
    size_t num_points = (p_end - p_dat) / sizeof(int16_t);
    // ADC is 12bit, thus 2^12 = 2048
    volk_16i_s32f_convert_32f((float *) iio->output, (const int16_t *) p_dat, 2048.0F, num_points);
    *output = iio->output;
    *output_len = num_points / 2;
}

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
        iio_strerror(-v, tmpstr, tmpstr_len);
        fprintf(stderr, "unable to write value for \"%s\": %s\n", what, tmpstr);
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

int ad9361_set_trx_fir_enable(struct iio_device *dev, int enable) {
    int ret = iio_device_attr_write_bool(dev, "in_out_voltage_filter_fir_en", !!enable);
    if (ret < 0) {
        ret = iio_channel_attr_write_bool(iio_device_find_channel(dev, "out", false), "voltage_filter_fir_en", !!enable);
    }
    return ret;
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
    int code = wr_ch_lli(chn, "rf_bandwidth", cfg->sampling_freq);
    if (code != 0) {
        return code;
    }
    code = wr_ch_lli(chn, "sampling_frequency", cfg->sampling_freq);
    if (code != 0) {
        return code;
    }
    switch (cfg->gain_control_mode) {
        case IIO_GAIN_MODE_MANUAL:
            if (type == RX) {
                code = wr_ch_str(chn, "gain_control_mode", "manual");
            }
            if (code == 0) {
                code = error_check(iio_channel_attr_write_double(chn, "hardwaregain", cfg->manual_gain), "hardwaregain");
            }
            break;
        case IIO_GAIN_MODE_FAST_ATTACK:
            code = wr_ch_str(chn, "gain_control_mode", "fast_attack");
            break;
        case IIO_GAIN_MODE_SLOW_ATTACK:
            code = wr_ch_str(chn, "gain_control_mode", "slow_attack");
            break;
        case IIO_GAIN_MODE_HYBRID:
            code = wr_ch_str(chn, "gain_control_mode", "hybrid");
            break;
        default:
            fprintf(stderr, "unknown gain mode: %d\n", cfg->gain_control_mode);
            code = -1;
            break;
    }
    if (code != 0) {
        return code;
    }

    chn = iio_get_lo_chan(ctx, type);
    if (chn == NULL) {
        return -1;
    }
    return wr_ch_lli(chn, "frequency", cfg->center_freq);
}

int iio_plugin_select_fir_filter_config(struct stream_cfg *cfg, int *decimation, int16_t **fir_filter_taps) {
    if (cfg == NULL) {
        *decimation = 0;
        *fir_filter_taps = NULL;
        return 0;
    }
    if (cfg->sampling_freq < MIN_FIR_FILTER) {
        fprintf(stderr, "sampling freq is too low: %u\n", cfg->sampling_freq);
        return -1;
    } else if (cfg->sampling_freq < MIN_FIR_FILTER_2) {
        *decimation = 4;
        *fir_filter_taps = fir_128_4;
    } else if (cfg->sampling_freq < MIN_NO_FIR_FILTER) {
        *decimation = 2;
        *fir_filter_taps = fir_128_2;
    }
    return 0;
}

int iio_plugin_setup_fir_filter(struct iio_context *ctx, struct stream_cfg *rx_config, struct stream_cfg *tx_config) {
    int rx_decimation = 0;
    int16_t *rx_fir_filter_taps = NULL;
    int code = iio_plugin_select_fir_filter_config(rx_config, &rx_decimation, &rx_fir_filter_taps);
    if (code < 0) {
        return code;
    }
    int tx_decimation = 0;
    int16_t *tx_fir_filter_taps = NULL;
    code = iio_plugin_select_fir_filter_config(tx_config, &tx_decimation, &tx_fir_filter_taps);
    if (code < 0) {
        return code;
    }

    struct iio_device *phy_device = iio_context_find_device(ctx, "ad9361-phy");

    // filter is not needed
    if (rx_fir_filter_taps == NULL && tx_fir_filter_taps == NULL) {
        // filter might be configured prior to execution. disable it to support higher rates
        return error_check(ad9361_set_trx_fir_enable(phy_device, false), "in_out_voltage_filter_fir_en");
    }

    // just to simplify the code below a bit
    if (rx_fir_filter_taps != NULL && tx_fir_filter_taps == NULL) {
        tx_fir_filter_taps = rx_fir_filter_taps;
        tx_decimation = rx_decimation;
    }
    if (rx_fir_filter_taps == NULL && tx_fir_filter_taps != NULL) {
        rx_fir_filter_taps = tx_fir_filter_taps;
        rx_decimation = tx_decimation;
    }

    if (tx_decimation != rx_decimation) {
        fprintf(stderr, "rx and tx should be in the same sampling freq band. rx: %d, tx: %d\n", rx_decimation, tx_decimation);
        return -1;
    }

    char *buf = malloc(FIR_BUF_SIZE);
    if (buf == NULL) {
        return -ENOMEM;
    }
    int len = 0;
    if (rx_decimation > 0) {
        len += snprintf(buf + len, FIR_BUF_SIZE - len, "RX 3 GAIN -6 DEC %d\n", rx_decimation);
    }
    if (tx_decimation > 0) {
        len += snprintf(buf + len, FIR_BUF_SIZE - len, "TX 3 GAIN 0 INT %d\n", tx_decimation);
    }
    for (int i = 0; i < 128; i++) {
        len += snprintf(buf + len, FIR_BUF_SIZE - len, "%d,%d\n", tx_fir_filter_taps[i], rx_fir_filter_taps[i]);
    }
    len += snprintf(buf + len, FIR_BUF_SIZE - len, "\n");

    code = error_check(iio_device_attr_write_raw(phy_device, "filter_fir_config", buf, len), "filter_fir_config");
    free(buf);
    if (code < 0) {
        return code;
    }

    code = error_check(ad9361_set_trx_fir_enable(phy_device, true), "in_out_voltage_filter_fir_en");
    if (code < 0) {
        return code;
    }

    return 0;
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

    code = iio_plugin_setup_fir_filter(result->ctx, rx_config, tx_config);
    if (code < 0) {
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
