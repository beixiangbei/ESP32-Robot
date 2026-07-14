#ifndef MOSS_NETWORK_CONFIG_H
#define MOSS_NETWORK_CONFIG_H

#include <stdint.h>

constexpr uint32_t MOSS_NETWORK_CONFIG_VERSION = 1;

struct MossNetworkConfig {
    char ssid[33];
    char password[65];
    char hostname[33];
    bool configured;
};

extern MossNetworkConfig mossNetworkConfig;

void loadNetworkConfig();
bool saveNetworkConfig(const char* ssid, const char* password, const char* hostname);
bool clearNetworkConfig();

#endif
