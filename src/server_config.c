#include <libconfig.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "server_config.h"

char *read_and_copy_str(const config_setting_t *setting, const char *default_value) {
    const char *value;
    if (setting == NULL) {
        value = default_value;
    } else {
        value = config_setting_get_string(setting);
    }
    size_t length = strlen(value);
    char *result = malloc(sizeof(char) * length + 1);
    if (result == NULL) {
        return NULL;
    }
    strncpy(result, value, length);
    result[length] = '\0';
    return result;
}

int server_config_create(struct server_config **config, const char *path) {
    fprintf(stdout, "loading configuration from: %s\n", path);
    struct server_config *result = malloc(sizeof(struct server_config));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct server_config) {0};

    config_t libconfig;
    config_init(&libconfig);

    int code = config_read_file(&libconfig, path);
    if (code == CONFIG_FALSE) {
        fprintf(stderr, "<3>unable to read configuration: %s\n", config_error_text(&libconfig));
        config_destroy(&libconfig);
        server_config_destroy(result);
        return -1;
    }

    const config_setting_t *setting = config_lookup(&libconfig, "buffer_size");
    uint32_t buffer_size;
    if (setting == NULL) {
        buffer_size = 262144;
    } else {
        buffer_size = (uint32_t) config_setting_get_int(setting);
    }
    fprintf(stdout, "buffer size: %d\n", buffer_size);
    result->buffer_size = buffer_size;

    setting = config_lookup(&libconfig, "bind_address");
    char *bind_address = read_and_copy_str(setting, "127.0.0.1");
    if (bind_address == NULL) {
        config_destroy(&libconfig);
        server_config_destroy(result);
        return -ENOMEM;
    }
    result->bind_address = bind_address;
    setting = config_lookup(&libconfig, "port");
    int port;
    if (setting == NULL) {
        port = 8091;
    } else {
        port = config_setting_get_int(setting);
    }
    result->port = port;
    fprintf(stdout, "start listening on %s:%d\n", result->bind_address, result->port);

    setting = config_lookup(&libconfig, "read_timeout_seconds");
    int read_timeout_seconds;
    if (setting == NULL) {
        read_timeout_seconds = 5;
    } else {
        read_timeout_seconds = config_setting_get_int(setting);
        if (read_timeout_seconds <= 0) {
            config_destroy(&libconfig);
            server_config_destroy(result);
            fprintf(stderr, "<3>read timeout should be positive: %d\n", read_timeout_seconds);
            return -1;
        }
    }
    result->read_timeout_seconds = read_timeout_seconds;
    fprintf(stdout, "read timeout %ds\n", result->read_timeout_seconds);

    setting = config_lookup(&libconfig, "queue_size");
    uint16_t queue_size;
    if (setting == NULL) {
        queue_size = 64;
    } else {
        queue_size = config_setting_get_int(setting);
    }
    fprintf(stdout, "queue_size: %d\n", queue_size);
    result->queue_size = queue_size;

    const char *default_folder = getenv("TMPDIR");
    if (default_folder == NULL) {
        default_folder = "/tmp";
    }

    setting = config_lookup(&libconfig, "base_path");
    char *base_path = read_and_copy_str(setting, default_folder);
    if (base_path == NULL) {
        config_destroy(&libconfig);
        server_config_destroy(result);
        return -ENOMEM;
    }
    result->base_path = base_path;
    fprintf(stdout, "base path for storing results: %s\n", result->base_path);

    setting = config_lookup(&libconfig, "rx_sdr_type");
    uint8_t rx_sdr_type;
    if (setting == NULL) {
        rx_sdr_type = RX_SDR_TYPE_SDR_SERVER;
    } else {
        const char *rx_sdr_type_str = config_setting_get_string(setting);
        if (strcmp(rx_sdr_type_str, "sdr-server") == 0) {
            rx_sdr_type = RX_SDR_TYPE_SDR_SERVER;
        } else {
            config_destroy(&libconfig);
            server_config_destroy(result);
            fprintf(stderr, "<3>unsupported rx_sdr_type: %s\n", rx_sdr_type_str);
            return -1;
        }
        fprintf(stdout, "rx sdr: %s\n", rx_sdr_type_str);
    }
    result->rx_sdr_type = rx_sdr_type;
    if (result->rx_sdr_type == RX_SDR_TYPE_SDR_SERVER) {
        setting = config_lookup(&libconfig, "rx_sdr_server_address");
        char *sdr_server_address = read_and_copy_str(setting, "127.0.0.1");
        if (sdr_server_address == NULL) {
            config_destroy(&libconfig);
            server_config_destroy(result);
            return -ENOMEM;
        }
        result->rx_sdr_server_address = sdr_server_address;
        setting = config_lookup(&libconfig, "rx_sdr_server_port");
        int rx_sdr_server_port;
        if (setting == NULL) {
            rx_sdr_server_port = 8090;
        } else {
            rx_sdr_server_port = config_setting_get_int(setting);
        }
        result->rx_sdr_server_port = rx_sdr_server_port;
        fprintf(stdout, "sdr server used: %s:%d\n", result->rx_sdr_server_address, result->rx_sdr_server_port);
    }

    config_destroy(&libconfig);

    *config = result;
    return 0;
}

void server_config_destroy(struct server_config *config) {
    if (config == NULL) {
        return;
    }
    if (config->bind_address != NULL) {
        free(config->bind_address);
    }
    if (config->base_path != NULL) {
        free(config->base_path);
    }
    if (config->rx_sdr_server_address != NULL) {
        free(config->rx_sdr_server_address);
    }
    free(config);
}
