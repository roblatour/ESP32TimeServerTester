#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
    struct Config
    {
        uint32_t key_id = 1;
        const char *key_value = "";
        struct { uint8_t spp = 0; } policy;
        uint32_t recovery_ms = 1000;
    };
    struct Packet {};
    struct Evidence
    {
        char output[2048] = {};
        void add(const char *format, ...)
        {
            const size_t used = std::strlen(output);
            va_list args;
            va_start(args, format);
            std::vsnprintf(output + used, sizeof(output) - used, format, args);
            va_end(args);
        }
    };
    struct Lease
    {
        bool pending = true;
        bool same_target = true;
        int64_t deadline_us = 120000000;
        uint8_t target[6] = {};
        unsigned domain = 0;
        bool matches(const Config &) { return same_target; }
        void completed(bool recovered) { pending = !recovered; }
    } s_foreign_cleanup;
    int64_t s_now = 0;
    unsigned s_pulls = 0;
    unsigned s_recoveries = 0;
    bool s_recovered = true;
    std::atomic<bool> s_stop{false};
    bool s_cancel_on_pull = false;
    int64_t esp_timer_get_time() { return s_now; }
    bool signed_policy(const Config &) { return false; }
    bool configured_key(const Config &) { return false; }
    struct Session
    {
        const Config &config;
        const std::atomic<bool> &stop;
        struct
        {
            bool initialize(uint32_t, const char *, uint8_t) { return true; }
        } auth;
        int service_authentication = 0;
        const char *reason = "completed";
        bool healthy()
        {
            if (stop.load())
            {
                reason = "cancelled";
                return false;
            }
            return true;
        }
        void rebase() {}
        void progress(const char *) {}
        bool pull(Packet &, int64_t deadline)
        {
            ++s_pulls;
            s_now = deadline;
            if (s_cancel_on_pull)
                s_stop.store(true);
            return healthy();
        }
    };
    int64_t healthy_window_us(const Config &) { return 1000; }
    bool recover_foreign(Session &session, Evidence &, int64_t)
    {
        ++s_recoveries;
        return session.healthy() && s_recovered;
    }

#include "ptp_skip_cleanup_production.inc"

    void check(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void reset()
    {
        s_foreign_cleanup = {};
        s_stop.store(false);
        s_now = 0;
        s_pulls = s_recoveries = 0;
        s_cancel_on_pull = false;
        s_recovered = true;
    }
}

int main()
{
    try
    {
        Config config;
        Session session{config, s_stop};
        Evidence evidence;
        reset();
        s_stop.store(true);
        check(!cleanup_foreign(session, evidence) && s_now == 0 && s_pulls == 0 && s_recoveries == 0,
              "skip before cleanup must not wait for the 120-second lease or send recovery traffic");
        check(s_foreign_cleanup.pending && s_foreign_cleanup.deadline_us == 120000000 &&
              std::strstr(evidence.output, "dependent PTP tests remain blocked"),
              "skip must preserve the exact pending safety lease and explain the blocker");

        reset();
        evidence = {};
        s_cancel_on_pull = true;
        check(!cleanup_foreign(session, evidence) && s_now <= 20000 && s_pulls == 1 && s_recoveries == 0 &&
              s_foreign_cleanup.pending,
              "skip during cleanup must exit within a 20 ms slice without clearing the safety gate");

        reset();
        evidence = {};
        s_recovered = false;
        check(!cleanup_foreign(session, evidence) && s_foreign_cleanup.pending,
              "lease expiry alone cannot clear an unhealthy recovery");
        s_recovered = true;
        check(cleanup_foreign(session, evidence) && !s_foreign_cleanup.pending,
              "a later healthy rerun must complete the retained cleanup");

        reset();
        evidence = {};
        s_foreign_cleanup.same_target = false;
        check(!cleanup_foreign(session, evidence) && s_pulls == 0 && s_foreign_cleanup.pending,
              "a different DUT cannot clear retained cleanup");
        std::puts("PASS: PTP skip cleanup: 5 scenarios, 0 failures");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "FAIL: PTP skip cleanup: %s\n", error.what());
        return 1;
    }
}
