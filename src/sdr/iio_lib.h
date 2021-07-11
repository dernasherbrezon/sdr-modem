#ifndef SDR_MODEM_IIO_LIB_H
#define SDR_MODEM_IIO_LIB_H

#include <stdbool.h>
#include <iio.h>

typedef struct iio_lib_t iio_lib;

struct iio_lib_t {
    void *handle;

    void *(*iio_buffer_end)(const struct iio_buffer *buf);

    void *(*iio_buffer_first)(const struct iio_buffer *buf, const struct iio_channel *chn);

    ssize_t (*iio_buffer_push_partial)(struct iio_buffer *buf, size_t samples_count);

    ssize_t (*iio_buffer_refill)(struct iio_buffer *buf);

    struct iio_device *(*iio_context_find_device)(const struct iio_context *ctx, const char *name);

    struct iio_channel *(*iio_device_find_channel)(const struct iio_device *dev, const char *name, bool output);

    void (*iio_strerror)(int err, char *dst, size_t len);

    ssize_t (*iio_channel_attr_write)(const struct iio_channel *chn, const char *attr, const char *src);

    int (*iio_channel_attr_write_longlong)(const struct iio_channel *chn, const char *attr, long long val);

    int (*iio_device_attr_write_bool)(const struct iio_device *dev, const char *attr, bool val);

    int (*iio_channel_attr_write_bool)(const struct iio_channel *chn, const char *attr, bool val);

    int (*iio_channel_attr_write_double)(const struct iio_channel *chn, const char *attr, double val);

    ssize_t (*iio_device_attr_write_raw)(const struct iio_device *dev, const char *attr, const void *src, size_t len);

    struct iio_scan_context *(*iio_create_scan_context)(const char *backend, unsigned int flags);

    ssize_t (*iio_scan_context_get_info_list)(struct iio_scan_context *ctx, struct iio_context_info ***info);

    void (*iio_scan_context_destroy)(struct iio_scan_context *ctx);

    const char *(*iio_context_info_get_uri)(const struct iio_context_info *info);

    struct iio_context *(*iio_create_context_from_uri)(const char *uri);

    void (*iio_context_info_list_free)(struct iio_context_info **info);

    int (*iio_context_set_timeout)(struct iio_context *ctx, unsigned int timeout_ms);

    void (*iio_channel_enable)(struct iio_channel *chn);

    struct iio_buffer *(*iio_device_create_buffer)(const struct iio_device *dev, size_t samples_count, bool cyclic);

    void (*iio_buffer_destroy)(struct iio_buffer *buf);

    void (*iio_channel_disable)(struct iio_channel *chn);

    void (*iio_context_destroy)(struct iio_context *ctx);

};

int iio_lib_create(iio_lib **lib);
void iio_lib_destroy(iio_lib *lib);

#endif //SDR_MODEM_IIO_LIB_H
