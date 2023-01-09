

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/synthesizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/instrument.c
    ${CMAKE_CURRENT_SOURCE_DIR}/key_assign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/arpeggio.c
)

add_subdirectory(dsp)