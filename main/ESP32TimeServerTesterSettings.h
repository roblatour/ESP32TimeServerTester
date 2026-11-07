// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#pragma once

#include <cstdint>
#include "ptp_dut_policy.h"

// Hostname for the ESP32 Time Server Tester device
constexpr char Hostname[] = "ESP32TimeServerTester";

// Native HTTP Over The Ethernet updates; password is in ESP32TimeServerTesterSettingsSecret.h.
#define OTE_UPDATES_ENABLED 1 // 0 = disabled, 1 = enabled
static constexpr uint16_t OTEPort = 3232; // (TCP)
static constexpr unsigned int OTEReceiveTimeoutSeconds = 15;
static constexpr unsigned int OTEMaxUploadTimeSeconds = 300;
static_assert(OTEPort != 0 && OTEPort != 80);
static_assert(OTEReceiveTimeoutSeconds > 0 && OTEReceiveTimeoutSeconds <= 65535);
static_assert(OTEMaxUploadTimeSeconds > 0 && OTEMaxUploadTimeSeconds <= 3600);

// to override the current mac address, set it directly below, e.g."00:00:00:00:07:24";
static constexpr char MACAddress[] = "";

static constexpr uint16_t DefaultNtpPort = 123;

// Master switch for enabling or disabling all PTP tests.
static constexpr bool PtpTestsEnabled = true;

// Positive tests verify normal, expected behaviour: a valid request is sent and
// the DUT is expected to accept it and return a valid response.
//
// Negative tests verify that the DUT safely handles invalid or unexpected input:
// an invalid request is sent and the DUT is expected to reject it or otherwise
// avoid treating it as a valid request.
static constexpr bool PtpNegativeTestsEnabled = true;

// PTP L2 foreign-candidate tests verify how the DUT qualifies, ranks, and rejects
// candidate PTP masters. They intentionally introduce controlled foreign-master
// scenarios and therefore require an isolated, authorized test link.
static constexpr bool PtpSuppressionTestEnabled = true;

// PTP assisted tests require the operator to perform or confirm physical actions,
// such as interrupting and restoring the DUT's GNSS, PPS, or Ethernet connection.
// Standby, startup, rotation and GNSS/PPS/restart fixture prompts allow ten minutes.
// Assisted standby scenarios also require the foreign-candidate opt-in.
static constexpr bool PtpAssistedTestsEnabled = true;

// Staged key rotation requires an isolated, explicitly rebuilt DUT fixture.
// The additional fixture key below is public test data and must never protect production service.
static constexpr bool PtpKeyRotationTestEnabled = false;
static constexpr uint32_t PtpRotationFixtureKeyId = 65001;
static constexpr char PtpRotationFixtureKeyValue[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

// Combined-load scenarios require NTP and the corresponding PTP transport.
// L2 load uses both NTP families; L3 load uses the selected UDP family/families.
// Some L2 scenarios also need foreign/assisted/negative opt-ins.
static constexpr bool PtpCombinedLoadEnabled = true;

// Enables controlled seeding of L2 Delay_Req sequence numbers near rollover so that
// sequence-wrap behaviour can be exercised. It does not force Sync rollover;
// normal sequence progression remains available when this is disabled.
// Explicit L2 foreign-Announce sequence scenarios run independently.
static constexpr bool PtpSequenceSeedEnabled = true;

// Number of NTP requests to send in a mini stress test; must be between 1 and 10000.
constexpr int number_of_NTP_requests_in_a_mini_stress_test = 1000;

static constexpr uint8_t DefaultPtpDomain = 0;
// Defaults for new profiles; each UDP transport is enabled by entering its master address.
static constexpr uint8_t DefaultPtpUdpDomain = 0;
static constexpr uint32_t DefaultPtpUdpWithdrawalMs = 5000;
static constexpr uint32_t PtpTestTimeoutMs = 12000;
static constexpr uint32_t PtpObservationMs = 20000;
static constexpr uint8_t PtpObservationSamples = 8;
static constexpr uint32_t PtpResponseWindowMs = 500;
static constexpr uint32_t PtpRequestIntervalMs = 250;
static constexpr uint32_t PtpProcessingAllowanceMs = 250;
static constexpr uint32_t PtpRecoveryMs = 60000;
static constexpr uint32_t PtpOperatorPhaseTimeoutMs = 120000;

static constexpr uint32_t PtpSequenceSeed = 65532;
static constexpr uint16_t PtpCombinedRequestsPerSecond = 10;
static constexpr uint32_t PtpCombinedSchedulingAllowanceMs = 250;

// Use the versioned policy extracted from the configured time-server source.
// Enable the override only when that source layout cannot be validated.
// Mixed means enabled/optional; the fresh Announce determines signed vs unsigned
// operation. Leave both profile PTP key fields blank for optional keyless service.
static constexpr bool PtpUseOperatorPolicy = false;
static constexpr ptp_tester::DutPolicy PtpOperatorPolicy{
    1,
    ptp_tester::SelectionMode::Ranked,
    ptp_tester::AuthenticationMode::Disabled,
    8,
    0,
    1,
    0,
    10000,
    3500,
    1,
    1,
    false,
    "masterclock-authentication-off",
    "operator-configured-masterclock-authentication-off"};
static constexpr ptp_tester::DutPolicy PtpDutPolicy = PtpUseOperatorPolicy ? PtpOperatorPolicy : ptp_tester::ExtractedDutPolicy;
static constexpr uint32_t PtpDutForeignQuietMs = PtpDutPolicy.foreign_timeout_ms;
static constexpr uint32_t PtpDutWithdrawalMs = PtpDutPolicy.qualification_timeout_ms;

static_assert(PtpTestTimeoutMs >= 1000 && PtpTestTimeoutMs <= 60000);
static_assert(PtpObservationMs >= 1000 && PtpObservationMs <= 120000);
static_assert(PtpObservationSamples >= 2 && PtpObservationSamples <= 64);
static_assert(PtpResponseWindowMs >= 1 && PtpResponseWindowMs <= 5000);
static_assert(PtpRequestIntervalMs >= 100 && PtpRequestIntervalMs <= 5000);
static_assert(PtpProcessingAllowanceMs <= 5000);
static_assert(PtpRecoveryMs >= 1000 && PtpRecoveryMs <= 600000);
static_assert(PtpOperatorPhaseTimeoutMs >= 1000 && PtpOperatorPhaseTimeoutMs <= 600000);
static_assert(PtpSequenceSeed <= 65535);
static_assert(PtpRotationFixtureKeyId > 0 && PtpRotationFixtureKeyId <= 65535);
static_assert(sizeof(PtpRotationFixtureKeyValue) == 33);
static_assert(PtpCombinedRequestsPerSecond >= 1 && PtpCombinedRequestsPerSecond <= 100);
static_assert(PtpCombinedSchedulingAllowanceMs >= 1 && PtpCombinedSchedulingAllowanceMs <= 5000);
static_assert(PtpObservationMs >= PtpObservationSamples * PtpRequestIntervalMs + PtpResponseWindowMs + PtpProcessingAllowanceMs);
static_assert(PtpDutForeignQuietMs <= 3600000);
static_assert(PtpDutWithdrawalMs <= 3600000);
static_assert(DefaultPtpUdpWithdrawalMs >= 1000 && DefaultPtpUdpWithdrawalMs <= 60000);
static_assert(number_of_NTP_requests_in_a_mini_stress_test >= 1 && number_of_NTP_requests_in_a_mini_stress_test <= 10000);

// Two chained WS2812 RGB LEDs: IPv4 then IPv6. Enables the independent, NVS-backed web on/off switch;
// when enabled and switched on, both LEDs flash at 1.5-second intervals while tests run.
static constexpr bool WS2812_RGB_enabled = true;
static constexpr int RGB_LED_Brightness_Percent = 60; // 0-100%

// GPIO pin definitions for a WS2812 RGB LED module
static constexpr int WS2812DataPin = 4;

// WaveShare ESP32-P4-ETH IP101 Ethernet PHY wiring.
static constexpr int EthernetMdcGpio = 31;
static constexpr int EthernetMdioGpio = 52;
static constexpr int EthernetPhyResetGpio = 51;
static constexpr int EthernetPhyAddress = 1;
