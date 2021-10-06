#include "../src/tcp_server.h"
#include "sdr_modem_client.h"
#include "utils.h"
#include <time.h>

int setup_initial_data(sdr_modem_client *client0);

int main(void) {

    struct server_config *config = NULL;
    tcp_server *server = NULL;
    int code = server_config_create(&config, "full.conf");
    if (code != 0) {
        return EXIT_FAILURE;
    }
    config->tx_sdr_type = TX_SDR_TYPE_FILE;
    config->rx_sdr_type = RX_SDR_TYPE_FILE;
    code = tcp_server_create(config, &server);
    if (code != 0) {
        return EXIT_FAILURE;
    }

    sdr_modem_client *client0 = NULL;
    code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, 10000, &client0);
    if (code != 0) {
        return EXIT_FAILURE;
    }
    code = setup_initial_data(client0);
    if (code != 0) {
        return EXIT_FAILURE;
    }
    sdr_modem_client_destroy_gracefully(client0);
    client0 = NULL;

    clock_t begin = clock();
    code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, 10000, &client0);
    if (code != 0) {
        return EXIT_FAILURE;
    }
    struct RxRequest *req = create_rx_request();
    req->filename = "tx.cf32";
    req->rx_sampling_freq = 48000;
    req->fsk_settings->demod_fsk_use_dc_block = false;
    // do not correct doppler - this will make test unstable and dependent on the
    // current satellite position
    doppler_settings__free_unpacked(req->doppler, NULL);
    req->doppler = NULL;
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_RX_REQUEST;
    code = sdr_modem_client_write_request(&header, req, client0);
    if (code != 0) {
        return EXIT_FAILURE;
    }

    size_t total = 0;
    while (total < 12288) {
        int8_t *output = NULL;
        size_t expected_read = 1024;
        code = sdr_modem_client_read_stream(&output, expected_read, client0);
        if (code != 0) {
            return EXIT_FAILURE;
        }
        total += expected_read;
    }

    clock_t end = clock();
    double time_spent = (double) (end - begin) / CLOCKS_PER_SEC;

    sdr_modem_client_destroy_gracefully(client0);
    tcp_server_destroy(server);
    server = NULL;

    // MacBook Air M1
    // VOLK_GENERIC=1:
    // completed in: 0.011671 seconds
    // tuned kernel:
    // completed in: 0.012629 seconds

    printf("completed in: %f seconds\n", time_spent);
    return EXIT_SUCCESS;
}

int setup_initial_data(sdr_modem_client *client0) {
    struct TxRequest *tx_req = create_tx_request();
    tx_req->filename = "tx.cf32";
    tx_req->tx_sampling_freq = 48000;
    // keep stable
    doppler_settings__free_unpacked(tx_req->doppler, NULL);
    tx_req->doppler = NULL;
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_TX_REQUEST;
    int code = sdr_modem_client_write_tx_request(&header, tx_req, client0);
    if (code != 0) {
        return code;
    }

    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_TX_DATA;
    struct TxData tx = TX_DATA__INIT;
    tx.data.len = 16 * 1024; //16kb
    tx.data.data = malloc(sizeof(uint8_t) * tx.data.len);
    for (size_t i = 0; i < tx.data.len; i++) {
        tx.data.data[i] = (uint8_t) i;
    }
    return sdr_modem_client_write_tx(&header, &tx, client0);
}
