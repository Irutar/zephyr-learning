# SPDX-License-Identifier: Apache-2.0
#
# Sysbuild CMake configuration for my_app.
#
# When this file exists, sysbuild uses it instead of the default template.
# We must include the sysbuild boilerplate first, then add custom logic.

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sysbuild LANGUAGES)

# Partition configuration is handled by:
#   sysbuild/mcuboot.overlay → custom mcuboot flash layout
#   boards/esp32_devkitc_esp32_procpu.overlay → my_app flash layout
# Both must define identical partitions.
