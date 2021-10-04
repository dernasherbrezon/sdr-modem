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
    uint16_t port;
    if (setting == NULL) {
        port = 8091;
    } else {
        port = (uint16_t) config_setting_get_int(setting);
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
            fprintf(stderr, "<3>read timeout should be positive: %d\n", read_timeout_seconds);
            config_destroy(&libconfig);
            server_config_destroy(result);
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
        } else if (strcmp(rx_sdr_type_str, "plutosdr") == 0) {
            rx_sdr_type = RX_SDR_TYPE_PLUTOSDR;
        } else if (strcmp(rx_sdr_type_str, "file") == 0) {
            rx_sdr_type = RX_SDR_TYPE_FILE;
        } else {
            fprintf(stderr, "<3>unsupported rx_sdr_type: %s\n", rx_sdr_type_str);
            config_destroy(&libconfig);
            server_config_destroy(result);
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

    setting = config_lookup(&libconfig, "tx_sdr_type");
    uint8_t tx_sdr_type;
    if (setting == NULL) {
        tx_sdr_type = TX_SDR_TYPE_NONE;
    } else {
        const char *tx_sdr_type_str = config_setting_get_string(setting);
        if (strcmp(tx_sdr_type_str, "plutosdr") == 0) {
            tx_sdr_type = TX_SDR_TYPE_PLUTOSDR;
        } else if (strcmp(tx_sdr_type_str, "none") == 0) {
            tx_sdr_type = TX_SDR_TYPE_NONE;
        } else if (strcmp(tx_sdr_type_str, "file") == 0) {
            tx_sdr_type = TX_SDR_TYPE_FILE;
        } else {
            fprintf(stderr, "<3>unsupported tx_sdr_type: %s\n", tx_sdr_type_str);
            config_destroy(&libconfig);
            server_config_destroy(result);
            return -1;
        }
        fprintf(stdout, "tx sdr: %s\n", tx_sdr_type_str);
    }
    result->tx_sdr_type = tx_sdr_type;
    if (result->tx_sdr_type == TX_SDR_TYPE_PLUTOSDR) {
        code = iio_lib_create(&result->iio);
        if (code != 0) {
            config_destroy(&libconfig);
            server_config_destroy(result);
            return -1;
        }
    }

    setting = config_lookup(&libconfig, "rx_file_base_path");
    char *rx_file_base_path = read_and_copy_str(setting, default_folder);
    if (rx_file_base_path == NULL) {
        config_destroy(&libconfig);
        server_config_destroy(result);
        return -ENOMEM;
    }
    result->rx_file_base_path = rx_file_base_path;
    if (result->rx_sdr_type == RX_SDR_TYPE_FILE) {
        fprintf(stdout, "rx file base path: %s\n", result->rx_file_base_path);
    }
    setting = config_lookup(&libconfig, "tx_file_base_path");
    char *tx_file_base_path = read_and_copy_str(setting, default_folder);
    if (tx_file_base_path == NULL) {
        config_destroy(&libconfig);
        server_config_destroy(result);
        return -ENOMEM;
    }
    result->tx_file_base_path = tx_file_base_path;
    if (result->tx_sdr_type == TX_SDR_TYPE_FILE) {
        fprintf(stdout, "tx file base path: %s\n", result->tx_file_base_path);
    }

    setting = config_lookup(&libconfig, "tx_plutosdr_gain");
    double tx_plutosdr_gain;
    if (setting == NULL) {
        tx_plutosdr_gain = 0.0;
    } else {
        tx_plutosdr_gain = config_setting_get_float(setting);
    }
    result->tx_plutosdr_gain = tx_plutosdr_gain;
    // log parameter only if it is relevant
    if (result->tx_sdr_type == TX_SDR_TYPE_PLUTOSDR) {
        fprintf(stdout, "tx plutosdr gain %f\n", result->tx_plutosdr_gain);
    }
    setting = config_lookup(&libconfig, "rx_plutosdr_gain");
    double rx_plutosdr_gain;
    if (setting == NULL) {
        rx_plutosdr_gain = 0.0;
    } else {
        rx_plutosdr_gain = config_setting_get_float(setting);
    }
    result->rx_plutosdr_gain = rx_plutosdr_gain;
    // log parameter only if it is relevant
    if (result->rx_sdr_type == RX_SDR_TYPE_PLUTOSDR) {
        fprintf(stdout, "rx plutosdr gain %f\n", result->rx_plutosdr_gain);
    }
    setting = config_lookup(&libconfig, "tx_plutosdr_timeout_millis");
    unsigned int tx_plutosdr_timeout_millis;
    if (setting == NULL) {
        tx_plutosdr_timeout_millis = 10000;
    } else {
        tx_plutosdr_timeout_millis = config_setting_get_int(setting);
    }
    result->tx_plutosdr_timeout_millis = tx_plutosdr_timeout_millis;
    // log parameter only if it is relevant
    if (result->tx_sdr_type == TX_SDR_TYPE_PLUTOSDR) {
        fprintf(stdout, "tx timeout millis %d\n", result->tx_plutosdr_timeout_millis);
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
    if (config->iio != NULL) {
        iio_lib_destroy(config->iio);
    }
    free(config);
}
