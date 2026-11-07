#include <functional>
#include <iostream>
#include <stdexcept>

// Exercise the production handler with only the ESP-IDF boundary replaced.
#include "..\..\main\custom\native_ote.cpp"

#define CHECK(expression) do { if (!(expression)) throw std::runtime_error(#expression); } while (0)

static bool begin_callback()
{
    if (fake.tests_running || fake.update_active)
        return false;
    fake.update_active = true;
    return true;
}
static void progress_callback(unsigned int value, unsigned int total)
{
    fake.progress.emplace_back(value, total);
    fake.events.push_back("progress:" + std::to_string(value));
}
static void failure_callback(const char *)
{
    ++fake.failures;
    fake.update_active = false;
    fake.events.push_back("failure");
}
static void success_callback() { ++fake.successes; fake.events.push_back("success"); }

static httpd_req_t fresh_request()
{
    fake = FakeState{};
    s_server = nullptr;
    s_boot_selected = false;
    s_callbacks = {begin_callback, progress_callback, failure_callback, success_callback};
    fake.description.magic_word = ESP_APP_DESC_MAGIC_WORD;
    std::memcpy(fake.description.project_name, Hostname, sizeof(Hostname));
    httpd_req_t request;
    request.content_len = 2500;
    request.body.resize(request.content_len, 'x');
    request.headers = {{"Authorization", std::string("Bearer ") + OTEPassword},
                       {"Content-Type", "application/octet-stream"},
                       {"Content-Length", std::to_string(request.content_len)}};
    esp_image_header_t image{};
    image.magic = ESP_IMAGE_HEADER_MAGIC;
    image.segment_count = 1;
    image.chip_id = 42;
    esp_image_segment_header_t segment{};
    segment.data_len = static_cast<uint32_t>(request.content_len - sizeof(image) - sizeof(segment));
    std::memcpy(request.body.data(), &image, sizeof(image));
    std::memcpy(request.body.data() + sizeof(image), &segment, sizeof(segment));
    std::memcpy(request.body.data() + sizeof(image) + sizeof(segment), &fake.description, sizeof(fake.description));
    return request;
}

static void rejected_before_writes()
{
    for (int scenario = 0; scenario < 17; ++scenario)
    {
        auto request = fresh_request();
        std::string status = "400";
        if (scenario == 0) { request.headers["Authorization"] = "Bearer incorrect"; status = "401"; }
        if (scenario == 1) { request.headers["Content-Type"] = "multipart/form-data"; status = "415"; }
        if (scenario == 2) { fake.inactive.size = 100; status = "413"; }
        if (scenario == 3) request.body[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + 4] = '!';
        if (scenario == 4) fake.chip_error = ESP_FAIL;
        if (scenario == 5) request.headers.erase("Content-Length");
        if (scenario == 6) request.headers["Content-Length"] = "999999999999999999999999999";
        if (scenario == 7) request.headers["Transfer-Encoding"] = "chunked";
        if (scenario == 8) { fake.inactive.address = fake.running.address; status = "500"; }
        if (scenario == 9) { fake.no_partition = true; status = "500"; }
        if (scenario == 10) { fake.tests_running = true; status = "409"; }
        if (scenario == 11) { request.headers.erase("Authorization"); status = "401"; }
        if (scenario == 12) request.headers["Content-Length"] = "2501";
        if (scenario == 13) request.body[0] = 0;
        if (scenario == 14) request.body[1] = 0;
        if (scenario == 15) { request.uri = "/ote?unexpected"; status = "404"; }
        if (scenario == 16) { fake.inactive.subtype = 0; status = "500"; }
        CHECK(upload_handler(&request) == ESP_FAIL);
        CHECK(request.status.find(status) == 0);
        CHECK(fake.begin_count == 0 && fake.writes == 0 && fake.boots == 0 && fake.aborts == 0);
        CHECK(!s_boot_selected && fake.successes == 0 && !fake.update_active);
    }
}

static void failures_release_only_live_handles()
{
    for (int scenario = 0; scenario < 9; ++scenario)
    {
        auto request = fresh_request();
        int aborts = 0, ends = 0, boots = 0;
        if (scenario == 0) fake.begin_error = ESP_FAIL;
        if (scenario == 1) { fake.fail_write = 1; aborts = 1; }
        if (scenario == 2) { fake.fail_write = 2; aborts = 1; }
        if (scenario == 3) fake.fail_receive = 1;
        if (scenario == 4) { fake.fail_receive = 2; aborts = 1; }
        if (scenario == 5) { fake.end_error = ESP_FAIL; ends = 1; }
        if (scenario == 6) { fake.boot_error = ESP_FAIL; ends = 1; boots = 1; }
        if (scenario == 7) { request.body.resize(ImagePrefixSize + 100); aborts = 1; }
        if (scenario == 8) { fake.fail_receive = 2; fake.receive_error = HTTPD_SOCK_ERR_TIMEOUT; aborts = 1; }
        CHECK(upload_handler(&request) == ESP_FAIL);
        CHECK(fake.aborts == aborts && fake.ends == ends && fake.boots == boots);
        CHECK(fake.failures == 1 && fake.successes == 0 && !s_boot_selected && !fake.update_active);
        CHECK(request.status.find("200") != 0);
    }
}

static void successful_order_and_lockout()
{
    auto request = fresh_request();
    fake.receive_limit = 73;
    CHECK(upload_handler(&request) == ESP_FAIL);
    CHECK(request.status == "200 OK" && s_boot_selected && fake.update_active);
    CHECK(fake.written == request.body);
    CHECK(fake.begin_count == 1 && fake.ends == 1 && fake.boots == 1 && fake.aborts == 0);
    CHECK(fake.successes == 1 && fake.failures == 0);
    CHECK(fake.progress.front().first == 0 && fake.progress.back().first == request.content_len);
    for (size_t index = 1; index < fake.progress.size(); ++index)
        CHECK(fake.progress[index].first > fake.progress[index - 1].first);
    for (const auto &entry : fake.progress) CHECK(entry.second == request.content_len);
    const auto &events = fake.events;
    CHECK(events.front() == "progress:0" && events[1] == "begin" && events[2] == "write");
    CHECK(events[events.size() - 4] == "end" && events[events.size() - 3] == "boot");
    CHECK(events[events.size() - 2] == "success" && events.back() == "reply");
    const int writes = fake.writes;
    request.offset = 0;
    CHECK(upload_handler(&request) == ESP_FAIL && request.status.find("409") == 0);
    CHECK(fake.writes == writes && fake.boots == 1 && fake.successes == 1 && fake.failures == 0);
    request = fresh_request();
    fake.reply_error = ESP_FAIL;
    upload_handler(&request);
    CHECK(s_boot_selected && fake.successes == 1 && fake.update_active);
}

static void deadlines_and_socket_failures()
{
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        auto request = fresh_request();
        int aborts = 0;
        if (scenario == 0) { fake.fail_receive = 1; fake.receive_error = HTTPD_SOCK_ERR_TIMEOUT; }
        if (scenario == 1) fake.receive_advance_us = 300000000;
        if (scenario == 2) { fake.begin_advance_us = 300000000; aborts = 1; }
        if (scenario == 3) { fake.write_advance_us = 300000000; aborts = 1; }
        if (scenario == 4) fake.end_advance_us = 300000000;
        if (scenario == 5) fake.socket_error = -1;
        upload_handler(&request);
        CHECK(request.status.find(scenario == 5 ? "400" : "408") == 0);
        CHECK(fake.aborts == aborts && fake.boots == 0 && !s_boot_selected && fake.failures == 1);
        CHECK(!fake.update_active);
    }
    auto request = fresh_request();
    char bytes[1];
    CHECK(receive_bytes(&request, bytes, 1, 1) == ESP_OK);
    CHECK(fake.socket_timeouts.back() == 1000);
    CHECK(receive_bytes(&request, bytes, 1, 0) == ESP_ERR_TIMEOUT);
    request = fresh_request();
    CHECK(receive_bytes(&request, bytes, 1, 300000000) == ESP_OK);
    CHECK(fake.socket_timeouts.back() == 15000000);
    CHECK(receive_bytes(&request, bytes, 1, 2000000) == ESP_OK);
    CHECK(fake.socket_timeouts.back() == 2000000);
}

static void retry_after_failure()
{
    auto request = fresh_request();
    fake.fail_write = 1;
    upload_handler(&request);
    CHECK(!fake.update_active && !s_boot_selected);
    fake.fail_write = 0;
    request.offset = 0;
    upload_handler(&request);
    CHECK(request.status == "200 OK" && fake.update_active && s_boot_selected);
}

static void startup_and_mdns()
{
    fresh_request();
    esp_netif_t netif;
    CHECK(native_ote_start(nullptr, s_callbacks) == ESP_ERR_INVALID_ARG);
    auto incomplete = s_callbacks;
    incomplete.begin = nullptr;
    CHECK(native_ote_start(&netif, incomplete) == ESP_ERR_INVALID_ARG);
    CHECK(native_ote_start(&netif, s_callbacks) == ESP_OK);
    CHECK(fake.instance == Hostname && fake.service == "_http");
    CHECK(fake.path == "/ote" && fake.service_port == 3232);
    CHECK(fake.configuration.server_port == 3232 && fake.configuration.ctrl_port != 32768);
    CHECK(fake.configuration.max_open_sockets == 2 && fake.interface_name == "en0");
    CHECK(native_ote_start(&netif, s_callbacks) == ESP_ERR_INVALID_STATE);
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        fresh_request();
        if (scenario == 0) fake.start_error = ESP_FAIL;
        if (scenario == 1) fake.service_error = ESP_FAIL;
        if (scenario == 2) fake.register_error = ESP_FAIL;
        CHECK(native_ote_start(&netif, s_callbacks) == ESP_FAIL);
        CHECK(fake.stops == (scenario == 0 ? 0 : 1) && s_server == nullptr);
    }
}

int main()
{
    const std::pair<const char *, std::function<void()>> tests[] = {
        {"prewrite rejection and active-test exclusion", rejected_before_writes},
        {"OTA failure cleanup", failures_release_only_live_handles},
        {"success ordering and lockout", successful_order_and_lockout},
        {"deadlines and sockets", deadlines_and_socket_failures},
        {"retry after failure", retry_after_failure},
        {"startup, Ethernet binding, UI coexistence and mDNS", startup_and_mdns},
    };
    try
    {
        for (const auto &test : tests) { test.second(); std::cout << "PASS " << test.first << '\n'; }
        std::cout << "6 OTE groups passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
