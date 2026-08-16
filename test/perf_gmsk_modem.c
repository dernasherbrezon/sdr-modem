#include "utils.h"
#include <time.h>
#include "../src/dsp/gmsk_modem.h"
#include "../src/dsp/gmsk_modem.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void perf_gmsk_demod();

void perf_gmsk_mod();

int main(void) {
  perf_gmsk_demod();
  perf_gmsk_mod();

  return EXIT_SUCCESS;
}

void perf_gmsk_mod() {
  gmsk_modem *mod = NULL;
  GmskModemSettings settings = {
    .sample_rate = 19200,
    .baud_rate = 4800,
    .deviation = 3000,
    .transition_width = 2000,
    .use_dc_block = true,
    .bt = 0.5F,
    .decimation = 2
  };
  int code = gmsk_modem_create(&settings, 2016000, &mod);
  if (code != 0) {
    return;
  }
  size_t input_len = 2048;
  uint8_t *input = malloc(sizeof(uint8_t) * input_len);
  if (input == NULL) {
    return;
  }
  for (size_t i = 0; i < input_len; i++) {
    input[i] = (uint8_t) i;
  }

  double totalTime = 0.0;
  int total = 10;
  for (int j = 0; j < total; j++) {
    int total_executions = 100;
    clock_t begin = clock();
    for (int i = 0; i < total_executions; i++) {
      complex float *output = NULL;
      size_t output_len = 0;
      gmsk_modem_modulate(input, input_len, &output, &output_len, mod);
    }
    clock_t end = clock();
    double time_spent = (double) (end - begin) / CLOCKS_PER_SEC;
    totalTime += time_spent;
  }
  // MacBook Air M1
  // VOLK_GENERIC=1:
  // completed in: 0.054459 seconds
  // tuned kernel:
  // completed in: 0.044478 seconds

  // Raspberry pi 3 mod B
  // VOLK_GENERIC=1:
  // completed in: 9.711632 seconds
  // tuned kernel:
  // completed in: 0.851478 seconds

  printf("gmsk modulation completed (average): %f seconds\n", (totalTime / total));
}

void perf_gmsk_demod() {
  GmskModemSettings settings = {
    .sample_rate = 19200,
    .baud_rate = 4800,
    .deviation = 3000,
    .transition_width = 2000,
    .use_dc_block = true,
    .bt = 0.5F,
    .decimation = 2
  };
  gmsk_modem *demod = NULL;
  int code = gmsk_modem_create(&settings, 2016000, &demod);
  if (code != 0) {
    return;
  }
  size_t input_len = 4096;
  float complex *input = malloc(sizeof(float complex) * input_len);
  if (input == NULL) {
    return;
  }
  for (size_t i = 0; i < input_len; i++) {
    input[i] = (uint8_t) i;
  }

  double totalTime = 0.0;
  int total = 10;
  for (int j = 0; j < total; j++) {
    int total_executions = 100;
    clock_t begin = clock();
    for (int i = 0; i < total_executions; i++) {
      int8_t *output = NULL;
      size_t output_len = 0;
      gmsk_modem_demodulate(input, input_len, &output, &output_len, demod);
    }
    clock_t end = clock();
    double time_spent = (double) (end - begin) / CLOCKS_PER_SEC;
    totalTime += time_spent;
  }

  // MacBook Air M1
  // VOLK_GENERIC=1:
  // completed in: 0.037171 seconds
  // tuned kernel:
  // completed in: 0.036825 seconds

  // Raspberry pi 3 mod B
  // VOLK_GENERIC=1:
  // completed in: 0.640384 seconds
  // tuned kernel:
  // completed in: 0.655575 seconds

  printf("gmsk demodulation completed (average): %f seconds\n", (totalTime / total));
}
