#define PTP_TESTER_HOST_TEST
#include "../main/ptp_tester.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    struct CombinedWorker;
    constexpr int AF_UNSPEC = 0;
    constexpr int AF_INET = 4;
    constexpr int AF_INET6 = 6;
    constexpr size_t NtpTestCount = 20;
    constexpr size_t PtpL2TestCount = static_cast<size_t>(ptp_tester::Test::Count);
    constexpr size_t PtpUdpTestCount = 24;
    constexpr size_t TestCount = NtpTestCount + PtpL2TestCount + PtpUdpTestCount;
    constexpr bool PtpTestsEnabled = true;
    struct RunMetadata
    {
        ptp_tester::FirmwareMode mode = ptp_tester::FirmwareMode::Unknown;
    };
    enum class TestStatus
    {
        Running, AwaitingOperatorAction, Skipped, TimedOutAwaitingOperatorAction
    };
    struct TestResult
    {
        TestStatus status = TestStatus::Running;
    };
    constexpr uint32_t PtpOperatorPhaseTimeoutMs = 120000;
    bool PtpNegativeTestsEnabled = true;
    bool PtpSuppressionTestEnabled = true;
    bool PtpAssistedTestsEnabled = true;
    bool PtpCombinedLoadEnabled = true;
    bool PtpKeyRotationTestEnabled = false;
    std::atomic<bool> s_tests_running{true};
    std::atomic<bool> s_ethernet_connected{true};
    std::atomic<uint32_t> s_tester_link_generation{1};
    std::array<TestResult, TestCount> s_results{};
    size_t s_active_test = TestCount - 1;
    uint32_t s_phase_token = 0;
    int64_t s_phase_deadline_us = 0;
    char s_phase_prompt[512] = {};
    bool s_phase_acknowledged = false;
    bool s_phase_skipped = false;
    bool s_phase_waiting = false;
    constexpr int s_state_mutex = 1;
    constexpr int portMAX_DELAY = -1;
    using TickType_t = uint32_t;
    constexpr TickType_t pdMS_TO_TICKS(uint32_t value) { return value; }
    void xSemaphoreTake(int, int) {}
    void xSemaphoreGive(int) {}

    enum class Action { Acknowledge, Skip, Stop, Disconnect, LinkGeneration, Timeout };
    Action s_action = Action::Acknowledge;
    int64_t s_now = 1000000;
    std::atomic<bool> s_stop{false};
    std::atomic<bool> &s_stop_tests_requested = s_stop;
    std::atomic<bool> s_stop_run_requested{false};
    using esp_err_t = int;
    constexpr int HTTPD_400_BAD_REQUEST = 400;
    struct httpd_req_t
    {
        const char *token;
        int status = 200;
    };
    bool read_form_body(httpd_req_t *request, char *body, size_t size)
    {
        std::snprintf(body, size, "%s", request->token);
        return true;
    }
    bool form_value(const char *body, const char *, char *output, size_t size)
    {
        std::snprintf(output, size, "%s", body);
        return true;
    }
    esp_err_t httpd_resp_send_err(httpd_req_t *request, int status, const char *)
    {
        request->status = status;
        return 0;
    }
    void httpd_resp_set_status(httpd_req_t *request, const char *) { request->status = 409; }
    esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *) { return 0; }
    esp_err_t respond_to_phase(httpd_req_t *request, bool skip);
    uint32_t s_delay_calls = 0;
    uint32_t s_poll_calls = 0;
    bool s_poll_succeeds = true;
    std::string s_captured_prompt;
    size_t s_assertions = 0;

    void check(bool condition, const char *message)
    {
        ++s_assertions;
        if (!condition)
            throw std::runtime_error(message);
    }

    int64_t esp_timer_get_time() { return s_now; }

    void vTaskDelay(TickType_t ticks)
    {
        check(s_phase_waiting && s_results[s_active_test].status == TestStatus::AwaitingOperatorAction,
              "prompt is visible and awaiting action while the task waits");
        s_captured_prompt = s_phase_prompt;
        s_now += s_action == Action::Timeout ? 300000000 : static_cast<int64_t>(ticks) * 1000;
        if (++s_delay_calls < 2)
            return;
        switch (s_action)
        {
        case Action::Acknowledge: s_phase_acknowledged = true; break;
        case Action::Skip:
        {
            char token[16];
            std::snprintf(token, sizeof(token), "%lu", static_cast<unsigned long>(s_phase_token));
            httpd_req_t request{token};
            respond_to_phase(&request, true);
            check(request.status == 200, "current Skip request is accepted");
            break;
        }
        case Action::Stop: s_stop.store(true); break;
        case Action::Disconnect: s_ethernet_connected.store(false); break;
        case Action::LinkGeneration: s_tester_link_generation.fetch_add(1); break;
        case Action::Timeout: break;
        }
    }

    bool append_detail(char *output, size_t size, const char *format, ...)
    {
        const size_t used = std::strlen(output);
        va_list args;
        va_start(args, format);
        const int written = std::vsnprintf(output + used, size - used, format, args);
        va_end(args);
        return written >= 0 && static_cast<size_t>(written) < size - used;
    }

#include "ptp_assist_production.inc"

    const char *setting_for(TestDefinition::Kind kind)
    {
        TestDefinition test{};
        test.kind = kind;
        return disabled_ptp_setting(test);
    }

    void reset(Action action = Action::Acknowledge)
    {
        s_action = action;
        s_now = 1000000;
        s_stop.store(false);
        s_stop_run_requested.store(false);
        s_tests_running.store(true);
        s_ethernet_connected.store(true);
        s_tester_link_generation.store(1);
        s_phase_token = 0;
        s_phase_waiting = s_phase_acknowledged = s_phase_skipped = false;
        s_phase_prompt[0] = '\0';
        s_results.fill(TestResult{});
        s_delay_calls = s_poll_calls = 0;
        s_poll_succeeds = true;
        s_captured_prompt.clear();
        PtpAssistedTestsEnabled = PtpSuppressionTestEnabled = PtpNegativeTestsEnabled = PtpCombinedLoadEnabled = true;
        PtpKeyRotationTestEnabled = false;
    }

    bool poll(void *context)
    {
        check(context == &s_poll_calls, "cooperative poll receives its own context");
        check(s_phase_waiting, "poll executes while the prompt is still active");
        ++s_poll_calls;
        return s_poll_succeeds;
    }

    bool assist(bool restoration, PtpRunContext &context)
    {
        return ptp_assist(ptp_tester::Test::StandbyRecovery, restoration, s_stop, &context, poll, &s_poll_calls);
    }

    void prompt_polling_and_scope()
    {
        for (bool restoration : {false, true})
        {
            reset();
            PtpRunContext context{};
            check(assist(restoration, context), "both standby operator phases accept valid acknowledgements");
            check(s_poll_calls >= 3 && !context.missing_action && !context.skipped && !context.action_timed_out,
                  "foreign refresh/RX polling continues throughout both acknowledgement waits");
            check(s_phase_deadline_us == 601000000 && s_captured_prompt.find("10 minutes") != std::string::npos,
                  "standby GNSS phases allow exactly ten minutes per prompt");
            check(s_captured_prompt.find("GNSS") != std::string::npos &&
                  s_captured_prompt.find("PPS") != std::string::npos &&
                  s_captured_prompt.find("foreign master") != std::string::npos &&
                  s_captured_prompt.find("restore GNSS manually") != std::string::npos,
                  "scope, foreign refresh and manual restoration instructions are explicit");
            check(s_captured_prompt.find(restoration ? "fresh GNSS/PPS qualification" : "wire silence alone") != std::string::npos,
                  "operator verifies the actual GNSS state instead of inferring it from standby silence");
            check(std::strstr(context.phases, "operator acknowledged") != nullptr &&
                  std::strstr(context.phases, restoration ? "Restoration" : "Injection") != nullptr,
                  "both operator acknowledgements appear in detailed evidence");
            check(!s_phase_waiting && s_phase_prompt[0] == '\0' &&
                  s_results[s_active_test].status == TestStatus::Running,
                  "acknowledgement clears the prompt and restores running status");
        }
        for (auto test : {ptp_tester::Test::GnssRecovery, ptp_tester::Test::PpsRecovery,
                          ptp_tester::Test::LinkRecovery, ptp_tester::Test::RestartRecovery})
        {
            for (bool restoration : {false, true})
            {
                reset();
                PtpRunContext context{};
                check(ptp_assist(test, restoration, s_stop, &context, nullptr, nullptr),
                      "existing assisted tests still work without a cooperative poll");
                check(s_poll_calls == 0 && !context.missing_action, "null poll preserves existing assistance behavior");
                const uint32_t expected_timeout_ms = test == ptp_tester::Test::LinkRecovery ? 120000U : 600000U;
                check(operator_phase_timeout_ms(test) == expected_timeout_ms &&
                      s_phase_deadline_us == 1000000 + static_cast<int64_t>(expected_timeout_ms) * 1000 &&
                      s_captured_prompt.find(test == ptp_tester::Test::LinkRecovery ? "2 minutes" : "10 minutes") != std::string::npos,
                      "both phases use ten minutes for GNSS/PPS/restart and two minutes for link recovery");
            }
        }
        for (bool restoration : {false, true})
        {
            reset(Action::Timeout);
            PtpRunContext context{};
            check(!ptp_assist(ptp_tester::Test::RestartRecovery, restoration, s_stop, &context, nullptr, nullptr) &&
                  context.action_timed_out && s_now == 601000000 &&
                  s_results[s_active_test].status == TestStatus::TimedOutAwaitingOperatorAction,
                  "both restart/OTA phases time out after exactly ten minutes without acknowledgement");
        }
        for (auto test : {ptp_tester::Test::StandbyPpsRecovery, ptp_tester::Test::StandbyLinkRecovery,
                          ptp_tester::Test::StandbyRestartRecovery})
        {
            for (bool restoration : {false, true})
            {
                reset();
                PtpRunContext context{};
                check(ptp_assist(test, restoration, s_stop, &context, poll, &s_poll_calls),
                      "new standby fault and restoration prompts accept operator acknowledgements");
                check(s_poll_calls >= 3 && s_captured_prompt.find("foreign master") != std::string::npos &&
                      s_captured_prompt.find("tester link") != std::string::npos,
                      "new standby prompts keep refreshing candidates and distinguish the tester link");
                check(operator_phase_timeout_ms(test) == 600000U,
                      "new standby interactions have bounded ten-minute operator phases");
            }
        }
        for (auto phase : {ptp_tester::Test::KeyRotationReady, ptp_tester::Test::KeyRotation,
                           ptp_tester::Test::KeyRotationRestore})
        {
            for (bool restoration : {false, true})
            {
                reset();
                PtpRunContext context{};
                check(ptp_assist(phase, restoration, s_stop, &context, nullptr, nullptr) &&
                      s_captured_prompt.find("fixture") != std::string::npos &&
                      s_captured_prompt.find("key") != std::string::npos,
                      "rotation phases require explicit fixture and key confirmation");
                check(s_captured_prompt.find("0123456789") == std::string::npos,
                      "operator prompts do not expose fixture or profile key values");
            }
        }
        for (bool restoration : {false, true})
        {
            reset();
            PtpRunContext context{};
            check(ptp_assist(ptp_tester::Test::StartupListening, restoration, s_stop, &context, poll, &s_poll_calls),
                  "timestamp-referenced startup prompts accept confirmed worker states");
            check(s_poll_calls >= 3 && s_captured_prompt.find("worker") != std::string::npos &&
                  s_captured_prompt.find("qualified") != std::string::npos,
                  "startup requires qualified timing and continuous tester observation");
            check(operator_phase_timeout_ms(ptp_tester::Test::StartupListening) == 600000U,
                  "startup fixture prompts remain bounded");
        }
        for (bool restoration : {false, true})
        {
            reset();
            PtpRunContext context{};
            check(ptp_assist(ptp_tester::Test::StandbyExpiryUnqualified, restoration, s_stop, &context,
                             restoration ? nullptr : poll, restoration ? nullptr : &s_poll_calls),
                  "unqualified-expiry phases require explicit operator acknowledgement");
            check(s_captured_prompt.find("generation") != std::string::npos &&
                  s_captured_prompt.find("GNSS") != std::string::npos &&
                  s_captured_prompt.find(restoration ? "no longer being refreshed" : "unqualified") != std::string::npos,
                  "the operator independently confirms timing, generation and whether foreign refresh continues");
        }
        for (bool restoration : {false, true})
        {
            reset();
            PtpRunContext context{};
            check(ptp_assist(ptp_tester::Test::SelectionNtpHoldover, restoration, s_stop, &context, poll, &s_poll_calls),
                  "loaded NTP holdover phases accept explicit operator confirmation");
            check(s_poll_calls >= 3 && s_captured_prompt.find("GNSS") != std::string::npos &&
                  s_captured_prompt.find("foreign master") != std::string::npos &&
                  (restoration || s_captured_prompt.find("NTP holdover") != std::string::npos),
                  "the candidate stays refreshed and the operator independently confirms holdover");
        }
        for (auto phase : {ptp_tester::Test::StartupRestore, ptp_tester::Test::TimingRestore})
        {
            reset();
            PtpRunContext context{};
            check(ptp_assist(phase, true, s_stop, &context, nullptr, nullptr) &&
                  s_captured_prompt.find("qualification") != std::string::npos &&
                  s_captured_prompt.find("tester link") != std::string::npos,
                  "interrupted worker and timing fixtures require an explicit qualified restoration acknowledgement");
        }
    }

    void interruption_and_skip()
    {
        reset();
        s_poll_succeeds = false;
        PtpRunContext failed{};
        check(!assist(false, failed) && failed.missing_action && !failed.action_timed_out,
              "refresh/RX failure rejects the phase without pretending it timed out");
        check(s_poll_calls == 1 && s_delay_calls == 0 &&
              std::strstr(failed.phases, "observation/refresh failed") != nullptr,
              "failed polling is surfaced immediately in detailed evidence");
        for (Action action : {Action::Skip, Action::Stop, Action::Disconnect, Action::LinkGeneration, Action::Timeout})
        {
            reset(action);
            PtpRunContext context{};
            check(!assist(true, context), "skip, stop, timeout and tester link changes cannot acknowledge restoration");
            check(!s_phase_waiting && s_phase_prompt[0] == '\0', "all interrupted prompts are cleared");
            if (action == Action::Skip)
            {
                check(context.skipped && !context.action_timed_out &&
                      s_results[s_active_test].status == TestStatus::Skipped,
                      "skip remains Skipped, not a failure or timeout");
                const uint32_t token = s_phase_token;
                const uint32_t polls = s_poll_calls;
                check(s_delay_calls == 2 && s_now == 1100000,
                      "skip exits the operator wait within its 50 ms polling interval");
                check(!assist(false, context) && context.skipped && s_phase_token == token &&
                      s_poll_calls == polls && !s_phase_waiting,
                      "skipped tests cannot reopen a later phase or resume foreign refresh");
                check(s_stop_tests_requested.load() && !s_stop_run_requested.load(),
                      "Skip cancels current workers without stopping the remaining run");
            }
            else if (action == Action::Timeout)
                check(context.action_timed_out && context.missing_action &&
                      s_results[s_active_test].status == TestStatus::TimedOutAwaitingOperatorAction,
                      "missing acknowledgement retains the distinct timeout result");
            else
                check(context.missing_action && !context.action_timed_out,
                      "cancellation and tester link loss remain inconclusive, not operator timeouts");
            if (action == Action::Disconnect || action == Action::LinkGeneration)
                check(std::strstr(context.phases, "Tester Ethernet link changed") != nullptr,
                      "tester link disruption is not attributed to the DUT");
        }
        for (const char *token : {"0", "invalid", "8", "4294967296"})
        {
            reset();
            s_phase_token = 7;
            s_phase_waiting = true;
            s_phase_deadline_us = s_now + 1000000;
            httpd_req_t request{token};
            respond_to_phase(&request, true);
            check(request.status != 200 && !s_stop_tests_requested.load() && !s_phase_skipped,
                  "invalid or stale Skip requests cannot cancel a test");
        }
    }

    void both_opt_ins()
    {
        for (bool assisted : {false, true})
            for (bool suppression : {false, true})
            {
                reset();
                PtpAssistedTestsEnabled = assisted;
                PtpSuppressionTestEnabled = suppression;
                const char *disabled = setting_for(TestDefinition::Kind::PtpStandbyRecovery);
                check((disabled == nullptr) == (assisted && suppression),
                      "standby recovery is disabled unless both independent opt-ins are enabled");
                check((setting_for(TestDefinition::Kind::PtpStandbyExpiryUnqualified) == nullptr) ==
                          (assisted && suppression),
                      "unqualified candidate expiry also requires assisted and foreign-master opt-ins");
                check((setting_for(TestDefinition::Kind::PtpSelectionNtpHoldover) == nullptr) ==
                          (assisted && suppression),
                      "NTP holdover under load requires assisted and foreign-master opt-ins");
                check((setting_for(TestDefinition::Kind::PtpSelectionNtpPolicyLoad) == nullptr) == suppression,
                      "NTP key-policy load also requires foreign-master opt-in");
                if (disabled != nullptr)
                    check(std::strcmp(disabled, assisted ? "PtpSuppressionTestEnabled" : "PtpAssistedTestsEnabled") == 0,
                          "UI/report identify the exact disabled setting");
                check((setting_for(TestDefinition::Kind::PtpGnssRecovery) == nullptr) == assisted &&
                      (setting_for(TestDefinition::Kind::PtpForeignAnnounce) == nullptr) == suppression,
                      "existing assisted and foreign-master tests keep their own opt-in semantics");
            }
        for (bool negative : {false, true})
            for (bool suppression : {false, true})
            {
                PtpNegativeTestsEnabled = negative;
                PtpSuppressionTestEnabled = suppression;
                PtpAssistedTestsEnabled = false;
                const char *disabled = setting_for(TestDefinition::Kind::PtpOptionalSignedRejection);
                check((disabled == nullptr) == (negative && suppression),
                      "optional signed-input rejection requires negative and foreign-master opt-ins, not assistance");
                if (disabled != nullptr)
                    check(std::strcmp(disabled, negative ? "PtpSuppressionTestEnabled" : "PtpNegativeTestsEnabled") == 0,
                          "optional rejection UI/report identify the exact disabled setting");
            }
        for (bool assisted : {false, true})
            for (bool negative : {false, true})
                for (bool rotation : {false, true})
                {
                    PtpAssistedTestsEnabled = assisted;
                    PtpNegativeTestsEnabled = negative;
                    PtpKeyRotationTestEnabled = rotation;
                    check((setting_for(TestDefinition::Kind::PtpKeyRotation) == nullptr) ==
                              (assisted && negative && rotation),
                          "staged rotation requires all three independent safety opt-ins");
                }
        for (bool combined : {false, true})
            for (bool suppression : {false, true})
            {
                PtpCombinedLoadEnabled = combined;
                PtpSuppressionTestEnabled = suppression;
                check((setting_for(TestDefinition::Kind::PtpSelectionNtpLoad) == nullptr) ==
                          (combined && suppression),
                      "selection-aware NTP load requires both load and foreign-master opt-ins");
            }
        for (bool combined : {false, true})
            for (bool suppression : {false, true})
                for (bool enabled : {false, true})
                {
                    PtpCombinedLoadEnabled = combined;
                    PtpSuppressionTestEnabled = suppression;
                    PtpNegativeTestsEnabled = PtpAssistedTestsEnabled = enabled;
                    check((setting_for(TestDefinition::Kind::PtpSelectionNtpPolicyLoad) == nullptr) ==
                              (combined && suppression && enabled),
                          "loaded NTP negative probes require load, foreign-master and negative opt-ins");
                    check((setting_for(TestDefinition::Kind::PtpSelectionNtpHoldover) == nullptr) ==
                              (combined && suppression && enabled),
                          "loaded NTP holdover requires load, foreign-master and assisted opt-ins");
                }
    }

    void catalog_and_dispatch()
    {
        std::array<bool, TestCount> seen{};
        std::array<unsigned, 8> groups{};
        unsigned previous_group = 0;
        for (size_t position = 0; position < TestCount; ++position)
        {
            const size_t index = TestOrder[position];
            check(index < TestCount && !seen[index] && TestNumbers[index] == position + 1,
                  "UI/report ordering exposes every real catalog entry exactly once with sequential numbers");
            seen[index] = true;
            check(Tests[index].name && Tests[index].name[0] && Tests[index].description && Tests[index].description[0],
                  "every selectable/reportable test has a name and description");
            if (index >= NtpTestCount && index < NtpTestCount + PtpL2TestCount)
                check(is_ptp_l2_test(Tests[index]) &&
                      static_cast<size_t>(ptp_test_id(Tests[index])) == index - NtpTestCount,
                      "each PTP L2 entry dispatches to its stable engine ID");
            if (index >= NtpTestCount + PtpL2TestCount)
                check(is_ptp_udp_test(Tests[index]) && std::strncmp(Tests[index].name, "PTP L3 UDP ", 11) == 0,
                      "each UDP entry has the required label and dispatches to the UDP engine");
            if (is_ptp_l2_test(Tests[index]))
                check(std::strncmp(Tests[index].name, "PTP L2", 6) == 0, "all old PTP names identify L2");
            const auto &test = Tests[index];
            const unsigned group = test.kind == TestDefinition::Kind::Ping ? 0
                                 : !is_ptp_test(test) ? 1
                                 : is_ptp_l2_test(test) && test_involves_ntp(test) ? 2
                                 : is_ptp_udp_test(test) && test_involves_ntp(test) ? 3
                                 : is_ptp_l2_test(test) && !test_requires_operator(test) ? 4
                                 : is_ptp_udp_test(test) && !test_requires_operator(test) ? 5
                                 : is_ptp_l2_test(test) ? 6 : 7;
            check(group >= previous_group, "display order follows ping, NTP, combined, L2/L3 unattended and L2/L3 assisted groups");
            previous_group = group;
            ++groups[group];
        }
        check(groups == std::array<unsigned, 8>{{2, 18, 4, 3, 25, 13, 11, 8}},
              "all eight requested groups contain the expected tests");
        check(TestOrder[0] == 0 && TestOrder[1] == 10, "both IPv4 and IPv6 pings precede all NTP tests");
        for (size_t index : {size_t{1}, size_t{2}, size_t{11}, size_t{12}})
            check(std::strstr(Tests[index].description, "rejected") &&
                  std::strstr(Tests[index].description, "authenticated"),
                  "standard NTP descriptions explicitly state required-policy rejection and signed controls");
        for (size_t index : {size_t{3}, size_t{9}, size_t{13}, size_t{19}})
            check(std::strstr(Tests[index].description, "authenticated") &&
                  std::strstr(Tests[index].description, "required"),
                  "interleaved and stress descriptions identify their required-policy signed mode");
        for (size_t index : {size_t{6}, size_t{7}, size_t{8}, size_t{16}, size_t{17}, size_t{18}})
            check(std::strstr(Tests[index].description, "before/after controls") != nullptr,
                  "standalone NTP negative-test descriptions disclose their healthy controls");
        struct ModeCase { ptp_tester::FirmwareMode mode; bool ntp, l2, l3, unknown; };
        const std::array<ModeCase, 8> modes{{
            {ptp_tester::FirmwareMode::Unknown, false, false, false, true},
            {ptp_tester::FirmwareMode::NtpOnly, true, false, false, false},
            {ptp_tester::FirmwareMode::PtpOnly, false, true, false, false},
            {ptp_tester::FirmwareMode::PtpL3Only, false, false, true, false},
            {ptp_tester::FirmwareMode::Combined, true, true, false, false},
            {ptp_tester::FirmwareMode::NtpPtpL3, true, false, true, false},
            {ptp_tester::FirmwareMode::PtpL2PtpL3, false, true, true, false},
            {ptp_tester::FirmwareMode::NtpPtpL2PtpL3, true, true, true, false}
        }};
        for (const ModeCase &mode : modes)
        {
            TestRequest request{};
            request.metadata.mode = mode.mode;
            request.udp_ipv4_configured = request.udp_ipv6_configured = true;
            for (size_t index = 0; index < TestCount; ++index)
            {
                const auto &test = Tests[index];
                const bool l2 = is_ptp_l2_test(test), l3 = is_ptp_udp_test(test);
                const bool expected = test.kind == TestDefinition::Kind::Ping ? mode.mode != ptp_tester::FirmwareMode::PtpOnly
                                    : !l2 && !l3 ? mode.unknown || mode.ntp
                                    : l2 && test.kind == TestDefinition::Kind::PtpOnly ? mode.mode == ptp_tester::FirmwareMode::PtpOnly
                                    : test_involves_ntp(test) ? mode.ntp && (l2 ? mode.l2 : mode.l3)
                                    : mode.unknown || (l2 ? mode.l2 : mode.l3);
                check(compatible_with_firmware_mode(test, mode.mode) == expected, "firmware mode matrix matches declared services");
                request.scope = TestRequest::Scope::Ptp;
                check(test_selected(request, index) == (expected && (l2 || l3)), "all PTP group is the union of both transports");
                request.scope = TestRequest::Scope::PtpL2;
                check(test_selected(request, index) == (expected && l2), "PTP L2 group excludes PTP L3 UDP");
                request.scope = TestRequest::Scope::PtpUnattended;
                check(test_selected(request, index) == (expected && l2 && !test_requires_operator(test)), "PTP L2 unattended scope");
                request.scope = TestRequest::Scope::PtpAssisted;
                check(test_selected(request, index) == (expected && l2 && test_requires_operator(test)), "PTP L2 assisted scope");
                request.scope = TestRequest::Scope::PtpUdp;
                check(test_selected(request, index) == (expected && l3), "PTP L3 UDP group excludes PTP L2");
                request.scope = TestRequest::Scope::PtpUdpUnattended;
                check(test_selected(request, index) == (expected && l3 && !test_requires_operator(test)), "PTP L3 UDP unattended scope");
                request.scope = TestRequest::Scope::PtpUdpAssisted;
                check(test_selected(request, index) == (expected && l3 && test_requires_operator(test)), "PTP L3 UDP assisted scope");
            }
        }
        TestRequest request{};
        request.udp_ipv4_configured = request.udp_ipv6_configured = true;
        request.metadata.mode = ptp_tester::FirmwareMode::PtpOnly;
        request.scope = TestRequest::Scope::Ptp;
        check(!test_selected(request, NtpTestCount + static_cast<size_t>(ptp_tester::Test::SelectionNtpHoldover)) &&
              !test_selected(request, NtpTestCount + static_cast<size_t>(ptp_tester::Test::SelectionNtpPolicyLoad)) &&
              test_selected(request, NtpTestCount + static_cast<size_t>(ptp_tester::Test::PtpOnly)),
              "combined-load scenarios are excluded from PTP-only firmware without excluding its dedicated test");
        request.metadata.mode = ptp_tester::FirmwareMode::NtpPtpL2PtpL3;
        request.scope = TestRequest::Scope::Ptp;
        for (bool ipv4 : {false, true})
            for (bool ipv6 : {false, true})
            {
                request.udp_ipv4_configured = ipv4;
                request.udp_ipv6_configured = ipv6;
                for (size_t index = NtpTestCount + PtpL2TestCount; index < TestCount; ++index)
                {
                    const auto &test = Tests[index];
                    const bool configured = test.family == AF_INET ? ipv4 : test.family == AF_INET6 ? ipv6 : ipv4 && ipv6;
                    check(test_selected(request, index) == configured, "blank UDP addresses disable that family and dual-stack scenarios");
                    request.scope = TestRequest::Scope::Individual;
                    request.index = static_cast<int>(index);
                    check(test_selected(request, index) == configured, "individual requests cannot bypass a disabled UDP family");
                    request.scope = TestRequest::Scope::Ptp;
                }
            }
    }
}

int main()
{
    try
    {
        prompt_polling_and_scope();
        interruption_and_skip();
        both_opt_ins();
        catalog_and_dispatch();
        std::printf("PASS: PTP assistance/catalog: 4 groups, %zu assertions, 0 failures\n", s_assertions);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "FAIL: PTP assistance: %s\n", error.what());
        return 1;
    }
}
