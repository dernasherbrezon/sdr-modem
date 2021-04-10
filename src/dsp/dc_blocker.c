#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "dc_blocker.h"

struct moving_average {
    float *delay_line;
    int delay_line_len;
    int length;
    float inDelayed;
    float outD1;
};

struct dc_blocker_t {
    struct moving_average *ma0;
    struct moving_average *ma1;
    struct moving_average *ma2;
    struct moving_average *ma3;

    float *delay_line;
    int delay_line_len;
};

void moving_average_destroy(struct moving_average *mavg) {
    if (mavg == NULL) {
        return;
    }
    if (mavg->delay_line != NULL) {
        free(mavg->delay_line);
    }
    free(mavg);
}

int moving_average_create(int length, struct moving_average **mavg) {
    struct moving_average *result = malloc(sizeof(struct moving_average));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct moving_average) {0};
    result->delay_line_len = length - 1;
    result->delay_line = malloc(sizeof(float) * result->delay_line_len);
    if (result->delay_line == NULL) {
        moving_average_destroy(result);
        return -ENOMEM;
    }
    memset(result->delay_line, 0, sizeof(float) * result->delay_line_len);
    result->length = length;
    result->inDelayed = 0.0F;
    result->outD1 = 0.0F;
    *mavg = result;
    return 0;
}

float moving_average_process(float input, struct moving_average *mavg) {
    float inDelayed = mavg->inDelayed;
    mavg->inDelayed = mavg->delay_line[0];
    memmove(mavg->delay_line, mavg->delay_line + 1, sizeof(float) * (mavg->delay_line_len - 1));
    mavg->delay_line[(mavg->delay_line_len - 1)] = input;
    float y = input - inDelayed + mavg->outD1;
    mavg->outD1 = y;
    return y / mavg->length;
}

int dc_blocker_create(int length, dc_blocker **blocker) {
    struct dc_blocker_t *result = malloc(sizeof(struct dc_blocker_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct dc_blocker_t) {0};
    result->delay_line_len = length - 1;
    result->delay_line = malloc(sizeof(float) * result->delay_line_len);
    if (result->delay_line == NULL) {
        dc_blocker_destroy(result);
        return -ENOMEM;
    }
    memset(result->delay_line, 0, sizeof(float) * result->delay_line_len);

    int code = moving_average_create(length, &result->ma0);
    if (code != 0) {
        dc_blocker_destroy(result);
        return code;
    }
    code = moving_average_create(length, &result->ma1);
    if (code != 0) {
        dc_blocker_destroy(result);
        return code;
    }
    code = moving_average_create(length, &result->ma2);
    if (code != 0) {
        dc_blocker_destroy(result);
        return code;
    }
    code = moving_average_create(length, &result->ma3);
    if (code != 0) {
        dc_blocker_destroy(result);
        return code;
    }
    *blocker = result;
    return 0;
}

void dc_blocker_process(float *input, size_t input_len, float **output, size_t *output_len, dc_blocker *blocker) {
    for (size_t i = 0; i < input_len; i++) {
        float y1 = moving_average_process(input[i], blocker->ma0);
        float y2 = moving_average_process(y1, blocker->ma1);
        float y3 = moving_average_process(y2, blocker->ma2);
        float y4 = moving_average_process(y3, blocker->ma3);

        float d = blocker->delay_line[0];
        memmove(blocker->delay_line, blocker->delay_line + 1, sizeof(float) * (blocker->delay_line_len - 1));
        blocker->delay_line[(blocker->delay_line_len - 1)] = blocker->ma0->inDelayed;
        input[i] = d - y4;
    }
    *output = input;
    *output_len = input_len;
}

void dc_blocker_destroy(dc_blocker *blocker) {
    if (blocker == NULL) {
        return;
    }
    if (blocker->delay_line != NULL) {
        free(blocker->delay_line);
    }
    moving_average_destroy(blocker->ma0);
    moving_average_destroy(blocker->ma1);
    moving_average_destroy(blocker->ma2);
    moving_average_destroy(blocker->ma3);
    free(blocker);
}
