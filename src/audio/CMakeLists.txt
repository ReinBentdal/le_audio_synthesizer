#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/audio_process.c
	${CMAKE_CURRENT_SOURCE_DIR}/audio_sync_timer.c
	${CMAKE_CURRENT_SOURCE_DIR}/stream_control.c
	${CMAKE_CURRENT_SOURCE_DIR}/sw_codec.c
	${CMAKE_CURRENT_SOURCE_DIR}/tick_provider.c
)
