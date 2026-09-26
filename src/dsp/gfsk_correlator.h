#ifndef DSP_GFSK_CORRELATOR_H_
#define DSP_GFSK_CORRELATOR_H_

#include <stdlib.h>
#include <stdint.h>

// data-aided symbol synchronizer for gfsk bursts. input is the filtered frequency discriminator
// output (+-1 for +-deviation), output is soft symbols (int8, positive = bit 1, +-127 = amplitude
// of the sync word).
//
// the input is correlated with the known sync word (either polarity). the peak of the correlation
// gives the symbol timing, the frequency offset (dc) and the amplitude of the burst, which are then
// held for the whole burst: there is no timing loop. the output is continuous: between bursts the
// last estimates are kept. the sync word itself is included in the output. output lags the input by
// sync word + peak search symbols
typedef struct gfsk_correlator_t gfsk_correlator;

// sps - samples per symbol of the input, must be even
// bt, filter_delay - parameters of the tx gaussian pulse (LIQUID_CPFSK_GMSK)
// rx_filter - filter applied to the discriminator output. together with the tx pulse it defines
//             how the sync word looks like at the input
// syncword - on-air symbols, msb first
// threshold - normalized correlation (pearson, so independent of amplitude and frequency offset)
//             to declare the sync word found
int gfsk_correlator_create(unsigned int sps, float bt, unsigned int filter_delay, const float *rx_filter, size_t rx_filter_len, const uint8_t *syncword, size_t syncword_bits, float threshold, size_t max_input_buffer_length, gfsk_correlator **correlator);

void gfsk_correlator_process(const float *input, size_t input_len, int8_t **output, size_t *output_len, gfsk_correlator *correlator);

void gfsk_correlator_destroy(gfsk_correlator *correlator);

#endif /* DSP_GFSK_CORRELATOR_H_ */
