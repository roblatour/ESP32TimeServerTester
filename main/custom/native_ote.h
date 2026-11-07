// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#pragma once

#include "esp_err.h"
#include "esp_netif.h"

struct native_ote_callbacks_t
{
    bool (*begin)();
    void (*progress)(unsigned int, unsigned int);
    void (*failure)(const char *);
    void (*success)();
};

// Call once after Ethernet and mDNS initialization. Callbacks run on the OTE HTTP task.
esp_err_t native_ote_start(esp_netif_t *netif, const native_ote_callbacks_t &callbacks);
