#ifndef MOSS_MOTOR_CONFIG_H
#define MOSS_MOTOR_CONFIG_H

#include "motor_policy.h"

constexpr int MOSS_MOTOR_AXIS_COUNT = 2;
constexpr uint32_t MOSS_MOTOR_CONFIG_VERSION = 1;

extern MotorAxisConfig motorAxisConfigs[MOSS_MOTOR_AXIS_COUNT];

void loadMotorConfig();
bool saveMotorConfig();
bool setMotorAxisConfig(int axis, const MotorAxisConfig& config);
void resetMotorConfig();

#endif

