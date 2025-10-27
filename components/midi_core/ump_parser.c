#include "ump_types.h"
#include "ump_defs.h"
#include "ump_parser.h"
#include <string.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t ump_parser_parse_packet(const uint32_t *words, ump_packet_t *packet) {
    if (!words || !packet) return ESP_ERR_INVALID_ARG;

    uint8_t mt = UMP_GET_MT(words[0]);
    uint8_t num_words;

    switch (mt) {
        case UMP_MT_UTILITY:
        case UMP_MT_SYSTEM:
        case UMP_MT_MIDI1_CHANNEL_VOICE:
        case UMP_MT_RESERVED_6:
        case UMP_MT_RESERVED_7:
            num_words = 1; break;
        case UMP_MT_DATA_64:
        case UMP_MT_MIDI2_CHANNEL_VOICE:
        case UMP_MT_RESERVED_8:
        case UMP_MT_RESERVED_9:
        case UMP_MT_RESERVED_A:
            num_words = 2; break;
        case UMP_MT_RESERVED_B:
        case UMP_MT_RESERVED_C:
            num_words = 3; break;
        case UMP_MT_DATA_128:
        case UMP_MT_FLEX_DATA:
        case UMP_MT_UMP_STREAM:
        case UMP_MT_RESERVED_E:
            num_words = 4; break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    memcpy(packet->words, words, sizeof(uint32_t) * num_words);
    packet->num_words = num_words;
    packet->message_type = mt;
    packet->group = UMP_GET_GROUP(words[0]);
    packet->timestamp_us = 0; // Set externally if needed

    return ESP_OK;
}

esp_err_t ump_message_serialize(const ump_packet_t *packet, uint32_t *words_out, size_t words_out_count) {
    if (!packet || !words_out || (words_out_count < packet->num_words))
        return ESP_ERR_INVALID_ARG;
    memcpy(words_out, packet->words, sizeof(uint32_t) * packet->num_words);
    return ESP_OK;
}
