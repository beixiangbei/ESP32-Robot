#include "motor_config.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

const MotorAxisConfig DEFAULT_CONFIGS[MOSS_MOTOR_AXIS_COUNT] = {
    {-1024, 1024, 0, false},
    {-256, 256, 0, false},
};

const char* MIN_KEYS[MOSS_MOTOR_AXIS_COUNT] = {"p_min", "t_min"};
const char* MAX_KEYS[MOSS_MOTOR_AXIS_COUNT] = {"p_max", "t_max"};
const char* CENTER_KEYS[MOSS_MOTOR_AXIS_COUNT] = {"p_center", "t_center"};
const char* REVERSED_KEYS[MOSS_MOTOR_AXIS_COUNT] = {"p_reverse", "t_reverse"};

void applyDefaults() {
    for (int axis = 0; axis < MOSS_MOTOR_AXIS_COUNT; ++axis) {
        motorAxisConfigs[axis] = DEFAULT_CONFIGS[axis];
    }
}

}  // namespace

MotorAxisConfig motorAxisConfigs[MOSS_MOTOR_AXIS_COUNT];

void loadMotorConfig() {
    applyDefaults();

    Preferences preferences;
    if (!preferences.begin("moss_motor", true)) {
        Serial.println("[CONFIG] motor NVS unavailable; using defaults");
        return;
    }

    uint32_t version = preferences.getUInt("version", 0);
    if (version != MOSS_MOTOR_CONFIG_VERSION) {
        preferences.end();
        Serial.println("[CONFIG] motor config missing or outdated; using defaults");
        return;
    }

    MotorAxisConfig loaded[MOSS_MOTOR_AXIS_COUNT];
    for (int axis = 0; axis < MOSS_MOTOR_AXIS_COUNT; ++axis) {
        loaded[axis] = {
            preferences.getInt(MIN_KEYS[axis], DEFAULT_CONFIGS[axis].min_position),
            preferences.getInt(MAX_KEYS[axis], DEFAULT_CONFIGS[axis].max_position),
            preferences.getInt(CENTER_KEYS[axis], DEFAULT_CONFIGS[axis].center_position),
            preferences.getBool(REVERSED_KEYS[axis], DEFAULT_CONFIGS[axis].reversed),
        };
    }
    preferences.end();

    for (int axis = 0; axis < MOSS_MOTOR_AXIS_COUNT; ++axis) {
        if (!motorAxisConfigValid(loaded[axis])) {
            Serial.printf("[CONFIG] invalid motor axis %d config; using defaults\n", axis);
            return;
        }
    }

    for (int axis = 0; axis < MOSS_MOTOR_AXIS_COUNT; ++axis) {
        motorAxisConfigs[axis] = loaded[axis];
    }
    Serial.println("[CONFIG] motor config loaded");
}

bool saveMotorConfig() {
    Preferences preferences;
    if (!preferences.begin("moss_motor", false)) {
        return false;
    }

    preferences.putUInt("version", MOSS_MOTOR_CONFIG_VERSION);
    for (int axis = 0; axis < MOSS_MOTOR_AXIS_COUNT; ++axis) {
        preferences.putInt(MIN_KEYS[axis], motorAxisConfigs[axis].min_position);
        preferences.putInt(MAX_KEYS[axis], motorAxisConfigs[axis].max_position);
        preferences.putInt(CENTER_KEYS[axis], motorAxisConfigs[axis].center_position);
        preferences.putBool(REVERSED_KEYS[axis], motorAxisConfigs[axis].reversed);
    }
    preferences.end();
    return true;
}

bool setMotorAxisConfig(int axis, const MotorAxisConfig& config) {
    if (axis < 0 || axis >= MOSS_MOTOR_AXIS_COUNT || !motorAxisConfigValid(config)) {
        return false;
    }
    MotorAxisConfig previous = motorAxisConfigs[axis];
    motorAxisConfigs[axis] = config;
    if (!saveMotorConfig()) {
        motorAxisConfigs[axis] = previous;
        return false;
    }
    return true;
}

void resetMotorConfig() {
    applyDefaults();
    saveMotorConfig();
}
