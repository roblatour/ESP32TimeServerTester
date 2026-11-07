cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED PTP_POLICY_TEST_DIR)
    message(FATAL_ERROR "Pass -DPTP_POLICY_TEST_DIR=<dedicated generated fixture directory>")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../main/PtpDutPolicy.cmake")
set(fixture "${PTP_POLICY_TEST_DIR}")
file(MAKE_DIRECTORY "${fixture}/main/custom")
set(settings "#define PTP_MASTER_SELECTION_ENABLED 1
#define PTP_FOREIGN_MASTER_CAPACITY 8
#define PTP_DOMAIN 7
#define PTP_LOG_ANNOUNCE_INTERVAL 1
#define PTP_LOG_SYNC_INTERVAL 0
#define PTP_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS 10
#define PTP_AUTHENTICATION_ENABLED 0
#define PTP_AUTHENTICATION_REQUIRED 0
#define PTP_AUTHENTICATION_SPP 1
#define PTP_AUTHENTICATION_ACTIVE_KEY_ID 123
#define NTP_AUTHENTICATION_REQUIRED 0
")
set(runtime "Gnss_Validity_Timeout_Us = 3500000LL;
ptp_time_qualified() && s_ptp_master_selection.role(now_us) == ptp::SelectionRole::Master
PTP_MASTER_SELECTION_ENABLED != 0, PTP_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS * 1000000LL
2000000LL << PTP_LOG_ANNOUNCE_INTERVAL
s_ptp_master_selection.observe(parsed.announce_dataset, parsed.source_port_identity, parsed.sequence_id,
status.stratum != 1 || status.holdover) return false;
gnss_timing_valid && last_gnss_valid_us > 0 && (now_us - last_gnss_valid_us) <= Gnss_Validity_Timeout_Us
gnss_recent && !sync_stale && !faults.gnss_invalid
last_pps_us > 0 && pps_age_us >= 0 && pps_age_us <= Gnss_Validity_Timeout_Us
!has_sync_fault(faults) && pps_active && pps_recent && gnss_synchronized
ptp_auth_verify(message, message_length)
")
set(selection "advance == 0 || advance >= 0x8000
entry.count >= 2
log_interval < -7 || log_interval > 4
entry->last_seen_us = now_us
entry->timeout_us = interval_us * 3 > timeout_us_ ? interval_us * 3 : timeout_us_
")
file(WRITE "${fixture}/main/ESP32TimeServerSettings.h" "${settings}")
file(WRITE "${fixture}/main/main.cpp" "${runtime}")
file(WRITE "${fixture}/main/custom/ptp_master_selection.h" "${selection}")
file(WRITE "${fixture}/main/custom/ptp_protocol.h" "announce_dataset")
file(WRITE "${fixture}/main/custom/ptp_auth.cpp" "PTP_AUTHENTICATION_REQUIRED")
ptp_extract_policy("${fixture}")
if(NOT PTP_POLICY_AVAILABLE EQUAL 1 OR NOT PTP_POLICY_SELECTION EQUAL 1 OR
   NOT PTP_POLICY_FOREIGN_MS EQUAL 10000 OR NOT PTP_POLICY_QUALIFICATION_MS EQUAL 3500 OR
   NOT PTP_POLICY_KEY_ID EQUAL 123)
    message(FATAL_ERROR "Clean supported fixture did not produce exact validated metadata")
endif()
set(original_identity "${PTP_POLICY_IDENTITY}")
ptp_extract_policy("${fixture}")
if(NOT PTP_POLICY_AVAILABLE EQUAL 1 OR NOT PTP_POLICY_IDENTITY STREQUAL original_identity)
    message(FATAL_ERROR "Cached/repeated extraction differs from clean metadata")
endif()
string(REPLACE "PTP_MASTER_SELECTION_ENABLED 1" "PTP_MASTER_SELECTION_ENABLED 0" legacy "${settings}")
file(WRITE "${fixture}/main/ESP32TimeServerSettings.h" "${legacy}")
ptp_extract_policy("${fixture}")
if(NOT PTP_POLICY_AVAILABLE EQUAL 1 OR NOT PTP_POLICY_SELECTION EQUAL 0 OR PTP_POLICY_IDENTITY STREQUAL original_identity)
    message(FATAL_ERROR "Changed source retained stale ranked policy/hash")
endif()
foreach(required 0 1)
    string(REPLACE "PTP_AUTHENTICATION_ENABLED 0" "PTP_AUTHENTICATION_ENABLED 1" enabled "${settings}")
    string(REPLACE "PTP_AUTHENTICATION_REQUIRED 0" "PTP_AUTHENTICATION_REQUIRED ${required}" enabled "${enabled}")
    file(WRITE "${fixture}/main/ESP32TimeServerSettings.h" "${enabled}")
    ptp_extract_policy("${fixture}")
    if(NOT PTP_POLICY_AVAILABLE EQUAL 1 OR NOT PTP_POLICY_AUTH_ENABLED EQUAL 1 OR
       NOT PTP_POLICY_AUTH_REQUIRED EQUAL required)
        message(FATAL_ERROR "Configured optional/required policy was not preserved independently of secret key state")
    endif()
endforeach()
foreach(bad "PTP_FOREIGN_MASTER_CAPACITY 33" "PTP_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS 0"
            "PTP_AUTHENTICATION_REQUIRED 1" "PTP_LOG_ANNOUNCE_INTERVAL 5")
    string(REGEX REPLACE " [0-9]+$" "" name "${bad}")
    string(REGEX REPLACE "${name} [0-9]+" "${bad}" invalid "${settings}")
    file(WRITE "${fixture}/main/ESP32TimeServerSettings.h" "${invalid}")
    ptp_extract_policy("${fixture}")
    if(PTP_POLICY_AVAILABLE OR NOT PTP_POLICY_FOREIGN_MS EQUAL 0 OR NOT PTP_POLICY_IDENTITY STREQUAL "unavailable")
        message(FATAL_ERROR "Invalid fixture or stale cached outputs accepted: ${bad}")
    endif()
endforeach()
file(WRITE "${fixture}/main/ESP32TimeServerSettings.h" "${settings}\n#define PTP_MASTER_SELECTION_ENABLED 0\n")
ptp_extract_policy("${fixture}")
if(PTP_POLICY_AVAILABLE)
    message(FATAL_ERROR "Duplicate policy definitions accepted")
endif()
file(WRITE "${fixture}/main/ESP32TimeServerSettings.h" "${settings}")
# Comment out every line rather than just the first one.
string(REPLACE "\n" "\n//" commented "${runtime}")
file(WRITE "${fixture}/main/main.cpp" "//${commented}")
ptp_extract_policy("${fixture}")
if(PTP_POLICY_AVAILABLE)
    message(FATAL_ERROR "Contract markers in comments accepted")
endif()
file(REMOVE "${fixture}/main/custom/ptp_auth.cpp")
ptp_extract_policy("${fixture}")
if(PTP_POLICY_AVAILABLE)
    message(FATAL_ERROR "Missing auth contract retained stale available policy")
endif()

set(current_fixture "${fixture}_current")
file(MAKE_DIRECTORY "${current_fixture}/main/custom")
file(WRITE "${current_fixture}/main/ESP32TimeServerSettings.h" "#define PTP_L2_MASTER_SELECTION_ENABLED 1
#define PTP_L2_FOREIGN_MASTER_CAPACITY 8
#define PTP_L2_DOMAIN 0
#define PTP_L2_LOG_ANNOUNCE_INTERVAL 1
#define PTP_L2_LOG_SYNC_INTERVAL 0
#define PTP_L2_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS 10
#define PTP_L2_AUTHENTICATION_ENABLED 1
#define PTP_L2_AUTHENTICATION_REQUIRED 0
#define PTP_L2_AUTHENTICATION_SPP 1
#define PTP_L2_AUTHENTICATION_ACTIVE_KEY_ID 1
#define NTP_AUTHENTICATION_REQUIRED 0
")
file(WRITE "${current_fixture}/main/main.cpp" "Gnss_Validity_Timeout_Us = 3500000LL;
ptp_time_qualified() && s_ptp_l2_tx_healthy.load(std::memory_order_acquire)
s_ptp_l2_master_selection.role(now_us) == ptp::SelectionRole::Master
PTP_L2_MASTER_SELECTION_ENABLED != 0, PTP_L2_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS * 1000000LL
2000000LL << PTP_L2_LOG_ANNOUNCE_INTERVAL
s_ptp_l2_master_selection.observe(parsed.announce_dataset, parsed.source_port_identity, parsed.sequence_id,
status.stratum != 1 || status.holdover) return false;
gnss_timing_valid && last_gnss_valid_us > 0 && (now_us - last_gnss_valid_us) <= Gnss_Validity_Timeout_Us
gnss_recent && !sync_stale && !faults.gnss_invalid
last_pps_us > 0 && pps_age_us >= 0 && pps_age_us <= Gnss_Validity_Timeout_Us
!has_sync_fault(faults) && pps_active && pps_recent && gnss_synchronized
ptp_l2_auth_verify(message, message_length)
")
file(WRITE "${current_fixture}/main/custom/ptp_l2_master_selection.h" "advance == 0 || advance >= 0x8000
entry.count >= 2
log_interval < -7 || log_interval > 4
entry->last_seen_us = now_us
entry->timeout_us = interval_us * 3 > timeout_us_ ? interval_us * 3 : timeout_us_
")
file(WRITE "${current_fixture}/main/custom/ptp_l2_protocol.h" "announce_dataset")
file(WRITE "${current_fixture}/main/custom/ptp_l2_auth.cpp" "PTP_L2_AUTHENTICATION_REQUIRED")
ptp_extract_policy("${current_fixture}")
if(NOT PTP_POLICY_AVAILABLE EQUAL 1 OR NOT PTP_POLICY_SELECTION EQUAL 1 OR
   NOT PTP_POLICY_AUTH_ENABLED EQUAL 1 OR NOT PTP_POLICY_AUTH_REQUIRED EQUAL 0 OR
   NOT PTP_POLICY_SPP EQUAL 1 OR NOT PTP_POLICY_KEY_ID EQUAL 1 OR
   NOT PTP_POLICY_PROVENANCE STREQUAL "public-source-contract-v2-not-runtime-attestation")
    message(FATAL_ERROR "Current PTP_L2 source contract did not produce validated metadata")
endif()

if(DEFINED PTP_POLICY_SOURCE_DIR)
    ptp_extract_policy("${PTP_POLICY_SOURCE_DIR}")
    if(NOT PTP_POLICY_AVAILABLE EQUAL 1 OR
       NOT PTP_POLICY_PROVENANCE STREQUAL "public-source-contract-v2-not-runtime-attestation")
        message(FATAL_ERROR "Configured current server source did not satisfy the public policy contract")
    endif()
endif()
message(STATUS "PASS: legacy/current extraction, cached/hash changes, optional/required policy without secrets, bounds, duplicate definitions, comments and missing contract")
