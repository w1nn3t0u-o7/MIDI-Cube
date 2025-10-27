#include "ump_types.h"
#include "ump_defs.h"
#include "ump_message.h"
#include "esp_err.h"

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

/**
 * @brief Build MIDI 2.0 Control Change (32-bit resolution)
 */
esp_err_t ump_build_midi2_control_change(
    uint8_t group, uint8_t channel, uint8_t controller,
    uint32_t value32,  // Full 32-bit resolution!
    ump_packet_t *packet_out)
{
    if (!packet_out || group > 15 || channel > 15 || controller > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t status = 0xB0 | (channel & 0x0F);
    
    // Word 0: MT + Group + Status + Controller + 0
    packet_out->words[0] = 
        ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
        ((uint32_t)group << 24) |
        ((uint32_t)status << 16) |
        ((uint32_t)controller << 8) |
        0x00;
    
    // Word 1: Full 32-bit controller value
    packet_out->words[1] = value32;
    
    packet_out->num_words = 2;
    packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
    packet_out->group = group;
    
    return ESP_OK;
}

/**
 * @brief Build MIDI 2.0 Pitch Bend (32-bit resolution)
 */
esp_err_t ump_build_midi2_pitch_bend(
    uint8_t group, uint8_t channel,
    uint32_t value32,  // 0x00000000 to 0xFFFFFFFF, center = 0x80000000
    ump_packet_t *packet_out)
{
    if (!packet_out || group > 15 || channel > 15) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t status = 0xE0 | (channel & 0x0F);
    
    packet_out->words[0] = 
        ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
        ((uint32_t)group << 24) |
        ((uint32_t)status << 16) |
        0x0000;
    
    packet_out->words[1] = value32;
    
    packet_out->num_words = 2;
    packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
    packet_out->group = group;
    
    return ESP_OK;
}

/**
 * @brief Build MIDI 2.0 Program Change with Bank Select
 */
esp_err_t ump_build_midi2_program_change(
    uint8_t group, uint8_t channel, uint8_t program,
    bool bank_valid, uint8_t bank_msb, uint8_t bank_lsb,
    ump_packet_t *packet_out)
{
    if (!packet_out || group > 15 || channel > 15 || program > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t status = 0xC0 | (channel & 0x0F);
    uint8_t options = bank_valid ? 0x01 : 0x00;
    
    packet_out->words[0] = 
        ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
        ((uint32_t)group << 24) |
        ((uint32_t)status << 16) |
        ((uint32_t)program << 8) |
        options;
    
    packet_out->words[1] = 
        ((uint32_t)bank_msb << 8) |
        ((uint32_t)bank_lsb << 0);
    
    packet_out->num_words = 2;
    packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
    packet_out->group = group;
    
    return ESP_OK;
}
