#include "network_config.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstring>

MossNetworkConfig mossNetworkConfig = {{0}, {0}, {0}, false};

namespace {

void setDefaultHostname() {
    uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF);
    snprintf(mossNetworkConfig.hostname, sizeof(mossNetworkConfig.hostname),
             "moss-%04x", suffix);
}

void copyValue(char* destination, size_t size, const char* source) {
    strncpy(destination, source ? source : "", size - 1);
    destination[size - 1] = '\0';
}

}  // namespace

void loadNetworkConfig() {
    mossNetworkConfig = {{0}, {0}, {0}, false};
    setDefaultHostname();

    Preferences preferences;
    if (!preferences.begin("moss_network", true)) {
        Serial.println("[CONFIG] network NVS unavailable");
        return;
    }

    uint32_t version = preferences.getUInt("version", 0);
    if (version == MOSS_NETWORK_CONFIG_VERSION) {
        String ssid = preferences.getString("ssid", "");
        String password = preferences.getString("password", "");
        String hostname = preferences.getString("hostname", mossNetworkConfig.hostname);
        if (!ssid.isEmpty() && ssid.length() <= 32 && password.length() <= 64 &&
            !hostname.isEmpty() && hostname.length() <= 32) {
            copyValue(mossNetworkConfig.ssid, sizeof(mossNetworkConfig.ssid), ssid.c_str());
            copyValue(mossNetworkConfig.password, sizeof(mossNetworkConfig.password), password.c_str());
            copyValue(mossNetworkConfig.hostname, sizeof(mossNetworkConfig.hostname), hostname.c_str());
            mossNetworkConfig.configured = true;
        }
    }
    preferences.end();
    Serial.printf("[CONFIG] network config: %s\n",
                  mossNetworkConfig.configured ? "configured" : "not configured");
}

bool saveNetworkConfig(const char* ssid, const char* password, const char* hostname) {
    if (!ssid || ssid[0] == '\0' || strlen(ssid) > 32 || !password ||
        strlen(password) > 64 || !hostname || hostname[0] == '\0' ||
        strlen(hostname) > 32) {
        return false;
    }

    Preferences preferences;
    if (!preferences.begin("moss_network", false)) {
        return false;
    }
    size_t versionBytes = preferences.putUInt("version", MOSS_NETWORK_CONFIG_VERSION);
    size_t ssidBytes = preferences.putString("ssid", ssid);
    size_t passwordBytes = preferences.putString("password", password);
    size_t hostnameBytes = preferences.putString("hostname", hostname);
    bool saved = versionBytes > 0 && ssidBytes > 0 && hostnameBytes > 0 &&
                 (password[0] == '\0' || passwordBytes > 0);
    preferences.end();
    if (!saved) return false;

    copyValue(mossNetworkConfig.ssid, sizeof(mossNetworkConfig.ssid), ssid);
    copyValue(mossNetworkConfig.password, sizeof(mossNetworkConfig.password), password);
    copyValue(mossNetworkConfig.hostname, sizeof(mossNetworkConfig.hostname), hostname);
    mossNetworkConfig.configured = true;
    return true;
}

bool clearNetworkConfig() {
    Preferences preferences;
    if (!preferences.begin("moss_network", false)) {
        return false;
    }
    bool cleared = preferences.clear();
    preferences.end();
    mossNetworkConfig = {{0}, {0}, {0}, false};
    setDefaultHostname();
    return cleared;
}
