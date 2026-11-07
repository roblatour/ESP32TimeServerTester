// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <limits>

#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "ping/ping_sock.h"
#include "psa/crypto.h"

#include "ESP32TimeServerTesterSettings.h"
#include "app_metadata.h"
#include "custom/native_ote.h"
#include "ntp_load_schedule.h"
#include "ptp_tester.h"

extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

namespace
{
    constexpr char Tag[] = "time_tester";
    constexpr char NvsNamespace[] = "tester";
    constexpr char PhysicalLedsNvsKey[] = "leds_on";
    constexpr size_t TargetNameSize = 41;
    constexpr size_t MaxTargetHistory = 20;
    constexpr char TargetHistoryNvsKey[] = "profiles_v5";
    constexpr size_t TargetOptionsBufferSize = 19000;
    constexpr size_t NtpTestCount = 20;
    constexpr size_t PtpL2TestCount = static_cast<size_t>(ptp_tester::Test::Count);
    constexpr size_t PtpUdpTestCount = 24;
    constexpr size_t TestCount = NtpTestCount + PtpL2TestCount + PtpUdpTestCount;
    constexpr size_t PageBufferSize = 56000;
    constexpr size_t StatusResponseBufferSize = 4096;
    constexpr size_t ReportBufferSize = 16384;
    constexpr size_t ResultDetailSize = 32768;
    constexpr size_t PtpOutputSize = 28672;
    constexpr char PtpWarning[] = "PTP L2 is a restricted two-step E2E observer with legacy/ranked foreign-master policy testing and optional HMAC-SHA256-128 authentication. PTP L3 UDP tests unsigned IPv4 and link-local IPv6 multicast E2E service; the UDP socket API does not provide hardware timestamps, so those tests do not measure clock offset, path delay or synchronization accuracy. Neither transport is a follower servo or a full BMCA/profile conformance suite. PTP L2 and NTP keys are independent; PTP L3 UDP does not use a key. Use only an authorized isolated test link with no unrelated PTP master. The tester PHC is not disciplined. L2 measurements require hardware Sync RX and Delay_Req TX timestamps. Accuracy requires a hardware-timestamped client with a running servo, packet capture and independent PPS/phase measurement. Source metadata is not runtime attestation. Incomplete or interrupted scenarios cannot pass.";
    constexpr uint64_t NtpEpochOffset = 2208988800ULL;
    constexpr size_t NtpPacketSize = 48;
    constexpr size_t NtpAuthTrailerSize = 24;

    enum class TestStatus : uint8_t
    {
        NotRun,
        Running,
        AwaitingOperatorAction,
        TimedOutAwaitingOperatorAction,
        Pass,
        Fail,
        CouldNotRun,
        DidNotRun,
        Skipped
    };

    struct TestDefinition
    {
        const char *name;
        const char *description;
        int family;
        bool enabled;
        enum class Kind : uint8_t
        {
            Ping,
            StandardV3,
            StandardV4,
            Interleaved,
            Auth,
            AuthInterleaved,
            InvalidAuth,
            MissingAuth,
            TruncatedMac,
            MiniStress,
            PtpAnnounce,
            PtpSync,
            PtpDelay,
            PtpProperties,
            PtpMeasurement,
            PtpSustained,
            PtpNegativeDelay,
            PtpForeignAnnounce,
            PtpOnly,
            PtpGnssRecovery,
            PtpPpsRecovery,
            PtpLinkRecovery,
            PtpRestartRecovery,
            PtpCombinedLoad,
            PtpRankingPrecedence,
            PtpSourceIsolation,
            PtpSequencePolicy,
            PtpDatasetChanges,
            PtpMultipleMasters,
            PtpIntervalExpiry,
            PtpStandbyRecovery,
            PtpSignedMessages,
            PtpSignedExchange,
            PtpInvalidAuthentication,
            PtpAuthenticationPolicy,
            PtpAuthenticationBeforeSelection,
            PtpOptionalUnsignedService,
            PtpOptionalSignedRejection,
            PtpAnnounceAuthenticationPolicy,
            PtpInvalidAuthenticationRefresh,
            PtpForeignCapacity,
            PtpSelectionNtpLoad,
            PtpStandbyPpsRecovery,
            PtpStandbyLinkRecovery,
            PtpStandbyRestartRecovery,
            PtpKeyRotation,
            PtpStartupListening,
            PtpStandbyExpiryUnqualified,
            PtpSelectionNtpHoldover,
            PtpSelectionNtpPolicyLoad,
            PtpUdp
        } kind;
        ptp_tester::UdpTest udp_test = ptp_tester::UdpTest::Announce;
    };

    struct TestResult
    {
        TestStatus status = TestStatus::NotRun;
        char timestamp[512] = "n/a";
        std::unique_ptr<char[]> details;
    };

    struct RunMetadata
    {
        ptp_tester::FirmwareMode mode = ptp_tester::FirmwareMode::Unknown;
    };

    const char *firmware_mode_text(ptp_tester::FirmwareMode mode)
    {
        switch (mode)
        {
        case ptp_tester::FirmwareMode::NtpOnly:
            return "NtpOnly";
        case ptp_tester::FirmwareMode::PtpOnly:
            return "PtpOnly";
        case ptp_tester::FirmwareMode::Combined:
            return "Combined";
        case ptp_tester::FirmwareMode::PtpL3Only:
            return "PtpL3Only";
        case ptp_tester::FirmwareMode::NtpPtpL3:
            return "NtpPtpL3";
        case ptp_tester::FirmwareMode::PtpL2PtpL3:
            return "PtpL2PtpL3";
        case ptp_tester::FirmwareMode::NtpPtpL2PtpL3:
            return "NtpPtpL2PtpL3";
        default:
            return "Unknown";
        }
    }

    const char *firmware_mode_label(ptp_tester::FirmwareMode mode)
    {
        switch (mode)
        {
        case ptp_tester::FirmwareMode::NtpOnly:
            return "NTP only";
        case ptp_tester::FirmwareMode::PtpOnly:
            return "PTP L2 only";
        case ptp_tester::FirmwareMode::PtpL3Only:
            return "PTP L3 UDP only";
        case ptp_tester::FirmwareMode::Combined:
            return "NTP + PTP L2";
        case ptp_tester::FirmwareMode::NtpPtpL3:
            return "NTP + PTP L3 UDP";
        case ptp_tester::FirmwareMode::PtpL2PtpL3:
            return "PTP L2 + PTP L3 UDP";
        case ptp_tester::FirmwareMode::NtpPtpL2PtpL3:
            return "NTP + PTP L2 + PTP L3 UDP";
        default:
            return "unknown";
        }
    }

    bool parse_firmware_mode(const char *text, ptp_tester::FirmwareMode &mode)
    {
        for (const auto candidate : {ptp_tester::FirmwareMode::Unknown, ptp_tester::FirmwareMode::NtpOnly,
                                     ptp_tester::FirmwareMode::PtpOnly, ptp_tester::FirmwareMode::Combined,
                                     ptp_tester::FirmwareMode::PtpL3Only, ptp_tester::FirmwareMode::NtpPtpL3,
                                     ptp_tester::FirmwareMode::PtpL2PtpL3, ptp_tester::FirmwareMode::NtpPtpL2PtpL3})
        {
            if (std::strcmp(text, firmware_mode_text(candidate)) == 0)
            {
                mode = candidate;
                return true;
            }
        }
        return false;
    }

    void ptp_policy_text(char *output, size_t size)
    {
        const char *issue = ptp_tester::policy_error(PtpDutPolicy);
        std::snprintf(output, size,
                      "DUT policy v%u: selection=%s; PTP authentication=%s; capacity=%u; domain=%u; "
                      "foreign timeout=%lu ms; local log Announce/Sync=%d/%d; GNSS/PPS qualification=%lu ms; "
                      "SPP=%u outgoing key ID=%lu; NTP authentication=%s; source identity=%s; provenance=%s. %s",
                      PtpDutPolicy.version, ptp_tester::selection_text(PtpDutPolicy.selection),
                      ptp_tester::authentication_text(PtpDutPolicy.authentication), PtpDutPolicy.capacity,
                      PtpDutPolicy.domain, static_cast<unsigned long>(PtpDutPolicy.foreign_timeout_ms),
                      PtpDutPolicy.log_announce_interval, PtpDutPolicy.log_sync_interval,
                      static_cast<unsigned long>(PtpDutPolicy.qualification_timeout_ms), PtpDutPolicy.spp,
                      static_cast<unsigned long>(PtpDutPolicy.outgoing_key_id),
                      issue ? "unknown/unverified" : PtpDutPolicy.ntp_authentication_required ? "required"
                                                                                              : "mixed",
                      PtpDutPolicy.identity ? PtpDutPolicy.identity : "unavailable",
                      PtpDutPolicy.provenance ? PtpDutPolicy.provenance : "unavailable",
                      issue ? issue : "Source configuration only, not runtime attestation.");
    }

    const char *result_details(const TestResult &result)
    {
        return result.details ? result.details.get() : result.timestamp;
    }

    char *could_not_run_reason(char *details)
    {
        constexpr char CouldNotRunMarker[] = "CouldNotRun:";
        if (char *reason = std::strstr(details, CouldNotRunMarker))
        {
            reason += sizeof(CouldNotRunMarker) - 1;
            while (*reason == ' ' || *reason == '\t')
                ++reason;
            return reason;
        }
        for (const char *marker : {"Incomplete evidence / missing operator action / cancellation:",
                                   "Interrupted observation:"})
            if (char *reason = std::strstr(details, marker))
                return reason;

        char *reason = details;
        if (std::strncmp(reason, "Operator firmware=", sizeof("Operator firmware=") - 1) == 0)
        {
            reason = std::strchr(reason, '\n');
            if (reason != nullptr)
                ++reason;
        }
        while (reason != nullptr &&
               (std::strncmp(reason, "DUT policy ", sizeof("DUT policy ") - 1) == 0 ||
                std::strncmp(reason, "Target=", sizeof("Target=") - 1) == 0 ||
                std::strncmp(reason, "Observation=", sizeof("Observation=") - 1) == 0))
        {
            reason = std::strchr(reason, '\n');
            if (reason != nullptr)
                ++reason;
        }
        return reason && reason[0] ? reason : details;
    }

    struct symmetric_key_t
    {
        uint16_t key_id = 0;
        char key_value[33] = {};
    };

    struct TargetHistoryEntryV1
    {
        char target[TargetNameSize] = {};
        char ipv4[INET_ADDRSTRLEN] = {};
        char ipv6[INET6_ADDRSTRLEN] = {};
        uint16_t ipv4_ntp_port = 0;
        uint16_t ipv6_ntp_port = 0;
        symmetric_key_t key{};
        bool stress_tests = false;
        uint64_t last_stored = 0;
    };

    struct TargetHistoryEntryV2 : TargetHistoryEntryV1
    {
        char ptp_mac[18] = {};
        uint8_t ptp_domain = DefaultPtpDomain;
    };

    struct TargetHistoryEntryV3 : TargetHistoryEntryV2
    {
        symmetric_key_t ptp_key{};
    };

    struct TargetHistoryEntryV4 : TargetHistoryEntryV3
    {
        ptp_tester::FirmwareMode firmware_mode = ptp_tester::FirmwareMode::Unknown;
    };

    struct TargetHistoryEntry : TargetHistoryEntryV4
    {
        char ptp_udp_ipv4[INET_ADDRSTRLEN] = {};
        char ptp_udp_ipv6[INET6_ADDRSTRLEN] = {};
        uint8_t ptp_udp_domain = DefaultPtpUdpDomain;
        uint32_t ptp_udp_withdrawal_ms = DefaultPtpUdpWithdrawalMs;
    };

    struct TargetHistoryStorageV1
    {
        uint8_t count = 0;
        std::array<TargetHistoryEntryV1, MaxTargetHistory> entries{};
    };

    struct TargetHistoryStorage
    {
        uint8_t count = 0;
        std::array<TargetHistoryEntry, MaxTargetHistory> entries{};
    };

    struct TargetHistoryStorageV2
    {
        uint8_t count = 0;
        std::array<TargetHistoryEntryV2, MaxTargetHistory> entries{};
    };

    struct TargetHistoryStorageV3
    {
        uint8_t count = 0;
        std::array<TargetHistoryEntryV3, MaxTargetHistory> entries{};
    };

    struct TargetHistoryStorageV4
    {
        uint8_t count = 0;
        std::array<TargetHistoryEntryV4, MaxTargetHistory> entries{};
    };

    const char *disabled_ptp_setting(const TestDefinition &test)
    {
        switch (test.kind)
        {
        case TestDefinition::Kind::PtpNegativeDelay:
            return PtpNegativeTestsEnabled ? nullptr : "PtpNegativeTestsEnabled";
        case TestDefinition::Kind::PtpOptionalSignedRejection:
        case TestDefinition::Kind::PtpInvalidAuthenticationRefresh:
            return !PtpNegativeTestsEnabled     ? "PtpNegativeTestsEnabled"
                   : !PtpSuppressionTestEnabled ? "PtpSuppressionTestEnabled"
                                                : nullptr;
        case TestDefinition::Kind::PtpForeignAnnounce:
        case TestDefinition::Kind::PtpRankingPrecedence:
        case TestDefinition::Kind::PtpSourceIsolation:
        case TestDefinition::Kind::PtpSequencePolicy:
        case TestDefinition::Kind::PtpDatasetChanges:
        case TestDefinition::Kind::PtpMultipleMasters:
        case TestDefinition::Kind::PtpIntervalExpiry:
        case TestDefinition::Kind::PtpAuthenticationBeforeSelection:
        case TestDefinition::Kind::PtpAnnounceAuthenticationPolicy:
        case TestDefinition::Kind::PtpForeignCapacity:
            return PtpSuppressionTestEnabled ? nullptr : "PtpSuppressionTestEnabled";
        case TestDefinition::Kind::PtpGnssRecovery:
        case TestDefinition::Kind::PtpPpsRecovery:
        case TestDefinition::Kind::PtpLinkRecovery:
        case TestDefinition::Kind::PtpRestartRecovery:
        case TestDefinition::Kind::PtpStartupListening:
            return PtpAssistedTestsEnabled ? nullptr : "PtpAssistedTestsEnabled";
        case TestDefinition::Kind::PtpStandbyRecovery:
        case TestDefinition::Kind::PtpStandbyPpsRecovery:
        case TestDefinition::Kind::PtpStandbyLinkRecovery:
        case TestDefinition::Kind::PtpStandbyRestartRecovery:
        case TestDefinition::Kind::PtpStandbyExpiryUnqualified:
            return !PtpAssistedTestsEnabled     ? "PtpAssistedTestsEnabled"
                   : !PtpSuppressionTestEnabled ? "PtpSuppressionTestEnabled"
                                                : nullptr;
        case TestDefinition::Kind::PtpCombinedLoad:
            return PtpCombinedLoadEnabled ? nullptr : "PtpCombinedLoadEnabled";
        case TestDefinition::Kind::PtpSelectionNtpLoad:
            return !PtpCombinedLoadEnabled      ? "PtpCombinedLoadEnabled"
                   : !PtpSuppressionTestEnabled ? "PtpSuppressionTestEnabled"
                                                : nullptr;
        case TestDefinition::Kind::PtpSelectionNtpHoldover:
            return !PtpCombinedLoadEnabled      ? "PtpCombinedLoadEnabled"
                   : !PtpAssistedTestsEnabled   ? "PtpAssistedTestsEnabled"
                   : !PtpSuppressionTestEnabled ? "PtpSuppressionTestEnabled"
                                                : nullptr;
        case TestDefinition::Kind::PtpSelectionNtpPolicyLoad:
            return !PtpCombinedLoadEnabled      ? "PtpCombinedLoadEnabled"
                   : !PtpSuppressionTestEnabled ? "PtpSuppressionTestEnabled"
                   : !PtpNegativeTestsEnabled   ? "PtpNegativeTestsEnabled"
                                                : nullptr;
        case TestDefinition::Kind::PtpKeyRotation:
            return !PtpAssistedTestsEnabled     ? "PtpAssistedTestsEnabled"
                   : !PtpNegativeTestsEnabled   ? "PtpNegativeTestsEnabled"
                   : !PtpKeyRotationTestEnabled ? "PtpKeyRotationTestEnabled"
                                                : nullptr;
        case TestDefinition::Kind::PtpUdp:
            return test.udp_test == ptp_tester::UdpTest::NegativeDelay && !PtpNegativeTestsEnabled
                       ? "PtpNegativeTestsEnabled"
                   : ptp_tester::udp_involves_ntp(test.udp_test) && !PtpCombinedLoadEnabled
                       ? "PtpCombinedLoadEnabled"
                   : ptp_tester::udp_requires_operator(test.udp_test) && !PtpAssistedTestsEnabled
                       ? "PtpAssistedTestsEnabled"
                       : nullptr;
        default:
            return nullptr;
        }
    }

    constexpr std::array<TestDefinition, TestCount> Tests = {{
        {"IPv4 Ping target time server", "Ensure the DUT is reachable over an IPv4 connection.", AF_INET, true, TestDefinition::Kind::Ping},
        {"IPv4 NTPv3 Standard", "Verify standard NTPv3 service over IPv4; under required NTP authentication, verify that unsigned NTPv3 is rejected using authenticated NTPv4 before/after controls.", AF_INET, true, TestDefinition::Kind::StandardV3},
        {"IPv4 NTPv4 Standard", "Verify standard NTPv4 service over IPv4; under required NTP authentication, verify that unsigned NTPv4 is rejected using authenticated before/after controls.", AF_INET, true, TestDefinition::Kind::StandardV4},
        {"IPv4 NTPv4 Interleaved", "Verify interleaved NTPv4 exchanges over IPv4, using authenticated requests and verified responses when NTP authentication is required.", AF_INET, true, TestDefinition::Kind::Interleaved},
        {"IPv4 NTPv4 Symmetric-key standard", "Verify that the DUT accepts authenticated standard NTP version 4 requests over IPv4.", AF_INET, true, TestDefinition::Kind::Auth},
        {"IPv4 NTPv4 Symmetric-key interleaved", "Verify that the DUT accepts authenticated interleaved NTP version 4 requests over IPv4.", AF_INET, true, TestDefinition::Kind::AuthInterleaved},
        {"IPv4 NTPv4 Invalid symmetric-key", "Verify that the DUT rejects NTPv4 requests with invalid authentication over IPv4, with healthy before/after controls.", AF_INET, true, TestDefinition::Kind::InvalidAuth},
        {"IPv4 NTPv4 Missing symmetric-key", "Verify that the DUT rejects an incomplete NTPv4 authentication trailer over IPv4, with healthy before/after controls.", AF_INET, true, TestDefinition::Kind::MissingAuth},
        {"IPv4 NTPv4 Truncated MAC", "Verify that the DUT rejects NTPv4 requests with incomplete authentication data over IPv4, with healthy before/after controls.", AF_INET, true, TestDefinition::Kind::TruncatedMac},
        {"IPv4 NTPv4 Standard mini saturated stress test", "Verify responsiveness during a short, saturated IPv4 NTP load, with authenticated requests and verified responses when required. (Runs only when mini stress tests are enabled for the target.)", AF_INET, true, TestDefinition::Kind::MiniStress},
        {"IPv6 Ping target time server", "Ensure the DUT is reachable over an IPv6 connection.", AF_INET6, true, TestDefinition::Kind::Ping},
        {"IPv6 NTPv3 Standard", "Verify standard NTPv3 service over IPv6; under required NTP authentication, verify that unsigned NTPv3 is rejected using authenticated NTPv4 before/after controls.", AF_INET6, true, TestDefinition::Kind::StandardV3},
        {"IPv6 NTPv4 Standard", "Verify standard NTPv4 service over IPv6; under required NTP authentication, verify that unsigned NTPv4 is rejected using authenticated before/after controls.", AF_INET6, true, TestDefinition::Kind::StandardV4},
        {"IPv6 NTPv4 Interleaved", "Verify interleaved NTPv4 exchanges over IPv6, using authenticated requests and verified responses when NTP authentication is required.", AF_INET6, true, TestDefinition::Kind::Interleaved},
        {"IPv6 NTPv4 Symmetric-key standard", "Verify that the DUT accepts authenticated standard NTP version 4 requests over IPv6.", AF_INET6, true, TestDefinition::Kind::Auth},
        {"IPv6 NTPv4 Symmetric-key interleaved", "Verify that the DUT accepts authenticated interleaved NTP version 4 requests over IPv6.", AF_INET6, true, TestDefinition::Kind::AuthInterleaved},
        {"IPv6 NTPv4 Invalid symmetric-key", "Verify that the DUT rejects NTPv4 requests with invalid authentication over IPv6, with healthy before/after controls.", AF_INET6, true, TestDefinition::Kind::InvalidAuth},
        {"IPv6 NTPv4 Missing symmetric-key", "Verify that the DUT rejects an incomplete NTPv4 authentication trailer over IPv6, with healthy before/after controls.", AF_INET6, true, TestDefinition::Kind::MissingAuth},
        {"IPv6 NTPv4 Truncated MAC", "Verify that the DUT rejects NTPv4 requests with incomplete authentication data over IPv6, with healthy before/after controls.", AF_INET6, true, TestDefinition::Kind::TruncatedMac},
        {"IPv6 NTPv4 Standard mini saturated stress test", "Verify responsiveness during a short, saturated IPv6 NTP load, with authenticated requests and verified responses when required. (Runs only when mini stress tests are enabled for the target.)", AF_INET6, true, TestDefinition::Kind::MiniStress},
        {"PTP L2 Announce and master identity", "Verify that the DUT advertises valid PTP master and identity information.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpAnnounce},
        {"PTP L2 two-step Sync / Follow_Up", "Verify that the DUT sends valid two-step PTP synchronization exchanges.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSync},
        {"PTP L2 E2E Delay_Req / Delay_Resp", "Verify that the DUT completes end-to-end PTP delay measurements.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpDelay},
        {"PTP L2 time properties and clock quality", "Verify that the DUT advertises valid time properties and clock-quality data.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpProperties},
        {"PTP L2 offset / path delay diagnostic", "Collect PTP offset and path-delay measurements for diagnostic review.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpMeasurement},
        {"PTP L2 sustained exchange integrity", "Verify that PTP message exchanges remain valid over an extended observation period.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSustained},
        {"PTP L2 negative Delay_Req acceptance", "Verify that the DUT handles invalid PTP Delay_Req fields safely. Note: signed-DUT coverage includes authenticated PTP-field mutations, but excludes version, TLV and Ethernet-framing cases. (Requires negative-test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpNegativeDelay},
        {"PTP L2 foreign-master policy and recovery", "Verify that the DUT applies its foreign-master policy and recovers when the candidate is withdrawn. (Requires foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpForeignAnnounce},
        {"PTP L2-only IP-independent operation", "Verify that PTP L2 continues to operate without IP services. (Requires PTP L2 only firmware.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpOnly},
        {"PTP L2 GNSS loss and recovery (assisted)", "Verify that the DUT responds correctly when its GNSS reference is lost and restored. (Requires operator assistance.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpGnssRecovery},
        {"PTP L2 PPS loss and recovery (assisted)", "Verify that the DUT responds correctly when its PPS reference is lost and restored. (Requires operator assistance.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpPpsRecovery},
        {"PTP L2 server link loss and recovery (assisted)", "Verify that the DUT recovers correctly after its server link is interrupted and restored. (Requires operator assistance.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpLinkRecovery},
        {"PTP L2 restart / OTA recovery (assisted)", "Verify that the DUT resumes correct PTP service after a restart or firmware update. (Requires operator assistance.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpRestartRecovery},
        {"PTP L2 bounded simultaneous IPv4 / IPv6 NTP load", "Verify that the DUT remains responsive while serving bounded IPv4, IPv6, and PTP L2 traffic together. (Requires NTP and PTP L2 firmware.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpCombinedLoad},
        {"PTP L2 ranking precedence", "Verify that the DUT selects the preferred PTP master according to its ranking policy. (Requires foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpRankingPrecedence},
        {"PTP L2 qualification and source isolation", "Verify that the DUT qualifies trusted PTP sources without accepting unrelated candidates. (Requires foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSourceIsolation},
        {"PTP L2 duplicate, reorder and rollover policy", "Verify foreign Announce duplicate, backward, half-range and sequence-rollover qualification and lifetime behavior under the DUT's selection policy. (Requires foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSequencePolicy},
        {"PTP L2 dataset changes and self-grandmaster reflection", "Verify that a changed grandmaster must requalify and self-grandmaster reflection cannot establish or retain standby. (Requires ranked selection and foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpDatasetChanges},
        {"PTP L2 multiple better masters", "Verify continued standby across overlapping candidate lifetimes in both best/worse expiry orders, then bounded healthy recovery. (Requires ranked selection, at least two candidate slots and foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpMultipleMasters},
        {"PTP L2 interval-sensitive expiry", "Verify negative and long foreign Announce lifetimes and unsupported-interval handling under the DUT's ranked or legacy policy. (Requires foreign-master test opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpIntervalExpiry},
        {"PTP L2 service recovery while standing by (assisted)", "Verify that GNSS loss/restoration does not release standby while a better foreign master remains active, then verify healthy service after candidate expiry. (Requires ranked selection, foreign-master and assisted-test opt-ins, and operator assistance.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpStandbyRecovery},
        {"PTP L2 signed service messages", "Verify that the DUT sends PTP service messages with valid authentication.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSignedMessages},
        {"PTP L2 authenticated E2E exchange", "Verify that the DUT completes an authenticated end-to-end PTP delay exchange.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSignedExchange},
        {"PTP L2 tampered and malformed authentication", "Verify that the DUT rejects PTP messages with tampered or malformed authentication.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpInvalidAuthentication},
        {"PTP L2 mixed versus required authentication", "Verify that the DUT enforces its configured PTP authentication policy.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpAuthenticationPolicy},
        {"PTP L2 authentication before selection", "Verify that the DUT authenticates PTP candidates before considering them for master selection.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpAuthenticationBeforeSelection},
        {"PTP L2 optional unsigned service", "Verify enabled, optional keyless PTP uses unsigned v2.0 Announce, Sync, Follow_Up and Delay_Resp with base lengths, legacy controls, valid UTC metadata and hardware-timestamped exchanges. (Not applicable to signed service.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpOptionalUnsignedService},
        {"PTP L2 optional unsigned signed-input rejection", "Verify that signed-looking and tampered Delay_Req and better Announce packets cannot elicit responses or suppress healthy unsigned service. (Requires optional unsigned DUT, isolated link, negative-test and foreign-master opt-ins; no operator action.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpOptionalSignedRejection},
        {"PTP L2 unsigned Announce authentication policy", "Verify that keyed mixed or required PTP authentication accepts or rejects unsigned better Announce messages with signed controls. (Requires isolated link and foreign-master opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpAnnounceAuthenticationPolicy},
        {"PTP L2 invalid authentication cannot refresh standby", "Verify that invalid signed updates cannot extend a qualified foreign candidate's original lifetime. (Requires isolated link, negative-test and foreign-master opt-ins.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpInvalidAuthenticationRefresh},
        {"PTP L2 bounded foreign-master capacity", "Exercise configured capacity, qualified-candidate protection and stronger unqualified replacement. Internal table occupancy requires server-side instrumentation. (Requires ranked selection and foreign-master opt-in.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpForeignCapacity},
        {"PTP L2 standby with bounded IPv4 / IPv6 NTP load", "Verify NTP controls and bounded traffic while a qualified foreign master holds PTP in standby. (Requires NTP + PTP L2 firmware and load/foreign-master opt-ins.)", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSelectionNtpLoad},
        {"PTP L2 PPS recovery while standing by (assisted)", "Verify a refreshed better candidate preserves standby across controlled PPS loss and restoration.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpStandbyPpsRecovery},
        {"PTP L2 server link recovery while standing by (assisted)", "Verify standby across controlled server-link recovery while fresh candidates are transmitted during ranked startup listening.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpStandbyLinkRecovery},
        {"PTP L2 restart recovery while standing by (assisted)", "Verify standby across a controlled DUT restart while fresh candidates are transmitted during ranked startup listening.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpStandbyRestartRecovery},
        {"PTP L2 staged key rotation and revocation (assisted)", "Verify an isolated, explicitly rebuilt multi-key fixture across inbound-key acceptance, outgoing-key rotation and old-key revocation. Never use public fixture keys in production.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpKeyRotation},
        {"PTP L2 timestamp-referenced startup listening (assisted)", "Bound the worker-start time using an independently instrumented qualified DUT, continuously observe startup listening, and classify ambiguous timing as inconclusive.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpStartupListening},
        {"PTP L2 candidate expiry while timing is unqualified (assisted)", "While a qualified foreign master holds standby, independently confirm GNSS qualification loss without a generation reset, let the candidate expire, then verify immediate bounded recovery after requalification.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpStandbyExpiryUnqualified},
        {"PTP L2 standby with NTP holdover under load (assisted)", "With a qualified foreign master holding PTP in standby, independently confirm GNSS loss and legitimate NTP holdover, then verify bounded IPv4/IPv6 NTP load before restoring the DUT.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSelectionNtpHoldover},
        {"PTP L2 standby with NTP key-policy probes under load", "While a qualified foreign master holds PTP in standby, test invalid NTP authentication under bounded IPv4/IPv6 load, plus unsigned rejection when NTP authentication is required. Requires the separate target NTP key.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpSelectionNtpPolicyLoad},
        {"PTP L3 UDP IPv4 Announce and master identity", "Verify the selected IPv4 multicast master advertises a valid PTP Announce and clock identity.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::Announce},
        {"PTP L3 UDP IPv6 Announce and master identity", "Verify the selected IPv6 multicast master advertises a valid PTP Announce and clock identity.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::Announce},
        {"PTP L3 UDP IPv4 two-step Sync / Follow_Up", "Match an IPv4 multicast two-step Sync and Follow_Up from the selected master.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::SyncFollowUp},
        {"PTP L3 UDP IPv6 two-step Sync / Follow_Up", "Match an IPv6 multicast two-step Sync and Follow_Up from the selected master.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::SyncFollowUp},
        {"PTP L3 UDP IPv4 E2E Delay_Req / Delay_Resp", "Send an IPv4 multicast Delay_Req and match the selected master's response to its requester and sequence.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::DelayExchange},
        {"PTP L3 UDP IPv6 E2E Delay_Req / Delay_Resp", "Send an IPv6 multicast Delay_Req and match the selected master's response to its requester and sequence.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::DelayExchange},
        {"PTP L3 UDP IPv4 time properties and clock quality", "Validate and report the selected IPv4 master's advertised time properties and clock quality.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::TimeProperties},
        {"PTP L3 UDP IPv6 time properties and clock quality", "Validate and report the selected IPv6 master's advertised time properties and clock quality.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::TimeProperties},
        {"PTP L3 UDP IPv4 sustained exchange integrity", "Observe a bounded IPv4 multicast stream and repeated matched E2E exchanges without claiming clock accuracy.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::Sustained},
        {"PTP L3 UDP IPv6 sustained exchange integrity", "Observe a bounded IPv6 multicast stream and repeated matched E2E exchanges without claiming clock accuracy.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::Sustained},
        {"PTP L3 UDP IPv4 invalid Delay_Req rejection", "Test malformed or wrong-domain IPv4 requests with healthy before/after controls. Requires negative-test opt-in.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::NegativeDelay},
        {"PTP L3 UDP IPv6 invalid Delay_Req rejection", "Test malformed or wrong-domain IPv6 requests with healthy before/after controls. Requires negative-test opt-in.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::NegativeDelay},
        {"PTP L3 UDP simultaneous IPv4 / IPv6 service", "Observe both multicast transports concurrently and verify independent complete exchanges and a common grandmaster.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::Simultaneous},
        {"PTP L3 UDP IPv4 with bounded NTP load", "Verify IPv4 PTP multicast exchanges during bounded IPv4 NTP traffic.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::CombinedLoad},
        {"PTP L3 UDP IPv6 with bounded NTP load", "Verify IPv6 PTP multicast exchanges during bounded IPv6 NTP traffic.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::CombinedLoad},
        {"PTP L3 UDP simultaneous IPv4 / IPv6 with bounded NTP load", "Verify both PTP multicast transports concurrently during bounded IPv4 and IPv6 NTP traffic.", AF_UNSPEC, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::CombinedLoad},
        {"PTP L3 UDP IPv4 GNSS loss and recovery (assisted)", "Verify IPv4 service withdrawal and recovery during a controlled GNSS interruption.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::GnssRecovery},
        {"PTP L3 UDP IPv6 GNSS loss and recovery (assisted)", "Verify IPv6 service withdrawal and recovery during a controlled GNSS interruption.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::GnssRecovery},
        {"PTP L3 UDP IPv4 PPS loss and recovery (assisted)", "Verify IPv4 service withdrawal and recovery during a controlled PPS interruption.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::PpsRecovery},
        {"PTP L3 UDP IPv6 PPS loss and recovery (assisted)", "Verify IPv6 service withdrawal and recovery during a controlled PPS interruption.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::PpsRecovery},
        {"PTP L3 UDP IPv4 server link recovery (assisted)", "Verify IPv4 service withdrawal and recovery when the server-side Ethernet link is interrupted.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::LinkRecovery},
        {"PTP L3 UDP IPv6 server link recovery (assisted)", "Verify IPv6 service withdrawal and recovery when the server-side Ethernet link is interrupted.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::LinkRecovery},
        {"PTP L3 UDP IPv4 restart recovery (assisted)", "Verify IPv4 service withdrawal and recovery across a controlled DUT restart.", AF_INET, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::RestartRecovery},
        {"PTP L3 UDP IPv6 restart recovery (assisted)", "Verify IPv6 service withdrawal and recovery across a controlled DUT restart.", AF_INET6, PtpTestsEnabled, TestDefinition::Kind::PtpUdp, ptp_tester::UdpTest::RestartRecovery},
    }};

    constexpr auto TestOrder = []()
    {
        std::array<size_t, TestCount> order{};
        size_t position = 0;
        order[position++] = 0;
        order[position++] = 10;
        for (size_t index = 0; index < NtpTestCount; ++index)
            if (index != 0 && index != 10)
                order[position++] = index;
        const auto append_l2 = [&](bool combined, bool assisted)
        {
            for (size_t offset = 0; offset < PtpL2TestCount; ++offset)
            {
                const size_t index = ptp_tester::ordered_test_index(offset);
                const auto test = static_cast<ptp_tester::Test>(index);
                if (ptp_tester::involves_ntp(test) == combined &&
                    (combined || ptp_tester::requires_operator(test) == assisted))
                    order[position++] = NtpTestCount + index;
            }
        };
        const auto append_udp = [&](bool combined, bool assisted)
        {
            for (size_t index = NtpTestCount + PtpL2TestCount; index < TestCount; ++index)
                if (ptp_tester::udp_involves_ntp(Tests[index].udp_test) == combined &&
                    (combined || ptp_tester::udp_requires_operator(Tests[index].udp_test) == assisted))
                    order[position++] = index;
        };
        append_l2(true, false);
        append_udp(true, false);
        append_l2(false, false);
        append_udp(false, false);
        append_l2(false, true);
        append_udp(false, true);
        return order;
    }();

    constexpr auto TestNumbers = []()
    {
        std::array<size_t, TestCount> numbers{};
        for (size_t position = 0; position < TestCount; ++position)
            numbers[TestOrder[position]] = position + 1;
        return numbers;
    }();

    static_assert([]()
                  {
        for (size_t position = 0; position < TestCount; ++position)
            if (TestNumbers[TestOrder[position]] != position + 1)
                return false;
        return true; }(), "Every test must have a unique sequential display number");

    constexpr bool is_ptp_l2_test(const TestDefinition &test)
    {
        return test.kind >= TestDefinition::Kind::PtpAnnounce &&
               test.kind <= TestDefinition::Kind::PtpSelectionNtpPolicyLoad;
    }

    constexpr bool is_ptp_udp_test(const TestDefinition &test)
    {
        return test.kind == TestDefinition::Kind::PtpUdp;
    }

    constexpr bool is_ptp_test(const TestDefinition &test)
    {
        return is_ptp_l2_test(test) || is_ptp_udp_test(test);
    }

    bool configured_ptp_udp_test(const TestDefinition &test, bool ipv4, bool ipv6)
    {
        if (!is_ptp_udp_test(test))
            return true;
        return test.family == AF_INET ? ipv4 : test.family == AF_INET6 ? ipv6
                                                                       : test.family == AF_UNSPEC && ipv4 && ipv6;
    }

    ptp_tester::Test ptp_test_id(const TestDefinition &test)
    {
        static_assert(static_cast<unsigned>(TestDefinition::Kind::PtpCombinedLoad) -
                          static_cast<unsigned>(TestDefinition::Kind::PtpAnnounce) ==
                      static_cast<unsigned>(ptp_tester::Test::CombinedLoad));
        static_assert(static_cast<unsigned>(TestDefinition::Kind::PtpSelectionNtpPolicyLoad) -
                          static_cast<unsigned>(TestDefinition::Kind::PtpAnnounce) + 1 ==
                      static_cast<unsigned>(ptp_tester::Test::Count));
        return static_cast<ptp_tester::Test>(
            static_cast<unsigned>(test.kind) - static_cast<unsigned>(TestDefinition::Kind::PtpAnnounce));
    }

    bool compatible_with_firmware_mode(const TestDefinition &test, ptp_tester::FirmwareMode mode)
    {
        if (test.kind == TestDefinition::Kind::Ping)
            return mode != ptp_tester::FirmwareMode::PtpOnly;
        if (!is_ptp_test(test))
            return mode == ptp_tester::FirmwareMode::Unknown || ptp_tester::mode_has_ntp(mode);
        if (is_ptp_udp_test(test))
            return ptp_tester::udp_compatible_with_firmware_mode(test.udp_test, mode);
        return ptp_tester::compatible_with_firmware_mode(ptp_test_id(test), mode);
    }

    bool test_requires_operator(const TestDefinition &test)
    {
        return is_ptp_udp_test(test) ? ptp_tester::udp_requires_operator(test.udp_test)
                                     : is_ptp_l2_test(test) && ptp_tester::requires_operator(ptp_test_id(test));
    }

    bool test_involves_ntp(const TestDefinition &test)
    {
        return is_ptp_udp_test(test) ? ptp_tester::udp_involves_ntp(test.udp_test)
                                     : is_ptp_l2_test(test) && ptp_tester::involves_ntp(ptp_test_id(test));
    }

    const char *firmware_mode_incompatibility_reason(const TestDefinition &test, ptp_tester::FirmwareMode mode)
    {
        if (compatible_with_firmware_mode(test, mode))
            return nullptr;
        if (!is_ptp_test(test))
            return "This NTP/IP test is not applicable to the selected firmware mode.";
        if (is_ptp_udp_test(test))
            return ptp_tester::udp_involves_ntp(test.udp_test)
                       ? "This scenario requires operator-declared NTP and PTP L3 UDP firmware."
                       : "PTP L3 UDP is not included in the selected firmware mode.";
        const auto ptp_test = ptp_test_id(test);
        if (ptp_test == ptp_tester::Test::PtpOnly)
            return "PTP L2-only test requires operator-declared PTP L2 only firmware.";
        if (ptp_tester::involves_ntp(ptp_test))
            return "This PTP L2 scenario requires operator-declared NTP and PTP L2 firmware.";
        return "PTP L2 is not included in the selected firmware mode.";
    }

    std::array<TestResult, TestCount> s_results{};
    SemaphoreHandle_t s_state_mutex;
    std::atomic<bool> s_tests_running{false};
    // Shared by the current test and its workers; Skip must not stop the whole run.
    std::atomic<bool> s_stop_tests_requested{false};
    std::atomic<bool> s_stop_run_requested{false};
    bool s_ote_in_progress = false; // Protected by s_state_mutex, also used by start_tests.
#if OTE_UPDATES_ENABLED
    esp_timer_handle_t s_ote_restart_timer = nullptr;
#endif
    TargetHistoryEntry s_profile{};
    TargetHistoryStorage s_target_history{};
    size_t s_active_test = TestCount;
    char s_progress[768] = "Idle";
    char s_observed_ptp_identity[768] = "Not yet observed; run the Announce test or foreign-policy preflight.";
    char s_observed_ptp_mac[18] = {};
    uint8_t s_observed_ptp_domain = 0;
    char s_phase_prompt[768] = {};
    uint32_t s_phase_token = 0;
    bool s_phase_waiting = false;
    bool s_phase_acknowledged = false;
    bool s_phase_skipped = false;
    int64_t s_phase_deadline_us = 0;
    std::atomic<uint32_t> s_tester_link_generation{0};
    esp_netif_t *s_eth_netif = nullptr;
    esp_eth_handle_t s_eth_handle = nullptr;
    std::atomic<bool> s_ethernet_connected{false};
    std::atomic<bool> s_ethernet_link_known{false};
    bool s_ipv4_was_assigned = false;
    bool s_ipv6_lla_was_assigned = false;
    bool s_ipv6_ula_was_assigned = false;
    bool s_ipv6_gua_was_assigned = false;
    bool s_startup_ready = false;
    bool s_startup_logged = false;
    char s_startup_ipv4[INET_ADDRSTRLEN] = {};
    char s_startup_ipv6_linklocal[INET6_ADDRSTRLEN] = {};
    char s_ipv6_ula[INET6_ADDRSTRLEN] = {};
    char s_ipv6_gua[INET6_ADDRSTRLEN] = {};
    char s_ipv6_in_use[INET6_ADDRSTRLEN] = {};
    led_strip_handle_t s_led_strip = nullptr;
    bool s_physical_leds_on = true;   // Protected by s_state_mutex after startup.
    bool s_test_leds_flash_on = true; // Protected by s_state_mutex.
    esp_timer_handle_t s_test_led_flash_timer = nullptr;

    enum class StatusColor
    {
        Blue,
        Green,
        Yellow,
        Red,
        Off,
        Orange
    };

    // Caller holds s_state_mutex.
    bool ipv4_available_locked() { return s_startup_ipv4[0] != '\0'; }

    // Caller holds s_state_mutex. Returns "gua", "ula", "lla" or "none".
    const char *ipv6_level_locked()
    {
        return s_ipv6_gua[0] != '\0'                 ? "gua"
               : s_ipv6_ula[0] != '\0'               ? "ula"
               : s_startup_ipv6_linklocal[0] != '\0' ? "lla"
                                                     : "none";
    }

    esp_err_t set_status_led(int index, StatusColor color)
    {
        if (s_led_strip == nullptr)
            return ESP_ERR_INVALID_STATE;
        const uint32_t level = 255U * static_cast<uint32_t>(std::clamp(RGB_LED_Brightness_Percent, 0, 100)) / 100U;
        uint32_t red = 0, green = 0, blue = 0;
        switch (color)
        {
        case StatusColor::Blue:
            blue = level;
            break;
        case StatusColor::Green:
            green = level;
            break;
        case StatusColor::Yellow:
            red = level;
            green = level;
            break;
        case StatusColor::Red:
            red = level;
            break;
        case StatusColor::Off:
            break;
        case StatusColor::Orange:
            red = level;
            green = level * 165U / 255U;
            break;
        }
        return led_strip_set_pixel(s_led_strip, index, red, green, blue);
    }

    // Caller holds s_state_mutex.
    esp_err_t write_status_leds_locked(StatusColor ipv4, StatusColor ipv6)
    {
        if (!WS2812_RGB_enabled)
            return ESP_OK;
        if (s_led_strip == nullptr)
            return ESP_ERR_INVALID_STATE;
        esp_err_t result = set_status_led(0, ipv4);
        if (result == ESP_OK)
            result = set_status_led(1, ipv6);
        if (result == ESP_OK)
            result = led_strip_refresh(s_led_strip);
        if (result != ESP_OK)
            ESP_LOGE(Tag, "Unable to update physical LEDs: %s", esp_err_to_name(result));
        return result;
    }

    esp_err_t update_status_leds_locked();

    void init_status_leds()
    {

        if (WS2812_RGB_enabled)
        {

            static_assert(
                RGB_LED_Brightness_Percent >= 0 && RGB_LED_Brightness_Percent <= 100,
                "RGB_LED_Brightness_Percent must be between 0 and 100");

            led_strip_config_t strip_config = {};
            strip_config.strip_gpio_num = WS2812DataPin;
            strip_config.max_leds = 2;
            strip_config.led_model = LED_MODEL_WS2812;
            strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
            led_strip_rmt_config_t rmt_config = {};
            rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
            rmt_config.resolution_hz = 10 * 1000 * 1000;
            // Fit both 24-bit pixels, reset and EOF without a mid-frame ISR refill.
            rmt_config.mem_block_symbols = 64;
            if (led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip) != ESP_OK)
            {
                ESP_LOGE(Tag, "Unable to initialize WS2812 RGB LEDs");
                s_led_strip = nullptr;
                return;
            }
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
    }

    // Caller holds s_state_mutex.
    esp_err_t update_status_leds_locked()
    {
        if (WS2812_RGB_enabled)
        {
            if (s_led_strip == nullptr)
                return ESP_ERR_INVALID_STATE;
            StatusColor ipv4 = StatusColor::Blue;
            StatusColor ipv6 = StatusColor::Blue;
            if (!s_physical_leds_on)
            {
                ipv4 = StatusColor::Off;
                ipv6 = StatusColor::Off;
            }
            else if (s_tests_running.load() && !s_test_leds_flash_on)
            {
                ipv4 = StatusColor::Off;
                ipv6 = StatusColor::Off;
            }
            else if (s_ote_in_progress)
            {
                ipv4 = StatusColor::Orange;
                ipv6 = StatusColor::Orange;
            }
            else if (!s_startup_logged)
            {
                ipv4 = StatusColor::Blue;
                ipv6 = StatusColor::Blue;
            }
            else if (s_ethernet_link_known.load() && !s_ethernet_connected.load())
            {
                ipv4 = StatusColor::Red;
                ipv6 = StatusColor::Red;
            }
            else
            {
                ipv4 = ipv4_available_locked() ? StatusColor::Green : StatusColor::Red;
                const char *level = ipv6_level_locked();
                ipv6 = std::strcmp(level, "gua") == 0   ? StatusColor::Blue
                       : std::strcmp(level, "ula") == 0 ? StatusColor::Green
                       : std::strcmp(level, "lla") == 0 ? StatusColor::Yellow
                                                        : StatusColor::Red;
            }
            return write_status_leds_locked(ipv4, ipv6);
        }
        return ESP_OK;
    }

    void test_led_flash_timer_callback(void *)
    {
        if (xSemaphoreTake(s_state_mutex, 0) != pdTRUE)
            return;
        if (s_tests_running.load() && s_physical_leds_on && s_led_strip != nullptr)
        {
            s_test_leds_flash_on = !s_test_leds_flash_on;
            update_status_leds_locked();
        }
        xSemaphoreGive(s_state_mutex);
    }

    void init_test_led_flash_timer()
    {
        if constexpr (WS2812_RGB_enabled)
        {
            esp_timer_create_args_t timer{};
            timer.callback = test_led_flash_timer_callback;
            timer.name = "test_led_flash";
            esp_err_t result = esp_timer_create(&timer, &s_test_led_flash_timer);
            if (result != ESP_OK)
            {
                ESP_LOGE(Tag, "Unable to create test LED flash timer: %s", esp_err_to_name(result));
                return;
            }
            result = esp_timer_start_periodic(s_test_led_flash_timer, 1000000); // 1.0 second interval
            if (result != ESP_OK)
            {
                ESP_LOGE(Tag, "Unable to start test LED flash timer: %s", esp_err_to_name(result));
                const esp_err_t delete_result = esp_timer_delete(s_test_led_flash_timer);
                if (delete_result != ESP_OK)
                    ESP_LOGE(Tag, "Unable to delete test LED flash timer: %s", esp_err_to_name(delete_result));
                s_test_led_flash_timer = nullptr;
            }
        }
    }

    using ResponseBuffer = std::unique_ptr<char, decltype(&std::free)>;

    ResponseBuffer allocate_response_buffer(size_t size)
    {
        return ResponseBuffer(static_cast<char *>(std::malloc(size)), &std::free);
    }

    void format_number_with_thousands(int value, char *output, size_t output_size)
    {
        char digits[12];
        std::snprintf(digits, sizeof(digits), "%d", value);
        const size_t digit_count = std::strlen(digits);
        const size_t comma_count = (digit_count - 1) / 3;
        if (digit_count + comma_count + 1 > output_size)
        {
            output[0] = '\0';
            return;
        }

        size_t source = digit_count;
        size_t destination = digit_count + comma_count;
        output[destination--] = '\0';
        while (source > 0)
        {
            output[destination--] = digits[--source];
            if (source > 0 && (digit_count - source) % 3 == 0)
                output[destination--] = ',';
        }
    }

    const char *status_text(TestStatus status)
    {
        switch (status)
        {
        case TestStatus::Running:
            return "Running";
        case TestStatus::AwaitingOperatorAction:
            return "Awaiting operator action";
        case TestStatus::TimedOutAwaitingOperatorAction:
            return "Timed out awaiting operator action";
        case TestStatus::Pass:
            return "Pass";
        case TestStatus::Fail:
            return "Fail";
        case TestStatus::CouldNotRun:
            return "Could not run";
        case TestStatus::DidNotRun:
            return "Did not run";
        case TestStatus::Skipped:
            return "Skipped";
        default:
            return "Not yet run";
        }
    }

    bool has_test_key(const symmetric_key_t &key)
    {
        return key.key_id != 0 && key.key_value[0] != '\0';
    }

    bool valid_key_value(const char *value)
    {
        if (*value == '\0')
            return true;
        if (std::strlen(value) != 32)
            return false;
        for (const char *character = value; *character != '\0'; ++character)
        {
            if (!(*character >= '0' && *character <= '9') &&
                !(*character >= 'A' && *character <= 'Z') &&
                !(*character >= 'a' && *character <= 'z'))
                return false;
        }
        return true;
    }

    bool equals_ignore_case(const char *left, const char *right)
    {
        while (*left != '\0' && *right != '\0')
        {
            if (std::tolower(static_cast<unsigned char>(*left)) != std::tolower(static_cast<unsigned char>(*right)))
                return false;
            ++left;
            ++right;
        }
        return *left == *right;
    }

    bool valid_target_name(const char *target)
    {
        const size_t length = std::strlen(target);
        if (length == 0 || length >= TargetNameSize)
            return false;
        for (const unsigned char *character = reinterpret_cast<const unsigned char *>(target); *character; ++character)
        {
            if (*character < 32 || *character > 126)
                return false;
        }
        return true;
    }

    int compare_targets(const char *left, const char *right)
    {
        return std::strcmp(left, right);
    }

    void remove_history_entry(size_t index)
    {
        if (index + 1 < s_target_history.count)
            std::memmove(&s_target_history.entries[index], &s_target_history.entries[index + 1],
                         (s_target_history.count - index - 1) * sizeof(TargetHistoryEntry));
        --s_target_history.count;
        s_target_history.entries[s_target_history.count] = {};
    }

    TargetHistoryEntry *find_history_entry(const char *target)
    {
        for (size_t index = 0; index < s_target_history.count; ++index)
        {
            if (std::strcmp(s_target_history.entries[index].target, target) == 0)
                return &s_target_history.entries[index];
        }
        return nullptr;
    }

    uint64_t timestamp_sort_value(const char *timestamp)
    {
        if (std::strlen(timestamp) < 19)
            return 0;
        uint64_t value = 0;
        constexpr size_t DigitPositions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
        for (size_t position : DigitPositions)
        {
            if (!std::isdigit(static_cast<unsigned char>(timestamp[position])))
                return 0;
            value = value * 10 + static_cast<uint64_t>(timestamp[position] - '0');
        }
        return value;
    }

    void add_or_update_history_entry(const TargetHistoryEntry &profile)
    {
        TargetHistoryEntry *entry = find_history_entry(profile.target);
        if (entry == nullptr)
        {
            if (s_target_history.count == MaxTargetHistory)
            {
                size_t oldest = 0;
                for (size_t index = 1; index < s_target_history.count; ++index)
                {
                    const auto &candidate = s_target_history.entries[index];
                    const auto &current = s_target_history.entries[oldest];
                    if (candidate.last_stored < current.last_stored ||
                        (candidate.last_stored == current.last_stored && compare_targets(candidate.target, current.target) < 0))
                        oldest = index;
                }
                remove_history_entry(oldest);
            }
            entry = &s_target_history.entries[s_target_history.count++];
        }
        *entry = profile;
        std::sort(s_target_history.entries.begin(), s_target_history.entries.begin() + s_target_history.count,
                  [](const TargetHistoryEntry &left, const TargetHistoryEntry &right)
                  { return compare_targets(left.target, right.target) < 0; });
    }

    bool is_self_target(const char *target)
    {
        char hostname_with_local[sizeof(Hostname) + 7];
        std::snprintf(hostname_with_local, sizeof(hostname_with_local), "%s.local", Hostname);
        if (equals_ignore_case(target, Hostname) || equals_ignore_case(target, hostname_with_local))
            return true;

        in_addr loopback_ipv4{};
        if (inet_pton(AF_INET, target, &loopback_ipv4) == 1 &&
            ((ntohl(loopback_ipv4.s_addr) >> 24) == 127 || loopback_ipv4.s_addr == 0))
            return true;
        in6_addr loopback_ipv6{};
        if (inet_pton(AF_INET6, target, &loopback_ipv6) == 1)
        {
            const uint8_t *bytes = loopback_ipv6.s6_addr;
            if (std::all_of(bytes, bytes + 15, [](uint8_t byte)
                            { return byte == 0; }) &&
                bytes[15] <= 1)
                return true;
            if (std::all_of(bytes, bytes + 10, [](uint8_t byte)
                            { return byte == 0; }) &&
                bytes[10] == 0xFF && bytes[11] == 0xFF)
            {
                char mapped[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, bytes + 12, mapped, sizeof(mapped));
                return is_self_target(mapped);
            }
        }
        if (s_eth_netif == nullptr)
            return false;
        in_addr target_ipv4{};
        esp_netif_ip_info_t ipv4{};
        if (inet_pton(AF_INET, target, &target_ipv4) == 1 &&
            esp_netif_get_ip_info(s_eth_netif, &ipv4) == ESP_OK && target_ipv4.s_addr == ipv4.ip.addr)
            return true;

        in6_addr target_ipv6{};
        if (inet_pton(AF_INET6, target, &target_ipv6) != 1)
            return false;
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        const int count = esp_netif_get_all_ip6(s_eth_netif, addresses);
        for (int index = 0; index < count; ++index)
        {
            if (std::memcmp(&target_ipv6, &addresses[index], sizeof(target_ipv6)) == 0)
                return true;
        }
        return false;
    }

    bool target_requires_external_warning()
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        xSemaphoreGive(s_state_mutex);
        esp_netif_ip_info_t local_ipv4{};
        const bool has_ipv4 = s_eth_netif != nullptr && esp_netif_get_ip_info(s_eth_netif, &local_ipv4) == ESP_OK;
        for (const char *target : {profile.ipv4, profile.ptp_udp_ipv4})
        {
            in_addr ipv4{};
            if (inet_pton(AF_INET, target, &ipv4) != 1)
                continue;
            const uint32_t candidate = ntohl(ipv4.s_addr);
            const bool loopback = (candidate >> 24) == 127;
            const bool link_local = (candidate >> 16) == 0xA9FE;
            const bool local_network = has_ipv4 &&
                                       ((candidate & ntohl(local_ipv4.netmask.addr)) ==
                                        (ntohl(local_ipv4.ip.addr) & ntohl(local_ipv4.netmask.addr)));
            if (!loopback && !link_local && !local_network)
                return true;
        }
        for (const char *target : {profile.ipv6, profile.ptp_udp_ipv6})
        {
            in6_addr ipv6{};
            if (inet_pton(AF_INET6, target, &ipv6) != 1)
                continue;
            const uint8_t *bytes = ipv6.s6_addr;
            const bool loopback = std::all_of(bytes, bytes + 15, [](uint8_t byte)
                                              { return byte == 0; }) &&
                                  bytes[15] == 1;
            const bool link_local = bytes[0] == 0xFE && (bytes[1] & 0xC0) == 0x80;
            const bool unique_local = (bytes[0] & 0xFE) == 0xFC;
            if (!loopback && !link_local && !unique_local)
                return true;
        }
        return false;
    }

    bool parse_mac_address(const char *text, uint8_t output[6])
    {
        if (std::strlen(text) != 17)
            return false;
        for (size_t index = 0; index < 6; ++index)
        {
            const size_t offset = index * 3;
            if ((index != 5 && text[offset + 2] != ':') ||
                !std::isxdigit(static_cast<unsigned char>(text[offset])) ||
                !std::isxdigit(static_cast<unsigned char>(text[offset + 1])))
                return false;
            const auto hex_value = [](char digit)
            {
                return std::isdigit(static_cast<unsigned char>(digit)) ? digit - '0' : std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10;
            };
            output[index] = static_cast<uint8_t>((hex_value(text[offset]) << 4) | hex_value(text[offset + 1]));
        }
        return (output[0] & 0x01U) == 0;
    }

    bool valid_address(const char *address, int family)
    {
        if (*address == '\0')
            return true;
        for (const char *character = address; *character; ++character)
        {
            const bool allowed = family == AF_INET
                                     ? ((*character >= '0' && *character <= '9') || *character == '.')
                                     : (std::isxdigit(static_cast<unsigned char>(*character)) || *character == ':');
            if (!allowed)
                return false;
        }
        in6_addr parsed{};
        return inet_pton(family, address, &parsed) == 1;
    }

    bool valid_ptp_mac(const char *text)
    {
        if (*text == '\0')
            return true;
        uint8_t mac[6]{};
        if (!parse_mac_address(text, mac) || std::all_of(mac, mac + 6, [](uint8_t byte)
                                                         { return byte == 0; }))
            return false;
        uint8_t local[6]{};
        return s_eth_netif == nullptr || esp_netif_get_mac(s_eth_netif, local) != ESP_OK ||
               std::memcmp(mac, local, sizeof(mac)) != 0;
    }

    bool valid_ptp_udp_target(const char *text, int family)
    {
        if (*text == '\0')
            return true;
        if (!valid_address(text, family) || is_self_target(text))
            return false;
        if (family == AF_INET)
        {
            in_addr address{};
            if (inet_pton(AF_INET, text, &address) != 1)
                return false;
            const uint32_t host = ntohl(address.s_addr);
            return (host >> 24) != 0 && (host >> 24) != 127 && (host >> 28) < 14;
        }
        in6_addr address{};
        if (inet_pton(AF_INET6, text, &address) != 1 || address.s6_addr[0] == 0xff)
            return false;
        const auto *bytes = address.s6_addr;
        const bool first_15_zero = std::all_of(bytes, bytes + 15, [](uint8_t octet)
                                               { return octet == 0; });
        const bool ipv4_mapped = std::all_of(bytes, bytes + 10, [](uint8_t octet)
                                             { return octet == 0; }) &&
                                 bytes[10] == 0xff && bytes[11] == 0xff;
        return !(first_15_zero && (bytes[15] == 0 || bytes[15] == 1)) && !ipv4_mapped;
    }

    bool valid_profile(const TargetHistoryEntry &profile)
    {
        if (std::memchr(profile.target, '\0', sizeof(profile.target)) == nullptr ||
            std::memchr(profile.ipv4, '\0', sizeof(profile.ipv4)) == nullptr ||
            std::memchr(profile.ipv6, '\0', sizeof(profile.ipv6)) == nullptr ||
            std::memchr(profile.key.key_value, '\0', sizeof(profile.key.key_value)) == nullptr ||
            std::memchr(profile.ptp_key.key_value, '\0', sizeof(profile.ptp_key.key_value)) == nullptr ||
            std::memchr(profile.ptp_mac, '\0', sizeof(profile.ptp_mac)) == nullptr ||
            std::memchr(profile.ptp_udp_ipv4, '\0', sizeof(profile.ptp_udp_ipv4)) == nullptr ||
            std::memchr(profile.ptp_udp_ipv6, '\0', sizeof(profile.ptp_udp_ipv6)) == nullptr)
            return false;
        return valid_target_name(profile.target) && valid_key_value(profile.key.key_value) && valid_key_value(profile.ptp_key.key_value) &&
               valid_address(profile.ipv4, AF_INET) && valid_address(profile.ipv6, AF_INET6) &&
               valid_ptp_mac(profile.ptp_mac) &&
               valid_ptp_udp_target(profile.ptp_udp_ipv4, AF_INET) &&
               valid_ptp_udp_target(profile.ptp_udp_ipv6, AF_INET6) &&
               profile.ptp_udp_withdrawal_ms >= 1000 && profile.ptp_udp_withdrawal_ms <= 60000 &&
               profile.firmware_mode <= ptp_tester::FirmwareMode::NtpPtpL2PtpL3;
    }

    void save_settings()
    {
        nvs_handle_t nvs;
        ESP_ERROR_CHECK(nvs_open(NvsNamespace, NVS_READWRITE, &nvs));
        ESP_ERROR_CHECK(nvs_set_str(nvs, "selected_v1", s_profile.target));
        ESP_ERROR_CHECK(nvs_set_blob(nvs, TargetHistoryNvsKey, &s_target_history, sizeof(s_target_history)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
        nvs_close(nvs);
    }

    void load_physical_led_setting()
    {
        nvs_handle_t nvs;
        ESP_ERROR_CHECK(nvs_open(NvsNamespace, NVS_READWRITE, &nvs));
        uint8_t enabled = 1;
        const esp_err_t result = nvs_get_u8(nvs, PhysicalLedsNvsKey, &enabled);
        if (result == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_ERROR_CHECK(nvs_set_u8(nvs, PhysicalLedsNvsKey, enabled));
            ESP_ERROR_CHECK(nvs_commit(nvs));
        }
        else
            ESP_ERROR_CHECK(result);
        nvs_close(nvs);
        if (enabled > 1)
        {
            ESP_LOGE(Tag, "Invalid physical LED preference in NVS: %u", enabled);
            ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
        }
        s_physical_leds_on = enabled != 0;
    }

    void load_settings()
    {
        nvs_handle_t nvs;
        ESP_ERROR_CHECK(nvs_open(NvsNamespace, NVS_READWRITE, &nvs));
        s_profile = {};
        size_t history_size = sizeof(s_target_history);
        esp_err_t history_result = nvs_get_blob(nvs, TargetHistoryNvsKey, &s_target_history, &history_size);
        bool changed = false;
        bool history_changed = false;
        if (history_result == ESP_ERR_NVS_NOT_FOUND)
        {
            auto legacy = std::unique_ptr<TargetHistoryStorageV4>(new (std::nothrow) TargetHistoryStorageV4{});
            if (!legacy)
                ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
            size_t legacy_size = sizeof(*legacy);
            const esp_err_t result = nvs_get_blob(nvs, "profiles_v4", legacy.get(), &legacy_size);
            if (result == ESP_OK && legacy_size == sizeof(*legacy) && legacy->count <= MaxTargetHistory)
            {
                s_target_history = {};
                s_target_history.count = legacy->count;
                for (size_t index = 0; index < legacy->count; ++index)
                    static_cast<TargetHistoryEntryV4 &>(s_target_history.entries[index]) = legacy->entries[index];
                history_result = ESP_OK;
                history_size = sizeof(s_target_history);
                history_changed = true;
                changed = true;
            }
            else if (result != ESP_ERR_NVS_NOT_FOUND)
                ESP_LOGW(Tag, "Cannot migrate v4 target history: %s (size=%u)", esp_err_to_name(result), static_cast<unsigned>(legacy_size));
        }
        if (history_result == ESP_ERR_NVS_NOT_FOUND)
        {
            auto legacy = std::unique_ptr<TargetHistoryStorageV3>(new (std::nothrow) TargetHistoryStorageV3{});
            if (!legacy)
                ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
            size_t legacy_size = sizeof(*legacy);
            const esp_err_t result = nvs_get_blob(nvs, "profiles_v3", legacy.get(), &legacy_size);
            if (result == ESP_OK && legacy_size == sizeof(*legacy) && legacy->count <= MaxTargetHistory)
            {
                s_target_history = {};
                s_target_history.count = legacy->count;
                for (size_t index = 0; index < legacy->count; ++index)
                    static_cast<TargetHistoryEntryV3 &>(s_target_history.entries[index]) = legacy->entries[index];
                history_result = ESP_OK;
                history_size = sizeof(s_target_history);
                history_changed = true;
                changed = true;
            }
            else if (result != ESP_ERR_NVS_NOT_FOUND)
                ESP_LOGW(Tag, "Cannot migrate v3 target history: %s (size=%u)", esp_err_to_name(result), static_cast<unsigned>(legacy_size));
        }
        if (history_result == ESP_ERR_NVS_NOT_FOUND)
        {
            auto legacy = std::unique_ptr<TargetHistoryStorageV2>(new (std::nothrow) TargetHistoryStorageV2{});
            if (!legacy)
                ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
            size_t legacy_size = sizeof(*legacy);
            const esp_err_t result = nvs_get_blob(nvs, "profiles_v2", legacy.get(), &legacy_size);
            if (result == ESP_OK && legacy_size == sizeof(*legacy) && legacy->count <= MaxTargetHistory)
            {
                s_target_history = {};
                s_target_history.count = legacy->count;
                for (size_t index = 0; index < legacy->count; ++index)
                    static_cast<TargetHistoryEntryV2 &>(s_target_history.entries[index]) = legacy->entries[index];
                history_result = ESP_OK;
                history_size = sizeof(s_target_history);
                history_changed = true;
                changed = true;
            }
            else if (result != ESP_ERR_NVS_NOT_FOUND)
                ESP_LOGW(Tag, "Cannot migrate v2 target history: %s (size=%u)", esp_err_to_name(result), static_cast<unsigned>(legacy_size));
        }
        if (history_result == ESP_ERR_NVS_NOT_FOUND)
        {
            auto legacy = std::unique_ptr<TargetHistoryStorageV1>(new (std::nothrow) TargetHistoryStorageV1{});
            if (!legacy)
                ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
            size_t legacy_size = sizeof(*legacy);
            if (nvs_get_blob(nvs, "profiles_v1", legacy.get(), &legacy_size) == ESP_OK &&
                legacy_size == sizeof(*legacy) && legacy->count <= MaxTargetHistory)
            {
                s_target_history = {};
                s_target_history.count = legacy->count;
                for (size_t index = 0; index < legacy->count; ++index)
                    static_cast<TargetHistoryEntryV1 &>(s_target_history.entries[index]) = legacy->entries[index];
                history_result = ESP_OK;
                history_size = sizeof(s_target_history);
                history_changed = true;
                changed = true;
            }
        }
        if (history_result != ESP_OK || history_size != sizeof(s_target_history) ||
            s_target_history.count > MaxTargetHistory)
        {
            s_target_history = {};
            if (history_result != ESP_ERR_NVS_NOT_FOUND)
            {
                ESP_ERROR_CHECK(nvs_erase_key(nvs, TargetHistoryNvsKey));
                changed = true;
            }
        }
        for (size_t index = 0; index < s_target_history.count;)
        {
            auto &entry = s_target_history.entries[index];
            bool duplicate = false;
            if (valid_profile(entry))
            {
                for (size_t previous = 0; previous < index; ++previous)
                    duplicate = duplicate || std::strcmp(s_target_history.entries[previous].target, entry.target) == 0;
            }
            if (!valid_profile(entry) || duplicate || entry.last_stored == std::numeric_limits<uint64_t>::max())
            {
                remove_history_entry(index);
                history_changed = true;
                changed = true;
                continue;
            }
            ++index;
        }
        std::sort(s_target_history.entries.begin(), s_target_history.entries.begin() + s_target_history.count,
                  [](const TargetHistoryEntry &left, const TargetHistoryEntry &right)
                  { return compare_targets(left.target, right.target) < 0; });
        char selected[TargetNameSize]{};
        size_t selected_size = sizeof(selected);
        const esp_err_t selected_result = nvs_get_str(nvs, "selected_v1", selected, &selected_size);
        const bool selected_valid = selected_result == ESP_OK && valid_target_name(selected);
        if (selected_valid)
        {
            const auto *entry = find_history_entry(selected);
            if (entry != nullptr)
                s_profile = *entry;
        }
        if (s_profile.target[0] == '\0')
            s_profile = s_target_history.count == 0 ? TargetHistoryEntry{} : s_target_history.entries[0];
        if (selected_result != ESP_ERR_NVS_NOT_FOUND &&
            (!selected_valid || std::strcmp(selected, s_profile.target) != 0))
        {
            ESP_ERROR_CHECK(nvs_erase_key(nvs, "selected_v1"));
            changed = true;
        }
        if (s_profile.target[0] != '\0' &&
            (!selected_valid || std::strcmp(selected, s_profile.target) != 0))
        {
            ESP_ERROR_CHECK(nvs_set_str(nvs, "selected_v1", s_profile.target));
            changed = true;
        }
        for (const char *key : {"target_history", "target", "ntp_port", "key_id", "key_value", "stress_tests", "mac"})
        {
            const esp_err_t result = nvs_erase_key(nvs, key);
            if (result == ESP_OK)
                changed = true;
            else if (result != ESP_ERR_NVS_NOT_FOUND)
                ESP_ERROR_CHECK(result);
        }
        if (history_changed)
            ESP_ERROR_CHECK(nvs_set_blob(nvs, TargetHistoryNvsKey, &s_target_history, sizeof(s_target_history)));
        if (history_changed || history_result == ESP_OK)
        {
            for (const char *key : {"profiles_v1", "profiles_v2", "profiles_v3", "profiles_v4"})
            {
                const esp_err_t legacy_result = nvs_erase_key(nvs, key);
                if (legacy_result == ESP_OK)
                    changed = true;
                else if (legacy_result != ESP_ERR_NVS_NOT_FOUND)
                    ESP_ERROR_CHECK(legacy_result);
            }
        }
        if (changed)
            ESP_ERROR_CHECK(nvs_commit(nvs));
        nvs_close(nvs);
    }

    bool stress_tests_enabled()
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const bool enabled = s_profile.stress_tests;
        xSemaphoreGive(s_state_mutex);
        return enabled;
    }

    bool family_available(int family)
    {
        if (family == AF_INET)
        {
            esp_netif_ip_info_t ip{};
            return s_eth_netif != nullptr && esp_netif_get_ip_info(s_eth_netif, &ip) == ESP_OK && ip.ip.addr != 0;
        }
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        return s_eth_netif != nullptr && esp_netif_get_all_preferred_ip6(s_eth_netif, addresses) > 0;
    }

    bool test_available(const TestDefinition &test, const TargetHistoryEntry &profile)
    {
        if (is_ptp_udp_test(test))
        {
            const bool ipv4 = test.family == AF_INET || test.family == AF_UNSPEC;
            const bool ipv6 = test.family == AF_INET6 || test.family == AF_UNSPEC;
            return test.enabled && !disabled_ptp_setting(test) && valid_profile(profile) && s_ethernet_connected.load() &&
                   configured_ptp_udp_test(test, profile.ptp_udp_ipv4[0] != '\0', profile.ptp_udp_ipv6[0] != '\0') &&
                   (!ipv4 || family_available(AF_INET)) && (!ipv6 || family_available(AF_INET6));
        }
        if (is_ptp_l2_test(test))
            return test.enabled && !disabled_ptp_setting(test) && valid_profile(profile) && profile.ptp_mac[0] != '\0' &&
                   s_ethernet_connected.load() && ptp_tester::ready();
        const char *address = test.family == AF_INET ? profile.ipv4 : profile.ipv6;
        const uint16_t port = test.family == AF_INET ? profile.ipv4_ntp_port : profile.ipv6_ntp_port;
        if (!test.enabled || !valid_profile(profile) || address[0] == '\0' ||
            is_self_target(address) || !family_available(test.family))
            return false;
        if (test.kind == TestDefinition::Kind::Ping)
            return true;
        if (port == 0)
            return false;
        if (test.kind == TestDefinition::Kind::MiniStress)
            return profile.stress_tests;
        if (test.kind == TestDefinition::Kind::Auth || test.kind == TestDefinition::Kind::AuthInterleaved ||
            test.kind == TestDefinition::Kind::InvalidAuth || test.kind == TestDefinition::Kind::MissingAuth ||
            test.kind == TestDefinition::Kind::TruncatedMac)
            return has_test_key(profile.key);
        return true;
    }

    void format_ntp_timestamp(const uint8_t *packet, char *output, size_t output_size)
    {
        uint32_t seconds_network;
        uint32_t fraction_network;
        std::memcpy(&seconds_network, packet + 40, sizeof(seconds_network));
        std::memcpy(&fraction_network, packet + 44, sizeof(fraction_network));
        const uint64_t seconds = ntohl(seconds_network);
        if (seconds < NtpEpochOffset)
        {
            std::snprintf(output, output_size, "n/a");
            return;
        }
        const time_t unix_seconds = static_cast<time_t>(seconds - NtpEpochOffset);
        const tm *utc = gmtime(&unix_seconds);
        if (utc == nullptr)
        {
            std::snprintf(output, output_size, "n/a");
            return;
        }
        const uint64_t nanoseconds = (static_cast<uint64_t>(ntohl(fraction_network)) * 1000000000ULL) >> 32;
        std::snprintf(output, output_size, "%04d-%02d-%02d %02d:%02d:%02d.%09llu (UTC)",
                      utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday, utc->tm_hour,
                      utc->tm_min, utc->tm_sec, static_cast<unsigned long long>(nanoseconds));
    }

    struct NtpExchange
    {
        uint64_t t1;
        uint64_t t2;
        uint64_t t3;
        uint64_t t4;
    };

    uint64_t ntp_timestamp_now()
    {
        const uint64_t microseconds = esp_timer_get_time();
        return ((NtpEpochOffset + microseconds / 1000000ULL) << 32) |
               ((microseconds % 1000000ULL) << 32) / 1000000ULL;
    }

    void write_ntp_timestamp(uint8_t *packet, size_t offset, uint64_t timestamp)
    {
        for (size_t index = 0; index < 8; ++index)
            packet[offset + index] = static_cast<uint8_t>(timestamp >> (56 - index * 8));
    }

    uint64_t read_ntp_timestamp(const uint8_t *packet, size_t offset)
    {
        uint64_t timestamp = 0;
        for (size_t index = 0; index < 8; ++index)
            timestamp = (timestamp << 8) | packet[offset + index];
        return timestamp;
    }

    uint32_t read_u32_be(const uint8_t *bytes)
    {
        return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
    }

    bool target_destination(int family, sockaddr_storage *destination, socklen_t *destination_length, bool ntp)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        xSemaphoreGive(s_state_mutex);
        const char *target = family == AF_INET ? profile.ipv4 : profile.ipv6;
        const uint16_t port = family == AF_INET ? profile.ipv4_ntp_port : profile.ipv6_ntp_port;
        if (target[0] == '\0' || is_self_target(target) || (ntp && port == 0))
            return false;
        *destination = {};
        if (family == AF_INET)
        {
            auto *address = reinterpret_cast<sockaddr_in *>(destination);
            address->sin_family = AF_INET;
            address->sin_port = htons(port);
            if (inet_pton(AF_INET, target, &address->sin_addr) != 1)
                return false;
            *destination_length = sizeof(*address);
        }
        else if (family == AF_INET6)
        {
            auto *address = reinterpret_cast<sockaddr_in6 *>(destination);
            address->sin6_family = AF_INET6;
            address->sin6_port = htons(port);
            if (inet_pton(AF_INET6, target, &address->sin6_addr) != 1)
                return false;
            if (address->sin6_addr.s6_addr[0] == 0xFE && (address->sin6_addr.s6_addr[1] & 0xC0) == 0x80)
                address->sin6_scope_id = esp_netif_get_netif_impl_index(s_eth_netif);
            *destination_length = sizeof(*address);
        }
        else
            return false;
        return true;
    }

    bool open_ntp_socket(int family, int *socket_fd, sockaddr_storage *destination, socklen_t *destination_length)
    {
        if (!target_destination(family, destination, destination_length, true))
            return false;
        *socket_fd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
        if (*socket_fd < 0)
            return false;
        timeval timeout{3, 0};
        if (setsockopt(*socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
        {
            close(*socket_fd);
            *socket_fd = -1;
            return false;
        }
        return true;
    }

    bool ntp_standard_exchange(int socket_fd, const sockaddr *destination, socklen_t destination_length,
                               uint8_t version, NtpExchange *exchange)
    {
        std::array<uint8_t, NtpPacketSize> request{};
        request[0] = static_cast<uint8_t>((version << 3) | 3);
        exchange->t1 = ntp_timestamp_now();
        write_ntp_timestamp(request.data(), 40, exchange->t1);
        const bool sent = sendto(socket_fd, request.data(), request.size(), 0, destination, destination_length) ==
                          static_cast<ssize_t>(request.size());
        std::array<uint8_t, 128> response{};
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        exchange->t4 = ntp_timestamp_now();
        if (received < NtpPacketSize || (response[0] & 0x07) != 4)
            return false;
        exchange->t2 = read_ntp_timestamp(response.data(), 32);
        exchange->t3 = read_ntp_timestamp(response.data(), 40);
        return true;
    }

    bool append_ntp_authentication(uint8_t *packet, size_t capacity, size_t *packet_length, const symmetric_key_t &key);
    bool verify_ntp_authentication(const uint8_t *packet, size_t packet_length, const symmetric_key_t &key);
    bool ntp_authenticated_standard_exchange(int socket_fd, const sockaddr *destination, socklen_t destination_length,
                                             NtpExchange *exchange, const symmetric_key_t &key);

    bool ntp_exchange(int family, uint8_t version, char *timestamp, size_t timestamp_size,
                      uint32_t timeout_ms = 0, int *failure = nullptr,
                      const std::atomic<bool> *worker_stop = nullptr, const symmetric_key_t *key = nullptr,
                      bool *explicit_auth_rejection = nullptr)
    {
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (failure != nullptr)
            *failure = EIO;
        if (explicit_auth_rejection != nullptr)
            *explicit_auth_rejection = false;
        if (timeout_ms != 0 && (s_stop_tests_requested.load() || (worker_stop && worker_stop->load())))
        {
            if (failure != nullptr)
                *failure = ECANCELED;
            return false;
        }
        errno = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
        {
            if (failure != nullptr)
                *failure = errno ? errno : EINVAL;
            return false;
        }
        NtpExchange exchange{};
        bool succeeded = false;
        if (timeout_ms == 0)
            succeeded = key != nullptr
                            ? version == 4 && ntp_authenticated_standard_exchange(socket_fd,
                                                                                  reinterpret_cast<const sockaddr *>(&destination), destination_length, &exchange, *key)
                            : ntp_standard_exchange(socket_fd, reinterpret_cast<const sockaddr *>(&destination),
                                                    destination_length, version, &exchange);
        else
        {
            if (failure != nullptr)
                *failure = 0;
            timeval timeout{0, static_cast<suseconds_t>(std::min<uint32_t>(25, timeout_ms) * 1000)};
            if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
                setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0 ||
                connect(socket_fd, reinterpret_cast<const sockaddr *>(&destination), destination_length) != 0)
            {
                if (failure != nullptr)
                    *failure = errno;
                close(socket_fd);
                return false;
            }
            std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
            size_t request_length = NtpPacketSize;
            request[0] = static_cast<uint8_t>((version << 3) | 3);
            exchange.t1 = ntp_timestamp_now();
            write_ntp_timestamp(request.data(), 40, exchange.t1);
            if (key != nullptr && (version != 4 ||
                                   !append_ntp_authentication(request.data(), request.size(), &request_length, *key)))
            {
                if (failure != nullptr)
                    *failure = EIO;
                close(socket_fd);
                return false;
            }
            const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(timeout_ms) * 1000;
            if (send(socket_fd, request.data(), request_length, 0) == static_cast<ssize_t>(request_length))
            {
                while (!s_stop_tests_requested.load() && !(worker_stop && worker_stop->load()) &&
                       esp_timer_get_time() < deadline)
                {
                    const int64_t remaining_us = deadline - esp_timer_get_time();
                    if (remaining_us <= 0)
                        break;
                    timeout.tv_usec = static_cast<suseconds_t>(std::min<int64_t>(25000, remaining_us));
                    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
                    {
                        if (failure != nullptr)
                            *failure = errno;
                        break;
                    }
                    uint8_t response[128]{};
                    const ssize_t received = recv(socket_fd, response, sizeof(response), 0);
                    if (received < 0)
                    {
                        const int error = errno;
                        if (error == EAGAIN || error == EWOULDBLOCK || error == ETIMEDOUT)
                            continue;
                        if (failure != nullptr)
                            *failure = error;
                        break;
                    }
                    if (!ntp_load::valid_response(response, static_cast<size_t>(received), version, exchange.t1, key != nullptr) ||
                        (key != nullptr && !verify_ntp_authentication(response, static_cast<size_t>(received), *key)))
                    {
                        if (explicit_auth_rejection != nullptr && key == nullptr)
                            *explicit_auth_rejection = ntp_load::classify_auth_rejection(
                                                           response, static_cast<size_t>(received), version, exchange.t1) ==
                                                       ntp_load::AuthRejectionResponse::Rejected;
                        if (failure != nullptr)
                            *failure = EPROTO;
                        break;
                    }
                    exchange.t3 = read_ntp_timestamp(response, 40);
                    succeeded = esp_timer_get_time() <= deadline;
                    break;
                }
            }
            else if (failure != nullptr)
                *failure = errno;
            if (!succeeded && failure != nullptr && *failure == 0)
                *failure = ETIMEDOUT;
        }
        close(socket_fd);
        if (!succeeded)
            return false;
        uint8_t response[48]{};
        write_ntp_timestamp(response, 40, exchange.t3);
        if (timestamp != nullptr)
            format_ntp_timestamp(response, timestamp, timestamp_size);
        return true;
    }

    bool ntp_interleaved_exchange(int family, char *timestamp, size_t timestamp_size)
    {
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return false;

        const sockaddr *address = reinterpret_cast<const sockaddr *>(&destination);
        NtpExchange previous{};
        if (!ntp_standard_exchange(socket_fd, address, destination_length, 4, &previous))
        {
            close(socket_fd);
            return false;
        }

        std::array<uint8_t, NtpPacketSize> request{};
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        write_ntp_timestamp(request.data(), 24, previous.t2);
        write_ntp_timestamp(request.data(), 32, previous.t4);
        write_ntp_timestamp(request.data(), 40, previous.t1);
        std::array<uint8_t, 128> response{};
        const bool sent = sendto(socket_fd, request.data(), request.size(), 0, address, destination_length) ==
                          static_cast<ssize_t>(request.size());
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        close(socket_fd);
        if (received != NtpPacketSize || ((response[0] >> 3) & 0x07) != 4 || (response[0] & 0x07) != 4 ||
            read_ntp_timestamp(response.data(), 24) != previous.t4)
            return false;
        const uint64_t reply_t3 = read_ntp_timestamp(response.data(), 40);
        const uint64_t difference = reply_t3 >= previous.t3 ? reply_t3 - previous.t3 : previous.t3 - reply_t3;
        if (difference > ((1ULL << 32) * 5ULL / 1000ULL))
            return false;
        format_ntp_timestamp(response.data(), timestamp, timestamp_size);
        return true;
    }

    void write_ntp_key_id(uint8_t *packet, uint16_t key_id)
    {
        packet[NtpPacketSize] = 0;
        packet[NtpPacketSize + 1] = 0;
        packet[NtpPacketSize + 2] = static_cast<uint8_t>(key_id >> 8);
        packet[NtpPacketSize + 3] = static_cast<uint8_t>(key_id);
    }

    bool append_ntp_authentication(uint8_t *packet, size_t capacity, size_t *packet_length, const symmetric_key_t &key)
    {
        if (!has_test_key(key) || *packet_length != NtpPacketSize || capacity < NtpPacketSize + NtpAuthTrailerSize)
            return false;
        write_ntp_key_id(packet, key.key_id);
        uint8_t digest[32]{};
        psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
        size_t digest_length = 0;
        const bool succeeded = psa_hash_setup(&operation, PSA_ALG_SHA_256) == PSA_SUCCESS &&
                               psa_hash_update(&operation, reinterpret_cast<const uint8_t *>(key.key_value),
                                               std::strlen(key.key_value)) == PSA_SUCCESS &&
                               psa_hash_update(&operation, packet, NtpPacketSize) == PSA_SUCCESS &&
                               psa_hash_finish(&operation, digest, sizeof(digest), &digest_length) == PSA_SUCCESS;
        psa_hash_abort(&operation);
        if (!succeeded || digest_length != 32)
            return false;
        std::memcpy(packet + NtpPacketSize + 4, digest, 20);
        *packet_length = NtpPacketSize + NtpAuthTrailerSize;
        return true;
    }

    bool verify_ntp_authentication(const uint8_t *packet, size_t packet_length, const symmetric_key_t &key)
    {
        if (packet_length != NtpPacketSize + NtpAuthTrailerSize ||
            read_u32_be(packet + NtpPacketSize) != key.key_id)
            return false;
        uint8_t expected_digest[32]{};
        psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
        size_t digest_length = 0;
        const bool succeeded = psa_hash_setup(&operation, PSA_ALG_SHA_256) == PSA_SUCCESS &&
                               psa_hash_update(&operation, reinterpret_cast<const uint8_t *>(key.key_value),
                                               std::strlen(key.key_value)) == PSA_SUCCESS &&
                               psa_hash_update(&operation, packet, NtpPacketSize) == PSA_SUCCESS &&
                               psa_hash_finish(&operation, expected_digest, sizeof(expected_digest), &digest_length) == PSA_SUCCESS;
        psa_hash_abort(&operation);
        return succeeded && digest_length == sizeof(expected_digest) &&
               std::memcmp(expected_digest, packet + NtpPacketSize + 4, 20) == 0;
    }

    bool ntp_authenticated_standard_exchange(int socket_fd, const sockaddr *destination, socklen_t destination_length,
                                             NtpExchange *exchange, const symmetric_key_t &key)
    {
        std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
        size_t request_length = NtpPacketSize;
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        exchange->t1 = ntp_timestamp_now();
        write_ntp_timestamp(request.data(), 40, exchange->t1);
        if (!append_ntp_authentication(request.data(), request.size(), &request_length, key))
            return false;
        const bool sent = sendto(socket_fd, request.data(), request_length, 0, destination, destination_length) ==
                          static_cast<ssize_t>(request_length);
        std::array<uint8_t, 128> response{};
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        exchange->t4 = ntp_timestamp_now();
        if (received < 0 ||
            !ntp_load::valid_response(response.data(), static_cast<size_t>(received), 4, exchange->t1, true) ||
            !verify_ntp_authentication(response.data(), static_cast<size_t>(received), key))
            return false;
        exchange->t2 = read_ntp_timestamp(response.data(), 32);
        exchange->t3 = read_ntp_timestamp(response.data(), 40);
        return exchange->t2 != 0 && exchange->t3 != 0;
    }

    bool ntp_authenticated_exchange(int family, bool interleaved, char *timestamp, size_t timestamp_size,
                                    const symmetric_key_t &key)
    {
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return false;
        const sockaddr *address = reinterpret_cast<const sockaddr *>(&destination);
        NtpExchange previous{};
        if (!ntp_authenticated_standard_exchange(socket_fd, address, destination_length, &previous, key))
        {
            close(socket_fd);
            return false;
        }
        if (!interleaved)
        {
            close(socket_fd);
            uint8_t response[NtpPacketSize]{};
            write_ntp_timestamp(response, 40, previous.t3);
            format_ntp_timestamp(response, timestamp, timestamp_size);
            return true;
        }

        std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
        size_t request_length = NtpPacketSize;
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        write_ntp_timestamp(request.data(), 24, previous.t2);
        write_ntp_timestamp(request.data(), 32, previous.t4);
        write_ntp_timestamp(request.data(), 40, previous.t1);
        const bool authenticated = append_ntp_authentication(request.data(), request.size(), &request_length, key);
        std::array<uint8_t, 128> response{};
        const bool sent = authenticated && sendto(socket_fd, request.data(), request_length, 0, address, destination_length) ==
                                               static_cast<ssize_t>(request_length);
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        close(socket_fd);
        if (received < 0 ||
            !ntp_load::valid_response(response.data(), static_cast<size_t>(received), 4, previous.t4, true) ||
            !verify_ntp_authentication(response.data(), static_cast<size_t>(received), key))
            return false;
        const uint64_t reply_t3 = read_ntp_timestamp(response.data(), 40);
        const uint64_t difference = reply_t3 >= previous.t3 ? reply_t3 - previous.t3 : previous.t3 - reply_t3;
        if (difference > ((1ULL << 32) * 5ULL / 1000ULL))
            return false;
        format_ntp_timestamp(response.data(), timestamp, timestamp_size);
        return true;
    }

    TestStatus ntp_authentication_rejection(int family, TestDefinition::Kind kind, const symmetric_key_t &key,
                                            char *timestamp, size_t timestamp_size)
    {
        if (key.key_id == 0 || (kind != TestDefinition::Kind::MissingAuth && !has_test_key(key)))
            return TestStatus::CouldNotRun;
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return TestStatus::CouldNotRun;
        std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
        size_t request_length = NtpPacketSize;
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        const uint64_t request_timestamp = ntp_timestamp_now();
        write_ntp_timestamp(request.data(), 40, request_timestamp);
        if (kind == TestDefinition::Kind::MissingAuth)
        {
            write_ntp_key_id(request.data(), key.key_id);
            request_length += sizeof(uint32_t);
        }
        else
        {
            if (!append_ntp_authentication(request.data(), request.size(), &request_length, key))
            {
                close(socket_fd);
                return TestStatus::CouldNotRun;
            }
            if (kind == TestDefinition::Kind::InvalidAuth)
                request[request_length - 1] ^= 0x01;
            else
                --request_length;
        }
        if (connect(socket_fd, reinterpret_cast<const sockaddr *>(&destination), destination_length) != 0 ||
            send(socket_fd, request.data(), request_length, 0) != static_cast<ssize_t>(request_length))
        {
            close(socket_fd);
            return TestStatus::CouldNotRun;
        }
        uint8_t response[128]{};
        const ssize_t received = recv(socket_fd, response, sizeof(response), 0);
        const int receive_error = errno;
        close(socket_fd);
        if (received < 0)
            return receive_error == EAGAIN || receive_error == EWOULDBLOCK || receive_error == ETIMEDOUT
                       ? TestStatus::Pass
                       : TestStatus::CouldNotRun;
        const auto classification = ntp_load::classify_auth_rejection(
            response, static_cast<size_t>(received), 4, request_timestamp);
        if (classification == ntp_load::AuthRejectionResponse::Rejected)
            return TestStatus::Pass;
        if (classification != ntp_load::AuthRejectionResponse::Accepted)
            return TestStatus::CouldNotRun;
        format_ntp_timestamp(response, timestamp, timestamp_size);
        return TestStatus::Fail;
    }

    TestStatus ntp_required_unsigned_rejection(int family, uint8_t version, const symmetric_key_t &key,
                                               char *output, size_t size)
    {
        const uint32_t generation = s_tester_link_generation.load();
        char control[128] = {};
        if (!has_test_key(key) || !ntp_authenticated_exchange(family, false, control, sizeof(control), key))
        {
            std::snprintf(output, size, "CouldNotRun: NTP required policy needs a healthy authenticated positive control before unsigned rejection.");
            return TestStatus::CouldNotRun;
        }
        int failure = 0;
        bool explicit_rejection = false;
        const bool accepted = ntp_exchange(family, version, nullptr, 0, 3000, &failure, nullptr, nullptr,
                                           &explicit_rejection);
        const bool restored = !s_stop_tests_requested.load() &&
                              ntp_authenticated_exchange(family, false, control, sizeof(control), key);
        const bool intact = restored && s_ethernet_connected.load() && s_tester_link_generation.load() == generation;
        const bool timed_out = failure == EAGAIN || failure == EWOULDBLOCK || failure == ETIMEDOUT;
        std::snprintf(output, size,
                      "Operator-confirmed required NTP policy: unsigned NTPv%u expected rejected; "
                      "matching accepted response=%u; explicit authentication rejection=%u; receive error=%d; "
                      "authenticated pre/post controls=1/%u; link intact=%u.",
                      version, accepted, explicit_rejection, failure, restored, intact);
        if (!intact || (!accepted && !timed_out && !explicit_rejection))
            return TestStatus::CouldNotRun;
        return accepted ? TestStatus::Fail : TestStatus::Pass;
    }

    TestStatus ntp_controlled_authentication_rejection(int family, TestDefinition::Kind kind,
                                                       const symmetric_key_t &key, bool required,
                                                       char *output, size_t size)
    {
        const symmetric_key_t *control_key = has_test_key(key) ? &key : nullptr;
        if (required && control_key == nullptr)
        {
            std::snprintf(output, size, "CouldNotRun: required NTP authentication needs a separate NTP key for positive controls.");
            return TestStatus::CouldNotRun;
        }
        const uint32_t generation = s_tester_link_generation.load();
        int before_error = 0;
        const bool before = ntp_exchange(family, 4, nullptr, 0, 3000, &before_error, nullptr, control_key);
        if (!before)
        {
            std::snprintf(output, size, "CouldNotRun: %s NTP before-control unavailable (error=%d).",
                          control_key ? "authenticated" : "unsigned", before_error);
            return TestStatus::CouldNotRun;
        }
        char probe_detail[128] = {};
        const TestStatus probe = ntp_authentication_rejection(family, kind, key, probe_detail, sizeof(probe_detail));
        int after_error = 0;
        const bool after = !s_stop_tests_requested.load() &&
                           ntp_exchange(family, 4, nullptr, 0, 3000, &after_error, nullptr, control_key);
        const bool intact = after && s_ethernet_connected.load() && s_tester_link_generation.load() == generation;
        std::snprintf(output, size, "%s NTP controls before/after=1/%u (after error=%d); link intact=%u; negative probe=%s; %s",
                      control_key ? "Authenticated" : "Unsigned", after, after_error, intact,
                      probe == TestStatus::Pass ? "rejected" : probe == TestStatus::Fail ? "accepted"
                                                                                         : "unavailable",
                      probe_detail[0] ? probe_detail : "No usable time response to the probe.");
        return intact ? probe : TestStatus::CouldNotRun;
    }

    struct PingContext
    {
        SemaphoreHandle_t completed;
        bool succeeded;
    };

    void ping_end_callback(esp_ping_handle_t handle, void *arguments)
    {
        auto *context = static_cast<PingContext *>(arguments);
        uint32_t replies = 0;
        esp_ping_get_profile(handle, ESP_PING_PROF_REPLY, &replies, sizeof(replies));
        context->succeeded = replies > 0;
        xSemaphoreGive(context->completed);
    }

    bool ping_target(int family)
    {
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!target_destination(family, &destination, &destination_length, false))
            return false;

        esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
        config.count = 3;
        config.interval_ms = 250;
        config.timeout_ms = 1500;
        config.interface = static_cast<uint32_t>(esp_netif_get_netif_impl_index(s_eth_netif));
        if (destination.ss_family == AF_INET)
        {
            const auto *address = reinterpret_cast<const sockaddr_in *>(&destination);
            inet_addr_to_ip4addr(ip_2_ip4(&config.target_addr), &address->sin_addr);
            IP_SET_TYPE_VAL(config.target_addr, IPADDR_TYPE_V4);
        }
        else if (destination.ss_family == AF_INET6)
        {
            const auto *address = reinterpret_cast<const sockaddr_in6 *>(&destination);
            inet6_addr_to_ip6addr(ip_2_ip6(&config.target_addr), &address->sin6_addr);
            IP_SET_TYPE_VAL(config.target_addr, IPADDR_TYPE_V6);
        }
        else
            return false;

        PingContext context{xSemaphoreCreateBinary(), false};
        if (context.completed == nullptr)
            return false;
        const esp_ping_callbacks_t callbacks = {
            .cb_args = &context,
            .on_ping_success = nullptr,
            .on_ping_timeout = nullptr,
            .on_ping_end = ping_end_callback,
        };
        esp_ping_handle_t handle = nullptr;
        if (esp_ping_new_session(&config, &callbacks, &handle) != ESP_OK || esp_ping_start(handle) != ESP_OK)
        {
            if (handle != nullptr)
                esp_ping_delete_session(handle);
            vSemaphoreDelete(context.completed);
            return false;
        }
        bool completed = false;
        for (int elapsed = 0; elapsed < 6000 && !s_stop_tests_requested.load(); elapsed += 100)
        {
            if (xSemaphoreTake(context.completed, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                completed = true;
                break;
            }
        }
        if (!completed)
            esp_ping_stop(handle);
        esp_ping_delete_session(handle);
        vSemaphoreDelete(context.completed);
        return completed && context.succeeded;
    }

    bool append_detail(char *output, size_t size, const char *format, ...)
    {
        const size_t used = std::strlen(output);
        if (used >= size)
            return false;
        va_list arguments;
        va_start(arguments, format);
        const int written = std::vsnprintf(output + used, size - used, format, arguments);
        va_end(arguments);
        if (written >= 0 && static_cast<size_t>(written) < size - used)
            return true;
        constexpr char marker[] = "\n[Evidence truncated; not a complete result]\n";
        if (size >= sizeof(marker))
            std::memcpy(output + size - sizeof(marker), marker, sizeof(marker));
        return false;
    }

    struct CombinedWorker;

    struct PtpRunContext
    {
        CombinedWorker *combined = nullptr;
        bool udp = false;
        char phases[4096] = {};
        bool missing_action = false;
        bool action_timed_out = false;
        bool skipped = false;
        bool truncated = false;
    };

    uint32_t operator_phase_timeout_ms(ptp_tester::Test test)
    {
        return test == ptp_tester::Test::GnssRecovery || test == ptp_tester::Test::PpsRecovery ||
                       test == ptp_tester::Test::RestartRecovery || ptp_tester::is_standby_recovery(test) ||
                       test == ptp_tester::Test::StartupListening || test == ptp_tester::Test::KeyRotation ||
                       test == ptp_tester::Test::TimingRestore || test == ptp_tester::Test::StartupRestore ||
                       test == ptp_tester::Test::KeyRotationReady ||
                       test == ptp_tester::Test::KeyRotationRestore
                   ? 600000U
                   : PtpOperatorPhaseTimeoutMs;
    }

    void ptp_progress(const char *text, void *)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
            std::snprintf(s_progress, sizeof(s_progress), "%s", text ? text : "");
        xSemaphoreGive(s_state_mutex);
    }

    void ptp_target_identity(const char *text, void *)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        std::snprintf(s_observed_ptp_identity, sizeof(s_observed_ptp_identity), "%s", text);
        std::snprintf(s_observed_ptp_mac, sizeof(s_observed_ptp_mac), "%s", s_profile.ptp_mac);
        s_observed_ptp_domain = s_profile.ptp_domain;
        xSemaphoreGive(s_state_mutex);
    }

    bool ptp_assist(ptp_tester::Test test, bool restoration, const std::atomic<bool> &stop, void *argument, ptp_tester::AssistPoll poll, void *poll_context)
    {
        auto *context = static_cast<PtpRunContext *>(argument);
        if (stop.load() || context->skipped)
            return false;
        const char *action = nullptr;
        if (test == ptp_tester::Test::StandbyRecovery)
            action = restoration
                         ? "Restore DUT GNSS reception/data while keeping PPS and both Ethernet links connected. Verify fresh GNSS/PPS qualification on the DUT before acknowledging. The tester is still refreshing the better foreign master: the DUT must remain in standby. If skipping/stopping, restore GNSS manually."
                         : "Remove DUT GNSS reception/data using the controlled fixture, keeping PPS and both Ethernet links connected. Verify that DUT GNSS qualification is lost before acknowledging; wire silence alone is not proof. The tester continues refreshing the better foreign master. If skipping/stopping, restore GNSS manually.";
        else if (test == ptp_tester::Test::StandbyPpsRecovery)
            action = restoration
                         ? "Restore DUT PPS while keeping GNSS and the tester link connected. Verify fresh GNSS/PPS qualification before acknowledging. The tester continues refreshing the better foreign master. Restore PPS manually if skipped or stopped."
                         : "Remove DUT PPS using the controlled fixture; keep GNSS and the tester link connected. Verify PPS qualification loss before acknowledging; silence alone is not proof. The tester continues refreshing the better foreign master. Restore PPS manually if skipped or stopped.";
        else if (test == ptp_tester::Test::StandbyLinkRecovery)
            action = restoration
                         ? "Reconnect the SERVER-side Ethernet link, not the tester link. Verify the DUT is running and freshly GNSS/PPS qualified before acknowledging. The tester keeps transmitting the better foreign master through recovery."
                         : "Disconnect only the SERVER-side Ethernet link on the isolated switched fixture; leave the tester link connected. Confirm the DUT link is down before acknowledging. The tester keeps transmitting the better foreign master.";
        else if (test == ptp_tester::Test::StandbyRestartRecovery)
            action = restoration
                         ? "Allow the DUT restart to complete; verify fresh GNSS/PPS qualification before acknowledging. Keep the tester link connected while it continues transmitting the better foreign master."
                         : "Start the authorized controlled DUT restart while keeping the tester link connected. Acknowledge when the restart has begun; the tester continues transmitting the better foreign master.";
        else if (test == ptp_tester::Test::StandbyExpiryUnqualified)
            action = restoration
                         ? "Restore DUT GNSS and verify fresh GNSS/PPS qualification. Confirm independently that the PTP worker/clock generation did not reset during the outage. The foreign master is no longer being refreshed. Acknowledge promptly; the tester checks service immediately."
                         : "Remove DUT GNSS reception/data, keeping PPS and both Ethernet links connected. Verify with DUT instrumentation that GNSS timing is unqualified and the PTP worker/clock generation has not reset. Keep GNSS unavailable until the restoration prompt. The tester continues refreshing the foreign master until acknowledgement.";
        else if (test == ptp_tester::Test::SelectionNtpHoldover)
            action = restoration
                         ? "Restore DUT GNSS while keeping PPS and both Ethernet links connected. Verify fresh GNSS/PPS qualification before acknowledging. The tester continues refreshing the better foreign master; restore manually if skipped or stopped."
                         : "Remove DUT GNSS reception/data using the controlled fixture while keeping PPS and both Ethernet links connected. Independently verify GNSS qualification loss AND that the DUT has entered its supported NTP holdover state. Keep GNSS unavailable until restoration. The tester continues refreshing the foreign master.";
        else if (test == ptp_tester::Test::KeyRotationReady)
            action = "Confirm this is an isolated authorized test fixture with the profile PTP key active and the public rotation fixture key additionally accepted. The key ID/value are specified in the tester settings header. Keep the tester link connected. Never deploy the public fixture key.";
        else if (test == ptp_tester::Test::KeyRotation)
            action = restoration
                         ? "On the isolated test fixture only, rebuild/restart the DUT with the public rotation fixture key active and the original PTP key revoked. Keep independent NTP settings unchanged. Acknowledge after the DUT is freshly GNSS/PPS qualified. Never deploy the public fixture key."
                         : "On the isolated test fixture only, rebuild/restart the DUT with both PTP keys still accepted and the public rotation fixture key active for outgoing messages. Keep independent NTP settings unchanged. Acknowledge after fresh GNSS/PPS qualification. Never deploy the public fixture key.";
        else if (test == ptp_tester::Test::KeyRotationRestore)
            action = "Restore the originally authorized DUT firmware and PTP key configuration, removing the public rotation fixture key. Keep the tester link connected and acknowledge after the DUT is freshly GNSS/PPS qualified.";
        else if (test == ptp_tester::Test::StartupListening)
            action = restoration
                         ? "Start/reset the DUT PTP worker while its timing remains qualified. Verify the worker-start/reset and timing qualification using authorized instrumentation or device logs, then acknowledge immediately. The tester observes continuously; delayed acknowledgement may make the result inconclusive."
                         : "Stop the DUT PTP worker using an authorized instrumented fixture while keeping the tester link and DUT timing qualified. Verify the worker is stopped before acknowledging. Do not start it again until the next prompt.";
        else if (test == ptp_tester::Test::StartupRestore)
            action = "Restart or restore the DUT PTP worker, keep the tester link connected, and verify fresh GNSS/PPS qualification before acknowledging. This recovery step does not claim a startup-listening timing result.";
        else if (test == ptp_tester::Test::TimingRestore)
            action = "Restore DUT GNSS/PPS and verify fresh timing qualification while keeping the tester link connected. Acknowledge only after the DUT is ready for independent PTP recovery controls.";
        else if (restoration)
            action = "Restore the DUT GNSS/PPS, server Ethernet link and firmware as applicable. Verify fresh GNSS/PPS clock qualification on the DUT before acknowledging. Do not reset or disconnect the tester.";
        else
        {
            switch (test)
            {
            case ptp_tester::Test::GnssRecovery:
                action = "Inject GNSS qualification loss on the DUT using the controlled fixture; keep the tester link and PPS as required by the case. Acknowledge only after the fault is active.";
                break;
            case ptp_tester::Test::PpsRecovery:
                action = "Remove PPS from the DUT using the controlled fixture, keeping the tester link connected. Acknowledge only after PPS loss is active.";
                break;
            case ptp_tester::Test::LinkRecovery:
                action = "Disconnect the SERVER-side Ethernet link on an isolated switched test network, NOT the tester link. Acknowledge only after the DUT link is down.";
                break;
            case ptp_tester::Test::RestartRecovery:
                action = context->udp
                             ? "Power down or hold the DUT stopped for a controlled restart; keep the tester link connected. Acknowledge only after the DUT is stopped, and leave it stopped until the restoration prompt."
                             : "Start the controlled DUT restart or OTA phase; keep the tester link connected. Acknowledge only after the restart/OTA fault phase has begun.";
                break;
            default:
                context->missing_action = true;
                append_detail(context->phases, sizeof(context->phases), "Unsupported operator phase requested; fixture/action unavailable.\n");
                return false;
            }
        }
        context->truncated |= !append_detail(context->phases, sizeof(context->phases),
                                             "%s phase requested: %s\n", restoration ? "Restoration" : "Injection", action);
        const uint32_t timeout_ms = operator_phase_timeout_ms(test);
        const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(timeout_ms) * 1000;
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        ++s_phase_token;
        if (s_phase_token == 0)
            ++s_phase_token;
        const uint32_t token = s_phase_token;
        const uint32_t link_generation = s_tester_link_generation.load();
        s_phase_deadline_us = deadline;
        std::snprintf(s_phase_prompt, sizeof(s_phase_prompt), "%s (acknowledgement limit: %g minutes)",
                      action, static_cast<double>(timeout_ms) / 60000.0);
        s_phase_acknowledged = false;
        s_phase_skipped = false;
        s_phase_waiting = s_tests_running.load() && !stop.load() && timeout_ms > 0;
        if (s_phase_waiting && s_active_test < TestCount)
            s_results[s_active_test].status = TestStatus::AwaitingOperatorAction;
        xSemaphoreGive(s_state_mutex);
        bool acknowledged = false;
        bool poll_failed = false;
        while (!stop.load() && esp_timer_get_time() < deadline && s_ethernet_connected.load() &&
               s_tester_link_generation.load() == link_generation)
        {
            if (poll != nullptr && !poll(poll_context))
            {
                poll_failed = true;
                break;
            }
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            acknowledged = s_tests_running.load() && s_phase_token == token && s_phase_acknowledged;
            context->skipped = s_tests_running.load() && s_phase_token == token && s_phase_skipped;
            xSemaphoreGive(s_state_mutex);
            if (acknowledged || context->skipped)
                break;
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(50)));
        }
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        context->skipped = s_tests_running.load() && s_phase_token == token && s_phase_skipped;
        const bool tester_link_changed = !s_ethernet_connected.load() || s_tester_link_generation.load() != link_generation;
        const bool timed_out = !s_phase_acknowledged && !context->skipped && !stop.load() &&
                               !tester_link_changed && esp_timer_get_time() >= deadline;
        context->action_timed_out |= timed_out;
        s_phase_waiting = false;
        s_phase_prompt[0] = '\0';
        if (s_active_test < TestCount)
            s_results[s_active_test].status = context->skipped ? TestStatus::Skipped
                                              : timed_out      ? TestStatus::TimedOutAwaitingOperatorAction
                                                               : TestStatus::Running;
        xSemaphoreGive(s_state_mutex);
        if (context->skipped)
            return false;
        acknowledged = acknowledged && !poll_failed && !stop.load() && esp_timer_get_time() < deadline && !tester_link_changed;
        context->missing_action |= !acknowledged;
        if (poll_failed)
            context->truncated |= !append_detail(context->phases, sizeof(context->phases),
                                                 "PTP observation/refresh failed during acknowledgement; phase rejected. Restore the DUT manually if changed.\n");
        if (tester_link_changed)
            context->truncated |= !append_detail(context->phases, sizeof(context->phases),
                                                 "Tester Ethernet link changed during acknowledgement; phase rejected, not a DUT fault.\n");
        context->truncated |= !append_detail(context->phases, sizeof(context->phases),
                                             "%s phase: %s; token=%lu; monotonic_ms=%lld.\n",
                                             restoration ? "Restoration" : "Injection",
                                             acknowledged ? "operator acknowledged" : "missing action / timed out / cancelled",
                                             static_cast<unsigned long>(token), static_cast<long long>(esp_timer_get_time() / 1000));
        return acknowledged;
    }

    struct CombinedWorker
    {
        SemaphoreHandle_t completed = nullptr;
        const symmetric_key_t *key = nullptr;
        std::atomic<bool> stop{false};
        std::atomic<bool> begin{false};
        std::atomic<bool> active{false};
        std::atomic<bool> armed{false};
        std::atomic<bool> traffic_established{false};
        std::atomic<bool> collection_finished{false};
        int64_t start_us = 0;
        int64_t end_us = 0;
        int64_t observation_start_us = 0;
        int64_t observation_end_us = 0;
        int64_t deadline_us = 0;
        uint32_t attempts[2] = {};
        uint32_t successes[2] = {};
        uint32_t timeouts[2] = {};
        uint32_t invalid[2] = {};
        uint32_t socket_errors[2] = {};
        uint32_t buffer_errors = 0;
        uint64_t pacing_misses = 0;
        bool full_window = false;
        uint8_t family_mask = 3;
    };

    struct NtpPolicyWorker
    {
        CombinedWorker *load = nullptr;
        const symmetric_key_t *key = nullptr;
        SemaphoreHandle_t completed = nullptr;
        ntp_load::PolicyProbeEvidence evidence{};
        uint32_t link_generation = 0;
        bool required = false;
    };

    void ptp_observation(bool starting, void *argument)
    {
        auto *context = static_cast<PtpRunContext *>(argument);
        CombinedWorker *worker = context ? context->combined : nullptr;
        if (!worker)
            return;
        if (!starting)
        {
            worker->observation_end_us = esp_timer_get_time();
            worker->collection_finished.store(true, std::memory_order_release);
            return;
        }
        if (worker->begin.exchange(true))
        {
            worker->stop.store(true);
            return;
        }
        const int64_t activation_deadline = esp_timer_get_time() + 1000000;
        while (!worker->active.load() && !s_stop_tests_requested.load() && esp_timer_get_time() < activation_deadline)
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(1)));
        if (!worker->active.load() || s_stop_tests_requested.load())
        {
            worker->stop.store(true);
            return;
        }
        worker->observation_start_us = esp_timer_get_time();
        worker->deadline_us = worker->observation_start_us +
                              (static_cast<int64_t>(PtpObservationMs) + PtpResponseWindowMs + PtpCombinedSchedulingAllowanceMs) * 1000;
        worker->armed.store(true);
    }

    void combined_ntp_worker(void *argument)
    {
        auto *worker = static_cast<CombinedWorker *>(argument);
        while (!worker->begin.load() && !worker->stop.load() && !s_stop_tests_requested.load())
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(1)));
        worker->start_us = esp_timer_get_time();
        worker->active.store(true);
        while (!worker->armed.load() && !worker->stop.load() && !s_stop_tests_requested.load())
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(1)));
        const int64_t interval_us = 1000000LL / PtpCombinedRequestsPerSecond;
        int64_t next_us = worker->armed.load() ? worker->observation_start_us : 0;
        const int64_t load_end_us = next_us + (static_cast<int64_t>(PtpObservationMs) + PtpResponseWindowMs) * 1000;
        size_t family_index = 0;
        while (worker->armed.load() && !worker->stop.load() && !s_stop_tests_requested.load() &&
               !worker->collection_finished.load(std::memory_order_acquire))
        {
            const int64_t now = esp_timer_get_time();
            if (now >= worker->deadline_us)
                break;
            if (now < next_us || now >= load_end_us)
            {
                vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(1)));
                continue;
            }
            if (now - next_us >= interval_us)
            {
                const int64_t missed = (now - next_us) / interval_us;
                worker->pacing_misses += static_cast<uint64_t>(missed);
                next_us += missed * interval_us;
            }
            const uint32_t remaining_ms = static_cast<uint32_t>((worker->deadline_us - now + 999) / 1000);
            const size_t slot = worker->family_mask == 1 ? 0 : worker->family_mask == 2 ? 1
                                                                                        : family_index++ % 2;
            ++worker->attempts[slot];
            int failure = EIO;
            if (ntp_exchange(slot == 0 ? AF_INET : AF_INET6, 4, nullptr, 0,
                             std::min<uint32_t>(PtpResponseWindowMs, remaining_ms), &failure, &worker->stop, worker->key))
            {
                ++worker->successes[slot];
                if (((worker->family_mask & 1) == 0 || worker->successes[0]) &&
                    ((worker->family_mask & 2) == 0 || worker->successes[1]))
                    worker->traffic_established.store(true, std::memory_order_release);
            }
            else if (!s_stop_tests_requested.load() && !worker->stop.load())
            {
                if (failure == ETIMEDOUT)
                    ++worker->timeouts[slot];
                else if (failure == EPROTO)
                    ++worker->invalid[slot];
                else
                {
                    ++worker->socket_errors[slot];
                    if (failure == ENOBUFS || failure == ENOMEM)
                        ++worker->buffer_errors;
                }
            }
            next_us += interval_us;
            const bool collection_finished = worker->collection_finished.load(std::memory_order_acquire);
            const int64_t finished_us = collection_finished ? worker->observation_end_us : esp_timer_get_time();
            const uint64_t missed = ntp_load::missed_slots(next_us, finished_us, load_end_us, interval_us);
            worker->pacing_misses += missed;
            next_us += static_cast<int64_t>(missed) * interval_us;
        }
        worker->end_us = esp_timer_get_time();
        worker->full_window = worker->armed.load() && !worker->stop.load() && !s_stop_tests_requested.load() &&
                              worker->collection_finished.load(std::memory_order_acquire) &&
                              worker->observation_end_us <= worker->deadline_us &&
                              worker->observation_end_us - worker->observation_start_us >=
                                  (static_cast<int64_t>(PtpObservationMs) + PtpResponseWindowMs) * 1000;
        if (worker->full_window)
            worker->pacing_misses += ntp_load::missed_slots(next_us, worker->observation_end_us, load_end_us, interval_us);
        xSemaphoreGive(worker->completed);
        vTaskDelete(nullptr);
    }

    ntp_load::ProbeResult ntp_policy_result(TestStatus status)
    {
        return status == TestStatus::Pass   ? ntp_load::ProbeResult::Pass
               : status == TestStatus::Fail ? ntp_load::ProbeResult::Fail
                                            : ntp_load::ProbeResult::Unavailable;
    }

    void combined_ntp_policy_worker(void *argument)
    {
        auto *worker = static_cast<NtpPolicyWorker *>(argument);
        while (!worker->load->armed.load() && !worker->load->stop.load() && !s_stop_tests_requested.load())
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(1)));
        while (worker->load->armed.load() && !worker->load->traffic_established.load(std::memory_order_acquire) &&
               !worker->load->collection_finished.load(std::memory_order_acquire) &&
               !worker->load->stop.load() && !s_stop_tests_requested.load())
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(1)));
        if (worker->load->traffic_established.load(std::memory_order_acquire) &&
            !worker->load->collection_finished.load(std::memory_order_acquire) &&
            !worker->load->stop.load() && !s_stop_tests_requested.load())
        {
            worker->link_generation = s_tester_link_generation.load();
            worker->evidence.start_us = esp_timer_get_time();
            for (size_t slot = 0; slot < 2; ++slot)
            {
                if (worker->load->collection_finished.load(std::memory_order_acquire) ||
                    worker->load->stop.load() || s_stop_tests_requested.load())
                    break;
                const int family = slot == 0 ? AF_INET : AF_INET6;
                worker->evidence.before[slot] = ntp_exchange(family, 4, nullptr, 0, PtpResponseWindowMs,
                                                             nullptr, &worker->load->stop, worker->key);
                if (!worker->evidence.before[slot])
                    continue;
                char detail[256] = {};
                worker->evidence.invalid_mac[slot] = ntp_policy_result(ntp_authentication_rejection(
                    family, TestDefinition::Kind::InvalidAuth, *worker->key, detail, sizeof(detail)));
                if (worker->required && !worker->load->collection_finished.load(std::memory_order_acquire) &&
                    !worker->load->stop.load() && !s_stop_tests_requested.load())
                    worker->evidence.unsigned_required[slot] = ntp_policy_result(
                        ntp_required_unsigned_rejection(family, 4, *worker->key, detail, sizeof(detail)));
                if (!worker->load->collection_finished.load(std::memory_order_acquire) &&
                    !worker->load->stop.load() && !s_stop_tests_requested.load())
                    worker->evidence.after[slot] = ntp_exchange(family, 4, nullptr, 0, PtpResponseWindowMs,
                                                                nullptr, &worker->load->stop, worker->key);
            }
            worker->evidence.end_us = esp_timer_get_time();
        }
        xSemaphoreGive(worker->completed);
        vTaskDelete(nullptr);
    }

    ptp_tester::Result run_combined_load(ptp_tester::Test loaded_test, const ptp_tester::Config &config,
                                         const symmetric_key_t &ntp_key, char *output, size_t size)
    {
        const symmetric_key_t *load_key = config.policy.ntp_authentication_required ? &ntp_key : nullptr;
        const bool policy_requested = loaded_test == ptp_tester::Test::SelectionNtpPolicyLoad;
        if (load_key != nullptr && !has_test_key(*load_key))
        {
            append_detail(output, size, "Combined load requires the target profile's separate NTP key for required authentication.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        if (policy_requested && !has_test_key(ntp_key))
        {
            append_detail(output, size, "NTP key-policy probes require the target profile's separate NTP key, even when NTP authentication is mixed.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        ResponseBuffer evidence = allocate_response_buffer(PtpOutputSize);
        if (!evidence)
        {
            append_detail(output, size, "Combined load: insufficient memory for baseline evidence.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        ptp_progress("Combined load: collecting unloaded Sustained baseline", nullptr);
        evidence.get()[0] = '\0';
        const auto baseline = ptp_tester::run(ptp_tester::Test::Sustained, config, s_stop_tests_requested,
                                              evidence.get(), PtpOutputSize);
        bool complete = append_detail(output, size, "Unloaded Sustained baseline (%s):\n%s\n",
                                      baseline == ptp_tester::Result::Pass ? "Pass" : baseline == ptp_tester::Result::Fail ? "Fail"
                                                                                                                           : "Could not run",
                                      evidence.get());
        if (baseline != ptp_tester::Result::Pass || s_stop_tests_requested.load() || !complete)
            return !complete || s_stop_tests_requested.load() ? ptp_tester::Result::CouldNotRun : baseline;
        bool before[2] = {};
        ptp_progress("Combined load: IPv4 / IPv6 NTP positive controls before load", nullptr);
        for (size_t slot = 0; slot < 2 && !s_stop_tests_requested.load(); ++slot)
            before[slot] = ntp_exchange(slot == 0 ? AF_INET : AF_INET6, 4, nullptr, 0, PtpResponseWindowMs,
                                        nullptr, nullptr, load_key);
        append_detail(output, size, "NTP controls before load (%s): IPv4=%s; IPv6=%s.\n",
                      load_key ? "authenticated and verified" : "unsigned",
                      before[0] ? "Pass" : "unavailable/failed", before[1] ? "Pass" : "unavailable/failed");
        if (!before[0] || !before[1] || s_stop_tests_requested.load())
            return ptp_tester::Result::CouldNotRun;
        CombinedWorker worker;
        worker.key = load_key;
        worker.completed = xSemaphoreCreateBinary();
        if (!worker.completed)
        {
            append_detail(output, size, "Combined NTP worker completion semaphore unavailable.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        NtpPolicyWorker policy_worker;
        if (policy_requested)
        {
            policy_worker.load = &worker;
            policy_worker.key = &ntp_key;
            policy_worker.required = config.policy.ntp_authentication_required;
            policy_worker.completed = xSemaphoreCreateBinary();
            if (!policy_worker.completed)
            {
                vSemaphoreDelete(worker.completed);
                append_detail(output, size, "NTP key-policy probe completion semaphore unavailable.\n");
                return ptp_tester::Result::CouldNotRun;
            }
        }
        auto *context = static_cast<PtpRunContext *>(config.context);
        if (!context || !config.observation)
        {
            if (policy_worker.completed)
                vSemaphoreDelete(policy_worker.completed);
            vSemaphoreDelete(worker.completed);
            append_detail(output, size, "Combined load observation callback/context unavailable.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        context->combined = &worker;
        if (xTaskCreate(combined_ntp_worker, "bounded_ntp", 6144, &worker, 4, nullptr) != pdPASS)
        {
            if (policy_worker.completed)
                vSemaphoreDelete(policy_worker.completed);
            vSemaphoreDelete(worker.completed);
            context->combined = nullptr;
            append_detail(output, size, "Combined NTP worker could not be started.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        if (policy_requested &&
            xTaskCreate(combined_ntp_policy_worker, "ntp_key_probe", 6144, &policy_worker, 4, nullptr) != pdPASS)
        {
            worker.stop.store(true);
            while (xSemaphoreTake(worker.completed, pdMS_TO_TICKS(50)) != pdTRUE)
            {
            }
            vSemaphoreDelete(policy_worker.completed);
            vSemaphoreDelete(worker.completed);
            context->combined = nullptr;
            append_detail(output, size, "NTP key-policy probe worker could not be started.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        ptp_progress("Combined load: bounded alternating IPv4 / IPv6 NTP plus L2 observation", nullptr);
        evidence.get()[0] = '\0';
        const auto loaded = ptp_tester::run(loaded_test, config, s_stop_tests_requested,
                                            evidence.get(), PtpOutputSize);
        if (!worker.armed.load() || !worker.observation_end_us || s_stop_tests_requested.load())
            worker.stop.store(true);
        while (xSemaphoreTake(worker.completed, pdMS_TO_TICKS(50)) != pdTRUE)
        {
            if (s_stop_tests_requested.load())
                worker.stop.store(true);
        }
        if (policy_requested)
            while (xSemaphoreTake(policy_worker.completed, pdMS_TO_TICKS(50)) != pdTRUE)
            {
                if (s_stop_tests_requested.load())
                    worker.stop.store(true);
            }
        vSemaphoreDelete(worker.completed);
        if (policy_worker.completed)
            vSemaphoreDelete(policy_worker.completed);
        context->combined = nullptr;
        const int64_t observation_us = std::max<int64_t>(0, worker.observation_end_us - worker.observation_start_us);
        const int64_t overlap_us = worker.observation_start_us && worker.observation_end_us
                                       ? std::max<int64_t>(0, std::min(worker.end_us, worker.observation_end_us) -
                                                                  std::max(worker.start_us, worker.observation_start_us))
                                       : 0;
        const bool covered = worker.armed.load() && worker.observation_end_us > worker.observation_start_us &&
                             overlap_us == observation_us && observation_us >= static_cast<int64_t>(PtpObservationMs) * 1000;
        complete &= append_detail(output, size, "Loaded PTP (%s):\n%s\n",
                                  loaded == ptp_tester::Result::Pass ? "Pass" : loaded == ptp_tester::Result::Fail ? "Fail"
                                                                                                                   : "Could not run",
                                  evidence.get());
        complete &= append_detail(output, size,
                                  "NTP load: %s; configured observation+drain duration=%llu ms (half-open request schedule; endpoint scheduling grace adds no requests); scheduled load rate=%lu requests/s; load worker max outstanding=1; full load window=%s; entire PTP collection covered=%s.\n"
                                  "Monotonic windows (us): NTP=[%lld,%lld]; PTP collection=[%lld,%lld]; actual overlap=%lld us; uncovered collection=%lld us.\n",
                                  load_key ? "authenticated requests and verified responses" : "unsigned requests",
                                  static_cast<unsigned long long>(PtpObservationMs) + PtpResponseWindowMs,
                                  static_cast<unsigned long>(PtpCombinedRequestsPerSecond), worker.full_window ? "yes" : "no", covered ? "yes" : "no",
                                  static_cast<long long>(worker.start_us), static_cast<long long>(worker.end_us),
                                  static_cast<long long>(worker.observation_start_us), static_cast<long long>(worker.observation_end_us),
                                  static_cast<long long>(overlap_us), static_cast<long long>(observation_us - overlap_us));
        for (size_t slot = 0; slot < 2; ++slot)
            complete &= append_detail(output, size,
                                      "IPv%u load: attempts=%lu; successes=%lu; timeouts=%lu; invalid replies=%lu; local/socket errors=%lu.\n",
                                      slot == 0 ? 4U : 6U, static_cast<unsigned long>(worker.attempts[slot]),
                                      static_cast<unsigned long>(worker.successes[slot]), static_cast<unsigned long>(worker.timeouts[slot]),
                                      static_cast<unsigned long>(worker.invalid[slot]), static_cast<unsigned long>(worker.socket_errors[slot]));
        complete &= append_detail(output, size,
                                  "Client pacing misses=%llu; ENOBUFS/ENOMEM=%lu; UDP RX queue drop counters unavailable via this socket API (not assumed zero). PTP RX queue evidence is reported by the engine. Client overload is not server loss.\n",
                                  static_cast<unsigned long long>(worker.pacing_misses), static_cast<unsigned long>(worker.buffer_errors));
        ntp_load::ProbeResult policy_result = ntp_load::ProbeResult::Unavailable;
        if (policy_requested)
        {
            policy_result = ntp_load::classify_policy_probes(policy_worker.evidence, policy_worker.required,
                                                             worker.observation_start_us, worker.observation_end_us,
                                                             s_ethernet_connected.load() && s_tester_link_generation.load() == policy_worker.link_generation);
            const auto result_text = [](ntp_load::ProbeResult value)
            {
                return value == ntp_load::ProbeResult::Pass   ? "pass"
                       : value == ntp_load::ProbeResult::Fail ? "FAIL"
                                                              : "unavailable";
            };
            complete &= append_detail(output, size,
                                      "Concurrent NTP key-policy probes: interval=[%lld,%lld] us; required policy=%u; "
                                      "overall=%s. Additional probes/controls are concurrent with, and not counted in, the scheduled load rate; "
                                      "their complete interval must be inside the PTP standby/load window.\n",
                                      static_cast<long long>(policy_worker.evidence.start_us), static_cast<long long>(policy_worker.evidence.end_us),
                                      policy_worker.required, result_text(policy_result));
            for (size_t slot = 0; slot < 2; ++slot)
                complete &= append_detail(output, size,
                                          "IPv%u signed controls before/after=%u/%u; invalid-MAC rejection=%s; unsigned required-policy rejection=%s.\n",
                                          slot == 0 ? 4U : 6U, policy_worker.evidence.before[slot], policy_worker.evidence.after[slot],
                                          result_text(policy_worker.evidence.invalid_mac[slot]),
                                          policy_worker.required ? result_text(policy_worker.evidence.unsigned_required[slot]) : "not applicable (mixed policy)");
        }
        bool after[2] = {};
        int after_failure[2] = {};
        if (!s_stop_tests_requested.load())
        {
            ptp_progress("Combined load: IPv4 / IPv6 NTP positive controls after load", nullptr);
            for (size_t slot = 0; slot < 2 && !s_stop_tests_requested.load(); ++slot)
                after[slot] = ntp_exchange(slot == 0 ? AF_INET : AF_INET6, 4, nullptr, 0, PtpResponseWindowMs,
                                           &after_failure[slot], nullptr, load_key);
        }
        complete &= append_detail(output, size, "NTP controls after load: IPv4=%s (error=%d); IPv6=%s (error=%d).\n",
                                  after[0] ? "Pass" : "failed/not completed", after_failure[0],
                                  after[1] ? "Pass" : "failed/not completed", after_failure[1]);
        bool control_socket_error = false;
        for (size_t slot = 0; slot < 2; ++slot)
            control_socket_error |= !after[slot] && after_failure[slot] != ETIMEDOUT && after_failure[slot] != EPROTO;
        if (!complete || s_stop_tests_requested.load() || control_socket_error || loaded == ptp_tester::Result::CouldNotRun ||
            (policy_requested && policy_result == ntp_load::ProbeResult::Unavailable) ||
            !worker.full_window || !covered || worker.pacing_misses || worker.buffer_errors ||
            worker.socket_errors[0] || worker.socket_errors[1] || !worker.attempts[0] || !worker.attempts[1])
        {
            append_detail(output, size, "Incomplete observation / client pacing, socket or overload evidence: Could not run; no server attribution.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        if (loaded == ptp_tester::Result::Fail)
            return loaded;
        if (policy_requested && policy_result == ntp_load::ProbeResult::Fail)
            return ptp_tester::Result::Fail;
        if (!after[0] || !after[1] || worker.timeouts[0] || worker.timeouts[1] || worker.invalid[0] || worker.invalid[1])
            return ptp_tester::Result::Fail;
        return ptp_tester::Result::Pass;
    }

    ptp_tester::Result run_udp_combined_load(const ptp_tester::UdpConfig &config, const TargetHistoryEntry &profile,
                                             char *output, size_t size)
    {
        const uint8_t mask = static_cast<uint8_t>((config.ipv4 ? 1 : 0) | (config.ipv6 ? 2 : 0));
        if (!PtpCombinedLoadEnabled || !profile.stress_tests ||
            ((mask & 1) && !test_available(Tests[2], profile)) ||
            ((mask & 2) && !test_available(Tests[12], profile)))
        {
            append_detail(output, size, "Combined PTP L3 UDP load requires the load opt-in, target stress toggle, and NTP address/port for each selected family.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        if (ptp_tester::policy_error(PtpDutPolicy) == nullptr && PtpDutPolicy.ntp_authentication_required &&
            !has_test_key(profile.key))
        {
            append_detail(output, size, "The known NTP policy requires the separate target NTP symmetric key.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        const symmetric_key_t *load_key = has_test_key(profile.key) ? &profile.key : nullptr;
        ResponseBuffer evidence = allocate_response_buffer(PtpOutputSize);
        if (!evidence)
        {
            append_detail(output, size, "Insufficient memory for combined PTP L3 UDP evidence.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        ptp_progress("PTP L3 UDP combined load: collecting unloaded sustained baseline", nullptr);
        const auto baseline = ptp_tester::run_udp(ptp_tester::UdpTest::Sustained, config, s_stop_tests_requested,
                                                  evidence.get(), PtpOutputSize);
        bool complete = append_detail(output, size, "Unloaded PTP L3 UDP baseline (%s):\n%s\n",
                                      baseline == ptp_tester::Result::Pass   ? "Pass"
                                      : baseline == ptp_tester::Result::Fail ? "Fail"
                                                                             : "Could not run",
                                      evidence.get());
        if (baseline != ptp_tester::Result::Pass || s_stop_tests_requested.load() || !complete)
            return !complete || s_stop_tests_requested.load() ? ptp_tester::Result::CouldNotRun : baseline;

        ptp_progress("PTP L3 UDP combined load: NTP controls before load", nullptr);
        for (size_t slot = 0; slot < 2; ++slot)
        {
            if ((mask & (1U << slot)) == 0)
                continue;
            int failure = 0;
            const bool before = ntp_exchange(slot == 0 ? AF_INET : AF_INET6, 4, nullptr, 0,
                                             PtpResponseWindowMs, &failure, nullptr, load_key);
            complete &= append_detail(output, size, "IPv%u NTP control before load=%s; error=%d.\n",
                                      slot == 0 ? 4U : 6U, before ? "Pass" : "unavailable", failure);
            if (!before || !complete || s_stop_tests_requested.load())
            {
                append_detail(output, size, "NTP baseline unavailable; check the target address, port and separate NTP key.\n");
                return ptp_tester::Result::CouldNotRun;
            }
        }
        CombinedWorker worker;
        worker.key = load_key;
        worker.family_mask = mask;
        worker.completed = xSemaphoreCreateBinary();
        if (!worker.completed)
        {
            append_detail(output, size, "Combined NTP worker completion semaphore unavailable.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        auto *context = static_cast<PtpRunContext *>(config.context);
        if (!context || !config.observation)
        {
            vSemaphoreDelete(worker.completed);
            append_detail(output, size, "Combined PTP L3 UDP observation context unavailable.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        context->combined = &worker;
        if (xTaskCreate(combined_ntp_worker, "bounded_ntp_udp", 6144, &worker, 4, nullptr) != pdPASS)
        {
            context->combined = nullptr;
            vSemaphoreDelete(worker.completed);
            append_detail(output, size, "Combined NTP worker could not be started.\n");
            return ptp_tester::Result::CouldNotRun;
        }
        ptp_progress("PTP L3 UDP combined load: concurrent multicast observation and NTP traffic", nullptr);
        evidence.get()[0] = '\0';
        const auto loaded = ptp_tester::run_udp(ptp_tester::UdpTest::CombinedLoad, config, s_stop_tests_requested,
                                                evidence.get(), PtpOutputSize);
        if (!worker.armed.load() || !worker.observation_end_us || s_stop_tests_requested.load())
            worker.stop.store(true);
        while (xSemaphoreTake(worker.completed, pdMS_TO_TICKS(50)) != pdTRUE)
            if (s_stop_tests_requested.load())
                worker.stop.store(true);
        vSemaphoreDelete(worker.completed);
        context->combined = nullptr;
        const int64_t observation_us = std::max<int64_t>(0, worker.observation_end_us - worker.observation_start_us);
        const int64_t overlap_us = worker.observation_start_us && worker.observation_end_us
                                       ? std::max<int64_t>(0, std::min(worker.end_us, worker.observation_end_us) -
                                                                  std::max(worker.start_us, worker.observation_start_us))
                                       : 0;
        const bool covered = worker.armed.load() && worker.observation_end_us > worker.observation_start_us &&
                             overlap_us == observation_us && observation_us >= static_cast<int64_t>(PtpObservationMs) * 1000;
        complete &= append_detail(output, size,
                                  "Loaded PTP L3 UDP (%s):\n%s\nNTP load: %s; %u requests/s total; "
                                  "full window=%s; complete PTP collection covered=%s; client pacing misses=%llu; buffer errors=%lu.\n",
                                  loaded == ptp_tester::Result::Pass ? "Pass" : loaded == ptp_tester::Result::Fail ? "Fail"
                                                                                                                   : "Could not run",
                                  evidence.get(), load_key ? "authenticated requests and verified responses" : "unsigned requests",
                                  static_cast<unsigned>(PtpCombinedRequestsPerSecond), worker.full_window ? "yes" : "no",
                                  covered ? "yes" : "no", static_cast<unsigned long long>(worker.pacing_misses),
                                  static_cast<unsigned long>(worker.buffer_errors));
        bool after_failed = false;
        bool local_failure = false;
        for (size_t slot = 0; slot < 2; ++slot)
        {
            if ((mask & (1U << slot)) == 0)
                continue;
            int failure = 0;
            const bool after = !s_stop_tests_requested.load() &&
                               ntp_exchange(slot == 0 ? AF_INET : AF_INET6, 4, nullptr, 0,
                                            PtpResponseWindowMs, &failure, nullptr, load_key);
            after_failed = after_failed || !after;
            local_failure = local_failure || (!after && failure != ETIMEDOUT && failure != EPROTO);
            complete &= append_detail(output, size,
                                      "IPv%u NTP: attempts=%lu; successes=%lu; timeouts=%lu; invalid=%lu; "
                                      "local/socket errors=%lu; after control=%s (error=%d).\n",
                                      slot == 0 ? 4U : 6U, static_cast<unsigned long>(worker.attempts[slot]),
                                      static_cast<unsigned long>(worker.successes[slot]), static_cast<unsigned long>(worker.timeouts[slot]),
                                      static_cast<unsigned long>(worker.invalid[slot]), static_cast<unsigned long>(worker.socket_errors[slot]),
                                      after ? "Pass" : "failed", failure);
            local_failure = local_failure || worker.socket_errors[slot] != 0 || worker.attempts[slot] == 0;
        }
        complete &= append_detail(output, size,
                                  "PTP socket RX drop counters and hardware timestamps are unavailable; no synchronization-accuracy claim is made.\n");
        if (!complete || s_stop_tests_requested.load() || loaded == ptp_tester::Result::CouldNotRun ||
            !worker.full_window || !covered || worker.pacing_misses || worker.buffer_errors || local_failure)
            return ptp_tester::Result::CouldNotRun;
        if (loaded == ptp_tester::Result::Fail || after_failed ||
            ((mask & 1) && (worker.timeouts[0] || worker.invalid[0])) ||
            ((mask & 2) && (worker.timeouts[1] || worker.invalid[1])))
            return ptp_tester::Result::Fail;
        return ptp_tester::Result::Pass;
    }

    TestStatus run_udp_definition(const TestDefinition &test, const TargetHistoryEntry &profile,
                                  const RunMetadata &metadata, char *output, size_t size)
    {
        const bool combined = ptp_tester::udp_involves_ntp(test.udp_test);
        auto context = std::unique_ptr<PtpRunContext>(new (std::nothrow) PtpRunContext{});
        ResponseBuffer evidence(nullptr, &std::free);
        if (!combined)
            evidence = allocate_response_buffer(PtpOutputSize);
        if (!context || (!combined && !evidence))
        {
            std::snprintf(output, size, "Insufficient memory for PTP L3 UDP evidence / operator context.");
            return TestStatus::CouldNotRun;
        }
        context->udp = true;
        ptp_tester::UdpConfig config{};
        config.ipv4_target = profile.ptp_udp_ipv4;
        config.ipv6_target = profile.ptp_udp_ipv6;
        config.ipv4 = test.family == AF_INET || test.family == AF_UNSPEC;
        config.ipv6 = test.family == AF_INET6 || test.family == AF_UNSPEC;
        config.domain = profile.ptp_udp_domain;
        config.timeout_ms = PtpTestTimeoutMs;
        config.observation_ms = PtpObservationMs;
        config.samples = PtpObservationSamples;
        config.response_window_ms = PtpResponseWindowMs;
        config.request_interval_ms = PtpRequestIntervalMs;
        config.withdrawal_ms = profile.ptp_udp_withdrawal_ms;
        config.recovery_ms = PtpRecoveryMs;
        config.netif = s_eth_netif;
        config.link_up = &s_ethernet_connected;
        config.link_generation = &s_tester_link_generation;
        config.assist = ptp_assist;
        config.progress = ptp_progress;
        config.observation = ptp_observation;
        config.context = context.get();
        output[0] = '\0';
        bool complete = append_detail(output, size,
                                      "PTP L3 UDP target IPv4=%s; IPv6=%s; domain=%u; operator firmware=%s; "
                                      "withdrawal bound=%lu ms. PTP L2 and NTP keys are not used by PTP L3 UDP.\n",
                                      config.ipv4 ? config.ipv4_target : "(not selected)",
                                      config.ipv6 ? config.ipv6_target : "(not selected)", config.domain,
                                      firmware_mode_label(metadata.mode), static_cast<unsigned long>(config.withdrawal_ms));
        if (evidence)
            evidence.get()[0] = '\0';
        auto result = combined
                          ? run_udp_combined_load(config, profile, output, size)
                          : ptp_tester::run_udp(test.udp_test, config, s_stop_tests_requested, evidence.get(), PtpOutputSize);
        if (!combined)
            complete &= append_detail(output, size, "%s\n", evidence.get());
        complete &= append_detail(output, size, "Operator phase evidence:\n%s\n",
                                  context->phases[0] ? context->phases : "No assisted phase requested.");
        if (context->skipped)
        {
            append_detail(output, size, "Operator skipped this test. Restore the DUT manually if changed; unresolved cleanup still blocks dependent tests.\n");
            return TestStatus::Skipped;
        }
        if (!complete || context->truncated || context->missing_action || s_stop_tests_requested.load())
        {
            append_detail(output, size, "Incomplete evidence / missing operator action / cancellation: Could not run.\n");
            result = ptp_tester::Result::CouldNotRun;
        }
        if (context->action_timed_out)
            return TestStatus::TimedOutAwaitingOperatorAction;
        return result == ptp_tester::Result::Pass   ? TestStatus::Pass
               : result == ptp_tester::Result::Fail ? TestStatus::Fail
                                                    : TestStatus::CouldNotRun;
    }

    TestStatus run_test(size_t index, const RunMetadata &metadata, char *timestamp, size_t timestamp_size)
    {
        const TestDefinition &test = Tests[index];
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        xSemaphoreGive(s_state_mutex);
        const bool ptp = is_ptp_test(test);
        if (const char *setting = disabled_ptp_setting(test))
        {
            std::snprintf(timestamp, timestamp_size,
                          "Intentionally not run: %s is set to false in the current firmware build. "
                          "Set %s to true in ESP32TimeServerTesterSettings.h and rebuild/flash the tester firmware to allow this test to run. "
                          "Other test prerequisites still apply.",
                          setting, setting);
            return TestStatus::DidNotRun;
        }
        if (const char *reason = firmware_mode_incompatibility_reason(test, metadata.mode))
        {
            std::snprintf(timestamp, timestamp_size, "Intentionally not run: %s", reason);
            return TestStatus::DidNotRun;
        }
        if (!test_available(test, profile))
        {
            std::snprintf(timestamp, timestamp_size, "%s",
                          is_ptp_udp_test(test) ? "PTP L3 UDP prerequisites missing: select the required master address(es), matching IP family/families and an active tester Ethernet link."
                          : ptp                 ? "PTP L2 prerequisites missing: enabled tester, selected target MAC/domain, tester Ethernet link and hardware timestamp engine; IP is not required."
                                                : "NTP/IP prerequisites missing, or mini stress disabled in target profile.");
            return !test.enabled || (test.kind == TestDefinition::Kind::MiniStress && !profile.stress_tests)
                       ? TestStatus::DidNotRun
                       : TestStatus::CouldNotRun;
        }
        if (is_ptp_udp_test(test))
            return run_udp_definition(test, profile, metadata, timestamp, timestamp_size);
        if (ptp)
        {
            timestamp[0] = '\0';
            const auto ptp_test = ptp_test_id(test);
            if (const char *pending = ptp_tester::pending_reason(ptp_test))
            {
                std::snprintf(timestamp, timestamp_size, "%s Independently selectable; not implemented or physically validated.", pending);
                return TestStatus::CouldNotRun;
            }
            const bool assisted = ptp_tester::requires_operator(ptp_test);
            const char *missing = nullptr;
            if (ptp_test == ptp_tester::Test::NegativeDelay && !PtpNegativeTestsEnabled)
                missing = "Negative tests require settings opt-in.";
            if (ptp_tester::requires_suppression_opt_in(ptp_test) &&
                (!PtpSuppressionTestEnabled || ptp_tester::policy_error(PtpDutPolicy) != nullptr || profile.ptp_domain != PtpDutPolicy.domain))
                missing = "Foreign-candidate scenarios require settings opt-in, valid versioned DUT metadata and matching domain. Unknown policy is not guessed.";
            if (assisted && (!PtpAssistedTestsEnabled || PtpOperatorPhaseTimeoutMs == 0 || PtpDutWithdrawalMs == 0))
                missing = "Assisted tests require settings opt-in, bounded acknowledgement timeout and configured DUT withdrawal limit.";
            if (assisted && ptp_tester::policy_error(PtpDutPolicy) != nullptr)
                missing = "Assisted recovery requires validated DUT policy metadata (including ranked listening and GNSS/PPS limits).";
            if (ptp_tester::involves_ntp(ptp_test) &&
                (!PtpCombinedLoadEnabled || !ptp_tester::mode_has_ntp(metadata.mode) ||
                 !ptp_tester::mode_has_l2(metadata.mode) ||
                 !profile.stress_tests || !test_available(Tests[2], profile) || !test_available(Tests[12], profile) ||
                 PtpCombinedRequestsPerSecond == 0 || PtpCombinedRequestsPerSecond > 100 ||
                 PtpObservationMs == 0 || PtpResponseWindowMs == 0))
                missing = "Combined load requires NTP + PTP L2 firmware, settings opt-in, isolated local link, profile stress toggle, both IP families/target NTP ports, nonzero duration/window and total rate 1..100 requests/s.";
            if (ptp_tester::involves_ntp(ptp_test) && ptp_tester::policy_error(PtpDutPolicy) != nullptr)
                missing = "Combined load requires validated DUT policy metadata; the NTP authentication policy cannot be guessed.";
            if (ptp_tester::involves_ntp(ptp_test) && PtpDutPolicy.ntp_authentication_required &&
                !has_test_key(profile.key))
                missing = "Required-NTP combined load needs the target profile's separate NTP symmetric key.";
            if (missing)
            {
                char policy[1024];
                ptp_policy_text(policy, sizeof(policy));
                std::snprintf(timestamp, timestamp_size, "%s\n%s", missing, policy);
                return TestStatus::CouldNotRun;
            }
            auto context = std::unique_ptr<PtpRunContext>(new (std::nothrow) PtpRunContext{});
            ResponseBuffer evidence = allocate_response_buffer(PtpOutputSize);
            if (!context || !evidence)
            {
                std::snprintf(timestamp, timestamp_size, "Insufficient memory for PTP evidence / phase context.");
                return TestStatus::CouldNotRun;
            }
            ptp_tester::Config config{};
            config.policy = PtpDutPolicy;
            config.key_id = profile.ptp_key.key_id;
            config.key_value = profile.ptp_key.key_value;
            config.rotation_key_id = PtpRotationFixtureKeyId;
            config.rotation_key_value = PtpRotationFixtureKeyValue;
            if (!parse_mac_address(profile.ptp_mac, config.target_mac))
            {
                std::snprintf(timestamp, timestamp_size, "CouldNotRun: configured PTP target MAC address could not be parsed.");
                return TestStatus::CouldNotRun;
            }
            config.domain = profile.ptp_domain;
            config.timeout_ms = PtpTestTimeoutMs;
            config.observation_ms = PtpObservationMs;
            config.samples = PtpObservationSamples;
            config.response_window_ms = PtpResponseWindowMs;
            config.request_interval_ms = PtpRequestIntervalMs;
            config.processing_allowance_ms = PtpProcessingAllowanceMs;
            config.foreign_quiet_ms = PtpDutForeignQuietMs;
            config.withdrawal_ms = PtpDutWithdrawalMs;
            config.recovery_ms = PtpRecoveryMs;
            config.firmware_mode = metadata.mode;
            config.isolated_link = true;
            config.negative_enabled = PtpNegativeTestsEnabled;
            config.suppression_enabled = PtpSuppressionTestEnabled;
            config.assisted_enabled = PtpAssistedTestsEnabled;
            config.rotation_enabled = PtpKeyRotationTestEnabled;
            config.combined_enabled = PtpCombinedLoadEnabled;
            config.no_ip_confirmed = true;
            config.sequence_seed_enabled = PtpSequenceSeedEnabled;
            config.sequence_seed = PtpSequenceSeed;
            config.assist = ptp_assist;
            config.progress = ptp_progress;
            config.target_identity = ptp_target_identity;
            config.observation = ptp_observation;
            config.context = context.get();
            char policy[1024];
            ptp_policy_text(policy, sizeof(policy));
            bool complete = append_detail(timestamp, timestamp_size,
                                          "%s\nTarget=%s; domain=%u; operator firmware=%s.\n"
                                          "Observation=%lu ms; samples=%u; response window=%lu ms; request interval=%lu ms; processing allowance=%lu ms; "
                                          "DUT foreign quiet=%lu ms; DUT withdrawal=%lu ms; recovery=%lu ms; operator phase limit=%lu ms; sequence seed=%s/%u.\n",
                                          policy,
                                          profile.ptp_mac, static_cast<unsigned>(profile.ptp_domain), firmware_mode_text(metadata.mode),
                                          static_cast<unsigned long>(config.observation_ms), static_cast<unsigned>(config.samples),
                                          static_cast<unsigned long>(config.response_window_ms), static_cast<unsigned long>(config.request_interval_ms),
                                          static_cast<unsigned long>(config.processing_allowance_ms), static_cast<unsigned long>(config.foreign_quiet_ms),
                                          static_cast<unsigned long>(config.withdrawal_ms), static_cast<unsigned long>(config.recovery_ms),
                                          static_cast<unsigned long>(operator_phase_timeout_ms(ptp_test)), config.sequence_seed_enabled ? "enabled" : "disabled",
                                          static_cast<unsigned>(config.sequence_seed));
            evidence.get()[0] = '\0';
            auto result = ptp_tester::involves_ntp(ptp_test)
                              ? run_combined_load(ptp_test, config, profile.key, timestamp, timestamp_size)
                              : ptp_tester::run(ptp_test, config, s_stop_tests_requested, evidence.get(), PtpOutputSize);
            if (!ptp_tester::involves_ntp(ptp_test))
                complete &= append_detail(timestamp, timestamp_size, "%s\n", evidence.get());
            complete &= append_detail(timestamp, timestamp_size, "Operator phase evidence:\n%s\n",
                                      context->phases[0] ? context->phases : "No assisted phase requested.");
            if (context->skipped)
            {
                append_detail(timestamp, timestamp_size, "Operator skipped this test. Restore the DUT manually if changed; unresolved cleanup still blocks dependent tests.\n");
                return TestStatus::Skipped;
            }
            if (!complete || context->truncated || context->missing_action || s_stop_tests_requested.load())
            {
                append_detail(timestamp, timestamp_size, "Incomplete evidence / missing operator action / cancellation: Could not run.\n");
                result = ptp_tester::Result::CouldNotRun;
            }
            if (context->action_timed_out)
                return TestStatus::TimedOutAwaitingOperatorAction;
            return result == ptp_tester::Result::Pass ? TestStatus::Pass : result == ptp_tester::Result::Fail ? TestStatus::Fail
                                                                                                              : TestStatus::CouldNotRun;
        }
        if (test.kind == TestDefinition::Kind::Ping)
            return ping_target(test.family) ? TestStatus::Pass : TestStatus::Fail;
        const symmetric_key_t &key = profile.key;
        if (test.kind == TestDefinition::Kind::Auth || test.kind == TestDefinition::Kind::AuthInterleaved)
            return ntp_authenticated_exchange(test.family, test.kind == TestDefinition::Kind::AuthInterleaved,
                                              timestamp, timestamp_size, key)
                       ? TestStatus::Pass
                       : TestStatus::Fail;
        if (test.kind == TestDefinition::Kind::InvalidAuth || test.kind == TestDefinition::Kind::TruncatedMac ||
            test.kind == TestDefinition::Kind::MissingAuth)
            return ntp_controlled_authentication_rejection(test.family, test.kind, key,
                                                           ptp_tester::policy_error(PtpDutPolicy) == nullptr && PtpDutPolicy.ntp_authentication_required,
                                                           timestamp, timestamp_size);
        const uint8_t version = test.kind == TestDefinition::Kind::StandardV3 ? 3 : 4;
        if (ptp_tester::policy_error(PtpDutPolicy) == nullptr &&
            PtpDutPolicy.ntp_authentication_required)
        {
            if (test.kind == TestDefinition::Kind::StandardV3 || test.kind == TestDefinition::Kind::StandardV4)
                return ntp_required_unsigned_rejection(test.family, version, key, timestamp, timestamp_size);
            if ((test.kind == TestDefinition::Kind::Interleaved || test.kind == TestDefinition::Kind::MiniStress) &&
                !has_test_key(key))
            {
                std::snprintf(timestamp, timestamp_size,
                              "CouldNotRun: required NTP authentication needs the target profile's separate NTP symmetric key.");
                return TestStatus::CouldNotRun;
            }
        }
        const bool required_ntp = ptp_tester::policy_error(PtpDutPolicy) == nullptr &&
                                  PtpDutPolicy.ntp_authentication_required;
        if (test.kind == TestDefinition::Kind::MiniStress)
        {
            for (int request = 0; request < number_of_NTP_requests_in_a_mini_stress_test; ++request)
            {
                if (s_stop_tests_requested.load())
                    return TestStatus::CouldNotRun;
                if (!stress_tests_enabled())
                    return TestStatus::DidNotRun;
                int failure = 0;
                if (!ntp_exchange(test.family, version, timestamp, timestamp_size, required_ntp ? 3000 : 0,
                                  &failure, nullptr, required_ntp ? &key : nullptr))
                {
                    if (required_ntp)
                    {
                        std::snprintf(timestamp, timestamp_size,
                                      "Authenticated NTPv4 mini stress stopped at request %d of %d; error=%d.",
                                      request + 1, number_of_NTP_requests_in_a_mini_stress_test, failure);
                        return failure == ETIMEDOUT || failure == EPROTO ? TestStatus::Fail : TestStatus::CouldNotRun;
                    }
                    return TestStatus::Fail;
                }
            }
            if (required_ntp)
                std::snprintf(timestamp, timestamp_size, "Authenticated NTPv4 mini stress: %d signed requests and verified responses.",
                              number_of_NTP_requests_in_a_mini_stress_test);
            return TestStatus::Pass;
        }
        if (test.kind == TestDefinition::Kind::Interleaved)
        {
            const bool succeeded = required_ntp
                                       ? ntp_authenticated_exchange(test.family, true, timestamp, timestamp_size, key)
                                       : ntp_interleaved_exchange(test.family, timestamp, timestamp_size);
            if (required_ntp && succeeded)
                std::snprintf(timestamp, timestamp_size, "Authenticated NTPv4 interleaved exchange: request and response signatures verified.");
            return succeeded ? TestStatus::Pass : TestStatus::Fail;
        }
        return ntp_exchange(test.family, version, timestamp, timestamp_size) ? TestStatus::Pass : TestStatus::Fail;
    }

    struct TestRequest
    {
        enum class Scope : uint8_t
        {
            All,
            Ping,
            Ntp,
            NtpIpv4,
            NtpIpv6,
            Ptp,
            PtpL2,
            PtpUnattended,
            PtpAssisted,
            PtpUdp,
            PtpUdpUnattended,
            PtpUdpAssisted,
            Individual
        } scope;
        int index;
        RunMetadata metadata;
        bool udp_ipv4_configured = false;
        bool udp_ipv6_configured = false;
    };

    bool test_selected(const TestRequest &request, size_t index)
    {
        const auto &test = Tests[index];
        const bool ptp = is_ptp_test(test);
        if (!compatible_with_firmware_mode(test, request.metadata.mode) ||
            !configured_ptp_udp_test(test, request.udp_ipv4_configured, request.udp_ipv6_configured))
            return false;
        const bool ntp = test.kind != TestDefinition::Kind::Ping && !ptp;
        return request.scope == TestRequest::Scope::All ||
               (request.scope == TestRequest::Scope::Ping && test.kind == TestDefinition::Kind::Ping) ||
               (request.scope == TestRequest::Scope::Ntp && ntp) ||
               (request.scope == TestRequest::Scope::NtpIpv4 && ntp && test.family == AF_INET) ||
               (request.scope == TestRequest::Scope::NtpIpv6 && ntp && test.family == AF_INET6) ||
               (request.scope == TestRequest::Scope::Ptp && ptp) ||
               (request.scope == TestRequest::Scope::PtpL2 && is_ptp_l2_test(test)) ||
               (request.scope == TestRequest::Scope::PtpUnattended && is_ptp_l2_test(test) &&
                !test_requires_operator(test)) ||
               (request.scope == TestRequest::Scope::PtpAssisted && is_ptp_l2_test(test) &&
                test_requires_operator(test)) ||
               (request.scope == TestRequest::Scope::PtpUdp && is_ptp_udp_test(test)) ||
               (request.scope == TestRequest::Scope::PtpUdpUnattended && is_ptp_udp_test(test) &&
                !test_requires_operator(test)) ||
               (request.scope == TestRequest::Scope::PtpUdpAssisted && is_ptp_udp_test(test) &&
                test_requires_operator(test)) ||
               (request.scope == TestRequest::Scope::Individual && index == static_cast<size_t>(request.index));
    }

    void test_task(void *parameter)
    {
        const TestRequest request = *static_cast<TestRequest *>(parameter);
        delete static_cast<TestRequest *>(parameter);
        const RunMetadata metadata = request.metadata;
        ResponseBuffer timestamp = allocate_response_buffer(ResultDetailSize);
        for (size_t index : TestOrder)
        {
            if (!test_selected(request, index))
                continue;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            // All workers from the preceding test have joined before cancellation is reset.
            s_stop_tests_requested.store(s_stop_run_requested.load());
            s_active_test = index;
            std::snprintf(s_progress, sizeof(s_progress), "%s", Tests[index].name);
            xSemaphoreGive(s_state_mutex);
            TestStatus status = TestStatus::CouldNotRun;
            if (timestamp)
            {
                std::snprintf(timestamp.get(), ResultDetailSize, "n/a");
                if (s_stop_run_requested.load())
                {
                    status = TestStatus::DidNotRun;
                    std::snprintf(timestamp.get(), ResultDetailSize, "Cancelled before execution.");
                }
                else
                {
                    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
                    s_results[index].status = TestStatus::Running;
                    xSemaphoreGive(s_state_mutex);
                    status = run_test(index, metadata, timestamp.get(), ResultDetailSize);
                    if (s_stop_run_requested.load() && status != TestStatus::Skipped)
                    {
                        status = TestStatus::CouldNotRun;
                        append_detail(timestamp.get(), ResultDetailSize, "\nInterrupted observation: cancelled; not a pass.\n");
                    }
                }
            }
            const char *details = timestamp ? timestamp.get() : "Insufficient memory for test output; not run.";
            const size_t capacity = std::strlen(details) + 512;
            std::unique_ptr<char[]> stored(new (std::nothrow) char[capacity]);
            if (stored)
                std::snprintf(stored.get(), capacity, "Operator firmware=%s.\n%s", firmware_mode_label(metadata.mode), details);
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_results[index].status = stored ? status : TestStatus::CouldNotRun;
            s_results[index].details = std::move(stored);
            std::snprintf(s_results[index].timestamp, sizeof(s_results[index].timestamp), "%s",
                          s_results[index].details ? "See detailed evidence" : "Insufficient memory to retain evidence; not a pass.");
            xSemaphoreGive(s_state_mutex);
            if (!s_stop_run_requested.load())
                vTaskDelay(pdMS_TO_TICKS(100));
        }
        timestamp.reset();
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        s_active_test = TestCount;
        s_phase_waiting = false;
        s_phase_prompt[0] = '\0';
        std::snprintf(s_progress, sizeof(s_progress), "%s", s_stop_run_requested.load() ? "Stopped; interrupted observations are not passes" : "Run complete");
        s_tests_running.store(false);
        s_test_leds_flash_on = true;
        update_status_leds_locked();
        xSemaphoreGive(s_state_mutex);
        vTaskDelete(nullptr);
    }

    bool start_tests(TestRequest::Scope scope, const RunMetadata &metadata, int index = -1)
    {
        TestRequest selection{scope, index, metadata};
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        selection.udp_ipv4_configured = s_profile.ptp_udp_ipv4[0] != '\0';
        selection.udp_ipv6_configured = s_profile.ptp_udp_ipv6[0] != '\0';
        bool available = false;
        for (size_t test_index = 0; test_index < Tests.size(); ++test_index)
            available = available || (test_selected(selection, test_index) && valid_profile(s_profile));
        bool expected = false;
        const bool started = !s_ote_in_progress && available && metadata.mode == s_profile.firmware_mode &&
                             s_tests_running.compare_exchange_strong(expected, true);
        if (started)
        {
            s_stop_tests_requested.store(false);
            s_stop_run_requested.store(false);
            s_phase_waiting = false;
            s_phase_acknowledged = false;
            s_phase_skipped = false;
            s_phase_prompt[0] = '\0';
            std::snprintf(s_progress, sizeof(s_progress), "Starting run");
            for (size_t test_index = 0; test_index < Tests.size(); ++test_index)
                if (test_selected(selection, test_index))
                    s_results[test_index] = {};
            s_test_leds_flash_on = true;
            update_status_leds_locked();
        }
        xSemaphoreGive(s_state_mutex);
        if (!started)
            return false;
        auto *request = new (std::nothrow) TestRequest{selection};
        if (request == nullptr)
        {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_tests_running.store(false);
            s_test_leds_flash_on = true;
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
            return false;
        }
        if (xTaskCreate(test_task, "protocol_tests", 16384, request, 5, nullptr) != pdPASS)
        {
            delete request;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_tests_running.store(false);
            s_test_leds_flash_on = true;
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
            return false;
        }
        return true;
    }

    void html_escape(const char *source, char *destination, size_t destination_size)
    {
        size_t cursor = 0;
        for (; *source && cursor + 6 < destination_size; ++source)
        {
            const char *replacement = nullptr;
            switch (*source)
            {
            case '&':
                replacement = "&amp;";
                break;
            case '<':
                replacement = "&lt;";
                break;
            case '>':
                replacement = "&gt;";
                break;
            case '"':
                replacement = "&quot;";
                break;
            default:
                destination[cursor++] = *source;
                continue;
            }
            const size_t replacement_size = std::strlen(replacement);
            std::memcpy(destination + cursor, replacement, replacement_size);
            cursor += replacement_size;
        }
        destination[cursor] = '\0';
    }

    void markdown_escape(const char *source, char *destination, size_t destination_size)
    {
        char escaped[TargetNameSize * 6];
        html_escape(source, escaped, sizeof(escaped));
        size_t cursor = 0;
        for (const char *character = escaped; *character && cursor + 2 < destination_size; ++character)
        {
            if (std::strchr("\\`*_{}[]()#+-.!|", *character) != nullptr)
                destination[cursor++] = '\\';
            destination[cursor++] = *character;
        }
        destination[cursor] = '\0';
    }

    void format_optional_u16(uint16_t value, char output[6])
    {
        output[0] = '\0';
        if (value != 0)
            std::snprintf(output, 6, "%u", value);
    }

    struct NetworkInfo
    {
        char mac[18] = "Not assigned";
        char ipv4[INET_ADDRSTRLEN] = "Not assigned";
        char lla[INET6_ADDRSTRLEN] = "Not assigned";
        char ula[INET6_ADDRSTRLEN] = "Not assigned";
        char gua[INET6_ADDRSTRLEN] = "Not assigned";
    };

    NetworkInfo read_network_info()
    {
        NetworkInfo info;
        if (s_eth_netif == nullptr)
            return info;
        uint8_t mac[6]{};
        if (esp_netif_get_mac(s_eth_netif, mac) == ESP_OK)
            std::snprintf(info.mac, sizeof(info.mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        esp_netif_ip_info_t ip{};
        if (esp_netif_get_ip_info(s_eth_netif, &ip) == ESP_OK && ip.ip.addr != 0)
            std::snprintf(info.ipv4, sizeof(info.ipv4), IPSTR, IP2STR(&ip.ip));
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        const int count = esp_netif_get_all_ip6(s_eth_netif, addresses);
        for (int index = 0; index < count; ++index)
        {
            char *destination = nullptr;
            switch (esp_netif_ip6_get_addr_type(&addresses[index]))
            {
            case ESP_IP6_ADDR_IS_LINK_LOCAL:
                destination = info.lla;
                break;
            case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
                destination = info.ula;
                break;
            case ESP_IP6_ADDR_IS_GLOBAL:
                destination = info.gua;
                break;
            default:
                break;
            }
            if (destination != nullptr)
                inet6_ntoa_r(addresses[index], destination, INET6_ADDRSTRLEN);
        }
        return info;
    }

    esp_err_t root_handler(httpd_req_t *request)
    {
        char target[TargetNameSize * 6];
        char stress_test_request_count[16];
        format_number_with_thousands(number_of_NTP_requests_in_a_mini_stress_test,
                                     stress_test_request_count, sizeof(stress_test_request_count));
        ResponseBuffer target_options = allocate_response_buffer(TargetOptionsBufferSize);
        if (!target_options)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate target history");
        target_options.get()[0] = '\0';
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        const bool physical_leds_on = s_physical_leds_on;
        html_escape(profile.target, target, sizeof(target));
        size_t options_cursor = 0;
        for (size_t index = 0; index < s_target_history.count; ++index)
        {
            const auto &entry = s_target_history.entries[index];
            char escaped_target[TargetNameSize * 6];
            html_escape(entry.target, escaped_target, sizeof(escaped_target));
            char ipv4_port[6], ipv6_port[6], key_id[6], ptp_key_id[6];
            format_optional_u16(entry.ipv4_ntp_port, ipv4_port);
            format_optional_u16(entry.ipv6_ntp_port, ipv6_port);
            format_optional_u16(entry.key.key_id, key_id);
            format_optional_u16(entry.ptp_key.key_id, ptp_key_id);
            const int written = std::snprintf(target_options.get() + options_cursor, TargetOptionsBufferSize - options_cursor,
                                              "<button class=\"target-option\" type=\"button\" role=\"option\" data-ipv4=\"%s\" data-ipv6=\"%s\" "
                                              "data-ipv4-port=\"%s\" data-ipv6-port=\"%s\" data-key-id=\"%s\" data-key-value=\"%s\" data-stress=\"%s\" "
                                              "data-ptp-mac=\"%s\" data-ptp-domain=\"%u\" data-ptp-key-id=\"%s\" data-ptp-key-value=\"%s\" "
                                              "data-ptp-udp-ipv4=\"%s\" data-ptp-udp-ipv6=\"%s\" data-ptp-udp-domain=\"%u\" data-ptp-udp-withdrawal=\"%lu\" "
                                              "data-firmware-mode=\"%s\">%s</button>",
                                              entry.ipv4, entry.ipv6, ipv4_port, ipv6_port, key_id, entry.key.key_value,
                                              entry.stress_tests ? "1" : "0", entry.ptp_mac, entry.ptp_domain,
                                              ptp_key_id, entry.ptp_key.key_value, entry.ptp_udp_ipv4, entry.ptp_udp_ipv6,
                                              static_cast<unsigned>(entry.ptp_udp_domain), static_cast<unsigned long>(entry.ptp_udp_withdrawal_ms),
                                              firmware_mode_text(entry.firmware_mode), escaped_target);
            if (written < 0 || static_cast<size_t>(written) >= TargetOptionsBufferSize - options_cursor)
            {
                xSemaphoreGive(s_state_mutex);
                return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Target history generation failed");
            }
            options_cursor += static_cast<size_t>(written);
        }
        xSemaphoreGive(s_state_mutex);
        char ipv4_port[6], ipv6_port[6], key_id[6], ptp_key_id[6];
        format_optional_u16(profile.ipv4_ntp_port, ipv4_port);
        format_optional_u16(profile.ipv6_ntp_port, ipv6_port);
        format_optional_u16(profile.key.key_id, key_id);
        format_optional_u16(profile.ptp_key.key_id, ptp_key_id);
        NetworkInfo network = read_network_info();
        const char *mac_address = network.mac;
        const char *ipv4_address = network.ipv4;
        const char *ipv6_lla = network.lla;
        const char *ipv6_ula = network.ula;
        const char *ipv6_gua = network.gua;
        ResponseBuffer page = allocate_response_buffer(PageBufferSize);
        if (!page)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate page");
        const int length = std::snprintf(page.get(), PageBufferSize,
                                         R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 Time Server Tester</title>
<style>body{font-family:Arial,sans-serif;margin:20px}h1{color:#0665b5;font-size:1.5em}hr{border:0;border-top:1px solid #999}label{margin:.5em 0}input{width:22em}input[type="checkbox"]{width:auto}input[type="number"]{height:1.5em;width:7em}#key-value,.key-visibility,#target-toggle{box-sizing:border-box;height:2em;vertical-align:middle}.key-visibility,#target-toggle{width:2em;padding:0;display:inline-flex;align-items:center;justify-content:center}.key-visibility svg,#target-toggle svg{width:1.35em;height:1.35em}button{margin:.25em;padding:.4em .7em}table{border-collapse:collapse;width:100%%;max-width:1000px;margin-top:.5em}th,td{border:1px solid #bbb;padding:.4em;text-align:left}.disabled{color:#777}.result{color:#000}.result.pass{color:#008000}.result.fail{color:#c00}footer{margin-top:1em}.target-combobox{display:inline-block;position:relative}.target-combobox input{width:22em}.target-combobox button{margin-left:.15em}.target-combobox>span{position:absolute;z-index:1;left:0;top:100%%;width:calc(100%% - 2.7em);max-height:12em;overflow-y:auto;background:#fff;border:1px solid #999}.target-option{display:block;width:100%%;margin:0;border:0;border-radius:0;text-align:left;background:#fff}.target-option:hover,.target-option.selected{background:#d9ecff}.settings-top{display:table;border-spacing:0}.settings-top .row{display:table-row}.settings-top .setting-label,.settings-top .setting-control{display:table-cell}.settings-top .setting-label{padding-right:.28em}.settings-top .row+.row .setting-label,.settings-top .row+.row .setting-control{padding-top:1em}.settings-grid{display:grid;grid-template-columns:minmax(0,34em) minmax(0,34em);gap:.8em 1.5em;max-width:72em;margin:1em 0}.settings-grid label{display:flex;align-items:center;gap:.5em;margin:0}.settings-grid label>span:first-child{min-width:10em}.settings-grid input:not([type="checkbox"]){min-width:0;flex:1}.settings-grid input[type="number"]{max-width:8em}.key-entry{display:flex;align-items:center;flex:1;min-width:0}.key-entry input{width:100%%}@media(max-width:780px){.settings-grid{grid-template-columns:1fr}}h1{display:flex;align-items:center;justify-content:space-between}.ip-leds{display:inline-flex;gap:.5em}.ip-led{width:1em;height:1em;border-radius:50%%;background:#0000ff;border:1px solid #555;display:inline-block}</style>
<style>.settings-grid label.key-label>span:first-child{min-width:13em}.key-entry input{box-sizing:border-box;height:2em;vertical-align:middle}.stress-row,.settings-actions{grid-column:1 / -1}@media(max-width:780px){.settings-grid{grid-template-columns:minmax(0,1fr)}}</style>
<style>h1{flex-wrap:wrap;gap:.25em .75em}.header-status{display:inline-flex;align-items:center;justify-content:flex-end;gap:.5em;margin-left:auto}.physical-led-control{display:inline-flex;align-items:center;margin:0;font-weight:normal;cursor:pointer}.physical-led-control[hidden]{display:none}.physical-led-target{position:relative;display:inline-flex;align-items:center;justify-content:center;width:36px;height:44px;flex:none}.physical-led-target input{position:absolute;inset:0;width:100%%;height:100%%;margin:0;opacity:0;cursor:pointer;z-index:1}.physical-led-track{position:relative;width:1.1em;height:calc(.5em + 1px);border-radius:1em;background:#ddd}.physical-led-track::after{content:"";position:absolute;top:2px;left:2px;height:calc(100%% - 4px);aspect-ratio:1;border-radius:50%%;background:#fff}.physical-led-target input:checked+.physical-led-track{background:#48cc00}.physical-led-target input:checked+.physical-led-track::after{left:calc(100%% - 2px);transform:translateX(-100%%)}.physical-led-target input:focus-visible+.physical-led-track{outline:2px solid #0665b5;outline-offset:3px}.physical-led-target input:disabled{cursor:wait}.ip-leds{align-items:center}</style>
</head><body><h1>ESP32TimeServerTester<span class="header-status"><label id="physical-leds-control" class="physical-led-control" for="physical-leds" title="Physical LED indicators" %s><span class="physical-led-target"><input id="physical-leds" type="checkbox" role="switch" aria-label="Physical LED indicators" title="Physical LED indicators" %s %s><span class="physical-led-track" aria-hidden="true"></span></span></label><span class="ip-leds"><span id="led-ipv4" class="ip-led" title="IPv4"></span><span id="led-ipv6" class="ip-led" title="IPv6"></span></span></span></h1><hr><h2>Settings</h2>
<form id="settings-form" method="post" action="/settings">
<input type="hidden" name="client_time" id="client-time">
<div class="settings-top">
<div class="row">
<label class="setting-label" for="target">Target time server:</label><span class="setting-control"><span class="target-combobox"><input id="target" type="text" role="combobox" aria-controls="target-menu" aria-expanded="false" name="target" minlength="1" maxlength="40" value="%s" placeholder="Target name" autocomplete="off" required><button id="target-toggle" type="button" aria-label="Show saved target profiles"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="m6 9 6 6 6-6"/></svg></button><span id="target-menu" role="listbox" hidden>%s</span></span></span>
</div>
<div class="row">
<label class="setting-label" for="firmware-mode">Firmware mode:</label><span class="setting-control"><select id="firmware-mode" name="firmware_mode"><option value="Unknown" %s>unknown</option><option value="NtpOnly" %s>NTP only</option><option value="PtpOnly" %s>PTP L2 only</option><option value="PtpL3Only" %s>PTP L3 UDP only</option><option value="Combined" %s>NTP + PTP L2</option><option value="NtpPtpL3" %s>NTP + PTP L3 UDP</option><option value="PtpL2PtpL3" %s>PTP L2 + PTP L3 UDP</option><option value="NtpPtpL2PtpL3" %s>NTP + PTP L2 + PTP L3 UDP</option></select></span>
</div>
</div>
<div class="settings-grid">
<label><span>NTP IPv4 address:</span><input id="ipv4" name="ipv4" maxlength="15" pattern="[0-9.]*" title="Enter an IPv4 address containing only digits and periods" value="%s" placeholder="Optional IPv4 address" inputmode="decimal" autocomplete="off"></label>
<label><span>NTP IPv4 Port:</span><input id="ipv4-ntp-port" name="ipv4_ntp_port" type="number" min="1" max="65535" step="1" value="%s"></label>
<label><span>NTP IPv6 address:</span><input id="ipv6" name="ipv6" maxlength="45" pattern="[0-9A-Fa-f:]*" title="Enter an IPv6 address containing only hexadecimal digits and colons" value="%s" placeholder="Optional IPv6 address" autocomplete="off"></label>
<label><span>NTP IPv6 Port:</span><input id="ipv6-ntp-port" name="ipv6_ntp_port" type="number" min="1" max="65535" step="1" value="%s"></label>
<label class="key-label"><span>NTPv4 symmetric-key id:</span><input id="key-id" name="key_id" type="number" min="1" max="65535" step="1" value="%s"></label>
<label class="key-label"><span>NTPv4 symmetric-key value:</span><span class="key-entry"><input id="key-value" name="key_value" type="password" maxlength="32" pattern="[A-Za-z0-9]{32}" value="%s" autocomplete="off" spellcheck="false"><button class="key-visibility" type="button" id="toggle-key" aria-label="Show symmetric-key value" aria-pressed="false"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M2 12C6 3 18 3 22 12C18 21 6 21 2 12Z"/><circle cx="12" cy="12" r="3"/><path id="key-hidden" d="M3 21 21 3"/></svg></button></span></label>
<div class="row stress-row"><label><input type="checkbox" id="stress-tests" name="stress_tests" value="1" %s> Enable IPv4 and IPv6 NTPv4 Standard mini saturated stress tests ( %s NTP requests each )</label></div>
<label><span>PTP L2 master MAC:</span><input id="ptp-mac" name="ptp_mac" maxlength="17" pattern="([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}" value="%s" placeholder="Optional master Ethernet MAC" autocomplete="off"></label>
<label><span>PTP L2 domain:</span><input id="ptp-domain" name="ptp_domain" type="number" min="0" max="255" step="1" value="%u" required></label>
<label class="key-label"><span>PTP L2 symmetric-key id:</span><input id="ptp-key-id" name="ptp_key_id" type="number" min="1" max="65535" step="1" value="%s"></label>
<label class="key-label"><span>PTP L2 symmetric-key value:</span><span class="key-entry"><input id="ptp-key-value" name="ptp_key_value" type="password" maxlength="32" pattern="[A-Za-z0-9]{32}" value="%s" autocomplete="off" spellcheck="false"><button class="key-visibility" type="button" id="toggle-ptp-key" aria-label="Show PTP L2 symmetric-key value" aria-pressed="false"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M2 12C6 3 18 3 22 12C18 21 6 21 2 12Z"/><circle cx="12" cy="12" r="3"/><path id="ptp-key-hidden" d="M3 21 21 3"/></svg></button></span></label>
<label class="key-label"><span>PTP L3 UDP IPv4 master:</span><input id="ptp-udp-ipv4" name="ptp_udp_ipv4" maxlength="15" pattern="[0-9.]*" value="%s" placeholder="Blank disables IPv4 PTP L3 UDP" autocomplete="off"></label>
<label class="key-label"><span>PTP L3 UDP IPv6 master:</span><input id="ptp-udp-ipv6" name="ptp_udp_ipv6" maxlength="45" pattern="[0-9A-Fa-f:]*" value="%s" placeholder="Master link-local IPv6; blank disables" autocomplete="off"></label>
<label class="key-label"><span>PTP L3 UDP domain:</span><input id="ptp-udp-domain" name="ptp_udp_domain" type="number" min="0" max="255" step="1" value="%u" required></label>
<label class="key-label"><span>PTP L3 UDP withdrawal ms:</span><input id="ptp-udp-withdrawal" name="ptp_udp_withdrawal" type="number" min="1000" max="60000" step="1" value="%lu" required></label>
<div class="settings-actions">
<button id="save-settings" type="submit" disabled>Save changes</button>
<button id="cancel-settings" type="button" disabled>Cancel changes</button>
<button id="clone-target" type="button" disabled>Clone target</button>
<button id="rename-target" type="button" disabled>Rename target</button>
<button id="delete-target" type="button" disabled>Delete target</button>
</div>
</div>
</form>
<dialog id="rename-dialog"><form id="rename-form"><label>Target time server: <input id="new-target-name" type="text" minlength="1" maxlength="40" required></label><div><button type="submit">Change</button><button type="button" id="cancel-rename">Cancel</button></div></form></dialog>
<hr><h2>Tests</h2><button class="test-run" onclick="run('all')" disabled>Run all tests</button><button class="test-run" onclick="run('ping')" disabled>Run ping tests</button><button class="test-run" onclick="run('ntp')" disabled>Run all NTP tests</button>
<button class="test-run" onclick="run('ipv4')" disabled>Run all NTP tests over IPv4</button><button class="test-run" onclick="run('ipv6')" disabled>Run all NTP tests over IPv6</button>
<button class="test-run" onclick="run('ptp-l2')" disabled>Run all PTP L2 tests</button>
<button class="test-run" onclick="run('ptp-auto')" disabled>Run PTP L2 tests not needing operator assistance</button>
<button class="test-run" onclick="run('ptp-assist')" disabled>Run PTP L2 tests needing operator assistance</button>
<button class="test-run" onclick="run('ptp-l3')" disabled>Run all PTP L3 UDP tests</button>
<button class="test-run" onclick="run('ptp-l3-auto')" disabled>Run PTP L3 UDP tests not needing operator assistance</button>
<button class="test-run" onclick="run('ptp-l3-assist')" disabled>Run PTP L3 UDP tests needing operator assistance</button>
<button class="test-run" onclick="run('ptp')" disabled>Run all PTP tests</button>
<button id="stop-tests" onclick="stopTests()" disabled>Stop tests</button><br>
<p>%s</p>
<fieldset id="run-metadata"><legend>Device Under Test (DUT) policy metadata</legend>
<p id="ptp-policy">Loading non-secret DUT policy metadata...</p>
<p id="ptp-preflight">Selected PTP target/observed identity preflight...</p>
<p>Settings opt-ins still apply. Unknown permits normal positive tests; combined-load tests require an explicitly declared NTP and corresponding PTP transport. Leave an L3 UDP master address blank to disable that family. The IPv6 master address is normally its Ethernet link-local address. L3 UDP uses no PTP symmetric key. Missing prerequisites or unacknowledged phases are not passes.</p></fieldset>
<p id="test-progress" role="status">Idle</p>
<div id="operator-phase" hidden><strong>Operator action required:</strong><p id="phase-prompt"></p><button id="ack-phase" onclick="acknowledgePhase()">I performed and verified this action</button><button id="skip-phase" onclick="skipPhase()">Skip this test</button></div>
<button id="clear-results" onclick="clearResults()" disabled>Clear all test results</button>
<table><thead><tr><th>Test</th><th>Result</th><th>Reported date/time or PTP diagnostic</th></tr></thead><tbody id="tests"></tbody></table>
<hr><h2>Download</h2><a href="/report" id="download-link"><button id="download-button" disabled>Download report</button></a><hr>
<pre>%s v%s

MAC address:      <span id="network-mac">%s</span>
IPv4 address:     <span id="network-ipv4">%s</span>
IPv6 LLA address: <span id="network-lla">%s</span>
IPv6 ULA address: <span id="network-ula">%s</span>
IPv6 GUA address: <span id="network-gua">%s</span></pre><hr>
<footer><br>Copyright Rob Latour, 2026<br>License: MIT<br>Website: <a href="https://github.com/roblatour/ESP32TimeServerTester" target="_blank">https://github.com/roblatour/ESP32TimeServerTester</a></footer>
<script>
function wireKeyVisibility(fieldId,buttonId,hiddenId,name){
    const field=document.getElementById(fieldId),button=document.getElementById(buttonId);
    button.addEventListener('click',()=>{const visible=field.type==='password';field.type=visible?'text':'password';button.setAttribute('aria-pressed',String(visible));button.setAttribute('aria-label',(visible?'Hide ':'Show ')+name);document.getElementById(hiddenId).style.display=visible?'none':''});
}
wireKeyVisibility('key-value','toggle-key','key-hidden','symmetric-key value');
wireKeyVisibility('ptp-key-value','toggle-ptp-key','ptp-key-hidden','PTP L2 symmetric-key value');
const settingsForm=document.getElementById('settings-form'),settingsInputs=Array.from(settingsForm.querySelectorAll('input:not([type="hidden"]),select')),targetField=document.getElementById('target');
const targetMenu=document.getElementById('target-menu'),targetToggle=document.getElementById('target-toggle');
const profileFields={firmwareMode:document.getElementById('firmware-mode'),ipv4:document.getElementById('ipv4'),ipv6:document.getElementById('ipv6'),ipv4Port:document.getElementById('ipv4-ntp-port'),ipv6Port:document.getElementById('ipv6-ntp-port'),keyId:document.getElementById('key-id'),keyValue:document.getElementById('key-value'),stress:document.getElementById('stress-tests'),ptpMac:document.getElementById('ptp-mac'),ptpDomain:document.getElementById('ptp-domain'),ptpKeyId:document.getElementById('ptp-key-id'),ptpKeyValue:document.getElementById('ptp-key-value'),ptpUdpIpv4:document.getElementById('ptp-udp-ipv4'),ptpUdpIpv6:document.getElementById('ptp-udp-ipv6'),ptpUdpDomain:document.getElementById('ptp-udp-domain'),ptpUdpWithdrawal:document.getElementById('ptp-udp-withdrawal')};
let selectedTargetOption=null;
function savedOption(){return Array.from(targetMenu.querySelectorAll('.target-option')).find(option=>option.textContent===targetField.value)}
function loadProfile(option){
    const data=option?.dataset;
    profileFields.firmwareMode.value=data?.firmwareMode||'Unknown';
    profileFields.ipv4.value=data?.ipv4||'';
    profileFields.ipv6.value=data?.ipv6||'';
    profileFields.ipv4Port.value=data?data.ipv4Port||'':String(%u);
    profileFields.ipv6Port.value=data?data.ipv6Port||'':String(%u);
    profileFields.keyId.value=data?.keyId||'';
    profileFields.keyValue.value=data?.keyValue||'';
    profileFields.stress.checked=data?.stress==='1';
    profileFields.ptpMac.value=data?.ptpMac||'';
    profileFields.ptpDomain.value=data?.ptpDomain??String(%u);
    profileFields.ptpKeyId.value=data?.ptpKeyId||'';
    profileFields.ptpKeyValue.value=data?.ptpKeyValue||'';
    profileFields.ptpUdpIpv4.value=data?.ptpUdpIpv4||'';
    profileFields.ptpUdpIpv6.value=data?.ptpUdpIpv6||'';
    profileFields.ptpUdpDomain.value=data?.ptpUdpDomain??String(%u);
    profileFields.ptpUdpWithdrawal.value=data?.ptpUdpWithdrawal??String(%lu);
    updateSaveButton();
}
async function selectTarget(option){
    targetMenu.hidden=true;
    targetField.setAttribute('aria-expanded','false');
    markTargetOption(null);
    if(option.textContent===targetField.value&&document.getElementById('save-settings').disabled)return;
    if(!document.getElementById('save-settings').disabled){
        if(confirm('You have unsaved changes to the current target time server.\n\nOK = keep (save) the changes\nCancel = discard the changes')){
            if(!settingsForm.reportValidity())return;
            document.getElementById('client-time').value=clientTime();
            try{
                const saved=await fetch('/settings',{method:'POST',body:new URLSearchParams(new FormData(settingsForm))});
                if(!saved.ok){alert(await saved.text());return}
            }catch{alert('Unable to save settings.');return}
        }
    }
    const data=option.dataset;
    const body=new URLSearchParams({target:option.textContent,firmware_mode:data.firmwareMode||'Unknown',ipv4:data.ipv4||'',ipv6:data.ipv6||'',ipv4_ntp_port:data.ipv4Port||'',ipv6_ntp_port:data.ipv6Port||'',key_id:data.keyId||'',key_value:data.keyValue||'',ptp_mac:data.ptpMac||'',ptp_domain:data.ptpDomain??String(%u),ptp_key_id:data.ptpKeyId||'',ptp_key_value:data.ptpKeyValue||'',ptp_udp_ipv4:data.ptpUdpIpv4||'',ptp_udp_ipv6:data.ptpUdpIpv6||'',ptp_udp_domain:data.ptpUdpDomain??String(%u),ptp_udp_withdrawal:data.ptpUdpWithdrawal??String(%lu),client_time:clientTime()});
    if(data.stress==='1')body.set('stress_tests','1');
    try{
        const response=await fetch('/settings',{method:'POST',body});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to load the selected target.')}
}
function markTargetOption(option){
    if(selectedTargetOption)selectedTargetOption.classList.remove('selected');
    selectedTargetOption=option;
    if(option)option.classList.add('selected');
}
targetToggle.addEventListener('click',()=>{targetMenu.hidden=!targetMenu.hidden;targetField.setAttribute('aria-expanded',String(!targetMenu.hidden));if(targetMenu.hidden)markTargetOption(null);else targetField.focus()});
targetMenu.addEventListener('click',event=>{const option=event.target.closest('.target-option');if(option)selectTarget(option)});
targetMenu.addEventListener('contextmenu',event=>{const option=event.target.closest('.target-option');if(!option)return;event.preventDefault();markTargetOption(option)});
async function deleteTarget(option){
    if(!option)return;
    if(!confirm('Delete saved target '+option.textContent+' and all its settings?'))return;
    try{
        const response=await fetch('/targets/delete',{method:'POST',body:new URLSearchParams({target:option.textContent,client_time:clientTime()})});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to delete saved target.')}
}
document.addEventListener('keydown',event=>{if(event.key==='Delete'&&selectedTargetOption){event.preventDefault();deleteTarget(selectedTargetOption)}});
document.getElementById('delete-target').addEventListener('click',()=>deleteTarget(savedOption()));
targetField.addEventListener('input',()=>{markTargetOption(null);loadProfile(savedOption())});
document.addEventListener('click',event=>{if(!event.target.closest('.target-combobox')){targetMenu.hidden=true;targetField.setAttribute('aria-expanded','false');markTargetOption(null)}});
let currentStatus=null;
const physicalLedsControl=document.getElementById('physical-leds-control'),physicalLedsSwitch=document.getElementById('physical-leds');
const physicalLedsCapable=!physicalLedsControl.hidden;
let physicalLedsAvailable=physicalLedsCapable,physicalLedsOn=physicalLedsSwitch.checked,physicalLedsPending=false,physicalLedsRevision=0;
let webLedFlashTimer=null,webLedsFlashOn=true,webLedColors={ipv4:'#0000ff',ipv6:'#0000ff'};
function renderWebLedsFlash(){
    document.getElementById('led-ipv4').style.background=webLedsFlashOn?webLedColors.ipv4:'#fff';
    document.getElementById('led-ipv6').style.background=webLedsFlashOn?webLedColors.ipv6:'#fff';
}
function setWebLedsFlashing(flashing,flashOn){
    if(!flashing){
        if(webLedFlashTimer!==null){clearInterval(webLedFlashTimer);webLedFlashTimer=null}
        webLedsFlashOn=true;
        renderWebLedsFlash();
        return;
    }
    webLedsFlashOn=flashOn;
    renderWebLedsFlash();
    if(webLedFlashTimer!==null)return;
    webLedFlashTimer=setInterval(()=>{webLedsFlashOn=!webLedsFlashOn;renderWebLedsFlash()},1500);
}
function renderPhysicalLeds(){
    physicalLedsControl.hidden=!physicalLedsCapable||!physicalLedsAvailable;
    physicalLedsSwitch.checked=physicalLedsOn;
    physicalLedsSwitch.disabled=physicalLedsPending||!physicalLedsCapable||!physicalLedsAvailable;
}
function applyPhysicalLeds(j){
    physicalLedsAvailable=physicalLedsCapable&&j.physical_leds_available===true;
    if(typeof j.physical_leds_on==='boolean')physicalLedsOn=j.physical_leds_on;
    renderPhysicalLeds();
}
physicalLedsSwitch.addEventListener('change',async ()=>{
    if(physicalLedsPending||!physicalLedsCapable||!physicalLedsAvailable){renderPhysicalLeds();return}
    const previous=physicalLedsOn;
    physicalLedsOn=physicalLedsSwitch.checked;
    physicalLedsPending=true;
    renderPhysicalLeds();
    const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),4000);
    try{
        const response=await fetch('/physical-leds',{method:'POST',body:new URLSearchParams({enabled:physicalLedsOn?'1':'0'}),signal:controller.signal});
        if(!response.ok)throw new Error(await response.text()||'Unable to save physical LED preference.');
        const state=await response.json();
        if(typeof state.physical_leds_available!=='boolean'||typeof state.physical_leds_on!=='boolean')throw new Error('Invalid physical LED response.');
        applyPhysicalLeds(state);
    }catch(error){
        physicalLedsOn=previous;
        alert('Unable to save physical LED preference. '+error.message);
    }finally{
        clearTimeout(timeout);
        // Only polls begun after this request completes may update the LED preference.
        ++physicalLedsRevision;
        physicalLedsPending=false;
        renderPhysicalLeds();
        refresh();
    }
});
function updateSaveButton(){
    const changed=settingsInputs.some(input=>input.type==='checkbox'?input.checked!==input.defaultChecked:input.tagName==='SELECT'?!input.selectedOptions[0]?.defaultSelected:input.value!==input.defaultValue);
    document.getElementById('save-settings').disabled=!changed;
    document.getElementById('cancel-settings').disabled=!changed;
    const canManage=!changed&&!!savedOption()&&!currentStatus?.running;
    document.getElementById('clone-target').disabled=!canManage||targetField.value.length+6>40;
    document.getElementById('rename-target').disabled=!canManage;
    document.getElementById('delete-target').disabled=!savedOption()||!!currentStatus?.running;
    if(currentStatus)renderStatus(currentStatus);
}
function renderStatus(j){
    const colors={blue:'#0000ff',green:'#00c000',yellow:'#ffd000',red:'#e00000'};
    const v6={gua:['blue','IPv6: global unicast address (GUA)'],ula:['green','IPv6: unique local address (ULA)'],lla:['yellow','IPv6: link-local address (LLA)'],none:['red','IPv6: not available']};
    const led4=document.getElementById('led-ipv4'),led6=document.getElementById('led-ipv6');
    const c4=!j.ready?'blue':!j.link?'red':j.ipv4?'green':'red',s6=v6[j.ipv6]||v6.none,c6=!j.ready?'blue':!j.link?'red':s6[0];
    webLedColors={ipv4:colors[c4],ipv6:colors[c6]};
    setWebLedsFlashing(j.physical_leds_flashing===true,j.physical_leds_flash_on===true);
    led4.title=!j.ready?'IPv4: starting up':!j.link?'Ethernet: link disconnected':j.ipv4?'IPv4: available':'IPv4: not available';
    led6.title=!j.ready?'IPv6: starting up':!j.link?'Ethernet: link disconnected':s6[1];
    document.getElementById('network-mac').textContent=j.network.mac;
    document.getElementById('network-ipv4').textContent=j.network.ipv4;
    document.getElementById('network-lla').textContent=j.network.lla;
    document.getElementById('network-ula').textContent=j.network.ula;
    document.getElementById('network-gua').textContent=j.network.gua;
    document.getElementById('ptp-policy').textContent=j.ptp_policy;
    document.getElementById('ptp-preflight').textContent=j.ptp_preflight;
    const ready=j.target_valid&&document.getElementById('save-settings').disabled;
    const modeAllows=test=>test.compatible;
    const available=filter=>j.tests.some(test=>test.enabled&&modeAllows(test)&&filter(test));
    const groups=[()=>true,test=>!test.ntp&&!test.ptp,test=>test.ntp,test=>test.ntp&&test.family===4,test=>test.ntp&&test.family===6,test=>test.ptpL2,test=>test.ptpL2&&!test.assisted,test=>test.ptpL2&&test.assisted,test=>test.ptpL3,test=>test.ptpL3&&!test.assisted,test=>test.ptpL3&&test.assisted,test=>test.ptp];
    document.querySelectorAll('.test-run').forEach((button,index)=>button.disabled=!ready||j.running||!available(groups[index]));
    document.getElementById('delete-target').disabled=!savedOption()||j.running;
    const hasResults=j.tests.some(t=>t.status!=='Not yet run');
    document.getElementById('stop-tests').disabled=!j.running;
    document.getElementById('clear-results').disabled=j.running||!hasResults;
    document.getElementById('download-button').disabled=!hasResults;
    document.getElementById('download-link').style.pointerEvents=hasResults?'':'none';
    document.getElementById('run-metadata').disabled=j.running;
    document.getElementById('firmware-mode').disabled=j.running;
    const activeTest=j.tests.find(test=>test.id===j.active_test);
    document.getElementById('test-progress').textContent=(activeTest?'Test '+activeTest.number+': ':'')+j.progress;
    document.getElementById('operator-phase').hidden=!j.phase_waiting||!j.running;
    document.getElementById('phase-prompt').textContent=j.phase_prompt;
    document.getElementById('ack-phase').disabled=!j.phase_waiting||!j.running;
    document.getElementById('skip-phase').disabled=!j.phase_waiting||!j.running;
    const rows=j.tests.map(t=>{
        const row=document.createElement('tr');
        const enabled=t.enabled&&modeAllows(t);
        if(!enabled||!ready)row.className='disabled';
        const name=row.insertCell();name.textContent=t.number+'. '+t.name;
        const result=row.insertCell(),button=document.createElement('button');
        button.textContent='Run';button.disabled=!enabled||!ready||j.running;button.onclick=()=>run('test-'+t.id);result.append(button);
        const label=document.createElement('span');label.className='result '+(t.status==='Pass'?'pass':t.status==='Fail'?'fail':'');label.textContent=t.status;result.append(label);
        const detail=row.insertCell();detail.style.whiteSpace='pre-wrap';detail.style.overflowWrap='anywhere';detail.textContent=t.time;
        return row;
    });
    document.getElementById('tests').replaceChildren(...rows);
}
function renderConnectionLost(){
    const lost='Connection lost';
    const led4=document.getElementById('led-ipv4'),led6=document.getElementById('led-ipv6');
    led4.style.background='#e00000';led6.style.background='#e00000';
    led4.title='Ethernet: connection lost';led6.title='Ethernet: connection lost';
    ['network-ipv4','network-lla','network-ula','network-gua'].forEach(id=>{
        const address=document.getElementById(id);
        if(address.textContent!=='Not assigned')address.textContent=lost;
    });
}
settingsForm.addEventListener('input',updateSaveButton);
settingsForm.addEventListener('change',updateSaveButton);
window.addEventListener('pageshow',updateSaveButton);
updateSaveButton();
function clientTime(){const d=new Date(),p=n=>String(n).padStart(2,'0');return d.getFullYear()+'-'+p(d.getMonth()+1)+'-'+p(d.getDate())+' '+p(d.getHours())+':'+p(d.getMinutes())+':'+p(d.getSeconds())}
settingsForm.addEventListener('submit',async event=>{
    event.preventDefault();
    if(!settingsForm.reportValidity())return;
    document.getElementById('client-time').value=clientTime();
    try{
        const response=await fetch('/settings',{method:'POST',body:new URLSearchParams(new FormData(settingsForm))});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to save settings.')}
});
document.getElementById('cancel-settings').addEventListener('click',async ()=>{
    try{
        const response=await fetch('/settings/cancel',{method:'POST'});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to reload saved settings.')}
});
async function changeTarget(action,newName){
    try{
        const response=await fetch('/targets/'+action,{method:'POST',body:new URLSearchParams({target:targetField.value,new_target:newName,client_time:clientTime()})});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to '+action+' target.')}
}
document.getElementById('clone-target').addEventListener('click',()=>changeTarget('clone',targetField.value+' - Copy'));
const renameDialog=document.getElementById('rename-dialog'),newTargetName=document.getElementById('new-target-name');
document.getElementById('rename-target').addEventListener('click',()=>{newTargetName.value=targetField.value;renameDialog.showModal();newTargetName.focus();newTargetName.select()});
document.getElementById('cancel-rename').addEventListener('click',()=>renameDialog.close());
document.getElementById('rename-form').addEventListener('submit',event=>{event.preventDefault();if(newTargetName.value===targetField.value){renameDialog.close();return}changeTarget('rename',newTargetName.value)});
let statusRequestPending=false;
async function refresh(){
    if(statusRequestPending)return;
    statusRequestPending=true;
    const ledRevision=physicalLedsRevision,ledRequestPending=physicalLedsPending;
    const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),4000);
    try{
        const response=await fetch('/status?timestamp='+Date.now(),{cache:'no-store',signal:controller.signal});
        if(!response.ok)throw new Error('Unable to refresh status.');
        currentStatus=await response.json();
        if(!ledRequestPending&&!physicalLedsPending&&ledRevision===physicalLedsRevision)applyPhysicalLeds(currentStatus);
        updateSaveButton();
    }catch(error){
        console.warn('Unable to refresh ESP32 status.',error);
        renderConnectionLost();
    }finally{
        clearTimeout(timeout);
        statusRequestPending=false;
    }
}
async function run(x){
    if(!currentStatus?.target_valid||!document.getElementById('save-settings').disabled)return;
    const safety=await fetch('/target-safety');
    if(await safety.text()==='external'&&!confirm('Warning: this target is a public time-server pool or resolves to an external address. Testing it may result in your address being blocked or banned. Continue only if you are authorized to test this server.'))return;
    if(!document.getElementById('save-settings').disabled)return;
    const body=new URLSearchParams({scope:x,firmware_mode:document.getElementById('firmware-mode').value});
    const response=await fetch('/run',{method:'POST',body});
    if(!response.ok)alert(await response.text());
    refresh();
}
async function acknowledgePhase(){
    if(!currentStatus?.running||!currentStatus.phase_waiting)return;
    const token=currentStatus.phase_token;
    if(!confirm(currentStatus.phase_prompt+'\n\nConfirm only if you performed and verified this action.'))return;
    const response=await fetch('/phase/ack',{method:'POST',body:new URLSearchParams({token:String(token)})});
    if(!response.ok)alert(await response.text());
    refresh();
}
async function skipPhase(){
    if(!currentStatus?.running||!currentStatus.phase_waiting)return;
    const token=currentStatus.phase_token;
    const response=await fetch('/phase/skip',{method:'POST',body:new URLSearchParams({token:String(token)})});
    if(!response.ok)alert(await response.text());
    refresh();
}
async function stopTests(){let r=await fetch('/stop');if(!r.ok)alert(await r.text());refresh()}
async function clearResults(){let r=await fetch('/clear');if(!r.ok)alert(await r.text());refresh()}
refresh();setInterval(refresh,1500);
</script></body></html>)HTML",
                                         WS2812_RGB_enabled ? "" : "hidden",
                                         physical_leds_on ? "checked" : "",
                                         WS2812_RGB_enabled ? "" : "disabled",
                                         target, target_options.get(),
                                         profile.firmware_mode == ptp_tester::FirmwareMode::Unknown ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::NtpOnly ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::PtpOnly ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::PtpL3Only ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::Combined ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::NtpPtpL3 ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::PtpL2PtpL3 ? "selected" : "",
                                         profile.firmware_mode == ptp_tester::FirmwareMode::NtpPtpL2PtpL3 ? "selected" : "",
                                         profile.ipv4, ipv4_port,
                                         profile.ipv6, ipv6_port, key_id, profile.key.key_value,
                                         profile.stress_tests ? "checked" : "", stress_test_request_count,
                                         profile.ptp_mac, static_cast<unsigned>(profile.ptp_domain),
                                         ptp_key_id, profile.ptp_key.key_value,
                                         profile.ptp_udp_ipv4, profile.ptp_udp_ipv6, static_cast<unsigned>(profile.ptp_udp_domain),
                                         static_cast<unsigned long>(profile.ptp_udp_withdrawal_ms), PtpWarning,
                                         get_app_metadata()->project_name, get_app_metadata()->version,
                                         mac_address, ipv4_address, ipv6_lla, ipv6_ula, ipv6_gua,
                                         static_cast<unsigned>(DefaultNtpPort), static_cast<unsigned>(DefaultNtpPort),
                                         static_cast<unsigned>(DefaultPtpDomain),
                                         static_cast<unsigned>(DefaultPtpUdpDomain), static_cast<unsigned long>(DefaultPtpUdpWithdrawalMs),
                                         static_cast<unsigned>(DefaultPtpDomain), static_cast<unsigned>(DefaultPtpUdpDomain),
                                         static_cast<unsigned long>(DefaultPtpUdpWithdrawalMs));
        if (length < 0 || static_cast<size_t>(length) >= PageBufferSize)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Page generation failed");
        httpd_resp_set_type(request, "text/html");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_send(request, page.get(), HTTPD_RESP_USE_STRLEN);
    }

    bool form_value(const char *body, const char *key, char *output, size_t output_size)
    {
        bool found = false;
        const size_t key_size = std::strlen(key);
        for (const char *entry = body; *entry;)
        {
            const char *end = std::strchr(entry, '&');
            if (end == nullptr)
                end = entry + std::strlen(entry);
            const char *equals = static_cast<const char *>(std::memchr(entry, '=', end - entry));
            if (equals != nullptr && static_cast<size_t>(equals - entry) == key_size &&
                std::memcmp(entry, key, key_size) == 0)
            {
                if (found)
                    return false;
                found = true;
                size_t cursor = 0;
                for (const char *read = equals + 1; read < end; ++read)
                {
                    unsigned char value = static_cast<unsigned char>(*read);
                    if (value == '+')
                        value = ' ';
                    else if (value == '%')
                    {
                        if (end - read < 3 || !std::isxdigit(static_cast<unsigned char>(read[1])) ||
                            !std::isxdigit(static_cast<unsigned char>(read[2])))
                            return false;
                        const auto hex_value = [](char digit)
                        {
                            return std::isdigit(static_cast<unsigned char>(digit)) ? digit - '0' : std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10;
                        };
                        value = static_cast<unsigned char>((hex_value(read[1]) << 4) | hex_value(read[2]));
                        read += 2;
                    }
                    if (value == 0 || cursor + 1 >= output_size)
                        return false;
                    output[cursor++] = static_cast<char>(value);
                }
                output[cursor] = '\0';
            }
            entry = *end ? end + 1 : end;
        }
        return found;
    }

    bool form_target_value(const char *body, char *output, size_t output_size)
    {
        char raw[TargetNameSize + 64]{};
        if (!form_value(body, "target", raw, sizeof(raw)))
            return false;
        const char *start = raw;
        while (*start == ' ' || (*start >= '\t' && *start <= '\r'))
            ++start;
        size_t length = std::strlen(start);
        while (length > 0 && (start[length - 1] == ' ' || (start[length - 1] >= '\t' && start[length - 1] <= '\r')))
            --length;
        if (length >= output_size)
            return false;
        std::memcpy(output, start, length);
        output[length] = '\0';
        return true;
    }

    bool optional_u16(const char *text, uint16_t *value)
    {
        *value = 0;
        if (*text == '\0')
            return true;
        uint32_t parsed = 0;
        for (const char *digit = text; *digit; ++digit)
        {
            if (*digit < '0' || *digit > '9')
                return false;
            parsed = parsed * 10 + static_cast<unsigned>(*digit - '0');
            if (parsed > UINT16_MAX)
                return false;
        }
        if (parsed == 0)
            return false;
        *value = static_cast<uint16_t>(parsed);
        return true;
    }

    bool required_u32(const char *text, uint32_t minimum, uint32_t maximum, uint32_t &value)
    {
        if (*text == '\0')
            return false;
        uint32_t parsed = 0;
        for (const char *digit = text; *digit; ++digit)
        {
            if (*digit < '0' || *digit > '9' || parsed > (maximum - static_cast<unsigned>(*digit - '0')) / 10)
                return false;
            parsed = parsed * 10 + static_cast<unsigned>(*digit - '0');
        }
        if (parsed < minimum || parsed > maximum)
            return false;
        value = parsed;
        return true;
    }

    bool read_form_body(httpd_req_t *request, char *body, size_t size)
    {
        if (request->content_len <= 0 || static_cast<size_t>(request->content_len) >= size)
            return false;
        size_t cursor = 0;
        while (cursor < static_cast<size_t>(request->content_len))
        {
            const int received = httpd_req_recv(request, body + cursor, request->content_len - cursor);
            if (received <= 0)
                return false;
            cursor += static_cast<size_t>(received);
        }
        body[cursor] = '\0';
        return std::memchr(body, '\0', cursor) == nullptr;
    }

    esp_err_t physical_leds_handler(httpd_req_t *request)
    {
        if (!WS2812_RGB_enabled)
            return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Physical LED control is disabled in this firmware");
        char body[32]{};
        char value[2]{};
        if (!read_form_body(request, body, sizeof(body)) ||
            !form_value(body, "enabled", value, sizeof(value)) ||
            (std::strcmp(value, "0") != 0 && std::strcmp(value, "1") != 0))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Expected enabled=0 or enabled=1");
        const bool enabled = value[0] == '1';
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        esp_err_t result = ESP_OK;
        if (enabled != s_physical_leds_on)
        {
            nvs_handle_t nvs;
            result = nvs_open(NvsNamespace, NVS_READWRITE, &nvs);
            if (result == ESP_OK)
            {
                result = nvs_set_u8(nvs, PhysicalLedsNvsKey, enabled ? 1 : 0);
                if (result == ESP_OK)
                    result = nvs_commit(nvs);
                nvs_close(nvs);
            }
            if (result == ESP_OK)
                s_physical_leds_on = enabled;
        }
        const bool saved = result == ESP_OK;
        if (saved)
            result = update_status_leds_locked();
        xSemaphoreGive(s_state_mutex);
        if (result != ESP_OK)
        {
            ESP_LOGE(Tag, "Physical LED setting %s failed: %s",
                     saved ? "application" : "persistence", esp_err_to_name(result));
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       saved ? "LED preference saved, but physical LEDs could not be updated"
                                             : "Unable to save the physical LED preference");
        }
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_sendstr(request, enabled ? "{\"physical_leds_available\":true,\"physical_leds_on\":true}"
                                                   : "{\"physical_leds_available\":true,\"physical_leds_on\":false}");
    }

    uint64_t next_stored_timestamp()
    {
        uint64_t newest = 0;
        for (size_t index = 0; index < s_target_history.count; ++index)
            newest = std::max(newest, s_target_history.entries[index].last_stored);
        if (newest >= std::numeric_limits<uint64_t>::max() - 1)
        {
            std::sort(s_target_history.entries.begin(), s_target_history.entries.begin() + s_target_history.count,
                      [](const TargetHistoryEntry &left, const TargetHistoryEntry &right)
                      { return left.last_stored < right.last_stored; });
            for (size_t index = 0; index < s_target_history.count; ++index)
                s_target_history.entries[index].last_stored = index + 1;
            newest = s_target_history.count;
        }
        return newest + 1;
    }

    esp_err_t apply_settings(httpd_req_t *request)
    {
        char body[1024]{};
        if (!read_form_body(request, body, sizeof(body)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid settings body");
        TargetHistoryEntry profile{};
        char ipv4_port[6]{};
        char ipv6_port[6]{};
        char key_id[6]{};
        char ptp_key_id[6]{};
        char stress_tests[2]{};
        char client_time[24]{};
        char ptp_domain[4]{};
        char ptp_udp_domain[4]{};
        char ptp_udp_withdrawal[6]{};
        char firmware_mode[24]{};
        if (!form_value(body, "firmware_mode", firmware_mode, sizeof(firmware_mode)) ||
            !parse_firmware_mode(firmware_mode, profile.firmware_mode))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Unknown firmware mode value");
        if (!form_value(body, "ptp_mac", profile.ptp_mac, sizeof(profile.ptp_mac)) || !valid_ptp_mac(profile.ptp_mac) ||
            !form_value(body, "ptp_domain", ptp_domain, sizeof(ptp_domain)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Enter a unicast master MAC (not this device), or leave it blank.");
        char *domain_end = nullptr;
        const unsigned long domain = std::strtoul(ptp_domain, &domain_end, 10);
        if (*ptp_domain == '\0' || *domain_end != '\0' || domain > UINT8_MAX ||
            !std::all_of(ptp_domain, domain_end, [](char digit)
                         { return digit >= '0' && digit <= '9'; }))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "PTP domain must be between 0 and 255.");
        profile.ptp_domain = static_cast<uint8_t>(domain);
        uint32_t udp_domain = 0;
        if (!form_value(body, "ptp_udp_ipv4", profile.ptp_udp_ipv4, sizeof(profile.ptp_udp_ipv4)) ||
            !form_value(body, "ptp_udp_ipv6", profile.ptp_udp_ipv6, sizeof(profile.ptp_udp_ipv6)) ||
            !valid_ptp_udp_target(profile.ptp_udp_ipv4, AF_INET) ||
            !valid_ptp_udp_target(profile.ptp_udp_ipv6, AF_INET6))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "PTP L3 UDP master addresses must be unicast addresses other than this tester, or blank.");
        if (!form_value(body, "ptp_udp_domain", ptp_udp_domain, sizeof(ptp_udp_domain)) ||
            !required_u32(ptp_udp_domain, 0, UINT8_MAX, udp_domain))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "PTP L3 UDP domain must be between 0 and 255.");
        profile.ptp_udp_domain = static_cast<uint8_t>(udp_domain);
        if (!form_value(body, "ptp_udp_withdrawal", ptp_udp_withdrawal, sizeof(ptp_udp_withdrawal)) ||
            !required_u32(ptp_udp_withdrawal, 1000, 60000, profile.ptp_udp_withdrawal_ms))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "PTP L3 UDP withdrawal must be between 1000 and 60000 ms.");
        if (!form_target_value(body, profile.target, sizeof(profile.target)) || !valid_target_name(profile.target))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Target name must be 1 to 40 printable ASCII characters.");
        if (!form_value(body, "ipv4", profile.ipv4, sizeof(profile.ipv4)) ||
            !form_value(body, "ipv6", profile.ipv6, sizeof(profile.ipv6)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target addresses");
        if (!valid_address(profile.ipv4, AF_INET) || !valid_address(profile.ipv6, AF_INET6))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Enter literal IPv4 and IPv6 addresses or leave them blank.");
        if (is_self_target(profile.ipv4) || is_self_target(profile.ipv6))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "You must enter the target address of the server, not this program.");
        if (!form_value(body, "ipv4_ntp_port", ipv4_port, sizeof(ipv4_port)) ||
            !optional_u16(ipv4_port, &profile.ipv4_ntp_port) ||
            !form_value(body, "ipv6_ntp_port", ipv6_port, sizeof(ipv6_port)) ||
            !optional_u16(ipv6_port, &profile.ipv6_ntp_port))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "NTP ports must be blank or between 1 and 65535.");
        if (!form_value(body, "key_id", key_id, sizeof(key_id)) || !optional_u16(key_id, &profile.key.key_id))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Symmetric-key Id must be blank or between 1 and 65535.");
        if (!form_value(body, "key_value", profile.key.key_value, sizeof(profile.key.key_value)) ||
            !valid_key_value(profile.key.key_value))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Symmetric-key value must be blank or exactly 32 alphanumeric characters.");
        if (!form_value(body, "ptp_key_id", ptp_key_id, sizeof(ptp_key_id)) || !optional_u16(ptp_key_id, &profile.ptp_key.key_id))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "PTP symmetric-key Id must be blank or between 1 and 65535.");
        if (!form_value(body, "ptp_key_value", profile.ptp_key.key_value, sizeof(profile.ptp_key.key_value)) ||
            !valid_key_value(profile.ptp_key.key_value))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "PTP symmetric-key value must be blank or exactly 32 alphanumeric characters.");
        if (std::strstr(body, "stress_tests=") != nullptr)
        {
            if (!form_value(body, "stress_tests", stress_tests, sizeof(stress_tests)) || std::strcmp(stress_tests, "1") != 0)
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid stress test setting");
            profile.stress_tests = true;
        }
        if (!form_value(body, "client_time", client_time, sizeof(client_time)) || timestamp_sort_value(client_time) == 0)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid client date and time");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot change settings while tests are running.");
        }
        profile.last_stored = next_stored_timestamp();
        add_or_update_history_entry(profile);
        s_profile = profile;
        for (auto &result : s_results)
            result = {};
        save_settings();
        xSemaphoreGive(s_state_mutex);
        httpd_resp_set_status(request, "303 See Other");
        httpd_resp_set_hdr(request, "Location", "/");
        return httpd_resp_send(request, nullptr, 0);
    }

    esp_err_t settings_handler(httpd_req_t *request)
    {
        return apply_settings(request);
    }

    esp_err_t copy_or_rename_target(httpd_req_t *request, bool rename)
    {
        char body[256]{};
        if (!read_form_body(request, body, sizeof(body)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target body");
        char target[TargetNameSize]{};
        char new_target[TargetNameSize]{};
        char client_time[24]{};
        if (!form_target_value(body, target, sizeof(target)) || !valid_target_name(target) ||
            !form_value(body, "new_target", new_target, sizeof(new_target)) || !valid_target_name(new_target))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target name");
        if (!form_value(body, "client_time", client_time, sizeof(client_time)) || timestamp_sort_value(client_time) == 0)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid client date and time");
        if (!rename && (std::strlen(target) + sizeof(" - Copy") > sizeof(new_target) ||
                        std::strcmp(new_target + std::strlen(target), " - Copy") != 0 ||
                        std::strncmp(new_target, target, std::strlen(target)) != 0))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid cloned target name");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load() || std::strcmp(s_profile.target, target) != 0)
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Target changed or tests are running. Reload the page.");
        }
        TargetHistoryEntry *source = find_history_entry(target);
        if (source == nullptr)
        {
            xSemaphoreGive(s_state_mutex);
            return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Target not found");
        }
        if (find_history_entry(new_target) != nullptr)
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "A target with that name already exists.");
        }
        TargetHistoryEntry updated = *source;
        std::snprintf(updated.target, sizeof(updated.target), "%s", new_target);
        updated.last_stored = next_stored_timestamp();
        if (rename)
        {
            source = find_history_entry(target);
            remove_history_entry(static_cast<size_t>(source - s_target_history.entries.data()));
        }
        add_or_update_history_entry(updated);
        s_profile = updated;
        for (auto &result : s_results)
            result = {};
        save_settings();
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, rename ? "renamed" : "cloned");
    }

    esp_err_t clone_target_handler(httpd_req_t *request)
    {
        return copy_or_rename_target(request, false);
    }

    esp_err_t rename_target_handler(httpd_req_t *request)
    {
        return copy_or_rename_target(request, true);
    }

    esp_err_t delete_target_handler(httpd_req_t *request)
    {
        char body[256]{};
        if (!read_form_body(request, body, sizeof(body)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target body");
        char target[TargetNameSize]{};
        char client_time[24]{};
        if (!form_target_value(body, target, sizeof(target)) || !valid_target_name(target))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target name");
        if (!form_value(body, "client_time", client_time, sizeof(client_time)) || timestamp_sort_value(client_time) == 0)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid client date and time");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot delete targets while tests are running.");
        }
        TargetHistoryEntry *entry = find_history_entry(target);
        if (entry == nullptr)
        {
            xSemaphoreGive(s_state_mutex);
            return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Target not found");
        }
        const size_t index = static_cast<size_t>(entry - s_target_history.entries.data());
        const bool deleting_current = std::strcmp(s_profile.target, target) == 0;
        remove_history_entry(index);
        if (deleting_current)
        {
            s_profile = s_target_history.count == 0 ? TargetHistoryEntry{} : s_target_history.entries[0];
            for (auto &result : s_results)
                result = {};
        }
        save_settings();
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "deleted");
    }

    esp_err_t cancel_settings_handler(httpd_req_t *request)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot reload settings while tests are running.");
        }
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "reloaded");
    }

    esp_err_t send_escaped(httpd_req_t *request, const char *text, bool json)
    {
        char buffer[1024];
        size_t cursor = 0;
        for (const unsigned char *character = reinterpret_cast<const unsigned char *>(text); *character; ++character)
        {
            char escaped[8] = {};
            const char *replacement = nullptr;
            if (json)
            {
                if (*character == '"')
                    replacement = "\\\"";
                else if (*character == '\\')
                    replacement = "\\\\";
                else if (*character < 32 || *character >= 127)
                {
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(*character));
                    replacement = escaped;
                }
            }
            else
            {
                if (*character == '&')
                    replacement = "&amp;";
                else if (*character == '<')
                    replacement = "&lt;";
                else if (*character == '>')
                    replacement = "&gt;";
                else if (*character == '"')
                    replacement = "&quot;";
                else if (*character == '|')
                    replacement = "&#124;";
            }
            if (cursor + 8 >= sizeof(buffer))
            {
                if (httpd_resp_send_chunk(request, buffer, cursor) != ESP_OK)
                    return ESP_FAIL;
                cursor = 0;
            }
            if (replacement)
            {
                const size_t length = std::strlen(replacement);
                std::memcpy(buffer + cursor, replacement, length);
                cursor += length;
            }
            else
                buffer[cursor++] = static_cast<char>(*character);
        }
        return cursor ? httpd_resp_send_chunk(request, buffer, cursor) : ESP_OK;
    }

    esp_err_t status_handler(httpd_req_t *request)
    {
        ResponseBuffer response = allocate_response_buffer(StatusResponseBufferSize);
        ResponseBuffer detail = allocate_response_buffer(ResultDetailSize + 512);
        if (!response || !detail)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate status");
        char progress[sizeof(s_progress)];
        char prompt[sizeof(s_phase_prompt)];
        char policy[1024];
        char preflight[1280];
        ptp_policy_text(policy, sizeof(policy));
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const char *ipv4_address = s_startup_ipv4[0] ? s_startup_ipv4 : "Not assigned";
        const char *ipv6_lla = s_startup_ipv6_linklocal[0] ? s_startup_ipv6_linklocal : "Not assigned";
        const char *ipv6_ula = s_ipv6_ula[0] ? s_ipv6_ula : "Not assigned";
        const char *ipv6_gua = s_ipv6_gua[0] ? s_ipv6_gua : "Not assigned";
        if (!s_ethernet_connected.load())
        {
            ipv4_address = s_ipv4_was_assigned ? "Connection lost" : ipv4_address;
            ipv6_lla = s_ipv6_lla_was_assigned ? "Connection lost" : ipv6_lla;
            ipv6_ula = s_ipv6_ula_was_assigned ? "Connection lost" : ipv6_ula;
            ipv6_gua = s_ipv6_gua_was_assigned ? "Connection lost" : ipv6_gua;
        }
        const NetworkInfo network = read_network_info();
        const auto firmware_mode = s_profile.firmware_mode;
        const bool udp_ipv4_configured = s_profile.ptp_udp_ipv4[0] != '\0';
        const bool udp_ipv6_configured = s_profile.ptp_udp_ipv6[0] != '\0';
        const bool same_observation = s_profile.ptp_mac[0] && std::strcmp(s_profile.ptp_mac, s_observed_ptp_mac) == 0 &&
                                      s_profile.ptp_domain == s_observed_ptp_domain;
        std::snprintf(preflight, sizeof(preflight),
                      "Selected target: PTP L2 MAC=%s domain=%u; PTP L3 UDP IPv4=%s IPv6=%s domain=%u; "
                      "NTP IPv4=%s IPv6=%s. %s Last observed L2: %s "
                      "L2 tests do not require IP; the tester never silently substitutes a visible master.",
                      s_profile.ptp_mac[0] ? s_profile.ptp_mac : "(not selected)", s_profile.ptp_domain,
                      s_profile.ptp_udp_ipv4[0] ? s_profile.ptp_udp_ipv4 : "(disabled)",
                      s_profile.ptp_udp_ipv6[0] ? s_profile.ptp_udp_ipv6 : "(disabled)", s_profile.ptp_udp_domain,
                      s_profile.ipv4[0] ? s_profile.ipv4 : "(none)", s_profile.ipv6[0] ? s_profile.ipv6 : "(none)",
                      ptp_tester::policy_error(PtpDutPolicy) == nullptr && s_profile.ptp_domain != PtpDutPolicy.domain
                          ? "WARNING: selected domain conflicts with DUT policy."
                          : "",
                      same_observation ? s_observed_ptp_identity : "unavailable for this selected MAC/domain");
        const int written = std::snprintf(response.get(), StatusResponseBufferSize,
                                          "{\"running\":%s,\"target_valid\":%s,\"ready\":%s,\"link\":%s,\"ipv4\":%s,\"ipv6\":\"%s\","
                                          "\"physical_leds_available\":%s,\"physical_leds_on\":%s,\"physical_leds_flashing\":%s,\"physical_leds_flash_on\":%s,"
                                          "\"network\":{\"mac\":\"%s\",\"ipv4\":\"%s\",\"lla\":\"%s\",\"ula\":\"%s\",\"gua\":\"%s\"},"
                                          "\"active_test\":%d,\"phase_waiting\":%s,\"phase_token\":%lu,\"progress\":\"",
                                          s_tests_running.load() ? "true" : "false", valid_profile(s_profile) ? "true" : "false",
                                          s_startup_logged ? "true" : "false", s_ethernet_connected.load() ? "true" : "false",
                                          ipv4_available_locked() ? "true" : "false", ipv6_level_locked(),
                                          WS2812_RGB_enabled ? "true" : "false", s_physical_leds_on ? "true" : "false",
                                          WS2812_RGB_enabled && s_tests_running.load() && s_physical_leds_on && s_led_strip != nullptr ? "true" : "false",
                                          s_test_leds_flash_on ? "true" : "false", network.mac,
                                          ipv4_address, ipv6_lla, ipv6_ula, ipv6_gua,
                                          s_active_test < TestCount ? static_cast<int>(s_active_test) : -1,
                                          s_phase_waiting ? "true" : "false", static_cast<unsigned long>(s_phase_token));
        std::snprintf(progress, sizeof(progress), "%s", s_progress);
        std::snprintf(prompt, sizeof(prompt), "%s", s_phase_prompt);
        xSemaphoreGive(s_state_mutex);
        if (written < 0 || static_cast<size_t>(written) >= StatusResponseBufferSize)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Status generation failed");
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        if (httpd_resp_sendstr_chunk(request, response.get()) != ESP_OK || send_escaped(request, progress, true) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\",\"phase_prompt\":\"") != ESP_OK || send_escaped(request, prompt, true) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\",\"ptp_policy\":\"") != ESP_OK || send_escaped(request, policy, true) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\",\"ptp_preflight\":\"") != ESP_OK || send_escaped(request, preflight, true) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\",\"tests\":[") != ESP_OK)
            return ESP_FAIL;
        size_t position = 0;
        for (size_t index : TestOrder)
        {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            const auto status = s_results[index].status;
            std::snprintf(detail.get(), ResultDetailSize + 512, "%s", result_details(s_results[index]));
            xSemaphoreGive(s_state_mutex);
            std::snprintf(response.get(), StatusResponseBufferSize,
                          "%s{\"id\":%u,\"number\":%u,\"name\":\"%s\",\"enabled\":%s,\"family\":%u,\"ntp\":%s,\"ptp\":%s,\"ptpL2\":%s,\"ptpL3\":%s,\"compatible\":%s,\"ptpOnly\":%s,\"requiresCombined\":%s,\"assisted\":%s,\"status\":\"%s\",\"time\":\"",
                          position++ ? "," : "", static_cast<unsigned>(index), static_cast<unsigned>(TestNumbers[index]), Tests[index].name,
                          Tests[index].enabled && !disabled_ptp_setting(Tests[index]) &&
                                  configured_ptp_udp_test(Tests[index], udp_ipv4_configured, udp_ipv6_configured)
                              ? "true"
                              : "false",
                          Tests[index].family == AF_INET ? 4U : Tests[index].family == AF_INET6 ? 6U
                                                                                                : 0U,
                          Tests[index].kind != TestDefinition::Kind::Ping && !is_ptp_test(Tests[index]) ? "true" : "false",
                          is_ptp_test(Tests[index]) ? "true" : "false",
                          is_ptp_l2_test(Tests[index]) ? "true" : "false",
                          is_ptp_udp_test(Tests[index]) ? "true" : "false",
                          compatible_with_firmware_mode(Tests[index], firmware_mode) ? "true" : "false",
                          Tests[index].kind == TestDefinition::Kind::PtpOnly ? "true" : "false",
                          test_involves_ntp(Tests[index]) ? "true" : "false",
                          test_requires_operator(Tests[index]) ? "true" : "false",
                          status_text(status));
            if (httpd_resp_sendstr_chunk(request, response.get()) != ESP_OK || send_escaped(request, detail.get(), true) != ESP_OK ||
                httpd_resp_sendstr_chunk(request, "\"}") != ESP_OK)
                return ESP_FAIL;
        }
        if (httpd_resp_sendstr_chunk(request, "]}") != ESP_OK)
            return ESP_FAIL;
        return httpd_resp_send_chunk(request, nullptr, 0);
    }

    esp_err_t favicon_handler(httpd_req_t *request)
    {
        httpd_resp_set_type(request, "image/x-icon");
        return httpd_resp_send(request, reinterpret_cast<const char *>(favicon_ico_start),
                               favicon_ico_end - favicon_ico_start);
    }

    esp_err_t clear_results_handler(httpd_req_t *request)
    {
        if (s_tests_running.load())
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot clear test results while tests are running.");
        }

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        for (auto &result : s_results)
            result = {};
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "cleared");
    }

    esp_err_t stop_tests_handler(httpd_req_t *request)
    {
        if (!s_tests_running.load())
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "No tests are running.");
        }

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        s_stop_run_requested.store(true);
        s_stop_tests_requested.store(true);
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "stopping");
    }

    esp_err_t respond_to_phase(httpd_req_t *request, bool skip)
    {
        char body[64]{}, value[16]{};
        if (!read_form_body(request, body, sizeof(body)) || !form_value(body, "token", value, sizeof(value)) || !value[0])
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Missing phase token");
        for (const char *digit = value; *digit; ++digit)
            if (*digit < '0' || *digit > '9')
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid phase token");
        errno = 0;
        char *end = nullptr;
        const unsigned long token = std::strtoul(value, &end, 10);
        if (errno == ERANGE || *end || token == 0 || token > std::numeric_limits<uint32_t>::max())
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid phase token");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const bool accepted = s_tests_running.load() && !s_stop_tests_requested.load() && s_phase_waiting &&
                              !s_phase_acknowledged && !s_phase_skipped && s_ethernet_connected.load() &&
                              token == s_phase_token && esp_timer_get_time() < s_phase_deadline_us;
        if (accepted)
        {
            s_phase_acknowledged = !skip;
            s_phase_skipped = skip;
            s_phase_waiting = false;
            if (skip)
                s_stop_tests_requested.store(true);
            if (s_active_test < TestCount)
                s_results[s_active_test].status = skip ? TestStatus::Skipped : TestStatus::Running;
        }
        xSemaphoreGive(s_state_mutex);
        if (!accepted)
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Phase expired, cancelled, already answered or no longer current.");
        }
        return httpd_resp_sendstr(request, skip ? "skipped" : "acknowledged");
    }

    esp_err_t acknowledge_phase_handler(httpd_req_t *request)
    {
        return respond_to_phase(request, false);
    }

    esp_err_t skip_phase_handler(httpd_req_t *request)
    {
        return respond_to_phase(request, true);
    }

    esp_err_t target_safety_handler(httpd_req_t *request)
    {
        return httpd_resp_sendstr(request, target_requires_external_warning() ? "external" : "internal");
    }

    esp_err_t run_handler(httpd_req_t *request)
    {
        char scope[24]{};
        RunMetadata metadata{};
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        metadata.mode = s_profile.firmware_mode;
        xSemaphoreGive(s_state_mutex);
        if (request->method == HTTP_POST)
        {
            char body[512]{};
            char mode[24]{};
            if (!read_form_body(request, body, sizeof(body)) || !form_value(body, "scope", scope, sizeof(scope)) ||
                !form_value(body, "firmware_mode", mode, sizeof(mode)))
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid run metadata");
            if (!parse_firmware_mode(mode, metadata.mode))
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Unknown firmware mode value");
        }
        else
        {
            char query[40]{};
            if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
                httpd_query_key_value(query, "scope", scope, sizeof(scope)) != ESP_OK)
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Missing test scope");
        }
        bool started = false;
        if (std::strcmp(scope, "all") == 0)
            started = start_tests(TestRequest::Scope::All, metadata);
        else if (std::strcmp(scope, "ping") == 0)
            started = start_tests(TestRequest::Scope::Ping, metadata);
        else if (std::strcmp(scope, "ntp") == 0)
            started = start_tests(TestRequest::Scope::Ntp, metadata);
        else if (std::strcmp(scope, "ipv4") == 0)
            started = start_tests(TestRequest::Scope::NtpIpv4, metadata);
        else if (std::strcmp(scope, "ipv6") == 0)
            started = start_tests(TestRequest::Scope::NtpIpv6, metadata);
        else if (std::strcmp(scope, "ptp") == 0)
            started = start_tests(TestRequest::Scope::Ptp, metadata);
        else if (std::strcmp(scope, "ptp-l2") == 0)
            started = start_tests(TestRequest::Scope::PtpL2, metadata);
        else if (std::strcmp(scope, "ptp-auto") == 0)
            started = start_tests(TestRequest::Scope::PtpUnattended, metadata);
        else if (std::strcmp(scope, "ptp-assist") == 0)
            started = start_tests(TestRequest::Scope::PtpAssisted, metadata);
        else if (std::strcmp(scope, "ptp-l3") == 0)
            started = start_tests(TestRequest::Scope::PtpUdp, metadata);
        else if (std::strcmp(scope, "ptp-l3-auto") == 0)
            started = start_tests(TestRequest::Scope::PtpUdpUnattended, metadata);
        else if (std::strcmp(scope, "ptp-l3-assist") == 0)
            started = start_tests(TestRequest::Scope::PtpUdpAssisted, metadata);
        else if (std::strncmp(scope, "test-", 5) == 0)
        {
            char *end = nullptr;
            const long index = std::strtol(scope + 5, &end, 10);
            if (end != scope + 5 && *end == '\0' && index >= 0 && index < static_cast<long>(Tests.size()))
                started = start_tests(TestRequest::Scope::Individual, metadata, static_cast<int>(index));
        }
        if (!started)
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Tests or a firmware update are already running, tests are unavailable, or firmware mode differs from saved settings. Reload and save settings before running.");
        }
        return httpd_resp_sendstr(request, "started");
    }

    esp_err_t report_handler(httpd_req_t *request)
    {
        ResponseBuffer report = allocate_response_buffer(ReportBufferSize);
        ResponseBuffer detail = allocate_response_buffer(ResultDetailSize + 512);
        if (!report || !detail)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate report");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        char target[TargetNameSize * 12];
        markdown_escape(s_profile.target, target, sizeof(target));
        char ipv4_port[6], ipv6_port[6], key_id[6];
        format_optional_u16(s_profile.ipv4_ntp_port, ipv4_port);
        format_optional_u16(s_profile.ipv6_ntp_port, ipv6_port);
        format_optional_u16(s_profile.key.key_id, key_id);
        size_t cursor = std::snprintf(report.get(), ReportBufferSize,
                                      "<p align=\"center\"><strong>ESP32TimeServerTester Report</strong></p>\n\n"
                                      "## Settings\n\n"
                                      "| Settings | Values |\n"
                                      "| --- | --- |\n"
                                      "| **Target time server:** | %s |\n"
                                      "| **Firmware mode:** | %s |\n"
                                      "| **IPv4 address:** | %s |\n"
                                      "| **IPv4 NTP Port:** | %s |\n"
                                      "| **IPv6 address:** | %s |\n"
                                      "| **IPv6 NTP Port:** | %s |\n"
                                      "| **PTP L2 master MAC:** | %s |\n"
                                      "| **PTP L2 domain:** | %u |\n"
                                      "| **PTP L2 symmetric-key id:** | %s |\n"
                                      "| **PTP L2 symmetric-key value:** | %s |\n"
                                      "| **PTP L3 UDP IPv4 master:** | %s |\n"
                                      "| **PTP L3 UDP IPv6 master:** | %s |\n"
                                      "| **PTP L3 UDP domain:** | %u |\n"
                                      "| **PTP L3 UDP withdrawal bound:** | %lu ms |\n"
                                      "| **PTP L3 UDP multicast:** | IPv4 224.0.1.129; IPv6 ff02::181; event/general UDP ports 319/320; no PTP authentication |\n"
                                      "| **NTPv4 symmetric-key id:** | %s |\n"
                                      "| **NTPv4 symmetric-key value:** | %s |\n"
                                      "| **Mini saturated stress tests:** | %s (%d NTP requests each) |\n\n"
                                      "## Test results\n\n"
                                      "| No. | Test | Result | Reported date/time or PTP diagnostic |\n"
                                      "| ---: | --- | --- | --- |\n",
                                      target, firmware_mode_label(s_profile.firmware_mode),
                                      s_profile.ipv4[0] ? s_profile.ipv4 : "(blank)",
                                      ipv4_port[0] ? ipv4_port : "(blank)",
                                      s_profile.ipv6[0] ? s_profile.ipv6 : "(blank)",
                                      ipv6_port[0] ? ipv6_port : "(blank)",
                                      s_profile.ptp_mac[0] ? s_profile.ptp_mac : "(blank)", static_cast<unsigned>(s_profile.ptp_domain),
                                      s_profile.ptp_key.key_id ? "Present but redacted in this report" : "Not entered",
                                      s_profile.ptp_key.key_value[0] ? "Present but redacted in this report" : "Not entered",
                                      s_profile.ptp_udp_ipv4[0] ? s_profile.ptp_udp_ipv4 : "(disabled)",
                                      s_profile.ptp_udp_ipv6[0] ? s_profile.ptp_udp_ipv6 : "(disabled)",
                                      static_cast<unsigned>(s_profile.ptp_udp_domain),
                                      static_cast<unsigned long>(s_profile.ptp_udp_withdrawal_ms),
                                      key_id[0] ? "Present but redacted in this report" : "not entered",
                                      s_profile.key.key_value[0] ? "Present but redacted in this report" : "Not entered",
                                      s_profile.stress_tests ? "Enabled" : "Disabled",
                                      number_of_NTP_requests_in_a_mini_stress_test);
        for (size_t index : TestOrder)
        {
            if (cursor >= ReportBufferSize)
                break;
            const char *result = s_results[index].status == TestStatus::Pass
                                     ? "<span style=\"color:green\">Pass</span>"
                                 : s_results[index].status == TestStatus::Fail
                                     ? "<span style=\"color:red\">Fail</span>"
                                     : status_text(s_results[index].status);
            cursor += std::snprintf(report.get() + cursor, ReportBufferSize - cursor, "| %u | %s | %s | %s |\n",
                                    static_cast<unsigned>(TestNumbers[index]), Tests[index].name,
                                    result, s_results[index].timestamp);
        }
        const NetworkInfo network = read_network_info();
        xSemaphoreGive(s_state_mutex);
        if (cursor >= ReportBufferSize ||
            static_cast<size_t>(std::snprintf(report.get() + cursor, ReportBufferSize - cursor,
                                              "\n---\n\n"
                                              "%s v%s\n\n"
                                              "| Address Types | Addresses  |\n"
                                              "| --- | --- |\n"
                                              "| **MAC:** | %s |\n"
                                              "| **IPv4:** | %s |\n"
                                              "| **IPv6 LLA:** | %s |\n"
                                              "| **IPv6 ULA:** | %s |\n"
                                              "| **IPv6 GUA:** | %s |\n\n"
                                              "\n---\n\n"
                                              "https://github.com/roblatour/ESP32TimeServerTester\n",
                                              get_app_metadata()->project_name, get_app_metadata()->version,
                                              network.mac, network.ipv4, network.lla, network.ula, network.gua)) >=
                ReportBufferSize - cursor)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Report generation failed");
        httpd_resp_set_type(request, "text/markdown");

        char safe_target[TargetNameSize];
        size_t j = 0;

        // Format the original target name (remove unsafe characters etc.) for use in the filename.
        for (size_t i = 0; s_profile.target[i] != '\0' && j < sizeof(safe_target) - 1; i++)
        {
            char c = s_profile.target[i];

            if (isalnum((unsigned char)c) || c == '-' || c == '(' || c == ')')
            {
                safe_target[j++] = c;
            }
            else
            {
                safe_target[j++] = '_';
            }
        }

        safe_target[j] = '\0';

        char content_disposition[128];
        snprintf(content_disposition,
                 sizeof(content_disposition),
                 "attachment; filename=ESP32TimeServerTester_%s.md",
                 safe_target);

        httpd_resp_set_hdr(request,
                           "Content-Disposition",
                           content_disposition);

        if (httpd_resp_sendstr_chunk(request, report.get()) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\n## PTP interpretation limits\n\n") != ESP_OK ||
            send_escaped(request, PtpWarning, false) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\n\n## DUT policy metadata\n\n") != ESP_OK)
            return ESP_FAIL;
        char policy[1024];
        ptp_policy_text(policy, sizeof(policy));
        if (send_escaped(request, policy, false) != ESP_OK ||
            httpd_resp_sendstr_chunk(request, "\n\n## Detailed test evidence\n") != ESP_OK)
            return ESP_FAIL;
        for (size_t index : TestOrder)
        {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            const TestStatus status = s_results[index].status;
            std::snprintf(detail.get(), ResultDetailSize + 512, "%s", result_details(s_results[index]));
            xSemaphoreGive(s_state_mutex);
            std::snprintf(report.get(), ReportBufferSize, "\n### %u. %s\n\n%s\n\nResult: **%s**\n",
                          static_cast<unsigned>(TestNumbers[index]), Tests[index].name, Tests[index].description,
                          status_text(status));
            if (httpd_resp_sendstr_chunk(request, report.get()) != ESP_OK)
                return ESP_FAIL;
            if (status == TestStatus::CouldNotRun)
            {
                char *reason = could_not_run_reason(detail.get());
                char *reason_end = std::strpbrk(reason, "\r\n");
                const char saved_character = reason_end ? *reason_end : '\0';
                if (reason_end)
                    *reason_end = '\0';
                if (httpd_resp_sendstr_chunk(request, "Reason: ") != ESP_OK ||
                    send_escaped(request, reason, false) != ESP_OK ||
                    httpd_resp_sendstr_chunk(request, "\n") != ESP_OK)
                    return ESP_FAIL;
                if (reason_end)
                    *reason_end = saved_character;
            }
            if (httpd_resp_sendstr_chunk(request, "\n<pre>") != ESP_OK)
                return ESP_FAIL;
            const char *setting = disabled_ptp_setting(Tests[index]);
            if (setting && status != TestStatus::Pass && status != TestStatus::Fail && !std::strstr(detail.get(), setting))
            {
                std::snprintf(report.get(), ReportBufferSize,
                              "Test disabled: %s is set to false in the current firmware build. "
                              "Set %s to true in ESP32TimeServerTesterSettings.h and rebuild/flash the tester firmware to allow this test to run. "
                              "Other test prerequisites still apply.\n",
                              setting, setting);
                if (send_escaped(request, report.get(), false) != ESP_OK)
                    return ESP_FAIL;
            }
            if (is_ptp_l2_test(Tests[index]) && Tests[index].enabled && !setting && !PtpSequenceSeedEnabled &&
                httpd_resp_sendstr_chunk(request,
                                         "Controlled Delay_Req sequence-wrap coverage was not run: PtpSequenceSeedEnabled is set to false in the current firmware build. "
                                         "Set PtpSequenceSeedEnabled to true in ESP32TimeServerTesterSettings.h and rebuild/flash the tester firmware to enable controlled sequence seeding. "
                                         "This setting does not disable the test's other checks; natural sequence progression remains available and Sync wrap is not forced.\n") != ESP_OK)
                return ESP_FAIL;
            if (send_escaped(request, detail.get(), false) != ESP_OK ||
                httpd_resp_sendstr_chunk(request, "</pre>\n") != ESP_OK)
                return ESP_FAIL;
        }
        return httpd_resp_send_chunk(request, nullptr, 0);
    }

#if OTE_UPDATES_ENABLED
    bool begin_ote_update()
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const bool allowed = !s_tests_running.load() && !s_ote_in_progress;
        if (allowed)
        {
            s_ote_in_progress = true;
            update_status_leds_locked();
        }
        xSemaphoreGive(s_state_mutex);
        return allowed;
    }

    void ote_progress(unsigned int written, unsigned int total)
    {
        static unsigned int last_bucket = 0;
        const unsigned int percent = static_cast<unsigned int>(static_cast<uint64_t>(written) * 100 / total);
        if (written == 0 || percent / 10 != last_bucket)
            ESP_LOGI(Tag, "OTE update: %u%% (%u/%u bytes)", percent, written, total);
        last_bucket = percent / 10;
    }

    void ote_failure(const char *reason)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        s_ote_in_progress = false;
        update_status_leds_locked();
        xSemaphoreGive(s_state_mutex);
        ESP_LOGE(Tag, "OTE update failed; current firmware retained: %s", reason);
    }

    void ote_restart(void *)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if constexpr (WS2812_RGB_enabled)
        {
            if (s_led_strip != nullptr)
                write_status_leds_locked(StatusColor::Off, StatusColor::Off);
        }
        xSemaphoreGive(s_state_mutex);
        ESP_LOGI(Tag, "OTE restarting now; physical LEDs off, saved preference unchanged");
        esp_restart();
    }

    void ote_success()
    {
        ESP_LOGI(Tag, "OTE image validated; restarting in 5 seconds");
        ESP_ERROR_CHECK(esp_timer_start_once(s_ote_restart_timer, 5000000));
    }

    void start_ote_server()
    {
        esp_timer_create_args_t timer{};
        timer.callback = ote_restart;
        timer.name = "ote_restart";
        esp_err_t result = esp_timer_create(&timer, &s_ote_restart_timer);
        if (result != ESP_OK)
        {
            ESP_LOGE(Tag, "Unable to create OTE restart timer: %s", esp_err_to_name(result));
            return;
        }
        const native_ote_callbacks_t callbacks{begin_ote_update, ote_progress, ote_failure, ote_success};
        result = native_ote_start(s_eth_netif, callbacks);
        if (result != ESP_OK)
        {
            ESP_LOGE(Tag, "Unable to start OTE server: %s", esp_err_to_name(result));
            ESP_ERROR_CHECK(esp_timer_delete(s_ote_restart_timer));
            s_ote_restart_timer = nullptr;
        }
    }
#endif

    void start_web_server()
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 17;
        config.stack_size = 8192;
        httpd_handle_t server = nullptr;
        ESP_ERROR_CHECK(httpd_start(&server, &config));
        const httpd_uri_t routes[] = {
            {"/", HTTP_GET, root_handler, nullptr},
            {"/favicon.ico", HTTP_GET, favicon_handler, nullptr},
            {"/settings", HTTP_POST, settings_handler, nullptr},
            {"/settings/cancel", HTTP_POST, cancel_settings_handler, nullptr},
            {"/targets/clone", HTTP_POST, clone_target_handler, nullptr},
            {"/targets/rename", HTTP_POST, rename_target_handler, nullptr},
            {"/targets/delete", HTTP_POST, delete_target_handler, nullptr},
            {"/status", HTTP_GET, status_handler, nullptr},
            {"/physical-leds", HTTP_POST, physical_leds_handler, nullptr},
            {"/run", HTTP_GET, run_handler, nullptr},
            {"/run", HTTP_POST, run_handler, nullptr},
            {"/phase/ack", HTTP_POST, acknowledge_phase_handler, nullptr},
            {"/phase/skip", HTTP_POST, skip_phase_handler, nullptr},
            {"/stop", HTTP_GET, stop_tests_handler, nullptr},
            {"/clear", HTTP_GET, clear_results_handler, nullptr},
            {"/target-safety", HTTP_GET, target_safety_handler, nullptr},
            {"/report", HTTP_GET, report_handler, nullptr},
        };
        for (const auto &route : routes)
            ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    }

    void log_startup_if_ready()
    {
        if (s_startup_logged || !s_startup_ready || s_startup_ipv4[0] == '\0' ||
            s_startup_ipv6_linklocal[0] == '\0')
            return;

        NetworkInfo network = read_network_info();
        std::printf("\n");
        ESP_LOGI(Tag, "*********************************************************");
        ESP_LOGI(Tag, "%s v%s is open for business", get_app_metadata()->project_name, get_app_metadata()->version);
        ESP_LOGI(Tag, "Host name:               %s", Hostname);
        ESP_LOGI(Tag, "MAC address:             %s", network.mac);
        ESP_LOGI(Tag, "IPv4 address:            %s", s_startup_ipv4);
        ESP_LOGI(Tag, "Link-local IPv6 address: %s", s_startup_ipv6_linklocal);

        ESP_LOGI(Tag, "Unique-local IPv6 (ULA): %s", s_ipv6_ula[0] != '\0' ? s_ipv6_ula : "Not assigned");
        ESP_LOGI(Tag, "Global IPv6 (GUA):       %s", s_ipv6_gua[0] != '\0' ? s_ipv6_gua : "Not assigned");
        ESP_LOGI(Tag, "Browse to:               http://%s", Hostname);
        ESP_LOGI(Tag, "*********************************************************");
        std::printf("\n");

        s_startup_logged = true;
        update_status_leds_locked();
    }

    // Address in use: GUA, else ULA, else LLA. Caller holds s_state_mutex.
    void update_ipv6_in_use_locked()
    {
        update_status_leds_locked();
        const char *best = s_ipv6_gua[0] != '\0'   ? s_ipv6_gua
                           : s_ipv6_ula[0] != '\0' ? s_ipv6_ula
                                                   : s_startup_ipv6_linklocal;
        if (std::strcmp(best, s_ipv6_in_use) == 0)
            return;
        std::snprintf(s_ipv6_in_use, sizeof(s_ipv6_in_use), "%s", best);
        if (best[0] != '\0')
            ESP_LOGI(Tag, "IPv6 address now in use: %s", best);
    }

    // Caller holds s_state_mutex.
    void record_ipv6_locked(const esp_ip6_addr_t &ip)
    {
        const char *label = nullptr;
        char *destination = nullptr;
        switch (esp_netif_ip6_get_addr_type(&ip))
        {
        case ESP_IP6_ADDR_IS_LINK_LOCAL:
            label = "link-local (LLA)";
            destination = s_startup_ipv6_linklocal;
            break;
        case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
            label = "unique-local (ULA)";
            destination = s_ipv6_ula;
            break;
        case ESP_IP6_ADDR_IS_GLOBAL:
            label = "global (GUA)";
            destination = s_ipv6_gua;
            break;
        default:
            return;
        }
        char address[INET6_ADDRSTRLEN];
        if (inet6_ntoa_r(ip, address, sizeof(address)) == nullptr || std::strcmp(address, destination) == 0)
            return;
        std::snprintf(destination, INET6_ADDRSTRLEN, "%s", address);
        if (destination == s_startup_ipv6_linklocal)
            s_ipv6_lla_was_assigned = true;
        else if (destination == s_ipv6_ula)
            s_ipv6_ula_was_assigned = true;
        else if (destination == s_ipv6_gua)
            s_ipv6_gua_was_assigned = true;
        ESP_LOGI(Tag, "New IPv6 %s address: %s", label, address);
        update_ipv6_in_use_locked();
        log_startup_if_ready();
    }

    void network_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP)
        {
            const auto *event = static_cast<const ip_event_got_ip_t *>(event_data);
            if (event->esp_netif != s_eth_netif)
                return;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            std::snprintf(s_startup_ipv4, sizeof(s_startup_ipv4), IPSTR, IP2STR(&event->ip_info.ip));
            s_ipv4_was_assigned = true;
            if (s_startup_logged)
                ESP_LOGI(Tag, "Ethernet IPv4 address: %s", s_startup_ipv4);
            else
                log_startup_if_ready();
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
        if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6)
        {
            const auto *event = static_cast<const ip_event_got_ip6_t *>(event_data);
            if (event->esp_netif != s_eth_netif)
                return;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            record_ipv6_locked(event->ip6_info.ip);
            xSemaphoreGive(s_state_mutex);
        }
        if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_LOST_IP)
        {
            ESP_LOGW(Tag, "Ethernet IPv4 address lost");
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_startup_ipv4[0] = '\0';
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
    }

    // Polls the netif so a ULA/GUA that arrives after the link-local address is still picked up
    void refresh_ipv6_addresses()
    {
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        const int count = esp_netif_get_all_ip6(s_eth_netif, addresses);
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        bool has_lla = false, has_ula = false, has_gua = false;
        for (int index = 0; index < count; ++index)
        {
            switch (esp_netif_ip6_get_addr_type(&addresses[index]))
            {
            case ESP_IP6_ADDR_IS_LINK_LOCAL:
                has_lla = true;
                break;
            case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
                has_ula = true;
                break;
            case ESP_IP6_ADDR_IS_GLOBAL:
                has_gua = true;
                break;
            default:
                break;
            }
            record_ipv6_locked(addresses[index]);
        }
        const auto drop = [](char *address, bool present, const char *label)
        {
            if (present || address[0] == '\0')
                return;
            ESP_LOGW(Tag, "IPv6 %s address lost: %s", label, address);
            address[0] = '\0';
        };
        drop(s_startup_ipv6_linklocal, has_lla, "link-local (LLA)");
        drop(s_ipv6_ula, has_ula, "unique-local (ULA)");
        drop(s_ipv6_gua, has_gua, "global (GUA)");
        update_ipv6_in_use_locked();
        xSemaphoreGive(s_state_mutex);
    }

    void ethernet_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *)
    {
        if (event_base != ETH_EVENT)
            return;

        if (event_id == ETHERNET_EVENT_CONNECTED)
        {
            s_tester_link_generation.fetch_add(1);
            s_ethernet_connected.store(true);
            s_ethernet_link_known.store(true);
            ESP_LOGI(Tag, "Ethernet link connected");
            const esp_err_t error = esp_netif_create_ip6_linklocal(s_eth_netif);
            if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
                ESP_LOGE(Tag, "Unable to create Ethernet IPv6 link-local address: %s", esp_err_to_name(error));
            refresh_ipv6_addresses();
        }
        else if (event_id == ETHERNET_EVENT_DISCONNECTED)
        {
            s_tester_link_generation.fetch_add(1);
            s_ethernet_connected.store(false);
            s_ethernet_link_known.store(true);
            ESP_LOGW(Tag, "Ethernet link disconnected; waiting for automatic link recovery");
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_startup_ipv4[0] = '\0';
            s_startup_ipv6_linklocal[0] = '\0';
            s_ipv6_ula[0] = '\0';
            s_ipv6_gua[0] = '\0';
            s_ipv6_in_use[0] = '\0';
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
    }

    void ethernet_recovery_task(void *)
    {
        int missing_ipv6_checks = 0;
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            if (s_ethernet_connected.load())
            {
                esp_ip6_addr_t ipv6{};
                if (esp_netif_get_ip6_linklocal(s_eth_netif, &ipv6) == ESP_OK)
                {
                    missing_ipv6_checks = 0;
                    refresh_ipv6_addresses();
                    continue;
                }
                else
                {
                    ++missing_ipv6_checks;
                    if (esp_netif_is_netif_up(s_eth_netif))
                    {
                        const esp_err_t error = esp_netif_create_ip6_linklocal(s_eth_netif);
                        if (error != ESP_OK)
                            ESP_LOGW(Tag, "Unable to restore Ethernet IPv6 link-local address: %s", esp_err_to_name(error));
                    }
                }
                refresh_ipv6_addresses();
                if (missing_ipv6_checks < 3)
                    continue;
                ESP_LOGW(Tag, "Ethernet link-local IPv6 address missing; restarting Ethernet driver");
            }
            else
                ESP_LOGW(Tag, "Ethernet link is still down; restarting Ethernet driver");

            missing_ipv6_checks = 0;
            const esp_err_t stop_error = esp_eth_stop(s_eth_handle);
            if (stop_error != ESP_OK && stop_error != ESP_ERR_INVALID_STATE)
                ESP_LOGE(Tag, "Unable to stop Ethernet driver for recovery: %s", esp_err_to_name(stop_error));
            vTaskDelay(pdMS_TO_TICKS(250));
            const esp_err_t start_error = esp_eth_start(s_eth_handle);
            if (start_error != ESP_OK && start_error != ESP_ERR_INVALID_STATE)
                ESP_LOGE(Tag, "Unable to restart Ethernet driver: %s", esp_err_to_name(start_error));
        }
    }

    void start_ethernet()
    {
        if (MACAddress[0] != '\0')
        {
            uint8_t mac_address[6];
            if (!parse_mac_address(MACAddress, mac_address))
            {
                ESP_LOGE(Tag, "Configured MAC address is invalid: %s", MACAddress);
                return;
            }
            ESP_ERROR_CHECK(esp_base_mac_addr_set(mac_address));
            ESP_LOGI(Tag, "Using Ethernet MAC address %s", MACAddress);
        }

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
        s_eth_netif = esp_netif_new(&netif_config);
        if (s_eth_netif == nullptr)
            ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        if ((esp_netif_get_flags(s_eth_netif) & ESP_NETIF_FLAG_IPV6_AUTOCONFIG_ENABLED) == 0)
        {
            ESP_LOGE(Tag, "Ethernet IPv6 SLAAC autoconfiguration is disabled");
            return;
        }
        ESP_LOGI(Tag, "Ethernet IPv6 SLAAC autoconfiguration is enabled");
        ESP_ERROR_CHECK(esp_netif_set_hostname(s_eth_netif, Hostname));

        eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
        emac_config.smi_gpio.mdc_num = EthernetMdcGpio;
        emac_config.smi_gpio.mdio_num = EthernetMdioGpio;
        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        phy_config.phy_addr = EthernetPhyAddress;
        phy_config.reset_gpio_num = EthernetPhyResetGpio;
        esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_config);
        esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
        ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
        bool receive_all_multicast = true;
        ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_ALL_MULTICAST, &receive_all_multicast));
        ESP_LOGI(Tag, "Ethernet is accepting all multicast frames for IPv6 Router Advertisements");
        ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));
        if constexpr (PtpTestsEnabled)
        {
            const esp_err_t ptp_error = ptp_tester::initialize(s_eth_handle, s_eth_netif);
            if (ptp_error != ESP_OK)
                ESP_LOGE(Tag, "PTP tester unavailable: %s; existing IP tests remain enabled", esp_err_to_name(ptp_error));
        }
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &ethernet_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &ethernet_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &network_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &network_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &network_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
        if (xTaskCreate(ethernet_recovery_task, "eth_recovery", 3072, nullptr, 5, nullptr) != pdPASS)
            ESP_LOGE(Tag, "Unable to create Ethernet recovery task");
    }
} // namespace

extern "C" void app_main()
{

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(psa_crypto_init() == PSA_SUCCESS ? ESP_OK : ESP_FAIL);
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == nullptr)
    {
        ESP_LOGE(Tag, "Unable to create state mutex");
        return;
    }
    load_settings();
    load_physical_led_setting();
    init_status_leds();
    init_test_led_flash_timer();
    start_ethernet();
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(Hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Time Server Tester"));
    start_web_server();
#if OTE_UPDATES_ENABLED
    start_ote_server();
#endif
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_startup_ready = true;
    log_startup_if_ready();
    xSemaphoreGive(s_state_mutex);
}
