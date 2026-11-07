// ESP32 Time Server Tester
// Copyright Rob Latour, 2026
// License: MIT
// Wire format matches ESP32 Time Server's immediate HMAC-SHA256-128 TLV.

#include "ptp_auth.h"

#include <cstdio>
#include <cstring>

#ifdef PTP_TESTER_HOST_TEST
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#undef near
#undef far
#pragma comment(lib, "bcrypt.lib")
#else
#include "esp_log.h"
#endif

namespace ptp_tester
{
    namespace
    {
        size_t auth_body_size(uint8_t type)
        {
            switch (type & 0x0f)
            {
            case 0:
            case 1:
            case 8: return 44;
            case 9: return 54;
            case 11: return 64;
            default: return 0;
            }
        }

        uint16_t auth_read16(const uint8_t *bytes)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
        }

        void auth_error(const char *operation, long status)
        {
#ifdef PTP_TESTER_HOST_TEST
            std::fprintf(stderr, "PTP authentication %s failed: %ld\n", operation, status);
#else
            ESP_LOGE("ptp_auth", "%s failed: %ld", operation, status);
#endif
        }
    }

    Authenticator::~Authenticator()
    {
#ifdef PTP_TESTER_HOST_TEST
        SecureZeroMemory(key_, sizeof(key_));
#else
        if (ready_)
        {
            const psa_status_t status = psa_destroy_key(key_);
            if (status != PSA_SUCCESS)
                auth_error("key destruction", status);
        }
#endif
    }

    bool Authenticator::initialize(uint32_t id, const char *value, uint8_t spp)
    {
        if (ready_ || id == 0 || value == nullptr || std::strlen(value) != 32)
        {
            auth_error("key configuration", -1);
            return false;
        }
        for (size_t index = 0; index < 32; ++index)
        {
            const char c = value[index];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            {
                auth_error("key configuration", -1);
                return false;
            }
        }
#ifdef PTP_TESTER_HOST_TEST
        std::memcpy(key_, value, sizeof(key_));
#else
        psa_status_t status = psa_crypto_init();
        if (status != PSA_SUCCESS)
        {
            auth_error("crypto initialization", status);
            return false;
        }
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attributes, 256);
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&attributes, PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), AuthIcvSize));
        status = psa_import_key(&attributes, reinterpret_cast<const uint8_t *>(value), 32, &key_);
        psa_reset_key_attributes(&attributes);
        if (status != PSA_SUCCESS)
        {
            auth_error("key import", status);
            return false;
        }
#endif
        id_ = id;
        spp_ = spp;
        ready_ = true;
        return true;
    }

    bool Authenticator::mac(const uint8_t *message, size_t length, uint8_t *output) const
    {
#ifdef PTP_TESTER_HOST_TEST
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (status >= 0)
            status = BCryptCreateHash(algorithm, &hash, nullptr, 0, const_cast<PUCHAR>(key_), sizeof(key_), 0);
        if (status >= 0)
            status = BCryptHashData(hash, const_cast<PUCHAR>(message), static_cast<ULONG>(length), 0);
        uint8_t digest[32] = {};
        if (status >= 0)
            status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
        if (hash != nullptr)
            BCryptDestroyHash(hash);
        if (algorithm != nullptr)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0)
        {
            auth_error("HMAC", status);
            return false;
        }
        std::memcpy(output, digest, AuthIcvSize);
        SecureZeroMemory(digest, sizeof(digest));
#else
        size_t produced = 0;
        const psa_status_t status = psa_mac_compute(key_, PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), AuthIcvSize),
                                                    message, length, output, AuthIcvSize, &produced);
        if (status != PSA_SUCCESS || produced != AuthIcvSize)
        {
            auth_error("HMAC", status);
            return false;
        }
#endif
        return true;
    }

    bool Authenticator::append(uint8_t *message, size_t capacity, size_t &length) const
    {
        if (!ready_ || message == nullptr || length < 34 || length > capacity || capacity - length < AuthTlvSize ||
            auth_body_size(message[0]) != length || auth_read16(message + 2) != length || message[1] != 0x12)
        {
            auth_error("signing buffer/header", -1);
            return false;
        }
        const size_t total = length + AuthTlvSize;
        uint8_t *tlv = message + length;
        tlv[0] = 0x80;
        tlv[1] = 0x09;
        tlv[2] = 0;
        tlv[3] = AuthTlvSize - 4;
        tlv[4] = spp_;
        tlv[5] = 0;
        for (size_t index = 0; index < 4; ++index)
            tlv[6 + index] = static_cast<uint8_t>(id_ >> (24 - 8 * index));
        message[2] = static_cast<uint8_t>(total >> 8);
        message[3] = static_cast<uint8_t>(total);
        if (!mac(message, total - AuthIcvSize, tlv + 10))
            return false;
        length = total;
        return true;
    }

    AuthResult Authenticator::verify(const uint8_t *message, size_t length) const
    {
        if (message == nullptr || length < 34 || auth_read16(message + 2) != length)
            return AuthResult::Malformed;
        const size_t body = auth_body_size(message[0]);
        if (body == 0 || length < body)
            return AuthResult::Malformed;
        if (length == body)
            return AuthResult::Unsigned;
        if (length != body + AuthTlvSize || message[1] != 0x12)
            return AuthResult::Malformed;
        const uint8_t *tlv = message + body;
        if (auth_read16(tlv) != 0x8009 || auth_read16(tlv + 2) != AuthTlvSize - 4 || tlv[4] != spp_ || tlv[5] != 0)
            return AuthResult::Malformed;
        if (!ready_)
            return AuthResult::Unavailable;
        uint32_t id = 0;
        for (size_t index = 0; index < 4; ++index)
            id = (id << 8) | tlv[6 + index];
        if (id != id_)
            return AuthResult::UnknownKey;
        uint8_t expected[AuthIcvSize] = {};
        if (!mac(message, length - AuthIcvSize, expected))
            return AuthResult::Unavailable;
        volatile uint8_t difference = 0;
        for (size_t index = 0; index < AuthIcvSize; ++index)
            difference |= expected[index] ^ tlv[10 + index];
        return difference == 0 ? AuthResult::Valid : AuthResult::InvalidMac;
    }
}
