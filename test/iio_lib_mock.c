#include "iio_lib_mock.h"
#include <errno.h>

int16_t *iio_lib_mock_rx = NULL;
size_t iio_lib_mock_rx_len = 0;

int16_t *iio_lib_mock_tx = NULL;
size_t iio_lib_mock_tx_len = 0;

struct iio_device *ad9361_phy = NULL;
struct iio_channel *iio_lib_mock_tx_channel = NULL;
struct iio_channel *iio_lib_mock_rx_channel = NULL;

struct iio_scan_context {
    uint8_t dummy;
};

struct iio_context_info {
    uint8_t dummy;
};

struct iio_context {
    uint8_t dummy;
};

struct iio_device {
    uint8_t dummy;
};

struct iio_channel {
    uint8_t dummy;
};

struct iio_buffer {
    uint8_t dummy;
};

void *iio_buffer_end(const struct iio_buffer *buf) {
    if (iio_lib_mock_rx != NULL) {
        return iio_lib_mock_rx + iio_lib_mock_rx_len;
    }
    return NULL;
}

void *iio_buffer_first(const struct iio_buffer *buf, const struct iio_channel *chn) {
    if (iio_lib_mock_rx != NULL) {
        return iio_lib_mock_rx;
    }
    if (iio_lib_mock_tx != NULL) {
        return iio_lib_mock_tx;
    }
    return NULL;
}

ssize_t iio_buffer_push_partial(struct iio_buffer *buf, size_t samples_count) {
    iio_lib_mock_tx_len = samples_count;
    return samples_count;
}

ssize_t iio_buffer_refill(struct iio_buffer *buf) {
    return 0;
}

struct iio_device *iio_context_find_device(const struct iio_context *ctx, const char *name) {
    if (ad9361_phy == NULL) {
        ad9361_phy = malloc(sizeof(struct iio_device));
    }
    return ad9361_phy;
}

struct iio_channel *iio_device_find_channel(const struct iio_device *dev, const char *name, bool output) {
    if (output) {
        if (iio_lib_mock_tx_channel == NULL) {
            iio_lib_mock_tx_channel = malloc(sizeof(struct iio_channel));
        }
        return iio_lib_mock_tx_channel;
    } else {
        if (iio_lib_mock_rx_channel == NULL) {
            iio_lib_mock_rx_channel = malloc(sizeof(struct iio_channel));
        }
        return iio_lib_mock_rx_channel;
    }
}

void iio_strerror(int err, char *dst, size_t len) {
    //do nothing
}

ssize_t iio_channel_attr_write(const struct iio_channel *chn, const char *attr, const char *src) {
    return 0;
}

int iio_channel_attr_write_longlong(const struct iio_channel *chn, const char *attr, long long val) {
    return 0;
}

int iio_device_attr_write_bool(const struct iio_device *dev, const char *attr, bool val) {
    return 0;
}

int iio_channel_attr_write_bool(const struct iio_channel *chn, const char *attr, bool val) {
    return 0;
}

int iio_channel_attr_write_double(const struct iio_channel *chn, const char *attr, double val) {
    return 0;
}

ssize_t iio_device_attr_write_raw(const struct iio_device *dev, const char *attr, const void *src, size_t len) {
    return 0;
}

struct iio_scan_context *iio_create_scan_context(const char *backend, unsigned int flags) {
    return malloc(sizeof(struct iio_scan_context));
}

ssize_t iio_scan_context_get_info_list(struct iio_scan_context *ctx, struct iio_context_info ***info) {
    *info = malloc(sizeof(struct iio_context_info *) * 1);
    *info[0] = malloc(sizeof(struct iio_context_info));
    return 1;
}

void iio_scan_context_destroy(struct iio_scan_context *ctx) {
    if (ctx == NULL) {
        return;
    }
    free(ctx);
}

const char *iio_context_info_get_uri(const struct iio_context_info *info) {
    return "test";
}

struct iio_context *iio_create_context_from_uri(const char *uri) {
    return malloc(sizeof(struct iio_context));
}

void iio_context_info_list_free(struct iio_context_info **info) {
    free(info[0]);
    free(info);
}

int iio_context_set_timeout(struct iio_context *ctx, unsigned int timeout_ms) {
    return 0;
}

void iio_channel_enable(struct iio_channel *chn) {
    //do nothing
}

struct iio_buffer *iio_device_create_buffer(const struct iio_device *dev, size_t samples_count, bool cyclic) {
    return malloc(sizeof(struct iio_buffer));
}

void iio_buffer_destroy(struct iio_buffer *buf) {
    if (buf == NULL) {
        return;
    }
    free(buf);
}

void iio_channel_disable(struct iio_channel *chn) {
    //do nothing
}

void iio_context_destroy(struct iio_context *ctx) {
    if (ctx == NULL) {
        return;
    }
    free(ctx);
    if (ad9361_phy != NULL) {
        free(ad9361_phy);
        ad9361_phy = NULL;
    }
    if (iio_lib_mock_tx_channel != NULL) {
        free(iio_lib_mock_tx_channel);
        iio_lib_mock_tx_channel = NULL;
    }
    if (iio_lib_mock_rx_channel != NULL) {
        free(iio_lib_mock_rx_channel);
        iio_lib_mock_rx_channel = NULL;
    }
}

int iio_lib_mock_create(int16_t *expected_rx, size_t expected_rx_len, int16_t *expected_tx, iio_lib **lib) {
    struct iio_lib_t *result = malloc(sizeof(struct iio_lib_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct iio_lib_t) {0};
    iio_lib_mock_rx = expected_rx;
    iio_lib_mock_rx_len = expected_rx_len;
    iio_lib_mock_tx = expected_tx;
    result->iio_buffer_destroy = iio_buffer_destroy;
    result->iio_buffer_end = iio_buffer_end;
    result->iio_buffer_first = iio_buffer_first;
    result->iio_buffer_push_partial = iio_buffer_push_partial;
    result->iio_buffer_refill = iio_buffer_refill;
    result->iio_channel_attr_write = iio_channel_attr_write;
    result->iio_channel_attr_write_bool = iio_channel_attr_write_bool;
    result->iio_channel_attr_write_double = iio_channel_attr_write_double;
    result->iio_channel_attr_write_longlong = iio_channel_attr_write_longlong;
    result->iio_channel_disable = iio_channel_disable;
    result->iio_channel_enable = iio_channel_enable;
    result->iio_context_destroy = iio_context_destroy;
    result->iio_context_find_device = iio_context_find_device;
    result->iio_context_info_get_uri = iio_context_info_get_uri;
    result->iio_context_info_list_free = iio_context_info_list_free;
    result->iio_context_set_timeout = iio_context_set_timeout;
    result->iio_create_context_from_uri = iio_create_context_from_uri;
    result->iio_create_scan_context = iio_create_scan_context;
    result->iio_device_attr_write_bool = iio_device_attr_write_bool;
    result->iio_device_attr_write_raw = iio_device_attr_write_raw;
    result->iio_device_create_buffer = iio_device_create_buffer;
    result->iio_device_find_channel = iio_device_find_channel;
    result->iio_scan_context_destroy = iio_scan_context_destroy;
    result->iio_scan_context_get_info_list = iio_scan_context_get_info_list;
    result->iio_strerror = iio_strerror;

    *lib = result;
    return 0;
}

void iio_lib_mock_get_tx(int16_t **output, size_t *output_len) {
    *output = iio_lib_mock_tx;
    *output_len = iio_lib_mock_tx_len;
}