#include "ump_defs.h"
#include "ump_types.h"
#include "esp_err.h"

esp_err_t ump_build_midi2_note_on(
    uint8_t group, uint8_t channel, uint8_t note,
    uint16_t velocity16, uint8_t attr_type, uint16_t attr_data,
    ump_packet_t *packet_out);