

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/oscillator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/effect_modulation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/effect_envelope.c
    ${CMAKE_CURRENT_SOURCE_DIR}/effect_echo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_allpass.c
)