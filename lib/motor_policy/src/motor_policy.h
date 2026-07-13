#ifndef MOSS_MOTOR_POLICY_H
#define MOSS_MOTOR_POLICY_H

#include <stdint.h>

struct MotorAxisConfig {
    int32_t min_position;
    int32_t max_position;
    int32_t center_position;
    bool reversed;
};

enum class MotorMoveError {
    NONE,
    INVALID_AXIS,
    INVALID_LIMITS,
    OUT_OF_RANGE,
    OVERFLOW
};

struct MotorMoveResult {
    MotorMoveError error;
    int32_t target_position;

    bool accepted() const { return error == MotorMoveError::NONE; }
};

bool motorAxisConfigValid(const MotorAxisConfig& config);
MotorMoveResult validateAbsoluteMove(
    const MotorAxisConfig* configs,
    int axis_count,
    int axis,
    int32_t target_position
);
MotorMoveResult validateRelativeMove(
    const MotorAxisConfig* configs,
    int axis_count,
    int axis,
    int32_t current_position,
    int32_t steps
);
int motorPhysicalDirection(const MotorAxisConfig& config, int logical_direction);
const char* motorMoveErrorMessage(MotorMoveError error);

#endif

