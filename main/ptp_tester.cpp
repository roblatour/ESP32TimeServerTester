// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#include "ptp_tester.h"
#include "ptp_auth.h"

#include <array>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#ifndef PTP_TESTER_HOST_TEST
#include <arpa/inet.h>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include "esp_eth_clock.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#endif

namespace ptp_tester
{
    const char *selection_text(SelectionMode mode)
    {
        switch (mode)
        {
        case SelectionMode::Legacy: return "legacy any-Announce suppression";
        case SelectionMode::Ranked: return "ranked two-fresh-Announce qualification";
        default: return "unknown";
        }
    }

    const char *authentication_text(AuthenticationMode mode)
    {
        switch (mode)
        {
        case AuthenticationMode::Disabled: return "disabled (unsigned)";
        case AuthenticationMode::Mixed: return "enabled, optional (signed with keys, unsigned without keys)";
        case AuthenticationMode::Required: return "enabled, required";
        default: return "unknown";
        }
    }

    const char *policy_error(const DutPolicy &policy)
    {
        if (policy.version != 1)
            return "missing/unsupported DUT policy version";
        if (policy.selection != SelectionMode::Legacy && policy.selection != SelectionMode::Ranked)
            return "unknown DUT selection policy";
        if (policy.authentication != AuthenticationMode::Disabled && policy.authentication != AuthenticationMode::Mixed &&
            policy.authentication != AuthenticationMode::Required)
            return "unknown DUT authentication policy";
        if (policy.capacity < 1 || policy.capacity > 32 || policy.foreign_timeout_ms < 1000 ||
            policy.foreign_timeout_ms > 120000 || policy.qualification_timeout_ms < 1 ||
            policy.qualification_timeout_ms > 120000 || policy.log_announce_interval < 0 ||
            policy.log_announce_interval > 4 || policy.log_sync_interval < 0 || policy.log_sync_interval > 4 ||
            policy.outgoing_key_id == 0)
            return "DUT policy fields outside supported bounds";
        if (policy.identity == nullptr || policy.identity[0] == '\0' ||
            policy.provenance == nullptr || policy.provenance[0] == '\0')
            return "DUT policy source/build identity or provenance missing";
        return nullptr;
    }

    const char *pending_reason(Test)
    {
        return nullptr;
    }

    namespace
    {
        constexpr size_t MaxMessageSize = 256;
        constexpr size_t MaxFrameSize = 14 + MaxMessageSize;
        constexpr uint8_t MulticastMac[6] = {0x01, 0x1b, 0x19, 0x00, 0x00, 0x00};
        constexpr uint16_t TwoStepFlag = 0x0200;
        constexpr uint16_t UtcValidFlag = 0x0004;
        constexpr uint16_t PtpTimescaleFlag = 0x0008;
        constexpr int64_t PairLifetimeUs = 5000000;

        uint16_t read16(const uint8_t *bytes)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
        }

        uint32_t mac_prefix(const uint8_t *bytes)
        {
            return (static_cast<uint32_t>(read16(bytes)) << 16) | read16(bytes + 2);
        }

        uint64_t read64(const uint8_t *bytes)
        {
            uint64_t value = 0;
            for (size_t index = 0; index < 8; ++index)
                value = (value << 8) | bytes[index];
            return value;
        }

        int64_t read_signed64(const uint8_t *bytes)
        {
            const uint64_t value = read64(bytes);
            if (value <= static_cast<uint64_t>(INT64_MAX))
                return static_cast<int64_t>(value);
            return -static_cast<int64_t>(~value) - 1;
        }

        void write16(uint8_t *bytes, uint16_t value)
        {
            bytes[0] = static_cast<uint8_t>(value >> 8);
            bytes[1] = static_cast<uint8_t>(value);
        }

        bool valid_identity(const uint8_t *identity)
        {
            bool all_zero = true;
            bool all_ones = true;
            for (size_t index = 0; index < 8; ++index)
            {
                all_zero = all_zero && identity[index] == 0;
                all_ones = all_ones && identity[index] == 0xff;
            }
            return !all_zero && !all_ones;
        }

        bool valid_port_identity(const uint8_t *identity)
        {
            const uint16_t port = read16(identity + 8);
            return valid_identity(identity) && port != 0 && port != 0xffff;
        }

        bool valid_target(const uint8_t *mac)
        {
            bool nonzero = false;
            for (size_t index = 0; index < 6; ++index)
                nonzero = nonzero || mac[index] != 0;
            return nonzero && (mac[0] & 1) == 0;
        }

        struct Timestamp
        {
            uint64_t seconds = 0;
            uint32_t nanoseconds = 0;
        };

        bool read_timestamp(const uint8_t *bytes, Timestamp &timestamp)
        {
            timestamp.seconds = 0;
            for (size_t index = 0; index < 6; ++index)
                timestamp.seconds = (timestamp.seconds << 8) | bytes[index];
            timestamp.nanoseconds = (static_cast<uint32_t>(bytes[6]) << 24) |
                                    (static_cast<uint32_t>(bytes[7]) << 16) |
                                    (static_cast<uint32_t>(bytes[8]) << 8) | bytes[9];
            return timestamp.nanoseconds < 1000000000U;
        }

        long double difference_ns(const Timestamp &left, const Timestamp &right)
        {
            return (static_cast<long double>(left.seconds) - static_cast<long double>(right.seconds)) * 1000000000.0L +
                   static_cast<long double>(left.nanoseconds) - static_cast<long double>(right.nanoseconds);
        }

        struct Message
        {
            uint8_t type = 0;
            uint8_t minor_version = 0;
            bool authenticated = false;
            uint16_t flags = 0;
            uint16_t sequence = 0;
            int64_t correction = 0;
            uint8_t source[10] = {};
            Timestamp timestamp{};
            uint8_t requester[10] = {};
            int16_t utc_offset = 0;
            uint8_t priority1 = 0;
            uint8_t clock_class = 0;
            uint8_t accuracy = 0;
            uint16_t variance = 0;
            uint8_t priority2 = 0;
            uint8_t grandmaster[8] = {};
            uint16_t steps = 0;
            uint8_t time_source = 0;
            int8_t log_interval = 0;
        };

        struct Rank
        {
            int order;
            const char *field;
        };

        constexpr const char *RankFields[] = {"priority1", "class", "accuracy", "variance", "priority2", "grandmaster identity"};
        constexpr size_t RankFieldCount = sizeof(RankFields) / sizeof(RankFields[0]);

        Rank compare_rank(const Message &left, const Message &right)
        {
            const uint32_t lhs[] = {left.priority1, left.clock_class, left.accuracy, left.variance, left.priority2};
            const uint32_t rhs[] = {right.priority1, right.clock_class, right.accuracy, right.variance, right.priority2};
            for (size_t index = 0; index < 5; ++index)
                if (lhs[index] != rhs[index])
                    return {lhs[index] < rhs[index] ? -1 : 1, RankFields[index]};
            const int identity = std::memcmp(left.grandmaster, right.grandmaster, 8);
            return {identity < 0 ? -1 : identity > 0 ? 1 : 0, RankFields[5]};
        }

        bool change_identity(uint8_t *identity, bool smaller)
        {
            uint8_t changed[8];
            std::memcpy(changed, identity, 8);
            for (size_t index = 8; index-- > 0;)
            {
                if ((smaller && changed[index] != 0) || (!smaller && changed[index] != 255))
                {
                    changed[index] = static_cast<uint8_t>(smaller ? changed[index] - 1 : changed[index] + 1);
                    if (!valid_identity(changed))
                        return false;
                    std::memcpy(identity, changed, 8);
                    return true;
                }
                changed[index] = smaller ? 255 : 0;
            }
            return false;
        }

        bool ranked_dataset(const Message &local, bool better, Message &candidate, bool identity_only = false)
        {
            candidate = local;
            if (!change_identity(candidate.grandmaster, better) &&
                (identity_only || !change_identity(candidate.grandmaster, !better)))
                return false;
            if (!identity_only && better && local.priority1 != 0)
                --candidate.priority1;
            else if (!identity_only && !better && local.priority1 != 255)
                ++candidate.priority1;
            else if (!identity_only && better && local.clock_class > 1)
                candidate.clock_class = 1;
            else if (!identity_only && !better && local.clock_class < 254)
                candidate.clock_class = 254;
            else if (!identity_only && better && local.accuracy > 0x20)
                candidate.accuracy = 0x20;
            else if (!identity_only && !better && local.accuracy != 0xfe)
                candidate.accuracy = 0xfe;
            else if (!identity_only && better && local.variance != 0)
                --candidate.variance;
            else if (!identity_only && !better && local.variance != 65535)
                ++candidate.variance;
            else if (!identity_only && better && local.priority2 != 0)
                --candidate.priority2;
            else if (!identity_only && !better && local.priority2 != 255)
                ++candidate.priority2;
            const Rank rank = compare_rank(candidate, local);
            return (better ? rank.order < 0 : rank.order > 0) &&
                   std::memcmp(candidate.grandmaster, local.grandmaster, 8) != 0;
        }

        bool ranked_source_dataset(const Message &local, bool better, const uint8_t *source, Message &candidate)
        {
            if (!ranked_dataset(local, better, candidate))
                return false;
            Message distinct = candidate;
            std::memcpy(distinct.grandmaster, source, 8);
            const int rank = compare_rank(distinct, local).order;
            if (valid_identity(distinct.grandmaster) && std::memcmp(distinct.grandmaster, local.grandmaster, 8) != 0 &&
                (better ? rank < 0 : rank > 0))
                candidate = distinct;
            return true;
        }

        bool changed_grandmaster_dataset(const Message &local, const Message &original, Message &changed)
        {
            for (bool smaller : {true, false})
            {
                changed = original;
                if (change_identity(changed.grandmaster, smaller) &&
                    std::memcmp(changed.grandmaster, local.grandmaster, 8) != 0 &&
                    compare_rank(changed, local).order < 0)
                    return true;
            }
            return false;
        }

        bool precedence_dataset(const Message &local, size_t field, bool better, Message &candidate)
        {
            candidate = local;
            switch (field)
            {
            case 0:
                if (local.priority1 == (better ? 0 : 255))
                    return false;
                candidate.priority1 = static_cast<uint8_t>(better ? local.priority1 - 1 : local.priority1 + 1);
                break;
            case 1:
                if (local.clock_class == (better ? 1 : 254))
                    return false;
                candidate.clock_class = static_cast<uint8_t>(better ? local.clock_class - 1 : local.clock_class + 1);
                break;
            case 2:
                if (local.accuracy == (better ? 0x20 : 0xfe))
                    return false;
                candidate.accuracy = static_cast<uint8_t>(better ? (local.accuracy == 0xfe ? 0x31 : local.accuracy - 1)
                                                                : (local.accuracy == 0x31 ? 0xfe : local.accuracy + 1));
                break;
            case 3:
                if (local.variance == (better ? 0 : 65535))
                    return false;
                candidate.variance = static_cast<uint16_t>(better ? local.variance - 1 : local.variance + 1);
                break;
            case 4:
                if (local.priority2 == (better ? 0 : 255))
                    return false;
                candidate.priority2 = static_cast<uint8_t>(better ? local.priority2 - 1 : local.priority2 + 1);
                break;
            case 5:
                return change_identity(candidate.grandmaster, better);
            default:
                return false;
            }
            // Oppose the deciding field with every later field that has room.
            if (field < 1)
                candidate.clock_class = better ? 254 : 1;
            if (field < 2)
                candidate.accuracy = better ? 0xfe : 0x20;
            if (field < 3)
                candidate.variance = better ? 65535 : 0;
            if (field < 4)
                candidate.priority2 = better ? 255 : 0;
            std::memset(candidate.grandmaster, better ? 0xff : 0, sizeof(candidate.grandmaster));
            candidate.grandmaster[7] = better ? 0xfe : 1;
            if (std::memcmp(candidate.grandmaster, local.grandmaster, sizeof(candidate.grandmaster)) == 0)
                return change_identity(candidate.grandmaster, better);
            return true;
        }

        bool interval_us(int8_t logarithm, int64_t &value)
        {
            if (logarithm < -7 || logarithm > 4)
                return false;
            value = logarithm >= 0 ? int64_t{1000000} << logarithm
                                   : (int64_t{1000000} + (int64_t{1} << -logarithm) - 1) >> -logarithm;
            return true;
        }

        int64_t candidate_lifetime_us(const DutPolicy &policy, int8_t logarithm)
        {
            int64_t interval = 0;
            if (!interval_us(logarithm, interval))
                return 0;
            return policy.selection == SelectionMode::Legacy ? static_cast<int64_t>(policy.foreign_timeout_ms) * 1000
                : std::max(static_cast<int64_t>(policy.foreign_timeout_ms) * 1000, interval * 3);
        }

        bool fresh_sequence(uint16_t previous, uint16_t next)
        {
            const uint16_t distance = static_cast<uint16_t>(next - previous);
            return distance != 0 && distance < 32768;
        }

        struct AnnounceSequence
        {
            uint16_t value = 0;
            uint16_t next()
            {
                value = static_cast<uint16_t>(value + 1);
                return value;
            }

            bool record(uint16_t sequence, SelectionMode mode, bool first)
            {
                const bool fresh = first || fresh_sequence(value, sequence);
                if (fresh || mode == SelectionMode::Legacy)
                    value = sequence;
                return fresh;
            }
        };

        struct SequenceScenario
        {
            const char *label;
            uint16_t first;
            uint16_t probe;
            uint16_t qualifier;
            uint16_t update;
            uint16_t legacy_update;
            bool rejected;
        };

        constexpr SequenceScenario SequenceScenarios[] = {
            {"duplicate", 100, 100, 101, 101, 100, true},
            {"backward across zero", 0, 65535, 1, 0, 65534, true},
            {"half-range", 100, 32868, 101, 32869, 100, true},
            {"forward rollover", 65535, 0, 0, 1, 1, false},
        };

        bool sequence_timing(const Config &config, int8_t &log_interval, int64_t &check_us)
        {
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            const int64_t response = static_cast<int64_t>(config.response_window_ms) * 1000;
            check_us = allowance + 2 * static_cast<int64_t>(
                std::max(config.response_window_ms, config.request_interval_ms)) * 1000;
            int64_t announce = 0;
            int64_t sync = 0;
            if (!interval_us(config.policy.log_announce_interval, announce) ||
                !interval_us(config.policy.log_sync_interval, sync))
                return false;
            for (log_interval = 1; log_interval <= 4; ++log_interval)
            {
                const int64_t lifetime = candidate_lifetime_us(config.policy, log_interval);
                if (lifetime > 4 * check_us &&
                    lifetime * 2 / 3 > std::max(announce, sync) + 4 * allowance + 2 * response)
                    return true;
            }
            return false;
        }

        struct IntervalScenario
        {
            const char *label;
            int8_t logarithm;
            bool supported;
        };

        constexpr IntervalScenario IntervalScenarios[] = {
            {"minimum negative interval", -7, true},
            {"negative half-second interval", -1, true},
            {"long sixteen-second interval", 4, true},
            {"below supported interval range", -8, false},
            {"above supported interval range", 5, false},
            {"unspecified interval", 127, false},
            {"minimum signed-byte interval", -128, false},
        };

        struct IntervalTiming
        {
            int64_t check_us = 0;
            int64_t recovery_us = 0;
        };

        bool observation_timing(const Config &config, IntervalTiming &timing)
        {
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            timing.check_us = allowance + 2 * static_cast<int64_t>(
                std::max(config.response_window_ms, config.request_interval_ms)) * 1000;
            int64_t announce = 0;
            int64_t sync = 0;
            if (!interval_us(config.policy.log_announce_interval, announce) ||
                !interval_us(config.policy.log_sync_interval, sync))
                return false;
            timing.recovery_us = 2 * std::max(announce, sync) + 4 * allowance +
                                 2 * static_cast<int64_t>(config.response_window_ms) * 1000;
            return true;
        }

        bool selection_timing(const Config &config, int8_t &log_interval, IntervalTiming &timing)
        {
            if (!observation_timing(config, timing))
                return false;
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            for (log_interval = 1; log_interval <= 4; ++log_interval)
                if (candidate_lifetime_us(config.policy, log_interval) >
                    2 * timing.recovery_us + 4 * timing.check_us + 4 * allowance)
                    return true;
            return false;
        }

        bool invalid_refresh_timing(const Config &config, int8_t &log_interval, IntervalTiming &timing)
        {
            if (!observation_timing(config, timing))
                return false;
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            for (log_interval = 1; log_interval <= 4; ++log_interval)
                if (candidate_lifetime_us(config.policy, log_interval) >
                    timing.recovery_us + 2 * timing.check_us + 4 * allowance)
                    return true;
            return false;
        }

        bool interval_timing(const Config &config, IntervalTiming &timing)
        {
            if (!observation_timing(config, timing))
                return false;
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            const int64_t fixed = static_cast<int64_t>(config.policy.foreign_timeout_ms) * 1000;
            const int64_t longest = std::max(fixed, int64_t{48000000});
            const int64_t shortest = candidate_lifetime_us(config.policy, -7);
            return shortest > timing.check_us + 2 * allowance &&
                   longest - fixed > timing.recovery_us + 2 * allowance &&
                   (config.policy.selection == SelectionMode::Legacy ||
                    (longest > 4 * timing.check_us &&
                     longest * 2 / 3 > timing.recovery_us + 2 * allowance));
        }

        constexpr int8_t StandbyLogInterval = -1;
        constexpr int64_t StandbyPollBudgetUs = 100000;

        bool standby_timing(const Config &config, IntervalTiming &timing)
        {
            if (!observation_timing(config, timing))
                return false;
            int64_t refresh = 0;
            int64_t announce = 0;
            interval_us(StandbyLogInterval, refresh);
            interval_us(config.policy.log_announce_interval, announce);
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            // A generation reset must allow two refreshes before startup listening ends.
            return 2 * refresh + 2 * allowance + StandbyPollBudgetUs < 2 * announce &&
                   candidate_lifetime_us(config.policy, StandbyLogInterval) > timing.check_us + 2 * allowance;
        }

        Result classify_foreign(Result observation, bool cleanup_healthy, bool interrupted, bool cancelled)
        {
            return !cleanup_healthy || interrupted || cancelled ? Result::CouldNotRun : observation;
        }

        Result classify_startup_reference(int64_t earliest_start, int64_t latest_start, int64_t listening_us,
                                          int64_t first_service_us, bool healthy_after, bool interrupted)
        {
            if (interrupted || !healthy_after || earliest_start <= 0 || latest_start < earliest_start ||
                listening_us <= 0 || latest_start > std::numeric_limits<int64_t>::max() - listening_us)
                return Result::CouldNotRun;
            if (first_service_us == 0 || first_service_us >= latest_start + listening_us)
                return Result::Pass;
            if (first_service_us < earliest_start)
                return Result::CouldNotRun;
            return first_service_us < earliest_start + listening_us ? Result::Fail : Result::CouldNotRun;
        }

        struct ForeignSchedule
        {
            SelectionMode mode;
            int64_t lifetime_us;
            int64_t allowance_us;
            int64_t last_begin_us = 0;
            int64_t last_end_us = 0;
            int64_t silence_after_us = 0;
            uint32_t sends = 0;
            uint32_t fresh_sends = 0;

            void sent(int64_t begin_us, int64_t end_us, bool fresh = true)
            {
                ++sends;
                if (fresh)
                    ++fresh_sends;
                if (fresh || mode == SelectionMode::Legacy)
                {
                    last_begin_us = begin_us;
                    last_end_us = end_us;
                }
                if ((mode == SelectionMode::Legacy && sends == 1) ||
                    (mode == SelectionMode::Ranked && fresh_sends == 2 && fresh && silence_after_us == 0))
                    silence_after_us = end_us + allowance_us;
            }

            int64_t no_early_service_until() const { return last_begin_us + lifetime_us; }
            int64_t cleanup_until() const { return last_end_us + lifetime_us + allowance_us; }
        };

        bool standby_poll_valid(int64_t now, int64_t last_poll, const ForeignSchedule &schedule)
        {
            return (last_poll == 0 || (now >= last_poll && now - last_poll <= StandbyPollBudgetUs)) &&
                   now + schedule.allowance_us < schedule.no_early_service_until();
        }

        struct CleanupLease
        {
            bool pending = false;
            int64_t deadline_us = 0;
            uint8_t target[6] = {};
            uint8_t domain = 0;

            void attempted(const Config &config, int64_t end_us, int64_t lifetime_us, int64_t allowance_us)
            {
                pending = true;
                std::memcpy(target, config.target_mac, sizeof(target));
                domain = config.domain;
                deadline_us = std::max(deadline_us, end_us + lifetime_us + allowance_us);
            }

            bool matches(const Config &config) const
            {
                return std::memcmp(target, config.target_mac, sizeof(target)) == 0 && domain == config.domain;
            }

            void completed(bool healthy)
            {
                pending = !healthy;
                if (healthy)
                    deadline_us = 0;
            }
        };

        struct RotationRestoreLease
        {
            bool pending = false;
            uint8_t target[6] = {};
            uint8_t domain = 0;
            uint32_t original_key_id = 0;

            void begin(const Config &config)
            {
                pending = true;
                std::memcpy(target, config.target_mac, sizeof(target));
                domain = config.domain;
                original_key_id = config.key_id;
            }

            bool matches(const Config &config) const
            {
                return std::memcmp(target, config.target_mac, sizeof(target)) == 0 &&
                       domain == config.domain && original_key_id == config.key_id;
            }

            void completed(bool confirmed)
            {
                if (confirmed)
                    pending = false;
            }
        };

        bool signed_policy(const Config &config)
        {
            return config.policy.authentication == AuthenticationMode::Mixed ||
                   config.policy.authentication == AuthenticationMode::Required;
        }

        bool configured_key(const Config &config)
        {
            return config.key_id != 0 && config.key_value != nullptr && config.key_value[0] != '\0';
        }

        const char *policy_prerequisite(Test test, const Config &config)
        {
            if (config.policy.authentication == AuthenticationMode::Required && !configured_key(config))
                return "PTP authentication requires the target profile's separate PTP symmetric-key id and value";
            if (signed_policy(config) && !configured_key(config) &&
                (config.key_id != 0 || (config.key_value != nullptr && config.key_value[0] != '\0')))
                return "PTP key configuration is incomplete; supply both id and value, or leave both empty for optional unsigned service";
            if (requires_suppression_opt_in(test) || requires_operator(test))
            {
                if (const char *issue = policy_error(config.policy))
                    return issue;
                if (config.domain != config.policy.domain)
                    return "selected PTP domain contradicts DUT policy";
            }
            if (test == Test::RankingPrecedence && config.policy.selection != SelectionMode::Ranked)
                return "ranking precedence is not applicable to legacy any-Announce suppression; ranked DUT selection is required";
            if ((test == Test::DatasetChanges || test == Test::MultipleMasters) &&
                config.policy.selection != SelectionMode::Ranked)
                return "dataset requalification/reflection and independent candidate lifetimes are not applicable to legacy any-Announce suppression; ranked DUT selection is required";
            if (test == Test::MultipleMasters && config.policy.capacity < 2)
                return "overlapping qualified masters require DUT foreign-master capacity of at least two";
            if ((is_standby_recovery(test) || test == Test::ForeignCapacity || test == Test::SelectionNtpLoad ||
                 test == Test::SelectionNtpPolicyLoad ||
                 test == Test::StartupListening) &&
                config.policy.selection != SelectionMode::Ranked)
                return "this standby/capacity scenario requires ranked DUT selection; legacy global suppression has no equivalent qualification contract";
            if ((test >= Test::SignedMessages && test <= Test::AuthenticationBeforeSelection) ||
                test == Test::AnnounceAuthenticationPolicy || test == Test::InvalidAuthenticationRefresh ||
                test == Test::KeyRotation)
            {
                if (const char *issue = policy_error(config.policy))
                    return issue;
                if (!signed_policy(config))
                    return "authentication scenarios require a DUT with PTP authentication enabled (mixed or required)";
            }
            if (test == Test::OptionalUnsignedService || test == Test::OptionalSignedRejection)
            {
                if (const char *issue = policy_error(config.policy))
                    return issue;
                if (config.domain != config.policy.domain)
                    return "selected PTP domain contradicts DUT policy";
                if (config.policy.authentication != AuthenticationMode::Mixed)
                    return "optional unsigned scenarios require enabled, optional DUT authentication";
            }
            return nullptr;
        }

        const char *service_prerequisite(Test test, const Message &master)
        {
            if (((test >= Test::SignedMessages && test <= Test::AuthenticationBeforeSelection) ||
                 test == Test::AnnounceAuthenticationPolicy || test == Test::InvalidAuthenticationRefresh ||
                 test == Test::KeyRotation) && !master.authenticated)
                return "signed scenarios are not applicable to observed unsigned service; provision matching PTP keys on both devices and restart the DUT";
            if ((test == Test::OptionalUnsignedService || test == Test::OptionalSignedRejection) && master.authenticated)
                return "optional unsigned scenarios are not applicable to a verified signed DUT; intentionally keyless optional service must be configured on the DUT before testing";
            return nullptr;
        }

        const char *wire_policy_error(const Config &config, const Message &message)
        {
            if (policy_error(config.policy) != nullptr)
                return nullptr;
            if (config.domain != config.policy.domain ||
                message.minor_version != (message.authenticated ? 1 : 0) ||
                (config.policy.authentication == AuthenticationMode::Disabled && message.authenticated) ||
                (config.policy.authentication == AuthenticationMode::Required && !message.authenticated) ||
                (message.type == 11 && message.log_interval != config.policy.log_announce_interval) ||
                ((message.type == 0 || message.type == 8) && message.log_interval != config.policy.log_sync_interval))
                return "on-wire domain/version/periodic interval contradicts DUT metadata";
            return nullptr;
        }

        struct ServiceAuthentication
        {
            bool learned = false;
            bool authenticated = false;

            const char *observe(const Message &message)
            {
                if (learned && authenticated != message.authenticated)
                    return "DUT outgoing authentication mode changed during the test; no signed/unsigned downgrade or parallel stream is permitted";
                if (!learned && message.type == 11)
                {
                    learned = true;
                    authenticated = message.authenticated;
                }
                return nullptr;
            }
        };

        enum class ParseResult : uint8_t
        {
            Valid,
            Ignore,
            Malformed,
            Unsupported,
            AuthenticationUnsupported,
            AuthenticationInvalid,
            AuthenticationUnavailable
        };

        ParseResult parse_message(const uint8_t *bytes, size_t available, uint8_t domain, Message &message,
                                  const Authenticator *auth = nullptr)
        {
            message = {};
            if (bytes == nullptr || available < 34)
                return ParseResult::Malformed;
            const size_t length = read16(bytes + 2);
            if (length < 34 || length > available || length > MaxMessageSize || (length & 1) != 0)
                return ParseResult::Malformed;
            if (bytes[4] != domain)
                return ParseResult::Ignore;
            if ((bytes[0] & 0xf0) != 0 || (bytes[1] & 0x0f) != 2 || (bytes[1] >> 4) > 1 || bytes[5] != 0)
                return ParseResult::Unsupported;
            message.type = bytes[0] & 0x0f;
            message.minor_version = bytes[1] >> 4;
            size_t base_length = 0;
            uint8_t expected_control = 0;
            switch (message.type)
            {
            case 0:
                base_length = 44;
                expected_control = 0;
                break;
            case 1:
                base_length = 44;
                expected_control = 1;
                break;
            case 8:
                base_length = 44;
                expected_control = 2;
                break;
            case 9:
                base_length = 54;
                expected_control = 3;
                break;
            case 11:
                base_length = 64;
                expected_control = 5;
                break;
            default:
                return ParseResult::Ignore;
            }
            // PTP v2.1 deprecates controlField; messageType determines dispatch.
            if (length < base_length || (message.minor_version == 0 && bytes[32] != expected_control) ||
                !valid_port_identity(bytes + 20))
                return ParseResult::Malformed;
            for (size_t index = 16; index < 20; ++index)
            {
                if (bytes[index] != 0)
                    return ParseResult::Unsupported;
            }
            message.flags = read16(bytes + 6);
            if ((message.flags & ~uint16_t{0x063f}) != 0 || (message.flags & 0x0400) != 0)
                return ParseResult::Unsupported;
            for (size_t offset = base_length; offset < length;)
            {
                if (length - offset < 4)
                    return ParseResult::Malformed;
                const uint16_t type = read16(bytes + offset);
                const size_t tlv_length = read16(bytes + offset + 2);
                if ((tlv_length & 1) != 0 || tlv_length > length - offset - 4)
                    return ParseResult::Malformed;
                if (type == 0x8009)
                {
                    if (auth == nullptr)
                        return ParseResult::AuthenticationUnsupported;
                    const AuthResult verified = auth->verify(bytes, length);
                    if (verified == AuthResult::Unavailable)
                        return ParseResult::AuthenticationUnavailable;
                    if (verified != AuthResult::Valid)
                        return ParseResult::AuthenticationInvalid;
                    message.authenticated = true;
                    offset += 4 + tlv_length;
                    continue;
                }
                if (type != 0x0008 || message.type != 11)
                    return ParseResult::Unsupported;
                if (tlv_length == 0 || tlv_length % 8 != 0)
                    return ParseResult::Malformed;
                for (size_t index = 0; index < tlv_length; index += 8)
                {
                    if (!valid_identity(bytes + offset + 4 + index))
                        return ParseResult::Malformed;
                }
                offset += 4 + tlv_length;
            }
            message.sequence = read16(bytes + 30);
            message.log_interval = static_cast<int8_t>(bytes[33] < 128 ? bytes[33] : static_cast<int>(bytes[33]) - 256);
            message.correction = read_signed64(bytes + 8);
            std::memcpy(message.source, bytes + 20, 10);
            if (!read_timestamp(bytes + 34, message.timestamp))
                return ParseResult::Malformed;
            if (message.type == 9)
            {
                std::memcpy(message.requester, bytes + 44, 10);
                if (!valid_port_identity(message.requester))
                    return ParseResult::Malformed;
            }
            if (message.type == 11)
            {
                const uint16_t raw_offset = read16(bytes + 44);
                message.utc_offset = raw_offset < 0x8000 ? static_cast<int16_t>(raw_offset)
                                                       : static_cast<int16_t>(static_cast<int32_t>(raw_offset) - 65536);
                message.priority1 = bytes[47];
                message.clock_class = bytes[48];
                message.accuracy = bytes[49];
                message.variance = read16(bytes + 50);
                message.priority2 = bytes[52];
                std::memcpy(message.grandmaster, bytes + 53, 8);
                message.steps = read16(bytes + 61);
                message.time_source = bytes[63];
                if (bytes[46] != 0 || !valid_identity(message.grandmaster) || message.steps >= 255 ||
                    message.clock_class == 0 || message.clock_class == 255 ||
                    !((message.accuracy >= 0x20 && message.accuracy <= 0x31) || message.accuracy == 0xfe))
                    return ParseResult::Malformed;
                switch (message.time_source)
                {
                case 0x10:
                case 0x20:
                case 0x30:
                case 0x40:
                case 0x50:
                case 0x60:
                case 0x90:
                case 0xa0:
                    break;
                default:
                    return ParseResult::Unsupported;
                }
                if ((message.flags & 3) == 3 ||
                    ((message.flags & PtpTimescaleFlag) == 0 && (message.flags & 7) != 0))
                    return ParseResult::Malformed;
            }
            return ParseResult::Valid;
        }

        struct Packet
        {
            uint8_t frame[MaxFrameSize] = {};
            uint16_t length = 0;
            bool unsupported_vlan = false;
            bool oversized = false;
            bool hardware_timestamp = false;
            Timestamp timestamp{};
            int64_t received_us = 0;
            uint32_t generation = 0;
        };

        ParseResult parse_config_message(const Packet &packet, const Config &config, const Authenticator &auth, Message &message)
        {
            const ParseResult result = parse_message(packet.frame + 14, packet.length - 14, config.domain, message, &auth);
            if (result == ParseResult::Valid && config.policy.authentication == AuthenticationMode::Required && !message.authenticated)
                return ParseResult::AuthenticationInvalid;
            if (result == ParseResult::Valid && config.policy.authentication == AuthenticationMode::Disabled && message.authenticated)
                return ParseResult::AuthenticationInvalid;
            return result;
        }

        bool has_required_rx_timestamp(const Message &message, const Packet &packet)
        {
            // E2E uses Sync RX and Delay_Req TX, not general-message RX timestamps.
            return message.type != 0 || packet.hardware_timestamp;
        }

        struct Pair
        {
            bool occupied = false;
            bool sync_seen = false;
            bool follow_seen = false;
            uint16_t sequence = 0;
            uint8_t minor_version = 0;
            uint32_t generation = 0;
            int64_t first_us = 0;
            Message sync{};
            Message follow{};
            Packet reception{};
        };

        bool match_pair(std::array<Pair, 4> &pairs, const Message &message, const Packet &packet, Pair &completed)
        {
            if (message.type != 0 && message.type != 8)
                return false;
            if (message.type == 0 && (message.flags & TwoStepFlag) == 0)
                return false;
            Pair *slot = nullptr;
            for (Pair &pair : pairs)
            {
                if (pair.occupied && (packet.generation != pair.generation ||
                                      packet.received_us - pair.first_us > PairLifetimeUs))
                    pair = {};
                if (pair.occupied && pair.sequence == message.sequence && pair.minor_version == message.minor_version &&
                    std::memcmp((pair.sync_seen ? pair.sync : pair.follow).source, message.source, 10) == 0)
                    slot = &pair;
            }
            if (slot == nullptr)
            {
                for (Pair &pair : pairs)
                {
                    if (!pair.occupied)
                    {
                        slot = &pair;
                        break;
                    }
                }
            }
            if (slot == nullptr)
                slot = &*std::min_element(pairs.begin(), pairs.end(), [](const Pair &left, const Pair &right)
                                         { return left.first_us < right.first_us; });
            if (!slot->occupied || slot->sequence != message.sequence || slot->minor_version != message.minor_version ||
                std::memcmp((slot->sync_seen ? slot->sync : slot->follow).source, message.source, 10) != 0)
            {
                *slot = {};
                slot->occupied = true;
                slot->sequence = message.sequence;
                slot->minor_version = message.minor_version;
                slot->generation = packet.generation;
                slot->first_us = packet.received_us;
            }
            if (message.type == 0)
            {
                if (!slot->sync_seen)
                {
                    slot->sync_seen = true;
                    slot->sync = message;
                    slot->reception = packet;
                }
            }
            else if (!slot->follow_seen)
            {
                slot->follow_seen = true;
                slot->follow = message;
            }
            if (!slot->sync_seen || !slot->follow_seen)
                return false;
            completed = *slot;
            *slot = {};
            return true;
        }

        bool match_delay_response(const Message &message, const uint8_t *master, uint8_t minor_version,
                                  uint16_t sequence, const uint8_t *requester)
        {
            return message.type == 9 && message.minor_version == minor_version && message.sequence == sequence &&
                   std::memcmp(message.source, master, 10) == 0 && std::memcmp(message.requester, requester, 10) == 0;
        }

        void promote_to_v21(uint8_t *message)
        {
            if (message[1] == 2)
                message[32] = 0;
            message[1] = 0x12;
        }

        void make_delay_request(uint8_t *frame, const uint8_t *local_mac, const uint8_t *identity,
                                uint8_t domain, uint8_t minor_version, uint16_t sequence)
        {
            std::memset(frame, 0, 60);
            std::memcpy(frame, MulticastMac, 6);
            std::memcpy(frame + 6, local_mac, 6);
            write16(frame + 12, 0x88f7);
            uint8_t *message = frame + 14;
            message[0] = 1;
            message[1] = static_cast<uint8_t>((minor_version << 4) | 2);
            write16(message + 2, 44);
            message[4] = domain;
            std::memcpy(message + 20, identity, 10);
            write16(message + 30, sequence);
            message[32] = minor_version == 0 ? 1 : 0;
            message[33] = 0x7f;
        }

        struct Measurement
        {
            long double mean_path_delay_ns = 0;
            long double local_minus_master_ns = 0;
        };

        bool calculate_measurement(const Pair &pair, const Timestamp &transmit, const Message &response,
                                   Measurement &measurement)
        {
            const long double master_elapsed = difference_ns(response.timestamp, pair.follow.timestamp);
            const long double local_elapsed = difference_ns(transmit, pair.reception.timestamp);
            const long double sync_correction = static_cast<long double>(pair.sync.correction) / 65536.0L +
                                                static_cast<long double>(pair.follow.correction) / 65536.0L;
            const long double delay_correction = static_cast<long double>(response.correction) / 65536.0L;
            if (master_elapsed < 0 || master_elapsed > PairLifetimeUs * 1000.0L ||
                local_elapsed < 0 || local_elapsed > PairLifetimeUs * 1000.0L)
                return false;
            measurement.mean_path_delay_ns = (master_elapsed - local_elapsed - sync_correction - delay_correction) / 2.0L;
            measurement.local_minus_master_ns = difference_ns(pair.reception.timestamp, pair.follow.timestamp) -
                                                sync_correction - measurement.mean_path_delay_ns;
            return std::isfinite(measurement.mean_path_delay_ns) && std::isfinite(measurement.local_minus_master_ns) &&
                   measurement.mean_path_delay_ns >= 0 && measurement.mean_path_delay_ns <= 1000000000.0L;
        }

        bool raw_delay_match(const Packet &packet, const uint8_t *master, uint16_t sequence, const uint8_t *requester)
        {
            if (packet.length < 68 || packet.unsupported_vlan || packet.oversized)
                return false;
            const uint8_t *message = packet.frame + 14;
            return (message[0] & 0x0f) == 9 && read16(message + 2) >= 54 &&
                   read16(message + 2) <= packet.length - 14 && read16(message + 30) == sequence &&
                   std::memcmp(message + 20, master, 10) == 0 && std::memcmp(message + 44, requester, 10) == 0;
        }

        bool in_window(int64_t received_us, int64_t start_us, int64_t deadline_us)
        {
            return received_us >= start_us && received_us < deadline_us;
        }

        bool conservative_properties(const Message &message)
        {
            return message.type == 11 && (message.flags & 0x003f) == 0 &&
                   message.clock_class == 248 && message.accuracy == 0xfe &&
                   message.variance == 65535 && message.steps == 0 && message.time_source == 0xa0;
        }

        bool supported_properties(const Message &message)
        {
            if (conservative_properties(message))
                return true;
            return message.type == 11 && (message.flags & ~uint16_t{3}) == (PtpTimescaleFlag | UtcValidFlag) &&
                   (message.flags & 3) != 3 && message.clock_class == 248 && message.accuracy == 0xfe &&
                   message.variance == 65535 && message.steps == 0 && message.time_source == 0x20;
        }

        bool restricted_output(const Packet &packet, const Message &message)
        {
            uint16_t length = 0;
            switch (message.type)
            {
            case 0:
            case 8:
                length = 44;
                break;
            case 9:
                length = 54;
                break;
            case 11:
                length = 64;
                break;
            default:
                return false;
            }
            length += message.authenticated ? AuthTlvSize : 0;
            return packet.length >= 14 + length && read16(packet.frame + 16) == length &&
                   message.minor_version == (message.authenticated ? 1 : 0) && message.correction == 0 &&
                   (message.type == 11 ? message.flags == 0 ||
                                             ((message.flags & ~uint16_t{3}) == (PtpTimescaleFlag | UtcValidFlag) &&
                                              (message.flags & 3) != 3)
                                       : message.flags == (message.type == 0 ? TwoStepFlag : 0));
        }

        struct Statistics
        {
            uint32_t count = 0;
            long double minimum = 0;
            long double maximum = 0;
            long double mean = 0;
            long double square_sum = 0;

            void add(long double value)
            {
                if (count == 0)
                    minimum = maximum = value;
                minimum = std::min(minimum, value);
                maximum = std::max(maximum, value);
                ++count;
                const long double delta = value - mean;
                mean += delta / count;
                square_sum += delta * (value - mean);
            }

            long double deviation() const
            {
                return count > 1 ? std::sqrt(std::max(0.0L, square_sum / (count - 1))) : 0;
            }
        };

        struct Observation
        {
            std::array<Pair, 4> pending{};
            struct Seen
            {
                bool occupied = false;
                uint16_t sequence = 0;
                uint8_t minor = 0;
                uint8_t source[10] = {};
            };
            std::array<Seen, 128> history{};
            size_t history_cursor = 0;
            uint32_t generation = 0;
            uint32_t pairs = 0;
            uint32_t duplicates = 0;
            uint32_t unmatched = 0;
            uint32_t expired = 0;
            uint32_t out_of_order = 0;
            uint32_t missing = 0;
            uint32_t sequence_gaps = 0;
            uint32_t requests = 0;
            uint32_t responses = 0;
            uint32_t response_loss = 0;
            uint32_t timestamp_failures = 0;
            uint32_t unusable = 0;
            bool have_sequence = false;
            uint16_t last_sequence = 0;
            Statistics delay{};
            Statistics offset{};

            void expire(int64_t now_us, bool finish = false)
            {
                for (Pair &pair : pending)
                {
                    if (pair.occupied && (finish || now_us - pair.first_us > PairLifetimeUs))
                    {
                        ++unmatched;
                        ++missing;
                        expired += !finish;
                        pair = {};
                    }
                }
            }

            bool accept(const Message &message, const Packet &packet, Pair &completed)
            {
                if (packet.generation != generation || (message.type != 0 && message.type != 8))
                    return false;
                expire(packet.received_us);
                for (const Seen &seen : history)
                {
                    if (seen.occupied && seen.sequence == message.sequence && seen.minor == message.minor_version &&
                        std::memcmp(seen.source, message.source, 10) == 0)
                    {
                        ++duplicates;
                        return false;
                    }
                }
                bool existing = false;
                for (const Pair &pair : pending)
                {
                    if (pair.occupied && pair.sequence == message.sequence && pair.minor_version == message.minor_version &&
                        std::memcmp((pair.sync_seen ? pair.sync : pair.follow).source, message.source, 10) == 0)
                    {
                        existing = true;
                        if ((message.type == 0 && pair.sync_seen) || (message.type == 8 && pair.follow_seen))
                        {
                            ++duplicates;
                            return false;
                        }
                        if (message.type == 0 && pair.follow_seen)
                            ++out_of_order;
                    }
                }
                if (!existing && std::all_of(pending.begin(), pending.end(), [](const Pair &pair) { return pair.occupied; }))
                {
                    ++unmatched;
                    ++expired;
                    ++missing;
                }
                if (!match_pair(pending, message, packet, completed))
                    return false;
                Seen &seen = history[history_cursor++ % history.size()];
                seen.occupied = true;
                seen.sequence = message.sequence;
                seen.minor = message.minor_version;
                std::memcpy(seen.source, message.source, 10);
                if (have_sequence)
                {
                    const uint16_t advance = static_cast<uint16_t>(message.sequence - last_sequence);
                    if (advance < 0x8000 && advance != 0)
                    {
                        sequence_gaps += advance - 1;
                        last_sequence = message.sequence;
                    }
                    else
                        ++out_of_order;
                }
                else
                {
                    last_sequence = message.sequence;
                    have_sequence = true;
                }
                ++pairs;
                return true;
            }
        };

        Result classify_observation(const Observation &observation, uint16_t required, bool interrupted,
                                    uint32_t drops, bool protocol_fault)
        {
            if (interrupted || drops != 0 || observation.timestamp_failures != 0 || observation.unusable != 0)
                return Result::CouldNotRun;
            if (protocol_fault || observation.pairs < required || observation.responses < required ||
                observation.delay.count < required || observation.missing != 0 || observation.sequence_gaps != 0 ||
                observation.response_loss != 0)
                return Result::Fail;
            return Result::Pass;
        }

        enum class Mutation : uint8_t
        {
            WrongDomain, Version, MinorVersion, UnsupportedMinorVersion, Transport, ReservedDomain, ReservedHeader,
            Flags, Control, ShortHeader, ShortBody, InconsistentLength, Oversized,
            ExtraTlv, AuthenticationTlv, TruncatedTlv, OddTlv, OverrunningTlv,
            ZeroIdentity, OnesIdentity, ZeroPort, ReservedPort, OwnIdentity,
            InvalidNanoseconds, PositiveCorrection, NegativeCorrection, WrongDestination, Vlan,
            Padding, SecondPort, Count
        };

        constexpr const char *MutationNames[] = {
            "wrong-domain", "version", "minor-version", "unsupported-minor-version", "transportSpecific", "reserved-domain", "reserved-header",
            "flags", "control", "short-PTP-header", "short-PTP-body", "messageLength", "oversized",
            "extra-TLV", "authentication-TLV-unsupported", "truncated-TLV", "odd-TLV", "overrunning-TLV",
            "zero-clock", "ones-clock", "zero-port", "reserved-port", "target-clock",
            "nanoseconds", "positive-correction", "negative-correction", "destination-filter", "VLAN-filter",
            "legal-padding", "distinct-valid-port"
        };
        static_assert(sizeof(MutationNames) / sizeof(MutationNames[0]) == static_cast<size_t>(Mutation::Count));

        size_t mutate_request(uint8_t *frame, Mutation mutation, const uint8_t *master)
        {
            uint8_t *message = frame + 14;
            size_t length = 60;
            switch (mutation)
            {
            case Mutation::WrongDomain: ++message[4]; break;
            case Mutation::Version: message[1] = 3; break;
            case Mutation::MinorVersion: promote_to_v21(message); break;
            case Mutation::UnsupportedMinorVersion: message[1] = 0x22; break;
            case Mutation::Transport: message[0] |= 0x10; break;
            case Mutation::ReservedDomain: message[5] = 1; break;
            case Mutation::ReservedHeader: message[16] = 1; break;
            case Mutation::Flags: write16(message + 6, 0x0200); break;
            case Mutation::Control: message[32] = 5; break;
            case Mutation::ShortHeader: write16(message + 2, 32); break;
            case Mutation::ShortBody: write16(message + 2, 42); break;
            case Mutation::InconsistentLength: write16(message + 2, 64); break;
            case Mutation::Oversized: length = MaxFrameSize; write16(message + 2, MaxMessageSize); break;
            case Mutation::ExtraTlv:
            case Mutation::AuthenticationTlv:
                length = 62;
                write16(message + 2, 48);
                write16(message + 44, mutation == Mutation::AuthenticationTlv ? 0x8009 : 0x1234);
                break;
            case Mutation::TruncatedTlv: write16(message + 2, 46); message[44] = 0x12; break;
            case Mutation::OddTlv:
            case Mutation::OverrunningTlv:
                length = 64;
                write16(message + 2, 50);
                write16(message + 44, 0x1234);
                write16(message + 46, mutation == Mutation::OddTlv ? 1 : 4);
                break;
            case Mutation::ZeroIdentity: std::memset(message + 20, 0, 8); break;
            case Mutation::OnesIdentity: std::memset(message + 20, 0xff, 8); break;
            case Mutation::ZeroPort: write16(message + 28, 0); break;
            case Mutation::ReservedPort: write16(message + 28, 0xffff); break;
            case Mutation::OwnIdentity: std::memcpy(message + 20, master, 8); break;
            case Mutation::InvalidNanoseconds:
                message[40] = 0x3b; message[41] = 0x9a; message[42] = 0xca; message[43] = 0x00;
                break;
            case Mutation::PositiveCorrection: message[15] = 1; break;
            case Mutation::NegativeCorrection: std::memset(message + 8, 0xff, 8); break;
            case Mutation::WrongDestination: frame[0] = 0x03; frame[5] = 1; break;
            case Mutation::Vlan:
                std::memmove(frame + 18, frame + 14, 46);
                write16(frame + 12, 0x8100); write16(frame + 14, 1); write16(frame + 16, 0x88f7);
                length = 64;
                break;
            case Mutation::Padding: frame[58] = 0xa5; frame[59] = 0x5a; break;
            case Mutation::SecondPort: write16(message + 28, 2); break;
            case Mutation::Count: break;
            }
            return length;
        }

        bool mutation_expects_response(Mutation mutation, uint8_t minor_version)
        {
            return mutation == Mutation::MinorVersion || mutation == Mutation::Padding || mutation == Mutation::SecondPort ||
                   (mutation == Mutation::Control && minor_version == 1);
        }

        bool authenticated_mutation_supported(Mutation mutation)
        {
            switch (mutation)
            {
            case Mutation::Version:
            case Mutation::MinorVersion:
            case Mutation::UnsupportedMinorVersion:
            case Mutation::ShortHeader:
            case Mutation::ShortBody:
            case Mutation::InconsistentLength:
            case Mutation::Oversized:
            case Mutation::ExtraTlv:
            case Mutation::AuthenticationTlv:
            case Mutation::TruncatedTlv:
            case Mutation::OddTlv:
            case Mutation::OverrunningTlv:
            case Mutation::Vlan:
            case Mutation::Padding:
            case Mutation::Count:
                return false;
            default:
                return true;
            }
        }

        Result classify_negative(bool before, bool after, bool response, bool expected,
                                 bool interrupted, uint32_t drops)
        {
            if (!before || !after || interrupted || drops != 0)
                return Result::CouldNotRun;
            return response == expected ? Result::Pass : Result::Fail;
        }

        Result classify_authenticated_probe(bool before, bool after, bool response, bool verified, bool expected,
                                            bool interrupted, uint32_t drops, bool protocol_fault)
        {
            const Result result = classify_negative(before, after, response, expected, interrupted, drops);
            if (result == Result::CouldNotRun)
                return result;
            return result == Result::Fail || (response && !verified) || protocol_fault ? Result::Fail : Result::Pass;
        }

        void make_foreign_announce(uint8_t *frame, const uint8_t *mac, const uint8_t *identity,
                                   uint8_t domain, uint16_t sequence)
        {
            std::memset(frame, 0, 78);
            std::memcpy(frame, MulticastMac, 6);
            std::memcpy(frame + 6, mac, 6);
            write16(frame + 12, 0x88f7);
            uint8_t *message = frame + 14;
            message[0] = 11;
            message[1] = 2;
            write16(message + 2, 64);
            message[4] = domain;
            std::memcpy(message + 20, identity, 10);
            write16(message + 30, sequence);
            message[32] = 5;
            message[33] = 1;
            message[47] = 128;
            message[48] = 248;
            message[49] = 0xfe;
            write16(message + 50, 65535);
            message[52] = 128;
            std::memcpy(message + 53, identity, 8);
            message[63] = 0xa0;
        }

        void set_foreign_dataset(uint8_t *frame, const Message &dataset, int8_t log_interval)
        {
            uint8_t *message = frame + 14;
            message[33] = static_cast<uint8_t>(log_interval);
            message[47] = dataset.priority1;
            message[48] = dataset.clock_class;
            message[49] = dataset.accuracy;
            write16(message + 50, dataset.variance);
            message[52] = dataset.priority2;
            std::memcpy(message + 53, dataset.grandmaster, 8);
        }

        enum class UdpMutation : uint8_t
        {
            WrongDomain,
            WrongVersion,
            InconsistentLength,
            ZeroClockIdentity,
            Truncated,
            Count
        };

        constexpr const char *UdpMutationNames[] = {
            "wrong-domain", "wrong-version", "inconsistent-length", "zero-clock-identity", "truncated-header"
        };
        static_assert(std::size(UdpMutationNames) == static_cast<size_t>(UdpMutation::Count));

        size_t make_udp_delay_request(uint8_t *message, const uint8_t *local_mac, const uint8_t *identity,
                                      uint8_t domain, uint8_t minor_version, uint16_t sequence,
                                      UdpMutation mutation = UdpMutation::Count)
        {
            uint8_t frame[60];
            make_delay_request(frame, local_mac, identity, domain, minor_version, sequence);
            std::memcpy(message, frame + 14, 44);
            switch (mutation)
            {
            case UdpMutation::WrongDomain: ++message[4]; break;
            case UdpMutation::WrongVersion: message[1] = 3; break;
            case UdpMutation::InconsistentLength: write16(message + 2, 43); break;
            case UdpMutation::ZeroClockIdentity: std::memset(message + 20, 0, 8); break;
            case UdpMutation::Truncated: return 33;
            case UdpMutation::Count: break;
            }
            return 44;
        }

        bool raw_udp_delay_match(const uint8_t *bytes, size_t length, const uint8_t *master,
                                 uint16_t sequence, const uint8_t *requester)
        {
            return length >= 54 && (bytes[0] & 0x0f) == 9 && read16(bytes + 2) >= 54 &&
                   read16(bytes + 2) <= length && read16(bytes + 30) == sequence &&
                   std::memcmp(bytes + 20, master, 10) == 0 &&
                   std::memcmp(bytes + 44, requester, 10) == 0;
        }

        bool udp_timestamp_present(const Timestamp &timestamp)
        {
            return timestamp.seconds != 0 || timestamp.nanoseconds != 0;
        }

        struct UdpObservation
        {
            Message master{};
            Observation pairs{};
            uint8_t requester[10] = {};
            bool master_locked = false;
            bool source_changed = false;
            bool protocol_fault = false;
            bool outstanding = false;
            uint16_t sequence = 0;
            int64_t request_deadline_us = 0;
            int64_t next_request_us = 0;
            uint32_t announces = 0;
            uint32_t requests = 0;
            uint32_t responses = 0;
            uint32_t response_timeouts = 0;
            uint32_t malformed = 0;
            uint32_t unsupported = 0;

            void accept(const Message &message, const Packet &packet)
            {
                if (message.type == 11)
                {
                    ++announces;
                    if (!master_locked)
                    {
                        master = message;
                        master_locked = true;
                    }
                    else if (std::memcmp(master.source, message.source, 10) != 0 ||
                             std::memcmp(master.grandmaster, message.grandmaster, 8) != 0 ||
                             master.minor_version != message.minor_version)
                        source_changed = true;
                    return;
                }
                if (!master_locked || message.type == 1)
                    return;
                if (std::memcmp(master.source, message.source, 10) != 0 ||
                    master.minor_version != message.minor_version)
                {
                    source_changed = true;
                    return;
                }
                if (message.type == 0 || message.type == 8)
                {
                    if ((message.type == 0 && (message.flags & TwoStepFlag) == 0) ||
                        (message.type == 8 && !udp_timestamp_present(message.timestamp)))
                    {
                        protocol_fault = true;
                        return;
                    }
                    Pair completed{};
                    pairs.accept(message, packet, completed);
                }
                else if (outstanding && match_delay_response(message, master.source, master.minor_version,
                                                             sequence, requester))
                {
                    if (packet.received_us > request_deadline_us)
                        ++response_timeouts;
                    else if (!udp_timestamp_present(message.timestamp))
                        protocol_fault = true;
                    else
                        ++responses;
                    outstanding = false;
                }
            }
        };

        Result classify_udp_observation(const UdpObservation &observation, uint16_t required, bool interrupted)
        {
            if (interrupted || observation.source_changed)
                return Result::CouldNotRun;
            if (observation.protocol_fault || observation.malformed || observation.unsupported ||
                observation.pairs.pairs < required || observation.requests < required ||
                observation.responses < required || observation.response_timeouts ||
                observation.pairs.duplicates || observation.pairs.missing || observation.pairs.sequence_gaps)
                return Result::Fail;
            return Result::Pass;
        }

        enum class AuthMutation : uint8_t
        {
            UnknownKey, WrongKey, Body, Icv, Spp, Indicators, TlvType, TlvLength, Truncated, Duplicate, Count
        };

        constexpr const char *AuthMutationNames[] = {
            "unknown-key-id", "wrong-key-value", "tampered-body", "tampered-ICV", "wrong-SPP",
            "unsupported-indicators", "wrong-TLV-type", "wrong-TLV-length", "truncated-ICV", "duplicate-authentication-TLV"
        };
        static_assert(std::size(AuthMutationNames) == static_cast<size_t>(AuthMutation::Count));

        size_t mutate_authentication(uint8_t *message, size_t length, AuthMutation mutation)
        {
            const size_t body = length - AuthTlvSize;
            switch (mutation)
            {
            case AuthMutation::UnknownKey: message[body + 6] ^= 0x80; break;
            case AuthMutation::WrongKey: break; // Signed with a different value by the caller.
            case AuthMutation::Body: message[34] ^= 1; break;
            case AuthMutation::Icv: message[length - 1] ^= 1; break;
            case AuthMutation::Spp: message[body + 4] ^= 1; break;
            case AuthMutation::Indicators: message[body + 5] = 1; break;
            case AuthMutation::TlvType: message[body + 1] ^= 1; break;
            case AuthMutation::TlvLength: message[body + 3] -= 2; break;
            case AuthMutation::Truncated: length -= 2; break;
            case AuthMutation::Duplicate:
                std::memcpy(message + length, message + body, AuthTlvSize);
                length += AuthTlvSize;
                write16(message + 2, static_cast<uint16_t>(length));
                break;
            case AuthMutation::Count: break;
            }
            return length;
        }

        constexpr AuthMutation KeylessProbes[] = {
            AuthMutation::Count, AuthMutation::UnknownKey, AuthMutation::Body, AuthMutation::Icv,
            AuthMutation::Spp, AuthMutation::Indicators, AuthMutation::TlvType,
            AuthMutation::TlvLength, AuthMutation::Truncated, AuthMutation::Duplicate
        };

        bool sign_rejection_probe(uint8_t *message, size_t capacity, size_t &length,
                                  const Authenticator &auth, AuthMutation mutation)
        {
            if (length > capacity || capacity - length < 2 * AuthTlvSize)
                return false;
            promote_to_v21(message);
            if (!auth.append(message, capacity, length))
                return false;
            length = mutate_authentication(message, length, mutation);
            return true;
        }

        bool make_invalid_announce_update(uint8_t *frame, size_t capacity, size_t &length,
                                          uint16_t sequence, const Authenticator &auth, AuthMutation mutation)
        {
            if (frame == nullptr || capacity < 14 + 64 + 2 * AuthTlvSize)
                return false;
            write16(frame + 44, sequence);
            length = 64;
            if (!sign_rejection_probe(frame + 14, capacity - 14, length, auth, mutation))
                return false;
            length += 14;
            return true;
        }

#ifndef PTP_TESTER_HOST_TEST
        constexpr UBaseType_t QueueCapacity = 8;
        constexpr uint32_t MaxTimeoutMs = 120000;
        StaticQueue_t s_queue_control{};
        alignas(Packet) uint8_t s_queue_storage[QueueCapacity * sizeof(Packet)]{};
        QueueHandle_t s_queue = nullptr;
        StaticSemaphore_t s_run_mutex_storage{};
        SemaphoreHandle_t s_run_mutex = nullptr;
        esp_eth_handle_t s_ethernet = nullptr;
        esp_netif_t *s_netif = nullptr;
        uint8_t s_local_mac[6] = {};
        uint8_t s_local_identity[10] = {};
        std::atomic<bool> s_ready{false};
        std::atomic<bool> s_active{false};
        std::atomic<uint32_t> s_target_prefix{0};
        std::atomic<uint16_t> s_target_suffix{0};
        std::atomic<bool> s_link_up{false};
        std::atomic<uint32_t> s_generation{0};
        std::atomic<uint32_t> s_drops{0};
        esp_event_handler_instance_t s_event_instance = nullptr;
        uint16_t s_sequence = 0;
        uint64_t s_request_nonce = 0;
        CleanupLease s_foreign_cleanup{};
        RotationRestoreLease s_rotation_restore{};
        RotationRestoreLease s_startup_restore{};
        RotationRestoreLease s_timing_restore{};

        void ethernet_event(void *, esp_event_base_t, int32_t event_id, void *data)
        {
            if (data == nullptr || *static_cast<esp_eth_handle_t *>(data) != s_ethernet)
                return;
            if (event_id == ETHERNET_EVENT_CONNECTED)
            {
                s_generation.fetch_add(1);
                s_link_up.store(true);
            }
            else if (event_id == ETHERNET_EVENT_DISCONNECTED || event_id == ETHERNET_EVENT_STOP)
            {
                s_link_up.store(false);
                s_generation.fetch_add(1);
            }
        }

        esp_err_t ethernet_input(esp_eth_handle_t, uint8_t *buffer, uint32_t length, void *priv, void *info)
        {
            size_t type_offset = 12;
            bool vlan = false;
            if (length >= 14)
            {
                for (size_t count = 0; count < 2 && length >= type_offset + 6; ++count)
                {
                    const uint16_t type = read16(buffer + type_offset);
                    if (type != 0x8100 && type != 0x88a8)
                        break;
                    vlan = true;
                    type_offset += 4;
                }
            }
            if (length < type_offset + 2 || read16(buffer + type_offset) != 0x88f7)
                return esp_netif_receive(static_cast<esp_netif_t *>(priv), buffer, length, nullptr);
            if (s_active.load() && mac_prefix(buffer + 6) == s_target_prefix.load() &&
                read16(buffer + 10) == s_target_suffix.load() &&
                (std::memcmp(buffer, MulticastMac, 6) == 0 || std::memcmp(buffer, s_local_mac, 6) == 0))
            {
                Packet packet{};
                packet.length = static_cast<uint16_t>(std::min<size_t>(length, MaxFrameSize));
                packet.oversized = length > MaxFrameSize;
                packet.unsupported_vlan = vlan;
                std::memcpy(packet.frame, buffer, packet.length);
                packet.received_us = esp_timer_get_time();
                packet.generation = s_generation.load();
#if SOC_EMAC_IEEE1588V2_SUPPORTED
                if (info != nullptr)
                {
                    const eth_mac_time_t timestamp = *static_cast<const eth_mac_time_t *>(info);
                    if ((timestamp.seconds != 0 || timestamp.nanoseconds != 0) && timestamp.nanoseconds < 1000000000U)
                    {
                        packet.hardware_timestamp = true;
                        packet.timestamp.seconds = timestamp.seconds;
                        packet.timestamp.nanoseconds = timestamp.nanoseconds;
                    }
                }
#else
                (void)info;
#endif
                if (xQueueSend(s_queue, &packet, 0) != pdTRUE)
                    s_drops.fetch_add(1);
            }
            std::free(buffer);
            return ESP_OK;
        }

        struct RunGuard
        {
            ~RunGuard()
            {
                s_active.store(false);
                xSemaphoreGive(s_run_mutex);
            }
        };

        void identity_text(const uint8_t *identity, char *output, size_t size)
        {
            std::snprintf(output, size, "%02x%02x%02x%02x%02x%02x%02x%02x/%u",
                          identity[0], identity[1], identity[2], identity[3], identity[4], identity[5], identity[6], identity[7],
                          static_cast<unsigned>(read16(identity + 8)));
        }

        void announce_text(const Message &message, char *output, size_t size, const char *transport = "L2 observer")
        {
            char identity[32];
            char offset[32];
            identity_text(message.source, identity, sizeof(identity));
            if ((message.flags & UtcValidFlag) != 0)
                std::snprintf(offset, sizeof(offset), "%d s (valid)", static_cast<int>(message.utc_offset));
            else
                std::snprintf(offset, sizeof(offset), "unavailable (invalid flag)");
            std::snprintf(output, size,
                          "%s; %s PTP v2.%u; source=%s; "
                          "GM=%02x%02x%02x%02x%02x%02x%02x%02x; priority=%u/%u; class=%u; accuracy=0x%02x; "
                          "variance=%u; steps=%u; timeSource=0x%02x; timescale=%s; UTC offset=%s; "
                          "leap61=%u; leap59=%u; timeTraceable=%u; frequencyTraceable=%u. "
                          "Quality is advertised, not independently verified.",
                          transport, message.authenticated ? "HMAC-SHA256-128 verified" : "unauthenticated",
                          static_cast<unsigned>(message.minor_version), identity,
                          message.grandmaster[0], message.grandmaster[1], message.grandmaster[2], message.grandmaster[3],
                          message.grandmaster[4], message.grandmaster[5], message.grandmaster[6], message.grandmaster[7],
                          message.priority1, message.priority2, message.clock_class, message.accuracy, message.variance,
                          message.steps, message.time_source, (message.flags & PtpTimescaleFlag) != 0 ? "PTP/TAI" : "arbitrary",
                          offset, (message.flags & 1) != 0, (message.flags & 2) != 0,
                          (message.flags & 0x10) != 0, (message.flags & 0x20) != 0);
        }
        struct Evidence
        {
            char *output;
            size_t capacity;
            bool truncated = false;

            void add(const char *format, ...)
            {
                const size_t used = std::strlen(output);
                va_list arguments;
                va_start(arguments, format);
                const int written = std::vsnprintf(output + used, capacity - used, format, arguments);
                va_end(arguments);
                truncated = truncated || written < 0 || static_cast<size_t>(written) >= capacity - used;
            }
        };

        struct Session
        {
            const Config &config;
            const std::atomic<bool> &stop;
            Authenticator auth{};
            uint32_t generation = s_generation.load();
            uint32_t drops = s_drops.load();
            int64_t start_us = esp_timer_get_time();
            Message master{};
            ServiceAuthentication service_authentication{};
            bool interrupted = false;
            bool protocol_fault = false;
            const char *reason = "completed";
            Packet deferred{};
            bool have_deferred = false;
            int64_t last_activity_us = 0;
            int64_t first_activity_us = 0;

            bool healthy()
            {
                if (stop.load() || !s_link_up.load() || s_generation.load() != generation || s_drops.load() != drops)
                {
                    interrupted = true;
                    reason = stop.load() ? "cancelled" : s_drops.load() != drops ? "tester RX queue overflow" : "tester link/generation changed";
                    return false;
                }
                return true;
            }

            void progress(const char *text)
            {
                if (config.progress != nullptr)
                    config.progress(text, config.context);
            }

            void rebase()
            {
                Packet packet{};
                while (xQueueReceive(s_queue, &packet, 0) == pdTRUE)
                {
                }
                have_deferred = false;
                generation = s_generation.load();
                start_us = esp_timer_get_time();
            }

            bool pull(Packet &packet, int64_t deadline_us)
            {
                while (healthy() && esp_timer_get_time() < deadline_us)
                {
                    if (have_deferred)
                    {
                        packet = deferred;
                        have_deferred = false;
                    }
                    else if (xQueueReceive(s_queue, &packet, std::max<TickType_t>(1, pdMS_TO_TICKS(10))) != pdTRUE)
                        continue;
                    if (packet.generation != generation || packet.received_us < start_us || packet.length < 14 ||
                        std::memcmp(packet.frame + 6, config.target_mac, 6) != 0)
                        continue;
                    if (packet.received_us >= deadline_us)
                    {
                        deferred = packet;
                        have_deferred = true;
                        return false;
                    }
                    return healthy();
                }
                return false;
            }

            bool decode(const Packet &packet, Message &message)
            {
                if (packet.unsupported_vlan || packet.oversized)
                {
                    protocol_fault = true;
                    reason = "target emitted unsupported VLAN/oversized PTP";
                    return false;
                }
                const ParseResult parsed = parse_config_message(packet, config, auth, message);
                if (parsed == ParseResult::Ignore)
                    return false;
                if (parsed == ParseResult::AuthenticationUnavailable || parsed == ParseResult::AuthenticationUnsupported)
                {
                    interrupted = true;
                    reason = signed_policy(config)
                                 ? "PTP verification unavailable; check the target profile's PTP key and crypto support"
                                 : "target emitted an authentication TLV while DUT policy declares unsigned PTP service; verify the DUT firmware and configured policy match";
                    return false;
                }
                if (parsed == ParseResult::AuthenticationInvalid)
                {
                    if (!signed_policy(config))
                    {
                        interrupted = true;
                        reason = "target emitted an invalid authentication TLV while DUT policy declares unsigned PTP service; verify the DUT firmware and configured policy match";
                    }
                    else
                    {
                        protocol_fault = true;
                        reason = "DUT authentication missing/invalid for the configured PTP key/SPP; verify the fixture key before attributing a firmware failure";
                    }
                    return false;
                }
                if (parsed == ParseResult::Valid)
                    if (const char *issue = wire_policy_error(config, message))
                    {
                        interrupted = true;
                        reason = issue;
                        return false;
                    }
                if (parsed != ParseResult::Valid || !restricted_output(packet, message))
                {
                    protocol_fault = true;
                    reason = "target message violates restricted version/length/flags/zero-correction wire policy";
                    return false;
                }
                if (const char *issue = service_authentication.observe(message))
                {
                    protocol_fault = true;
                    reason = issue;
                    return false;
                }
                if (message.type == 11 && !supported_properties(message))
                {
                    protocol_fault = true;
                    reason = "unsupported time-property claims; expected conservative arbitrary or GNSS-derived PTP/TAI timescale";
                    return false;
                }
                if (!has_required_rx_timestamp(message, packet))
                {
                    interrupted = true;
                    reason = "selected DUT Sync hardware RX timestamp unavailable";
                    return false;
                }
                if (valid_port_identity(master.source) &&
                    (std::memcmp(master.source, message.source, 10) != 0 ||
                     (message.type == 11 && (compare_rank(message, master).order != 0 ||
                      message.flags != master.flags || message.utc_offset != master.utc_offset ||
                      message.minor_version != master.minor_version || message.steps != master.steps ||
                      message.log_interval != master.log_interval || message.time_source != master.time_source))))
                {
                    interrupted = true;
                    reason = "selected DUT source/dataset/time properties changed; stable baseline no longer available";
                    return false;
                }
                last_activity_us = packet.received_us;
                if (first_activity_us == 0)
                    first_activity_us = packet.received_us;
                return true;
            }

            bool learn(uint32_t timeout_ms)
            {
                progress("Waiting for a fresh supported Announce and qualified service");
                const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(timeout_ms) * 1000;
                Packet packet{};
                Message message{};
                while (pull(packet, deadline))
                {
                    if (decode(packet, message) && message.type == 11)
                    {
                        master = message;
                        if (config.target_identity != nullptr)
                        {
                            char text[768];
                            announce_text(master, text, sizeof(text));
                            config.target_identity(text, config.context);
                        }
                        return true;
                    }
                    if (interrupted || protocol_fault)
                        return false;
                }
                if (!interrupted && !protocol_fault)
                    reason = "no fresh healthy Announce control; verify target MAC/domain, DUT GNSS/PPS qualification and Ethernet PTP delivery";
                return false;
            }

            void requester(uint8_t *identity)
            {
                std::memcpy(identity, s_local_identity, 10);
                const uint64_t nonce = ++s_request_nonce;
                identity[0] ^= 0x80;
                for (size_t index = 0; index < 6; ++index)
                    identity[7 - index] ^= static_cast<uint8_t>(nonce >> (index * 8));
            }

            bool send(const uint8_t *frame, size_t length, Timestamp *timestamp = nullptr, bool authenticate = true)
            {
                if (!healthy())
                    return false;
                uint8_t signed_frame[MaxFrameSize] = {};
                if (authenticate && service_authentication.authenticated)
                {
                    if (length < 48 || length > sizeof(signed_frame))
                    {
                        interrupted = true;
                        reason = "invalid PTP signing frame length";
                        return false;
                    }
                    std::memcpy(signed_frame, frame, length);
                    promote_to_v21(signed_frame + 14);
                    size_t message_length = read16(signed_frame + 16);
                    if (message_length > length - 14 || !auth.append(signed_frame + 14, sizeof(signed_frame) - 14, message_length))
                    {
                        interrupted = true;
                        reason = "PTP request signing failed";
                        return false;
                    }
                    if (auth.verify(signed_frame + 14, message_length) != AuthResult::Valid)
                    {
                        interrupted = true;
                        reason = "PTP request signature self-verification failed";
                        return false;
                    }
                    frame = signed_frame;
                    length = message_length + 14;
                }
                esp_err_t error;
                bool hardware = false;
#if SOC_EMAC_IEEE1588V2_SUPPORTED
                eth_mac_time_t hardware_time{};
                error = esp_eth_transmit_ctrl_vargs(s_ethernet, timestamp == nullptr ? nullptr : &hardware_time,
                                                    2, const_cast<uint8_t *>(frame), static_cast<uint32_t>(length));
                if (timestamp != nullptr && error == ESP_OK &&
                    (hardware_time.seconds != 0 || hardware_time.nanoseconds != 0) && hardware_time.nanoseconds < 1000000000U)
                {
                    *timestamp = {hardware_time.seconds, hardware_time.nanoseconds};
                    hardware = true;
                }
#else
                error = esp_eth_transmit(s_ethernet, const_cast<uint8_t *>(frame), static_cast<uint32_t>(length));
#endif
                if (error != ESP_OK || (timestamp != nullptr && !hardware))
                {
                    interrupted = true;
                    reason = error != ESP_OK ? "tester transmit failed" : "required hardware TX timestamp unavailable";
                    return false;
                }
                return healthy();
            }

            bool control(uint16_t port = 1, const uint8_t *clock = nullptr)
            {
                uint8_t identity[10];
                if (clock != nullptr)
                    std::memcpy(identity, clock, 10);
                else
                    requester(identity);
                write16(identity + 8, port);
                uint8_t frame[60];
                const uint16_t sequence = ++s_sequence;
                make_delay_request(frame, s_local_mac, identity, config.domain, 0, sequence);
                Timestamp transmit{};
                const int64_t sent_us = esp_timer_get_time();
                if (!send(frame, sizeof(frame), &transmit))
                    return false;
                const int64_t deadline = sent_us + static_cast<int64_t>(config.response_window_ms) * 1000;
                Packet packet{};
                Message message{};
                while (pull(packet, deadline))
                {
                    if (packet.received_us >= sent_us && decode(packet, message) &&
                        match_delay_response(message, master.source, master.minor_version, sequence, identity))
                        return healthy();
                }
                reason = interrupted ? reason : "positive Delay control did not respond";
                return false;
            }
        };

        Result observe(Session &session, Evidence &evidence, bool notify_observation = false)
        {
            auto observation = std::unique_ptr<Observation>(new (std::nothrow) Observation{});
            if (!observation)
            {
                evidence.add("Observation allocation unavailable. ");
                return Result::CouldNotRun;
            }
            observation->generation = session.generation;
            int64_t sent_us = 0;
            uint16_t sequence = 0;
            uint8_t requester[10] = {};
            Pair requested_pair{};
            bool outstanding = false;
            Timestamp transmit{};
            Packet packet{};
            Message message{};
            session.progress("Collecting distinct hardware Sync/Follow_Up pairs and bounded E2E exchanges");
            if (!session.healthy())
                return Result::CouldNotRun;
            if (notify_observation && session.config.observation != nullptr)
                session.config.observation(true, session.config.context);
            const int64_t start_us = esp_timer_get_time();
            const int64_t deadline = start_us + static_cast<int64_t>(session.config.observation_ms) * 1000;
            const int64_t drain_deadline = deadline + static_cast<int64_t>(session.config.response_window_ms) * 1000;
            int64_t next_request_us = start_us;
            while (session.healthy() && esp_timer_get_time() < drain_deadline)
            {
                const int64_t now_us = esp_timer_get_time();
                observation->expire(now_us);
                if (outstanding && now_us - sent_us >= static_cast<int64_t>(session.config.response_window_ms) * 1000)
                {
                    outstanding = false;
                    ++observation->response_loss;
                }
                if (!session.pull(packet, std::min(drain_deadline, now_us + 20000)))
                    continue;
                if (packet.received_us < start_us)
                    continue;
                if (!session.decode(packet, message))
                    continue;
                if (std::memcmp(message.source, session.master.source, 10) != 0)
                {
                    ++observation->unmatched;
                    continue;
                }
                if (message.type == 11)
                    continue;
                if (message.type == 0 || message.type == 8)
                {
                    if (message.type == 0 && (message.flags & TwoStepFlag) == 0)
                    {
                        session.protocol_fault = true;
                        session.reason = "one-step Sync in two-step service";
                        continue;
                    }
                    if (packet.received_us >= deadline)
                    {
                        if (message.type == 0)
                            continue;
                        const bool pending = std::any_of(observation->pending.begin(), observation->pending.end(),
                            [&message](const Pair &pair) { return pair.occupied && pair.sequence == message.sequence; });
                        if (!pending)
                            continue;
                    }
                    Pair completed{};
                    if (!observation->accept(message, packet, completed))
                        continue;
                    if (!completed.reception.hardware_timestamp)
                    {
                        ++observation->timestamp_failures;
                        continue;
                    }
                    if (!outstanding && packet.received_us < deadline && esp_timer_get_time() >= next_request_us)
                    {
                        uint8_t frame[60];
                        session.requester(requester);
                        sequence = ++s_sequence;
                        make_delay_request(frame, s_local_mac, requester, session.config.domain, 0, sequence);
                        requested_pair = completed;
                        sent_us = esp_timer_get_time();
                        if (!session.send(frame, sizeof(frame), &transmit))
                        {
                            ++observation->timestamp_failures;
                            break;
                        }
                        next_request_us = sent_us + static_cast<int64_t>(session.config.request_interval_ms) * 1000;
                        ++observation->requests;
                        outstanding = true;
                    }
                }
                else if (message.type == 9)
                {
                    if (!outstanding || !in_window(packet.received_us, sent_us,
                            sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000) ||
                        !match_delay_response(message, session.master.source, session.master.minor_version, sequence, requester))
                    {
                        ++observation->unmatched;
                        continue;
                    }
                    outstanding = false;
                    ++observation->responses;
                    Measurement measurement{};
                    if (!calculate_measurement(requested_pair, transmit, message, measurement))
                    {
                        ++observation->unusable;
                        continue;
                    }
                    observation->delay.add(measurement.mean_path_delay_ns / 1000);
                    observation->offset.add(measurement.local_minus_master_ns / 1000);
                }
            }
            if (notify_observation && session.config.observation != nullptr)
                session.config.observation(false, session.config.context);
            observation->response_loss += outstanding;
            observation->expire(esp_timer_get_time(), true);
            session.healthy();
            const uint32_t drops = s_drops.load() - session.drops;
            evidence.add("window=%u ms+%u ms drain; pairs=%u; requests/responses=%u/%u; samples=%u/%u; "
                         "duplicate=%u unmatched=%u expired=%u out-of-order=%u missing=%u sequence-gaps=%u response-loss=%u; "
                         "hardware Sync RX/Delay_Req TX required; timestamp-failures=%u unusable=%u RX-drops=%u; "
                         "path us min/mean/max/sd=%.3f/%.3f/%.3f/%.3f; raw local-minus-master arbitrary us min/mean/max=%.3f/%.3f/%.3f. "
                         "PHC not disciplined; no UTC conversion or accuracy claim. %s. ",
                         session.config.observation_ms, session.config.response_window_ms, observation->pairs,
                         observation->requests, observation->responses, observation->delay.count, session.config.samples,
                         observation->duplicates, observation->unmatched, observation->expired, observation->out_of_order,
                         observation->missing, observation->sequence_gaps, observation->response_loss,
                         observation->timestamp_failures, observation->unusable, drops,
                         static_cast<double>(observation->delay.minimum), static_cast<double>(observation->delay.mean),
                         static_cast<double>(observation->delay.maximum), static_cast<double>(observation->delay.deviation()),
                         static_cast<double>(observation->offset.minimum), static_cast<double>(observation->offset.mean),
                         static_cast<double>(observation->offset.maximum), session.reason);
            return classify_observation(*observation, session.config.samples, session.interrupted, drops, session.protocol_fault);
        }

        Result negative_delays(Session &session, Evidence &evidence)
        {
            Result overall = Result::Pass;
            const bool authenticated = session.service_authentication.authenticated;
            session.progress(authenticated ? "Authenticated PTP-field negative Delay_Req cases; each has signed before/after controls"
                                           : "Isolated negative Delay_Req cases; each has before/after positive controls");
            for (size_t index = 0; index < static_cast<size_t>(Mutation::Count); ++index)
            {
                const Mutation mutation = static_cast<Mutation>(index);
                if (authenticated && !authenticated_mutation_supported(mutation))
                {
                    evidence.add("%s:not-covered-by-authenticated-variant; ", MutationNames[index]);
                    continue;
                }
                uint8_t identity[10];
                session.requester(identity);
                write16(identity + 8, 1);
                const bool before = session.control(1, mutation == Mutation::SecondPort ? identity : nullptr);
                if (!before)
                {
                    evidence.add("%s: CouldNotRun, before-control unavailable (%s). ", MutationNames[index], session.reason);
                    return Result::CouldNotRun;
                }
                std::array<uint8_t, MaxFrameSize> frame{};
                const uint16_t sequence = ++s_sequence;
                make_delay_request(frame.data(), s_local_mac, identity, session.config.domain, authenticated ? 1 : 0, sequence);
                const size_t length = mutate_request(frame.data(), mutation, session.master.source);
                const uint8_t minor_version = frame[15] >> 4;
                const bool expected = mutation_expects_response(mutation, minor_version);
                const size_t message_offset = mutation == Mutation::Vlan ? 18 : 14;
                std::memcpy(identity, frame.data() + message_offset + 20, 10);
                Timestamp transmit{};
                const int64_t sent_us = esp_timer_get_time();
                if (!session.send(frame.data(), length, expected ? &transmit : nullptr))
                {
                    evidence.add("%s: CouldNotRun (%s). ", MutationNames[index], session.reason);
                    return Result::CouldNotRun;
                }
                bool response = false;
                bool verified = false;
                Packet packet{};
                const int64_t deadline = sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000;
                while (session.pull(packet, deadline))
                {
                    if (packet.received_us < sent_us || !raw_delay_match(packet, session.master.source, sequence, identity))
                        continue;
                    if (!expected)
                    {
                        response = true;
                        continue;
                    }
                    Message message{};
                    if (!session.decode(packet, message) ||
                        !match_delay_response(message, session.master.source, session.master.minor_version, sequence, identity))
                        continue;
                    response = true;
                    verified = message.authenticated;
                }
                const bool after = session.control(1, mutation == Mutation::SecondPort ? identity : nullptr);
                const Result result = authenticated
                                          ? classify_authenticated_probe(before, after, response, verified, expected,
                                                                         session.interrupted, s_drops.load() - session.drops,
                                                                         session.protocol_fault)
                                          : classify_negative(before, after, response, expected, session.interrupted,
                                                              s_drops.load() - session.drops);
                evidence.add("%s:%s(minor=%u expected-response=%u before=%u response=%u response-ICV-verified=%u after=%u); ",
                             MutationNames[index], result == Result::Pass ? "pass" : result == Result::Fail ? "FAIL" : "CouldNotRun",
                             minor_version, expected, before, response, verified, after);
                if (result == Result::CouldNotRun)
                    return result;
                if (result == Result::Fail)
                    overall = result;
            }
            if (authenticated)
                evidence.add("Authenticated PTP-field probes were signed and self-verified before transmission; "
                             "version, TLV/declared-length, VLAN and Ethernet-padding cases are not covered because they cannot "
                             "retain a valid Immediate Authentication message. RX-drops=%u. ", s_drops.load() - session.drops);
            else
                evidence.add("Transport/short-frame cases are filtering observations, not parser-proven rejection; "
                             "short cases mutate declared PTP lengths, not physical Ethernet runt/FCS; packet generator required for those. "
                             "TLV rejection is not authentication validation. RX-drops=%u. ", s_drops.load() - session.drops);
            return session.protocol_fault && overall == Result::Pass ? Result::Fail : overall;
        }

        struct ForeignStream
        {
            uint8_t frame[78] = {};
            AnnounceSequence sequences{};
            int64_t interval = 2000000;
            int64_t next_us = 0;
            bool duplicate_refresh = false;
            bool authenticate = true;
            bool valid = false;
            ForeignSchedule schedule;
            Evidence &evidence;
            const char *label;

            ForeignStream(Session &session, const Message &dataset, const uint8_t *identity,
                          Evidence &output, const char *phase, int8_t log_interval = 1, bool sign = true)
                : schedule{session.config.policy.selection, candidate_lifetime_us(session.config.policy, log_interval),
                           static_cast<int64_t>(session.config.processing_allowance_ms) * 1000},
                  evidence(output), label(phase)
            {
                authenticate = sign;
                sequences.value = session.config.sequence_seed_enabled ? session.config.sequence_seed : 0;
                const bool supported_interval = interval_us(log_interval, interval);
                if (session.config.policy.selection == SelectionMode::Legacy)
                    interval = std::min<int64_t>(interval, schedule.lifetime_us / 3);
                make_foreign_announce(frame, s_local_mac, identity, session.config.domain, sequences.value);
                set_foreign_dataset(frame, dataset, log_interval);
                valid = supported_interval && valid_port_identity(identity) && valid_identity(dataset.grandmaster) &&
                        std::memcmp(identity, session.master.source, 10) != 0;
                char source[32];
                identity_text(identity, source, sizeof(source));
                const Rank rank = compare_rank(dataset, session.master);
                evidence.add("\n%s: synthetic test candidate source=%s GM=%02x%02x%02x%02x%02x%02x%02x%02x; "
                             "priority=%u/%u class=%u accuracy=0x%02x variance=%u; rank=%d deciding=%s; "
                             "logInterval=%d actual refresh=%lld us lifetime=%lld us allowance=%u ms. ",
                             label, source, dataset.grandmaster[0], dataset.grandmaster[1], dataset.grandmaster[2],
                             dataset.grandmaster[3], dataset.grandmaster[4], dataset.grandmaster[5],
                             dataset.grandmaster[6], dataset.grandmaster[7], dataset.priority1, dataset.priority2,
                             dataset.clock_class, dataset.accuracy, dataset.variance, rank.order, rank.field, log_interval,
                             static_cast<long long>(interval),
                             static_cast<long long>(schedule.lifetime_us), session.config.processing_allowance_ms);
            }

            bool send(Session &session, bool duplicate = false, bool record_evidence = true)
            {
                return send_sequence(session, duplicate ? sequences.value : static_cast<uint16_t>(sequences.value + 1),
                                     true, record_evidence);
            }

            bool send_sequence(Session &session, uint16_t sequence, bool accepted = true, bool record_evidence = true)
            {
                if (!valid)
                {
                    session.interrupted = true;
                    session.reason = "synthetic transmitter identity invalid or collides with selected DUT source";
                    return false;
                }
                write16(frame + 44, sequence);
                const int64_t begin = esp_timer_get_time();
                const bool sent = session.send(frame, sizeof(frame), nullptr, authenticate);
                const int64_t end = esp_timer_get_time();
                // A failed/ interrupted transmit may still have reached the DUT.
                s_foreign_cleanup.attempted(session.config, end, schedule.lifetime_us, schedule.allowance_us);
                if (!sent)
                {
                    evidence.add("TX sequence=%u attempted=[%lld,%lld] us not confirmed: %s; possible candidate lease retained for cleanup. ",
                                 sequence, static_cast<long long>(begin), static_cast<long long>(end), session.reason);
                    return false;
                }
                const bool fresh = accepted && sequences.record(sequence, schedule.mode, schedule.fresh_sends == 0);
                schedule.sent(begin, end, fresh);
                next_us = end + interval;
                if (record_evidence)
                    evidence.add("TX sequence=%u%s successful=[%lld,%lld] us; ", sequence,
                                 !accepted ? " (unsupported interval; must not qualify/refresh)" :
                                 fresh ? " (fresh)" : " (duplicate/backward/half-range)",
                                 static_cast<long long>(begin), static_cast<long long>(end));
                if (end - begin > schedule.allowance_us)
                {
                    session.interrupted = true;
                    session.reason = "successful TX duration exceeds configured delivery/processing uncertainty; timing observation inconclusive";
                    return false;
                }
                return true;
            }
        };

        bool service_window(Session &session, int64_t deadline, int64_t silence_after,
                            bool require_activity, ForeignStream *foreign = nullptr, bool require_announce = true)
        {
            if (esp_timer_get_time() >= deadline)
            {
                session.interrupted = true;
                session.reason = "tester missed the service-observation window; timing is inconclusive";
                return false;
            }
            bool announce = false;
            bool paired = false;
            bool delay = false;
            bool outstanding = false;
            int64_t next_probe = esp_timer_get_time();
            std::array<Pair, 4> pairs{};
            uint16_t sequence = 0;
            uint8_t identity[10] = {};
            int64_t sent_us = 0;
            Packet packet{};
            Message message{};
            while (session.healthy() && esp_timer_get_time() < deadline)
            {
                const int64_t now_us = esp_timer_get_time();
                if (foreign != nullptr && now_us >= foreign->next_us)
                {
                    if (!foreign->send(session, foreign->duplicate_refresh))
                        return false;
                }
                if (require_activity && outstanding &&
                    now_us - sent_us >= static_cast<int64_t>(session.config.response_window_ms) * 1000)
                {
                    session.reason = "healthy service continuity lost: periodic Delay control expired";
                    return false;
                }
                if (now_us >= next_probe && (!require_activity || (!outstanding &&
                    deadline - now_us > static_cast<int64_t>(session.config.response_window_ms) * 1000)))
                {
                    uint8_t frame[60];
                    session.requester(identity);
                    sequence = ++s_sequence;
                    make_delay_request(frame, s_local_mac, identity, session.config.domain, 0, sequence);
                    sent_us = now_us;
                    Timestamp transmit{};
                    if (!session.send(frame, sizeof(frame), &transmit))
                        return false;
                    outstanding = true;
                    next_probe = now_us + static_cast<int64_t>(std::max(session.config.request_interval_ms,
                                                                 session.config.response_window_ms)) * 1000;
                }
                if (!session.pull(packet, std::min(deadline, now_us + 20000)))
                    continue;
                if (!session.decode(packet, message))
                {
                    if (session.protocol_fault || session.interrupted)
                        return false;
                    continue;
                }
                const bool matched_delay = outstanding && in_window(packet.received_us, sent_us,
                    sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000) &&
                    match_delay_response(message, session.master.source, session.master.minor_version, sequence, identity);
                if (matched_delay)
                    outstanding = false;
                if ((message.type == 11 || message.type == 0 || message.type == 8 || message.type == 9) &&
                    packet.received_us >= silence_after && !require_activity)
                {
                    session.reason = "service transmitted inside required withdrawal/quiet window";
                    return false;
                }
                announce = announce || message.type == 11;
                if (message.type == 0 || message.type == 8)
                {
                    Pair completed{};
                    paired = match_pair(pairs, message, packet, completed) || paired;
                }
                delay = delay || matched_delay;
            }
            if (require_activity && ((require_announce && (!announce || !paired)) || !delay || outstanding))
            {
                session.reason = "healthy service continuity/control unavailable";
                return false;
            }
            return session.healthy() && !session.protocol_fault;
        }

        int64_t healthy_window_us(const Config &config)
        {
            int64_t announce = 0;
            int64_t sync = 0;
            interval_us(config.policy.log_announce_interval, announce);
            interval_us(config.policy.log_sync_interval, sync);
            return std::max(announce * 2, sync * 2) +
                   static_cast<int64_t>(config.response_window_ms + config.processing_allowance_ms) * 1000;
        }

        bool recover_foreign(Session &session, Evidence &evidence, int64_t deadline)
        {
            session.rebase();
            session.first_activity_us = 0;
            const int64_t remaining = deadline - esp_timer_get_time();
            if (remaining <= 0 || !session.learn(static_cast<uint32_t>((remaining + 999) / 1000)) || !session.control())
            {
                evidence.add("Healthy recovery unavailable: %s. ", session.reason);
                return false;
            }
            const bool healthy = service_window(session, std::min(deadline, esp_timer_get_time() +
                                                 healthy_window_us(session.config)), 0, true);
            evidence.add("Recovered fresh Announce+two-step+matching Delay=%u; first/last activity=%lld/%lld us. ",
                         healthy, static_cast<long long>(session.first_activity_us),
                         static_cast<long long>(session.last_activity_us));
            return healthy;
        }

        bool cleanup_foreign(Session &original, Evidence &evidence)
        {
            if (!s_foreign_cleanup.pending)
                return true;
            if (!s_foreign_cleanup.matches(original.config))
            {
                evidence.add("Cleanup remains unresolved for previous DUT %02x:%02x:%02x:%02x:%02x:%02x domain=%u; "
                             "select that target and rerun the foreign-master policy and recovery test first. ",
                             s_foreign_cleanup.target[0], s_foreign_cleanup.target[1], s_foreign_cleanup.target[2],
                             s_foreign_cleanup.target[3], s_foreign_cleanup.target[4], s_foreign_cleanup.target[5],
                             s_foreign_cleanup.domain);
                return false;
            }
            // Cancellation stops traffic and leaves the lease pending; dependent
            // tests remain gated until expiry and healthy recovery are verified.
            Session cleanup{original.config, original.stop};
            if (!cleanup.healthy())
            {
                evidence.add("Cleanup deferred: %s; foreign traffic stopped, dependent PTP tests remain blocked. ",
                             cleanup.reason);
                return false;
            }
            cleanup.service_authentication = original.service_authentication;
            if (signed_policy(original.config) && configured_key(original.config) &&
                !cleanup.auth.initialize(original.config.key_id, original.config.key_value, original.config.policy.spp))
            {
                evidence.add("Cleanup PTP key import unavailable; dependent tests remain blocked. ");
                return false;
            }
            cleanup.rebase();
            cleanup.progress("Foreign traffic stopped: bounded candidate-expiry and healthy-service cleanup");
            Packet packet{};
            evidence.add("\nCleanup: injections stopped; waiting through monotonic %lld us (cancellation retains the cleanup blocker). ",
                         static_cast<long long>(s_foreign_cleanup.deadline_us));
            while (cleanup.healthy() && esp_timer_get_time() < s_foreign_cleanup.deadline_us)
                cleanup.pull(packet, std::min(s_foreign_cleanup.deadline_us, esp_timer_get_time() + 20000));
            const bool recovered = cleanup.healthy() && recover_foreign(cleanup, evidence,
                esp_timer_get_time() + static_cast<int64_t>(cleanup.config.recovery_ms) * 1000 +
                healthy_window_us(cleanup.config));
            evidence.add("cleanup healthy=%u%s. ", recovered,
                         recovered ? "" : "; dependent PTP tests blocked until a fresh healthy cleanup succeeds");
            s_foreign_cleanup.completed(recovered);
            return recovered;
        }

        Result foreign_suppression(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                const int64_t healthy_us = healthy_window_us(session.config);
                const bool ranked = session.config.policy.selection == SelectionMode::Ranked;
                const auto healthy = [&]()
                {
                    return service_window(session, esp_timer_get_time() + healthy_us, 0, true) && session.control();
                };
                if (!healthy())
                {
                    evidence.add("Stable healthy Announce/two-step/Delay baseline unavailable: %s. ", session.reason);
                    return Result::CouldNotRun;
                }
                char baseline[768];
                announce_text(session.master, baseline, sizeof(baseline));
                evidence.add("\nBaseline: %s ", baseline);
                for (unsigned variant = 0; variant < 2; ++variant)
                {
                    uint8_t identity[10];
                    session.requester(identity);
                    Message dataset = session.master;
                    std::memcpy(dataset.grandmaster, identity, 8);
                    if (ranked && !ranked_source_dataset(session.master, true, identity, dataset))
                    {
                        evidence.add("Rejected-Announce control needs a demonstrably better non-self candidate. ");
                        return Result::CouldNotRun;
                    }
                    ForeignStream rejected(session, dataset, identity, evidence, variant == 0 ? "wrong-domain" : "malformed");
                    // A broken DUT might accept a rejected control; cleanup must
                    // therefore account for those sends too.
                    if (variant == 0)
                        rejected.frame[18] = static_cast<uint8_t>(session.config.domain + 1);
                    else
                        rejected.frame[60] = 1;
                    session.progress(variant == 0 ? "Rejected wrong-domain Announce controls" : "Rejected malformed Announce controls");
                    const bool active = service_window(session, esp_timer_get_time() + healthy_us, 0, true, &rejected);
                    const bool after = session.healthy() && session.control();
                    evidence.add("activity=%u post-control=%u; %s. ", active, after, session.reason);
                    if (!active || !after)
                        return session.interrupted || !after ? Result::CouldNotRun : Result::Fail;
                }

                const auto cycle = [&](bool duplicates) -> Result
                {
                    uint8_t identity[10];
                    session.requester(identity);
                    Message candidate = session.master;
                    std::memcpy(candidate.grandmaster, identity, 8);
                    if (ranked && !ranked_source_dataset(session.master, true, identity, candidate))
                    {
                        evidence.add("No supported strictly better non-self dataset can be generated without underflow. ");
                        return Result::CouldNotRun;
                    }
                    ForeignStream stream(session, candidate, identity, evidence, duplicates ? "legacy duplicate-refresh" : "foreign policy");
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    if (ranked)
                    {
                        session.progress("Ranked qualification: first Announce must preserve healthy service");
                        if (!service_window(session, stream.next_us, 0, true, nullptr, false))
                        {
                            evidence.add("Single-Announce healthy service contradicted: %s. ", session.reason);
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                        }
                        if (!stream.send(session))
                            return Result::CouldNotRun;
                    }
                    stream.duplicate_refresh = duplicates;
                    PhaseEvidence phase{PolicyPhase::Refresh, stream.schedule.last_end_us,
                        stream.schedule.last_end_us + stream.schedule.lifetime_us * 2 + stream.interval +
                        stream.schedule.allowance_us, stream.schedule.last_end_us};
                    session.progress(ranked ? "Ranked standby: allowance starts at second fresh Announce; refreshing >2 lifetimes"
                                            : "Legacy suppression: allowance starts at first Announce; refreshing >2 quiet periods");
                    const bool held = service_window(session, phase.deadline_us, stream.schedule.silence_after_us, false, &stream);
                    phase.last_tx_us = stream.schedule.last_end_us;
                    phase.last_activity_us = session.last_activity_us;
                    phase.sends = stream.schedule.sends;
                    evidence.add("\nStandby phase=%u start/deadline=%lld/%lld us silence-after=%lld us sends=%u last-TX/activity=%lld/%lld us held=%u; %s. ",
                                 static_cast<unsigned>(phase.phase), static_cast<long long>(phase.start_us),
                                 static_cast<long long>(phase.deadline_us), static_cast<long long>(stream.schedule.silence_after_us),
                                 phase.sends, static_cast<long long>(phase.last_tx_us),
                                 static_cast<long long>(phase.last_activity_us), held, session.reason);
                    if (!held)
                        return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    if (!stream.send(session, duplicates))
                        return Result::CouldNotRun;
                    session.progress("No-early-service bound, then bounded schedule-aware healthy recovery");
                    const bool quiet = service_window(session, stream.schedule.no_early_service_until(),
                                                       stream.schedule.last_end_us, false);
                    evidence.add("Expiry reference=[%lld,%lld] us no-early-before=%lld us quiet=%u. ",
                                 static_cast<long long>(stream.schedule.last_begin_us),
                                 static_cast<long long>(stream.schedule.last_end_us),
                                 static_cast<long long>(stream.schedule.no_early_service_until()), quiet);
                    if (!quiet)
                        return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    const bool resumed = recover_foreign(session, evidence, stream.schedule.cleanup_until() +
                        static_cast<int64_t>(session.config.recovery_ms) * 1000 + healthy_us);
                    return resumed ? Result::Pass : Result::CouldNotRun;
                };
                const Result baseline_cycle = cycle(false);
                if (baseline_cycle != Result::Pass)
                    return baseline_cycle;
                if (!ranked)
                    return cycle(true);

                session.progress("Ranked policy: inferior Announce and identity tie-break controls");
                Message inferior{};
                Message identity_inferior{};
                if (!ranked_dataset(session.master, false, inferior) ||
                    !ranked_dataset(session.master, false, identity_inferior, true))
                {
                    evidence.add("Inferior or identity-only tie-break dataset unavailable at supported limits. ");
                    return Result::CouldNotRun;
                }
                for (const Message &dataset : {inferior, identity_inferior})
                {
                    uint8_t identity[10];
                    session.requester(identity);
                    Message distinct = dataset;
                    if (distinct.priority1 != session.master.priority1)
                        std::memcpy(distinct.grandmaster, identity, 8);
                    ForeignStream stream(session, distinct, identity, evidence, "inferior/non-winning identity tie-break");
                    if (!stream.send(session) ||
                        !service_window(session, stream.next_us, 0, true, nullptr, false) || !stream.send(session))
                        return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    if (!healthy())
                    {
                        evidence.add("Inferior master stopped healthy service: %s. ", session.reason);
                        return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    }
                }

                uint8_t identity[10];
                session.requester(identity);
                Message identity_better{};
                if (!ranked_dataset(session.master, true, identity_better, true))
                {
                    evidence.add("Winning identity-only tie-break cannot be generated at supported identity boundary. ");
                    return Result::CouldNotRun;
                }
                ForeignStream changed(session, identity_better, identity, evidence, "winning identity tie-break then no-longer-better");
                if (!changed.send(session) ||
                    !service_window(session, changed.next_us, 0, true, nullptr, false) || !changed.send(session))
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                const bool standby = service_window(session, changed.schedule.silence_after_us + 1000000,
                                                     changed.schedule.silence_after_us, false);
                if (!standby)
                {
                    evidence.add("Winning identity tie-break did not establish standby: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                const int64_t original_expiry = changed.schedule.no_early_service_until();
                std::memcpy(inferior.grandmaster, identity_better.grandmaster, 8);
                if (compare_rank(inferior, session.master).order <= 0)
                {
                    evidence.add("No-longer-better prerequisite could not be established. ");
                    return Result::CouldNotRun;
                }
                set_foreign_dataset(changed.frame, inferior, 1);
                evidence.add("\nFresh same-source no-longer-better update: deciding=%s; must recover before original expiry=%lld us. ",
                             compare_rank(inferior, session.master).field, static_cast<long long>(original_expiry));
                if (!changed.send(session))
                    return Result::CouldNotRun;
                session.rebase();
                const int64_t remaining_us = original_expiry - esp_timer_get_time();
                if (remaining_us <= 0 || !session.learn(static_cast<uint32_t>(remaining_us / 1000)) || !session.control())
                {
                    evidence.add("Candidate release not observed before old lifetime: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                if (!healthy())
                    return Result::CouldNotRun;
                evidence.add("Fresh inferior update released standby before original expiry; RX-drops=%u. "
                             "Election does not discipline either clock or establish full BMCA/synchronization accuracy. ",
                             s_drops.load() - session.drops);
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result ranking_precedence(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                const int64_t healthy_us = healthy_window_us(session.config);
                const auto healthy = [&]()
                {
                    return service_window(session, esp_timer_get_time() + healthy_us, 0, true) && session.control();
                };
                char baseline[768];
                announce_text(session.master, baseline, sizeof(baseline));
                evidence.add("\nRanking baseline: %s\n"
                             "Order: priority1, class, accuracy, variance, priority2, grandmaster identity. "
                             "Earlier fields tie; later fields oppose the deciding field wherever possible. "
                             "Both directions are attempted; impossible directions are N/A, not tested passes. ",
                             baseline);
                size_t passed = 0;
                size_t boundaries = 0;
                for (size_t field = 0; field < RankFieldCount; ++field)
                {
                    for (bool better : {true, false})
                    {
                        Message candidate{};
                        if (!precedence_dataset(session.master, field, better, candidate))
                        {
                            ++boundaries;
                            evidence.add("\n%s %s: N/A at advertised dataset's supported %s boundary; "
                                         "this direction requires a different DUT dataset. ",
                                         RankFields[field], better ? "winning" : "losing", better ? "minimum" : "maximum");
                            continue;
                        }
                        char label[96];
                        std::snprintf(label, sizeof(label), "Ranking %s %s", RankFields[field], better ? "winning" : "losing");
                        session.progress(label);
                        if (!healthy())
                        {
                            evidence.add("\n%s: stable healthy before-control unavailable: %s. ", label, session.reason);
                            return Result::CouldNotRun;
                        }
                        uint8_t identity[10];
                        session.requester(identity);
                        ForeignStream stream(session, candidate, identity, evidence, label);
                        if (!stream.send(session))
                            return Result::CouldNotRun;
                        if (!service_window(session, stream.next_us, 0, true, nullptr, false))
                        {
                            evidence.add("First fresh Announce must not qualify a candidate: %s. ", session.reason);
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                        }
                        if (!stream.send(session))
                            return Result::CouldNotRun;
                        const bool observed = better
                            ? service_window(session, stream.schedule.no_early_service_until(),
                                             stream.schedule.silence_after_us, false)
                            : service_window(session, esp_timer_get_time() + healthy_us, 0, true, &stream);
                        evidence.add("Expected=%s; observed=%u; silence-after=%lld us; no-early-before=%lld us; "
                                     "last-DUT-activity=%lld us; %s. ",
                                     better ? "standby until candidate expiry" : "continued healthy master service",
                                     observed, static_cast<long long>(stream.schedule.silence_after_us),
                                     static_cast<long long>(stream.schedule.no_early_service_until()),
                                     static_cast<long long>(session.last_activity_us), session.reason);
                        if (!observed)
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                        if (!cleanup_foreign(session, evidence))
                            return Result::CouldNotRun;
                        session.rebase();
                        if (!healthy())
                        {
                            evidence.add("Stable same-dataset after-control unavailable: %s. ", session.reason);
                            return Result::CouldNotRun;
                        }
                        ++passed;
                        evidence.add("%s: PASS with fresh healthy after-control. ", label);
                    }
                }
                evidence.add("\nRanking coverage: %u/%u feasible directions passed; %u/%u directions N/A at supported "
                             "dataset boundaries. PASS covers all feasible directions only, not a full bidirectional "
                             "matrix when N/A is nonzero. No full BMCA or synchronization-accuracy claim. ",
                             static_cast<unsigned>(passed), static_cast<unsigned>(RankFieldCount * 2 - boundaries),
                             static_cast<unsigned>(boundaries), static_cast<unsigned>(RankFieldCount * 2));
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result source_isolation(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                if (session.config.policy.selection != SelectionMode::Ranked)
                {
                    evidence.add("Source isolation requires ranked two-fresh-Announce qualification; "
                                 "legacy any-Announce suppression has no equivalent qualification boundary. ");
                    return Result::CouldNotRun;
                }

                const int64_t healthy_us = healthy_window_us(session.config);
                const auto healthy = [&]()
                {
                    return service_window(session, esp_timer_get_time() + healthy_us, 0, true) && session.control();
                };
                if (!healthy())
                {
                    evidence.add("Stable healthy Announce/two-step/Delay baseline unavailable: %s. ", session.reason);
                    return Result::CouldNotRun;
                }

                uint8_t first_identity[10];
                uint8_t second_identity[10];
                session.requester(first_identity);
                session.requester(second_identity);
                if (std::memcmp(first_identity, second_identity, sizeof(first_identity)) == 0)
                {
                    evidence.add("Synthetic source-port identities unexpectedly collided. ");
                    return Result::CouldNotRun;
                }

                Message first_dataset{};
                Message second_dataset{};
                if (!ranked_source_dataset(session.master, true, first_identity, first_dataset) ||
                    !ranked_source_dataset(session.master, true, second_identity, second_dataset))
                {
                    evidence.add("A strictly better non-self dataset could not be generated for both synthetic sources. ");
                    return Result::CouldNotRun;
                }

                ForeignStream first(session, first_dataset, first_identity, evidence, "source A");
                ForeignStream second(session, second_dataset, second_identity, evidence, "source B");
                evidence.add("\nIsolation phase: one fresh better Announce from each source-port identity must preserve "
                             "healthy service; only source A's second fresh Announce may qualify source A. ");
                if (!first.send(session) || !second.send(session))
                    return Result::CouldNotRun;

                const int64_t continuity_deadline = std::max(first.schedule.last_end_us, second.schedule.last_end_us) +
                    first.schedule.allowance_us +
                    static_cast<int64_t>(std::max(session.config.response_window_ms, session.config.processing_allowance_ms)) * 1000;
                if (!service_window(session, continuity_deadline, 0, true, nullptr, false))
                {
                    evidence.add("Distinct first Announces pooled into a qualified candidate: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }

                if (!first.send(session))
                    return Result::CouldNotRun;
                const int64_t quiet_deadline = first.schedule.silence_after_us +
                    static_cast<int64_t>(std::max<uint32_t>(1000, session.config.response_window_ms)) * 1000;
                evidence.add("Source A second fresh Announce: source-A silence-after=%lld us; "
                             "source B remains unqualified with one fresh Announce. ",
                             static_cast<long long>(first.schedule.silence_after_us));
                if (!service_window(session, quiet_deadline, first.schedule.silence_after_us, false))
                {
                    evidence.add("Source A did not establish bounded standby after its own second fresh Announce: %s. ",
                                 session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }

                const bool resumed = recover_foreign(session, evidence, first.schedule.cleanup_until() +
                    static_cast<int64_t>(session.config.recovery_ms) * 1000 + healthy_us);
                if (!resumed)
                    return Result::CouldNotRun;
                evidence.add("Source-port qualification remained isolated; source B's first Announce neither pooled with "
                             "source A nor survived as a qualified candidate. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result sequence_policy(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                int8_t log_interval = 1;
                int64_t check_us = 0;
                if (!sequence_timing(session.config, log_interval, check_us))
                {
                    evidence.add("Sequence-policy timing cannot separate qualification/recovery from candidate expiry "
                                 "within supported intervals; reduce tester timing allowances or use a longer DUT foreign timeout. ");
                    return Result::CouldNotRun;
                }
                const bool ranked = session.config.policy.selection == SelectionMode::Ranked;
                const int64_t healthy_us = healthy_window_us(session.config);
                const auto contradiction = [&]()
                {
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                };
                evidence.add("\nSequence policy: exact synthetic Announce sequences (independent of Delay_Req seed); "
                             "ranked rejects duplicate/backward/0x8000 advances without qualification or refresh; "
                             "65535->0->1 is fresh. Legacy accepts every supported Announce and refreshes its fixed timeout. ");
                for (const SequenceScenario &scenario : SequenceScenarios)
                {
                    session.progress(scenario.label);
                    session.rebase();
                    if (!service_window(session, esp_timer_get_time() + healthy_us, 0, true) || !session.control())
                    {
                        evidence.add("\n%s: stable healthy before-control unavailable: %s. ", scenario.label, session.reason);
                        return Result::CouldNotRun;
                    }
                    uint8_t identity[10];
                    session.requester(identity);
                    Message candidate = session.master;
                    if (ranked && !ranked_source_dataset(session.master, true, identity, candidate))
                    {
                        evidence.add("Strictly better non-self candidate unavailable. ");
                        return Result::CouldNotRun;
                    }
                    if (!ranked)
                        std::memcpy(candidate.grandmaster, identity, sizeof(candidate.grandmaster));
                    ForeignStream stream(session, candidate, identity, evidence, scenario.label, log_interval);
                    if (!stream.send_sequence(session, scenario.first))
                        return Result::CouldNotRun;
                    if (ranked)
                    {
                        if (!service_window(session, esp_timer_get_time() + check_us, 0, true, nullptr, false) ||
                            !session.control())
                        {
                            evidence.add("First Announce incorrectly qualified the candidate: %s. ", session.reason);
                            return contradiction();
                        }
                    }
                    else if (!service_window(session, stream.schedule.silence_after_us + check_us,
                                             stream.schedule.silence_after_us, false))
                    {
                        evidence.add("Legacy first-Announce suppression absent: %s. ", session.reason);
                        return contradiction();
                    }
                    if (!stream.send_sequence(session, scenario.probe))
                        return Result::CouldNotRun;
                    if (ranked && scenario.rejected)
                    {
                        if (!service_window(session, esp_timer_get_time() + check_us, 0, true, nullptr, false) ||
                            !session.control())
                        {
                            evidence.add("Rejected sequence incorrectly qualified the candidate: %s. ", session.reason);
                            return contradiction();
                        }
                        if (!stream.send_sequence(session, scenario.qualifier))
                            return Result::CouldNotRun;
                    }
                    const int64_t original_expiry = stream.schedule.no_early_service_until();
                    const int64_t update_at = stream.schedule.last_end_us + stream.schedule.lifetime_us * 2 / 3;
                    if (stream.schedule.silence_after_us == 0 || update_at >= original_expiry)
                    {
                        evidence.add("Qualification/late-update timing no longer has a decisive observation window. ");
                        return Result::CouldNotRun;
                    }
                    if (!service_window(session, update_at, stream.schedule.silence_after_us, false))
                    {
                        evidence.add("Qualified candidate failed to hold standby: %s. ", session.reason);
                        return contradiction();
                    }
                    const int64_t update_begin = esp_timer_get_time();
                    if (update_begin + stream.schedule.allowance_us >= original_expiry)
                    {
                        evidence.add("Tester missed the pre-expiry late-update window. ");
                        return Result::CouldNotRun;
                    }
                    const uint16_t update = ranked ? scenario.update : scenario.legacy_update;
                    if (!stream.send_sequence(session, update))
                        return Result::CouldNotRun;
                    const bool rejected = ranked && scenario.rejected;
                    evidence.add("Late update=%u expected=%s; original expiry=%lld us; expected expiry=%lld us. ",
                                 update, rejected ? "no refresh" : "refresh",
                                 static_cast<long long>(original_expiry),
                                 static_cast<long long>(stream.schedule.no_early_service_until()));
                    if (!service_window(session, stream.schedule.no_early_service_until(),
                                         stream.schedule.silence_after_us, false))
                    {
                        evidence.add("Premature service before accepted-sequence expiry: %s. ", session.reason);
                        return contradiction();
                    }
                    if (rejected)
                    {
                        // Recovery must precede even the earliest expiry of a wrongly accepted update.
                        const int64_t deadline = update_begin + stream.schedule.lifetime_us -
                            stream.schedule.allowance_us;
                        session.rebase();
                        const int64_t learn_us = deadline - esp_timer_get_time() -
                            static_cast<int64_t>(session.config.response_window_ms) * 1000;
                        if (learn_us <= 1000)
                        {
                            evidence.add("Tester missed the no-refresh discrimination deadline. ");
                            return Result::CouldNotRun;
                        }
                        const bool recovered = session.learn(static_cast<uint32_t>(learn_us / 1000)) && session.control();
                        if (esp_timer_get_time() >= deadline)
                        {
                            evidence.add("Tester overran the no-refresh discrimination deadline. ");
                            return Result::CouldNotRun;
                        }
                        evidence.add("No-refresh recovery Announce+matching Delay=%u before %lld us; %s. ",
                                     recovered, static_cast<long long>(deadline), session.reason);
                        if (!recovered)
                            return contradiction();
                    }
                    else if (!recover_foreign(session, evidence, stream.schedule.cleanup_until() +
                              static_cast<int64_t>(session.config.recovery_ms) * 1000 + healthy_us))
                        return Result::CouldNotRun;

                    if (!cleanup_foreign(session, evidence))
                        return Result::CouldNotRun;
                    evidence.add("%s: PASS with healthy Announce/two-step/Delay after-control. ", scenario.label);
                }
                evidence.add("\nAll four sequence scenarios passed for %s. No DUT-generated Sync rollover, "
                             "persistent anti-replay or synchronization-accuracy claim. ",
                             selection_text(session.config.policy.selection));
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        bool recover_interval(Session &session, Evidence &evidence, int64_t deadline)
        {
            session.rebase();
            session.first_activity_us = 0;
            bool announce = false;
            bool paired = false;
            bool delay = false;
            bool outstanding = false;
            int64_t next_probe = esp_timer_get_time();
            int64_t sent_us = 0;
            uint16_t sequence = 0;
            uint8_t identity[10] = {};
            std::array<Pair, 4> pairs{};
            Packet packet{};
            Message message{};
            while (session.healthy() && !session.protocol_fault && esp_timer_get_time() < deadline)
            {
                const int64_t now_us = esp_timer_get_time();
                if (now_us >= next_probe && deadline - now_us >
                    static_cast<int64_t>(session.config.response_window_ms) * 1000)
                {
                    uint8_t frame[60];
                    session.requester(identity);
                    sequence = ++s_sequence;
                    make_delay_request(frame, s_local_mac, identity, session.config.domain, 0, sequence);
                    sent_us = now_us;
                    Timestamp transmit{};
                    if (!session.send(frame, sizeof(frame), &transmit))
                        return false;
                    outstanding = true;
                    next_probe = now_us + static_cast<int64_t>(std::max(session.config.request_interval_ms,
                                                                      session.config.response_window_ms)) * 1000;
                }
                if (!session.pull(packet, std::min(deadline, now_us + 20000)))
                    continue;
                if (!session.decode(packet, message))
                    continue;
                announce = announce || message.type == 11;
                if (message.type == 0 || message.type == 8)
                {
                    Pair completed{};
                    paired = match_pair(pairs, message, packet, completed) || paired;
                }
                delay = delay || (outstanding && in_window(packet.received_us, sent_us,
                    sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000) &&
                    match_delay_response(message, session.master.source, session.master.minor_version, sequence, identity));
                if (announce && paired && delay)
                {
                    evidence.add("Bounded recovery: fresh Announce+two-step+matching Delay before %lld us; "
                                 "first/last activity=%lld/%lld us. ", static_cast<long long>(deadline),
                                 static_cast<long long>(session.first_activity_us),
                                 static_cast<long long>(session.last_activity_us));
                    return session.healthy() && !session.protocol_fault;
                }
            }
            if (!session.interrupted && !session.protocol_fault)
                session.reason = "fresh Announce/two-step/matching Delay did not recover within the interval-policy deadline";
            evidence.add("Bounded recovery failed before %lld us: Announce=%u two-step=%u Delay=%u; %s. ",
                         static_cast<long long>(deadline), announce, paired, delay, session.reason);
            return false;
        }

        Result dataset_changes(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                int8_t log_interval = 1;
                IntervalTiming timing{};
                if (!selection_timing(session.config, log_interval, timing))
                {
                    evidence.add("Dataset-change timing cannot separate requalification/recovery from candidate expiry. ");
                    return Result::CouldNotRun;
                }
                const auto contradiction = [&]()
                {
                    evidence.add("Dataset/reflection policy contradicted: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                };
                for (bool reflection : {false, true})
                {
                    session.progress(reflection ? "Self-grandmaster reflection: idle rejection and qualified-source release"
                                                : "Same-source grandmaster change: independent requalification");
                    session.rebase();
                    if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                        !session.control())
                    {
                        evidence.add("Dataset/reflection healthy before-control unavailable: %s. ", session.reason);
                        return Result::CouldNotRun;
                    }
                    uint8_t identity[10];
                    session.requester(identity);
                    Message original{};
                    Message changed{};
                    if (!ranked_source_dataset(session.master, true, identity, original) ||
                        !changed_grandmaster_dataset(session.master, original, changed))
                    {
                        evidence.add("Two distinct strictly better non-self grandmasters cannot be generated at this dataset boundary. ");
                        return Result::CouldNotRun;
                    }
                    Message reflected = original;
                    std::memcpy(reflected.grandmaster, session.master.grandmaster, 8);
                    if (compare_rank(reflected, session.master).order >= 0)
                    {
                        evidence.add("Self-reflection cannot be made strictly better independently of identity; fixture unavailable. ");
                        return Result::CouldNotRun;
                    }
                    ForeignStream stream(session, original, identity, evidence,
                                         reflection ? "self-grandmaster reflection" : "grandmaster A -> B", log_interval);
                    const auto reset_dataset = [&](const Message &dataset)
                    {
                        set_foreign_dataset(stream.frame, dataset, log_interval);
                        stream.schedule = {SelectionMode::Ranked, stream.schedule.lifetime_us, stream.schedule.allowance_us};
                        evidence.add("Same source dataset GM=%02x%02x%02x%02x%02x%02x%02x%02x; sequence continues. ",
                                     dataset.grandmaster[0], dataset.grandmaster[1], dataset.grandmaster[2],
                                     dataset.grandmaster[3], dataset.grandmaster[4], dataset.grandmaster[5],
                                     dataset.grandmaster[6], dataset.grandmaster[7]);
                    };
                    if (reflection)
                    {
                        reset_dataset(reflected);
                        for (unsigned index = 0; index < 2; ++index)
                        {
                            if (!stream.send(session))
                                return Result::CouldNotRun;
                            if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                                return contradiction();
                        }
                        evidence.add("Two strictly better self-GM Announces preserved idle healthy service. ");
                        reset_dataset(original);
                    }
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                        return contradiction();
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    if (!service_window(session, stream.schedule.silence_after_us + timing.check_us,
                                        stream.schedule.silence_after_us, false))
                        return contradiction();

                    const int64_t old_expiry = stream.schedule.no_early_service_until();
                    if (esp_timer_get_time() + timing.recovery_us + timing.check_us +
                        2 * stream.schedule.allowance_us >= old_expiry)
                    {
                        evidence.add("Tester missed the pre-expiry dataset-release discrimination window. ");
                        return Result::CouldNotRun;
                    }
                    reset_dataset(reflection ? reflected : changed);
                    const int64_t recovery_deadline = esp_timer_get_time() + timing.recovery_us;
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    evidence.add("First changed-dataset Announce must release standby before %lld us, "
                                 "strictly before original expiry=%lld us. ",
                                 static_cast<long long>(recovery_deadline), static_cast<long long>(old_expiry));
                    const bool recovered = recover_interval(session, evidence, recovery_deadline) &&
                        service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false);
                    if (esp_timer_get_time() >= old_expiry)
                    {
                        evidence.add("Tester overran original expiry; dataset release is inconclusive. ");
                        return Result::CouldNotRun;
                    }
                    if (!recovered)
                        return contradiction();
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    if (reflection)
                    {
                        if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true))
                            return contradiction();
                        evidence.add("Self-GM update released a qualified source; repeated reflection did not requalify it. ");
                    }
                    else
                    {
                        if (!service_window(session, stream.schedule.no_early_service_until(),
                                            stream.schedule.silence_after_us, false) ||
                            !recover_interval(session, evidence, stream.schedule.cleanup_until() + timing.recovery_us))
                            return contradiction();
                        evidence.add("Changed grandmaster qualified only after its own second fresh Announce "
                                     "and held standby for its own lifetime. ");
                    }
                    if (!cleanup_foreign(session, evidence))
                        return Result::CouldNotRun;
                    evidence.add("%s: PASS with healthy Announce/two-step/Delay after-control. ",
                                 reflection ? "self-grandmaster reflection" : "grandmaster change");
                }
                evidence.add("Both independent dataset/reflection scenarios passed; no full BMCA or clock-discipline claim. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result multiple_masters(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                int8_t log_interval = 1;
                IntervalTiming timing{};
                if (!selection_timing(session.config, log_interval, timing))
                {
                    evidence.add("Overlapping-master timing cannot separate both qualification and individual expiry windows. ");
                    return Result::CouldNotRun;
                }
                const auto contradiction = [&]()
                {
                    evidence.add("Overlapping-master policy contradicted: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                };
                for (bool best_expires_first : {true, false})
                {
                    session.progress(best_expires_first ? "Overlapping masters: best expires first"
                                                        : "Overlapping masters: worse expires first");
                    session.rebase();
                    if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                        !session.control())
                    {
                        evidence.add("Overlapping-master healthy before-control unavailable: %s. ", session.reason);
                        return Result::CouldNotRun;
                    }
                    uint8_t first_identity[10];
                    uint8_t second_identity[10];
                    session.requester(first_identity);
                    session.requester(second_identity);
                    Message best{};
                    Message worse{};
                    if (std::memcmp(first_identity, second_identity, 10) == 0 ||
                        !ranked_source_dataset(session.master, true, first_identity, best) ||
                        !changed_grandmaster_dataset(session.master, best, worse))
                    {
                        evidence.add("Two distinct source-port identities and strictly better grandmasters unavailable. ");
                        return Result::CouldNotRun;
                    }
                    if (compare_rank(best, worse).order > 0)
                        std::swap(best, worse);
                    ForeignStream first(session, best_expires_first ? best : worse, first_identity, evidence,
                                        best_expires_first ? "best / early expiry" : "worse / early expiry", log_interval);
                    ForeignStream second(session, best_expires_first ? worse : best, second_identity, evidence,
                                         best_expires_first ? "worse / late expiry" : "best / late expiry", log_interval);
                    if (!first.send(session))
                        return Result::CouldNotRun;
                    if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                        return contradiction();
                    if (!first.send(session))
                        return Result::CouldNotRun;
                    if (!service_window(session, first.schedule.silence_after_us + timing.check_us,
                                        first.schedule.silence_after_us, false))
                        return contradiction();
                    if (!second.send(session))
                        return Result::CouldNotRun;
                    if (!service_window(session, esp_timer_get_time() + timing.check_us,
                                        first.schedule.silence_after_us, false))
                        return contradiction();
                    if (!second.send(session) || !first.send(session))
                        return Result::CouldNotRun;
                    const int64_t refresh_at = first.schedule.last_end_us + first.schedule.lifetime_us / 2;
                    if (!service_window(session, refresh_at, first.schedule.silence_after_us, false))
                        return contradiction();
                    if (esp_timer_get_time() + 2 * first.schedule.allowance_us >= first.schedule.no_early_service_until())
                    {
                        evidence.add("Tester missed the overlapping-candidate refresh window. ");
                        return Result::CouldNotRun;
                    }
                    if (!second.send(session))
                        return Result::CouldNotRun;
                    const int64_t first_expired_check = first.schedule.cleanup_until() + timing.check_us;
                    if (first_expired_check + second.schedule.allowance_us >= second.schedule.no_early_service_until())
                    {
                        evidence.add("Independent expiry windows overlap tester uncertainty; observation inconclusive. ");
                        return Result::CouldNotRun;
                    }
                    evidence.add("Independent expiry bounds: early candidate latest=%lld us; "
                                 "post-expiry checkpoint=%lld us; remaining candidate earliest=%lld us. ",
                                 static_cast<long long>(first.schedule.cleanup_until()),
                                 static_cast<long long>(first_expired_check),
                                 static_cast<long long>(second.schedule.no_early_service_until()));
                    if (!service_window(session, first_expired_check, first.schedule.silence_after_us, false) ||
                        !service_window(session, second.schedule.no_early_service_until(),
                                        first.schedule.silence_after_us, false) ||
                        !recover_interval(session, evidence, second.schedule.cleanup_until() + timing.recovery_us))
                        return contradiction();
                    if (!cleanup_foreign(session, evidence))
                        return Result::CouldNotRun;
                    evidence.add("%s expired first: PASS; remaining candidate held standby through its independent "
                                 "lifetime, then healthy Announce/two-step/Delay recovered. ",
                                 best_expires_first ? "Best" : "Worse");
                }
                evidence.add("Both overlapping expiry orders passed; on-wire availability only, "
                             "not DUT table occupancy, selected-winner identity or PTP-derived holdover. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result foreign_capacity(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                constexpr int8_t LogInterval = 4;
                IntervalTiming timing{};
                const int64_t lifetime = candidate_lifetime_us(session.config.policy, LogInterval);
                if (!observation_timing(session.config, timing) ||
                    lifetime <= 2 * timing.recovery_us + 4 * timing.check_us)
                {
                    evidence.add("Capacity fixture cannot separate qualification, replacement and expiry windows. ");
                    return Result::CouldNotRun;
                }
                const size_t capacity = session.config.policy.capacity;
                auto streams = std::unique_ptr<std::unique_ptr<ForeignStream>[]>(
                    new (std::nothrow) std::unique_ptr<ForeignStream>[capacity]);
                auto datasets = std::unique_ptr<Message[]>(new (std::nothrow) Message[capacity]);
                if (!streams || !datasets)
                {
                    evidence.add("Insufficient memory for bounded capacity fixture. ");
                    return Result::CouldNotRun;
                }
                const auto contradiction = [&]()
                {
                    evidence.add("Capacity policy contradicted: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                };
                const auto baseline = [&]()
                {
                    return service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) &&
                           session.control();
                };
                const auto populate = [&](const char *label) -> bool
                {
                    for (size_t index = 0; index < capacity; ++index)
                    {
                        uint8_t identity[10];
                        session.requester(identity);
                        if (!ranked_source_dataset(session.master, true, identity, datasets[index]))
                            return false;
                        streams[index].reset(new (std::nothrow) ForeignStream(
                            session, datasets[index], identity, evidence, label, LogInterval));
                        if (!streams[index] || !streams[index]->send(session, false, false))
                            return false;
                    }
                    return true;
                };
                if (!baseline())
                {
                    evidence.add("Capacity healthy before-control unavailable: %s. ", session.reason);
                    return Result::CouldNotRun;
                }
                session.progress("Capacity: filling and qualifying all configured foreign records");
                if (!populate("capacity qualified candidate"))
                {
                    evidence.add("CouldNotRun: distinct supported candidates or successful sends unavailable. ");
                    return Result::CouldNotRun;
                }
                if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                    return contradiction();
                for (size_t index = 0; index < capacity; ++index)
                    if (!streams[index]->send(session, false, false))
                        return Result::CouldNotRun;
                size_t weakest = 0;
                for (size_t index = 1; index < capacity; ++index)
                    if (compare_rank(datasets[index], datasets[weakest]).order > 0)
                        weakest = index;
                const int64_t quiet_after = streams[0]->schedule.silence_after_us;
                if (!service_window(session, std::max(esp_timer_get_time(), quiet_after) + timing.check_us,
                                    quiet_after, false))
                    return contradiction();
                uint8_t newcomer_identity[10];
                session.requester(newcomer_identity);
                Message stronger{};
                if (!ranked_source_dataset(datasets[weakest], true, newcomer_identity, stronger) ||
                    compare_rank(stronger, session.master).order >= 0)
                {
                    evidence.add("A supported newcomer stronger than the weakest qualified record is unavailable. ");
                    return Result::CouldNotRun;
                }
                ForeignStream newcomer(session, stronger, newcomer_identity, evidence, "single stronger newcomer", LogInterval);
                if (!newcomer.send(session))
                    return Result::CouldNotRun;
                Message inferior{};
                if (!ranked_dataset(session.master, false, inferior))
                {
                    evidence.add("A supported inferior update for releasing other records is unavailable. ");
                    return Result::CouldNotRun;
                }
                for (size_t index = 0; index < capacity; ++index)
                    if (index != weakest)
                    {
                        set_foreign_dataset(streams[index]->frame, inferior, LogInterval);
                        if (!streams[index]->send(session, false, false))
                            return Result::CouldNotRun;
                    }
                ForeignStream &protected_candidate = *streams[weakest];
                if (esp_timer_get_time() + timing.check_us >= protected_candidate.schedule.no_early_service_until())
                {
                    evidence.add("Candidate lifetime elapsed before the protection observation could start. ");
                    return Result::CouldNotRun;
                }
                if (!service_window(session, protected_candidate.schedule.no_early_service_until(), quiet_after, false) ||
                    !recover_interval(session, evidence, protected_candidate.schedule.cleanup_until() + timing.recovery_us))
                    return contradiction();
                evidence.add("All %u candidate sources were qualified; one stronger newcomer was sent once; "
                             "the weakest qualified source held standby after the others were released. ",
                             static_cast<unsigned>(capacity));
                if (!cleanup_foreign(session, evidence) || !baseline())
                    return Result::CouldNotRun;

                session.progress("Capacity: replacing an unqualified record with a stronger newcomer");
                if (!populate("capacity unqualified candidate"))
                {
                    evidence.add("CouldNotRun: unqualified candidate fill failed. ");
                    return Result::CouldNotRun;
                }
                if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                    return contradiction();
                session.requester(newcomer_identity);
                size_t strongest = 0;
                for (size_t index = 1; index < capacity; ++index)
                    if (compare_rank(datasets[index], datasets[strongest]).order < 0)
                        strongest = index;
                if (!ranked_source_dataset(datasets[strongest], true, newcomer_identity, stronger) ||
                    compare_rank(stronger, session.master).order >= 0)
                {
                    evidence.add("A supported candidate stronger than every unqualified record is unavailable. ");
                    return Result::CouldNotRun;
                }
                ForeignStream replacement(session, stronger, newcomer_identity, evidence, "stronger unqualified replacement", LogInterval);
                if (!replacement.send(session) ||
                    !service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                if (!replacement.send(session))
                    return Result::CouldNotRun;
                if (!service_window(session, replacement.schedule.silence_after_us + timing.check_us,
                                    replacement.schedule.silence_after_us, false) ||
                    !service_window(session, replacement.schedule.no_early_service_until(),
                                    replacement.schedule.silence_after_us, false) ||
                    !recover_interval(session, evidence, replacement.schedule.cleanup_until() + timing.recovery_us))
                    return contradiction();
                evidence.add("A stronger newcomer qualified after the full unqualified fill and expired normally. "
                             "These are bounded on-wire consequences, not direct occupancy or eviction instrumentation. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result selection_ntp_load(Test test, Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                if (session.config.observation == nullptr)
                {
                    evidence.add("Combined-load observation callback unavailable. ");
                    return Result::CouldNotRun;
                }
                IntervalTiming timing{};
                if (!observation_timing(session.config, timing) ||
                    !service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                    !session.control())
                {
                    evidence.add("Healthy pre-standby PTP control unavailable: %s. ", session.reason);
                    return Result::CouldNotRun;
                }
                uint8_t identity[10];
                session.requester(identity);
                Message better{};
                if (!ranked_source_dataset(session.master, true, identity, better))
                    return Result::CouldNotRun;
                ForeignStream stream(session, better, identity, evidence, "NTP-loaded standby");
                if (!stream.send(session))
                    return Result::CouldNotRun;
                if (!service_window(session, stream.next_us, 0, true, nullptr, false))
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                if (!stream.send(session))
                    return Result::CouldNotRun;
                if (!service_window(session, stream.schedule.silence_after_us + timing.check_us,
                                    stream.schedule.silence_after_us, false))
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                session.progress("Qualified standby: observing bounded simultaneous IPv4 / IPv6 NTP load");
                session.config.observation(true, session.config.context);
                const int64_t end = esp_timer_get_time() +
                    (static_cast<int64_t>(session.config.observation_ms) + session.config.response_window_ms) * 1000;
                const bool held = service_window(session, end, stream.schedule.silence_after_us, false, &stream);
                session.config.observation(false, session.config.context);
                if (!held)
                {
                    evidence.add("PTP standby was interrupted during the NTP load window: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                if (!stream.send(session))
                    return Result::CouldNotRun;
                if (!service_window(session, stream.schedule.no_early_service_until(),
                                    stream.schedule.silence_after_us, false) ||
                    !recover_interval(session, evidence, stream.schedule.cleanup_until() + timing.recovery_us) ||
                    !session.control())
                {
                    evidence.add("Post-load standby expiry / healthy PTP control failed: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                evidence.add("A refreshed better candidate held PTP in standby for the complete bounded NTP load; "
                             "PTP recovered after expiry. %s ",
                             test == Test::SelectionNtpPolicyLoad
                                 ? "Concurrent negative NTP key-policy evidence is reported separately by the load worker; no holdover is claimed."
                                 : "This does not claim NTP holdover or negative-key enforcement under load.");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result interval_expiry(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                IntervalTiming timing{};
                if (!interval_timing(session.config, timing))
                {
                    evidence.add("Interval timing cannot separate qualification, fixed/advertised expiry and "
                                 "healthy recovery; use a shorter DUT foreign timeout or smaller tester/service intervals. ");
                    return Result::CouldNotRun;
                }
                const bool ranked = session.config.policy.selection == SelectionMode::Ranked;
                const int64_t healthy_us = healthy_window_us(session.config);
                const auto contradiction = [&]()
                {
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                };
                evidence.add("\nInterval policy: ranked lifetime=max(foreign timeout, 3*advertised interval); "
                             "supported logInterval=-7..4, negative fractions rounded up to microseconds. "
                             "Unsupported ranked intervals must neither qualify nor refresh a source. "
                             "Legacy accepts all Announce interval bytes and uses the fixed timeout. "
                             "Recovery budget=%lld us; no synchronization-accuracy claim. ",
                             static_cast<long long>(timing.recovery_us));
                for (const IntervalScenario &scenario : IntervalScenarios)
                {
                    session.progress(scenario.label);
                    session.rebase();
                    if (!service_window(session, esp_timer_get_time() + healthy_us, 0, true) || !session.control())
                    {
                        evidence.add("\n%s: stable healthy before-control unavailable: %s. ", scenario.label, session.reason);
                        return Result::CouldNotRun;
                    }
                    uint8_t identity[10];
                    session.requester(identity);
                    Message candidate = session.master;
                    if (ranked && !ranked_source_dataset(session.master, true, identity, candidate))
                    {
                        evidence.add("Strictly better non-self interval candidate unavailable. ");
                        return Result::CouldNotRun;
                    }
                    if (!ranked)
                        std::memcpy(candidate.grandmaster, identity, sizeof(candidate.grandmaster));
                    ForeignStream stream(session, candidate, identity, evidence, scenario.label,
                                         scenario.supported ? scenario.logarithm : 4);
                    const bool rejected = ranked && !scenario.supported;
                    evidence.add("Probe logInterval=%d expected=%s. ", scenario.logarithm,
                                 rejected ? "rejected without qualification/refresh" :
                                 ranked ? "interval-derived lifetime" : "fixed-timeout suppression");
                    set_foreign_dataset(stream.frame, candidate, scenario.logarithm);
                    if (!stream.send_sequence(session, 100, !rejected))
                        return Result::CouldNotRun;
                    if (rejected)
                    {
                        if (!service_window(session, esp_timer_get_time() + healthy_us, 0, true) ||
                            !stream.send_sequence(session, 101, false) ||
                            !service_window(session, esp_timer_get_time() + healthy_us, 0, true))
                        {
                            evidence.add("Unsupported interval incorrectly suppressed healthy service: %s. ", session.reason);
                            return contradiction();
                        }
                        set_foreign_dataset(stream.frame, candidate, 4);
                        if (!stream.send_sequence(session, 102))
                            return Result::CouldNotRun;
                    }
                    if (ranked)
                    {
                        if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                        {
                            evidence.add("First supported Announce incorrectly qualified the interval candidate: %s. ", session.reason);
                            return contradiction();
                        }
                        if (!stream.send_sequence(session, rejected ? 103 : 101))
                            return Result::CouldNotRun;
                    }
                    int64_t recovery_deadline = stream.schedule.cleanup_until() + timing.recovery_us;
                    if (rejected)
                    {
                        const int64_t original_expiry = stream.schedule.no_early_service_until();
                        const int64_t update_at = stream.schedule.last_end_us + stream.schedule.lifetime_us * 2 / 3;
                        if (!service_window(session, update_at, stream.schedule.silence_after_us, false))
                        {
                            evidence.add("Supported positive interval control failed to hold standby: %s. ", session.reason);
                            return contradiction();
                        }
                        const int64_t update_begin = esp_timer_get_time();
                        if (update_begin + stream.schedule.allowance_us >= original_expiry)
                        {
                            evidence.add("Tester missed the pre-expiry unsupported-interval update window. ");
                            return Result::CouldNotRun;
                        }
                        set_foreign_dataset(stream.frame, candidate, scenario.logarithm);
                        if (!stream.send_sequence(session, 104, false))
                            return Result::CouldNotRun;
                        recovery_deadline = std::min(recovery_deadline,
                            update_begin + stream.schedule.lifetime_us - stream.schedule.allowance_us);
                        evidence.add("Fresh unsupported update must not shorten/extend original expiry=%lld us; "
                                     "no-refresh recovery deadline=%lld us. ",
                                     static_cast<long long>(original_expiry), static_cast<long long>(recovery_deadline));
                    }
                    session.progress("Checking interval-specific standby and bounded fresh recovery");
                    evidence.add("No service before %lld us; fresh recovery required before %lld us. ",
                                 static_cast<long long>(stream.schedule.no_early_service_until()),
                                 static_cast<long long>(recovery_deadline));
                    if (!service_window(session, stream.schedule.no_early_service_until(),
                                        stream.schedule.silence_after_us, false))
                    {
                        evidence.add("Premature service before interval expiry: %s. ", session.reason);
                        return contradiction();
                    }
                    if (!recover_interval(session, evidence, recovery_deadline))
                        return contradiction();
                    if (!cleanup_foreign(session, evidence))
                        return Result::CouldNotRun;
                    evidence.add("%s: PASS with healthy Announce/two-step/Delay after-control. ", scenario.label);
                }
                evidence.add("\nAll seven interval scenarios passed for %s. ", selection_text(session.config.policy.selection));
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result signed_service(Session &session, Evidence &evidence, bool exchange)
        {
            if (!session.master.authenticated || !session.control())
            {
                evidence.add("Signed Announce/Delay baseline unavailable: %s. ", session.reason);
                return Result::CouldNotRun;
            }
            std::array<Pair, 4> pairs{};
            Packet packet{};
            Message message{};
            Pair completed{};
            const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(session.config.timeout_ms) * 1000;
            while (session.pull(packet, deadline))
            {
                if (!session.decode(packet, message))
                    continue;
                if ((message.type == 0 || message.type == 8) && match_pair(pairs, message, packet, completed))
                {
                    if (!completed.sync.authenticated || !completed.follow.authenticated)
                        return Result::Fail;
                    const bool after = session.control();
                    evidence.add("Independent HMAC-SHA256-128 verification: Announce, Sync and Follow_Up; paired sequence=%u. "
                                 "Signed Delay_Req and matching verified Delay_Resp controls before/after=%u/%u. %s. "
                                 "Keys and ICVs redacted; functional authentication, not a servo/accuracy or anti-replay claim. ",
                                 completed.sequence, true, after,
                                 exchange ? "Authorized signed E2E exchange exercised" : "Signed periodic service exercised");
                    if (!session.healthy())
                        return Result::CouldNotRun;
                    return after && !session.protocol_fault ? Result::Pass : Result::Fail;
                }
            }
            evidence.add("Verified signed periodic pair unavailable: %s. ", session.reason);
            return session.interrupted ? Result::CouldNotRun : Result::Fail;
        }

        Result authentication_delays(Session &session, Evidence &evidence, bool policy_test)
        {
            Authenticator wrong_key;
            char wrong_value[33] = {};
            std::memcpy(wrong_value, session.config.key_value, 32);
            wrong_value[0] = wrong_value[0] == 'A' ? 'B' : 'A';
            const bool wrong_ready = wrong_key.initialize(session.config.key_id, wrong_value, session.config.policy.spp);
            std::memset(wrong_value, 0, sizeof(wrong_value));
            if (!wrong_ready)
                return Result::CouldNotRun;
            Result overall = Result::Pass;
            const size_t count = policy_test ? 2 : static_cast<size_t>(AuthMutation::Count);
            for (size_t index = 0; index < count; ++index)
            {
                const auto mutation = static_cast<AuthMutation>(index);
                const bool before = session.control();
                if (!before)
                {
                    evidence.add("Signed before-control unavailable: %s. ", session.reason);
                    return Result::CouldNotRun;
                }
                uint8_t identity[10];
                session.requester(identity);
                const uint16_t sequence = ++s_sequence;
                uint8_t frame[MaxFrameSize] = {};
                make_delay_request(frame, s_local_mac, identity, session.config.domain,
                                   policy_test ? static_cast<uint8_t>(index) : 1, sequence);
                size_t message_length = 44;
                if (!policy_test)
                {
                    const Authenticator &signer = mutation == AuthMutation::WrongKey ? wrong_key : session.auth;
                    if (!signer.append(frame + 14, sizeof(frame) - 14, message_length))
                        return Result::CouldNotRun;
                    message_length = mutate_authentication(frame + 14, message_length, mutation);
                }
                const int64_t sent_us = esp_timer_get_time();
                if (!session.send(frame, std::max<size_t>(60, message_length + 14), nullptr, false))
                    return Result::CouldNotRun;
                bool response = false;
                bool verified = false;
                Packet packet{};
                Message message{};
                const int64_t deadline = sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000;
                while (session.pull(packet, deadline))
                {
                    if (packet.received_us < sent_us || !raw_delay_match(packet, session.master.source, sequence, identity))
                        continue;
                    response = true;
                    verified |= session.decode(packet, message) && message.authenticated;
                }
                const bool expected = policy_test && session.config.policy.authentication == AuthenticationMode::Mixed;
                const bool after = session.control();
                const Result result = classify_authenticated_probe(before, after, response, verified, expected, session.interrupted,
                                                                   s_drops.load() - session.drops, session.protocol_fault);
                evidence.add("%s: %s (signed controls=%u/%u; response=%u; expected=%u; response ICV verified=%u). ",
                             policy_test ? (index == 0 ? "unsigned-v2.0-policy" : "unsigned-v2.1-policy") : AuthMutationNames[index],
                             result == Result::Pass ? "pass" : result == Result::CouldNotRun ? "CouldNotRun" : "FAIL",
                             before, after, response, expected, verified);
                if (result == Result::CouldNotRun)
                    return result;
                if (result == Result::Fail)
                    overall = Result::Fail;
            }
            evidence.add("Unique requester identities/sequences; complete bounded response windows; no pass from silence without signed positive controls. ");
            return overall;
        }

        Result authentication_before_selection(Session &session, Evidence &evidence)
        {
            if (!session.config.isolated_link || !session.config.suppression_enabled)
            {
                evidence.add("Authentication-before-selection requires the synthetic-Announce settings opt-in. ");
                return Result::CouldNotRun;
            }
            evidence.add("Authenticated foreign-Announce acceptance/recovery positive control: ");
            const Result accepted = foreign_suppression(session, evidence);
            if (accepted != Result::Pass || session.stop.load())
                return accepted == Result::Fail ? Result::Fail : Result::CouldNotRun;
            session.rebase();
            session.master = {};
            if (!session.learn(session.config.timeout_ms))
                return Result::CouldNotRun;
            const Result outcome = [&]() -> Result
            {
                if (!session.control())
                    return Result::CouldNotRun;
                Message better{};
                if (!ranked_dataset(session.master, true, better))
                {
                    evidence.add("No strictly better non-self candidate dataset available. ");
                    return Result::CouldNotRun;
                }
                int64_t lifetime = candidate_lifetime_us(session.config.policy, 1);
                for (AuthMutation mutation : {AuthMutation::UnknownKey, AuthMutation::Icv})
                {
                    uint8_t identity[10];
                    session.requester(identity);
                    for (unsigned count = 0; count < 2; ++count)
                    {
                        uint8_t frame[MaxFrameSize] = {};
                        make_foreign_announce(frame, s_local_mac, identity, session.config.domain, ++s_sequence);
                        set_foreign_dataset(frame, better, 1);
                        promote_to_v21(frame + 14);
                        size_t length = 64;
                        if (!session.auth.append(frame + 14, sizeof(frame) - 14, length))
                            return Result::CouldNotRun;
                        length = mutate_authentication(frame + 14, length, mutation);
                        const bool sent = session.send(frame, length + 14, nullptr, false);
                        s_foreign_cleanup.attempted(session.config, esp_timer_get_time(), lifetime,
                                                    static_cast<int64_t>(session.config.processing_allowance_ms) * 1000);
                        if (!sent)
                            return Result::CouldNotRun;
                        const int64_t window = count == 0 ? 2000000 : healthy_window_us(session.config);
                        const bool continued = service_window(session, esp_timer_get_time() + window, 0, true, nullptr, count != 0);
                        evidence.add("%s better Announce %u/2: continuous signed service=%u; candidate rank=%d. ",
                                     AuthMutationNames[static_cast<size_t>(mutation)], count + 1, continued,
                                     compare_rank(better, session.master).order);
                        if (!continued)
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    }
                }
                evidence.add("Rejected authentication cannot qualify/suppress the master; both legacy first-Announce and ranked two-fresh-Announce triggers exercised. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return cleaned && !session.stop.load() ? outcome : Result::CouldNotRun;
        }

        Result announce_authentication_policy(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                IntervalTiming timing{};
                if (!observation_timing(session.config, timing))
                    return Result::CouldNotRun;
                const bool mixed = session.config.policy.authentication == AuthenticationMode::Mixed;
                const bool ranked = session.config.policy.selection == SelectionMode::Ranked;
                for (uint8_t minor : {uint8_t{0}, uint8_t{1}})
                {
                    if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                        !session.control())
                    {
                        evidence.add("Signed baseline unavailable before unsigned v2.%u Announce: %s. ", minor, session.reason);
                        return Result::CouldNotRun;
                    }
                    uint8_t identity[10];
                    session.requester(identity);
                    Message better{};
                    if (!ranked_source_dataset(session.master, true, identity, better))
                    {
                        evidence.add("Strictly better non-self Announce dataset unavailable. ");
                        return Result::CouldNotRun;
                    }
                    ForeignStream stream(session, better, identity, evidence, "unsigned authentication policy", 1, false);
                    if (minor == 1)
                        promote_to_v21(stream.frame + 14);
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    if (!mixed || ranked)
                    {
                        const int64_t window = mixed ? timing.check_us : healthy_window_us(session.config);
                        if (!service_window(session, esp_timer_get_time() + window, 0, true, nullptr, !mixed))
                        {
                            evidence.add("Unsigned first Announce contradicted expected %s policy: %s. ",
                                         mixed ? "ranked qualification" : "required rejection", session.reason);
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                        }
                    }
                    if (ranked && esp_timer_get_time() + stream.schedule.allowance_us >=
                                      stream.schedule.no_early_service_until())
                    {
                        evidence.add("Unsigned qualification probes no longer fit within one candidate lifetime. ");
                        return Result::CouldNotRun;
                    }
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                    if (mixed)
                    {
                        if (!service_window(session, stream.schedule.silence_after_us + timing.check_us,
                                            stream.schedule.silence_after_us, false) ||
                            !service_window(session, stream.schedule.no_early_service_until(),
                                            stream.schedule.silence_after_us, false) ||
                            !recover_interval(session, evidence, stream.schedule.cleanup_until() + timing.recovery_us))
                        {
                            evidence.add("Mixed-policy unsigned v2.%u candidate acceptance/expiry contradicted: %s. ",
                                         minor, session.reason);
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                        }
                    }
                    else if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true))
                    {
                        evidence.add("Required policy failed to reject unsigned v2.%u candidate: %s. ", minor, session.reason);
                        return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    }
                    if (!session.control() || !cleanup_foreign(session, evidence))
                    {
                        evidence.add("Signed post-control or bounded cleanup unavailable: %s. ", session.reason);
                        return Result::CouldNotRun;
                    }
                    evidence.add("Unsigned v2.%u better Announce %s under keyed %s policy; signed service and Delay controls verified. ",
                                 minor, mixed ? "qualified/suppressed and expired" : "rejected without suppression",
                                 mixed ? "mixed" : "required");
                }
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result invalid_authentication_refresh(Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                int8_t logarithm = 1;
                IntervalTiming timing{};
                if (!invalid_refresh_timing(session.config, logarithm, timing))
                {
                    evidence.add("Configured timeout cannot separate original expiry from rejected-update expiry. ");
                    return Result::CouldNotRun;
                }
                if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                    !session.control())
                {
                    evidence.add("Signed pre-control unavailable: %s. ", session.reason);
                    return Result::CouldNotRun;
                }
                uint8_t identity[10];
                session.requester(identity);
                Message better{};
                if (!ranked_source_dataset(session.master, true, identity, better))
                    return Result::CouldNotRun;
                ForeignStream stream(session, better, identity, evidence, "signed live candidate", logarithm);
                if (!stream.send(session))
                    return Result::CouldNotRun;
                if (session.config.policy.selection == SelectionMode::Ranked)
                {
                    if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                        return session.interrupted ? Result::CouldNotRun : Result::Fail;
                    if (!stream.send(session))
                        return Result::CouldNotRun;
                }
                const int64_t invalid_at = stream.schedule.last_end_us + timing.recovery_us +
                                           timing.check_us + 2 * stream.schedule.allowance_us;
                if (invalid_at + timing.check_us >= stream.schedule.no_early_service_until() ||
                    !service_window(session, invalid_at, stream.schedule.silence_after_us, false))
                {
                    evidence.add("Qualified standby or decisive invalid-update window unavailable: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                int64_t earliest_invalid = 0;
                unsigned index = 0;
                for (AuthMutation mutation : {AuthMutation::Icv, AuthMutation::UnknownKey})
                {
                    uint8_t frame[MaxFrameSize] = {};
                    std::memcpy(frame, stream.frame, sizeof(stream.frame));
                    const uint16_t sequence = static_cast<uint16_t>(stream.sequences.value + ++index);
                    size_t length = 0;
                    if (!make_invalid_announce_update(frame, sizeof(frame), length, sequence, session.auth, mutation))
                    {
                        evidence.add("CouldNotRun: invalid signed Announce construction failed. ");
                        return Result::CouldNotRun;
                    }
                    const int64_t begin = esp_timer_get_time();
                    const bool sent = session.send(frame, length, nullptr, false);
                    const int64_t end = esp_timer_get_time();
                    s_foreign_cleanup.attempted(session.config, end, stream.schedule.lifetime_us,
                                                stream.schedule.allowance_us);
                    if (!sent || end - begin > stream.schedule.allowance_us)
                    {
                        evidence.add("Invalid-update TX not confirmed within allowance; possible candidate lease retained. ");
                        session.interrupted = true;
                        return Result::CouldNotRun;
                    }
                    if (earliest_invalid == 0)
                        earliest_invalid = begin;
                    evidence.add("%s same-source fresh-sequence=%u TX=[%lld,%lld] us; ",
                                 AuthMutationNames[static_cast<size_t>(mutation)], sequence,
                                 static_cast<long long>(begin), static_cast<long long>(end));
                }
                const int64_t recovery_deadline = stream.schedule.cleanup_until() + timing.recovery_us;
                if (recovery_deadline >= earliest_invalid + stream.schedule.lifetime_us)
                {
                    evidence.add("Original and invalid-update expiry windows overlap; no attribution possible. ");
                    return Result::CouldNotRun;
                }
                evidence.add("Original expiry reference=[%lld,%lld] us; recovery deadline=%lld us; "
                             "earliest possible invalid-update expiry=%lld us. ",
                             static_cast<long long>(stream.schedule.last_begin_us),
                             static_cast<long long>(stream.schedule.last_end_us),
                             static_cast<long long>(recovery_deadline),
                             static_cast<long long>(earliest_invalid + stream.schedule.lifetime_us));
                if (!service_window(session, stream.schedule.no_early_service_until(),
                                    stream.schedule.silence_after_us, false) ||
                    !recover_interval(session, evidence, recovery_deadline))
                {
                    evidence.add("Invalid authentication extended the original live-candidate lease or recovery failed: %s. ",
                                 session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                if (!session.control())
                    return Result::CouldNotRun;
                evidence.add("Original candidate expired despite invalid same-source updates; signed post-control passed. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result optional_unsigned_service(Session &session, Evidence &evidence)
        {
            if ((session.master.flags & (UtcValidFlag | PtpTimescaleFlag)) != (UtcValidFlag | PtpTimescaleFlag))
            {
                evidence.add("CouldNotRun: fresh GNSS-derived PTP timescale and valid UTC metadata required. ");
                return Result::CouldNotRun;
            }
            if (!session.control())
            {
                evidence.add("Unsigned Delay before-control unavailable: %s. ", session.reason);
                return Result::CouldNotRun;
            }
            const bool wire = service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true);
            evidence.add("Effective outgoing=unsigned v2.0; Announce/Sync/Follow_Up/Delay_Resp require base "
                         "messageLength=64/44/44/54, legacy control=5/0/2/3 and no TLV; Ethernet padding ignored. "
                         "Complete periodic and Delay wire window=%u. ", wire);
            if (!wire)
                return session.interrupted ? Result::CouldNotRun : Result::Fail;
            const Result measured = observe(session, evidence);
            const bool after = session.control();
            evidence.add("Unsigned hardware-timestamped exchange after-control=%u; "
                         "unsigned time is not authenticated time; no follower LOCKED/servo claim. ", after);
            return session.interrupted ? Result::CouldNotRun :
                   measured == Result::Pass && (!after || session.protocol_fault) ? Result::Fail : measured;
        }

        Result optional_signed_rejection(Session &session, Evidence &evidence)
        {
            constexpr int8_t LogInterval = 4;
            const int64_t lifetime = candidate_lifetime_us(session.config.policy, LogInterval);
            const int64_t allowance = static_cast<int64_t>(session.config.processing_allowance_ms) * 1000;
            if (session.config.policy.selection == SelectionMode::Ranked &&
                lifetime <= healthy_window_us(session.config) + 2 * allowance)
            {
                evidence.add("CouldNotRun: cannot keep both signed Announce qualification probes within one candidate lifetime. ");
                return Result::CouldNotRun;
            }
            // Public synthetic fixture only; no server or profile key is used.
            Authenticator signer;
            if (!signer.initialize(session.config.policy.outgoing_key_id,
                                   "0123456789ABCDEFGHIJKLMNOPQRSTUV", session.config.policy.spp))
            {
                evidence.add("CouldNotRun: synthetic rejection-probe signer unavailable. ");
                return Result::CouldNotRun;
            }
            evidence.add("Unsigned foreign-Announce acceptance/expiry positive control: ");
            const Result accepted = foreign_suppression(session, evidence);
            if (accepted != Result::Pass || session.stop.load())
                return accepted == Result::Fail ? Result::Fail : Result::CouldNotRun;
            const Result outcome = [&]() -> Result
            {
                Message better{};
                uint8_t candidate[10];
                session.requester(candidate);
                if (!ranked_source_dataset(session.master, true, candidate, better))
                {
                    evidence.add("CouldNotRun: strictly better non-self candidate unavailable. ");
                    return Result::CouldNotRun;
                }
                for (AuthMutation mutation : KeylessProbes)
                {
                    const char *label = mutation == AuthMutation::Count ? "synthetic-valid-signature"
                        : AuthMutationNames[static_cast<size_t>(mutation)];
                    if (!session.control())
                    {
                        evidence.add("Unsigned before-control unavailable for %s: %s. ", label, session.reason);
                        return Result::CouldNotRun;
                    }
                    uint8_t identity[10];
                    session.requester(identity);
                    const uint16_t sequence = ++s_sequence;
                    uint8_t frame[MaxFrameSize] = {};
                    make_delay_request(frame, s_local_mac, identity, session.config.domain, 0, sequence);
                    size_t length = 44;
                    if (!sign_rejection_probe(frame + 14, sizeof(frame) - 14, length, signer, mutation))
                    {
                        evidence.add("CouldNotRun: synthetic Delay probe signing failed. ");
                        return Result::CouldNotRun;
                    }
                    const int64_t sent_us = esp_timer_get_time();
                    if (!session.send(frame, length + 14, nullptr, false))
                    {
                        evidence.add("CouldNotRun: signed Delay probe transmit failed: %s. ", session.reason);
                        return Result::CouldNotRun;
                    }
                    bool response = false;
                    Packet packet{};
                    const int64_t deadline = sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000;
                    while (session.pull(packet, deadline))
                    {
                        if (packet.received_us < sent_us)
                            continue;
                        response |= raw_delay_match(packet, session.master.source, sequence, identity);
                        Message observed{};
                        session.decode(packet, observed);
                    }
                    const bool after = session.control();
                    const Result delay_result = classify_authenticated_probe(true, after, response, false, false,
                        session.interrupted, s_drops.load() - session.drops, session.protocol_fault);
                    evidence.add("\n%s signed Delay_Req: response=%u (expected 0); unsigned controls=1/%u; result=%s. ",
                                 label, response, after, delay_result == Result::Pass ? "pass" :
                                 delay_result == Result::Fail ? "FAIL" : "CouldNotRun");
                    if (delay_result != Result::Pass)
                    {
                        evidence.add("Probe/control observation: %s. ", session.reason);
                        return delay_result;
                    }
                    session.requester(candidate);
                    for (unsigned count = 0; count < 2; ++count)
                    {
                        make_foreign_announce(frame, s_local_mac, candidate, session.config.domain, ++s_sequence);
                        set_foreign_dataset(frame, better, LogInterval);
                        length = 64;
                        if (!sign_rejection_probe(frame + 14, sizeof(frame) - 14, length, signer, mutation))
                        {
                            evidence.add("CouldNotRun: synthetic Announce probe signing failed. ");
                            return Result::CouldNotRun;
                        }
                        const bool sent = session.send(frame, length + 14, nullptr, false);
                        s_foreign_cleanup.attempted(session.config, esp_timer_get_time(), lifetime, allowance);
                        if (!sent)
                        {
                            evidence.add("CouldNotRun: signed Announce probe transmit failed: %s. ", session.reason);
                            return Result::CouldNotRun;
                        }
                        const bool continued = service_window(session, esp_timer_get_time() +
                            healthy_window_us(session.config), 0, true);
                        evidence.add("%s better signed Announce %u/2: unsigned Announce/two-step/Delay continuity=%u. ",
                                     label, count + 1, continued);
                        if (!continued)
                        {
                            evidence.add("Service-continuity observation: %s. ", session.reason);
                            return session.interrupted ? Result::CouldNotRun : Result::Fail;
                        }
                    }
                }
                evidence.add("All signed-looking/tampered probes rejected without response, selection suppression "
                             "or outgoing-mode downgrade; healthy unsigned controls required throughout. ");
                return Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result signed_delay_probe(Session &session, Evidence &evidence, const Authenticator &signer,
                                  bool expected, const char *label)
        {
            const bool before = session.control();
            if (!before)
            {
                evidence.add("%s: signed before-control unavailable: %s. ", label, session.reason);
                return Result::CouldNotRun;
            }
            uint8_t identity[10];
            session.requester(identity);
            const uint16_t sequence = ++s_sequence;
            uint8_t frame[MaxFrameSize] = {};
            make_delay_request(frame, s_local_mac, identity, session.config.domain, 0, sequence);
            promote_to_v21(frame + 14);
            size_t length = 44;
            if (!signer.append(frame + 14, sizeof(frame) - 14, length) ||
                signer.verify(frame + 14, length) != AuthResult::Valid)
            {
                evidence.add("%s: probe signing/verification unavailable. ", label);
                return Result::CouldNotRun;
            }
            Timestamp transmit{};
            const int64_t sent_us = esp_timer_get_time();
            if (!session.send(frame, length + 14, &transmit, false))
                return Result::CouldNotRun;
            bool response = false;
            bool verified = false;
            Packet packet{};
            Message message{};
            const int64_t deadline = sent_us + static_cast<int64_t>(session.config.response_window_ms) * 1000;
            while (session.pull(packet, deadline))
            {
                if (packet.received_us < sent_us ||
                    !raw_delay_match(packet, session.master.source, sequence, identity))
                    continue;
                response = true;
                verified |= session.decode(packet, message) && message.authenticated &&
                            match_delay_response(message, session.master.source, session.master.minor_version,
                                                 sequence, identity);
            }
            const bool after = session.control();
            const Result result = classify_authenticated_probe(before, after, response, verified, expected,
                session.interrupted, s_drops.load() - session.drops, session.protocol_fault);
            evidence.add("%s: response=%u expected=%u verified=%u signed controls=%u/%u result=%s. ",
                         label, response, expected, verified, before, after,
                         result == Result::Pass ? "pass" : result == Result::Fail ? "FAIL" : "CouldNotRun");
            return result;
        }

        Result key_rotation(Session &session, Evidence &evidence)
        {
            Authenticator fixture_key;
            if (!session.config.rotation_enabled || session.config.rotation_key_id == 0 ||
                session.config.rotation_key_id == session.config.key_id ||
                session.config.rotation_key_value == nullptr ||
                !fixture_key.initialize(session.config.rotation_key_id, session.config.rotation_key_value,
                                        session.config.policy.spp))
            {
                evidence.add("Rotation requires the explicit isolated-fixture opt-in and a distinct valid public fixture key. ");
                return Result::CouldNotRun;
            }
            const auto prompt = [&](Test phase, bool restoration)
            {
                s_active.store(false);
                const bool accepted = session.config.assist(phase, restoration, session.stop,
                                                             session.config.context, nullptr, nullptr);
                s_active.store(true);
                return accepted && session.healthy();
            };
            if (!prompt(Test::KeyRotationReady, false))
            {
                evidence.add("The isolated two-key fixture was not confirmed; restore it manually if already prepared. ");
                return Result::CouldNotRun;
            }
            s_rotation_restore.begin(session.config);
            const Result result = [&]() -> Result
            {
                session.rebase();
                if (!session.learn(session.config.recovery_ms) || !session.control())
                {
                    evidence.add("Initial active-key service unavailable after fixture confirmation: %s. ", session.reason);
                    return Result::CouldNotRun;
                }
                const Result second_key = signed_delay_probe(session, evidence, fixture_key, true, "second inbound key");
                if (second_key != Result::Pass)
                    return second_key;
                if (!prompt(Test::KeyRotation, false))
                {
                    evidence.add("Outgoing-key rotation not acknowledged; original fixture must be restored. ");
                    return Result::CouldNotRun;
                }
                Config rotated_config = session.config;
                rotated_config.key_id = session.config.rotation_key_id;
                rotated_config.key_value = session.config.rotation_key_value;
                rotated_config.policy.outgoing_key_id = session.config.rotation_key_id;
                rotated_config.policy.provenance = "operator-confirmed staged rotation fixture, not runtime attestation";
                Session rotated{rotated_config, session.stop};
                if (!rotated.auth.initialize(rotated_config.key_id, rotated_config.key_value, rotated_config.policy.spp))
                    return Result::CouldNotRun;
                rotated.rebase();
                if (!rotated.learn(rotated.config.recovery_ms) || !rotated.control())
                {
                    evidence.add("Rotated outgoing-key service unavailable: %s. ", rotated.reason);
                    return Result::CouldNotRun;
                }
                evidence.add("Outgoing key ID %lu independently verified after operator-confirmed restart. ",
                             static_cast<unsigned long>(rotated_config.key_id));
                Result phase = signed_delay_probe(rotated, evidence, session.auth, true, "old key during overlap");
                if (phase != Result::Pass)
                    return phase;
                if (!prompt(Test::KeyRotation, true))
                {
                    evidence.add("Old-key revocation not acknowledged; original fixture must be restored. ");
                    return Result::CouldNotRun;
                }
                rotated.rebase();
                rotated.master = {};
                rotated.service_authentication = {};
                if (!rotated.learn(rotated.config.recovery_ms) || !rotated.control())
                {
                    evidence.add("New-key positive control unavailable after revocation: %s. ", rotated.reason);
                    return Result::CouldNotRun;
                }
                return signed_delay_probe(rotated, evidence, session.auth, false, "revoked old key");
            }();
            if (session.stop.load() || !prompt(Test::KeyRotationRestore, true))
            {
                evidence.add("Original fixture restoration not confirmed; restore it manually before further testing. ");
                return Result::CouldNotRun;
            }
            Session restored{session.config, session.stop};
            if (!restored.auth.initialize(session.config.key_id, session.config.key_value, session.config.policy.spp))
                return Result::CouldNotRun;
            restored.rebase();
            if (!restored.learn(restored.config.recovery_ms) || !restored.control())
            {
                evidence.add("Original outgoing-key positive control unavailable after restoration: %s. ", restored.reason);
                return Result::CouldNotRun;
            }
            if (signed_delay_probe(restored, evidence, fixture_key, false, "removed public fixture key") != Result::Pass)
            {
                evidence.add("Original fixture cleanup is unresolved; dependent PTP tests remain blocked. ");
                return Result::CouldNotRun;
            }
            s_rotation_restore.completed(true);
            evidence.add("Original outgoing key verified and public fixture key rejected after restoration. ");
            return result;
        }

        struct StartupMonitor
        {
            Session &session;
            bool armed = false;
            int64_t lower_bound_us = 0;
            int64_t last_poll_us = 0;
            int64_t next_probe_us = 0;
            int64_t first_service_us = 0;
            uint32_t service_packets = 0;

            static bool assist_poll(void *context)
            {
                return static_cast<StartupMonitor *>(context)->poll();
            }

            bool poll()
            {
                if (!session.healthy())
                    return false;
                const int64_t now = esp_timer_get_time();
                if (armed && last_poll_us != 0 &&
                    (now < last_poll_us || now - last_poll_us > StandbyPollBudgetUs))
                {
                    session.interrupted = true;
                    session.reason = "tester missed the continuous startup observation budget";
                    return false;
                }
                last_poll_us = now;
                if (now >= next_probe_us)
                {
                    uint8_t identity[10];
                    uint8_t frame[60];
                    session.requester(identity);
                    make_delay_request(frame, s_local_mac, identity, session.config.domain, 0, ++s_sequence);
                    Timestamp transmit{};
                    if (!session.send(frame, sizeof(frame), &transmit))
                        return false;
                    next_probe_us = esp_timer_get_time() +
                        static_cast<int64_t>(std::max(session.config.request_interval_ms,
                                                      session.config.response_window_ms)) * 1000;
                }
                Packet packet{};
                const int64_t deadline = esp_timer_get_time() + 20000;
                while (session.pull(packet, deadline))
                {
                    Message message{};
                    if (!session.decode(packet, message))
                    {
                        if (session.interrupted || session.protocol_fault)
                            return false;
                        continue;
                    }
                    if (armed && packet.received_us >= lower_bound_us &&
                        (message.type == 11 || message.type == 0 || message.type == 8 || message.type == 9))
                    {
                        ++service_packets;
                        if (first_service_us == 0)
                            first_service_us = packet.received_us;
                    }
                }
                return session.healthy();
            }

            bool wait_until(int64_t deadline)
            {
                while (session.healthy() && esp_timer_get_time() < deadline)
                {
                    if (!poll())
                        return false;
                    vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(10)));
                }
                return session.healthy();
            }
        };

        Result startup_listening(Session &session, Evidence &evidence)
        {
            if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                !session.control())
            {
                evidence.add("Healthy pre-startup controls unavailable; worker was not stopped. ");
                return Result::CouldNotRun;
            }
            int64_t announce = 0;
            if (!interval_us(session.config.policy.log_announce_interval, announce))
                return Result::CouldNotRun;
            const int64_t listening = 2 * announce;
            StartupMonitor monitor{session};
            s_startup_restore.begin(session.config);
            if (!session.config.assist(Test::StartupListening, false, session.stop, session.config.context,
                                       StartupMonitor::assist_poll, &monitor))
            {
                evidence.add("Stopped, timing-qualified worker not confirmed; restore the DUT manually if changed. ");
                return Result::CouldNotRun;
            }
            session.progress("Worker stopped: draining possible in-flight service before the start reference");
            if (!monitor.wait_until(esp_timer_get_time() +
                                    static_cast<int64_t>(session.config.processing_allowance_ms) * 1000))
                return Result::CouldNotRun;
            monitor.lower_bound_us = esp_timer_get_time();
            monitor.last_poll_us = 0;
            monitor.armed = true;
            if (!session.config.assist(Test::StartupListening, true, session.stop, session.config.context,
                                       StartupMonitor::assist_poll, &monitor))
            {
                evidence.add("Instrumented worker start and qualified timing were not confirmed; restore the DUT manually. ");
                return Result::CouldNotRun;
            }
            const int64_t upper_bound = esp_timer_get_time();
            session.progress("Observing through the latest possible two-Announce startup-listening boundary");
            if (!monitor.wait_until(upper_bound + listening))
            {
                evidence.add("Continuous startup observation interrupted: %s. ", session.reason);
                return Result::CouldNotRun;
            }
            const bool recovered = recover_foreign(session, evidence, esp_timer_get_time() +
                static_cast<int64_t>(session.config.recovery_ms) * 1000 + healthy_window_us(session.config));
            s_startup_restore.completed(recovered);
            const Result result = classify_startup_reference(monitor.lower_bound_us, upper_bound, listening,
                monitor.first_service_us, recovered, session.interrupted || session.stop.load());
            evidence.add("Operator-instrumented worker-start bounds=[%lld,%lld] us; listening=%lld us; "
                         "first observed service=%lld us; service packets=%lu; healthy post-control=%u. "
                         "The acknowledgement is an upper bound, not an exact worker timestamp; activity in the "
                         "uncertainty interval is inconclusive. Timing qualification at reset requires the operator's "
                         "independent instrumented evidence. ",
                         static_cast<long long>(monitor.lower_bound_us), static_cast<long long>(upper_bound),
                         static_cast<long long>(listening), static_cast<long long>(monitor.first_service_us),
                         static_cast<unsigned long>(monitor.service_packets), recovered);
            return result;
        }

        struct StandbyMonitor
        {
            Session &session;
            ForeignStream &stream;
            Evidence &evidence;
            int64_t last_poll_us = 0;
            int64_t next_probe_us = 0;
            int64_t first_unexpected_us = 0;
            uint32_t unexpected = 0;
            bool violated = false;

            static bool assist_poll(void *context)
            {
                return static_cast<StandbyMonitor *>(context)->poll();
            }

            bool poll()
            {
                if (!session.healthy())
                    return false;
                const int64_t now = esp_timer_get_time();
                if (!standby_poll_valid(now, last_poll_us, stream.schedule))
                {
                    session.interrupted = true;
                    session.reason = "tester missed the continuous standby refresh/observation budget; fault interaction inconclusive";
                    return false;
                }
                last_poll_us = now;
                if (now >= stream.next_us && !stream.send(session, false, false))
                    return false;
                if (now >= next_probe_us)
                {
                    uint8_t identity[10];
                    uint8_t frame[60];
                    session.requester(identity);
                    make_delay_request(frame, s_local_mac, identity, session.config.domain, 0, ++s_sequence);
                    Timestamp transmit{};
                    if (!session.send(frame, sizeof(frame), &transmit))
                        return false;
                    next_probe_us = esp_timer_get_time() +
                        static_cast<int64_t>(std::max(session.config.request_interval_ms,
                                                      session.config.response_window_ms)) * 1000;
                }
                Packet packet{};
                const int64_t deadline = esp_timer_get_time() + 20000;
                while (session.pull(packet, deadline))
                {
                    Message message{};
                    if (!session.decode(packet, message))
                    {
                        if (session.interrupted)
                            return false;
                        if (session.protocol_fault)
                            violated = true;
                        continue;
                    }
                    if ((message.type == 11 || message.type == 0 || message.type == 8 || message.type == 9) &&
                        packet.received_us >= stream.schedule.silence_after_us)
                    {
                        violated = true;
                        ++unexpected;
                        if (first_unexpected_us == 0)
                        {
                            first_unexpected_us = packet.received_us;
                            evidence.add("Unexpected DUT service in standby: type=%u at %lld us. ",
                                         message.type, static_cast<long long>(packet.received_us));
                        }
                    }
                }
                return session.healthy();
            }

            bool wait_until(int64_t deadline)
            {
                if (esp_timer_get_time() >= deadline)
                {
                    session.interrupted = true;
                    session.reason = "tester missed the standby observation window";
                    return false;
                }
                while (session.healthy() && esp_timer_get_time() < deadline)
                {
                    if (!poll())
                        return false;
                    vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(10)));
                }
                return session.healthy();
            }

            void record(const char *phase)
            {
                evidence.add("\n%s: fresh Announce sends=%lu last sequence=%u last TX=[%lld,%lld] us; "
                             "standby violations=%lu first=%lld us protocol-fault=%u; %s. ",
                             phase, static_cast<unsigned long>(stream.schedule.fresh_sends), stream.sequences.value,
                             static_cast<long long>(stream.schedule.last_begin_us),
                             static_cast<long long>(stream.schedule.last_end_us),
                             static_cast<unsigned long>(unexpected), static_cast<long long>(first_unexpected_us),
                             session.protocol_fault, session.reason);
            }
        };

        Result standby_recovery(Test test, Session &session, Evidence &evidence)
        {
            const Result outcome = [&]() -> Result
            {
                const char *fault = test == Test::StandbyPpsRecovery ? "PPS" :
                                    test == Test::StandbyLinkRecovery ? "server link" :
                                    test == Test::StandbyRestartRecovery ? "restart" : "GNSS";
                IntervalTiming timing{};
                if (!standby_timing(session.config, timing))
                {
                    evidence.add("Standby timing cannot guarantee two foreign refreshes within ranked startup listening "
                                 "or separate qualification from expiry; fault not requested. ");
                    return Result::CouldNotRun;
                }
                session.progress("Standby recovery: stable healthy before-control");
                if (!service_window(session, esp_timer_get_time() + healthy_window_us(session.config), 0, true) ||
                    !session.control())
                {
                    evidence.add("Healthy before-control unavailable; %s fault not requested: %s. ", fault, session.reason);
                    return Result::CouldNotRun;
                }
                uint8_t identity[10];
                session.requester(identity);
                Message better{};
                if (!ranked_source_dataset(session.master, true, identity, better))
                {
                    evidence.add("Strictly better non-self candidate unavailable; %s fault not requested. ", fault);
                    return Result::CouldNotRun;
                }
                ForeignStream stream(session, better, identity, evidence, "assisted standby recovery", StandbyLogInterval);
                if (!stream.send(session))
                    return Result::CouldNotRun;
                if (!service_window(session, esp_timer_get_time() + timing.check_us, 0, true, nullptr, false))
                {
                    evidence.add("First Announce incorrectly suppressed service: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                if (!stream.send(session))
                    return Result::CouldNotRun;
                if (!service_window(session, stream.schedule.silence_after_us + timing.check_us,
                                    stream.schedule.silence_after_us, false))
                {
                    evidence.add("Qualified candidate did not establish standby; %s fault not requested: %s. ", fault, session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                StandbyMonitor monitor{session, stream, evidence};
                if (!monitor.poll())
                    return Result::CouldNotRun;
                const bool expire_unqualified = test == Test::StandbyExpiryUnqualified;
                const bool ntp_holdover = test == Test::SelectionNtpHoldover;
                if (ntp_holdover && session.config.observation == nullptr)
                {
                    evidence.add("Bounded NTP load callback unavailable; GNSS fault not requested. ");
                    return Result::CouldNotRun;
                }
                evidence.add("Scope: controlled %s loss/restart and restoration; tester Ethernet link remains connected. "
                             "Operator verifies the DUT fault and fresh qualification; silence alone does not prove a fault. "
                             "Fresh foreign Announces continue %s; refresh=%lld us poll budget=%lld us. ",
                             fault, expire_unqualified ? "through the fault prompt" : "during both prompts",
                             static_cast<long long>(stream.interval), static_cast<long long>(StandbyPollBudgetUs));
                if (expire_unqualified || ntp_holdover)
                    s_timing_restore.begin(session.config);
                const bool injected = session.config.assist(test, false, session.stop,
                    session.config.context, StandbyMonitor::assist_poll, &monitor);
                monitor.record("Fault injection prompt completed");
                if (!injected)
                {
                    evidence.add("%s fault not acknowledged; restore the DUT manually if changed. ", fault);
                    return Result::CouldNotRun;
                }
                const int64_t withdrawal = (static_cast<int64_t>(session.config.withdrawal_ms) +
                                            session.config.processing_allowance_ms) * 1000;
                if (ntp_holdover)
                {
                    session.progress("Operator-confirmed NTP holdover: observing bounded load while PTP remains in standby");
                    session.config.observation(true, session.config.context);
                    const int64_t end = esp_timer_get_time() +
                        (static_cast<int64_t>(session.config.observation_ms) + session.config.response_window_ms) * 1000;
                    const bool observed = monitor.wait_until(end);
                    session.config.observation(false, session.config.context);
                    monitor.record("NTP holdover load completed");
                    const bool restored = session.config.assist(test, true, session.stop, session.config.context,
                        session.interrupted ? nullptr : StandbyMonitor::assist_poll, &monitor);
                    if (!restored || !observed || !session.healthy())
                    {
                        evidence.add("Holdover observation or GNSS restoration incomplete; restore the DUT and rerun before dependent PTP tests. ");
                        return Result::CouldNotRun;
                    }
                    const int64_t hold = std::max({static_cast<int64_t>(session.config.observation_ms) * 1000,
                                                   2 * stream.schedule.lifetime_us, healthy_window_us(session.config)});
                    if (!monitor.wait_until(esp_timer_get_time() + hold) || !stream.send(session))
                        return Result::CouldNotRun;
                    const bool recovered = service_window(session, stream.schedule.no_early_service_until(),
                                                           stream.schedule.silence_after_us, false) &&
                        recover_interval(session, evidence, stream.schedule.cleanup_until() + timing.recovery_us) &&
                        session.control();
                    s_timing_restore.completed(recovered);
                    if (!recovered)
                    {
                        evidence.add("Healthy post-holdover PTP recovery unavailable; timing restoration remains unresolved: %s. ",
                                     session.reason);
                        return Result::CouldNotRun;
                    }
                    evidence.add("Operator independently confirmed GNSS loss and legitimate NTP holdover; "
                                 "the concurrent NTP worker reports IPv4/IPv6 response and authentication evidence. "
                                 "PTP remained in standby through restoration, then recovered after candidate expiry. "
                                 "No internal holdover-state or external time-accuracy claim is inferred from packets. ");
                    return monitor.violated ? Result::Fail : Result::Pass;
                }
                if (expire_unqualified)
                {
                    const int64_t expiry_deadline = std::max(stream.schedule.cleanup_until(),
                        esp_timer_get_time() + withdrawal) +
                        static_cast<int64_t>(session.config.processing_allowance_ms) * 1000;
                    evidence.add("Foreign refresh stopped while independently confirmed unqualified; "
                                 "last TX=[%lld,%lld] us; expiry observation ends=%lld us. ",
                                 static_cast<long long>(stream.schedule.last_begin_us),
                                 static_cast<long long>(stream.schedule.last_end_us),
                                 static_cast<long long>(expiry_deadline));
                    session.progress("GNSS unqualified: allowing the foreign candidate to expire without refresh");
                    const bool silent = service_window(session, expiry_deadline,
                                                       stream.schedule.silence_after_us, false);
                    const bool restored = session.config.assist(test, true, session.stop,
                                                                 session.config.context, nullptr, nullptr);
                    if (!restored || !session.healthy())
                    {
                        evidence.add("Timing restoration or observation incomplete; restore GNSS and rerun this test before dependent PTP tests. ");
                        return Result::CouldNotRun;
                    }
                    session.progress("Timing restored: checking for service immediately, without an assumed new listening delay");
                    const bool recovered = recover_interval(session, evidence,
                        esp_timer_get_time() + timing.recovery_us) && session.control();
                    s_timing_restore.completed(recovered);
                    if (!recovered)
                    {
                        evidence.add("Fresh post-qualification control unavailable; timing restoration remains unresolved: %s. ", session.reason);
                        return Result::CouldNotRun;
                    }
                    evidence.add("Candidate expiry while unqualified: silence=%u; bounded immediate recovery=1. "
                                 "The operator must confirm unqualified timing and no worker/generation reset throughout; "
                                 "wire behavior alone cannot prove internal table removal. ", silent);
                    return session.interrupted ? Result::CouldNotRun : silent && !monitor.violated ? Result::Pass : Result::Fail;
                }
                session.progress("Fault acknowledged: maintaining foreign-master refresh and quiet service");
                const bool fault_observed = monitor.wait_until(esp_timer_get_time() + withdrawal +
                    static_cast<int64_t>(session.config.timeout_ms) * 1000);
                monitor.record("Fault observation completed");
                const bool restored = session.config.assist(test, true, session.stop,
                    session.config.context, session.interrupted ? nullptr : StandbyMonitor::assist_poll, &monitor);
                monitor.record("Restoration prompt completed");
                if (!restored || !fault_observed || !session.healthy())
                {
                    evidence.add("Fresh GNSS/PPS qualification not acknowledged or observation interrupted; "
                                 "restore the DUT manually if necessary. ");
                    return Result::CouldNotRun;
                }
                session.progress("DUT restored: PTP must remain in standby while the foreign master stays active");
                const int64_t hold = std::max({static_cast<int64_t>(session.config.observation_ms) * 1000,
                                               2 * stream.schedule.lifetime_us, healthy_window_us(session.config)});
                if (!monitor.wait_until(esp_timer_get_time() + hold))
                    return Result::CouldNotRun;
                monitor.record("Post-restoration standby completed");
                if (!stream.send(session))
                    return Result::CouldNotRun;
                evidence.add("Foreign refresh stopped: no service before %lld us; full-service recovery deadline=%lld us. ",
                             static_cast<long long>(stream.schedule.no_early_service_until()),
                             static_cast<long long>(stream.schedule.cleanup_until() + timing.recovery_us));
                session.progress("Foreign master withdrawn: verifying no early service and bounded healthy recovery");
                if (!service_window(session, stream.schedule.no_early_service_until(),
                                    stream.schedule.silence_after_us, false) ||
                    !recover_interval(session, evidence, stream.schedule.cleanup_until() + timing.recovery_us))
                {
                    evidence.add("Standby expiry/recovery contradicted: %s. ", session.reason);
                    return session.interrupted ? Result::CouldNotRun : Result::Fail;
                }
                evidence.add("%s standby interaction complete: post-restoration hold=%lld us; violations=%lu. "
                             "Continued injection spans any worker restart/listening interval; on-wire silence does not "
                             "prove internal candidate clearing or old-generation queue handling. ",
                             fault, static_cast<long long>(hold), static_cast<unsigned long>(monitor.unexpected));
                return monitor.violated ? Result::Fail : Result::Pass;
            }();
            const bool cleaned = cleanup_foreign(session, evidence);
            return classify_foreign(outcome, cleaned, session.interrupted, session.stop.load());
        }

        Result assisted_recovery(Test test, Session &session, Evidence &evidence)
        {
            session.progress("Healthy pre-fault baseline");
            const Result baseline = observe(session, evidence);
            if (baseline != Result::Pass || !session.control())
            {
                evidence.add("Healthy pre-fault control unavailable; fault not requested. ");
                return Result::CouldNotRun;
            }
            s_active.store(false);
            const bool injected = session.config.assist(test, false, session.stop, session.config.context, nullptr, nullptr);
            s_active.store(true);
            if (!injected)
            {
                evidence.add("Fault injection not acknowledged; missing operator/fixture prerequisite. ");
                return Result::CouldNotRun;
            }
            if (!session.healthy())
            {
                evidence.add("Tester link/generation lost; server absence cannot be proven from this observer. Restore and retry with independent capture. ");
                return Result::CouldNotRun;
            }
            session.rebase();
            const int64_t acknowledged = esp_timer_get_time();
            const int64_t withdrawal_deadline = acknowledged +
                (static_cast<int64_t>(session.config.withdrawal_ms) + session.config.processing_allowance_ms) * 1000;
            const int64_t silence_end = withdrawal_deadline + static_cast<int64_t>(session.config.timeout_ms) * 1000;
            session.progress("Fault acknowledged: waiting configured qualification withdrawal bound then observing silence");
            const bool withdrawn = service_window(session, silence_end, withdrawal_deadline, false);
            evidence.add("fault=%u acknowledged; withdrawal-bound=%u ms allowance=%u ms silent-window=%u ms withdrawn=%u; ",
                         static_cast<unsigned>(test), session.config.withdrawal_ms, session.config.processing_allowance_ms,
                         session.config.timeout_ms, withdrawn);
            s_active.store(false);
            const bool restored = session.config.assist(test, true, session.stop, session.config.context, nullptr, nullptr);
            s_active.store(true);
            if (!restored)
            {
                evidence.add("Restoration and fresh GNSS/PPS qualification not acknowledged. ");
                return Result::CouldNotRun;
            }
            if (!session.healthy())
            {
                evidence.add("Tester interruption/overflow makes fault observation inconclusive: %s. ", session.reason);
                return Result::CouldNotRun;
            }
            session.rebase();
            session.master = {};
            int64_t announce_interval = 0;
            interval_us(session.config.policy.log_announce_interval, announce_interval);
            const uint32_t listening_ms = session.config.policy.selection == SelectionMode::Ranked
                ? static_cast<uint32_t>(announce_interval * 2 / 1000) : 0;
            evidence.add("Ranked restart-listening allowance=%u ms, separate from GNSS/PPS withdrawal=%u ms. ",
                         listening_ms, session.config.withdrawal_ms);
            if (!session.learn(session.config.recovery_ms + listening_ms) || !session.control())
            {
                evidence.add("Post-restoration healthy control unavailable: %s. ", session.reason);
                return Result::CouldNotRun;
            }
            const Result recovered = observe(session, evidence);
            evidence.add("Restoration acknowledged qualified; fresh generation-local pair/request keys used. "
                         "Time properties checked against supported arbitrary or GNSS-derived PTP/TAI policy; traceability claims rejected; "
                         "real leap event NOT exercised (GNSS simulator required). "
                         "DMA timestamp provenance/reset ordering requires independent capture and server instrumentation. ");
            if (recovered == Result::CouldNotRun)
                return recovered;
            return withdrawn && recovered == Result::Pass ? Result::Pass : Result::Fail;
        }

        Result run_extended(Test test, const Config &config, const std::atomic<bool> &stop, char *output, size_t output_size)
        {
            Evidence evidence{output, output_size};
            auto unavailable = [&evidence](const char *reason)
            {
                evidence.add("CouldNotRun: %s", reason);
                return Result::CouldNotRun;
            };
            if (const char *pending = pending_reason(test))
                return unavailable(pending);
            if (const char *issue = policy_prerequisite(test, config))
                return unavailable(issue);
            if (config.observation_ms < 1000 || config.observation_ms > 120000 || config.samples < 2 || config.samples > 64 ||
                config.response_window_ms < 1 || config.response_window_ms > 5000 || config.request_interval_ms < 100 ||
                config.request_interval_ms > 5000 || config.processing_allowance_ms > 5000 ||
                config.recovery_ms < 1000 || config.recovery_ms > 600000)
                return unavailable("bounded observation/sample/rate configuration invalid");
            if (config.firmware_mode != FirmwareMode::Unknown && !mode_has_l2(config.firmware_mode))
                return unavailable("operator-declared firmware does not include PTP L2");
            const bool assisted = requires_operator(test);
            if (test == Test::NegativeDelay && (!config.isolated_link || !config.negative_enabled))
                return unavailable("negative tests require compile-time opt-in and explicit isolated local-link authorization");
            if ((test == Test::OptionalSignedRejection || test == Test::InvalidAuthenticationRefresh ||
                 test == Test::KeyRotation || test == Test::SelectionNtpPolicyLoad) && !config.negative_enabled)
                return unavailable("authentication rejection scenarios require negative-test opt-in");
            if (test == Test::KeyRotation && !config.rotation_enabled)
                return unavailable("staged key rotation requires the separate isolated-fixture opt-in");
            if (requires_suppression_opt_in(test) &&
                (!config.isolated_link || !config.suppression_enabled ||
                    config.policy.foreign_timeout_ms < 1000 || config.policy.foreign_timeout_ms > 120000))
                return unavailable("foreign-candidate scenarios require opt-in, isolated link with no other master, and validated bounded DUT policy");
            if (assisted && (!config.isolated_link || !config.assisted_enabled || config.assist == nullptr ||
                    config.withdrawal_ms == 0 || config.withdrawal_ms > 120000))
                return unavailable("assisted phases require opt-in, isolated link, operator callback, and verified DUT qualification limits");
            if (test == Test::PtpOnly && (config.firmware_mode != FirmwareMode::PtpOnly || !config.no_ip_confirmed))
                return unavailable("operator must identify PTP-only DUT with qualified GNSS/PPS and no usable IPv4/IPv6 (not UDP silence discovery)");
            if (involves_ntp(test) && (!config.combined_enabled || !config.isolated_link ||
                                       !mode_has_ntp(config.firmware_mode) || !mode_has_l2(config.firmware_mode)))
                return unavailable("combined workload requires explicit opt-in, authorized link, and declared combined firmware");
            if (!ready() || s_run_mutex == nullptr || xSemaphoreTake(s_run_mutex, 0) != pdTRUE)
                return unavailable("PTP dispatcher unavailable or busy");
            RunGuard guard;
            if (std::memcmp(config.target_mac, s_local_mac, 6) == 0)
                return unavailable("target is tester MAC");
            Session session{config, stop};
            if (signed_policy(config) && configured_key(config) &&
                !session.auth.initialize(config.key_id, config.key_value, config.policy.spp))
                return unavailable("PTP key import/crypto initialization failed; keys must be nonzero-ID, exactly 32 alphanumeric characters");
            session.rebase();
            if (config.sequence_seed_enabled)
                s_sequence = config.sequence_seed;
            s_target_prefix.store(mac_prefix(config.target_mac));
            s_target_suffix.store(read16(config.target_mac + 4));
            s_active.store(true);
            if (s_rotation_restore.pending)
            {
                if (test != Test::KeyRotation || !s_rotation_restore.matches(config))
                    return unavailable("previous staged key fixture is not restored; select the same target and rerun the rotation test to verify restoration before dependent PTP tests");
                s_active.store(false);
                const bool confirmed = config.assist(Test::KeyRotationRestore, true, stop, config.context, nullptr, nullptr);
                s_active.store(true);
                if (!confirmed || !session.healthy())
                    return unavailable("original key fixture restoration was not confirmed");
                session.rebase();
                if (!session.learn(config.recovery_ms) || !session.control())
                    return unavailable("original key fixture is not yet a healthy signed service");
                Authenticator fixture_key;
                if (!fixture_key.initialize(config.rotation_key_id, config.rotation_key_value, config.policy.spp) ||
                    signed_delay_probe(session, evidence, fixture_key, false, "restored fixture key rejection") != Result::Pass)
                    return unavailable("public fixture key rejection or signed post-control is not verified; dependent tests remain blocked");
                s_rotation_restore.completed(true);
                evidence.add("Previously interrupted rotation fixture restored and independently checked. ");
                session.rebase();
            }
            if (s_startup_restore.pending)
            {
                if (test != Test::StartupListening || !s_startup_restore.matches(config))
                    return unavailable("previous startup fixture is not restored; select the same target and rerun startup listening to verify worker recovery before dependent PTP tests");
                s_active.store(false);
                const bool confirmed = config.assist(Test::StartupRestore, true, stop, config.context, nullptr, nullptr);
                s_active.store(true);
                if (!confirmed || !session.healthy())
                    return unavailable("previously stopped PTP worker restoration was not confirmed");
                session.rebase();
                if (!session.learn(config.recovery_ms) || !session.control())
                    return unavailable("restored PTP worker lacks a fresh healthy positive control");
                s_startup_restore.completed(true);
                evidence.add("Previously interrupted startup fixture restored and independently checked. ");
                session.rebase();
            }
            if (s_timing_restore.pending)
            {
                if (test != Test::StandbyExpiryUnqualified || !s_timing_restore.matches(config))
                    return unavailable("previous timing fixture is not restored; select the same target and rerun candidate expiry (test 58) to verify recovery before dependent PTP tests");
                s_active.store(false);
                const bool confirmed = config.assist(Test::TimingRestore, true, stop, config.context, nullptr, nullptr);
                s_active.store(true);
                if (!confirmed || !session.healthy())
                    return unavailable("timing fixture restoration was not confirmed");
                if (s_foreign_cleanup.pending && !cleanup_foreign(session, evidence))
                    return unavailable("previous foreign candidate or restored timing lacks a healthy cleanup control");
                session.rebase();
                if (!session.learn(config.recovery_ms) || !session.control())
                    return unavailable("restored timing lacks a fresh healthy PTP positive control");
                s_timing_restore.completed(true);
                evidence.add("Previously interrupted timing fixture restored and independently checked. ");
                session.rebase();
            }
            if (s_foreign_cleanup.pending &&
                ((!requires_suppression_opt_in(test)) ||
                 !cleanup_foreign(session, evidence)))
                return unavailable("previous foreign-candidate cleanup incomplete; rerun the foreign-master policy and recovery test to establish healthy service before dependent PTP tests");
            evidence.add("MAC=%02x:%02x:%02x:%02x:%02x:%02x domain=%u mode=%u; L2 untagged multicast two-step E2E; "
                         "sequence-seed=%s; ", config.target_mac[0], config.target_mac[1], config.target_mac[2],
                         config.target_mac[3], config.target_mac[4], config.target_mac[5], config.domain,
                         static_cast<unsigned>(config.firmware_mode), config.sequence_seed_enabled ? "controlled Delay_Req wrap" : "natural (Sync wrap not forced)");
            evidence.add("selection=%s; authentication=%s; policy v%u; identity=%s; provenance=%s. ",
                         selection_text(config.policy.selection), authentication_text(config.policy.authentication),
                         config.policy.version,
                         config.policy.identity ? config.policy.identity : "unavailable",
                         config.policy.provenance ? config.policy.provenance : "unavailable");
            if (!session.healthy() || !session.learn(config.timeout_ms))
            {
                evidence.add("%s; RX-drops=%u. ", session.reason, s_drops.load() - session.drops);
                return session.interrupted || signed_policy(config) || assisted || test == Test::NegativeDelay ||
                       requires_suppression_opt_in(test)
                    ? Result::CouldNotRun : Result::Fail;
            }
            evidence.add("Observed outgoing=%s (not inferred from configured enabled/required policy). ",
                         session.master.authenticated ? "verified signed v2.1" : "unsigned v2.0");
            if (const char *issue = service_prerequisite(test, session.master))
                return unavailable(issue);
            Result result;
            if (test == Test::OptionalUnsignedService)
                result = optional_unsigned_service(session, evidence);
            else if (test == Test::OptionalSignedRejection)
                result = optional_signed_rejection(session, evidence);
            else if (test == Test::SignedMessages || test == Test::SignedExchange)
                result = signed_service(session, evidence, test == Test::SignedExchange);
            else if (test == Test::InvalidAuthentication || test == Test::AuthenticationPolicy)
                result = authentication_delays(session, evidence, test == Test::AuthenticationPolicy);
            else if (test == Test::AuthenticationBeforeSelection)
                result = authentication_before_selection(session, evidence);
            else if (test == Test::AnnounceAuthenticationPolicy)
                result = announce_authentication_policy(session, evidence);
            else if (test == Test::InvalidAuthenticationRefresh)
                result = invalid_authentication_refresh(session, evidence);
            else if (test == Test::NegativeDelay)
                result = negative_delays(session, evidence);
            else if (test == Test::ForeignAnnounce)
                result = foreign_suppression(session, evidence);
            else if (test == Test::RankingPrecedence)
                result = ranking_precedence(session, evidence);
            else if (test == Test::SourceIsolation)
                result = source_isolation(session, evidence);
            else if (test == Test::SequencePolicy)
                result = sequence_policy(session, evidence);
            else if (test == Test::DatasetChanges)
                result = dataset_changes(session, evidence);
            else if (test == Test::MultipleMasters)
                result = multiple_masters(session, evidence);
            else if (test == Test::ForeignCapacity)
                result = foreign_capacity(session, evidence);
            else if (test == Test::SelectionNtpLoad || test == Test::SelectionNtpPolicyLoad)
                result = selection_ntp_load(test, session, evidence);
            else if (test == Test::IntervalExpiry)
                result = interval_expiry(session, evidence);
            else if (is_standby_recovery(test))
                result = standby_recovery(test, session, evidence);
            else if (test == Test::KeyRotation)
                result = key_rotation(session, evidence);
            else if (test == Test::StartupListening)
                result = startup_listening(session, evidence);
            else if (assisted)
                result = assisted_recovery(test, session, evidence);
            else
                result = observe(session, evidence, test == Test::CombinedLoad);
            if (!session.healthy())
                result = Result::CouldNotRun;
            if (evidence.truncated)
            {
                const char suffix[] = " [Evidence truncated; result inconclusive]";
                if (output_size > sizeof(suffix))
                    std::memcpy(output + output_size - sizeof(suffix), suffix, sizeof(suffix));
                result = Result::CouldNotRun;
            }
            return result;
        }

        constexpr uint16_t UdpEventPort = 319;
        constexpr uint16_t UdpGeneralPort = 320;
        constexpr char UdpIpv4Group[] = "224.0.1.129";
        constexpr char UdpIpv6Group[] = "ff02::181";

        struct UdpChannel
        {
            int family = 0;
            int event_socket = -1;
            int general_socket = -1;
            sockaddr_storage destination{};
            socklen_t destination_length = 0;
            in_addr target4{};
            in6_addr target6{};
            UdpObservation observation{};
            uint16_t next_sequence = 0;
            uint32_t selected_packets = 0;
            uint32_t service_after_withdrawal = 0;
        };

        struct UdpSession
        {
            const UdpConfig &config;
            const std::atomic<bool> &stop;
            Evidence &evidence;
            std::array<UdpChannel, 2> channels{};
            size_t count = 0;
            uint8_t local_mac[6] = {};
            int interface_index = 0;
            uint32_t generation = 0;
            bool interrupted = false;
            const char *reason = "completed";
            int socket_error = 0;
            int64_t pair_cutoff_us = 0;
            int64_t withdrawal_deadline_us = 0;
            bool probe_active = false;
            bool probe_response = false;
            uint16_t probe_sequence = 0;
            uint8_t probe_requester[10] = {};
            UdpChannel *probe_channel = nullptr;

            ~UdpSession()
            {
                for (auto &channel : channels)
                {
                    for (int socket_fd : {channel.event_socket, channel.general_socket})
                        if (socket_fd >= 0 && close(socket_fd) != 0)
                            ESP_LOGW("ptp_udp_tester", "Closing PTP L3 UDP socket failed: errno=%d", errno);
                }
            }

            bool fail(const char *text, int error = 0)
            {
                interrupted = true;
                reason = text;
                socket_error = error;
                return false;
            }

            bool healthy()
            {
                if (stop.load())
                    return fail("cancelled");
                if (!config.link_up->load() || config.link_generation->load() != generation)
                    return fail("tester Ethernet link or generation changed");
                return !interrupted;
            }

            bool open_socket(UdpChannel &channel, uint16_t port, int &socket_fd)
            {
                socket_fd = socket(channel.family, SOCK_DGRAM, IPPROTO_UDP);
                if (socket_fd < 0)
                    return fail("cannot create PTP multicast socket", errno);
                ifreq interface{};
                if (esp_netif_get_netif_impl_name(config.netif, interface.ifr_name) != ESP_OK)
                    return fail("cannot obtain Ethernet interface name");
                if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &interface, sizeof(interface)) != 0)
                    return fail("cannot bind PTP socket to Ethernet interface", errno);

                if (channel.family == AF_INET)
                {
                    esp_netif_ip_info_t local{};
                    if (esp_netif_get_ip_info(config.netif, &local) != ESP_OK || local.ip.addr == 0)
                        return fail("Ethernet IPv4 address is unavailable");
                    sockaddr_in group{};
                    group.sin_family = AF_INET;
                    group.sin_port = htons(port);
                    if (inet_pton(AF_INET, UdpIpv4Group, &group.sin_addr) != 1)
                        return fail("invalid built-in IPv4 multicast group");
                    if (bind(socket_fd, reinterpret_cast<const sockaddr *>(&group), sizeof(group)) != 0)
                        return fail("cannot bind IPv4 multicast group/port", errno);
                    ip_mreq membership{};
                    membership.imr_multiaddr = group.sin_addr;
                    membership.imr_interface.s_addr = local.ip.addr;
                    const uint8_t ttl = 1;
                    const uint8_t loop = 0;
                    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) != 0 ||
                        setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_IF, &membership.imr_interface, sizeof(membership.imr_interface)) != 0 ||
                        setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0 ||
                        setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) != 0)
                        return fail("cannot join/configure IPv4 multicast on Ethernet", errno);
                    if (port == UdpEventPort)
                    {
                        std::memcpy(&channel.destination, &group, sizeof(group));
                        channel.destination_length = sizeof(group);
                    }
                }
                else
                {
                    const int only_v6 = 1;
                    if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &only_v6, sizeof(only_v6)) != 0)
                        return fail("cannot restrict PTP IPv6 socket to IPv6", errno);
                    esp_ip6_addr_t local{};
                    if (esp_netif_get_ip6_linklocal(config.netif, &local) != ESP_OK)
                        return fail("Ethernet IPv6 link-local address is unavailable");
                    sockaddr_in6 group{};
                    group.sin6_family = AF_INET6;
                    group.sin6_port = htons(port);
                    group.sin6_scope_id = static_cast<uint32_t>(interface_index);
                    if (inet_pton(AF_INET6, UdpIpv6Group, &group.sin6_addr) != 1)
                        return fail("invalid built-in IPv6 multicast group");
                    if (bind(socket_fd, reinterpret_cast<const sockaddr *>(&group), sizeof(group)) != 0)
                        return fail("cannot bind IPv6 multicast group/port", errno);
                    ipv6_mreq membership{};
                    membership.ipv6mr_multiaddr = group.sin6_addr;
                    membership.ipv6mr_interface = static_cast<unsigned>(interface_index);
                    const uint8_t index = static_cast<uint8_t>(interface_index);
                    const uint8_t hops = 1;
                    const uint8_t loop = 0;
                    if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &membership, sizeof(membership)) != 0 ||
                        setsockopt(socket_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &index, sizeof(index)) != 0 ||
                        setsockopt(socket_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) != 0 ||
                        setsockopt(socket_fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop)) != 0)
                        return fail("cannot join/configure IPv6 multicast on Ethernet", errno);
                    if (port == UdpEventPort)
                    {
                        std::memcpy(&channel.destination, &group, sizeof(group));
                        channel.destination_length = sizeof(group);
                    }
                }
                return true;
            }

            bool open()
            {
                generation = config.link_generation->load();
                if (!healthy())
                    return false;
                interface_index = esp_netif_get_netif_impl_index(config.netif);
                if (interface_index <= 0 || interface_index > UINT8_MAX)
                    return fail("Ethernet interface index is unavailable or outside lwIP's supported range");
                if (esp_netif_get_mac(config.netif, local_mac) != ESP_OK || !valid_target(local_mac))
                    return fail("tester Ethernet MAC is unavailable");
                for (int family : {AF_INET, AF_INET6})
                {
                    if ((family == AF_INET && !config.ipv4) || (family == AF_INET6 && !config.ipv6))
                        continue;
                    UdpChannel &channel = channels[count++];
                    channel.family = family;
                    const char *target = family == AF_INET ? config.ipv4_target : config.ipv6_target;
                    void *address = family == AF_INET ? static_cast<void *>(&channel.target4)
                                                       : static_cast<void *>(&channel.target6);
                    if (target == nullptr || inet_pton(family, target, address) != 1)
                        return fail("selected PTP L3 UDP master address is missing or invalid");
                    std::memcpy(channel.observation.requester, local_mac, 3);
                    channel.observation.requester[3] = 0xff;
                    channel.observation.requester[4] = 0xfe;
                    std::memcpy(channel.observation.requester + 5, local_mac + 3, 3);
                    channel.observation.requester[0] ^= 0x80;
                    const uint64_t nonce = ++s_request_nonce;
                    for (size_t index = 0; index < 6; ++index)
                        channel.observation.requester[7 - index] ^= static_cast<uint8_t>(nonce >> (index * 8));
                    write16(channel.observation.requester + 8, family == AF_INET ? 1 : 2);
                    channel.next_sequence = static_cast<uint16_t>(nonce);
                    if (!open_socket(channel, UdpEventPort, channel.event_socket) ||
                        !open_socket(channel, UdpGeneralPort, channel.general_socket))
                        return false;
                }
                return count != 0 || fail("no PTP L3 UDP transport selected");
            }

            void progress(const char *text)
            {
                if (config.progress)
                    config.progress(text, config.context);
            }

            bool send(UdpChannel &channel, UdpMutation mutation = UdpMutation::Count)
            {
                if (!healthy() || !channel.observation.master_locked)
                    return false;
                uint8_t message[44];
                const uint16_t sequence = ++channel.next_sequence;
                const size_t length = make_udp_delay_request(message, local_mac, channel.observation.requester,
                                                             config.domain, channel.observation.master.minor_version,
                                                             sequence, mutation);
                const ssize_t sent = sendto(channel.event_socket, message, length, 0,
                                            reinterpret_cast<const sockaddr *>(&channel.destination),
                                            channel.destination_length);
                if (sent < 0 || static_cast<size_t>(sent) != length)
                    return fail("PTP multicast Delay_Req transmission failed", errno);
                const int64_t now = esp_timer_get_time();
                if (mutation == UdpMutation::Count)
                {
                    channel.observation.sequence = sequence;
                    channel.observation.outstanding = true;
                    channel.observation.request_deadline_us = now + static_cast<int64_t>(config.response_window_ms) * 1000;
                    channel.observation.next_request_us = now + static_cast<int64_t>(config.request_interval_ms) * 1000;
                    ++channel.observation.requests;
                }
                else
                {
                    probe_active = true;
                    probe_response = false;
                    probe_sequence = sequence;
                    std::memcpy(probe_requester, message + 20, sizeof(probe_requester));
                    probe_channel = &channel;
                }
                return true;
            }

            bool selected_source(const UdpChannel &channel, const sockaddr_storage &source) const
            {
                if (source.ss_family != channel.family)
                    return false;
                if (channel.family == AF_INET)
                    return reinterpret_cast<const sockaddr_in *>(&source)->sin_addr.s_addr == channel.target4.s_addr;
                return std::memcmp(&reinterpret_cast<const sockaddr_in6 *>(&source)->sin6_addr,
                                   &channel.target6, sizeof(channel.target6)) == 0;
            }

            bool read_socket(UdpChannel &channel, int socket_fd, bool event_socket)
            {
                uint8_t bytes[MaxMessageSize + 1]{};
                sockaddr_storage source{};
                socklen_t source_length = sizeof(source);
                const ssize_t received = recvfrom(socket_fd, bytes, sizeof(bytes), 0,
                                                  reinterpret_cast<sockaddr *>(&source), &source_length);
                if (received < 0)
                    return errno == EAGAIN || errno == EWOULDBLOCK || fail("PTP multicast receive failed", errno);
                if (!selected_source(channel, source))
                    return true;
                const int64_t now = esp_timer_get_time();
                const size_t length = static_cast<size_t>(received);
                if (length >= 5 && bytes[4] == config.domain)
                {
                    ++channel.selected_packets;
                    if (withdrawal_deadline_us && now >= withdrawal_deadline_us &&
                        ((bytes[0] & 0x0f) == 0 || (bytes[0] & 0x0f) == 8 || (bytes[0] & 0x0f) == 11))
                        ++channel.service_after_withdrawal;
                }
                if (probe_active && probe_channel == &channel &&
                    raw_udp_delay_match(bytes, length, channel.observation.master.source, probe_sequence, probe_requester))
                    probe_response = true;
                if (length > MaxMessageSize)
                {
                    ++channel.observation.unsupported;
                    return true;
                }
                Message message{};
                const ParseResult parsed = parse_message(bytes, length, config.domain, message);
                if (parsed == ParseResult::Ignore)
                    return true;
                if (parsed != ParseResult::Valid)
                {
                    if (parsed == ParseResult::Malformed)
                        ++channel.observation.malformed;
                    else
                        ++channel.observation.unsupported;
                    return true;
                }
                if (read16(bytes + 2) != length)
                {
                    ++channel.observation.malformed;
                    return true;
                }
                if ((message.type == 0 && !event_socket) ||
                    ((message.type == 8 || message.type == 11) && event_socket))
                    channel.observation.protocol_fault = true;
                if (pair_cutoff_us && now >= pair_cutoff_us && (message.type == 0 || message.type == 8))
                {
                    bool pending = false;
                    for (const Pair &pair : channel.observation.pairs.pending)
                        pending = pending || (pair.occupied && pair.sequence == message.sequence &&
                                              pair.minor_version == message.minor_version &&
                                              std::memcmp((pair.sync_seen ? pair.sync : pair.follow).source, message.source, 10) == 0);
                    if (!pending)
                        return true;
                }
                Packet packet{};
                packet.received_us = now;
                channel.observation.accept(message, packet);
                return true;
            }

            bool poll(int64_t deadline_us)
            {
                if (!healthy())
                    return false;
                const int64_t remaining = deadline_us - esp_timer_get_time();
                if (remaining <= 0)
                    return true;
                fd_set ready;
                FD_ZERO(&ready);
                int maximum = -1;
                for (size_t index = 0; index < count; ++index)
                    for (int socket_fd : {channels[index].event_socket, channels[index].general_socket})
                    {
                        FD_SET(socket_fd, &ready);
                        maximum = std::max(maximum, socket_fd);
                    }
                const int64_t wait_us = std::min<int64_t>(remaining, 50000);
                timeval timeout{static_cast<long>(wait_us / 1000000), static_cast<long>(wait_us % 1000000)};
                const int result = select(maximum + 1, &ready, nullptr, nullptr, &timeout);
                if (result < 0)
                    return errno == EINTR || fail("PTP multicast socket polling failed", errno);
                for (size_t index = 0; index < count && result > 0; ++index)
                {
                    UdpChannel &channel = channels[index];
                    if (FD_ISSET(channel.event_socket, &ready) && !read_socket(channel, channel.event_socket, true))
                        return false;
                    if (FD_ISSET(channel.general_socket, &ready) && !read_socket(channel, channel.general_socket, false))
                        return false;
                }
                return healthy();
            }

            bool drain()
            {
                for (unsigned iteration = 0; iteration < 256; ++iteration)
                {
                    if (!healthy())
                        return false;
                    fd_set ready;
                    FD_ZERO(&ready);
                    int maximum = -1;
                    for (size_t index = 0; index < count; ++index)
                        for (int socket_fd : {channels[index].event_socket, channels[index].general_socket})
                        {
                            FD_SET(socket_fd, &ready);
                            maximum = std::max(maximum, socket_fd);
                        }
                    timeval timeout{};
                    const int result = select(maximum + 1, &ready, nullptr, nullptr, &timeout);
                    if (result < 0)
                    {
                        if (errno == EINTR)
                            continue;
                        return fail("cannot drain stale PTP multicast packets", errno);
                    }
                    if (result == 0)
                        return true;
                    for (size_t index = 0; index < count; ++index)
                        for (int socket_fd : {channels[index].event_socket, channels[index].general_socket})
                            if (FD_ISSET(socket_fd, &ready))
                            {
                                uint8_t discard[MaxMessageSize + 1];
                                if (recvfrom(socket_fd, discard, sizeof(discard), 0, nullptr, nullptr) < 0 &&
                                    errno != EAGAIN && errno != EWOULDBLOCK)
                                    return fail("cannot discard stale PTP multicast packet", errno);
                            }
                }
                return fail("PTP multicast receive queue could not be drained");
            }
        };

        void reset_udp_observation(UdpChannel &channel, bool keep_master)
        {
            const Message master = channel.observation.master;
            const bool locked = keep_master && channel.observation.master_locked;
            uint8_t requester[10];
            std::memcpy(requester, channel.observation.requester, sizeof(requester));
            channel.observation = {};
            std::memcpy(channel.observation.requester, requester, sizeof(requester));
            channel.observation.master = locked ? master : Message{};
            channel.observation.master_locked = locked;
            channel.selected_packets = 0;
            channel.service_after_withdrawal = 0;
        }

        bool udp_quick_complete(UdpTest test, const UdpObservation &observation)
        {
            if (!observation.master_locked)
                return false;
            switch (test)
            {
            case UdpTest::Announce:
            case UdpTest::TimeProperties:
                return true;
            case UdpTest::SyncFollowUp:
                return observation.pairs.pairs != 0;
            default:
                return observation.responses != 0;
            }
        }

        Result collect_udp(UdpSession &session, UdpTest test, uint32_t duration_ms, bool notify_observation)
        {
            const bool sustained = test == UdpTest::Sustained || test == UdpTest::Simultaneous ||
                                   test == UdpTest::CombinedLoad;
            if (sustained)
            {
                if (!session.drain())
                    return Result::CouldNotRun;
                for (size_t index = 0; index < session.count; ++index)
                    reset_udp_observation(session.channels[index], true);
            }
            if (notify_observation && session.config.observation)
                session.config.observation(true, session.config.context);
            const int64_t start = esp_timer_get_time();
            const int64_t active_end = start + static_cast<int64_t>(duration_ms) * 1000;
            const int64_t deadline = active_end +
                                     (sustained ? static_cast<int64_t>(session.config.response_window_ms) * 1000 : 0);
            session.pair_cutoff_us = sustained ? active_end : 0;
            const bool send_requests = sustained || test == UdpTest::DelayExchange;
            while (session.healthy() && esp_timer_get_time() < deadline)
            {
                const int64_t now = esp_timer_get_time();
                bool complete = !sustained;
                for (size_t index = 0; index < session.count; ++index)
                {
                    UdpChannel &channel = session.channels[index];
                    UdpObservation &observation = channel.observation;
                    if (observation.outstanding && now >= observation.request_deadline_us)
                    {
                        ++observation.response_timeouts;
                        observation.outstanding = false;
                    }
                    if (send_requests && observation.master_locked && !observation.outstanding &&
                        now < active_end && now >= observation.next_request_us &&
                        (sustained || observation.requests == 0))
                        if (!session.send(channel))
                            break;
                    if (!sustained)
                        complete = complete && udp_quick_complete(test, observation);
                }
                if (complete || !session.healthy())
                    break;
                if (!session.poll(deadline))
                    break;
            }
            if (notify_observation && session.config.observation)
                session.config.observation(false, session.config.context);
            session.pair_cutoff_us = 0;
            Result result = session.healthy() ? Result::Pass : Result::CouldNotRun;
            for (size_t index = 0; index < session.count; ++index)
            {
                UdpChannel &channel = session.channels[index];
                UdpObservation &observation = channel.observation;
                if (sustained)
                    observation.pairs.expire(esp_timer_get_time(), true);
                Result family_result;
                if (sustained)
                    family_result = classify_udp_observation(observation, session.config.samples, session.interrupted);
                else if (session.interrupted || observation.source_changed)
                    family_result = Result::CouldNotRun;
                else
                    family_result = udp_quick_complete(test, observation) && !observation.protocol_fault &&
                                            !observation.malformed && !observation.unsupported && !observation.response_timeouts
                                        ? Result::Pass
                                        : Result::Fail;
                if (sustained && observation.announces == 0 && family_result == Result::Pass)
                    family_result = Result::Fail;
                if (family_result == Result::CouldNotRun)
                    result = Result::CouldNotRun;
                else if (family_result == Result::Fail && result == Result::Pass)
                    result = Result::Fail;
                session.evidence.add(
                    "IPv%u: %s; selected multicast packets=%lu; Announce=%lu; matched Sync/Follow_Up=%lu; "
                    "Delay_Req=%lu; matched Delay_Resp=%lu; response timeouts=%lu; malformed=%lu; unsupported=%lu; "
                    "duplicates=%lu; unmatched=%lu; sequence gaps=%lu; source changed=%u.\n",
                    channel.family == AF_INET ? 4U : 6U,
                    family_result == Result::Pass ? "Pass" : family_result == Result::Fail ? "Fail" : "Could not run",
                    static_cast<unsigned long>(channel.selected_packets), static_cast<unsigned long>(observation.announces),
                    static_cast<unsigned long>(observation.pairs.pairs), static_cast<unsigned long>(observation.requests),
                    static_cast<unsigned long>(observation.responses), static_cast<unsigned long>(observation.response_timeouts),
                    static_cast<unsigned long>(observation.malformed), static_cast<unsigned long>(observation.unsupported),
                    static_cast<unsigned long>(observation.pairs.duplicates), static_cast<unsigned long>(observation.pairs.missing),
                    static_cast<unsigned long>(observation.pairs.sequence_gaps), observation.source_changed);
                if (observation.master_locked)
                {
                    char description[768];
                    announce_text(observation.master, description, sizeof(description), "L3 UDP observer");
                    session.evidence.add("IPv%u selected master: %s\n", channel.family == AF_INET ? 4U : 6U, description);
                }
            }
            if (session.count == 2 && session.channels[0].observation.master_locked &&
                session.channels[1].observation.master_locked &&
                std::memcmp(session.channels[0].observation.master.grandmaster,
                            session.channels[1].observation.master.grandmaster, 8) != 0)
            {
                session.evidence.add("IPv4 and IPv6 advertise different grandmaster identities.\n");
                if (result == Result::Pass)
                    result = Result::Fail;
            }
            return result;
        }

        bool udp_control(UdpSession &session, UdpChannel &channel)
        {
            const uint32_t previous = channel.observation.responses;
            const uint32_t previous_timeouts = channel.observation.response_timeouts;
            if (!session.send(channel))
                return false;
            const int64_t deadline = channel.observation.request_deadline_us;
            while (channel.observation.outstanding && session.healthy() && esp_timer_get_time() < deadline)
                if (!session.poll(deadline))
                    break;
            const bool received = !channel.observation.outstanding && channel.observation.responses == previous + 1 &&
                                  !channel.observation.source_changed && !channel.observation.protocol_fault;
            if (!received)
            {
                channel.observation.outstanding = false;
                if (channel.observation.response_timeouts == previous_timeouts)
                    ++channel.observation.response_timeouts;
            }
            return received && session.healthy();
        }

        Result udp_negative(UdpSession &session)
        {
            if (session.count != 1)
            {
                session.evidence.add("Negative tests require one selected transport.\n");
                return Result::CouldNotRun;
            }
            UdpChannel &channel = session.channels[0];
            bool rejected = true;
            for (unsigned index = 0; index < static_cast<unsigned>(UdpMutation::Count); ++index)
            {
                const auto mutation = static_cast<UdpMutation>(index);
                session.progress("PTP L3 UDP: invalid Delay_Req with healthy before/after controls");
                if (!udp_control(session, channel))
                {
                    session.evidence.add("%s: healthy control before probe unavailable.\n", UdpMutationNames[index]);
                    return Result::CouldNotRun;
                }
                if (!session.send(channel, mutation))
                    return Result::CouldNotRun;
                const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(session.config.response_window_ms) * 1000;
                while (session.healthy() && esp_timer_get_time() < deadline)
                    if (!session.poll(deadline))
                        break;
                if (!session.healthy() || !udp_control(session, channel))
                {
                    session.evidence.add("%s: healthy control after probe unavailable.\n", UdpMutationNames[index]);
                    return Result::CouldNotRun;
                }
                const bool response = session.probe_response;
                session.probe_active = false;
                session.evidence.add("%s: before/after controls passed; matching invalid-request response=%s.\n",
                                     UdpMutationNames[index], response ? "observed" : "not observed");
                rejected = rejected && !response;
            }
            session.evidence.add("No observed response is evidence of rejection only within this observation; "
                                 "socket or network packet loss cannot be excluded.\n");
            return rejected ? Result::Pass : Result::Fail;
        }

        Test udp_assist_test(UdpTest test)
        {
            switch (test)
            {
            case UdpTest::GnssRecovery: return Test::GnssRecovery;
            case UdpTest::PpsRecovery: return Test::PpsRecovery;
            case UdpTest::LinkRecovery: return Test::LinkRecovery;
            case UdpTest::RestartRecovery: return Test::RestartRecovery;
            default: return Test::Count;
            }
        }

        Result udp_assisted(UdpSession &session, UdpTest test)
        {
            if (!session.config.assist || udp_assist_test(test) == Test::Count)
            {
                session.evidence.add("Operator assistance is unavailable.\n");
                return Result::CouldNotRun;
            }
            session.progress("PTP L3 UDP: healthy pre-fault sustained baseline");
            if (collect_udp(session, UdpTest::Sustained, session.config.observation_ms, false) != Result::Pass)
            {
                session.evidence.add("Healthy pre-fault baseline unavailable; no fault requested.\n");
                return Result::CouldNotRun;
            }
            const Test action = udp_assist_test(test);
            if (!session.config.assist(action, false, session.stop, session.config.context, nullptr, nullptr))
            {
                session.evidence.add("Fault action was not acknowledged; restore the DUT manually if changed.\n");
                return Result::CouldNotRun;
            }
            bool withdrawal_observed = false;
            if (session.healthy() && session.drain())
            {
                for (size_t index = 0; index < session.count; ++index)
                    session.channels[index].service_after_withdrawal = 0;
                const int64_t acknowledged = esp_timer_get_time();
                session.withdrawal_deadline_us = acknowledged + static_cast<int64_t>(session.config.withdrawal_ms) * 1000;
                const int64_t silence_end = session.withdrawal_deadline_us +
                                            static_cast<int64_t>(session.config.timeout_ms) * 1000;
                session.progress("PTP L3 UDP: verifying service withdrawal after the configured bound");
                while (session.healthy() && esp_timer_get_time() < silence_end)
                    if (!session.poll(silence_end))
                        break;
                withdrawal_observed = session.healthy();
                for (size_t index = 0; index < session.count; ++index)
                {
                    withdrawal_observed = withdrawal_observed &&
                                          session.channels[index].service_after_withdrawal == 0;
                    session.evidence.add("IPv%u service packets after withdrawal bound=%lu.\n",
                                         session.channels[index].family == AF_INET ? 4U : 6U,
                                         static_cast<unsigned long>(session.channels[index].service_after_withdrawal));
                }
                session.evidence.add("Withdrawal bound=%lu ms; silence window=%lu ms.\n",
                                     static_cast<unsigned long>(session.config.withdrawal_ms),
                                     static_cast<unsigned long>(session.config.timeout_ms));
            }
            session.withdrawal_deadline_us = 0;
            if (!session.config.assist(action, true, session.stop, session.config.context, nullptr, nullptr))
            {
                session.evidence.add("Restoration and fresh timing qualification not acknowledged; restore the DUT manually.\n");
                return Result::CouldNotRun;
            }
            if (!session.healthy() || !session.drain())
            {
                session.evidence.add("Tester link or socket interruption makes withdrawal inconclusive.\n");
                return Result::CouldNotRun;
            }
            for (size_t index = 0; index < session.count; ++index)
                reset_udp_observation(session.channels[index], false);
            session.progress("PTP L3 UDP: verifying fresh post-restoration multicast and E2E controls");
            if (collect_udp(session, UdpTest::DelayExchange, session.config.recovery_ms, false) != Result::Pass)
            {
                session.evidence.add("Fresh post-restoration E2E control unavailable.\n");
                return Result::CouldNotRun;
            }
            const Result recovered = collect_udp(session, UdpTest::Sustained, session.config.observation_ms, false);
            session.evidence.add("Operator confirmed restored timing; UDP socket delivery does not prove internal DUT state or clock accuracy.\n");
            if (recovered == Result::CouldNotRun)
                return recovered;
            return withdrawal_observed && recovered == Result::Pass ? Result::Pass : Result::Fail;
        }

        Result run_udp_test(UdpTest test, const UdpConfig &config, const std::atomic<bool> &stop,
                            char *output, size_t output_size)
        {
            if (output == nullptr || output_size == 0)
            {
                ESP_LOGE("ptp_udp_tester", "PTP L3 UDP output buffer is unavailable");
                return Result::CouldNotRun;
            }
            output[0] = '\0';
            Evidence evidence{output, output_size};
            if (test > UdpTest::RestartRecovery || config.netif == nullptr || config.link_up == nullptr ||
                config.link_generation == nullptr || (!config.ipv4 && !config.ipv6) ||
                (config.ipv4 && (config.ipv4_target == nullptr || config.ipv4_target[0] == '\0')) ||
                (config.ipv6 && (config.ipv6_target == nullptr || config.ipv6_target[0] == '\0')) ||
                config.timeout_ms < 1000 || config.timeout_ms > MaxTimeoutMs ||
                config.observation_ms < 1000 || config.observation_ms > MaxTimeoutMs ||
                config.samples < 2 || config.samples > 64 || config.response_window_ms < 1 ||
                config.response_window_ms > 5000 || config.request_interval_ms < 100 ||
                config.request_interval_ms > 5000 || config.withdrawal_ms < 1000 ||
                config.withdrawal_ms > 60000 || config.recovery_ms < 1000 || config.recovery_ms > 600000 ||
                static_cast<uint64_t>(config.samples) * config.request_interval_ms + config.response_window_ms >
                    config.observation_ms ||
                (test == UdpTest::Simultaneous && (!config.ipv4 || !config.ipv6)))
            {
                evidence.add("Invalid PTP L3 UDP configuration or missing transport/link prerequisites.\n");
                return Result::CouldNotRun;
            }
            if (test == UdpTest::CombinedLoad && !config.observation)
            {
                evidence.add("Combined-load observation callback is unavailable.\n");
                return Result::CouldNotRun;
            }
            auto session = std::unique_ptr<UdpSession>(new (std::nothrow) UdpSession{config, stop, evidence});
            if (!session)
            {
                evidence.add("Insufficient memory for the PTP L3 UDP session.\n");
                return Result::CouldNotRun;
            }
            evidence.add("PTP L3 UDP unsigned E2E; domain=%u; IPv4=%s; IPv6=%s; "
                         "multicast=224.0.1.129 / ff02::181; event/general ports=319/320; hop limit=1. "
                         "Sockets are bound to the multicast groups and the Ethernet interface. "
                         "Hardware timestamps, packet-drop counters and clock accuracy are not available through these sockets.\n",
                         config.domain, config.ipv4 ? config.ipv4_target : "disabled",
                         config.ipv6 ? config.ipv6_target : "disabled");
            if (!session->open())
            {
                evidence.add("PTP L3 UDP setup interrupted: %s; errno=%d.\n", session->reason, session->socket_error);
                return Result::CouldNotRun;
            }
            session->progress("PTP L3 UDP: observing selected multicast master");
            const bool quick = test == UdpTest::Announce || test == UdpTest::TimeProperties ||
                               test == UdpTest::SyncFollowUp || test == UdpTest::DelayExchange;
            Result result = collect_udp(*session, quick ? test : UdpTest::DelayExchange, config.timeout_ms, false);
            if (!quick && result == Result::Pass)
            {
                if (test == UdpTest::NegativeDelay)
                    result = udp_negative(*session);
                else if (udp_requires_operator(test))
                    result = udp_assisted(*session, test);
                else
                {
                    session->progress("PTP L3 UDP: sustained multicast and E2E observation");
                    result = collect_udp(*session, test, config.observation_ms, test == UdpTest::CombinedLoad);
                }
            }
            else if (!quick && result != Result::Pass && (test == UdpTest::NegativeDelay || udp_requires_operator(test)))
            {
                evidence.add("A healthy baseline is required before negative or assisted testing.\n");
                result = Result::CouldNotRun;
            }
            if (session->interrupted)
            {
                evidence.add("Observation interrupted: %s; errno=%d.\n", session->reason, session->socket_error);
                result = Result::CouldNotRun;
            }
            if (evidence.truncated)
            {
                constexpr char suffix[] = " [Evidence truncated; result inconclusive]";
                if (output_size > sizeof(suffix))
                    std::memcpy(output + output_size - sizeof(suffix), suffix, sizeof(suffix));
                result = Result::CouldNotRun;
            }
            return result;
        }
#endif
    }

#ifndef PTP_TESTER_HOST_TEST
    esp_err_t initialize(esp_eth_handle_t ethernet, esp_netif_t *netif)
    {
        if (ethernet == nullptr || netif == nullptr)
            return ESP_ERR_INVALID_ARG;
#if !CONFIG_ETH_TRANSMIT_MUTEX
        return ESP_ERR_NOT_SUPPORTED;
#endif
        if (s_run_mutex == nullptr)
            s_run_mutex = xSemaphoreCreateMutexStatic(&s_run_mutex_storage);
        if (s_run_mutex == nullptr || xSemaphoreTake(s_run_mutex, 0) != pdTRUE)
            return ESP_ERR_INVALID_STATE;
        RunGuard guard;
        if (s_ethernet != nullptr && (s_ethernet != ethernet || s_netif != netif))
            return ESP_ERR_INVALID_STATE;
        s_ready.store(false);
        s_ethernet = ethernet;
        s_netif = netif;
        if (s_queue == nullptr)
            s_queue = xQueueCreateStatic(QueueCapacity, sizeof(Packet), s_queue_storage, &s_queue_control);
        if (s_queue == nullptr)
            return ESP_ERR_NO_MEM;
        esp_err_t error = esp_eth_ioctl(ethernet, ETH_CMD_G_MAC_ADDR, s_local_mac);
        if (error != ESP_OK)
            return error;
        if (!valid_target(s_local_mac))
            return ESP_ERR_INVALID_STATE;
        std::memcpy(s_local_identity, s_local_mac, 3);
        s_local_identity[3] = 0xff;
        s_local_identity[4] = 0xfe;
        std::memcpy(s_local_identity + 5, s_local_mac + 3, 3);
        write16(s_local_identity + 8, 1);
        if (s_event_instance == nullptr)
        {
            error = esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, ethernet_event, nullptr, &s_event_instance);
            if (error != ESP_OK)
                return error;
        }
#if CONFIG_ESP_NETIF_L2_TAP
        return ESP_ERR_NOT_SUPPORTED;
#endif
#if SOC_EMAC_IEEE1588V2_SUPPORTED
        const esp_eth_clock_cfg_t clock_config = {CLOCK_PTP_SYSTEM};
        error = esp_eth_clock_init(ethernet, &clock_config);
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE && error != ESP_ERR_NOT_SUPPORTED)
            return error;
#endif
        error = esp_eth_ioctl(ethernet, ETH_CMD_ADD_MAC_FILTER, const_cast<uint8_t *>(MulticastMac));
        if (error != ESP_OK)
            return error;
        error = esp_eth_update_input_path_info(ethernet, ethernet_input, netif);
        if (error != ESP_OK)
            return error;
        s_generation.fetch_add(1);
        s_ready.store(true);
        return ESP_OK;
    }

    bool ready()
    {
        return s_ready.load();
    }

    Result run(Test test, const Config &config, const std::atomic<bool> &stop, char *output, size_t output_size)
    {
        if (output == nullptr || output_size == 0)
            return Result::CouldNotRun;
        output[0] = '\0';
        auto finish = [output, output_size](Result result, const char *text)
        {
            std::snprintf(output, output_size, "%s", text);
            return result;
        };
        if (test >= Test::Count || !valid_target(config.target_mac) || config.timeout_ms == 0 || config.timeout_ms > MaxTimeoutMs)
            return finish(Result::CouldNotRun, "Invalid test/config: selected unicast master MAC and timeout 1..120000 ms are required.");
        if (const char *pending = pending_reason(test))
            return finish(Result::CouldNotRun, pending);
        if (const char *issue = policy_prerequisite(test, config))
            return finish(Result::CouldNotRun, issue);
        if (test >= Test::Sustained)
            return run_extended(test, config, stop, output, output_size);
        Authenticator auth;
        if (signed_policy(config) && configured_key(config) &&
            !auth.initialize(config.key_id, config.key_value, config.policy.spp))
            return finish(Result::CouldNotRun, "PTP key import/crypto initialization failed; check the separate PTP key configuration.");
        if (!ready() || s_run_mutex == nullptr)
            return finish(Result::CouldNotRun, "PTP dispatcher is not initialized after netif attach.");
        if (xSemaphoreTake(s_run_mutex, 0) != pdTRUE)
            return finish(Result::CouldNotRun, "Another PTP test or initialization is active.");
        RunGuard guard;
        if (s_rotation_restore.pending)
            return finish(Result::CouldNotRun, "Previous staged key fixture is not restored; rerun the rotation test for restoration verification.");
        if (s_startup_restore.pending)
            return finish(Result::CouldNotRun, "Previous startup fixture is not restored; rerun startup listening for worker recovery verification.");
        if (s_timing_restore.pending)
            return finish(Result::CouldNotRun, "Previous timing fixture is not restored; rerun candidate expiry (test 58) for recovery verification.");
        if (s_foreign_cleanup.pending)
            return finish(Result::CouldNotRun, "Previous foreign-candidate cleanup incomplete; rerun the foreign-master policy and recovery test before dependent PTP tests.");
        if (stop.load())
            return finish(Result::CouldNotRun, "Stopped before PTP test started.");
        if (!s_link_up.load())
            return finish(Result::CouldNotRun, "Ethernet link is down; L2 tests do not require IP.");
        if (std::memcmp(config.target_mac, s_local_mac, 6) == 0)
            return finish(Result::CouldNotRun, "Selected master MAC is this tester's MAC.");
        Packet packet{};
        while (xQueueReceive(s_queue, &packet, 0) == pdTRUE)
        {
        }
        const int64_t start_us = esp_timer_get_time();
        const int64_t deadline_us = start_us + static_cast<int64_t>(config.timeout_ms) * 1000;
        const uint32_t generation = s_generation.load();
        const uint32_t initial_drops = s_drops.load();
        Message announce{};
        ServiceAuthentication service_authentication{};
        bool master_locked = false;
        std::array<Pair, 4> pairs{};
        Pair completed{};
        bool pair_complete = false;
        bool request_sent = false;
        bool transmit_hardware = false;
        Timestamp transmit_time{};
        uint16_t request_sequence = 0;
        int64_t request_us = 0;
        uint32_t selected_packets = 0;
        uint32_t malformed = 0;
        uint32_t unsupported = 0;
        uint32_t mismatches = 0;
        s_target_prefix.store(mac_prefix(config.target_mac));
        s_target_suffix.store(read16(config.target_mac + 4));
        s_active.store(true);
        while (esp_timer_get_time() < deadline_us)
        {
            if (stop.load())
                return finish(Result::CouldNotRun, "PTP test stopped; no clock was adjusted.");
            if (!s_link_up.load() || s_generation.load() != generation)
                return finish(Result::CouldNotRun, "Ethernet link/driver generation changed; stale pairing discarded. Retry after recovery.");
            if (s_drops.load() != initial_drops)
                return finish(Result::CouldNotRun, "Bounded PTP RX queue overflowed; result is inconclusive. Retry without competing PTP traffic.");
            const int64_t remaining_us = deadline_us - esp_timer_get_time();
            if (remaining_us <= 0)
                break;
            const uint32_t wait_ms = static_cast<uint32_t>(std::min<int64_t>(20, (remaining_us + 999) / 1000));
            if (xQueueReceive(s_queue, &packet, std::max<TickType_t>(1, pdMS_TO_TICKS(wait_ms))) != pdTRUE)
                continue;
            if (packet.generation != generation || packet.received_us < start_us ||
                std::memcmp(packet.frame + 6, config.target_mac, 6) != 0)
                continue;
            if (std::memcmp(packet.frame, MulticastMac, 6) != 0 && std::memcmp(packet.frame, s_local_mac, 6) != 0)
                continue;
            ++selected_packets;
            if (packet.unsupported_vlan || packet.oversized)
            {
                ++unsupported;
                continue;
            }
            Message message{};
            const ParseResult parsed = parse_config_message(packet, config, auth, message);
            if (parsed == ParseResult::AuthenticationUnsupported || parsed == ParseResult::AuthenticationUnavailable)
                return finish(Result::CouldNotRun, "Selected master authentication verification unavailable; check the PTP key configuration and declared DUT policy.");
            if (parsed == ParseResult::AuthenticationInvalid)
                return finish(Result::CouldNotRun, "Selected master authentication missing/invalid for the configured PTP key/SPP; no verified baseline, so no firmware failure is inferred.");
            if (parsed != ParseResult::Valid)
            {
                malformed += parsed == ParseResult::Malformed;
                unsupported += parsed == ParseResult::Unsupported;
                continue;
            }
            if (const char *issue = wire_policy_error(config, message))
                return finish(Result::CouldNotRun, issue);
            if (const char *issue = service_authentication.observe(message))
                return finish(Result::Fail, issue);
            if (message.type == 11)
            {
                const bool previously_locked = master_locked;
                if (!master_locked)
                {
                    announce = message;
                    master_locked = true;
                    if (config.target_identity != nullptr)
                    {
                        char text[768];
                        announce_text(announce, text, sizeof(text));
                        config.target_identity(text, config.context);
                    }
                }
                else if (std::memcmp(message.source, announce.source, 10) != 0 || message.minor_version != announce.minor_version ||
                         std::memcmp(message.grandmaster, announce.grandmaster, 8) != 0 || message.flags != announce.flags ||
                         ((message.flags & UtcValidFlag) != 0 && message.utc_offset != announce.utc_offset))
                {
                    return finish(Result::CouldNotRun, "Selected master's identity/version/time properties changed during test; retry with a fresh Announce.");
                }
                if (previously_locked &&
                    (message.priority1 != announce.priority1 || message.priority2 != announce.priority2 ||
                     message.clock_class != announce.clock_class || message.accuracy != announce.accuracy ||
                     message.variance != announce.variance || message.time_source != announce.time_source ||
                     message.steps != announce.steps))
                    return finish(Result::CouldNotRun, "Clock quality changed between Announce messages; retry after master stabilizes.");
                if (test == Test::Announce ||
                    (test == Test::TimeProperties && previously_locked && message.sequence != announce.sequence))
                {
                    announce_text(announce, output, output_size);
                    if (test == Test::TimeProperties)
                    {
                        const size_t used = std::strlen(output);
                        std::snprintf(output + used, output_size - used, " Two distinct Announce sequences were consistent.");
                    }
                    return Result::Pass;
                }
                continue;
            }
            if (!master_locked || test == Test::Announce || test == Test::TimeProperties)
                continue;
            if (std::memcmp(message.source, announce.source, 10) != 0 || message.minor_version != announce.minor_version)
            {
                ++mismatches;
                continue;
            }
            if (!pair_complete && (message.type == 0 || message.type == 8))
            {
                if (message.type == 0 && (message.flags & TwoStepFlag) == 0)
                    return finish(Result::Fail, "Selected master sent one-step Sync; this functional test requires two-step E2E.");
                pair_complete = match_pair(pairs, message, packet, completed);
                if (!pair_complete)
                    continue;
                if (test == Test::SyncFollowUp)
                {
                    std::snprintf(output, output_size,
                                  "Matched %s L2 two-step Sync/Follow_Up seq=%u, PTP v2.%u; "
                                  "origin=%llu.%09u; correction=%.6f ns; RX=%s. "
                                  "Functional pairing only; no precision or synchronization claim.",
                                  announce.authenticated ? "HMAC-SHA256-128 verified" : "unauthenticated",
                                  completed.sequence, announce.minor_version,
                                  static_cast<unsigned long long>(completed.follow.timestamp.seconds), static_cast<unsigned>(completed.follow.timestamp.nanoseconds),
                                  static_cast<double>(static_cast<long double>(completed.sync.correction) / 65536.0L +
                                                      static_cast<long double>(completed.follow.correction) / 65536.0L),
                                  completed.reception.hardware_timestamp ? "hardware" : "software callback arrival (not wire time)");
                    return Result::Pass;
                }
                if (test == Test::Measurement && !completed.reception.hardware_timestamp)
                    return finish(Result::CouldNotRun, "Measurement unavailable: Sync has no valid hardware RX timestamp. Software arrival is functional-only.");
                if (stop.load() || esp_timer_get_time() >= deadline_us || !s_link_up.load() || s_generation.load() != generation)
                    return finish(Result::CouldNotRun, "Stopped, timed out, or link changed before Delay_Req; no request sent.");
                uint8_t frame[MaxFrameSize] = {};
                request_sequence = ++s_sequence;
                make_delay_request(frame, s_local_mac, s_local_identity, config.domain, announce.minor_version, request_sequence);
                size_t message_length = 44;
                if (announce.authenticated && !auth.append(frame + 14, sizeof(frame) - 14, message_length))
                    return finish(Result::CouldNotRun, "Delay_Req signing failed; no authenticated exchange inferred.");
                const uint32_t frame_length = static_cast<uint32_t>(std::max<size_t>(60, 14 + message_length));
                request_us = esp_timer_get_time();
                esp_err_t error;
#if SOC_EMAC_IEEE1588V2_SUPPORTED
                eth_mac_time_t hardware_time{};
                error = esp_eth_transmit_ctrl_vargs(s_ethernet, &hardware_time, 2, frame, frame_length);
                if (error == ESP_OK && (hardware_time.seconds != 0 || hardware_time.nanoseconds != 0) && hardware_time.nanoseconds < 1000000000U)
                {
                    transmit_hardware = true;
                    transmit_time.seconds = hardware_time.seconds;
                    transmit_time.nanoseconds = hardware_time.nanoseconds;
                }
#else
                error = esp_eth_transmit(s_ethernet, frame, frame_length);
#endif
                if (error != ESP_OK)
                {
                    std::snprintf(output, output_size, "Delay_Req transmission failed: %s; no response/measurement inferred.", esp_err_to_name(error));
                    return Result::CouldNotRun;
                }
                request_sent = true;
                if (test == Test::Measurement && !transmit_hardware)
                    return finish(Result::CouldNotRun, "Measurement unavailable: Delay_Req hardware TX timestamp missing/late; software pre-send time is not substituted.");
                continue;
            }
            if (request_sent && message.type == 9)
            {
                if (packet.received_us < request_us ||
                    !match_delay_response(message, announce.source, announce.minor_version, request_sequence, s_local_identity))
                {
                    ++mismatches;
                    continue;
                }
                if (test == Test::DelayExchange)
                {
                    std::snprintf(output, output_size,
                                  "Matched %s L2 Delay_Req/Delay_Resp seq=%u with exact source/requester identity; "
                                  "master ingress=%llu.%09u; correction=%.6f ns; Sync RX=%s; Delay_Req TX=%s. "
                                  "Functional E2E exchange only; no clock servo or accuracy claim.",
                                  announce.authenticated ? "HMAC-SHA256-128 verified" : "unauthenticated",
                                  request_sequence, static_cast<unsigned long long>(message.timestamp.seconds), static_cast<unsigned>(message.timestamp.nanoseconds),
                                  static_cast<double>(static_cast<long double>(message.correction) / 65536.0L),
                                  completed.reception.hardware_timestamp ? "hardware" : "software arrival",
                                  transmit_hardware ? "hardware" : "software submission only (egress unknown)");
                    return Result::Pass;
                }
                Measurement measurement{};
                if (!calculate_measurement(completed, transmit_time, message, measurement))
                    return finish(Result::CouldNotRun, "Measurement unusable: clock discontinuity, stale exchange, or implausible/negative path delay. No accuracy claim.");
                std::snprintf(output, output_size,
                              "Single hardware-timestamp E2E sample, symmetric-path assumption: mean path delay=%.3f us; "
                              "local PHC minus master's %s timescale=%.3f us; corrections included at 1/65536 ns scale. "
                              "PHC is not disciplined by this tester; offset is not UTC-converted. "
                              "Not a nanosecond precision/accuracy claim; authentication=%s.",
                              static_cast<double>(measurement.mean_path_delay_ns / 1000.0L),
                              (announce.flags & PtpTimescaleFlag) != 0 ? "PTP/TAI" : "arbitrary",
                              static_cast<double>(measurement.local_minus_master_ns / 1000.0L),
                              announce.authenticated ? "HMAC-SHA256-128 verified" : "unsigned");
                return Result::Pass;
            }
        }
        if (stop.load())
            return finish(Result::CouldNotRun, "PTP test stopped.");
        if (!s_link_up.load() || s_generation.load() != generation || s_drops.load() != initial_drops)
            return finish(Result::CouldNotRun, "Link changed or bounded RX queue overflowed; test is inconclusive.");
        std::snprintf(output, output_size,
                      "Timeout after %u ms: selected L2 packets=%u, malformed=%u, unsupported=%u, mismatches=%u; "
                      "Announce=%s, two-step pair=%s, Delay_Req=%s. Domain=%u; VLAN/other profiles unsupported; IP is irrelevant.",
                      static_cast<unsigned>(config.timeout_ms), static_cast<unsigned>(selected_packets),
                      static_cast<unsigned>(malformed), static_cast<unsigned>(unsupported), static_cast<unsigned>(mismatches),
                      master_locked ? "locked" : "absent", pair_complete ? "matched" : "absent", request_sent ? "sent" : "not sent", config.domain);
        return Result::Fail;
    }

    Result run_udp(UdpTest test, const UdpConfig &config, const std::atomic<bool> &stop,
                   char *output, size_t output_size)
    {
        return run_udp_test(test, config, stop, output, output_size);
    }
#endif
}
