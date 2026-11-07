// ESP32 Time Server Tester
// Copyright Rob Latour, 2026
// License: MIT

#include "../main/ntp_load_schedule.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
    using SemaphoreHandle_t = void *;
    using TickType_t = uint32_t;
    constexpr TickType_t pdMS_TO_TICKS(uint32_t value) { return value; }
    constexpr int AF_INET = 4;
    constexpr int AF_INET6 = 6;
    constexpr uint32_t PtpResponseWindowMs = 500;
    constexpr uint32_t PtpObservationMs = 1000;
    constexpr uint32_t PtpCombinedRequestsPerSecond = 10;
    struct symmetric_key_t { bool configured = true; };
    enum class TestStatus { NotRun, Pass, Fail, CouldNotRun };
    struct TestDefinition
    {
        enum class Kind { InvalidAuth, MissingAuth, TruncatedMac };
    };
    bool has_test_key(const symmetric_key_t &key) { return key.configured; }

    std::atomic<bool> s_stop_tests_requested{false};
    std::atomic<bool> s_ethernet_connected{true};
    std::atomic<uint32_t> s_tester_link_generation{7};
    int64_t s_now = 1000000;
    unsigned s_controls = 0;
    unsigned s_invalid = 0;
    unsigned s_unsigned = 0;
    unsigned s_delays = 0;
    unsigned s_signals = 0;
    unsigned s_signed_controls = 0;
    unsigned s_unsigned_controls = 0;
    unsigned s_family_controls[2] = {};
    int s_exchange_failure = 0;
    int64_t s_exchange_duration_us = 1000;
    bool s_cut_observation = false;
    unsigned s_failed_control = 0;
    TestStatus s_invalid_status[2] = {TestStatus::Pass, TestStatus::Pass};
    TestStatus s_unsigned_status[2] = {TestStatus::Pass, TestStatus::Pass};
    void (*s_on_delay)() = nullptr;
    void (*s_on_invalid)() = nullptr;

    int64_t esp_timer_get_time() { return s_now; }
    void vTaskDelay(TickType_t)
    {
        ++s_delays;
        s_now += 1000;
        if (s_on_delay)
            s_on_delay();
    }
    void xSemaphoreGive(SemaphoreHandle_t) { ++s_signals; }
    void vTaskDelete(void *) {}

    bool ntp_exchange(int family, uint8_t, char *, size_t, uint32_t, int *failure,
                      const std::atomic<bool> *, const symmetric_key_t *key)
    {
        ++s_controls;
        ++s_family_controls[family == AF_INET ? 0 : 1];
        if (key)
            ++s_signed_controls;
        else
            ++s_unsigned_controls;
        s_now += s_exchange_duration_us;
        if (failure)
            *failure = s_exchange_failure;
        return s_controls != s_failed_control && s_exchange_failure == 0;
    }

    TestStatus ntp_authentication_rejection(int family, TestDefinition::Kind, const symmetric_key_t &, char *, size_t)
    {
        ++s_invalid;
        s_now += 100000;
        if (s_cut_observation && s_on_invalid)
            s_on_invalid();
        return s_invalid_status[family == AF_INET ? 0 : 1];
    }

    TestStatus ntp_required_unsigned_rejection(int family, uint8_t, const symmetric_key_t &, char *, size_t)
    {
        ++s_unsigned;
        s_now += 100000;
        return s_unsigned_status[family == AF_INET ? 0 : 1];
    }

#include "ntp_policy_worker_production.inc"

    CombinedWorker *s_load = nullptr;
    void establish_traffic() { s_load->traffic_established.store(true); }
    void finish_observation() { s_load->collection_finished.store(true); }
    void finish_load_after_window()
    {
        if (s_now >= s_load->observation_start_us +
                         static_cast<int64_t>(PtpObservationMs + PtpResponseWindowMs) * 1000)
        {
            s_load->observation_end_us = s_now;
            s_load->collection_finished.store(true, std::memory_order_release);
        }
    }
    void change_link_generation() { s_tester_link_generation.fetch_add(1); }

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

    void reset()
    {
        s_stop_tests_requested.store(false);
        s_ethernet_connected.store(true);
        s_tester_link_generation.store(7);
        s_now = 1000000;
        s_controls = s_invalid = s_unsigned = s_delays = s_signals = 0;
        s_signed_controls = s_unsigned_controls = s_failed_control = 0;
        s_family_controls[0] = s_family_controls[1] = 0;
        s_exchange_failure = 0;
        s_exchange_duration_us = 1000;
        s_cut_observation = false;
        s_invalid_status[0] = s_invalid_status[1] = TestStatus::Pass;
        s_unsigned_status[0] = s_unsigned_status[1] = TestStatus::Pass;
        s_on_delay = s_on_invalid = nullptr;
    }

    ntp_load::ProbeResult classify(const NtpPolicyWorker &policy)
    {
        return ntp_load::classify_policy_probes(policy.evidence, policy.required, 999000, 5000000, true);
    }
}

int main()
{
    symmetric_key_t key;
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        combined_ntp_policy_worker(&policy);
        check(s_controls == 4 && s_invalid == 2 && s_unsigned == 0 && s_signals == 1,
              "mixed policy probes both families with signed before/after controls");
        check(classify(policy) == ntp_load::ProbeResult::Pass, "completed mixed-policy worker passes");
    }
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        policy.required = true;
        combined_ntp_policy_worker(&policy);
        check(s_controls == 4 && s_invalid == 2 && s_unsigned == 2 &&
              classify(policy) == ntp_load::ProbeResult::Pass,
              "required policy adds unsigned rejection on both families");
        check(ntp_load::classify_policy_probes(policy.evidence, true, 999000, policy.evidence.end_us - 1, true) ==
                  ntp_load::ProbeResult::Unavailable,
              "a completed probe after the load window is inconclusive");
    }
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        s_invalid_status[1] = TestStatus::Fail;
        combined_ntp_policy_worker(&policy);
        check(classify(policy) == ntp_load::ProbeResult::Fail && s_controls == 4,
              "an accepted invalid MAC fails only with both families' healthy controls");
    }
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        policy.required = true;
        s_unsigned_status[1] = TestStatus::Fail;
        combined_ntp_policy_worker(&policy);
        check(classify(policy) == ntp_load::ProbeResult::Fail && s_unsigned == 2,
              "an accepted unsigned request fails required policy under load");
    }
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        s_failed_control = 1;
        combined_ntp_policy_worker(&policy);
        check(s_invalid == 1 && classify(policy) == ntp_load::ProbeResult::Unavailable,
              "a failed signed baseline skips that family's negative probes and is inconclusive");
    }
    {
        reset();
        CombinedWorker load;
        s_load = &load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        policy.required = true;
        s_cut_observation = true;
        s_on_invalid = finish_observation;
        combined_ntp_policy_worker(&policy);
        check(s_invalid == 1 && s_unsigned == 0 && s_controls == 1 &&
              classify(policy) == ntp_load::ProbeResult::Unavailable,
              "window closure during a negative probe prevents further work and cannot pass");
    }
    {
        reset();
        CombinedWorker load;
        s_load = &load;
        load.armed.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        s_on_delay = establish_traffic;
        combined_ntp_policy_worker(&policy);
        check(s_delays == 1 && s_invalid == 2 && classify(policy) == ntp_load::ProbeResult::Pass,
              "policy probes wait for successful load traffic on both families");
    }
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.collection_finished.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        combined_ntp_policy_worker(&policy);
        check(s_controls == 0 && s_invalid == 0 && s_signals == 1 &&
              classify(policy) == ntp_load::ProbeResult::Unavailable,
              "missing established load cannot produce policy evidence");
    }
    {
        reset();
        CombinedWorker load;
        load.armed.store(true);
        load.traffic_established.store(true);
        NtpPolicyWorker policy{&load, &key, &key};
        s_stop_tests_requested.store(true);
        combined_ntp_policy_worker(&policy);
        check(s_controls == 0 && s_invalid == 0 && s_signals == 1 &&
              classify(policy) == ntp_load::ProbeResult::Unavailable,
              "cancellation cannot publish successful policy evidence");
    }
    {
        reset();
        char output[512] = {};
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::InvalidAuth,
                                                      key, true, output, sizeof(output)) == TestStatus::Pass &&
              s_signed_controls == 2 && s_invalid == 1,
              "standalone invalid-authentication rejection needs signed controls on both sides");
        reset();
        s_invalid_status[0] = TestStatus::Fail;
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::InvalidAuth,
                                                      key, true, output, sizeof(output)) == TestStatus::Fail &&
              s_signed_controls == 2,
              "an accepted invalid probe fails when both signed controls are healthy");
        reset();
        s_failed_control = 1;
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::InvalidAuth,
                                                      key, true, output, sizeof(output)) == TestStatus::CouldNotRun &&
              s_invalid == 0,
              "a failed before-control prevents an invalid probe from being sent");
    }
    {
        reset();
        symmetric_key_t no_key{false};
        char output[512] = {};
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::MissingAuth,
                                                      no_key, false, output, sizeof(output)) == TestStatus::Pass &&
              s_unsigned_controls == 2 && s_invalid == 1,
              "mixed policy can use healthy unsigned controls when no NTP key is configured");
        reset();
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::MissingAuth,
                                                      no_key, true, output, sizeof(output)) == TestStatus::CouldNotRun &&
              s_controls == 0 && s_invalid == 0,
              "required policy without a key cannot report a rejection pass");
    }
    {
        reset();
        char output[512] = {};
        s_invalid_status[0] = TestStatus::Fail;
        s_failed_control = 2;
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::InvalidAuth,
                                                      key, true, output, sizeof(output)) == TestStatus::CouldNotRun,
              "a failed post-control overrides an apparent negative-probe failure");
        reset();
        s_cut_observation = true;
        s_on_invalid = change_link_generation;
        check(ntp_controlled_authentication_rejection(AF_INET, TestDefinition::Kind::InvalidAuth,
                                                      key, true, output, sizeof(output)) == TestStatus::CouldNotRun,
              "a tester link generation change prevents a standalone rejection pass");
    }
    for (uint8_t mask : {uint8_t{1}, uint8_t{2}, uint8_t{3}})
    {
        reset();
        CombinedWorker load;
        s_load = &load;
        load.family_mask = mask;
        load.begin.store(true);
        load.armed.store(true);
        load.observation_start_us = s_now;
        load.deadline_us = s_now + static_cast<int64_t>(PtpObservationMs + PtpResponseWindowMs + 250) * 1000;
        s_on_delay = finish_load_after_window;
        combined_ntp_worker(&load);
        check(load.full_window && load.traffic_established.load() && load.pacing_misses == 0 && s_signals == 1,
              "selected NTP traffic covers the full PTP collection window");
        check(((mask & 1) != 0 ? load.attempts[0] > 0 : load.attempts[0] == 0) &&
              ((mask & 2) != 0 ? load.attempts[1] > 0 : load.attempts[1] == 0) &&
              load.attempts[0] == s_family_controls[0] && load.attempts[1] == s_family_controls[1],
              "family mask schedules only the selected IPv4/IPv6 NTP workers");
    }
    {
        reset();
        CombinedWorker load;
        s_load = &load;
        load.family_mask = 1;
        load.begin.store(true);
        load.armed.store(true);
        load.observation_start_us = s_now;
        load.deadline_us = s_now + static_cast<int64_t>(PtpObservationMs + PtpResponseWindowMs + 250) * 1000;
        s_on_delay = finish_load_after_window;
        s_exchange_failure = ETIMEDOUT;
        combined_ntp_worker(&load);
        check(load.timeouts[0] > 0 && load.successes[0] == 0 && !load.traffic_established.load(),
              "NTP timeouts do not establish successful combined-load traffic");
    }
    {
        reset();
        CombinedWorker load;
        s_load = &load;
        load.family_mask = 2;
        load.begin.store(true);
        load.armed.store(true);
        load.observation_start_us = s_now;
        load.deadline_us = s_now + static_cast<int64_t>(PtpObservationMs + PtpResponseWindowMs + 250) * 1000;
        s_on_delay = finish_load_after_window;
        s_exchange_duration_us = 200000;
        combined_ntp_worker(&load);
        check(load.pacing_misses > 0, "client-side pacing misses are recorded under excessive NTP latency");
    }
    std::printf("NTP policy/load workers: %u assertions, %u failures\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
