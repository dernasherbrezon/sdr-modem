#ifndef SDR_MODEM_IIO_LIB_MOCK_H
#define SDR_MODEM_IIO_LIB_MOCK_H

#include "../src/sdr/iio_lib.h"

int iio_lib_mock_create(int16_t *expected_rx, size_t expected_rx_len, int16_t *expected_tx, iio_lib **lib);

void iio_lib_mock_get_tx(int16_t **output, size_t *output_len);

#endif //SDR_MODEM_IIO_LIB_MOCK_H
