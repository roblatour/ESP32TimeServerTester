# Only public source files are inspected. Never preprocess the settings header:
# it includes server secrets that are not tester configuration.
function(ptp_extract_policy source_dir)
    set(PTP_POLICY_AVAILABLE 0 PARENT_SCOPE)
    foreach(field SELECTION CAPACITY DOMAIN LOG_ANNOUNCE LOG_SYNC FOREIGN_MS QUALIFICATION_MS AUTH_ENABLED AUTH_REQUIRED SPP KEY_ID NTP_REQUIRED)
        set(PTP_POLICY_${field} 0 PARENT_SCOPE)
    endforeach()
    set(PTP_POLICY_IDENTITY "unavailable" PARENT_SCOPE)
    set(PTP_POLICY_PROVENANCE "unsupported-or-missing-source-contract" PARENT_SCOPE)
    set(legacy_paths main/ESP32TimeServerSettings.h main/main.cpp main/custom/ptp_master_selection.h
                     main/custom/ptp_protocol.h main/custom/ptp_auth.cpp)
    set(current_paths main/ESP32TimeServerSettings.h main/main.cpp main/custom/ptp_l2_master_selection.h
                      main/custom/ptp_l2_protocol.h main/custom/ptp_l2_auth.cpp)
    set(current_contract TRUE)
    foreach(path IN LISTS current_paths)
        if(NOT EXISTS "${source_dir}/${path}")
            set(current_contract FALSE)
        endif()
    endforeach()
    if(current_contract)
        set(paths ${current_paths})
        set(provenance "public-source-contract-v2-not-runtime-attestation")
    else()
        foreach(path IN LISTS legacy_paths)
            if(NOT EXISTS "${source_dir}/${path}")
                return()
            endif()
        endforeach()
        set(paths ${legacy_paths})
        set(provenance "public-source-contract-v1-not-runtime-attestation")
    endif()
    set(fingerprint "")
    foreach(path IN LISTS paths)
        if(NOT EXISTS "${source_dir}/${path}")
            return()
        endif()
        if(NOT CMAKE_SCRIPT_MODE_FILE)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_dir}/${path}")
        endif()
        file(READ "${source_dir}/${path}" text)
        # Comments must not satisfy either literal extraction or contract markers.
        string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" text "${text}")
        string(REGEX REPLACE "//[^\n]*" "" text "${text}")
        string(REGEX REPLACE "[ \t\r\n]+" " " normalized "${text}")
        if(path STREQUAL "main/ESP32TimeServerSettings.h")
            set(settings "${text}")
        elseif(path STREQUAL "main/main.cpp")
            set(runtime "${normalized}")
            set(runtime_text "${text}")
        elseif(path STREQUAL "main/custom/ptp_master_selection.h" OR
               path STREQUAL "main/custom/ptp_l2_master_selection.h")
            set(selection "${normalized}")
        elseif(path STREQUAL "main/custom/ptp_protocol.h" OR
               path STREQUAL "main/custom/ptp_l2_protocol.h")
            set(protocol "${normalized}")
        else()
            set(auth "${normalized}")
        endif()
        file(SHA256 "${source_dir}/${path}" digest)
        string(APPEND fingerprint "${path}:${digest};")
    endforeach()
    if(current_contract)
        set(names PTP_L2_MASTER_SELECTION_ENABLED PTP_L2_FOREIGN_MASTER_CAPACITY PTP_L2_DOMAIN
                  PTP_L2_LOG_ANNOUNCE_INTERVAL PTP_L2_LOG_SYNC_INTERVAL PTP_L2_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS
                  PTP_L2_AUTHENTICATION_ENABLED PTP_L2_AUTHENTICATION_REQUIRED PTP_L2_AUTHENTICATION_SPP
                  PTP_L2_AUTHENTICATION_ACTIVE_KEY_ID NTP_AUTHENTICATION_REQUIRED)
    else()
        set(names PTP_MASTER_SELECTION_ENABLED PTP_FOREIGN_MASTER_CAPACITY PTP_DOMAIN PTP_LOG_ANNOUNCE_INTERVAL
                  PTP_LOG_SYNC_INTERVAL PTP_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS PTP_AUTHENTICATION_ENABLED
                  PTP_AUTHENTICATION_REQUIRED PTP_AUTHENTICATION_SPP PTP_AUTHENTICATION_ACTIVE_KEY_ID NTP_AUTHENTICATION_REQUIRED)
    endif()
    foreach(name IN LISTS names)
        string(REGEX MATCHALL "(^|\n)[ \t]*#[ \t]*define[ \t]+${name}[ \t]+[^\r\n]*" definitions "${settings}")
        list(LENGTH definitions count)
        if(NOT count EQUAL 1)
            return()
        endif()
        string(REGEX MATCH "define[ \t]+${name}[ \t]+(-?[0-9]+)[ \t]*$" literal "${definitions}")
        if(NOT literal)
            return()
        endif()
        set(${name} "${CMAKE_MATCH_1}")
    endforeach()
    if(current_contract)
        set(selection_enabled "${PTP_L2_MASTER_SELECTION_ENABLED}")
        set(foreign_capacity "${PTP_L2_FOREIGN_MASTER_CAPACITY}")
        set(domain "${PTP_L2_DOMAIN}")
        set(log_announce "${PTP_L2_LOG_ANNOUNCE_INTERVAL}")
        set(log_sync "${PTP_L2_LOG_SYNC_INTERVAL}")
        set(foreign_timeout "${PTP_L2_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS}")
        set(auth_enabled "${PTP_L2_AUTHENTICATION_ENABLED}")
        set(auth_required "${PTP_L2_AUTHENTICATION_REQUIRED}")
        set(auth_spp "${PTP_L2_AUTHENTICATION_SPP}")
        set(auth_key_id "${PTP_L2_AUTHENTICATION_ACTIVE_KEY_ID}")
    else()
        set(selection_enabled "${PTP_MASTER_SELECTION_ENABLED}")
        set(foreign_capacity "${PTP_FOREIGN_MASTER_CAPACITY}")
        set(domain "${PTP_DOMAIN}")
        set(log_announce "${PTP_LOG_ANNOUNCE_INTERVAL}")
        set(log_sync "${PTP_LOG_SYNC_INTERVAL}")
        set(foreign_timeout "${PTP_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS}")
        set(auth_enabled "${PTP_AUTHENTICATION_ENABLED}")
        set(auth_required "${PTP_AUTHENTICATION_REQUIRED}")
        set(auth_spp "${PTP_AUTHENTICATION_SPP}")
        set(auth_key_id "${PTP_AUTHENTICATION_ACTIVE_KEY_ID}")
    endif()
    if(selection_enabled LESS 0 OR selection_enabled GREATER 1 OR
       foreign_capacity LESS 1 OR foreign_capacity GREATER 32 OR
       domain LESS 0 OR domain GREATER 255 OR
       log_announce LESS 0 OR log_announce GREATER 4 OR
       log_sync LESS 0 OR log_sync GREATER 4 OR
       foreign_timeout LESS 1 OR foreign_timeout GREATER 3600 OR
       auth_enabled LESS 0 OR auth_enabled GREATER 1 OR
       auth_required LESS 0 OR auth_required GREATER 1 OR
       auth_required GREATER auth_enabled OR
       auth_spp LESS 0 OR auth_spp GREATER 255 OR
       auth_key_id LESS 1 OR auth_key_id GREATER 4294967295 OR
       NTP_AUTHENTICATION_REQUIRED LESS 0 OR NTP_AUTHENTICATION_REQUIRED GREATER 1)
        return()
    endif()
    string(REGEX MATCH "Gnss_Validity_Timeout_Us[ \t]*=[ \t]*([0-9]+)LL" qualification "${runtime_text}")
    if(NOT qualification)
        return()
    endif()
    set(qualification_us "${CMAKE_MATCH_1}")
    if(qualification_us LESS 1 OR qualification_us GREATER 120000000)
        return()
    endif()
    # Recognize the versioned GNSS-only ranked/legacy contract, not just its settings.
    if(current_contract)
        set(runtime_markers
            "ptp_time_qualified() && s_ptp_l2_tx_healthy.load(std::memory_order_acquire)"
            "s_ptp_l2_master_selection.role(now_us) == ptp::SelectionRole::Master"
            "PTP_L2_MASTER_SELECTION_ENABLED != 0, PTP_L2_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS * 1000000LL"
            "2000000LL << PTP_L2_LOG_ANNOUNCE_INTERVAL"
            "s_ptp_l2_master_selection.observe(parsed.announce_dataset, parsed.source_port_identity, parsed.sequence_id,"
            "status.stratum != 1 || status.holdover) return false;"
            "gnss_timing_valid && last_gnss_valid_us > 0 && (now_us - last_gnss_valid_us) <= Gnss_Validity_Timeout_Us"
            "gnss_recent && !sync_stale && !faults.gnss_invalid"
            "last_pps_us > 0 && pps_age_us >= 0 && pps_age_us <= Gnss_Validity_Timeout_Us"
            "!has_sync_fault(faults) && pps_active && pps_recent && gnss_synchronized"
            "ptp_l2_auth_verify(message, message_length)")
    else()
        set(runtime_markers
            "ptp_time_qualified() && s_ptp_master_selection.role(now_us) == ptp::SelectionRole::Master"
            "PTP_MASTER_SELECTION_ENABLED != 0, PTP_FOREIGN_ANNOUNCE_TIMEOUT_SECONDS * 1000000LL"
            "2000000LL << PTP_LOG_ANNOUNCE_INTERVAL"
            "s_ptp_master_selection.observe(parsed.announce_dataset, parsed.source_port_identity, parsed.sequence_id,"
            "status.stratum != 1 || status.holdover) return false;"
            "gnss_timing_valid && last_gnss_valid_us > 0 && (now_us - last_gnss_valid_us) <= Gnss_Validity_Timeout_Us"
            "gnss_recent && !sync_stale && !faults.gnss_invalid"
            "last_pps_us > 0 && pps_age_us >= 0 && pps_age_us <= Gnss_Validity_Timeout_Us"
            "!has_sync_fault(faults) && pps_active && pps_recent && gnss_synchronized"
            "ptp_auth_verify(message, message_length)")
    endif()
    foreach(marker IN LISTS runtime_markers)
        string(FIND "${runtime}" "${marker}" found)
        if(found LESS 0)
            return()
        endif()
    endforeach()
    foreach(marker "advance == 0 || advance >= 0x8000" "entry.count >= 2"
                   "log_interval < -7 || log_interval > 4" "entry->last_seen_us = now_us"
                   "entry->timeout_us = interval_us * 3 > timeout_us_ ? interval_us * 3 : timeout_us_")
        string(FIND "${selection}" "${marker}" found)
        if(found LESS 0)
            return()
        endif()
    endforeach()
    string(FIND "${protocol}" "announce_dataset" found_protocol)
    if(current_contract)
        string(FIND "${auth}" "PTP_L2_AUTHENTICATION_REQUIRED" found_auth)
    else()
        string(FIND "${auth}" "PTP_AUTHENTICATION_REQUIRED" found_auth)
    endif()
    if(found_protocol LESS 0 OR found_auth LESS 0)
        return()
    endif()
    math(EXPR foreign_ms "${foreign_timeout} * 1000")
    math(EXPR qualification_ms "(${qualification_us} + 999) / 1000")
    string(SHA256 fingerprint "${fingerprint}")
    set(PTP_POLICY_AVAILABLE 1 PARENT_SCOPE)
    set(PTP_POLICY_SELECTION ${selection_enabled} PARENT_SCOPE)
    set(PTP_POLICY_CAPACITY ${foreign_capacity} PARENT_SCOPE)
    set(PTP_POLICY_DOMAIN ${domain} PARENT_SCOPE)
    set(PTP_POLICY_LOG_ANNOUNCE ${log_announce} PARENT_SCOPE)
    set(PTP_POLICY_LOG_SYNC ${log_sync} PARENT_SCOPE)
    set(PTP_POLICY_FOREIGN_MS ${foreign_ms} PARENT_SCOPE)
    set(PTP_POLICY_QUALIFICATION_MS ${qualification_ms} PARENT_SCOPE)
    set(PTP_POLICY_AUTH_ENABLED ${auth_enabled} PARENT_SCOPE)
    set(PTP_POLICY_AUTH_REQUIRED ${auth_required} PARENT_SCOPE)
    set(PTP_POLICY_SPP ${auth_spp} PARENT_SCOPE)
    set(PTP_POLICY_KEY_ID ${auth_key_id} PARENT_SCOPE)
    set(PTP_POLICY_NTP_REQUIRED ${NTP_AUTHENTICATION_REQUIRED} PARENT_SCOPE)
    set(PTP_POLICY_IDENTITY "${fingerprint}" PARENT_SCOPE)
    set(PTP_POLICY_PROVENANCE "${provenance}" PARENT_SCOPE)
endfunction()
