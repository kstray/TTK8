# SPDX-License-Identifier: Apache-2.0
set(BOARD "actinius_icarus_ns")

cmake_minimum_required(VERSION 3.13.1)

set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml)
list(APPEND mcuboot_OVERLAY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay-rsa.conf")


find_package(Zephyr)
project(gps)

zephyr_include_directories(include)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PUBLIC include)

