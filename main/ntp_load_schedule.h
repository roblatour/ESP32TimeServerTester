// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ntp_load
{
    inline uint64_t missed_slots(int64_t next_us, int64_t until_us, int64_t window_end_us, int64_t interval_us)
    {
        // The collection callback may finish late, but it must not extend the request schedule.
        const int64_t end_us = std::min(until_us, window_end_us);
        return next_us < end_us ? static_cast<uint64_t>((end_us - next_us + interval_us - 1) / interval_us) : 0;
    }

    inline uint64_t timestamp(const uint8_t *bytes)
    {
        uint64_t value = 0;
        for (size_t index = 0; index < 8; ++index)
            value = (value << 8) | bytes[index];
        return value;
    }

    inline bool valid_response(const uint8_t *packet, size_t length, uint8_t version,
                               uint64_t request_timestamp, bool authenticated)
    {
        constexpr size_t BaseLength = 48;
        constexpr size_t AuthenticatedLength = 72;
        return packet != nullptr && length >= BaseLength &&
               (!authenticated || length == AuthenticatedLength) &&
               ((packet[0] >> 3) & 7) == version && (packet[0] & 7) == 4 &&
               (packet[0] >> 6) != 3 && packet[1] > 0 && packet[1] <= 15 &&
               timestamp(packet + 24) == request_timestamp &&
               timestamp(packet + 32) != 0 && timestamp(packet + 40) != 0;
    }

    enum class AuthRejectionResponse
    {
        Invalid,
        Rejected,
        Accepted
    };

    enum class ProbeResult
    {
        Unavailable,
        Pass,
        Fail
    };

    struct PolicyProbeEvidence
    {
        int64_t start_us = 0;
        int64_t end_us = 0;
        bool before[2] = {};
        bool after[2] = {};
        ProbeResult invalid_mac[2] = {};
        ProbeResult unsigned_required[2] = {};
    };

    inline ProbeResult classify_policy_probes(const PolicyProbeEvidence &evidence, bool required,
                                              int64_t observation_start_us, int64_t observation_end_us,
                                              bool link_intact)
    {
        if (!link_intact || observation_start_us <= 0 || observation_end_us <= observation_start_us ||
            evidence.start_us < observation_start_us || evidence.end_us <= evidence.start_us ||
            evidence.end_us > observation_end_us)
            return ProbeResult::Unavailable;
        bool failed = false;
        for (size_t slot = 0; slot < 2; ++slot)
        {
            if (!evidence.before[slot] || !evidence.after[slot] ||
                evidence.invalid_mac[slot] == ProbeResult::Unavailable ||
                (required && evidence.unsigned_required[slot] == ProbeResult::Unavailable))
                return ProbeResult::Unavailable;
            failed |= evidence.invalid_mac[slot] == ProbeResult::Fail ||
                      (required && evidence.unsigned_required[slot] == ProbeResult::Fail);
        }
        return failed ? ProbeResult::Fail : ProbeResult::Pass;
    }

    inline AuthRejectionResponse classify_auth_rejection(const uint8_t *packet, size_t length, uint8_t version,
                                                          uint64_t request_timestamp)
    {
        if (packet == nullptr || length < 48 || ((packet[0] >> 3) & 7) != version ||
            (packet[0] & 7) != 4 || timestamp(packet + 24) != request_timestamp)
            return AuthRejectionResponse::Invalid;
        if (length == 52 && packet[48] == 0 && packet[49] == 0 && packet[50] == 0 && packet[51] == 0)
            return AuthRejectionResponse::Rejected;
        if (packet[1] == 0)
        {
            const uint8_t *code = packet + 12;
            if (std::memcmp(code, "AUTH", 4) == 0 || std::memcmp(code, "CRYP", 4) == 0 ||
                std::memcmp(code, "NKEY", 4) == 0 || std::memcmp(code, "DENY", 4) == 0 ||
                std::memcmp(code, "RSTR", 4) == 0)
                return AuthRejectionResponse::Rejected;
            return AuthRejectionResponse::Invalid;
        }
        return valid_response(packet, length, version, request_timestamp, false)
                   ? AuthRejectionResponse::Accepted
                   : AuthRejectionResponse::Invalid;
    }
}
