/**
 * @file midi_message.c
 * @brief MIDI Message Creation and Manipulation Functions
 * 
 * Works with simplified midi_message_t structure using bytes[] array
 */

#include "midi_message.h"
#include "midi_defs.h"
#include <string.h>

//=============================================================================
// Message Serialization
//=============================================================================

/**
 * @brief Get expected message length based on status byte
 */
static inline uint8_t get_message_length(uint8_t status) {
    uint8_t type = status & 0xF0;
    
    // System Real-Time (1 byte)
    if (status >= 0xF8) {
        return 1;
    }
    
    // System Common
    if (status >= 0xF0) {
        switch (status) {
            case 0xF0:  // SysEx Start (variable)
            case 0xF7:  // SysEx End (variable)
                return 0;  // Variable length, handled separately
            case 0xF1:  // MTC Quarter Frame
            case 0xF3:  // Song Select
                return 2;
            case 0xF2:  // Song Position
                return 3;
            case 0xF6:  // Tune Request
                return 1;
            default:
                return 1;
        }
    }
    
    // Channel Voice Messages
    switch (type) {
        case 0x80:  // Note Off
        case 0x90:  // Note On
        case 0xA0:  // Poly Pressure
        case 0xB0:  // Control Change
        case 0xE0:  // Pitch Bend
            return 3;  // Status + 2 data bytes
            
        case 0xC0:  // Program Change
        case 0xD0:  // Channel Pressure
            return 2;  // Status + 1 data byte
            
        default:
            return 1;
    }
}

/**
 * @brief Serialize MIDI message to byte array
 */
esp_err_t midi_message_to_bytes(const midi_message_t *msg,
                                uint8_t *buffer,
                                size_t buffer_size,
                                size_t *bytes_written) {
    if (!msg || !buffer || !bytes_written) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *bytes_written = 0;
    
    //-------------------------------------------------------------------------
    // System Exclusive (variable length)
    //-------------------------------------------------------------------------
    // if (msg->type == MIDI_MSG_TYPE_SYSTEM_EXCLUSIVE) {
    //     size_t total_size = 2 + msg->data.sysex.length; // 0xF0 + data + 0xF7
        
    //     if (buffer_size < total_size) {
    //         return ESP_ERR_NO_MEM;
    //     }
        
    //     buffer[0] = 0xF0;  // SysEx start
    //     if (msg->data.sysex.data && msg->data.sysex.length > 0) {
    //         memcpy(&buffer[1], msg->data.sysex.data, msg->data.sysex.length);
    //     }
    //     buffer[1 + msg->data.sysex.length] = 0xF7;  // SysEx end
        
    //     *bytes_written = total_size;
    //     return ESP_OK;
    // }
    
    //-------------------------------------------------------------------------
    // All other messages (status + up to 2 data bytes)
    //-------------------------------------------------------------------------
    uint8_t msg_length = get_message_length(msg->status_byte);
    
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
