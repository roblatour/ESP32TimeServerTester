#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(expression) do { if (!(expression)) throw std::runtime_error(#expression); } while (0)

using esp_err_t = int;
using nvs_handle_t = unsigned int;
using led_strip_handle_t = void *;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_STATE = 1, ESP_ERR_NVS_NOT_FOUND = 2;
constexpr int NVS_READWRITE = 0, portMAX_DELAY = -1;
constexpr int HTTPD_400_BAD_REQUEST = 400, HTTPD_404_NOT_FOUND = 404, HTTPD_500_INTERNAL_SERVER_ERROR = 500;
constexpr int LED_MODEL_WS2812 = 0, LED_STRIP_COLOR_COMPONENT_FMT_GRB = 0, RMT_CLK_SRC_DEFAULT = 0;
constexpr bool WS2812_RGB_enabled = TEST_WS2812 != 0;
constexpr int WS2812DataPin = 4, RGB_LED_Brightness_Percent = 70;
constexpr char Tag[] = "test", NvsNamespace[] = "tester", PhysicalLedsNvsKey[] = "leds_on";
int s_state_mutex = 0;
bool s_physical_leds_on = true, s_startup_logged = false;
led_strip_handle_t s_led_strip = nullptr;
std::atomic<bool> s_ethernet_link_known{false}, s_ethernet_connected{false};
std::atomic<bool> s_tests_running{false};
bool s_ote_in_progress = false;
enum class StatusColor { Blue, Green, Yellow, Red, Off, Orange };
struct led_strip_config_t
{
    int strip_gpio_num = 0, max_leds = 0, led_model = 0, color_component_format = 0;
};
struct led_strip_rmt_config_t { int clk_src = 0, resolution_hz = 0, mem_block_symbols = 0; };
struct httpd_req_t
{
    std::string body, response;
    size_t content_len = 0, offset = 0;
    int status = 200;
};
struct FakeState
{
    bool has_value = false, locked = false, ipv4 = false;
    bool restarted = false;
    uint8_t value = 1, pending = 1;
    int open_error = 0, get_error = 0, set_error = 0, commit_error = 0, hardware_error = 0;
    int opens = 0, sets = 0, commits = 0, closes = 0, clears = 0, refreshes = 0, creates = 0, logs = 0;
    std::string ipv6 = "none";
    std::array<std::array<uint32_t, 3>, 2> pixels{};
    std::array<std::array<uint32_t, 3>, 2> transmitted_pixels{};
} fake;

template <typename... Args> void log_error(Args...) { ++fake.logs; }
#define ESP_LOGE(...) log_error(__VA_ARGS__)
#define ESP_LOGI(...) log_error(__VA_ARGS__)
const char *esp_err_to_name(esp_err_t) { return "test error"; }
void check_error(esp_err_t error) { if (error != ESP_OK) throw std::runtime_error("SDK error"); }
#define ESP_ERROR_CHECK(expression) check_error(expression)
void xSemaphoreTake(int, int) { CHECK(!fake.locked); fake.locked = true; }
void xSemaphoreGive(int) { CHECK(fake.locked); fake.locked = false; }
bool ipv4_available_locked() { CHECK(fake.locked); return fake.ipv4; }
const char *ipv6_level_locked() { CHECK(fake.locked); return fake.ipv6.c_str(); }

esp_err_t nvs_open(const char *name, int, nvs_handle_t *handle)
{
    CHECK(std::strcmp(name, "tester") == 0);
    ++fake.opens;
    *handle = 1;
    return fake.open_error;
}
esp_err_t nvs_get_u8(nvs_handle_t, const char *key, uint8_t *value)
{
    CHECK(std::strcmp(key, "leds_on") == 0);
    if (fake.get_error != ESP_OK) return fake.get_error;
    if (!fake.has_value) return ESP_ERR_NVS_NOT_FOUND;
    *value = fake.value;
    return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t, const char *key, uint8_t value)
{
    CHECK(std::strcmp(key, "leds_on") == 0 && value <= 1);
    ++fake.sets;
    fake.pending = value;
    return fake.set_error;
}
esp_err_t nvs_commit(nvs_handle_t)
{
    ++fake.commits;
    if (fake.commit_error != ESP_OK) return fake.commit_error;
    fake.value = fake.pending;
    fake.has_value = true;
    return ESP_OK;
}
void nvs_close(nvs_handle_t) { ++fake.closes; }
esp_err_t led_strip_new_rmt_device(const led_strip_config_t *strip, const led_strip_rmt_config_t *rmt,
                                  led_strip_handle_t *handle)
{
    CHECK(rmt->mem_block_symbols >= strip->max_leds * 24 + 2);
    ++fake.creates;
    *handle = &fake;
    return fake.hardware_error;
}
esp_err_t led_strip_set_pixel(led_strip_handle_t, int index, uint32_t red, uint32_t green, uint32_t blue)
{
    CHECK(fake.locked);
    fake.pixels[index] = {red, green, blue};
    return fake.hardware_error;
}
esp_err_t led_strip_refresh(led_strip_handle_t)
{
    CHECK(fake.locked);
    ++fake.refreshes;
    if (fake.hardware_error == ESP_OK)
        fake.transmitted_pixels = fake.pixels;
    return fake.hardware_error;
}
esp_err_t led_strip_clear(led_strip_handle_t)
{
    CHECK(fake.locked);
    ++fake.clears;
    fake.pixels = {};
    if (fake.hardware_error == ESP_OK)
        fake.transmitted_pixels = fake.pixels;
    return fake.hardware_error;
}
int httpd_req_recv(httpd_req_t *request, char *body, size_t count)
{
    count = std::min(count, request->body.size() - request->offset);
    std::memcpy(body, request->body.data() + request->offset, count);
    request->offset += count;
    return static_cast<int>(count);
}
esp_err_t httpd_resp_send_err(httpd_req_t *request, int status, const char *message)
{
    request->status = status;
    request->response = message;
    return ESP_OK;
}
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *) { return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *) { return ESP_OK; }
esp_err_t httpd_resp_sendstr(httpd_req_t *request, const char *message)
{
    request->response = message;
    return ESP_OK;
}
void esp_restart()
{
    CHECK(!fake.locked);
    CHECK(fake.transmitted_pixels == decltype(fake.pixels){});
    fake.restarted = true;
}

// The runner extracts these functions verbatim from main.cpp, not reimplementations.
#include "physical_led_production.inc"

void reset()
{
    fake = {};
    s_physical_leds_on = true;
    s_startup_logged = false;
    s_led_strip = nullptr;
    s_ethernet_link_known = false;
    s_ethernet_connected = false;
    s_tests_running = false;
    s_ote_in_progress = false;
}
httpd_req_t request_for(const std::string &body)
{
    httpd_req_t request;
    request.body = body;
    request.content_len = body.size();
    physical_leds_handler(&request);
    CHECK(!fake.locked);
    return request;
}
void apply()
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    CHECK(update_status_leds_locked() == ESP_OK);
    xSemaphoreGive(s_state_mutex);
}
void first_use_and_startup()
{
    reset();
    load_physical_led_setting();
    CHECK(s_physical_leds_on && fake.has_value && fake.value == 1);
    CHECK(fake.sets == 1 && fake.commits == 1 && fake.closes == 1);
    init_status_leds();
    CHECK(fake.creates == (WS2812_RGB_enabled ? 1 : 0));
    if (WS2812_RGB_enabled)
        CHECK(fake.pixels[0][2] > 0 && fake.pixels[1][2] > 0);
    fake.value = 0;
    s_physical_leds_on = true;
    load_physical_led_setting();
    CHECK(!s_physical_leds_on && fake.commits == 1);
    init_status_leds();
    CHECK(fake.pixels == decltype(fake.pixels){});
    CHECK(fake.transmitted_pixels == decltype(fake.pixels){});
}
void persistence_and_live_control()
{
    reset();
    load_physical_led_setting();
    init_status_leds();
    const int before = fake.commits;
    s_tests_running = true;
    auto response = request_for("enabled=0");
    if (!WS2812_RGB_enabled)
    {
        CHECK(response.status == 404 && fake.commits == before && s_physical_leds_on);
        return;
    }
    CHECK(response.status == 200 && response.response.find("\"physical_leds_on\":false") != std::string::npos);
    CHECK(s_tests_running && !s_physical_leds_on && fake.value == 0 && fake.commits == before + 1);
    CHECK(fake.pixels == decltype(fake.pixels){});
    CHECK(fake.transmitted_pixels == decltype(fake.pixels){});
    CHECK(request_for("enabled=0").status == 200 && fake.commits == before + 1);
    s_startup_logged = true;
    s_ethernet_link_known = true;
    s_ethernet_connected = false;
    apply();
    CHECK(fake.pixels == decltype(fake.pixels){});
    s_physical_leds_on = true;
    load_physical_led_setting();
    CHECK(!s_physical_leds_on);
    CHECK(request_for("enabled=1").status == 200);
    CHECK(fake.value == 1 && s_physical_leds_on);
    CHECK(fake.pixels[0][0] > 0 && fake.pixels[1][0] > 0);
    s_ethernet_connected = true;
    fake.ipv4 = true;
    fake.ipv6 = "lla";
    apply();
    CHECK(fake.pixels[0][1] > 0 && fake.pixels[1][0] > 0 && fake.pixels[1][1] > 0);
    fake.ipv6 = "ula";
    apply();
    CHECK(fake.pixels[1][0] == 0 && fake.pixels[1][1] > 0);
    fake.ipv6 = "gua";
    apply();
    CHECK(fake.pixels[1][2] > 0);
}
void invalid_requests_and_failures()
{
    for (const char *body : {"", "enabled=", "enabled=2", "enabled=false", "enabled=10",
                             "enabled=0&enabled=1", "enabled=%00", "target=test"})
    {
        reset();
        const auto response = request_for(body);
        CHECK(response.status == (WS2812_RGB_enabled ? 400 : 404));
        CHECK(fake.opens == 0 && fake.commits == 0 && s_physical_leds_on);
    }
    if (!WS2812_RGB_enabled) return;
    for (int failure = 0; failure < 4; ++failure)
    {
        reset();
        load_physical_led_setting();
        init_status_leds();
        if (failure == 0) fake.open_error = ESP_FAIL;
        if (failure == 1) fake.set_error = ESP_FAIL;
        if (failure == 2) fake.commit_error = ESP_FAIL;
        if (failure == 3) fake.hardware_error = ESP_FAIL;
        CHECK(request_for("enabled=0").status == 500 && fake.logs > 0);
        CHECK(s_physical_leds_on == (failure != 3));
        CHECK(fake.value == (failure == 3 ? 0 : 1));
    }
    reset();
    CHECK(request_for("enabled=0").status == 500);
}
void load_failures_are_explicit()
{
    for (int failure = 0; failure < 5; ++failure)
    {
        reset();
        if (failure == 0) fake.open_error = ESP_FAIL;
        if (failure == 1) fake.get_error = ESP_FAIL;
        if (failure == 2) fake.set_error = ESP_FAIL;
        if (failure == 3) fake.commit_error = ESP_FAIL;
        if (failure == 4) { fake.has_value = true; fake.value = 2; }
        bool threw = false;
        try { load_physical_led_setting(); }
        catch (const std::runtime_error &) { threw = true; }
        CHECK(threw);
    }
}
void orange_during_ote_respects_off()
{
    reset();
    load_physical_led_setting();
    init_status_leds();
    const int commits = fake.commits;
    CHECK(begin_ote_update() && s_ote_in_progress);
    CHECK(!begin_ote_update());
    if (WS2812_RGB_enabled)
    {
        const uint32_t level = 255U * RGB_LED_Brightness_Percent / 100U;
        const std::array<uint32_t, 3> orange{level, level * 165U / 255U, 0};
        CHECK(fake.transmitted_pixels[0] == orange && fake.transmitted_pixels[1] == orange);
        // Network events must not replace an in-progress update indication.
        s_startup_logged = true;
        s_ethernet_link_known = true;
        s_ethernet_connected = false;
        apply();
        CHECK(fake.transmitted_pixels[0] == orange && fake.transmitted_pixels[1] == orange);
        CHECK(request_for("enabled=0").status == 200);
        CHECK(fake.transmitted_pixels == decltype(fake.pixels){});
        apply();
        CHECK(fake.transmitted_pixels == decltype(fake.pixels){});
        CHECK(request_for("enabled=1").status == 200);
        CHECK(fake.transmitted_pixels[0] == orange && fake.transmitted_pixels[1] == orange);
    }
    ote_failure("test upload failure");
    CHECK(!s_ote_in_progress);
    if (WS2812_RGB_enabled)
        CHECK(fake.transmitted_pixels[0][0] > 0 && fake.transmitted_pixels[0][1] == 0);
    CHECK(fake.commits == commits + (WS2812_RGB_enabled ? 2 : 0));
    s_tests_running = true;
    CHECK(!begin_ote_update() && !s_ote_in_progress);
    s_tests_running = false;
    s_physical_leds_on = false;
    CHECK(begin_ote_update());
    CHECK(fake.transmitted_pixels == decltype(fake.pixels){});
}
int main()
{
    try
    {
        first_use_and_startup();
        persistence_and_live_control();
        invalid_requests_and_failures();
        load_failures_are_explicit();
        orange_during_ote_respects_off();
        reset();
        load_physical_led_setting();
        init_status_leds();
        CHECK(begin_ote_update());
        const int commits = fake.commits;
        ote_restart(nullptr);
        CHECK(fake.restarted && s_physical_leds_on && fake.value == 1 && fake.commits == commits);
        s_ote_in_progress = false;
        s_startup_logged = false;
        init_status_leds();
        if (WS2812_RGB_enabled)
            CHECK(fake.transmitted_pixels[0][2] > 0 && fake.transmitted_pixels[1][2] > 0);
        std::cout << "PASS physical LED transmission, persistence, colors, OTE lifecycle and failures; WS2812="
                  << WS2812_RGB_enabled << '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
