// ESP32TimeServerTester v1.0.0
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#pragma once

#include <cstdint>

// Hostname for the ESP32 Time Server Tester device
constexpr char Hostname[] = "ESP32TimeServerTester";

// to override the current mac address, set it directly below, e.g."00:00:00:00:07:24";
static constexpr char MACAddress[] = "";

static constexpr uint16_t DefaultNtpPort = 123;

constexpr int number_of_NTP_requests_in_a_mini_stress_test = 1000;

// Two chained WS2812 RGB LEDs: the first shows IPv4 status, the second shows IPv6 status.
static constexpr bool WS2812_RGB_enabled = false;
static constexpr int RGB_LED_Brightness_Percent = 70;

// GPIO pin definitions for a WS2812 RGB LED module
static constexpr int WS2812DataPin = 4;

// WaveShare ESP32-P4-ETH IP101 Ethernet PHY wiring.
static constexpr int EthernetMdcGpio = 31;
static constexpr int EthernetMdioGpio = 52;
static constexpr int EthernetPhyResetGpio = 51;
static constexpr int EthernetPhyAddress = 1;
