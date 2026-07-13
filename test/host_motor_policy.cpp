#include "motor_policy.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const MotorAxisConfig CONFIGS[2] = {
    {-1024, 1024, 0, false},
    {-256, 256, 0, true},
};

int main() {
    assert(motorAxisConfigValid(CONFIGS[0]));
    assert(!motorAxisConfigValid({10, 0, 5, false}));

    MotorMoveResult result = validateAbsoluteMove(CONFIGS, 2, 0, 1024);
    assert(result.accepted());
    assert(result.target_position == 1024);

    result = validateAbsoluteMove(CONFIGS, 2, 1, 257);
    assert(result.error == MotorMoveError::OUT_OF_RANGE);

    result = validateAbsoluteMove(CONFIGS, 2, 0, -1);
    assert(result.accepted());
    assert(result.target_position == -1);

    result = validateRelativeMove(CONFIGS, 2, 0, 1000, 24);
    assert(result.accepted());
    assert(result.target_position == 1024);

    result = validateRelativeMove(CONFIGS, 2, 0, 1000, 25);
    assert(result.error == MotorMoveError::OUT_OF_RANGE);

    result = validateRelativeMove(CONFIGS, 2, 0, INT32_MAX, 1);
    assert(result.error == MotorMoveError::OVERFLOW);

    result = validateAbsoluteMove(CONFIGS, 2, 2, 0);
    assert(result.error == MotorMoveError::INVALID_AXIS);

    assert(motorPhysicalDirection(CONFIGS[0], 1) == 1);
    assert(motorPhysicalDirection(CONFIGS[1], 1) == -1);
    assert(strcmp(motorMoveErrorMessage(MotorMoveError::OUT_OF_RANGE),
                  "target outside soft limits") == 0);

    puts("motor_policy tests passed");
    return 0;
}
