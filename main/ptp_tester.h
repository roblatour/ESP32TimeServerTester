// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#ifndef PTP_TESTER_HOST_TEST
#include "esp_eth.h"
#include "esp_netif.h"
#else
struct esp_netif_t;
#endif

namespace ptp_tester
{
    enum class Test : uint8_t
    {
        Announce,
        SyncFollowUp,
        DelayExchange,
        TimeProperties,
        Measurement,
        Sustained,
        NegativeDelay,
        ForeignAnnounce,
        PtpOnly,
        GnssRecovery,
        PpsRecovery,
        LinkRecovery,
        RestartRecovery,
        CombinedLoad,
        RankingPrecedence,
        SourceIsolation,
        SequencePolicy,
        DatasetChanges,
        MultipleMasters,
        IntervalExpiry,
        StandbyRecovery,
        SignedMessages,
        SignedExchange,
        InvalidAuthentication,
        AuthenticationPolicy,
        AuthenticationBeforeSelection,
        OptionalUnsignedService,
        OptionalSignedRejection,
        AnnounceAuthenticationPolicy,
        InvalidAuthenticationRefresh,
        ForeignCapacity,
        SelectionNtpLoad,
        StandbyPpsRecovery,
        StandbyLinkRecovery,
        StandbyRestartRecovery,
        KeyRotation,
        StartupListening,
        StandbyExpiryUnqualified,
        SelectionNtpHoldover,
        SelectionNtpPolicyLoad,
        Count,
        TimingRestore = 251,
        StartupRestore = 252,
        KeyRotationReady = 253,
        KeyRotationRestore = 254
    };

    enum class Result : uint8_t
    {
        Pass,
        Fail,
        CouldNotRun
    };

    enum class FirmwareMode : uint8_t
    {
        Unknown,
        NtpOnly,
        PtpOnly,
        Combined,
        PtpL3Only,
        NtpPtpL3,
        PtpL2PtpL3,
        NtpPtpL2PtpL3
    };

    constexpr bool mode_has_ntp(FirmwareMode mode)
    {
        return mode == FirmwareMode::NtpOnly || mode == FirmwareMode::Combined ||
               mode == FirmwareMode::NtpPtpL3 || mode == FirmwareMode::NtpPtpL2PtpL3;
    }

    constexpr bool mode_has_l2(FirmwareMode mode)
    {
        return mode == FirmwareMode::PtpOnly || mode == FirmwareMode::Combined ||
               mode == FirmwareMode::PtpL2PtpL3 || mode == FirmwareMode::NtpPtpL2PtpL3;
    }

    constexpr bool mode_has_l3(FirmwareMode mode)
    {
        return mode == FirmwareMode::PtpL3Only || mode == FirmwareMode::NtpPtpL3 ||
               mode == FirmwareMode::PtpL2PtpL3 || mode == FirmwareMode::NtpPtpL2PtpL3;
    }

    enum class UdpTest : uint8_t
    {
        Announce,
        SyncFollowUp,
        DelayExchange,
        TimeProperties,
        Sustained,
        NegativeDelay,
        Simultaneous,
        CombinedLoad,
        GnssRecovery,
        PpsRecovery,
        LinkRecovery,
        RestartRecovery
    };

    constexpr bool udp_requires_operator(UdpTest test)
    {
        return test >= UdpTest::GnssRecovery && test <= UdpTest::RestartRecovery;
    }

    constexpr bool udp_involves_ntp(UdpTest test)
    {
        return test == UdpTest::CombinedLoad;
    }

    constexpr bool udp_compatible_with_firmware_mode(UdpTest test, FirmwareMode mode)
    {
        if (udp_involves_ntp(test))
            return mode_has_ntp(mode) && mode_has_l3(mode);
        return mode == FirmwareMode::Unknown || mode_has_l3(mode);
    }

    enum class SelectionMode : uint8_t
    {
        Unknown,
        Legacy,
        Ranked
    };

    enum class AuthenticationMode : uint8_t
    {
        Unknown,
        Disabled,
        Mixed,
        Required
    };

    struct DutPolicy
    {
        uint8_t version = 0;
        SelectionMode selection = SelectionMode::Unknown;
        AuthenticationMode authentication = AuthenticationMode::Unknown;
        uint8_t capacity = 0;
        uint8_t domain = 0;
        int8_t log_announce_interval = 0;
        int8_t log_sync_interval = 0;
        uint32_t foreign_timeout_ms = 0;
        uint32_t qualification_timeout_ms = 0;
        uint8_t spp = 0;
        uint32_t outgoing_key_id = 0;
        bool ntp_authentication_required = false;
        const char *identity = nullptr;
        const char *provenance = nullptr;
    };

    const char *policy_error(const DutPolicy &policy);
    const char *selection_text(SelectionMode mode);
    const char *authentication_text(AuthenticationMode mode);
    const char *pending_reason(Test test);

    constexpr bool is_standby_recovery(Test test)
    {
        return test == Test::StandbyRecovery || test == Test::StandbyPpsRecovery ||
               test == Test::StandbyLinkRecovery || test == Test::StandbyRestartRecovery ||
               test == Test::StandbyExpiryUnqualified || test == Test::SelectionNtpHoldover;
    }

    constexpr bool requires_suppression_opt_in(Test test)
    {
        return test == Test::ForeignAnnounce || test == Test::RankingPrecedence ||
               test == Test::SourceIsolation || test == Test::SequencePolicy ||
               test == Test::DatasetChanges || test == Test::MultipleMasters ||
               test == Test::IntervalExpiry || test == Test::StandbyRecovery ||
               test == Test::AuthenticationBeforeSelection || test == Test::OptionalSignedRejection ||
               test == Test::AnnounceAuthenticationPolicy || test == Test::InvalidAuthenticationRefresh ||
               test == Test::ForeignCapacity || test == Test::SelectionNtpLoad ||
               test == Test::StandbyPpsRecovery || test == Test::StandbyLinkRecovery ||
               test == Test::StandbyRestartRecovery || test == Test::StandbyExpiryUnqualified ||
               test == Test::SelectionNtpHoldover || test == Test::SelectionNtpPolicyLoad;
    }

    constexpr bool requires_operator(Test test)
    {
        return (test >= Test::GnssRecovery && test <= Test::RestartRecovery) ||
               is_standby_recovery(test) || test == Test::KeyRotation || test == Test::StartupListening;
    }

    constexpr bool involves_ntp(Test test)
    {
        return test == Test::CombinedLoad || test == Test::SelectionNtpLoad ||
               test == Test::SelectionNtpHoldover || test == Test::SelectionNtpPolicyLoad;
    }

    constexpr bool compatible_with_firmware_mode(Test test, FirmwareMode mode)
    {
        if (test == Test::PtpOnly)
            return mode == FirmwareMode::PtpOnly;
        if (involves_ntp(test))
            return mode_has_ntp(mode) && mode_has_l2(mode);
        return mode == FirmwareMode::Unknown || mode_has_l2(mode);
    }

    constexpr size_t original_ordered_test_index(size_t position)
    {
        constexpr size_t original_count = static_cast<size_t>(Test::OptionalSignedRejection) + 1;
        size_t visited = 0;
        for (unsigned assisted = 0; assisted < 2; ++assisted)
            for (unsigned ptp_only = 0; ptp_only < 2; ++ptp_only)
                for (size_t index = 0; index < original_count; ++index)
                    if (requires_operator(static_cast<Test>(index)) == (assisted != 0) &&
                        involves_ntp(static_cast<Test>(index)) == (ptp_only == 0) && visited++ == position)
                        return index;
        return static_cast<size_t>(Test::Count);
    }

    constexpr size_t ordered_test_index(size_t position)
    {
        constexpr size_t original_count = static_cast<size_t>(Test::OptionalSignedRejection) + 1;
        constexpr size_t test_count = static_cast<size_t>(Test::Count);
        size_t original_unattended = 0;
        size_t appended_unattended = 0;
        for (size_t index = 0; index < original_count; ++index)
            original_unattended += !requires_operator(static_cast<Test>(index));
        for (size_t index = original_count; index < test_count; ++index)
            appended_unattended += !requires_operator(static_cast<Test>(index));

        if (position < original_unattended)
        {
            size_t visited = 0;
            for (size_t index = 0; index < original_count; ++index)
            {
                const size_t ordered = original_ordered_test_index(index);
                if (!requires_operator(static_cast<Test>(ordered)) && visited++ == position)
                    return ordered;
            }
            return test_count;
        }
        position -= original_unattended;

        if (position < appended_unattended)
        {
            size_t visited = 0;
            for (size_t index = original_count; index < test_count; ++index)
                if (!requires_operator(static_cast<Test>(index)) && visited++ == position)
                    return index;
            return test_count;
        }
        position -= appended_unattended;

        const size_t original_assisted = original_count - original_unattended;
        if (position < original_assisted)
        {
            size_t visited = 0;
            for (size_t index = 0; index < original_count; ++index)
            {
                const size_t ordered = original_ordered_test_index(index);
                if (requires_operator(static_cast<Test>(ordered)) && visited++ == position)
                    return ordered;
            }
            return test_count;
        }
        position -= original_assisted;

        size_t visited = 0;
        for (size_t index = original_count; index < test_count; ++index)
            if (requires_operator(static_cast<Test>(index)) && visited++ == position)
                return index;
        return test_count;
    }

    enum class PolicyPhase : uint8_t
    {
        Baseline,
        RejectedControl,
        SingleAnnounce,
        Qualification,
        Refresh,
        Expiry,
        Recovery,
        Cleanup
    };

    struct PhaseEvidence
    {
        PolicyPhase phase = PolicyPhase::Baseline;
        int64_t start_us = 0;
        int64_t deadline_us = 0;
        int64_t last_tx_us = 0;
        int64_t last_activity_us = 0;
        int64_t first_activity_us = 0;
        uint32_t sends = 0;
    };

    using AssistPoll = bool (*)(void *context);

    struct Config
    {
        DutPolicy policy{};
        uint32_t key_id = 0;
        const char *key_value = nullptr;
        uint32_t rotation_key_id = 0;
        const char *rotation_key_value = nullptr;
        uint8_t target_mac[6] = {};
        uint8_t domain = 0;
        uint32_t timeout_ms = 0;
        uint32_t observation_ms = 20000;
        uint16_t samples = 8;
        uint32_t response_window_ms = 500;
        uint32_t request_interval_ms = 250;
        uint32_t processing_allowance_ms = 250;
        uint32_t foreign_quiet_ms = 0;
        uint32_t withdrawal_ms = 0;
        uint32_t recovery_ms = 60000;
        FirmwareMode firmware_mode = FirmwareMode::Unknown;
        bool isolated_link = false;
        bool negative_enabled = false;
        bool suppression_enabled = false;
        bool assisted_enabled = false;
        bool rotation_enabled = false;
        bool combined_enabled = false;
        bool no_ip_confirmed = false;
        bool sequence_seed_enabled = false;
        uint16_t sequence_seed = 65532;
        bool (*assist)(Test test, bool restoration, const std::atomic<bool> &stop, void *context,
                       AssistPoll poll, void *poll_context) = nullptr;
        void (*progress)(const char *text, void *context) = nullptr;
        void (*target_identity)(const char *text, void *context) = nullptr;
        void (*observation)(bool starting, void *context) = nullptr;
        void *context = nullptr;
    };

    struct UdpConfig
    {
        const char *ipv4_target = nullptr;
        const char *ipv6_target = nullptr;
        bool ipv4 = false;
        bool ipv6 = false;
        uint8_t domain = 0;
        uint32_t timeout_ms = 0;
        uint32_t observation_ms = 0;
        uint16_t samples = 0;
        uint32_t response_window_ms = 0;
        uint32_t request_interval_ms = 0;
        uint32_t withdrawal_ms = 0;
        uint32_t recovery_ms = 0;
        esp_netif_t *netif = nullptr;
        const std::atomic<bool> *link_up = nullptr;
        const std::atomic<uint32_t> *link_generation = nullptr;
        bool (*assist)(Test test, bool restoration, const std::atomic<bool> &stop, void *context,
                       AssistPoll poll, void *poll_context) = nullptr;
        void (*progress)(const char *text, void *context) = nullptr;
        void (*observation)(bool starting, void *context) = nullptr;
        void *context = nullptr;
    };

#ifndef PTP_TESTER_HOST_TEST
    esp_err_t initialize(esp_eth_handle_t ethernet, esp_netif_t *netif);
    bool ready();
    Result run(Test test, const Config &config, const std::atomic<bool> &stop,
               char *output, size_t output_size);
    Result run_udp(UdpTest test, const UdpConfig &config, const std::atomic<bool> &stop,
                   char *output, size_t output_size);
#endif
}
