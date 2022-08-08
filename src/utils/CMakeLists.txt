#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pcm_stream_channel_modifier.c
    ${CMAKE_CURRENT_SOURCE_DIR}/data_fifo.c
)
