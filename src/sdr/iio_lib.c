#include "iio_lib.h"
#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>

void *iio_lib_dlsym(void *handle, const char *symbol) {
    void *result = dlsym(handle, symbol);
    if (result == NULL) {
        fprintf(stderr, "unable to load function %s: %s\n", symbol, dlerror());
    }
    return result;
}

int iio_lib_create(iio_lib **lib) {
    struct iio_lib_t *result = malloc(sizeof(struct iio_lib_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct iio_lib_t) {0};

    void *handle = dlopen("iio", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "unable to load libiio: %s\n", dlerror());
        iio_lib_destroy(result);
        return -1;
    }
    result->handle = handle;
    result->iio_buffer_first = iio_lib_dlsym(result->handle, "iio_buffer_first");
    if (result->iio_buffer_first == NULL) {
        return -1;
    }
    result->iio_buffer_end = iio_lib_dlsym(result->handle, "iio_buffer_end");
    if (result->iio_buffer_end == NULL) {
        return -1;
    }
    result->iio_buffer_push_partial = iio_lib_dlsym(result->handle, "iio_buffer_push_partial");
    if (result->iio_buffer_push_partial == NULL) {
        return -1;
    }
    result->iio_buffer_refill = iio_lib_dlsym(result->handle, "iio_buffer_refill");
    if (result->iio_buffer_refill == NULL) {
        return -1;
    }
    result->iio_context_find_device = iio_lib_dlsym(result->handle, "iio_context_find_device");
    if (result->iio_context_find_device == NULL) {
        return -1;
    }
    result->iio_device_find_channel = iio_lib_dlsym(result->handle, "iio_device_find_channel");
    if (result->iio_device_find_channel == NULL) {
        return -1;
    }
    result->iio_strerror = iio_lib_dlsym(result->handle, "iio_strerror");
    if (result->iio_strerror == NULL) {
        return -1;
    }
    result->iio_channel_attr_write = iio_lib_dlsym(result->handle, "iio_channel_attr_write");
    if (result->iio_channel_attr_write == NULL) {
        return -1;
    }
    result->iio_channel_attr_write_longlong = iio_lib_dlsym(result->handle, "iio_channel_attr_write_longlong");
    if (result->iio_channel_attr_write_longlong == NULL) {
        return -1;
    }
    result->iio_device_attr_write_bool = iio_lib_dlsym(result->handle, "iio_device_attr_write_bool");
    if (result->iio_device_attr_write_bool == NULL) {
        return -1;
    }
    result->iio_channel_attr_write_bool = iio_lib_dlsym(result->handle, "iio_channel_attr_write_bool");
    if (result->iio_channel_attr_write_bool == NULL) {
        return -1;
    }
    result->iio_channel_attr_write_double = iio_lib_dlsym(result->handle, "iio_channel_attr_write_double");
    if (result->iio_channel_attr_write_double == NULL) {
        return -1;
    }
    result->iio_device_attr_write_raw = iio_lib_dlsym(result->handle, "iio_device_attr_write_raw");
    if (result->iio_device_attr_write_raw == NULL) {
        return -1;
    }
    result->iio_create_scan_context = iio_lib_dlsym(result->handle, "iio_create_scan_context");
    if (result->iio_create_scan_context == NULL) {
        return -1;
    }
    result->iio_scan_context_get_info_list = iio_lib_dlsym(result->handle, "iio_scan_context_get_info_list");
    if (result->iio_scan_context_get_info_list == NULL) {
        return -1;
    }
    result->iio_scan_context_destroy = iio_lib_dlsym(result->handle, "iio_scan_context_destroy");
    if (result->iio_scan_context_destroy == NULL) {
        return -1;
    }
    result->iio_context_info_get_uri = iio_lib_dlsym(result->handle, "iio_context_info_get_uri");
    if (result->iio_context_info_get_uri == NULL) {
        return -1;
    }
    result->iio_create_context_from_uri = iio_lib_dlsym(result->handle, "iio_create_context_from_uri");
    if (result->iio_create_context_from_uri == NULL) {
        return -1;
    }
    result->iio_context_info_list_free = iio_lib_dlsym(result->handle, "iio_context_info_list_free");
    if (result->iio_context_info_list_free == NULL) {
        return -1;
    }
    result->iio_context_set_timeout = iio_lib_dlsym(result->handle, "iio_context_set_timeout");
    if (result->iio_context_set_timeout == NULL) {
        return -1;
    }
    result->iio_channel_enable = iio_lib_dlsym(result->handle, "iio_channel_enable");
    if (result->iio_channel_enable == NULL) {
        return -1;
    }
    result->iio_device_create_buffer = iio_lib_dlsym(result->handle, "iio_device_create_buffer");
    if (result->iio_device_create_buffer == NULL) {
        return -1;
    }
    result->iio_buffer_destroy = iio_lib_dlsym(result->handle, "iio_buffer_destroy");
    if (result->iio_buffer_destroy == NULL) {
        return -1;
    }
    result->iio_channel_disable = iio_lib_dlsym(result->handle, "iio_channel_disable");
    if (result->iio_channel_disable == NULL) {
        return -1;
    }
    result->iio_context_destroy = iio_lib_dlsym(result->handle, "iio_context_destroy");
    if (result->iio_context_destroy == NULL) {
        return -1;
    }

    *lib = result;
    return 0;
}

void iio_lib_destroy(iio_lib *lib) {
    if (lib == NULL) {
        return;
    }
    if (lib->handle != NULL) {
        dlclose(lib->handle);
    }
    free(lib);
}
