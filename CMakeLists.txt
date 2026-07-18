# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# DTC_OVERLAY_FILE must be set BEFORE find_package.
# Board overlay is appended automatically. Add module overlays here.
string(REPLACE "/" "_" BOARD_QUAL ${BOARD})
set(DTC_OVERLAY_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/app.overlay"
  "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_QUAL}.overlay"
  # ── module overlays ──────────────────────────────
  "${CMAKE_CURRENT_SOURCE_DIR}/drivers/sensor/voltage_monitor/vmon.dtsi"
)

# Local secrets (Wi-Fi credentials, etc.) — NOT tracked by git.
# Create prj_local.conf if you need to override placeholders.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/prj_local.conf")
  set(OVERLAY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/prj_local.conf")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_app)

# Custom DTS bindings (must be after find_package for bindings, but DTS_ROOT
# also used for preprocessor — we rely on DTC_OVERLAY_FILE for module DTS)
set(DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE src/main.c)

target_sources_ifdef(CONFIG_APP_WIFI_LOG app PRIVATE
  drivers/wifi_log/wifi_log.c
)

# Sensors
target_sources(app PRIVATE
  drivers/sensor/voltage_monitor/voltage_monitor.c
)

target_include_directories(app PRIVATE include)
