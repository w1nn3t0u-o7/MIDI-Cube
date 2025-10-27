#include "ump_types.h"
#include "ump_defs.h"

esp_err_t ump_build_midi2_note_on(
    uint8_t group, uint8_t channel, uint8_t note,
    uint16_t velocity16, uint8_t attr_type, uint16_t attr_data,
    ump_packet_t *packet_out)
{
    if (!packet_out || group > 0x0F || channel > 0x0F || note > 0x7F) return ESP_ERR_INVALID_ARG;
    // MT=0x4, group
    uint32_t word0 = (UMP_MT_MIDI2_CHANNEL_VOICE << 28)
        | ((group & 0x0F) << 24)
        | (0x90 << 16)
        | ((channel & 0x0F) << 16)
        | (note << 8);
    // High 16 bits: velocity; low 8 bits: attr type, attr data
    uint32_t word1 = ((uint32_t)velocity16 << 16)
        | ((uint32_t)attr_type << 8)
        | (attr_data & 0xFFFF);

    packet_out->words[0] = word0;
    packet_out->words[1] = word1;
    packet_out->num_words = 2;
    packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
    packet_out->group = group;
    packet_out->timestamp_us = 0;
    return ESP_OK;
}

// Similar helpers can be written for Note Off, Poly Pressure, CC, Program Change etc.

