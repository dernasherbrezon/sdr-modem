#include "plutosdr.h"
#include <iio.h>
#include <stdio.h>
#include <errno.h>
#include <volk/volk.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <inttypes.h>

#define FIR_BUF_SIZE    8192

static char plutosdr_tmpstr[256];
static const size_t plutosdr_tmpstr_len = 256;
static const uint32_t MIN_NO_FIR_FILTER = 2083334; // ((double) 25000000 / 12 + 1)
static const uint32_t MIN_FIR_FILTER_2 = 1041667; // MIN_NO_FIR_FILTER / 2;
static const uint32_t MIN_FIR_FILTER = 520834; // MIN_NO_FIR_FILTER / 4 + 1;
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

struct plutosdr_t {
    uint32_t id;

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

    iio_lib *lib;
    atomic_bool rx_is_running;
};

struct iio_context *global_iio_ctx = NULL;
int global_iio_ctx_usages = 0;
pthread_mutex_t global_iio_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

enum iio_direction {
    RX, TX
};

int plutosdr_process_tx(float complex *input, size_t input_len, void *plugin) {
    plutosdr *iio = (plutosdr *) plugin;
    if (iio->tx_buffer == NULL) {
        fprintf(stderr, "tx was not initialized\n");
        return -1;
    }

    size_t processed = 0;
    size_t left = input_len;
    while (left > 0) {
        char *p_start = (char *) iio->lib->iio_buffer_first(iio->tx_buffer, iio->tx0_i);
        size_t batch;
        if (left > iio->output_len / 2) {
            batch = iio->output_len / 2;
        } else {
            batch = left;
        }

        // put [-1;1] into 16bit interval [-32768;32768]
        // pluto has 16bit DAC for TX
        volk_32f_s32f_convert_16i((int16_t *) p_start, (const float *) (input + processed), 32768, batch * 2);

        // always use push_partial because normal push won't reset buffer to full length
        // https://github.com/analogdevicesinc/libiio/blob/e65a97863c3481f30c6ea8642bda86125a7ee39d/buffer.c#L154
        ssize_t nbytes_tx = iio->lib->iio_buffer_push_partial(iio->tx_buffer, batch);
        if (nbytes_tx < 0) {
            return -1;
        }

        left -= batch;
        processed += batch;
    }

    return 0;
}

int plutosdr_process_rx(float complex **output, size_t *output_len, void *plugin) {
    plutosdr *iio = (plutosdr *) plugin;
    if (!iio->rx_is_running) {
        *output = NULL;
        *output_len = 0;
        return -1;
    }
    if (iio->rx_buffer == NULL) {
        fprintf(stderr, "rx was not initialized\n");
        *output = NULL;
        *output_len = 0;
        return -1;
    }
    ssize_t ret = iio->lib->iio_buffer_refill(iio->rx_buffer);
    if (ret < 0) {
        *output = NULL;
        *output_len = 0;
        return -1;
    }

    char *p_end = iio->lib->iio_buffer_end(iio->rx_buffer);
    char *p_start = (char *) iio->lib->iio_buffer_first(iio->rx_buffer, iio->rx0_i);
    size_t num_points = (p_end - p_start) / sizeof(int16_t);
    if (num_points / 2 > iio->output_len) {
        fprintf(stderr, "<3>input buffer %zu is more than max: %zu\n", num_points / 2, iio->output_len);
        *output = NULL;
        *output_len = 0;
        return -1;
    }
    // ADC is 12bit, thus 2^12 = 2048
    volk_16i_s32f_convert_32f((float *) iio->output, (const int16_t *) p_start, 2048.0F, num_points);
    *output = iio->output;
    *output_len = num_points / 2;
    return 0;
}

static struct iio_device *plutosdr_find_device(struct iio_context *ctx, enum iio_direction d, plutosdr *iio) {
    switch (d) {
        case TX:
            return iio->lib->iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
        case RX:
            return iio->lib->iio_context_find_device(ctx, "cf-ad9361-lpc");
        default:
            return NULL;
    }
}

// pluto can generate tones at certain frequencies, phases and amplitudes.
// they are controlled by the TX1_I_F1/TX1_Q_F1/TX1_I_F2/TX1_Q_F2
// these parameters can be saved into pluto's persistent memory and accidentally loaded
// when the data transmitted by sdr-modem, it can be affected by the DDS tones, thus DDS tones must be disabled
int plutosdr_disable_dds(plutosdr *pluto) {
    // normally disabling just TX1_I_F1 would disable all other DDS channels
    // but I disable all just in case
    struct iio_channel *channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_I_F1", true);
    if (channel == NULL) {
        return -1;
    }
    int code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
    if (code < 0) {
        return code;
    }
    channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_Q_F1", true);
    if (channel == NULL) {
        return -1;
    }
    code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
    if (code < 0) {
        return code;
    }
    channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_Q_F2", true);
    if (channel == NULL) {
        return -1;
    }
    code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
    if (code < 0) {
        return code;
    }
    channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_I_F2", true);
    if (channel == NULL) {
        return -1;
    }
    code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
    if (code < 0) {
        return code;
    }
    return 0;
}

static char *plutosdr_format_channel_name(const char *type, int id) {
    snprintf(plutosdr_tmpstr, sizeof(plutosdr_tmpstr), "%s%d", type, id);
    return plutosdr_tmpstr;
}

static struct iio_channel *plutosdr_find_phy_channel(struct iio_context *ctx, enum iio_direction d, const char *channel_name, plutosdr *iio) {
    switch (d) {
        case RX:
            return iio->lib->iio_device_find_channel(iio->lib->iio_context_find_device(ctx, "ad9361-phy"), channel_name, false);
        case TX:
            return iio->lib->iio_device_find_channel(iio->lib->iio_context_find_device(ctx, "ad9361-phy"), channel_name, true);
        default:
            return NULL;
    }
}

static int plutosdr_error_check(int v, const char *what, plutosdr *iio) {
    if (v < 0) {
        iio->lib->iio_strerror(-v, plutosdr_tmpstr, plutosdr_tmpstr_len);
        fprintf(stderr, "unable to write value for \"%s\": %s\n", what, plutosdr_tmpstr);
        return v;
    }
    return 0;
}

static int plutosdr_write_str(struct iio_channel *chn, const char *what, const char *str, plutosdr *iio) {
    return plutosdr_error_check(iio->lib->iio_channel_attr_write(chn, what, str), what, iio);
}

static int plutosdr_write_lli(struct iio_channel *chn, const char *what, long long val, plutosdr *iio) {
    return plutosdr_error_check(iio->lib->iio_channel_attr_write_longlong(chn, what, val), what, iio);
}

int plutosdr_enable_fir_filter(struct iio_device *dev, bool enable, plutosdr *pluto) {
    int ret = pluto->lib->iio_device_attr_write_bool(dev, "in_out_voltage_filter_fir_en", enable);
    if (ret < 0) {
        ret = pluto->lib->iio_channel_attr_write_bool(pluto->lib->iio_device_find_channel(dev, "out", false), "voltage_filter_fir_en", enable);
    }
    return ret;
}

static struct iio_channel *plutosdr_find_lo_channel(struct iio_context *ctx, enum iio_direction d, plutosdr *pluto) {
    switch (d) {
        // LO chan is always output, i.e. true
        case RX:
            return pluto->lib->iio_device_find_channel(pluto->lib->iio_context_find_device(ctx, "ad9361-phy"), "altvoltage0", true);
        case TX:
            return pluto->lib->iio_device_find_channel(pluto->lib->iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true);
        default:
            return NULL;
    }
}

static struct iio_channel *plutosdr_find_streaming_channel(enum iio_direction d, struct iio_device *dev, int chid, plutosdr *pluto) {
    return pluto->lib->iio_device_find_channel(dev, plutosdr_format_channel_name("voltage", chid), d == TX);
}

int plutosdr_configure_streaming_channel(struct iio_context *ctx, bool rx_only, struct stream_cfg *cfg, enum iio_direction type, const char *channel_name, plutosdr *iio) {
    struct iio_channel *chn = plutosdr_find_lo_channel(ctx, type, iio);
    if (chn == NULL) {
        return -1;
    }

    if (rx_only && type == RX) {
        // completely disabling TX when doing RX only will significantly improve sensitivity
        // details: https://wiki.analog.com/university/tools/pluto/hacking/listening_to_yourself
        struct iio_channel *lo_channel = plutosdr_find_lo_channel(global_iio_ctx, TX, iio);
        if (lo_channel != NULL) {
            plutosdr_write_lli(lo_channel, "powerdown", 1, iio);
        }
    }
    if (type == TX) {
        plutosdr_write_lli(chn, "powerdown", 0, iio);
    }

    int code = plutosdr_write_lli(chn, "frequency", (long long) cfg->center_freq, iio);
    if (code != 0) {
        return code;
    }

    chn = plutosdr_find_phy_channel(ctx, type, channel_name, iio);
    if (chn == NULL) {
        return -1;
    }
    code = plutosdr_write_lli(chn, "rf_bandwidth", (long long) cfg->sampling_freq, iio);
    if (code != 0) {
        return code;
    }
    code = plutosdr_write_lli(chn, "sampling_frequency", (long long) cfg->sampling_freq, iio);
    if (code != 0) {
        return code;
    }
    switch (cfg->gain_control_mode) {
        case IIO_GAIN_MODE_MANUAL:
            if (type == RX) {
                code = plutosdr_write_str(chn, "gain_control_mode", "manual", iio);
            }
            if (code == 0) {
                code = plutosdr_error_check(iio->lib->iio_channel_attr_write_double(chn, "hardwaregain", cfg->manual_gain), "hardwaregain", iio);
            }
            break;
        case IIO_GAIN_MODE_FAST_ATTACK:
            code = plutosdr_write_str(chn, "gain_control_mode", "fast_attack", iio);
            break;
        case IIO_GAIN_MODE_SLOW_ATTACK:
            code = plutosdr_write_str(chn, "gain_control_mode", "slow_attack", iio);
            break;
        case IIO_GAIN_MODE_HYBRID:
            code = plutosdr_write_str(chn, "gain_control_mode", "hybrid", iio);
            break;
        default:
            fprintf(stderr, "unknown gain mode: %d\n", cfg->gain_control_mode);
            code = -1;
            break;
    }
    if (code != 0) {
        return code;
    }

    return code;
}

int plutosdr_select_fir_filter_config(struct stream_cfg *cfg, int *decimation, int16_t **fir_filter_taps) {
    if (cfg == NULL) {
        *decimation = 0;
        *fir_filter_taps = NULL;
        return 0;
    }

    if (cfg->sampling_freq < MIN_FIR_FILTER) {
        fprintf(stderr, "sampling freq is too low: %" PRIu64 "\n", cfg->sampling_freq);
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

int plutosdr_setup_fir_filter(struct iio_context *ctx, struct stream_cfg *rx_config, struct stream_cfg *tx_config, plutosdr *pluto) {
    int rx_decimation = 0;
    int16_t *rx_fir_filter_taps = NULL;
    int code = plutosdr_select_fir_filter_config(rx_config, &rx_decimation, &rx_fir_filter_taps);
    if (code < 0) {
        return code;
    }
    int tx_decimation = 0;
    int16_t *tx_fir_filter_taps = NULL;
    code = plutosdr_select_fir_filter_config(tx_config, &tx_decimation, &tx_fir_filter_taps);
    if (code < 0) {
        return code;
    }

    struct iio_device *phy_device = pluto->lib->iio_context_find_device(ctx, "ad9361-phy");

    // filter is not needed
    if (rx_fir_filter_taps == NULL && tx_fir_filter_taps == NULL) {
        //setup sampling freq a bit more to disable fir filter without error
        struct iio_channel *chn = plutosdr_find_phy_channel(ctx, TX, "voltage0", pluto);
        if (chn == NULL) {
            return -1;
        }
        code = plutosdr_write_lli(chn, "sampling_frequency", MIN_NO_FIR_FILTER, pluto);
        if (code < 0) {
            return code;
        }
        chn = plutosdr_find_phy_channel(ctx, RX, "voltage0", pluto);
        if (chn == NULL) {
            return -1;
        }
        code = plutosdr_write_lli(chn, "sampling_frequency", MIN_NO_FIR_FILTER, pluto);
        if (code < 0) {
            return code;
        }
        // filter might be configured prior to execution. disable it to support higher rates
        return plutosdr_error_check(plutosdr_enable_fir_filter(phy_device, false, pluto), "in_out_voltage_filter_fir_en", pluto);
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

    char *buf = malloc(sizeof(char) * FIR_BUF_SIZE);
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

    code = plutosdr_error_check(pluto->lib->iio_device_attr_write_raw(phy_device, "filter_fir_config", buf, len), "filter_fir_config", pluto);
    free(buf);
    if (code < 0) {
        return code;
    }

    code = plutosdr_error_check(plutosdr_enable_fir_filter(phy_device, true, pluto), "in_out_voltage_filter_fir_en", pluto);
    if (code < 0) {
        return code;
    }

    return 0;
}

void plutosdr_stop_rx(void *data) {
    plutosdr *pluto = (plutosdr *) data;
    pluto->rx_is_running = false;
}

ssize_t plutosdr_init_global_ctx(iio_lib *lib) {
    if (global_iio_ctx != NULL) {
        return 0;
    }
    struct iio_scan_context *scan_ctx = lib->iio_create_scan_context(NULL, 0);
    if (scan_ctx == NULL) {
        perror("unable to scan");
        return -1;
    }
    struct iio_context_info **info = NULL;
    ssize_t code = lib->iio_scan_context_get_info_list(scan_ctx, &info);
    if (code < 0) {
        fprintf(stderr, "unable to scan contexts: %zd\n", code);
        lib->iio_scan_context_destroy(scan_ctx);
        return -1;
    }
    if (code == 0) {
        fprintf(stderr, "no sdr contexts found\n");
        lib->iio_scan_context_destroy(scan_ctx);
        return -1;
    }
    const char *uri = lib->iio_context_info_get_uri(info[0]);
    fprintf(stdout, "plutosdr ctx initialized from context uri: %s\n", uri);
    global_iio_ctx = lib->iio_create_context_from_uri(uri);
    lib->iio_context_info_list_free(info);
    lib->iio_scan_context_destroy(scan_ctx);
    if (global_iio_ctx == NULL) {
        fprintf(stderr, "unable to init context: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int plutosdr_create(uint32_t id, bool rx_only, struct stream_cfg *rx_config, struct stream_cfg *tx_config, unsigned int timeout_ms, uint32_t max_input_buffer_length, iio_lib *lib, sdr_device **output) {
    if (rx_config == NULL && tx_config == NULL) {
        fprintf(stderr, "configuration is missing\n");
        return -1;
    }
    struct plutosdr_t *pluto = malloc(sizeof(struct plutosdr_t));
    if (pluto == NULL) {
        return -ENOMEM;
    }
    *pluto = (struct plutosdr_t) {0};
    pluto->rx_config = rx_config;
    pluto->tx_config = tx_config;
    pluto->lib = lib;
    pluto->id = id;

    pthread_mutex_lock(&global_iio_mutex);
    ssize_t code = plutosdr_init_global_ctx(pluto->lib);
    if (code == 0) {
        global_iio_ctx_usages++;
    }
    pthread_mutex_unlock(&global_iio_mutex);
    if (code != 0) {
        plutosdr_destroy(pluto);
        return -1;
    }

    code = pluto->lib->iio_context_set_timeout(global_iio_ctx, timeout_ms);
    if (code < 0) {
        fprintf(stderr, "unable to setup timeout: %zd\n", code);
        plutosdr_destroy(pluto);
        return -1;
    }

    code = plutosdr_setup_fir_filter(global_iio_ctx, rx_config, tx_config, pluto);
    if (code < 0) {
        plutosdr_destroy(pluto);
        return -1;
    }

    //always set this field
    //used for incoming argument validation
    pluto->output_len = max_input_buffer_length;

    if (tx_config != NULL) {
        pluto->tx = plutosdr_find_device(global_iio_ctx, TX, pluto);
        if (pluto->tx == NULL) {
            fprintf(stderr, "unable to find tx result\n");
            plutosdr_destroy(pluto);
            return -1;
        }

        code = plutosdr_disable_dds(pluto);
        if (code < 0) {
            plutosdr_destroy(pluto);
            return -1;
        }

        code = plutosdr_configure_streaming_channel(global_iio_ctx, rx_only, tx_config, TX, "voltage0", pluto);
        if (code < 0) {
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->tx0_i = plutosdr_find_streaming_channel(TX, pluto->tx, 0, pluto);
        if (pluto->tx0_i == NULL) {
            fprintf(stderr, "unable to find tx I channel\n");
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->tx0_q = plutosdr_find_streaming_channel(TX, pluto->tx, 1, pluto);
        if (pluto->tx0_q == NULL) {
            fprintf(stderr, "unable to find tx Q channel\n");
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->lib->iio_channel_enable(pluto->tx0_i);
        pluto->lib->iio_channel_enable(pluto->tx0_q);
        pluto->tx_buffer = pluto->lib->iio_device_create_buffer(pluto->tx, pluto->output_len, false);
        if (pluto->tx_buffer == NULL) {
            perror("unable to create tx buffer");
            plutosdr_destroy(pluto);
            return -1;
        }
    }

    if (rx_config != NULL) {
        pluto->rx = plutosdr_find_device(global_iio_ctx, RX, pluto);
        if (pluto->rx == NULL) {
            fprintf(stderr, "unable to find rx result\n");
            plutosdr_destroy(pluto);
            return -1;
        }
        code = plutosdr_configure_streaming_channel(global_iio_ctx, rx_only, rx_config, RX, "voltage0", pluto);
        if (code < 0) {
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->rx0_i = plutosdr_find_streaming_channel(RX, pluto->rx, 0, pluto);
        if (pluto->rx0_i == NULL) {
            fprintf(stderr, "unable to find rx I channel\n");
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->rx0_q = plutosdr_find_streaming_channel(RX, pluto->rx, 1, pluto);
        if (pluto->rx0_q == NULL) {
            fprintf(stderr, "unable to find rx Q channel\n");
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->lib->iio_channel_enable(pluto->rx0_i);
        pluto->lib->iio_channel_enable(pluto->rx0_q);
        pluto->rx_buffer = pluto->lib->iio_device_create_buffer(pluto->rx, pluto->output_len, false);
        if (pluto->rx_buffer == NULL) {
            perror("unable to create rx buffer");
            plutosdr_destroy(pluto);
            return -1;
        }
        pluto->output = malloc(sizeof(float complex) * pluto->output_len);
        if (pluto->output == NULL) {
            plutosdr_destroy(pluto);
            return -ENOMEM;
        }
        pluto->rx_is_running = true;
    } else {
        pluto->rx_is_running = false;
    }

    struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
    if (result == NULL) {
        plutosdr_destroy(pluto);
        return -ENOMEM;
    }
    result->plugin = pluto;
    result->destroy = plutosdr_destroy;
    result->sdr_process_rx = plutosdr_process_rx;
    result->sdr_process_tx = plutosdr_process_tx;
    result->stop_rx = plutosdr_stop_rx;

    *output = result;
    return 0;
}

void plutosdr_destroy(void *plugin) {
    if (plugin == NULL) {
        return;
    }
    plutosdr *pluto = (plutosdr *) plugin;
    if (pluto->tx_buffer != NULL) {
        pluto->lib->iio_buffer_destroy(pluto->tx_buffer);
    }
    if (pluto->tx0_i != NULL) {
        pluto->lib->iio_channel_disable(pluto->tx0_i);
    }
    if (pluto->tx0_q != NULL) {
        pluto->lib->iio_channel_disable(pluto->tx0_q);
    }
    if (pluto->rx_buffer != NULL) {
        pluto->lib->iio_buffer_destroy(pluto->rx_buffer);
    }
    if (pluto->rx0_i != NULL) {
        pluto->lib->iio_channel_disable(pluto->rx0_i);
    }
    if (pluto->rx0_q != NULL) {
        pluto->lib->iio_channel_disable(pluto->rx0_q);
    }
    if (pluto->output != NULL) {
        free(pluto->output);
    }

    pthread_mutex_lock(&global_iio_mutex);
    if (global_iio_ctx != NULL) {
        global_iio_ctx_usages--;
        if (global_iio_ctx_usages <= 0) {
            pluto->lib->iio_context_destroy(global_iio_ctx);
            global_iio_ctx = NULL;
            fprintf(stdout, "plutosdr ctx de-initialized\n");
        }
    }
    pthread_mutex_unlock(&global_iio_mutex);

    if (pluto->rx_config != NULL) {
        free(pluto->rx_config);
    }
    if (pluto->tx_config != NULL) {
        free(pluto->tx_config);
    }
    free(pluto);
}
