#define PTP_TESTER_HOST_TEST
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ptp_tester.h"

namespace
{
#include "constants.inc"
#include "types.inc"

    constexpr char NvsNamespace[] = "tester";
    constexpr char TargetHistoryNvsKey[] = "profiles_v5";
    constexpr char Tag[] = "profile_test";
    constexpr bool WS2812_RGB_enabled = true;
    constexpr int ESP_OK = 0;
    constexpr int ESP_ERR_NVS_NOT_FOUND = 1;
    constexpr int ESP_ERR_NVS_INVALID_LENGTH = 2;
    constexpr int ESP_ERR_NO_MEM = 3;
    constexpr int NVS_READWRITE = 1;
    using esp_err_t = int;
    using nvs_handle_t = int;
    using ResponseBuffer = std::unique_ptr<char[]>;
    TargetHistoryEntry s_profile{};
    TargetHistoryStorage s_target_history{};
    std::map<std::string, std::vector<uint8_t>> nvs_data;
    std::vector<std::string> nvs_operations;
    int warning_count = 0;
    int commit_count = 0;
    int assertions = 0;

    void check(bool condition, const char *description)
    {
        ++assertions;
        if (!condition)
            throw std::runtime_error(description);
    }

    void check_esp(int error)
    {
        if (error != ESP_OK)
            throw std::runtime_error("unexpected simulated NVS failure: " + std::to_string(error));
    }

    template <typename... Args> void warning(Args...) { ++warning_count; }
    const char *esp_err_to_name(int error)
    {
        return error == ESP_OK ? "OK" : error == ESP_ERR_NVS_NOT_FOUND ? "NOT_FOUND" : "INVALID_LENGTH";
    }
    int nvs_open(const char *, int, nvs_handle_t *handle) { *handle = 1; return ESP_OK; }
    int nvs_get_blob(nvs_handle_t, const char *key, void *output, size_t *size)
    {
        const auto item = nvs_data.find(key);
        if (item == nvs_data.end())
            return ESP_ERR_NVS_NOT_FOUND;
        const size_t actual = item->second.size();
        if (*size < actual)
        {
            *size = actual;
            return ESP_ERR_NVS_INVALID_LENGTH;
        }
        std::memcpy(output, item->second.data(), actual);
        *size = actual;
        return ESP_OK;
    }
    int nvs_get_str(nvs_handle_t handle, const char *key, char *output, size_t *size)
    {
        return nvs_get_blob(handle, key, output, size);
    }
    int nvs_set_blob(nvs_handle_t, const char *key, const void *value, size_t size)
    {
        const auto *bytes = static_cast<const uint8_t *>(value);
        nvs_data[key] = std::vector<uint8_t>(bytes, bytes + size);
        nvs_operations.push_back(std::string("set:") + key);
        return ESP_OK;
    }
    int nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
    {
        return nvs_set_blob(handle, key, value, std::strlen(value) + 1);
    }
    int nvs_erase_key(nvs_handle_t, const char *key)
    {
        if (!nvs_data.erase(key))
            return ESP_ERR_NVS_NOT_FOUND;
        nvs_operations.push_back(std::string("erase:") + key);
        return ESP_OK;
    }
    int nvs_commit(nvs_handle_t) { ++commit_count; return ESP_OK; }
    void nvs_close(nvs_handle_t) {}
    bool is_self_target(const char *text) { return std::strcmp(text, "192.0.2.10") == 0; }
    bool valid_ptp_mac(const char *) { return true; }

#define ESP_ERROR_CHECK(error) check_esp(error)
#define ESP_LOGW(...) warning(__VA_ARGS__)
#include "helpers.inc"

    struct Metadata { const char *project_name; const char *version; };
    Metadata *get_app_metadata()
    {
        static Metadata metadata{"ESP32TimeServerTester", "test"};
        return &metadata;
    }

    template <size_t Size> void set(char (&destination)[Size], const char *value)
    {
        const int length = std::snprintf(destination, Size, "%s", value);
        check(length >= 0 && static_cast<size_t>(length) < Size, "fixture string fits");
    }

    void reset()
    {
        nvs_data.clear();
        nvs_operations.clear();
        warning_count = commit_count = 0;
        s_profile = {};
        s_target_history = {};
    }

    void add_selected(const char *name)
    {
        check(nvs_set_str(1, "selected_v1", name) == ESP_OK, "selected target fixture saved");
    }

    void migration()
    {
        reset();
        TargetHistoryStorageV4 legacy{};
        legacy.count = 1;
        auto &old = legacy.entries[0];
        set(old.target, "Alpha");
        set(old.ipv4, "192.0.2.20");
        set(old.ipv6, "2001:db8::20");
        old.ipv4_ntp_port = 123;
        old.ipv6_ntp_port = 124;
        old.key.key_id = 42;
        set(old.key.key_value, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456");
        old.stress_tests = true;
        old.last_stored = 9;
        set(old.ptp_mac, "02:11:22:33:44:55");
        old.ptp_domain = 6;
        old.ptp_key.key_id = 44;
        set(old.ptp_key.key_value, "0123456789ABCDEFGHIJKLMNOPQRSTUV");
        old.firmware_mode = ptp_tester::FirmwareMode::Combined;
        check(nvs_set_blob(1, "profiles_v4", &legacy, sizeof(legacy)) == ESP_OK, "v4 history fixture saved");
        add_selected("Alpha");
        nvs_operations.clear();
        load_settings();
        check(s_target_history.count == 1 && std::strcmp(s_profile.target, "Alpha") == 0,
              "selected v4 profile migrates");
        check(s_profile.firmware_mode == ptp_tester::FirmwareMode::Combined &&
              std::strcmp(s_profile.ipv4, "192.0.2.20") == 0 &&
              std::strcmp(s_profile.ipv6, "2001:db8::20") == 0 &&
              s_profile.ipv4_ntp_port == 123 && s_profile.ipv6_ntp_port == 124 &&
              s_profile.key.key_id == 42 && s_profile.ptp_key.key_id == 44 &&
              s_profile.ptp_domain == 6 && s_profile.stress_tests,
              "migration preserves existing NTP, L2, authentication and mode settings");
        check(!s_profile.ptp_udp_ipv4[0] && !s_profile.ptp_udp_ipv6[0] &&
              s_profile.ptp_udp_domain == DefaultPtpUdpDomain &&
              s_profile.ptp_udp_withdrawal_ms == DefaultPtpUdpWithdrawalMs,
              "migration initializes disabled L3 families and safe defaults");
        check(nvs_data.count("profiles_v5") == 1 && nvs_data.count("profiles_v4") == 0 && commit_count == 1,
              "migrated history is persisted and legacy key retired");
        const auto write = std::find(nvs_operations.begin(), nvs_operations.end(), "set:profiles_v5");
        const auto erase = std::find(nvs_operations.begin(), nvs_operations.end(), "erase:profiles_v4");
        check(write != nvs_operations.end() && erase != nvs_operations.end() && write < erase,
              "new history is written before the legacy key is erased");

        set(s_profile.ptp_udp_ipv4, "192.0.2.20");
        set(s_profile.ptp_udp_ipv6, "fe80::20");
        s_profile.ptp_udp_domain = 17;
        s_profile.ptp_udp_withdrawal_ms = 25000;
        s_profile.firmware_mode = ptp_tester::FirmwareMode::NtpPtpL2PtpL3;
        s_target_history.entries[0] = s_profile;
        save_settings();
        s_profile = {};
        s_target_history = {};
        load_settings();
        check(std::strcmp(s_profile.ptp_udp_ipv4, "192.0.2.20") == 0 &&
              std::strcmp(s_profile.ptp_udp_ipv6, "fe80::20") == 0 &&
              s_profile.ptp_udp_domain == 17 && s_profile.ptp_udp_withdrawal_ms == 25000 &&
              s_profile.firmware_mode == ptp_tester::FirmwareMode::NtpPtpL2PtpL3,
              "L3 targets, domain, withdrawal and combined firmware mode survive reload");
        check(s_profile.key.key_id == 42 && s_profile.ptp_key.key_id == 44,
              "independent existing keys survive the L3 save/reload");

        reset();
        TargetHistoryStorageV3 older{};
        older.count = 1;
        set(older.entries[0].target, "Legacy");
        older.entries[0].last_stored = 1;
        const uint8_t invalid = 1;
        check(nvs_set_blob(1, "profiles_v4", &invalid, sizeof(invalid)) == ESP_OK &&
              nvs_set_blob(1, "profiles_v3", &older, sizeof(older)) == ESP_OK,
              "old and malformed histories initialized");
        add_selected("Legacy");
        load_settings();
        check(warning_count == 1 && std::strcmp(s_profile.target, "Legacy") == 0 &&
              s_profile.firmware_mode == ptp_tester::FirmwareMode::Unknown &&
              s_profile.ptp_udp_withdrawal_ms == DefaultPtpUdpWithdrawalMs,
              "invalid v4 is reported and older v3 history still migrates");
    }

    void modes()
    {
        static_assert(static_cast<unsigned>(ptp_tester::FirmwareMode::Unknown) == 0);
        static_assert(static_cast<unsigned>(ptp_tester::FirmwareMode::NtpOnly) == 1);
        static_assert(static_cast<unsigned>(ptp_tester::FirmwareMode::PtpOnly) == 2);
        static_assert(static_cast<unsigned>(ptp_tester::FirmwareMode::Combined) == 3);
        for (auto mode : {ptp_tester::FirmwareMode::Unknown, ptp_tester::FirmwareMode::NtpOnly,
                          ptp_tester::FirmwareMode::PtpOnly, ptp_tester::FirmwareMode::PtpL3Only,
                          ptp_tester::FirmwareMode::Combined, ptp_tester::FirmwareMode::NtpPtpL3,
                          ptp_tester::FirmwareMode::PtpL2PtpL3, ptp_tester::FirmwareMode::NtpPtpL2PtpL3})
        {
            ptp_tester::FirmwareMode parsed{};
            check(parse_firmware_mode(firmware_mode_text(mode), parsed) && parsed == mode &&
                  firmware_mode_label(mode)[0] != '\0', "all eight firmware modes round-trip");
        }
        ptp_tester::FirmwareMode parsed{};
        check(!parse_firmware_mode("Unexpected", parsed), "unknown firmware mode is rejected");
    }

    ResponseBuffer options_for_full_history()
    {
        ResponseBuffer target_options(new char[TargetOptionsBufferSize]{});
        size_t options_cursor = 0;
        const std::string ipv6(39, 'f');
        const std::string key(32, 'A');
        for (size_t index = 0; index < MaxTargetHistory; ++index)
        {
            TargetHistoryEntry entry{};
            const std::string target_name = std::string(38, '"') + (index < 10 ? "0" : "") + std::to_string(index);
            set(entry.target, target_name.c_str());
            set(entry.ipv4, "255.255.255.255");
            set(entry.ipv6, ipv6.c_str());
            entry.ipv4_ntp_port = entry.ipv6_ntp_port = 65535;
            entry.key.key_id = entry.ptp_key.key_id = 65535;
            set(entry.key.key_value, key.c_str());
            set(entry.ptp_key.key_value, key.c_str());
            set(entry.ptp_mac, "FE:FF:FF:FF:FF:FF");
            entry.ptp_domain = 255;
            set(entry.ptp_udp_ipv4, "255.255.255.255");
            set(entry.ptp_udp_ipv6, ipv6.c_str());
            entry.ptp_udp_domain = 255;
            entry.ptp_udp_withdrawal_ms = 60000;
            entry.stress_tests = true;
            entry.firmware_mode = ptp_tester::FirmwareMode::NtpPtpL2PtpL3;
            char escaped_target[TargetNameSize * 6];
            html_escape(entry.target, escaped_target, sizeof(escaped_target));
            char ipv4_port[6], ipv6_port[6], key_id[6], ptp_key_id[6];
            format_optional_u16(entry.ipv4_ntp_port, ipv4_port);
            format_optional_u16(entry.ipv6_ntp_port, ipv6_port);
            format_optional_u16(entry.key.key_id, key_id);
            format_optional_u16(entry.ptp_key.key_id, ptp_key_id);
#include "option.inc"
            check(written >= 0 && static_cast<size_t>(written) < TargetOptionsBufferSize - options_cursor,
                  "maximum profile history fits the generated options buffer");
            options_cursor += static_cast<size_t>(written);
        }
        return target_options;
    }

    std::string render_page(const TargetHistoryEntry &profile, ResponseBuffer &target_options)
    {
        const bool physical_leds_on = true;
        char target[TargetNameSize * 6];
        html_escape(profile.target, target, sizeof(target));
        char ipv4_port[6], ipv6_port[6], key_id[6], ptp_key_id[6];
        format_optional_u16(profile.ipv4_ntp_port, ipv4_port);
        format_optional_u16(profile.ipv6_ntp_port, ipv6_port);
        format_optional_u16(profile.key.key_id, key_id);
        format_optional_u16(profile.ptp_key.key_id, ptp_key_id);
        const char *stress_test_request_count = "1,000";
        const char *mac_address = "02:11:22:33:44:55";
        const char *ipv4_address = "192.0.2.10";
        const char *ipv6_lla = "fe80::10";
        const char *ipv6_ula = "fd00::10";
        const char *ipv6_gua = "2001:db8::10";
        ResponseBuffer page(new char[PageBufferSize]{});
#include "page.inc"
        check(length >= 0 && static_cast<size_t>(length) < PageBufferSize,
              "complete generated page fits even with 20 maximally sized profiles");
        return page.get();
    }

    std::string render_report_header()
    {
        ResponseBuffer report(new char[ReportBufferSize]{});
        const char *target = "Alpha";
        char ipv4_port[6], ipv6_port[6], key_id[6];
        format_optional_u16(s_profile.ipv4_ntp_port, ipv4_port);
        format_optional_u16(s_profile.ipv6_ntp_port, ipv6_port);
        format_optional_u16(s_profile.key.key_id, key_id);
#include "report.inc"
        check(cursor < ReportBufferSize, "generated report header fits");
        return report.get();
    }

    void page_and_report()
    {
        reset();
        set(s_profile.target, "Alpha");
        set(s_profile.ptp_udp_ipv4, "192.0.2.20");
        set(s_profile.ptp_udp_ipv6, "fe80::20");
        s_profile.ptp_udp_domain = 17;
        s_profile.ptp_udp_withdrawal_ms = 25000;
        s_profile.key.key_id = 7;
        set(s_profile.key.key_value, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456");
        s_profile.ptp_key.key_id = 8;
        set(s_profile.ptp_key.key_value, "0123456789ABCDEFGHIJKLMNOPQRSTUV");
        s_profile.firmware_mode = ptp_tester::FirmwareMode::NtpPtpL2PtpL3;
        ResponseBuffer options = options_for_full_history();
        const std::string page = render_page(s_profile, options);
        check(page.find("data-ptp-udp-ipv4=") != std::string::npos &&
              page.find("name=\"ptp_udp_ipv6\"") != std::string::npos &&
              page.find("name=\"ptp_udp_domain\"") != std::string::npos &&
              page.find("name=\"ptp_udp_withdrawal\"") != std::string::npos,
              "settings and saved options expose all UDP fields");
        check(page.find("ptp_udp_key") == std::string::npos, "L3 UDP has no symmetric-key setting");
        const std::string report = render_report_header();
        check(report.find("192.0.2.20") != std::string::npos && report.find("fe80::20") != std::string::npos &&
              report.find("25000 ms") != std::string::npos && report.find("224.0.1.129") != std::string::npos,
              "downloadable report includes UDP settings and multicast details");
        check(report.find(s_profile.key.key_value) == std::string::npos &&
              report.find(s_profile.ptp_key.key_value) == std::string::npos,
              "report continues to redact NTP and L2 PTP keys");
        std::ofstream file("rendered_page.html", std::ios::binary);
        file << page;
        check(file.good(), "rendered page fixture was written");
    }
}

int main()
{
    try
    {
        WSADATA data{};
        check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WinSock initializes");
        migration();
        modes();
        page_and_report();
        WSACleanup();
        std::printf("PASS: PTP settings/profile/report: 3 groups, %d assertions, 0 failures\n", assertions);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "FAIL after %d assertions: %s\n", assertions, error.what());
        WSACleanup();
        return 1;
    }
}
