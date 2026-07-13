#include "motor_policy.h"

#include <limits.h>

bool motorAxisConfigValid(const MotorAxisConfig& config) {
    return config.min_position <= config.center_position &&
           config.center_position <= config.max_position;
}

MotorMoveResult validateAbsoluteMove(
    const MotorAxisConfig* configs,
    int axis_count,
    int axis,
    int32_t target_position
) {
    if (configs == nullptr || axis < 0 || axis >= axis_count) {
        return {MotorMoveError::INVALID_AXIS, 0};
    }

    const MotorAxisConfig& config = configs[axis];
    if (!motorAxisConfigValid(config)) {
        return {MotorMoveError::INVALID_LIMITS, 0};
    }
    if (target_position < config.min_position || target_position > config.max_position) {
        return {MotorMoveError::OUT_OF_RANGE, target_position};
    }
    return {MotorMoveError::NONE, target_position};
}

MotorMoveResult validateRelativeMove(
    const MotorAxisConfig* configs,
    int axis_count,
    int axis,
    int32_t current_position,
    int32_t steps
) {
    int64_t target = static_cast<int64_t>(current_position) + steps;
    if (target < INT32_MIN || target > INT32_MAX) {
        return {MotorMoveError::OVERFLOW, current_position};
    }
    return validateAbsoluteMove(configs, axis_count, axis, static_cast<int32_t>(target));
}

int motorPhysicalDirection(const MotorAxisConfig& config, int logical_direction) {
    int normalized = logical_direction < 0 ? -1 : 1;
    return config.reversed ? -normalized : normalized;
}

const char* motorMoveErrorMessage(MotorMoveError error) {
    switch (error) {
        case MotorMoveError::NONE:
            return "none";
        case MotorMoveError::INVALID_AXIS:
            return "invalid motor";
        case MotorMoveError::INVALID_LIMITS:
            return "invalid motor limits";
        case MotorMoveError::OUT_OF_RANGE:
            return "target outside soft limits";
        case MotorMoveError::OVERFLOW:
            return "target overflow";
    }
    return "unknown motor error";
}

