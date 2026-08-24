#ifndef SDR_MODEM_FREQ_OFFSET_H
#define SDR_MODEM_FREQ_OFFSET_H

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct freq_offset_t freq_offset;

// file is a list of "<unix timestamp (seconds)> <frequency offset (hz)>" lines, sorted by timestamp.
// timestamp == 0 for the first line means the file is used against a pre-recorded I/Q stream: time
// advances purely with processed samples starting at 0, real (wall-clock) time is ignored.
// otherwise the current wall-clock time is used to find the applicable interval on creation; if every
// line is already in the past, creation fails since no offset would apply to samples processed from now on.
int freq_offset_create(const char *file_path, uint64_t sample_rate, size_t max_input_buffer_length, freq_offset **result);

void freq_offset_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len, freq_offset *fo);

void freq_offset_destroy(freq_offset *fo);

#endif //SDR_MODEM_FREQ_OFFSET_H
