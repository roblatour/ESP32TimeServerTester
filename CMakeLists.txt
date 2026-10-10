cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32TimeServerTester
        VERSION 1.0.0
        SPDX_LICENSE MIT
        DESCRIPTION "Ethernet functional tester for ESP32 Time Server")

if (IDF_VERSION_MAJOR LESS 6 OR
    (IDF_VERSION_MAJOR EQUAL 6 AND IDF_VERSION_MINOR LESS 1))
    message(FATAL_ERROR "This project requires ESP-IDF >= 6.1")
endif()
