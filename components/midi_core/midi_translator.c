#include "midi_types.h"
#include "midi_defs.h"
#include "ump_types.h"
#include "ump_message.h"
#include "midi_translator.h"

// MIDI 1.0 (7-bit) to 16-bit (MIDI 2.0) min-center-max upscaling
uint16_t midi_upscale_7to16(uint8_t value7) {
    if (value7 == 0) return 0;
    if (value7 == 64) return 32768;
    if (value7 >= 127) return 65535;
    if (value7 < 64)
        return (uint16_t)(((uint32_t)value7 * 32767) / 63);
    return (uint16_t)(32768 + (((uint32_t)(value7 - 64) * 32767) / 63));
}

// MIDI 1.0 (14-bit) to 32-bit (MIDI 2.0)
uint32_t midi_upscale_14to32(uint16_t value14) {
    if (value14 == 0) return 0;
    if (value14 == 8192) return 0x80000000; // Center value for 32-bit
    if (value14 >= 16383) return 0xFFFFFFFF;
    if (value14 < 8192)
        return ((uint32_t)value14 * 0x7FFFFFFF) / 8191;
    return 0x80000000 + (((uint32_t)(value14 - 8192) * 0x7FFFFFFF) / 8191);
}

// Downscale (MIDI 2.0 to MIDI 1.0)
uint8_t midi_downscale_16to7(uint16_t value16) {
    return value16 >> 9; // 16->7 bits: shift right by 16-7=9
}
uint16_t midi_downscale_32to14(uint32_t value32) {
    return value32 >> 18; // 32->14 bits: shift by 18
}

// MIDI 1.0 → UMP (MT 0x2 or 0x4 depending on translation mode)
esp_err_t midi_translate_1to2(const midi_message_t *msg, ump_packet_t *packet) {
    if (!msg || !packet) return ESP_ERR_INVALID_ARG;
    // Example: Note On translation only; expand for full coverage as needed
    if ((msg->status & 0xF0) == MIDI_STATUS_NOTE_ON) {
        // Upscale velocity
        uint16_t velocity16 = midi_upscale_7to16(msg->data2);
        return ump_build_midi2_note_on(0, msg->channel, msg->data1, velocity16, 0, 0, packet);
    }
    // Add translation for other message types here...
    return ESP_ERR_NOT_SUPPORTED;
}

// UMP (MIDI 2.0, Note On) → MIDI 1.0
esp_err_t midi_translate_2to1(const ump_packet_t *packet, midi_message_t *msg) {
    if (!packet || !msg) return ESP_ERR_INVALID_ARG;
    if (packet->message_type == UMP_MT_MIDI2_CHANNEL_VOICE) {
        uint32_t word0 = packet->words[0];
        uint32_t word1 = packet->words[1];
        uint8_t status = (word0 >> 16) & 0xFF;
        uint8_t channel = (word0 >> 8) & 0xF;
        uint8_t note = (word0 >> 8) & 0xFF;
        uint16_t velocity16 = word1 >> 16;
        uint8_t velocity7 = midi_downscale_16to7(velocity16);
        msg->status = MIDI_STATUS_NOTE_ON | channel;
        msg->channel = channel;
        msg->data1 = note;
        msg->data2 = velocity7;
        msg->length = 3;
        return ESP_OK;
    }
    // Add more as needed
    return ESP_ERR_NOT_SUPPORTED;
}
