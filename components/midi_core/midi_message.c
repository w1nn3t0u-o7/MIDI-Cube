/**
 * @file midi_message.c
 * @brief MIDI Message Creation and Manipulation Functions
 * 
 * Works with simplified midi_message_t structure using bytes[] array
 */

#include <string.h>

#include "midi_defs.h"
#include "midi_message.h"

uint8_t midi1_get_data_byte_count(uint8_t status) 
{
    uint8_t status_type = status & MIDI1_STATUS_TYPE_MASK;
    
    /* Channel Voice/Mode Messages */
    if (midi1_is_channel_message(status)) {
        switch (status_type) {
            case MIDI1_STATUS_PROGRAM_CHANGE:
            case MIDI1_STATUS_CHANNEL_PRESSURE:
                return 1;  // Single data byte
            default:
                return 2;  // Two data bytes
        }
    }
    
    /* System Common Messages */
    if (midi1_is_system_common_message(status)) {
        switch (status) {
            case MIDI1_STATUS_MTC_QUARTER_FRAME:
            case MIDI1_STATUS_SONG_SELECT:
                return 1;
            case MIDI1_STATUS_SONG_POSITION:
                return 2;
            case MIDI1_STATUS_TUNE_REQUEST:
            case MIDI1_STATUS_SYSEX_END:
                return 0;
            case MIDI1_STATUS_SYSEX_START:
                return 0;  // Variable length
            default:
                return 0;
        }
    }
    
    /* Real-Time Messages (0xF8-0xFF) */
    return 0;  // No data bytes
}

uint8_t midi1_get_message_length(uint8_t status) 
{
    return midi1_get_data_byte_count(status) + 1;  // +1 for status byte
}

esp_err_t midi1_message_to_bytes(const midi_message_t *msg, uint8_t *buffer,
                                size_t buffer_size, size_t *bytes_written)
{
    if (!msg || !buffer || !bytes_written) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *bytes_written = 0;
    uint8_t msg_length = midi1_get_message_length(msg->status_byte);
    
    if (buffer_size < msg_length) {
        return ESP_ERR_NO_MEM;
    }
    
    // Write status byte
    buffer[0] = msg->status_byte;
    
    // Write data bytes (if any)
    if (msg_length > 1) {
        buffer[1] = msg->data[0];
    }
    if (msg_length > 2) {
        buffer[2] = msg->data[1];
    }
    
    *bytes_written = msg_length;
    return ESP_OK;
}
