idf_component_register(
    SRCS "test_midi_core.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES midi_core midi_uart midi_router
)