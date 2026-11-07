// ESP32TimeServerTester v2
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#pragma once

#include <cstddef>
#include <cstdint>

#ifndef PTP_TESTER_HOST_TEST
#include "psa/crypto.h"
#endif

namespace ptp_tester
{
    constexpr size_t AuthTlvSize = 26;
    constexpr size_t AuthIcvSize = 16;

    enum class AuthResult : uint8_t
    {
        Unsigned,
        Valid,
        Malformed,
        UnknownKey,
        InvalidMac,
        Unavailable
    };

    class Authenticator
    {
    public:
        Authenticator() = default;
        ~Authenticator();
        Authenticator(const Authenticator &) = delete;
        Authenticator &operator=(const Authenticator &) = delete;
        bool initialize(uint32_t id, const char *value, uint8_t spp);
        bool available() const { return ready_; }
        bool append(uint8_t *message, size_t capacity, size_t &length) const;
        AuthResult verify(const uint8_t *message, size_t length) const;

    private:
        bool mac(const uint8_t *message, size_t length, uint8_t *output) const;
        uint32_t id_ = 0;
        uint8_t spp_ = 0;
        bool ready_ = false;
#ifdef PTP_TESTER_HOST_TEST
        uint8_t key_[32] = {};
#else
        psa_key_id_t key_ = 0;
#endif
    };
}
