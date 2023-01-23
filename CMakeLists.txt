#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Set the nRF5340 Audio version. x.x.99 indicates master branch/cutting edge
project(NRF5340_AUDIO VERSION 0.5.99)

string(TIMESTAMP NRF5340_AUDIO_CORE_APP_COMP_DATE "%a %b %d %H:%M:%S %Y")

# Target sources below are specific to the nRF5340 Audio DK HW
target_sources(app PRIVATE
    src/main.c
)

# Include application events and configuration headers
zephyr_library_include_directories(
    src/audio
    src/bluetooth
    src/utils
    src/utils/macros
    src/io
    src/synthesizer
)

zephyr_library_include_directories(app PRIVATE
    ${ZEPHYR_NRF_MODULE_DIR}/boards/arm/nrf5340_audio_dk_nrf5340)

# Application sources
add_subdirectory(src/audio)
add_subdirectory(src/bluetooth)
add_subdirectory(src/utils)
add_subdirectory(src/io)
add_subdirectory(src/synthesizer)

## LC3
# if (CONFIG_SW_CODEC_LC3_T2_SOFTWARE)
#     if (ZEPHYR_SDK_LC3_MODULE_DIR)
#         add_subdirectory(${ZEPHYR_SDK_LC3_MODULE_DIR} sdk_lc3_bin_dir)
#     else()
#         message(FATAL_ERROR "LC3 repository not found\n"
#                 "Path: ${ZEPHYR_SDK_LC3_MODULE_DIR}/CMakeLists.txt\n"
#                 "Run 'west config manifest.group-filter +nrf5340_audio' "
#                 "and then 'west update'.\n"
#                 "If access is denied, contact local Nordic Semiconductor "
#                 "support or use SBC codec")
#     endif()
# endif()