# SPDX-License-Identifier: Apache-2.0
set(BOARD "actinius_icarus_ns")

cmake_minimum_required(VERSION 3.13.1)

set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml)
list(APPEND mcuboot_OVERLAY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay-rsa.conf")


find_package(Zephyr)
project(gps)

target_sources(app PRIVATE src/main.c)
