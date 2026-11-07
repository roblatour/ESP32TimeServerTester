// ESP32 Time Server Tester
// Copyright Rob Latour, 2026
// License: MIT

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace
{
    constexpr size_t NtpPacketSize = 48;
    constexpr size_t NtpAuthTrailerSize = 24;
    constexpr int PSA_SUCCESS = 0;
    constexpr int PSA_ALG_SHA_256 = 1;

    struct psa_hash_operation_t
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
    };

#define PSA_HASH_OPERATION_INIT {}

    int failure_step = 0;
    unsigned update_count = 0;

    int psa_hash_setup(psa_hash_operation_t *operation, int algorithm)
    {
        if (algorithm != PSA_ALG_SHA_256 || failure_step == 1)
            return -1;
        if (BCryptOpenAlgorithmProvider(&operation->algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptCreateHash(operation->algorithm, &operation->hash, nullptr, 0, nullptr, 0, 0) < 0)
            return -1;
        return PSA_SUCCESS;
    }

    int psa_hash_update(psa_hash_operation_t *operation, const uint8_t *input, size_t length)
    {
        ++update_count;
        if ((failure_step == 2 && update_count == 1) || (failure_step == 3 && update_count == 2))
            return -1;
        return BCryptHashData(operation->hash, const_cast<PUCHAR>(input), static_cast<ULONG>(length), 0) < 0 ? -1 : PSA_SUCCESS;
    }

    int psa_hash_finish(psa_hash_operation_t *operation, uint8_t *output, size_t capacity, size_t *length)
    {
        if (failure_step == 4 || capacity < 32 ||
            BCryptFinishHash(operation->hash, output, 32, 0) < 0)
            return -1;
        *length = 32;
        return PSA_SUCCESS;
    }

    int psa_hash_abort(psa_hash_operation_t *operation)
    {
        if (operation->hash != nullptr)
            BCryptDestroyHash(operation->hash);
        if (operation->algorithm != nullptr)
            BCryptCloseAlgorithmProvider(operation->algorithm, 0);
        return PSA_SUCCESS;
    }

#include "ntp_auth_production.inc"

    unsigned assertions = 0;
    unsigned failures = 0;

    void check(bool condition, const char *message)
    {
        ++assertions;
        if (!condition)
        {
            ++failures;
            std::printf("FAIL: %s\n", message);
        }
    }
}

int main()
{
    const symmetric_key_t key{0x1234, "0123456789abcdef0123456789abcdef"};
    std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> packet{};
    for (size_t index = 0; index < NtpPacketSize; ++index)
        packet[index] = static_cast<uint8_t>(index);
    const std::array<uint8_t, 20> expected{
        0x1f, 0x28, 0xf4, 0x27, 0x05, 0xd8, 0xe1, 0x21, 0x8f, 0xb2,
        0xfb, 0x91, 0x46, 0xcb, 0x8e, 0xfa, 0x79, 0x41, 0xb6, 0x97};
    size_t length = NtpPacketSize;
    check(append_ntp_authentication(packet.data(), packet.size(), &length, key),
          "production signing accepts a valid key and packet");
    check(length == packet.size() &&
          packet[48] == 0 && packet[49] == 0 && packet[50] == 0x12 && packet[51] == 0x34,
          "authentication appends the expected network-order key ID and trailer length");
    check(std::memcmp(packet.data() + 52, expected.data(), expected.size()) == 0,
          "production signing matches the independent SHA256(key || packet) golden vector");
    check(verify_ntp_authentication(packet.data(), length, key), "golden signed packet verifies");

    auto modified = packet;
    modified[12] ^= 1;
    check(!verify_ntp_authentication(modified.data(), length, key), "modified packet is rejected");
    modified = packet;
    modified[52] ^= 1;
    check(!verify_ntp_authentication(modified.data(), length, key), "modified digest is rejected");
    modified = packet;
    modified[51] ^= 1;
    check(!verify_ntp_authentication(modified.data(), length, key), "wrong key ID is rejected");
    const symmetric_key_t wrong_key{0x1234, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"};
    check(!verify_ntp_authentication(packet.data(), length, wrong_key), "wrong key material is rejected");
    check(!verify_ntp_authentication(packet.data(), length - 1, key), "short authentication trailer is rejected");
    check(!verify_ntp_authentication(packet.data(), length + 1, key), "unexpected extra bytes are rejected");

    const symmetric_key_t no_id{0, "0123456789abcdef0123456789abcdef"};
    const symmetric_key_t no_value{0x1234, ""};
    size_t request_length = NtpPacketSize;
    check(!append_ntp_authentication(modified.data(), modified.size(), &request_length, no_id) &&
          !append_ntp_authentication(modified.data(), modified.size(), &request_length, no_value),
          "signing requires both key ID and key material");
    check(!append_ntp_authentication(modified.data(), modified.size() - 1, &request_length, key),
          "signing rejects insufficient trailer capacity");
    request_length = NtpPacketSize - 1;
    check(!append_ntp_authentication(modified.data(), modified.size(), &request_length, key),
          "signing rejects an incorrectly sized NTP payload");
    for (failure_step = 1; failure_step <= 4; ++failure_step)
    {
        update_count = 0;
        request_length = NtpPacketSize;
        check(!append_ntp_authentication(modified.data(), modified.size(), &request_length, key) &&
              request_length == NtpPacketSize,
              "each PSA signing failure is surfaced without publishing a signed length");
        update_count = 0;
        check(!verify_ntp_authentication(packet.data(), length, key),
              "each PSA verification failure rejects the response");
    }
    failure_step = 0;
    std::printf("NTP authentication: %u assertions, %u failures\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
