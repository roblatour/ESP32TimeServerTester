idf_component_register(
    SRCS "main.cpp" "app_metadata.cpp" "ptp_tester.cpp" "ptp_auth.cpp" "custom/native_ote.cpp"
    EMBED_FILES "favicon.ico"
    INCLUDE_DIRS "."
    REQUIRES esp_eth esp_event esp_netif esp_http_server esp_timer nvs_flash mdns lwip mbedtls driver
             app_update bootloader_support esp_app_format
)

string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP BUILD_TIME "%H:%M:%S")
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_SHA)
    set(GIT_SHA "unknown")
endif()

get_filename_component(_default_time_server_source_dir "${CMAKE_CURRENT_LIST_DIR}/../../ESP32TimeServer" ABSOLUTE)
set(ESP32_TIME_SERVER_SOURCE_DIR "${_default_time_server_source_dir}" CACHE PATH "Optional ESP32 Time Server source directory")
include("${CMAKE_CURRENT_LIST_DIR}/PtpDutPolicy.cmake")
ptp_extract_policy("${ESP32_TIME_SERVER_SOURCE_DIR}")
configure_file(ptp_dut_policy.h.in "${CMAKE_CURRENT_BINARY_DIR}/ptp_dut_policy.h" @ONLY)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "DUT PTP policy available=${PTP_POLICY_AVAILABLE}; provenance=${PTP_POLICY_PROVENANCE}; identity=${PTP_POLICY_IDENTITY}")

target_compile_definitions(${COMPONENT_TARGET} PRIVATE
    APP_PROJECT_NAME="${CMAKE_PROJECT_NAME}"
    APP_VERSION="${PROJECT_VER}"
    APP_COPYRIGHT="Copyright Rob Latour, 2026"
    APP_LICENSE="MIT"
    APP_HOMEPAGE="https://github.com/roblatour/ESP32TimeServerTester"
    APP_BUILD_DATE="${BUILD_DATE}"
    APP_BUILD_TIME="${BUILD_TIME}"
    APP_GIT_SHA="${GIT_SHA}"
)
