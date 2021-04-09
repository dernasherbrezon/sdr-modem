
#ifndef DSP_DC_BLOCKER_H_
#define DSP_DC_BLOCKER_H_

typedef struct dc_blocker_t dc_blocker;

int dc_blocker_create(int length, dc_blocker **blocker);

void dc_blocker_process(float *input, size_t input_len, float **output, size_t *output_len, dc_blocker *blocker);

void dc_blocker_destroy(dc_blocker *blocker);

#endif /* DSP_DC_BLOCKER_H_ */
