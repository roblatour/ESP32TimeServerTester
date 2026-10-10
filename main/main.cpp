// ESP32TimeServerTester v1.0.0
// Copyright Rob Latour, 2026
// License: MIT
// Website: https://github.com/roblatour/ESP32TimeServerTester
//

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <limits>

#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "ping/ping_sock.h"
#include "psa/crypto.h"

#include "ESP32TimeServerTesterSettings.h"
#include "app_metadata.h"

extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

namespace
{
    constexpr char Tag[] = "time_tester";
    constexpr char NvsNamespace[] = "tester";
    constexpr size_t TargetNameSize = 41;
    constexpr size_t MaxTargetHistory = 20;
    constexpr char TargetHistoryNvsKey[] = "profiles_v1";
    constexpr size_t TargetOptionsBufferSize = 17000;
    constexpr size_t TestCount = 20;
    constexpr size_t PageBufferSize = 36000;
    constexpr size_t StatusResponseBufferSize = 7000;
    constexpr size_t ReportBufferSize = 6000;
    constexpr uint64_t NtpEpochOffset = 2208988800ULL;
    constexpr size_t NtpPacketSize = 48;
    constexpr size_t NtpAuthTrailerSize = 24;

    enum class TestStatus : uint8_t
    {
        NotRun,
        Pass,
        Fail,
        CouldNotRun,
        DidNotRun
    };

    struct TestDefinition
    {
        const char *name;
        int family;
        bool enabled;
        enum class Kind : uint8_t
        {
            Ping,
            StandardV3,
            StandardV4,
            Interleaved,
            Auth,
            AuthInterleaved,
            InvalidAuth,
            MissingAuth,
            TruncatedMac,
            MiniStress
        } kind;
    };

    struct TestResult
    {
        TestStatus status = TestStatus::NotRun;
        char timestamp[48] = "n/a";
    };

    struct symmetric_key_t
    {
        uint16_t key_id = 0;
        char key_value[33] = {};
    };

    struct TargetHistoryEntry
    {
        char target[TargetNameSize] = {};
        char ipv4[INET_ADDRSTRLEN] = {};
        char ipv6[INET6_ADDRSTRLEN] = {};
        uint16_t ipv4_ntp_port = 0;
        uint16_t ipv6_ntp_port = 0;
        symmetric_key_t key{};
        bool stress_tests = false;
        uint64_t last_stored = 0;
    };

    struct TargetHistoryStorage
    {
        uint8_t count = 0;
        std::array<TargetHistoryEntry, MaxTargetHistory> entries{};
    };

    const std::array<TestDefinition, TestCount> Tests = {{
        {"IPv4 Ping target time server", AF_INET, true, TestDefinition::Kind::Ping},
        {"IPv4 NTPv3 Standard", AF_INET, true, TestDefinition::Kind::StandardV3},
        {"IPv4 NTPv4 Standard", AF_INET, true, TestDefinition::Kind::StandardV4},
        {"IPv4 NTPv4 Interleaved", AF_INET, true, TestDefinition::Kind::Interleaved},
        {"IPv4 NTPv4 Symmetric-key standard", AF_INET, true, TestDefinition::Kind::Auth},
        {"IPv4 NTPv4 Symmetric-key interleaved", AF_INET, true, TestDefinition::Kind::AuthInterleaved},
        {"IPv4 NTPv4 Invalid symmetric-key", AF_INET, true, TestDefinition::Kind::InvalidAuth},
        {"IPv4 NTPv4 Missing symmetric-key", AF_INET, true, TestDefinition::Kind::MissingAuth},
        {"IPv4 NTPv4 Truncated MAC", AF_INET, true, TestDefinition::Kind::TruncatedMac},
        {"IPv4 NTPv4 Standard mini saturated stress test", AF_INET, true, TestDefinition::Kind::MiniStress},
        {"IPv6 Ping target time server", AF_INET6, true, TestDefinition::Kind::Ping},
        {"IPv6 NTPv3 Standard", AF_INET6, true, TestDefinition::Kind::StandardV3},
        {"IPv6 NTPv4 Standard", AF_INET6, true, TestDefinition::Kind::StandardV4},
        {"IPv6 NTPv4 Interleaved", AF_INET6, true, TestDefinition::Kind::Interleaved},
        {"IPv6 NTPv4 Symmetric-key standard", AF_INET6, true, TestDefinition::Kind::Auth},
        {"IPv6 NTPv4 Symmetric-key interleaved", AF_INET6, true, TestDefinition::Kind::AuthInterleaved},
        {"IPv6 NTPv4 Invalid symmetric-key", AF_INET6, true, TestDefinition::Kind::InvalidAuth},
        {"IPv6 NTPv4 Missing symmetric-key", AF_INET6, true, TestDefinition::Kind::MissingAuth},
        {"IPv6 NTPv4 Truncated MAC", AF_INET6, true, TestDefinition::Kind::TruncatedMac},
        {"IPv6 NTPv4 Standard mini saturated stress test", AF_INET6, true, TestDefinition::Kind::MiniStress},
    }};

    std::array<TestResult, TestCount> s_results{};
    SemaphoreHandle_t s_state_mutex;
    std::atomic<bool> s_tests_running{false};
    std::atomic<bool> s_stop_tests_requested{false};
    TargetHistoryEntry s_profile{};
    TargetHistoryStorage s_target_history{};
    esp_netif_t *s_eth_netif = nullptr;
    esp_eth_handle_t s_eth_handle = nullptr;
    std::atomic<bool> s_ethernet_connected{false};
    std::atomic<bool> s_ethernet_link_known{false};
    bool s_ipv4_was_assigned = false;
    bool s_ipv6_lla_was_assigned = false;
    bool s_ipv6_ula_was_assigned = false;
    bool s_ipv6_gua_was_assigned = false;
    bool s_startup_ready = false;
    bool s_startup_logged = false;
    char s_startup_ipv4[INET_ADDRSTRLEN] = {};
    char s_startup_ipv6_linklocal[INET6_ADDRSTRLEN] = {};
    char s_ipv6_ula[INET6_ADDRSTRLEN] = {};
    char s_ipv6_gua[INET6_ADDRSTRLEN] = {};
    char s_ipv6_in_use[INET6_ADDRSTRLEN] = {};
    led_strip_handle_t s_led_strip = nullptr;

    enum class StatusColor
    {
        Blue,
        Green,
        Yellow,
        Red
    };

    // Caller holds s_state_mutex.
    bool ipv4_available_locked() { return s_startup_ipv4[0] != '\0'; }

    // Caller holds s_state_mutex. Returns "gua", "ula", "lla" or "none".
    const char *ipv6_level_locked()
    {
        return s_ipv6_gua[0] != '\0'                 ? "gua"
               : s_ipv6_ula[0] != '\0'               ? "ula"
               : s_startup_ipv6_linklocal[0] != '\0' ? "lla"
                                                     : "none";
    }

    void set_status_led(int index, StatusColor color)
    {
        if (s_led_strip == nullptr)
            return;
        const uint32_t level = 255U * static_cast<uint32_t>(std::clamp(RGB_LED_Brightness_Percent, 0, 100)) / 100U;
        uint32_t red = 0, green = 0, blue = 0;
        switch (color)
        {
        case StatusColor::Blue:
            blue = level;
            break;
        case StatusColor::Green:
            green = level;
            break;
        case StatusColor::Yellow:
            red = level;
            green = level;
            break;
        case StatusColor::Red:
            red = level;
            break;
        }
        led_strip_set_pixel(s_led_strip, index, red, green, blue);
    }

    void init_status_leds()
    {
        if constexpr (WS2812_RGB_enabled)
        {
            led_strip_config_t strip_config = {};
            strip_config.strip_gpio_num = WS2812DataPin;
            strip_config.max_leds = 2;
            strip_config.led_model = LED_MODEL_WS2812;
            strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
            led_strip_rmt_config_t rmt_config = {};
            rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
            rmt_config.resolution_hz = 10 * 1000 * 1000;
            if (led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip) != ESP_OK)
            {
                ESP_LOGE(Tag, "Unable to initialize WS2812 RGB LEDs");
                s_led_strip = nullptr;
                return;
            }
            set_status_led(0, StatusColor::Blue);
            set_status_led(1, StatusColor::Blue);
            led_strip_refresh(s_led_strip);
        }
    }

    // Caller holds s_state_mutex.
    void update_status_leds_locked()
    {
        if constexpr (WS2812_RGB_enabled)
        {
            if (s_led_strip == nullptr)
                return;
            if (!s_startup_logged)
            {
                set_status_led(0, StatusColor::Blue);
                set_status_led(1, StatusColor::Blue);
            }
            else if (s_ethernet_link_known.load() && !s_ethernet_connected.load())
            {
                set_status_led(0, StatusColor::Red);
                set_status_led(1, StatusColor::Red);
            }
            else
            {
                set_status_led(0, ipv4_available_locked() ? StatusColor::Green : StatusColor::Red);
                const char *level = ipv6_level_locked();
                set_status_led(1, std::strcmp(level, "gua") == 0   ? StatusColor::Blue
                                  : std::strcmp(level, "ula") == 0 ? StatusColor::Green
                                  : std::strcmp(level, "lla") == 0 ? StatusColor::Yellow
                                                                   : StatusColor::Red);
            }
            led_strip_refresh(s_led_strip);
        }
    }

    using ResponseBuffer = std::unique_ptr<char, decltype(&std::free)>;

    ResponseBuffer allocate_response_buffer(size_t size)
    {
        return ResponseBuffer(static_cast<char *>(std::malloc(size)), &std::free);
    }

    void format_number_with_thousands(int value, char *output, size_t output_size)
    {
        char digits[12];
        std::snprintf(digits, sizeof(digits), "%d", value);
        const size_t digit_count = std::strlen(digits);
        const size_t comma_count = (digit_count - 1) / 3;
        if (digit_count + comma_count + 1 > output_size)
        {
            output[0] = '\0';
            return;
        }

        size_t source = digit_count;
        size_t destination = digit_count + comma_count;
        output[destination--] = '\0';
        while (source > 0)
        {
            output[destination--] = digits[--source];
            if (source > 0 && (digit_count - source) % 3 == 0)
                output[destination--] = ',';
        }
    }

    const char *status_text(TestStatus status)
    {
        switch (status)
        {
        case TestStatus::Pass:
            return "Pass";
        case TestStatus::Fail:
            return "Fail";
        case TestStatus::CouldNotRun:
            return "Could not run";
        case TestStatus::DidNotRun:
            return "Did not run";
        default:
            return "Not yet run";
        }
    }

    bool has_test_key(const symmetric_key_t &key)
    {
        return key.key_id != 0 && key.key_value[0] != '\0';
    }

    bool valid_key_value(const char *value)
    {
        if (*value == '\0')
            return true;
        if (std::strlen(value) != 32)
            return false;
        for (const char *character = value; *character != '\0'; ++character)
        {
            if (!(*character >= '0' && *character <= '9') &&
                !(*character >= 'A' && *character <= 'Z') &&
                !(*character >= 'a' && *character <= 'z'))
                return false;
        }
        return true;
    }

    bool equals_ignore_case(const char *left, const char *right)
    {
        while (*left != '\0' && *right != '\0')
        {
            if (std::tolower(static_cast<unsigned char>(*left)) != std::tolower(static_cast<unsigned char>(*right)))
                return false;
            ++left;
            ++right;
        }
        return *left == *right;
    }

    bool valid_target_name(const char *target)
    {
        const size_t length = std::strlen(target);
        if (length == 0 || length >= TargetNameSize)
            return false;
        for (const unsigned char *character = reinterpret_cast<const unsigned char *>(target); *character; ++character)
        {
            if (*character < 32 || *character > 126)
                return false;
        }
        return true;
    }

    int compare_targets(const char *left, const char *right)
    {
        return std::strcmp(left, right);
    }

    void remove_history_entry(size_t index)
    {
        if (index + 1 < s_target_history.count)
            std::memmove(&s_target_history.entries[index], &s_target_history.entries[index + 1],
                         (s_target_history.count - index - 1) * sizeof(TargetHistoryEntry));
        --s_target_history.count;
        s_target_history.entries[s_target_history.count] = {};
    }

    TargetHistoryEntry *find_history_entry(const char *target)
    {
        for (size_t index = 0; index < s_target_history.count; ++index)
        {
            if (std::strcmp(s_target_history.entries[index].target, target) == 0)
                return &s_target_history.entries[index];
        }
        return nullptr;
    }

    uint64_t timestamp_sort_value(const char *timestamp)
    {
        if (std::strlen(timestamp) < 19)
            return 0;
        uint64_t value = 0;
        constexpr size_t DigitPositions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
        for (size_t position : DigitPositions)
        {
            if (!std::isdigit(static_cast<unsigned char>(timestamp[position])))
                return 0;
            value = value * 10 + static_cast<uint64_t>(timestamp[position] - '0');
        }
        return value;
    }

    void add_or_update_history_entry(const TargetHistoryEntry &profile)
    {
        TargetHistoryEntry *entry = find_history_entry(profile.target);
        if (entry == nullptr)
        {
            if (s_target_history.count == MaxTargetHistory)
            {
                size_t oldest = 0;
                for (size_t index = 1; index < s_target_history.count; ++index)
                {
                    const auto &candidate = s_target_history.entries[index];
                    const auto &current = s_target_history.entries[oldest];
                    if (candidate.last_stored < current.last_stored ||
                        (candidate.last_stored == current.last_stored && compare_targets(candidate.target, current.target) < 0))
                        oldest = index;
                }
                remove_history_entry(oldest);
            }
            entry = &s_target_history.entries[s_target_history.count++];
        }
        *entry = profile;
        std::sort(s_target_history.entries.begin(), s_target_history.entries.begin() + s_target_history.count,
                  [](const TargetHistoryEntry &left, const TargetHistoryEntry &right)
                  { return compare_targets(left.target, right.target) < 0; });
    }

    bool is_self_target(const char *target)
    {
        char hostname_with_local[sizeof(Hostname) + 7];
        std::snprintf(hostname_with_local, sizeof(hostname_with_local), "%s.local", Hostname);
        if (equals_ignore_case(target, Hostname) || equals_ignore_case(target, hostname_with_local))
            return true;

        in_addr loopback_ipv4{};
        if (inet_pton(AF_INET, target, &loopback_ipv4) == 1 &&
            ((ntohl(loopback_ipv4.s_addr) >> 24) == 127 || loopback_ipv4.s_addr == 0))
            return true;
        in6_addr loopback_ipv6{};
        if (inet_pton(AF_INET6, target, &loopback_ipv6) == 1)
        {
            const uint8_t *bytes = loopback_ipv6.s6_addr;
            if (std::all_of(bytes, bytes + 15, [](uint8_t byte)
                            { return byte == 0; }) &&
                bytes[15] <= 1)
                return true;
            if (std::all_of(bytes, bytes + 10, [](uint8_t byte)
                            { return byte == 0; }) &&
                bytes[10] == 0xFF && bytes[11] == 0xFF)
            {
                char mapped[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, bytes + 12, mapped, sizeof(mapped));
                return is_self_target(mapped);
            }
        }
        if (s_eth_netif == nullptr)
            return false;
        in_addr target_ipv4{};
        esp_netif_ip_info_t ipv4{};
        if (inet_pton(AF_INET, target, &target_ipv4) == 1 &&
            esp_netif_get_ip_info(s_eth_netif, &ipv4) == ESP_OK && target_ipv4.s_addr == ipv4.ip.addr)
            return true;

        in6_addr target_ipv6{};
        if (inet_pton(AF_INET6, target, &target_ipv6) != 1)
            return false;
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        const int count = esp_netif_get_all_ip6(s_eth_netif, addresses);
        for (int index = 0; index < count; ++index)
        {
            if (std::memcmp(&target_ipv6, &addresses[index], sizeof(target_ipv6)) == 0)
                return true;
        }
        return false;
    }

    bool target_requires_external_warning()
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        xSemaphoreGive(s_state_mutex);
        esp_netif_ip_info_t local_ipv4{};
        const bool has_ipv4 = s_eth_netif != nullptr && esp_netif_get_ip_info(s_eth_netif, &local_ipv4) == ESP_OK;
        in_addr ipv4{};
        if (inet_pton(AF_INET, profile.ipv4, &ipv4) == 1)
        {
            const uint32_t candidate = ntohl(ipv4.s_addr);
            const bool loopback = (candidate >> 24) == 127;
            const bool link_local = (candidate >> 16) == 0xA9FE;
            const bool local_network = has_ipv4 &&
                                       ((candidate & ntohl(local_ipv4.netmask.addr)) ==
                                        (ntohl(local_ipv4.ip.addr) & ntohl(local_ipv4.netmask.addr)));
            if (!loopback && !link_local && !local_network)
                return true;
        }
        in6_addr ipv6{};
        if (inet_pton(AF_INET6, profile.ipv6, &ipv6) == 1)
        {
            const uint8_t *bytes = ipv6.s6_addr;
            const bool loopback = std::all_of(bytes, bytes + 15, [](uint8_t byte)
                                              { return byte == 0; }) &&
                                  bytes[15] == 1;
            const bool link_local = bytes[0] == 0xFE && (bytes[1] & 0xC0) == 0x80;
            const bool unique_local = (bytes[0] & 0xFE) == 0xFC;
            if (!loopback && !link_local && !unique_local)
                return true;
        }
        return false;
    }

    bool parse_mac_address(const char *text, uint8_t output[6])
    {
        if (std::strlen(text) != 17)
            return false;
        for (size_t index = 0; index < 6; ++index)
        {
            const size_t offset = index * 3;
            if ((index != 5 && text[offset + 2] != ':') ||
                !std::isxdigit(static_cast<unsigned char>(text[offset])) ||
                !std::isxdigit(static_cast<unsigned char>(text[offset + 1])))
                return false;
            const auto hex_value = [](char digit)
            {
                return std::isdigit(static_cast<unsigned char>(digit)) ? digit - '0' : std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10;
            };
            output[index] = static_cast<uint8_t>((hex_value(text[offset]) << 4) | hex_value(text[offset + 1]));
        }
        return (output[0] & 0x01U) == 0;
    }

    bool valid_address(const char *address, int family)
    {
        if (*address == '\0')
            return true;
        for (const char *character = address; *character; ++character)
        {
            const bool allowed = family == AF_INET
                                     ? ((*character >= '0' && *character <= '9') || *character == '.')
                                     : (std::isxdigit(static_cast<unsigned char>(*character)) || *character == ':');
            if (!allowed)
                return false;
        }
        in6_addr parsed{};
        return inet_pton(family, address, &parsed) == 1;
    }

    bool valid_profile(const TargetHistoryEntry &profile)
    {
        if (std::memchr(profile.target, '\0', sizeof(profile.target)) == nullptr ||
            std::memchr(profile.ipv4, '\0', sizeof(profile.ipv4)) == nullptr ||
            std::memchr(profile.ipv6, '\0', sizeof(profile.ipv6)) == nullptr ||
            std::memchr(profile.key.key_value, '\0', sizeof(profile.key.key_value)) == nullptr)
            return false;
        return valid_target_name(profile.target) && valid_key_value(profile.key.key_value) &&
               valid_address(profile.ipv4, AF_INET) && valid_address(profile.ipv6, AF_INET6);
    }

    void save_settings()
    {
        nvs_handle_t nvs;
        ESP_ERROR_CHECK(nvs_open(NvsNamespace, NVS_READWRITE, &nvs));
        ESP_ERROR_CHECK(nvs_set_str(nvs, "selected_v1", s_profile.target));
        ESP_ERROR_CHECK(nvs_set_blob(nvs, TargetHistoryNvsKey, &s_target_history, sizeof(s_target_history)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
        nvs_close(nvs);
    }

    void load_settings()
    {
        nvs_handle_t nvs;
        ESP_ERROR_CHECK(nvs_open(NvsNamespace, NVS_READWRITE, &nvs));
        s_profile = {};
        size_t history_size = sizeof(s_target_history);
        const esp_err_t history_result = nvs_get_blob(nvs, TargetHistoryNvsKey, &s_target_history, &history_size);
        bool changed = false;
        bool history_changed = false;
        if (history_result != ESP_OK || history_size != sizeof(s_target_history) ||
            s_target_history.count > MaxTargetHistory)
        {
            s_target_history = {};
            if (history_result != ESP_ERR_NVS_NOT_FOUND)
            {
                ESP_ERROR_CHECK(nvs_erase_key(nvs, TargetHistoryNvsKey));
                changed = true;
            }
        }
        for (size_t index = 0; index < s_target_history.count;)
        {
            auto &entry = s_target_history.entries[index];
            bool duplicate = false;
            if (valid_profile(entry))
            {
                for (size_t previous = 0; previous < index; ++previous)
                    duplicate = duplicate || std::strcmp(s_target_history.entries[previous].target, entry.target) == 0;
            }
            if (!valid_profile(entry) || duplicate || entry.last_stored == std::numeric_limits<uint64_t>::max())
            {
                remove_history_entry(index);
                history_changed = true;
                changed = true;
                continue;
            }
            ++index;
        }
        std::sort(s_target_history.entries.begin(), s_target_history.entries.begin() + s_target_history.count,
                  [](const TargetHistoryEntry &left, const TargetHistoryEntry &right)
                  { return compare_targets(left.target, right.target) < 0; });
        char selected[TargetNameSize]{};
        size_t selected_size = sizeof(selected);
        const esp_err_t selected_result = nvs_get_str(nvs, "selected_v1", selected, &selected_size);
        const bool selected_valid = selected_result == ESP_OK && valid_target_name(selected);
        if (selected_valid)
        {
            const auto *entry = find_history_entry(selected);
            if (entry != nullptr)
                s_profile = *entry;
        }
        if (s_profile.target[0] == '\0')
            s_profile = s_target_history.count == 0 ? TargetHistoryEntry{} : s_target_history.entries[0];
        if (selected_result != ESP_ERR_NVS_NOT_FOUND &&
            (!selected_valid || std::strcmp(selected, s_profile.target) != 0))
        {
            ESP_ERROR_CHECK(nvs_erase_key(nvs, "selected_v1"));
            changed = true;
        }
        if (s_profile.target[0] != '\0' &&
            (!selected_valid || std::strcmp(selected, s_profile.target) != 0))
        {
            ESP_ERROR_CHECK(nvs_set_str(nvs, "selected_v1", s_profile.target));
            changed = true;
        }
        for (const char *key : {"target_history", "target", "ntp_port", "key_id", "key_value", "stress_tests", "mac"})
        {
            const esp_err_t result = nvs_erase_key(nvs, key);
            if (result == ESP_OK)
                changed = true;
            else if (result != ESP_ERR_NVS_NOT_FOUND)
                ESP_ERROR_CHECK(result);
        }
        if (history_changed)
        {
            if (s_target_history.count == 0)
                ESP_ERROR_CHECK(nvs_erase_key(nvs, TargetHistoryNvsKey));
            else
                ESP_ERROR_CHECK(nvs_set_blob(nvs, TargetHistoryNvsKey, &s_target_history, sizeof(s_target_history)));
        }
        if (changed)
            ESP_ERROR_CHECK(nvs_commit(nvs));
        nvs_close(nvs);
    }

    bool stress_tests_enabled()
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const bool enabled = s_profile.stress_tests;
        xSemaphoreGive(s_state_mutex);
        return enabled;
    }

    bool family_available(int family)
    {
        if (family == AF_INET)
        {
            esp_netif_ip_info_t ip{};
            return s_eth_netif != nullptr && esp_netif_get_ip_info(s_eth_netif, &ip) == ESP_OK && ip.ip.addr != 0;
        }
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        return s_eth_netif != nullptr && esp_netif_get_all_preferred_ip6(s_eth_netif, addresses) > 0;
    }

    bool test_available(const TestDefinition &test, const TargetHistoryEntry &profile)
    {
        const char *address = test.family == AF_INET ? profile.ipv4 : profile.ipv6;
        const uint16_t port = test.family == AF_INET ? profile.ipv4_ntp_port : profile.ipv6_ntp_port;
        if (!test.enabled || !valid_profile(profile) || address[0] == '\0' ||
            is_self_target(address) || !family_available(test.family))
            return false;
        if (test.kind == TestDefinition::Kind::Ping)
            return true;
        if (port == 0)
            return false;
        if (test.kind == TestDefinition::Kind::MiniStress)
            return profile.stress_tests;
        if (test.kind == TestDefinition::Kind::Auth || test.kind == TestDefinition::Kind::AuthInterleaved ||
            test.kind == TestDefinition::Kind::InvalidAuth || test.kind == TestDefinition::Kind::MissingAuth ||
            test.kind == TestDefinition::Kind::TruncatedMac)
            return has_test_key(profile.key);
        return true;
    }

    void format_ntp_timestamp(const uint8_t *packet, char *output, size_t output_size)
    {
        uint32_t seconds_network;
        uint32_t fraction_network;
        std::memcpy(&seconds_network, packet + 40, sizeof(seconds_network));
        std::memcpy(&fraction_network, packet + 44, sizeof(fraction_network));
        const uint64_t seconds = ntohl(seconds_network);
        if (seconds < NtpEpochOffset)
        {
            std::snprintf(output, output_size, "n/a");
            return;
        }
        const time_t unix_seconds = static_cast<time_t>(seconds - NtpEpochOffset);
        const tm *utc = gmtime(&unix_seconds);
        if (utc == nullptr)
        {
            std::snprintf(output, output_size, "n/a");
            return;
        }
        const uint64_t nanoseconds = (static_cast<uint64_t>(ntohl(fraction_network)) * 1000000000ULL) >> 32;
        std::snprintf(output, output_size, "%04d-%02d-%02d %02d:%02d:%02d.%09llu (UTC)",
                      utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday, utc->tm_hour,
                      utc->tm_min, utc->tm_sec, static_cast<unsigned long long>(nanoseconds));
    }

    struct NtpExchange
    {
        uint64_t t1;
        uint64_t t2;
        uint64_t t3;
        uint64_t t4;
    };

    uint64_t ntp_timestamp_now()
    {
        const uint64_t microseconds = esp_timer_get_time();
        return ((NtpEpochOffset + microseconds / 1000000ULL) << 32) |
               ((microseconds % 1000000ULL) << 32) / 1000000ULL;
    }

    void write_ntp_timestamp(uint8_t *packet, size_t offset, uint64_t timestamp)
    {
        for (size_t index = 0; index < 8; ++index)
            packet[offset + index] = static_cast<uint8_t>(timestamp >> (56 - index * 8));
    }

    uint64_t read_ntp_timestamp(const uint8_t *packet, size_t offset)
    {
        uint64_t timestamp = 0;
        for (size_t index = 0; index < 8; ++index)
            timestamp = (timestamp << 8) | packet[offset + index];
        return timestamp;
    }

    uint32_t read_u32_be(const uint8_t *bytes)
    {
        return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
    }

    bool target_destination(int family, sockaddr_storage *destination, socklen_t *destination_length, bool ntp)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        xSemaphoreGive(s_state_mutex);
        const char *target = family == AF_INET ? profile.ipv4 : profile.ipv6;
        const uint16_t port = family == AF_INET ? profile.ipv4_ntp_port : profile.ipv6_ntp_port;
        if (target[0] == '\0' || is_self_target(target) || (ntp && port == 0))
            return false;
        *destination = {};
        if (family == AF_INET)
        {
            auto *address = reinterpret_cast<sockaddr_in *>(destination);
            address->sin_family = AF_INET;
            address->sin_port = htons(port);
            if (inet_pton(AF_INET, target, &address->sin_addr) != 1)
                return false;
            *destination_length = sizeof(*address);
        }
        else if (family == AF_INET6)
        {
            auto *address = reinterpret_cast<sockaddr_in6 *>(destination);
            address->sin6_family = AF_INET6;
            address->sin6_port = htons(port);
            if (inet_pton(AF_INET6, target, &address->sin6_addr) != 1)
                return false;
            if (address->sin6_addr.s6_addr[0] == 0xFE && (address->sin6_addr.s6_addr[1] & 0xC0) == 0x80)
                address->sin6_scope_id = esp_netif_get_netif_impl_index(s_eth_netif);
            *destination_length = sizeof(*address);
        }
        else
            return false;
        return true;
    }

    bool open_ntp_socket(int family, int *socket_fd, sockaddr_storage *destination, socklen_t *destination_length)
    {
        if (!target_destination(family, destination, destination_length, true))
            return false;
        *socket_fd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
        if (*socket_fd < 0)
            return false;
        timeval timeout{3, 0};
        if (setsockopt(*socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
        {
            close(*socket_fd);
            *socket_fd = -1;
            return false;
        }
        return true;
    }

    bool ntp_standard_exchange(int socket_fd, const sockaddr *destination, socklen_t destination_length,
                               uint8_t version, NtpExchange *exchange)
    {
        std::array<uint8_t, NtpPacketSize> request{};
        request[0] = static_cast<uint8_t>((version << 3) | 3);
        exchange->t1 = ntp_timestamp_now();
        write_ntp_timestamp(request.data(), 40, exchange->t1);
        const bool sent = sendto(socket_fd, request.data(), request.size(), 0, destination, destination_length) ==
                          static_cast<ssize_t>(request.size());
        std::array<uint8_t, 128> response{};
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        exchange->t4 = ntp_timestamp_now();
        if (received < NtpPacketSize || (response[0] & 0x07) != 4)
            return false;
        exchange->t2 = read_ntp_timestamp(response.data(), 32);
        exchange->t3 = read_ntp_timestamp(response.data(), 40);
        return true;
    }

    bool ntp_exchange(int family, uint8_t version, char *timestamp, size_t timestamp_size)
    {
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return false;
        NtpExchange exchange{};
        const bool succeeded = ntp_standard_exchange(socket_fd, reinterpret_cast<const sockaddr *>(&destination),
                                                     destination_length, version, &exchange);
        close(socket_fd);
        if (!succeeded)
            return false;
        uint8_t response[48]{};
        write_ntp_timestamp(response, 40, exchange.t3);
        if (timestamp != nullptr)
            format_ntp_timestamp(response, timestamp, timestamp_size);
        return true;
    }

    bool ntp_interleaved_exchange(int family, char *timestamp, size_t timestamp_size)
    {
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return false;

        const sockaddr *address = reinterpret_cast<const sockaddr *>(&destination);
        NtpExchange previous{};
        if (!ntp_standard_exchange(socket_fd, address, destination_length, 4, &previous))
        {
            close(socket_fd);
            return false;
        }

        std::array<uint8_t, NtpPacketSize> request{};
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        write_ntp_timestamp(request.data(), 24, previous.t2);
        write_ntp_timestamp(request.data(), 32, previous.t4);
        write_ntp_timestamp(request.data(), 40, previous.t1);
        std::array<uint8_t, 128> response{};
        const bool sent = sendto(socket_fd, request.data(), request.size(), 0, address, destination_length) ==
                          static_cast<ssize_t>(request.size());
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        close(socket_fd);
        if (received != NtpPacketSize || ((response[0] >> 3) & 0x07) != 4 || (response[0] & 0x07) != 4 ||
            read_ntp_timestamp(response.data(), 24) != previous.t4)
            return false;
        const uint64_t reply_t3 = read_ntp_timestamp(response.data(), 40);
        const uint64_t difference = reply_t3 >= previous.t3 ? reply_t3 - previous.t3 : previous.t3 - reply_t3;
        if (difference > ((1ULL << 32) * 5ULL / 1000ULL))
            return false;
        format_ntp_timestamp(response.data(), timestamp, timestamp_size);
        return true;
    }

    void write_ntp_key_id(uint8_t *packet, uint16_t key_id)
    {
        packet[NtpPacketSize] = 0;
        packet[NtpPacketSize + 1] = 0;
        packet[NtpPacketSize + 2] = static_cast<uint8_t>(key_id >> 8);
        packet[NtpPacketSize + 3] = static_cast<uint8_t>(key_id);
    }

    bool append_ntp_authentication(uint8_t *packet, size_t capacity, size_t *packet_length, const symmetric_key_t &key)
    {
        if (!has_test_key(key) || *packet_length != NtpPacketSize || capacity < NtpPacketSize + NtpAuthTrailerSize)
            return false;
        write_ntp_key_id(packet, key.key_id);
        uint8_t digest[32]{};
        psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
        size_t digest_length = 0;
        const bool succeeded = psa_hash_setup(&operation, PSA_ALG_SHA_256) == PSA_SUCCESS &&
                               psa_hash_update(&operation, reinterpret_cast<const uint8_t *>(key.key_value),
                                               std::strlen(key.key_value)) == PSA_SUCCESS &&
                               psa_hash_update(&operation, packet, NtpPacketSize) == PSA_SUCCESS &&
                               psa_hash_finish(&operation, digest, sizeof(digest), &digest_length) == PSA_SUCCESS;
        psa_hash_abort(&operation);
        if (!succeeded || digest_length != 32)
            return false;
        std::memcpy(packet + NtpPacketSize + 4, digest, 20);
        *packet_length = NtpPacketSize + NtpAuthTrailerSize;
        return true;
    }

    bool verify_ntp_authentication(const uint8_t *packet, size_t packet_length, const symmetric_key_t &key)
    {
        if (packet_length != NtpPacketSize + NtpAuthTrailerSize ||
            read_u32_be(packet + NtpPacketSize) != key.key_id)
            return false;
        uint8_t expected_digest[32]{};
        psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
        size_t digest_length = 0;
        const bool succeeded = psa_hash_setup(&operation, PSA_ALG_SHA_256) == PSA_SUCCESS &&
                               psa_hash_update(&operation, reinterpret_cast<const uint8_t *>(key.key_value),
                                               std::strlen(key.key_value)) == PSA_SUCCESS &&
                               psa_hash_update(&operation, packet, NtpPacketSize) == PSA_SUCCESS &&
                               psa_hash_finish(&operation, expected_digest, sizeof(expected_digest), &digest_length) == PSA_SUCCESS;
        psa_hash_abort(&operation);
        return succeeded && digest_length == sizeof(expected_digest) &&
               std::memcmp(expected_digest, packet + NtpPacketSize + 4, 20) == 0;
    }

    bool ntp_authenticated_standard_exchange(int socket_fd, const sockaddr *destination, socklen_t destination_length,
                                             NtpExchange *exchange, const symmetric_key_t &key)
    {
        std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
        size_t request_length = NtpPacketSize;
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        exchange->t1 = ntp_timestamp_now();
        write_ntp_timestamp(request.data(), 40, exchange->t1);
        if (!append_ntp_authentication(request.data(), request.size(), &request_length, key))
            return false;
        const bool sent = sendto(socket_fd, request.data(), request_length, 0, destination, destination_length) ==
                          static_cast<ssize_t>(request_length);
        std::array<uint8_t, 128> response{};
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        exchange->t4 = ntp_timestamp_now();
        if (!verify_ntp_authentication(response.data(), received, key) || ((response[0] >> 3) & 0x07) != 4 ||
            (response[0] & 0x07) != 4 || read_ntp_timestamp(response.data(), 24) != exchange->t1)
            return false;
        exchange->t2 = read_ntp_timestamp(response.data(), 32);
        exchange->t3 = read_ntp_timestamp(response.data(), 40);
        return exchange->t2 != 0 && exchange->t3 != 0;
    }

    bool ntp_authenticated_exchange(int family, bool interleaved, char *timestamp, size_t timestamp_size,
                                    const symmetric_key_t &key)
    {
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return false;
        const sockaddr *address = reinterpret_cast<const sockaddr *>(&destination);
        NtpExchange previous{};
        if (!ntp_authenticated_standard_exchange(socket_fd, address, destination_length, &previous, key))
        {
            close(socket_fd);
            return false;
        }
        if (!interleaved)
        {
            close(socket_fd);
            uint8_t response[NtpPacketSize]{};
            write_ntp_timestamp(response, 40, previous.t3);
            format_ntp_timestamp(response, timestamp, timestamp_size);
            return true;
        }

        std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
        size_t request_length = NtpPacketSize;
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        write_ntp_timestamp(request.data(), 24, previous.t2);
        write_ntp_timestamp(request.data(), 32, previous.t4);
        write_ntp_timestamp(request.data(), 40, previous.t1);
        const bool authenticated = append_ntp_authentication(request.data(), request.size(), &request_length, key);
        std::array<uint8_t, 128> response{};
        const bool sent = authenticated && sendto(socket_fd, request.data(), request_length, 0, address, destination_length) ==
                                               static_cast<ssize_t>(request_length);
        const ssize_t received = sent ? recvfrom(socket_fd, response.data(), response.size(), 0, nullptr, nullptr) : -1;
        close(socket_fd);
        if (!verify_ntp_authentication(response.data(), received, key) || read_ntp_timestamp(response.data(), 24) != previous.t4)
            return false;
        const uint64_t reply_t3 = read_ntp_timestamp(response.data(), 40);
        const uint64_t difference = reply_t3 >= previous.t3 ? reply_t3 - previous.t3 : previous.t3 - reply_t3;
        if (difference > ((1ULL << 32) * 5ULL / 1000ULL))
            return false;
        format_ntp_timestamp(response.data(), timestamp, timestamp_size);
        return true;
    }

    TestStatus ntp_authentication_rejection(int family, TestDefinition::Kind kind, const symmetric_key_t &key,
                                            char *timestamp, size_t timestamp_size)
    {
        if (key.key_id == 0 || (kind != TestDefinition::Kind::MissingAuth && !has_test_key(key)))
            return TestStatus::CouldNotRun;
        int socket_fd = -1;
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!open_ntp_socket(family, &socket_fd, &destination, &destination_length))
            return TestStatus::CouldNotRun;
        std::array<uint8_t, NtpPacketSize + NtpAuthTrailerSize> request{};
        size_t request_length = NtpPacketSize;
        request[0] = static_cast<uint8_t>((4U << 3) | 3U);
        const uint64_t request_timestamp = ntp_timestamp_now();
        write_ntp_timestamp(request.data(), 40, request_timestamp);
        if (kind == TestDefinition::Kind::MissingAuth)
        {
            write_ntp_key_id(request.data(), key.key_id);
            request_length += sizeof(uint32_t);
        }
        else
        {
            if (!append_ntp_authentication(request.data(), request.size(), &request_length, key))
            {
                close(socket_fd);
                return TestStatus::CouldNotRun;
            }
            if (kind == TestDefinition::Kind::InvalidAuth)
                request[request_length - 1] ^= 0x01;
            else
                --request_length;
        }
        if (connect(socket_fd, reinterpret_cast<const sockaddr *>(&destination), destination_length) != 0 ||
            send(socket_fd, request.data(), request_length, 0) != static_cast<ssize_t>(request_length))
        {
            close(socket_fd);
            return TestStatus::CouldNotRun;
        }
        uint8_t response[128]{};
        const ssize_t received = recv(socket_fd, response, sizeof(response), 0);
        const int receive_error = errno;
        close(socket_fd);
        if (received < 0)
            return receive_error == EAGAIN || receive_error == EWOULDBLOCK || receive_error == ETIMEDOUT
                       ? TestStatus::Pass
                       : TestStatus::CouldNotRun;
        if (received < static_cast<ssize_t>(NtpPacketSize) ||
            ((response[0] >> 3) & 0x07) != 4 || (response[0] & 0x07) != 4 ||
            read_ntp_timestamp(response, 24) != request_timestamp)
            return TestStatus::CouldNotRun;
        if (received == static_cast<ssize_t>(NtpPacketSize + sizeof(uint32_t)) &&
            read_u32_be(response + NtpPacketSize) == 0)
            return TestStatus::Pass;
        if (response[1] == 0)
        {
            const uint8_t *code = response + 12;
            if (std::memcmp(code, "AUTH", 4) == 0 || std::memcmp(code, "CRYP", 4) == 0 ||
                std::memcmp(code, "NKEY", 4) == 0 || std::memcmp(code, "DENY", 4) == 0 ||
                std::memcmp(code, "RSTR", 4) == 0)
                return TestStatus::Pass;
            return TestStatus::CouldNotRun;
        }
        if (response[1] > 15 || read_ntp_timestamp(response, 40) == 0)
            return TestStatus::CouldNotRun;
        format_ntp_timestamp(response, timestamp, timestamp_size);
        return TestStatus::Fail;
    }

    struct PingContext
    {
        SemaphoreHandle_t completed;
        bool succeeded;
    };

    void ping_end_callback(esp_ping_handle_t handle, void *arguments)
    {
        auto *context = static_cast<PingContext *>(arguments);
        uint32_t replies = 0;
        esp_ping_get_profile(handle, ESP_PING_PROF_REPLY, &replies, sizeof(replies));
        context->succeeded = replies > 0;
        xSemaphoreGive(context->completed);
    }

    bool ping_target(int family)
    {
        sockaddr_storage destination{};
        socklen_t destination_length = 0;
        if (!target_destination(family, &destination, &destination_length, false))
            return false;

        esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
        config.count = 3;
        config.interval_ms = 250;
        config.timeout_ms = 1500;
        config.interface = static_cast<uint32_t>(esp_netif_get_netif_impl_index(s_eth_netif));
        if (destination.ss_family == AF_INET)
        {
            const auto *address = reinterpret_cast<const sockaddr_in *>(&destination);
            inet_addr_to_ip4addr(ip_2_ip4(&config.target_addr), &address->sin_addr);
            IP_SET_TYPE_VAL(config.target_addr, IPADDR_TYPE_V4);
        }
        else if (destination.ss_family == AF_INET6)
        {
            const auto *address = reinterpret_cast<const sockaddr_in6 *>(&destination);
            inet6_addr_to_ip6addr(ip_2_ip6(&config.target_addr), &address->sin6_addr);
            IP_SET_TYPE_VAL(config.target_addr, IPADDR_TYPE_V6);
        }
        else
            return false;

        PingContext context{xSemaphoreCreateBinary(), false};
        if (context.completed == nullptr)
            return false;
        const esp_ping_callbacks_t callbacks = {
            .cb_args = &context,
            .on_ping_success = nullptr,
            .on_ping_timeout = nullptr,
            .on_ping_end = ping_end_callback,
        };
        esp_ping_handle_t handle = nullptr;
        if (esp_ping_new_session(&config, &callbacks, &handle) != ESP_OK || esp_ping_start(handle) != ESP_OK)
        {
            if (handle != nullptr)
                esp_ping_delete_session(handle);
            vSemaphoreDelete(context.completed);
            return false;
        }
        bool completed = false;
        for (int elapsed = 0; elapsed < 6000 && !s_stop_tests_requested.load(); elapsed += 100)
        {
            if (xSemaphoreTake(context.completed, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                completed = true;
                break;
            }
        }
        if (!completed)
            esp_ping_stop(handle);
        esp_ping_delete_session(handle);
        vSemaphoreDelete(context.completed);
        return completed && context.succeeded;
    }

    TestStatus run_test(size_t index, char *timestamp, size_t timestamp_size)
    {
        const TestDefinition &test = Tests[index];
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        xSemaphoreGive(s_state_mutex);
        if (!test_available(test, profile))
            return test.kind == TestDefinition::Kind::MiniStress && !profile.stress_tests
                       ? TestStatus::DidNotRun
                       : TestStatus::CouldNotRun;
        if (test.kind == TestDefinition::Kind::Ping)
            return ping_target(test.family) ? TestStatus::Pass : TestStatus::Fail;
        const symmetric_key_t &key = profile.key;
        if (test.kind == TestDefinition::Kind::Auth || test.kind == TestDefinition::Kind::AuthInterleaved)
            return ntp_authenticated_exchange(test.family, test.kind == TestDefinition::Kind::AuthInterleaved,
                                              timestamp, timestamp_size, key)
                       ? TestStatus::Pass
                       : TestStatus::Fail;
        if (test.kind == TestDefinition::Kind::InvalidAuth || test.kind == TestDefinition::Kind::TruncatedMac ||
            test.kind == TestDefinition::Kind::MissingAuth)
            return ntp_authentication_rejection(test.family, test.kind, key, timestamp, timestamp_size);
        const uint8_t version = test.kind == TestDefinition::Kind::StandardV3 ? 3 : 4;
        if (test.kind == TestDefinition::Kind::MiniStress)
        {
            for (int request = 0; request < number_of_NTP_requests_in_a_mini_stress_test; ++request)
            {
                if (s_stop_tests_requested.load())
                    return TestStatus::CouldNotRun;
                if (!stress_tests_enabled())
                    return TestStatus::DidNotRun;
                if (!ntp_exchange(test.family, version, timestamp, timestamp_size))
                    return TestStatus::Fail;
            }
            return TestStatus::Pass;
        }
        if (test.kind == TestDefinition::Kind::Interleaved)
            return ntp_interleaved_exchange(test.family, timestamp, timestamp_size) ? TestStatus::Pass : TestStatus::Fail;
        return ntp_exchange(test.family, version, timestamp, timestamp_size) ? TestStatus::Pass : TestStatus::Fail;
    }

    struct TestRequest
    {
        enum class Scope : uint8_t
        {
            All,
            Ntp,
            NtpIpv4,
            NtpIpv6,
            Individual
        } scope;
        int index;
    };

    bool test_selected(const TestRequest &request, size_t index)
    {
        const auto &test = Tests[index];
        const bool ntp = test.kind != TestDefinition::Kind::Ping;
        return request.scope == TestRequest::Scope::All ||
               (request.scope == TestRequest::Scope::Ntp && ntp) ||
               (request.scope == TestRequest::Scope::NtpIpv4 && ntp && test.family == AF_INET) ||
               (request.scope == TestRequest::Scope::NtpIpv6 && ntp && test.family == AF_INET6) ||
               (request.scope == TestRequest::Scope::Individual && index == static_cast<size_t>(request.index));
    }

    void test_task(void *parameter)
    {
        const TestRequest request = *static_cast<TestRequest *>(parameter);
        delete static_cast<TestRequest *>(parameter);
        for (size_t index = 0; index < Tests.size(); ++index)
        {
            if (s_stop_tests_requested.load())
                break;
            if (!test_selected(request, index))
                continue;
            char timestamp[sizeof(TestResult::timestamp)] = "n/a";
            const TestStatus status = run_test(index, timestamp, sizeof(timestamp));
            if (s_stop_tests_requested.load())
                break;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_results[index].status = status;
            std::snprintf(s_results[index].timestamp, sizeof(s_results[index].timestamp), "%s", timestamp);
            xSemaphoreGive(s_state_mutex);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        s_tests_running.store(false);
        vTaskDelete(nullptr);
    }

    bool start_tests(TestRequest::Scope scope, int index = -1)
    {
        const TestRequest selection{scope, index};
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        bool available = false;
        for (size_t test_index = 0; test_index < Tests.size(); ++test_index)
            available = available || (test_selected(selection, test_index) && test_available(Tests[test_index], s_profile));
        bool expected = false;
        const bool started = available && s_tests_running.compare_exchange_strong(expected, true);
        if (started)
            s_stop_tests_requested.store(false);
        xSemaphoreGive(s_state_mutex);
        if (!started)
            return false;
        auto *request = new (std::nothrow) TestRequest{scope, index};
        if (request == nullptr)
        {
            s_tests_running.store(false);
            return false;
        }
        if (xTaskCreate(test_task, "ntp_tests", 6144, request, 5, nullptr) != pdPASS)
        {
            delete request;
            s_tests_running.store(false);
            return false;
        }
        return true;
    }

    void html_escape(const char *source, char *destination, size_t destination_size)
    {
        size_t cursor = 0;
        for (; *source && cursor + 6 < destination_size; ++source)
        {
            const char *replacement = nullptr;
            switch (*source)
            {
            case '&':
                replacement = "&amp;";
                break;
            case '<':
                replacement = "&lt;";
                break;
            case '>':
                replacement = "&gt;";
                break;
            case '"':
                replacement = "&quot;";
                break;
            default:
                destination[cursor++] = *source;
                continue;
            }
            const size_t replacement_size = std::strlen(replacement);
            std::memcpy(destination + cursor, replacement, replacement_size);
            cursor += replacement_size;
        }
        destination[cursor] = '\0';
    }

    void markdown_escape(const char *source, char *destination, size_t destination_size)
    {
        char escaped[TargetNameSize * 6];
        html_escape(source, escaped, sizeof(escaped));
        size_t cursor = 0;
        for (const char *character = escaped; *character && cursor + 2 < destination_size; ++character)
        {
            if (std::strchr("\\`*_{}[]()#+-.!|", *character) != nullptr)
                destination[cursor++] = '\\';
            destination[cursor++] = *character;
        }
        destination[cursor] = '\0';
    }

    void format_optional_u16(uint16_t value, char output[6])
    {
        output[0] = '\0';
        if (value != 0)
            std::snprintf(output, 6, "%u", value);
    }

    struct NetworkInfo
    {
        char mac[18] = "Not assigned";
        char ipv4[INET_ADDRSTRLEN] = "Not assigned";
        char lla[INET6_ADDRSTRLEN] = "Not assigned";
        char ula[INET6_ADDRSTRLEN] = "Not assigned";
        char gua[INET6_ADDRSTRLEN] = "Not assigned";
    };

    NetworkInfo read_network_info()
    {
        NetworkInfo info;
        if (s_eth_netif == nullptr)
            return info;
        uint8_t mac[6]{};
        if (esp_netif_get_mac(s_eth_netif, mac) == ESP_OK)
            std::snprintf(info.mac, sizeof(info.mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        esp_netif_ip_info_t ip{};
        if (esp_netif_get_ip_info(s_eth_netif, &ip) == ESP_OK && ip.ip.addr != 0)
            std::snprintf(info.ipv4, sizeof(info.ipv4), IPSTR, IP2STR(&ip.ip));
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        const int count = esp_netif_get_all_ip6(s_eth_netif, addresses);
        for (int index = 0; index < count; ++index)
        {
            char *destination = nullptr;
            switch (esp_netif_ip6_get_addr_type(&addresses[index]))
            {
            case ESP_IP6_ADDR_IS_LINK_LOCAL:
                destination = info.lla;
                break;
            case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
                destination = info.ula;
                break;
            case ESP_IP6_ADDR_IS_GLOBAL:
                destination = info.gua;
                break;
            default:
                break;
            }
            if (destination != nullptr)
                inet6_ntoa_r(addresses[index], destination, INET6_ADDRSTRLEN);
        }
        return info;
    }

    esp_err_t root_handler(httpd_req_t *request)
    {
        char target[TargetNameSize * 6];
        char stress_test_request_count[16];
        format_number_with_thousands(number_of_NTP_requests_in_a_mini_stress_test,
                                     stress_test_request_count, sizeof(stress_test_request_count));
        ResponseBuffer target_options = allocate_response_buffer(TargetOptionsBufferSize);
        if (!target_options)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate target history");
        target_options.get()[0] = '\0';
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const TargetHistoryEntry profile = s_profile;
        html_escape(profile.target, target, sizeof(target));
        size_t options_cursor = 0;
        for (size_t index = 0; index < s_target_history.count; ++index)
        {
            const auto &entry = s_target_history.entries[index];
            char escaped_target[TargetNameSize * 6];
            html_escape(entry.target, escaped_target, sizeof(escaped_target));
            char ipv4_port[6], ipv6_port[6], key_id[6];
            format_optional_u16(entry.ipv4_ntp_port, ipv4_port);
            format_optional_u16(entry.ipv6_ntp_port, ipv6_port);
            format_optional_u16(entry.key.key_id, key_id);
            const int written = std::snprintf(target_options.get() + options_cursor, TargetOptionsBufferSize - options_cursor,
                                              "<button class=\"target-option\" type=\"button\" role=\"option\" data-ipv4=\"%s\" data-ipv6=\"%s\" "
                                              "data-ipv4-port=\"%s\" data-ipv6-port=\"%s\" data-key-id=\"%s\" data-key-value=\"%s\" data-stress=\"%s\">%s</button>",
                                              entry.ipv4, entry.ipv6, ipv4_port, ipv6_port, key_id, entry.key.key_value,
                                              entry.stress_tests ? "1" : "0", escaped_target);
            if (written < 0 || static_cast<size_t>(written) >= TargetOptionsBufferSize - options_cursor)
            {
                xSemaphoreGive(s_state_mutex);
                return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Target history generation failed");
            }
            options_cursor += static_cast<size_t>(written);
        }
        xSemaphoreGive(s_state_mutex);
        char ipv4_port[6], ipv6_port[6], key_id[6];
        format_optional_u16(profile.ipv4_ntp_port, ipv4_port);
        format_optional_u16(profile.ipv6_ntp_port, ipv6_port);
        format_optional_u16(profile.key.key_id, key_id);
        NetworkInfo network = read_network_info();
        const char *mac_address = network.mac;
        const char *ipv4_address = network.ipv4;
        const char *ipv6_lla = network.lla;
        const char *ipv6_ula = network.ula;
        const char *ipv6_gua = network.gua;
        ResponseBuffer page = allocate_response_buffer(PageBufferSize);
        if (!page)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate page");
        const int length = std::snprintf(page.get(), PageBufferSize,
                                         R"HTML(<!doctype html><html><head><meta charset="utf-8"><title>ESP32 Time Server Tester</title>
<style>body{font-family:Arial,sans-serif;margin:20px}h1{color:#0665b5;font-size:1.5em}hr{border:0;border-top:1px solid #999}label{margin:.5em 0}input{width:22em}input[type="checkbox"]{width:auto}input[type="number"]{height:1.5em;width:7em}#key-value,.key-visibility,#target-toggle{box-sizing:border-box;height:2em;vertical-align:middle}.key-visibility,#target-toggle{width:2em;padding:0;display:inline-flex;align-items:center;justify-content:center}.key-visibility svg,#target-toggle svg{width:1.35em;height:1.35em}button{margin:.25em;padding:.4em .7em}table{border-collapse:collapse;width:100%%;max-width:1000px;margin-top:.5em}th,td{border:1px solid #bbb;padding:.4em;text-align:left}.disabled{color:#777}.result{color:#000}.result.pass{color:#008000}.result.fail{color:#c00}footer{margin-top:1em}.target-combobox{display:inline-block;position:relative}.target-combobox input{width:22em}.target-combobox button{margin-left:.15em}.target-combobox>span{position:absolute;z-index:1;left:0;top:100%%;width:calc(100%% - 2.7em);max-height:12em;overflow-y:auto;background:#fff;border:1px solid #999}.target-option{display:block;width:100%%;margin:0;border:0;border-radius:0;text-align:left;background:#fff}.target-option:hover,.target-option.selected{background:#d9ecff}.settings-grid{display:grid;grid-template-columns:minmax(0,34em) minmax(0,34em);gap:.8em 1.5em;max-width:72em;margin:1em 0}.settings-grid label{display:flex;align-items:center;gap:.5em;margin:0}.settings-grid label>span:first-child{min-width:10em}.settings-grid input:not([type="checkbox"]){min-width:0;flex:1}.settings-grid input[type="number"]{max-width:8em}.key-entry{display:flex;align-items:center;flex:1;min-width:0}.key-entry input{width:100%%}@media(max-width:780px){.settings-grid{grid-template-columns:1fr}}h1{display:flex;align-items:center;justify-content:space-between}.ip-leds{display:inline-flex;gap:.5em}.ip-led{width:1em;height:1em;border-radius:50%%;background:#0000ff;border:1px solid #555;display:inline-block}</style>
</head><body><h1>ESP32TimeServerTester<span class="ip-leds"><span id="led-ipv4" class="ip-led" title="IPv4"></span><span id="led-ipv6" class="ip-led" title="IPv6"></span></span></h1><hr><h2>Settings</h2>
<form id="settings-form" method="post" action="/settings">
<input type="hidden" name="client_time" id="client-time">
<div class="row">
<label>Target time server: <span class="target-combobox"><input id="target" type="text" role="combobox" aria-controls="target-menu" aria-expanded="false" name="target" minlength="1" maxlength="40" value="%s" placeholder="Target name" autocomplete="off" required><button id="target-toggle" type="button" aria-label="Show saved target profiles"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="m6 9 6 6 6-6"/></svg></button><span id="target-menu" role="listbox" hidden>%s</span></span></label>
</div>
<div class="settings-grid">
<label><span>IPv4 address:</span><input id="ipv4" name="ipv4" maxlength="15" pattern="[0-9.]*" title="Enter an IPv4 address containing only digits and periods" value="%s" placeholder="Optional IPv4 address" inputmode="decimal" autocomplete="off"></label>
<label><span>IPv4 NTP Port:</span><input id="ipv4-ntp-port" name="ipv4_ntp_port" type="number" min="1" max="65535" step="1" value="%s"></label>
<label><span>IPv6 address:</span><input id="ipv6" name="ipv6" maxlength="45" pattern="[0-9A-Fa-f:]*" title="Enter an IPv6 address containing only hexadecimal digits and colons" value="%s" placeholder="Optional IPv6 address" autocomplete="off"></label>
<label><span>IPv6 NTP Port:</span><input id="ipv6-ntp-port" name="ipv6_ntp_port" type="number" min="1" max="65535" step="1" value="%s"></label>
<label><span>NTPv4 symmetric-key id:</span><input id="key-id" name="key_id" type="number" min="1" max="65535" step="1" value="%s"></label>
<label><span>NTPv4 symmetric-key value:</span><span class="key-entry"><input id="key-value" name="key_value" type="password" maxlength="32" pattern="[A-Za-z0-9]{32}" value="%s" autocomplete="off" spellcheck="false"><button class="key-visibility" type="button" id="toggle-key" aria-label="Show symmetric-key value" aria-pressed="false"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M2 12s4-7 10-7 10 7 10 7-4 7S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/><path id="key-hidden" d="M3 21 21 3"/></svg></button></span></label>
</div>
<div class="row"><label><input type="checkbox" id="stress-tests" name="stress_tests" value="1" %s> Enable IPv4 and IPv6 NTPv4 Standard mini saturated stress tests ( %s NTP requests each )</label></div>
<br>
<div class="row">
<button id="save-settings" type="submit" disabled>Save changes</button>
<button id="cancel-settings" type="button" disabled>Cancel changes</button>
<button id="clone-target" type="button" disabled>Clone target</button>
<button id="rename-target" type="button" disabled>Rename target</button>
<button id="delete-target" type="button" disabled>Delete target</button>
</div>
</form>
<dialog id="rename-dialog"><form id="rename-form"><label>Target time server: <input id="new-target-name" type="text" minlength="1" maxlength="40" required></label><div><button type="submit">Change</button><button type="button" id="cancel-rename">Cancel</button></div></form></dialog>
<hr><h2>Tests</h2><button class="test-run" onclick="run('all')" disabled>Run all tests</button><button class="test-run" onclick="run('ntp')" disabled>Run all NTP tests</button>
<button class="test-run" onclick="run('ipv4')" disabled>Run all NTP tests over IPv4</button><button class="test-run" onclick="run('ipv6')" disabled>Run all NTP tests over IPv6</button>
<button id="stop-tests" onclick="stopTests()" disabled>Stop tests</button><br>
<button id="clear-results" onclick="clearResults()" disabled>Clear all test results</button>
<table><thead><tr><th>Test</th><th>Result</th><th>Reported date and time</th></tr></thead><tbody id="tests"></tbody></table>
<hr><h2>Download</h2><a href="/report" id="download-link"><button id="download-button" disabled>Download report</button></a><hr>
<pre>%s v%s

MAC address:      <span id="network-mac">%s</span>
IPv4 address:     <span id="network-ipv4">%s</span>
IPv6 LLA address: <span id="network-lla">%s</span>
IPv6 ULA address: <span id="network-ula">%s</span>
IPv6 GUA address: <span id="network-gua">%s</span></pre><hr>
<footer><br>Copyright Rob Latour, 2026<br>License: MIT<br>Website: <a href="https://github.com/roblatour/ESP32TimeServerTester" target="_blank">https://github.com/roblatour/ESP32TimeServerTester</a></footer>
<script>
const keyField=document.getElementById('key-value'),toggleKey=document.getElementById('toggle-key');
toggleKey.addEventListener('click',()=>{const visible=keyField.type==='password';keyField.type=visible?'text':'password';toggleKey.setAttribute('aria-pressed',String(visible));toggleKey.setAttribute('aria-label',visible?'Hide symmetric-key value':'Show symmetric-key value');document.getElementById('key-hidden').style.display=visible?'none':''});
const settingsForm=document.getElementById('settings-form'),settingsInputs=Array.from(settingsForm.querySelectorAll('input:not([type="hidden"])')),targetField=document.getElementById('target');
const targetMenu=document.getElementById('target-menu'),targetToggle=document.getElementById('target-toggle');
const profileFields={ipv4:document.getElementById('ipv4'),ipv6:document.getElementById('ipv6'),ipv4Port:document.getElementById('ipv4-ntp-port'),ipv6Port:document.getElementById('ipv6-ntp-port'),keyId:document.getElementById('key-id'),keyValue:keyField,stress:document.getElementById('stress-tests')};
let selectedTargetOption=null;
function savedOption(){return Array.from(targetMenu.querySelectorAll('.target-option')).find(option=>option.textContent===targetField.value)}
function loadProfile(option){
    const data=option?.dataset;
    profileFields.ipv4.value=data?.ipv4||'';
    profileFields.ipv6.value=data?.ipv6||'';
    profileFields.ipv4Port.value=data?data.ipv4Port||'':String(%u);
    profileFields.ipv6Port.value=data?data.ipv6Port||'':String(%u);
    profileFields.keyId.value=data?.keyId||'';
    profileFields.keyValue.value=data?.keyValue||'';
    profileFields.stress.checked=data?.stress==='1';
    updateSaveButton();
}
async function selectTarget(option){
    targetMenu.hidden=true;
    targetField.setAttribute('aria-expanded','false');
    markTargetOption(null);
    if(option.textContent===targetField.value&&document.getElementById('save-settings').disabled)return;
    if(!document.getElementById('save-settings').disabled){
        if(confirm('You have unsaved changes to the current target time server.\n\nOK = keep (save) the changes\nCancel = discard the changes')){
            if(!settingsForm.reportValidity())return;
            document.getElementById('client-time').value=clientTime();
            try{
                const saved=await fetch('/settings',{method:'POST',body:new URLSearchParams(new FormData(settingsForm))});
                if(!saved.ok){alert(await saved.text());return}
            }catch{alert('Unable to save settings.');return}
        }
    }
    const data=option.dataset;
    const body=new URLSearchParams({target:option.textContent,ipv4:data.ipv4||'',ipv6:data.ipv6||'',ipv4_ntp_port:data.ipv4Port||'',ipv6_ntp_port:data.ipv6Port||'',key_id:data.keyId||'',key_value:data.keyValue||'',client_time:clientTime()});
    if(data.stress==='1')body.set('stress_tests','1');
    try{
        const response=await fetch('/settings',{method:'POST',body});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to load the selected target.')}
}
function markTargetOption(option){
    if(selectedTargetOption)selectedTargetOption.classList.remove('selected');
    selectedTargetOption=option;
    if(option)option.classList.add('selected');
}
targetToggle.addEventListener('click',()=>{targetMenu.hidden=!targetMenu.hidden;targetField.setAttribute('aria-expanded',String(!targetMenu.hidden));if(targetMenu.hidden)markTargetOption(null);else targetField.focus()});
targetMenu.addEventListener('click',event=>{const option=event.target.closest('.target-option');if(option)selectTarget(option)});
targetMenu.addEventListener('contextmenu',event=>{const option=event.target.closest('.target-option');if(!option)return;event.preventDefault();markTargetOption(option)});
async function deleteTarget(option){
    if(!option)return;
    if(!confirm('Delete saved target '+option.textContent+' and all its settings?'))return;
    try{
        const response=await fetch('/targets/delete',{method:'POST',body:new URLSearchParams({target:option.textContent,client_time:clientTime()})});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to delete saved target.')}
}
document.addEventListener('keydown',event=>{if(event.key==='Delete'&&selectedTargetOption){event.preventDefault();deleteTarget(selectedTargetOption)}});
document.getElementById('delete-target').addEventListener('click',()=>deleteTarget(savedOption()));
targetField.addEventListener('input',()=>{markTargetOption(null);loadProfile(savedOption())});
document.addEventListener('click',event=>{if(!event.target.closest('.target-combobox')){targetMenu.hidden=true;targetField.setAttribute('aria-expanded','false');markTargetOption(null)}});
let currentStatus=null;
function updateSaveButton(){
    const changed=settingsInputs.some(input=>input.type==='checkbox'?input.checked!==input.defaultChecked:input.value!==input.defaultValue);
    document.getElementById('save-settings').disabled=!changed;
    document.getElementById('cancel-settings').disabled=!changed;
    const canManage=!changed&&!!savedOption()&&!currentStatus?.running;
    document.getElementById('clone-target').disabled=!canManage||targetField.value.length+6>40;
    document.getElementById('rename-target').disabled=!canManage;
    document.getElementById('delete-target').disabled=!savedOption()||!!currentStatus?.running;
    if(currentStatus)renderStatus(currentStatus);
}
function renderStatus(j){
    const colors={blue:'#0000ff',green:'#00c000',yellow:'#ffd000',red:'#e00000'};
    const v6={gua:['blue','IPv6: global unicast address (GUA)'],ula:['green','IPv6: unique local address (ULA)'],lla:['yellow','IPv6: link-local address (LLA)'],none:['red','IPv6: not available']};
    const led4=document.getElementById('led-ipv4'),led6=document.getElementById('led-ipv6');
    const c4=!j.ready?'blue':!j.link?'red':j.ipv4?'green':'red',s6=v6[j.ipv6]||v6.none,c6=!j.ready?'blue':!j.link?'red':s6[0];
    led4.style.background=colors[c4];led6.style.background=colors[c6];
    led4.title=!j.ready?'IPv4: starting up':!j.link?'Ethernet: link disconnected':j.ipv4?'IPv4: available':'IPv4: not available';
    led6.title=!j.ready?'IPv6: starting up':!j.link?'Ethernet: link disconnected':s6[1];
    document.getElementById('network-mac').textContent=j.network.mac;
    document.getElementById('network-ipv4').textContent=j.network.ipv4;
    document.getElementById('network-lla').textContent=j.network.lla;
    document.getElementById('network-ula').textContent=j.network.ula;
    document.getElementById('network-gua').textContent=j.network.gua;
    const ready=j.target_valid&&document.getElementById('save-settings').disabled;
    const available=filter=>j.tests.some(test=>test.enabled&&filter(test));
    const groups=[()=>true,test=>test.ntp,test=>test.ntp&&test.family===4,test=>test.ntp&&test.family===6];
    document.querySelectorAll('.test-run').forEach((button,index)=>button.disabled=!ready||j.running||!available(groups[index]));
    document.getElementById('delete-target').disabled=!savedOption()||j.running;
    const hasResults=j.tests.some(t=>t.status!=='Not yet run');
    document.getElementById('stop-tests').disabled=!j.running;
    document.getElementById('clear-results').disabled=j.running||!hasResults;
    document.getElementById('download-button').disabled=!hasResults;
    document.getElementById('download-link').style.pointerEvents=hasResults?'':'none';
    document.getElementById('tests').innerHTML=j.tests.map((t,i)=>`<tr class="${t.enabled&&ready?'':'disabled'}"><td>${i+1}. ${t.name}</td><td>${t.enabled&&ready?`<button onclick="run('test-'+${i})" ${j.running?'disabled':''}>Run</button> `:'<button disabled>Run</button> '}<span class="result ${t.status==='Pass'?'pass':t.status==='Fail'?'fail':''}">${t.status}</span></td><td>${t.time}</td></tr>`).join('');
}
function renderConnectionLost(){
    const lost='Connection lost';
    const led4=document.getElementById('led-ipv4'),led6=document.getElementById('led-ipv6');
    led4.style.background='#e00000';led6.style.background='#e00000';
    led4.title='Ethernet: connection lost';led6.title='Ethernet: connection lost';
    ['network-ipv4','network-lla','network-ula','network-gua'].forEach(id=>{
        const address=document.getElementById(id);
        if(address.textContent!=='Not assigned')address.textContent=lost;
    });
}
settingsForm.addEventListener('input',updateSaveButton);
settingsForm.addEventListener('change',updateSaveButton);
window.addEventListener('pageshow',updateSaveButton);
updateSaveButton();
function clientTime(){const d=new Date(),p=n=>String(n).padStart(2,'0');return d.getFullYear()+'-'+p(d.getMonth()+1)+'-'+p(d.getDate())+' '+p(d.getHours())+':'+p(d.getMinutes())+':'+p(d.getSeconds())}
settingsForm.addEventListener('submit',async event=>{
    event.preventDefault();
    if(!settingsForm.reportValidity())return;
    document.getElementById('client-time').value=clientTime();
    try{
        const response=await fetch('/settings',{method:'POST',body:new URLSearchParams(new FormData(settingsForm))});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to save settings.')}
});
document.getElementById('cancel-settings').addEventListener('click',async ()=>{
    try{
        const response=await fetch('/settings/cancel',{method:'POST'});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to reload saved settings.')}
});
async function changeTarget(action,newName){
    try{
        const response=await fetch('/targets/'+action,{method:'POST',body:new URLSearchParams({target:targetField.value,new_target:newName,client_time:clientTime()})});
        if(!response.ok){alert(await response.text());return}
        location.reload();
    }catch{alert('Unable to '+action+' target.')}
}
document.getElementById('clone-target').addEventListener('click',()=>changeTarget('clone',targetField.value+' - Copy'));
const renameDialog=document.getElementById('rename-dialog'),newTargetName=document.getElementById('new-target-name');
document.getElementById('rename-target').addEventListener('click',()=>{newTargetName.value=targetField.value;renameDialog.showModal();newTargetName.focus();newTargetName.select()});
document.getElementById('cancel-rename').addEventListener('click',()=>renameDialog.close());
document.getElementById('rename-form').addEventListener('submit',event=>{event.preventDefault();if(newTargetName.value===targetField.value){renameDialog.close();return}changeTarget('rename',newTargetName.value)});
let statusRequestPending=false;
async function refresh(){
    if(statusRequestPending)return;
    statusRequestPending=true;
    const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),4000);
    try{
        const response=await fetch('/status?timestamp='+Date.now(),{cache:'no-store',signal:controller.signal});
        if(!response.ok)throw new Error('Unable to refresh status.');
        currentStatus=await response.json();
        updateSaveButton();
    }catch(error){
        console.warn('Unable to refresh ESP32 status.',error);
        renderConnectionLost();
    }finally{
        clearTimeout(timeout);
        statusRequestPending=false;
    }
}
async function run(x){
    if(!currentStatus?.target_valid||!document.getElementById('save-settings').disabled)return;
    const safety=await fetch('/target-safety');
    if(await safety.text()==='external'&&!confirm('Warning: this target is a public time-server pool or resolves to an external address. Testing it may result in your address being blocked or banned. Continue only if you are authorized to test this server.'))return;
    if(!document.getElementById('save-settings').disabled)return;
    const response=await fetch('/run?scope='+encodeURIComponent(x));
    if(!response.ok)alert(await response.text());
    refresh();
}
async function stopTests(){let r=await fetch('/stop');if(!r.ok)alert(await r.text());refresh()}
async function clearResults(){let r=await fetch('/clear');if(!r.ok)alert(await r.text());refresh()}
refresh();setInterval(refresh,1500);
</script></body></html>)HTML",
                                         target, target_options.get(), profile.ipv4, ipv4_port,
                                         profile.ipv6, ipv6_port, key_id, profile.key.key_value,
                                         profile.stress_tests ? "checked" : "", stress_test_request_count,
                                         get_app_metadata()->project_name, get_app_metadata()->version,
                                         mac_address, ipv4_address, ipv6_lla, ipv6_ula, ipv6_gua,
                                         static_cast<unsigned>(DefaultNtpPort), static_cast<unsigned>(DefaultNtpPort));
        if (length < 0 || static_cast<size_t>(length) >= PageBufferSize)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Page generation failed");
        httpd_resp_set_type(request, "text/html");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_send(request, page.get(), HTTPD_RESP_USE_STRLEN);
    }

    bool form_value(const char *body, const char *key, char *output, size_t output_size)
    {
        bool found = false;
        const size_t key_size = std::strlen(key);
        for (const char *entry = body; *entry;)
        {
            const char *end = std::strchr(entry, '&');
            if (end == nullptr)
                end = entry + std::strlen(entry);
            const char *equals = static_cast<const char *>(std::memchr(entry, '=', end - entry));
            if (equals != nullptr && static_cast<size_t>(equals - entry) == key_size &&
                std::memcmp(entry, key, key_size) == 0)
            {
                if (found)
                    return false;
                found = true;
                size_t cursor = 0;
                for (const char *read = equals + 1; read < end; ++read)
                {
                    unsigned char value = static_cast<unsigned char>(*read);
                    if (value == '+')
                        value = ' ';
                    else if (value == '%')
                    {
                        if (end - read < 3 || !std::isxdigit(static_cast<unsigned char>(read[1])) ||
                            !std::isxdigit(static_cast<unsigned char>(read[2])))
                            return false;
                        const auto hex_value = [](char digit)
                        {
                            return std::isdigit(static_cast<unsigned char>(digit)) ? digit - '0' : std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10;
                        };
                        value = static_cast<unsigned char>((hex_value(read[1]) << 4) | hex_value(read[2]));
                        read += 2;
                    }
                    if (value == 0 || cursor + 1 >= output_size)
                        return false;
                    output[cursor++] = static_cast<char>(value);
                }
                output[cursor] = '\0';
            }
            entry = *end ? end + 1 : end;
        }
        return found;
    }

    bool form_target_value(const char *body, char *output, size_t output_size)
    {
        char raw[TargetNameSize + 64]{};
        if (!form_value(body, "target", raw, sizeof(raw)))
            return false;
        const char *start = raw;
        while (*start == ' ' || (*start >= '\t' && *start <= '\r'))
            ++start;
        size_t length = std::strlen(start);
        while (length > 0 && (start[length - 1] == ' ' || (start[length - 1] >= '\t' && start[length - 1] <= '\r')))
            --length;
        if (length >= output_size)
            return false;
        std::memcpy(output, start, length);
        output[length] = '\0';
        return true;
    }

    bool optional_u16(const char *text, uint16_t *value)
    {
        *value = 0;
        if (*text == '\0')
            return true;
        uint32_t parsed = 0;
        for (const char *digit = text; *digit; ++digit)
        {
            if (*digit < '0' || *digit > '9')
                return false;
            parsed = parsed * 10 + static_cast<unsigned>(*digit - '0');
            if (parsed > UINT16_MAX)
                return false;
        }
        if (parsed == 0)
            return false;
        *value = static_cast<uint16_t>(parsed);
        return true;
    }

    bool read_form_body(httpd_req_t *request, char *body, size_t size)
    {
        if (request->content_len <= 0 || static_cast<size_t>(request->content_len) >= size)
            return false;
        size_t cursor = 0;
        while (cursor < static_cast<size_t>(request->content_len))
        {
            const int received = httpd_req_recv(request, body + cursor, request->content_len - cursor);
            if (received <= 0)
                return false;
            cursor += static_cast<size_t>(received);
        }
        body[cursor] = '\0';
        return std::memchr(body, '\0', cursor) == nullptr;
    }

    uint64_t next_stored_timestamp()
    {
        uint64_t newest = 0;
        for (size_t index = 0; index < s_target_history.count; ++index)
            newest = std::max(newest, s_target_history.entries[index].last_stored);
        if (newest >= std::numeric_limits<uint64_t>::max() - 1)
        {
            std::sort(s_target_history.entries.begin(), s_target_history.entries.begin() + s_target_history.count,
                      [](const TargetHistoryEntry &left, const TargetHistoryEntry &right)
                      { return left.last_stored < right.last_stored; });
            for (size_t index = 0; index < s_target_history.count; ++index)
                s_target_history.entries[index].last_stored = index + 1;
            newest = s_target_history.count;
        }
        return newest + 1;
    }

    esp_err_t apply_settings(httpd_req_t *request)
    {
        char body[1024]{};
        if (!read_form_body(request, body, sizeof(body)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid settings body");
        TargetHistoryEntry profile{};
        char ipv4_port[6]{};
        char ipv6_port[6]{};
        char key_id[6]{};
        char stress_tests[2]{};
        char client_time[24]{};
        if (!form_target_value(body, profile.target, sizeof(profile.target)) || !valid_target_name(profile.target))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Target name must be 1 to 40 printable ASCII characters.");
        if (!form_value(body, "ipv4", profile.ipv4, sizeof(profile.ipv4)) ||
            !form_value(body, "ipv6", profile.ipv6, sizeof(profile.ipv6)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target addresses");
        if (!valid_address(profile.ipv4, AF_INET) || !valid_address(profile.ipv6, AF_INET6))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Enter literal IPv4 and IPv6 addresses or leave them blank.");
        if (is_self_target(profile.ipv4) || is_self_target(profile.ipv6))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "You must enter the target address of the server, not this program.");
        if (!form_value(body, "ipv4_ntp_port", ipv4_port, sizeof(ipv4_port)) ||
            !optional_u16(ipv4_port, &profile.ipv4_ntp_port) ||
            !form_value(body, "ipv6_ntp_port", ipv6_port, sizeof(ipv6_port)) ||
            !optional_u16(ipv6_port, &profile.ipv6_ntp_port))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "NTP ports must be blank or between 1 and 65535.");
        if (!form_value(body, "key_id", key_id, sizeof(key_id)) || !optional_u16(key_id, &profile.key.key_id))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Symmetric-key Id must be blank or between 1 and 65535.");
        if (!form_value(body, "key_value", profile.key.key_value, sizeof(profile.key.key_value)) ||
            !valid_key_value(profile.key.key_value))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Symmetric-key value must be blank or exactly 32 alphanumeric characters.");
        if (std::strstr(body, "stress_tests=") != nullptr)
        {
            if (!form_value(body, "stress_tests", stress_tests, sizeof(stress_tests)) || std::strcmp(stress_tests, "1") != 0)
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid stress test setting");
            profile.stress_tests = true;
        }
        if (!form_value(body, "client_time", client_time, sizeof(client_time)) || timestamp_sort_value(client_time) == 0)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid client date and time");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot change settings while tests are running.");
        }
        profile.last_stored = next_stored_timestamp();
        add_or_update_history_entry(profile);
        s_profile = profile;
        s_results = {};
        save_settings();
        xSemaphoreGive(s_state_mutex);
        httpd_resp_set_status(request, "303 See Other");
        httpd_resp_set_hdr(request, "Location", "/");
        return httpd_resp_send(request, nullptr, 0);
    }

    esp_err_t settings_handler(httpd_req_t *request)
    {
        return apply_settings(request);
    }

    esp_err_t copy_or_rename_target(httpd_req_t *request, bool rename)
    {
        char body[256]{};
        if (!read_form_body(request, body, sizeof(body)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target body");
        char target[TargetNameSize]{};
        char new_target[TargetNameSize]{};
        char client_time[24]{};
        if (!form_target_value(body, target, sizeof(target)) || !valid_target_name(target) ||
            !form_value(body, "new_target", new_target, sizeof(new_target)) || !valid_target_name(new_target))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target name");
        if (!form_value(body, "client_time", client_time, sizeof(client_time)) || timestamp_sort_value(client_time) == 0)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid client date and time");
        if (!rename && (std::strlen(target) + sizeof(" - Copy") > sizeof(new_target) ||
                        std::strcmp(new_target + std::strlen(target), " - Copy") != 0 ||
                        std::strncmp(new_target, target, std::strlen(target)) != 0))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid cloned target name");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load() || std::strcmp(s_profile.target, target) != 0)
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Target changed or tests are running. Reload the page.");
        }
        TargetHistoryEntry *source = find_history_entry(target);
        if (source == nullptr)
        {
            xSemaphoreGive(s_state_mutex);
            return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Target not found");
        }
        if (find_history_entry(new_target) != nullptr)
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "A target with that name already exists.");
        }
        TargetHistoryEntry updated = *source;
        std::snprintf(updated.target, sizeof(updated.target), "%s", new_target);
        updated.last_stored = next_stored_timestamp();
        if (rename)
        {
            source = find_history_entry(target);
            remove_history_entry(static_cast<size_t>(source - s_target_history.entries.data()));
        }
        add_or_update_history_entry(updated);
        s_profile = updated;
        s_results = {};
        save_settings();
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, rename ? "renamed" : "cloned");
    }

    esp_err_t clone_target_handler(httpd_req_t *request)
    {
        return copy_or_rename_target(request, false);
    }

    esp_err_t rename_target_handler(httpd_req_t *request)
    {
        return copy_or_rename_target(request, true);
    }

    esp_err_t delete_target_handler(httpd_req_t *request)
    {
        char body[256]{};
        if (!read_form_body(request, body, sizeof(body)))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target body");
        char target[TargetNameSize]{};
        char client_time[24]{};
        if (!form_target_value(body, target, sizeof(target)) || !valid_target_name(target))
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid target name");
        if (!form_value(body, "client_time", client_time, sizeof(client_time)) || timestamp_sort_value(client_time) == 0)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid client date and time");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot delete targets while tests are running.");
        }
        TargetHistoryEntry *entry = find_history_entry(target);
        if (entry == nullptr)
        {
            xSemaphoreGive(s_state_mutex);
            return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Target not found");
        }
        const size_t index = static_cast<size_t>(entry - s_target_history.entries.data());
        const bool deleting_current = std::strcmp(s_profile.target, target) == 0;
        remove_history_entry(index);
        if (deleting_current)
        {
            s_profile = s_target_history.count == 0 ? TargetHistoryEntry{} : s_target_history.entries[0];
            s_results = {};
        }
        save_settings();
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "deleted");
    }

    esp_err_t cancel_settings_handler(httpd_req_t *request)
    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_tests_running.load())
        {
            xSemaphoreGive(s_state_mutex);
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot reload settings while tests are running.");
        }
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "reloaded");
    }

    esp_err_t status_handler(httpd_req_t *request)
    {
        ResponseBuffer response = allocate_response_buffer(StatusResponseBufferSize);
        if (!response)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate status");
        size_t cursor = 0;
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        const bool target_valid = valid_profile(s_profile);
        const char *ipv4_address = s_startup_ipv4[0] != '\0' ? s_startup_ipv4 : "Not assigned";
        const char *ipv6_lla = s_startup_ipv6_linklocal[0] != '\0' ? s_startup_ipv6_linklocal : "Not assigned";
        const char *ipv6_ula = s_ipv6_ula[0] != '\0' ? s_ipv6_ula : "Not assigned";
        const char *ipv6_gua = s_ipv6_gua[0] != '\0' ? s_ipv6_gua : "Not assigned";
        if (!s_ethernet_connected.load())
        {
            ipv4_address = s_ipv4_was_assigned ? "Connection lost" : ipv4_address;
            ipv6_lla = s_ipv6_lla_was_assigned ? "Connection lost" : ipv6_lla;
            ipv6_ula = s_ipv6_ula_was_assigned ? "Connection lost" : ipv6_ula;
            ipv6_gua = s_ipv6_gua_was_assigned ? "Connection lost" : ipv6_gua;
        }
        NetworkInfo network = read_network_info();
        cursor += std::snprintf(response.get() + cursor, StatusResponseBufferSize - cursor,
                                "{\"running\":%s,\"target_valid\":%s,\"ready\":%s,\"link\":%s,\"ipv4\":%s,\"ipv6\":\"%s\","
                                "\"network\":{\"mac\":\"%s\",\"ipv4\":\"%s\",\"lla\":\"%s\",\"ula\":\"%s\",\"gua\":\"%s\"},\"tests\":[",
                                s_tests_running.load() ? "true" : "false", target_valid ? "true" : "false",
                                s_startup_logged ? "true" : "false", s_ethernet_connected.load() ? "true" : "false",
                                ipv4_available_locked() ? "true" : "false", ipv6_level_locked(), network.mac,
                                ipv4_address, ipv6_lla, ipv6_ula, ipv6_gua);
        for (size_t index = 0; index < Tests.size(); ++index)
        {
            char name[100];
            html_escape(Tests[index].name, name, sizeof(name));
            char timestamp[sizeof(s_results[index].timestamp)];
            html_escape(s_results[index].timestamp, timestamp, sizeof(timestamp));
            cursor += std::snprintf(response.get() + cursor, StatusResponseBufferSize - cursor,
                                    "%s{\"name\":\"%s\",\"enabled\":%s,\"family\":%u,\"ntp\":%s,\"status\":\"%s\",\"time\":\"%s\"}",
                                    index == 0 ? "" : ",", name,
                                    test_available(Tests[index], s_profile) ? "true" : "false",
                                    Tests[index].family == AF_INET ? 4U : 6U,
                                    Tests[index].kind == TestDefinition::Kind::Ping ? "false" : "true",
                                    status_text(s_results[index].status), timestamp);
        }

        xSemaphoreGive(s_state_mutex);
        if (cursor + 3 >= StatusResponseBufferSize)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Status generation failed");
        std::snprintf(response.get() + cursor, StatusResponseBufferSize - cursor, "]}");
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_send(request, response.get(), HTTPD_RESP_USE_STRLEN);
    }

    esp_err_t favicon_handler(httpd_req_t *request)
    {
        httpd_resp_set_type(request, "image/x-icon");
        return httpd_resp_send(request, reinterpret_cast<const char *>(favicon_ico_start),
                               favicon_ico_end - favicon_ico_start);
    }

    esp_err_t clear_results_handler(httpd_req_t *request)
    {
        if (s_tests_running.load())
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Cannot clear test results while tests are running.");
        }

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        s_results = {};
        xSemaphoreGive(s_state_mutex);
        return httpd_resp_sendstr(request, "cleared");
    }

    esp_err_t stop_tests_handler(httpd_req_t *request)
    {
        if (!s_tests_running.load())
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "No tests are running.");
        }

        s_stop_tests_requested.store(true);
        return httpd_resp_sendstr(request, "stopping");
    }

    esp_err_t target_safety_handler(httpd_req_t *request)
    {
        return httpd_resp_sendstr(request, target_requires_external_warning() ? "external" : "internal");
    }

    esp_err_t run_handler(httpd_req_t *request)
    {
        char query[40]{};
        char scope[12]{};
        if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
            httpd_query_key_value(query, "scope", scope, sizeof(scope)) != ESP_OK)
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Missing test scope");
        bool started = false;
        if (std::strcmp(scope, "all") == 0)
            started = start_tests(TestRequest::Scope::All);
        else if (std::strcmp(scope, "ntp") == 0)
            started = start_tests(TestRequest::Scope::Ntp);
        else if (std::strcmp(scope, "ipv4") == 0)
            started = start_tests(TestRequest::Scope::NtpIpv4);
        else if (std::strcmp(scope, "ipv6") == 0)
            started = start_tests(TestRequest::Scope::NtpIpv6);
        else if (std::strncmp(scope, "test-", 5) == 0)
        {
            char *end = nullptr;
            const long index = std::strtol(scope + 5, &end, 10);
            if (end != scope + 5 && *end == '\0' && index >= 0 && index < static_cast<long>(Tests.size()))
                started = start_tests(TestRequest::Scope::Individual, static_cast<int>(index));
        }
        if (!started)
        {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "Tests are already running or unavailable");
        }
        return httpd_resp_sendstr(request, "started");
    }

    esp_err_t report_handler(httpd_req_t *request)
    {
        ResponseBuffer report = allocate_response_buffer(ReportBufferSize);
        if (!report)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient memory to generate report");
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        char target[TargetNameSize * 12];
        markdown_escape(s_profile.target, target, sizeof(target));
        char ipv4_port[6], ipv6_port[6], key_id[6];
        format_optional_u16(s_profile.ipv4_ntp_port, ipv4_port);
        format_optional_u16(s_profile.ipv6_ntp_port, ipv6_port);
        format_optional_u16(s_profile.key.key_id, key_id);
        size_t cursor = std::snprintf(report.get(), ReportBufferSize,
                                      "<p align=\"center\"><strong>ESP32TimeServerTester Report</strong></p>\n\n"
                                      "## Settings\n\n"
                                      "| Settings | Values |\n"
                                      "| --- | --- |\n"
                                      "| **Target time server:** | %s |\n"
                                      "| **IPv4 address:** | %s |\n"
                                      "| **IPv4 NTP Port:** | %s |\n"
                                      "| **IPv6 address:** | %s |\n"
                                      "| **IPv6 NTP Port:** | %s |\n"
                                      "| **NTPv4 symmetric-key id:** | %s |\n"
                                      "| **NTPv4 symmetric-key value:** | %s |\n"
                                      "| **Mini saturated stress tests:** | %s (%d NTP requests each) |\n\n"
                                      "## Test results\n\n"
                                      "| No. | Test | Result | Reported date and time |\n"
                                      "| ---: | --- | --- | --- |\n",
                                      target,
                                      s_profile.ipv4[0] ? s_profile.ipv4 : "(blank)",
                                      ipv4_port[0] ? ipv4_port : "(blank)",
                                      s_profile.ipv6[0] ? s_profile.ipv6 : "(blank)",
                                      ipv6_port[0] ? ipv6_port : "(blank)",
                                      key_id[0] ? "Present but redacted in this report" : "not entered",
                                      s_profile.key.key_value[0] ? "Present but redacted in this report" : "Not entered",
                                      s_profile.stress_tests ? "Enabled" : "Disabled",
                                      number_of_NTP_requests_in_a_mini_stress_test);
        for (size_t index = 0; index < Tests.size() && cursor < ReportBufferSize; ++index)
        {
            const char *result = s_results[index].status == TestStatus::Pass
                                     ? "<span style=\"color:green\">Pass</span>"
                                 : s_results[index].status == TestStatus::Fail
                                     ? "<span style=\"color:red\">Fail</span>"
                                     : status_text(s_results[index].status);
            cursor += std::snprintf(report.get() + cursor, ReportBufferSize - cursor, "| %u | %s | %s | %s |\n",
                                    static_cast<unsigned>(index + 1), Tests[index].name,
                                    result, s_results[index].timestamp);
        }
        const NetworkInfo network = read_network_info();
        xSemaphoreGive(s_state_mutex);
        if (cursor >= ReportBufferSize ||
            static_cast<size_t>(std::snprintf(report.get() + cursor, ReportBufferSize - cursor,
                                              "\n---\n\n"
                                              "%s v%s\n\n"
                                              "| Address Types | Addresses  |\n"
                                              "| --- | --- |\n"
                                              "| **MAC:** | %s |\n"
                                              "| **IPv4:** | %s |\n"
                                              "| **IPv6 LLA:** | %s |\n"
                                              "| **IPv6 ULA:** | %s |\n"
                                              "| **IPv6 GUA:** | %s |\n\n"
                                              "\n---\n\n"
                                              "https://github.com/roblatour/ESP32TimeServerTester\n",
                                              get_app_metadata()->project_name, get_app_metadata()->version,
                                              network.mac, network.ipv4, network.lla, network.ula, network.gua)) >=
                ReportBufferSize - cursor)
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Report generation failed");
        httpd_resp_set_type(request, "text/markdown");

        char safe_target[TargetNameSize];
        size_t j = 0;

        // Format the original target name (remove unsafe characters etc.) for use in the filename.
        for (size_t i = 0; s_profile.target[i] != '\0' && j < sizeof(safe_target) - 1; i++)
        {
            char c = s_profile.target[i];

            if (isalnum((unsigned char)c) || c == '-' || c == '(' || c == ')')
            {
                safe_target[j++] = c;
            }
            else
            {
                safe_target[j++] = '_';
            }
        }

        safe_target[j] = '\0';

        char content_disposition[128];
        snprintf(content_disposition,
                 sizeof(content_disposition),
                 "attachment; filename=ESP32TimeServerTester_%s.md",
                 safe_target);

        httpd_resp_set_hdr(request,
                           "Content-Disposition",
                           content_disposition);

        return httpd_resp_send(request, report.get(), HTTPD_RESP_USE_STRLEN);
    }

    void start_web_server()
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 13;
        httpd_handle_t server = nullptr;
        ESP_ERROR_CHECK(httpd_start(&server, &config));
        const httpd_uri_t routes[] = {
            {"/", HTTP_GET, root_handler, nullptr},
            {"/favicon.ico", HTTP_GET, favicon_handler, nullptr},
            {"/settings", HTTP_POST, settings_handler, nullptr},
            {"/settings/cancel", HTTP_POST, cancel_settings_handler, nullptr},
            {"/targets/clone", HTTP_POST, clone_target_handler, nullptr},
            {"/targets/rename", HTTP_POST, rename_target_handler, nullptr},
            {"/targets/delete", HTTP_POST, delete_target_handler, nullptr},
            {"/status", HTTP_GET, status_handler, nullptr},
            {"/run", HTTP_GET, run_handler, nullptr},
            {"/stop", HTTP_GET, stop_tests_handler, nullptr},
            {"/clear", HTTP_GET, clear_results_handler, nullptr},
            {"/target-safety", HTTP_GET, target_safety_handler, nullptr},
            {"/report", HTTP_GET, report_handler, nullptr},
        };
        for (const auto &route : routes)
            ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    }

    void log_startup_if_ready()
    {
        if (s_startup_logged || !s_startup_ready || s_startup_ipv4[0] == '\0' ||
            s_startup_ipv6_linklocal[0] == '\0')
            return;

        NetworkInfo network = read_network_info();
        std::printf("\n");
        ESP_LOGI(Tag, "*********************************************************");
        ESP_LOGI(Tag, "%s v%s is open for business", get_app_metadata()->project_name, get_app_metadata()->version);
        ESP_LOGI(Tag, "Host name:               %s", Hostname);
        ESP_LOGI(Tag, "MAC address:             %s", network.mac);
        ESP_LOGI(Tag, "IPv4 address:            %s", s_startup_ipv4);
        ESP_LOGI(Tag, "Link-local IPv6 address: %s", s_startup_ipv6_linklocal);

        ESP_LOGI(Tag, "Unique-local IPv6 (ULA): %s", s_ipv6_ula[0] != '\0' ? s_ipv6_ula : "Not assigned");
        ESP_LOGI(Tag, "Global IPv6 (GUA):       %s", s_ipv6_gua[0] != '\0' ? s_ipv6_gua : "Not assigned");
        ESP_LOGI(Tag, "Browse to:               http://%s", Hostname);
        ESP_LOGI(Tag, "*********************************************************");
        std::printf("\n");

        s_startup_logged = true;
        update_status_leds_locked();
    }

    // Address in use: GUA, else ULA, else LLA. Caller holds s_state_mutex.
    void update_ipv6_in_use_locked()
    {
        update_status_leds_locked();
        const char *best = s_ipv6_gua[0] != '\0'   ? s_ipv6_gua
                           : s_ipv6_ula[0] != '\0' ? s_ipv6_ula
                                                   : s_startup_ipv6_linklocal;
        if (std::strcmp(best, s_ipv6_in_use) == 0)
            return;
        std::snprintf(s_ipv6_in_use, sizeof(s_ipv6_in_use), "%s", best);
        if (best[0] != '\0')
            ESP_LOGI(Tag, "IPv6 address now in use: %s", best);
    }

    // Caller holds s_state_mutex.
    void record_ipv6_locked(const esp_ip6_addr_t &ip)
    {
        const char *label = nullptr;
        char *destination = nullptr;
        switch (esp_netif_ip6_get_addr_type(&ip))
        {
        case ESP_IP6_ADDR_IS_LINK_LOCAL:
            label = "link-local (LLA)";
            destination = s_startup_ipv6_linklocal;
            break;
        case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
            label = "unique-local (ULA)";
            destination = s_ipv6_ula;
            break;
        case ESP_IP6_ADDR_IS_GLOBAL:
            label = "global (GUA)";
            destination = s_ipv6_gua;
            break;
        default:
            return;
        }
        char address[INET6_ADDRSTRLEN];
        if (inet6_ntoa_r(ip, address, sizeof(address)) == nullptr || std::strcmp(address, destination) == 0)
            return;
        std::snprintf(destination, INET6_ADDRSTRLEN, "%s", address);
        if (destination == s_startup_ipv6_linklocal)
            s_ipv6_lla_was_assigned = true;
        else if (destination == s_ipv6_ula)
            s_ipv6_ula_was_assigned = true;
        else if (destination == s_ipv6_gua)
            s_ipv6_gua_was_assigned = true;
        ESP_LOGI(Tag, "New IPv6 %s address: %s", label, address);
        update_ipv6_in_use_locked();
        log_startup_if_ready();
    }

    void network_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP)
        {
            const auto *event = static_cast<const ip_event_got_ip_t *>(event_data);
            if (event->esp_netif != s_eth_netif)
                return;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            std::snprintf(s_startup_ipv4, sizeof(s_startup_ipv4), IPSTR, IP2STR(&event->ip_info.ip));
            s_ipv4_was_assigned = true;
            if (s_startup_logged)
                ESP_LOGI(Tag, "Ethernet IPv4 address: %s", s_startup_ipv4);
            else
                log_startup_if_ready();
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
        if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6)
        {
            const auto *event = static_cast<const ip_event_got_ip6_t *>(event_data);
            if (event->esp_netif != s_eth_netif)
                return;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            record_ipv6_locked(event->ip6_info.ip);
            xSemaphoreGive(s_state_mutex);
        }
        if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_LOST_IP)
        {
            ESP_LOGW(Tag, "Ethernet IPv4 address lost");
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_startup_ipv4[0] = '\0';
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
    }

    // Polls the netif so a ULA/GUA that arrives after the link-local address is still picked up
    void refresh_ipv6_addresses()
    {
        esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
        const int count = esp_netif_get_all_ip6(s_eth_netif, addresses);
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        bool has_lla = false, has_ula = false, has_gua = false;
        for (int index = 0; index < count; ++index)
        {
            switch (esp_netif_ip6_get_addr_type(&addresses[index]))
            {
            case ESP_IP6_ADDR_IS_LINK_LOCAL:
                has_lla = true;
                break;
            case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
                has_ula = true;
                break;
            case ESP_IP6_ADDR_IS_GLOBAL:
                has_gua = true;
                break;
            default:
                break;
            }
            record_ipv6_locked(addresses[index]);
        }
        const auto drop = [](char *address, bool present, const char *label)
        {
            if (present || address[0] == '\0')
                return;
            ESP_LOGW(Tag, "IPv6 %s address lost: %s", label, address);
            address[0] = '\0';
        };
        drop(s_startup_ipv6_linklocal, has_lla, "link-local (LLA)");
        drop(s_ipv6_ula, has_ula, "unique-local (ULA)");
        drop(s_ipv6_gua, has_gua, "global (GUA)");
        update_ipv6_in_use_locked();
        xSemaphoreGive(s_state_mutex);
    }

    void ethernet_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *)
    {
        if (event_base != ETH_EVENT)
            return;

        if (event_id == ETHERNET_EVENT_CONNECTED)
        {
            s_ethernet_connected.store(true);
            s_ethernet_link_known.store(true);
            ESP_LOGI(Tag, "Ethernet link connected");
            const esp_err_t error = esp_netif_create_ip6_linklocal(s_eth_netif);
            if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
                ESP_LOGE(Tag, "Unable to create Ethernet IPv6 link-local address: %s", esp_err_to_name(error));
            refresh_ipv6_addresses();
        }
        else if (event_id == ETHERNET_EVENT_DISCONNECTED)
        {
            s_ethernet_connected.store(false);
            s_ethernet_link_known.store(true);
            ESP_LOGW(Tag, "Ethernet link disconnected; waiting for automatic link recovery");
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_startup_ipv4[0] = '\0';
            s_startup_ipv6_linklocal[0] = '\0';
            s_ipv6_ula[0] = '\0';
            s_ipv6_gua[0] = '\0';
            s_ipv6_in_use[0] = '\0';
            update_status_leds_locked();
            xSemaphoreGive(s_state_mutex);
        }
    }

    void ethernet_recovery_task(void *)
    {
        int missing_ipv6_checks = 0;
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            if (s_ethernet_connected.load())
            {
                esp_ip6_addr_t ipv6{};
                if (esp_netif_get_ip6_linklocal(s_eth_netif, &ipv6) == ESP_OK)
                {
                    missing_ipv6_checks = 0;
                    refresh_ipv6_addresses();
                    continue;
                }
                else
                {
                    ++missing_ipv6_checks;
                    if (esp_netif_is_netif_up(s_eth_netif))
                    {
                        const esp_err_t error = esp_netif_create_ip6_linklocal(s_eth_netif);
                        if (error != ESP_OK)
                            ESP_LOGW(Tag, "Unable to restore Ethernet IPv6 link-local address: %s", esp_err_to_name(error));
                    }
                }
                refresh_ipv6_addresses();
                if (missing_ipv6_checks < 3)
                    continue;
                ESP_LOGW(Tag, "Ethernet link-local IPv6 address missing; restarting Ethernet driver");
            }
            else
                ESP_LOGW(Tag, "Ethernet link is still down; restarting Ethernet driver");

            missing_ipv6_checks = 0;
            const esp_err_t stop_error = esp_eth_stop(s_eth_handle);
            if (stop_error != ESP_OK && stop_error != ESP_ERR_INVALID_STATE)
                ESP_LOGE(Tag, "Unable to stop Ethernet driver for recovery: %s", esp_err_to_name(stop_error));
            vTaskDelay(pdMS_TO_TICKS(250));
            const esp_err_t start_error = esp_eth_start(s_eth_handle);
            if (start_error != ESP_OK && start_error != ESP_ERR_INVALID_STATE)
                ESP_LOGE(Tag, "Unable to restart Ethernet driver: %s", esp_err_to_name(start_error));
        }
    }

    void start_ethernet()
    {
        if (MACAddress[0] != '\0')
        {
            uint8_t mac_address[6];
            if (!parse_mac_address(MACAddress, mac_address))
            {
                ESP_LOGE(Tag, "Configured MAC address is invalid: %s", MACAddress);
                return;
            }
            ESP_ERROR_CHECK(esp_base_mac_addr_set(mac_address));
            ESP_LOGI(Tag, "Using Ethernet MAC address %s", MACAddress);
        }

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
        s_eth_netif = esp_netif_new(&netif_config);
        if (s_eth_netif == nullptr)
            ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        if ((esp_netif_get_flags(s_eth_netif) & ESP_NETIF_FLAG_IPV6_AUTOCONFIG_ENABLED) == 0)
        {
            ESP_LOGE(Tag, "Ethernet IPv6 SLAAC autoconfiguration is disabled");
            return;
        }
        ESP_LOGI(Tag, "Ethernet IPv6 SLAAC autoconfiguration is enabled");
        ESP_ERROR_CHECK(esp_netif_set_hostname(s_eth_netif, Hostname));

        eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
        emac_config.smi_gpio.mdc_num = EthernetMdcGpio;
        emac_config.smi_gpio.mdio_num = EthernetMdioGpio;
        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        phy_config.phy_addr = EthernetPhyAddress;
        phy_config.reset_gpio_num = EthernetPhyResetGpio;
        esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_config);
        esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
        ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
        bool receive_all_multicast = true;
        ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_ALL_MULTICAST, &receive_all_multicast));
        ESP_LOGI(Tag, "Ethernet is accepting all multicast frames for IPv6 Router Advertisements");
        ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &ethernet_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &ethernet_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &network_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &network_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &network_event_handler, nullptr));
        ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
        if (xTaskCreate(ethernet_recovery_task, "eth_recovery", 3072, nullptr, 5, nullptr) != pdPASS)
            ESP_LOGE(Tag, "Unable to create Ethernet recovery task");
    }
} // namespace

extern "C" void app_main()
{

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(psa_crypto_init() == PSA_SUCCESS ? ESP_OK : ESP_FAIL);
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == nullptr)
    {
        ESP_LOGE(Tag, "Unable to create state mutex");
        return;
    }
    load_settings();
    init_status_leds();
    start_ethernet();
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(Hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Time Server Tester"));
    start_web_server();
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_startup_ready = true;
    log_startup_if_ready();
    xSemaphoreGive(s_state_mutex);
}
