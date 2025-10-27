idf_component_register(
    SRCS "midi_translator.c" "ump_message.c" "ump_parser.c" "midi_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES log esp_timer
)
