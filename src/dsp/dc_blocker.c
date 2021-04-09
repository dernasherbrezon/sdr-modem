#include <stdlib.h>

#include "dc_blocker.h"

struct dc_blocker_t {

};


int dc_blocker_create(int length, dc_blocker **blocker) {
	return 0;
}

void dc_blocker_process(const float *input, size_t input_len, float **output, size_t *output_len, dc_blocker *blocker) {

}

void dc_blocker_destroy(dc_blocker *blocker) {
	if( blocker == NULL ) {
		return;
	}
}
