#pragma once
#include <cstdint>

#define OTE_UPDATES_ENABLED 1
static constexpr char Hostname[] = "ESP32TimeServerTester";
static constexpr uint16_t OTEPort = 3232;
static constexpr unsigned int OTEReceiveTimeoutSeconds = 15;
static constexpr unsigned int OTEMaxUploadTimeSeconds = 300;
