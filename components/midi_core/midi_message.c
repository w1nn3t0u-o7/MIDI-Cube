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
// Message Creation Functions
//=============================================================================

/**
 * @brief Create a Note On message
 */
esp_err_t midi_create_note_on(midi_message_t *msg, uint8_t channel,
                               uint8_t note, uint8_t velocity) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate parameters
    if (channel > 15 || note > 127 || velocity > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear message
    memset(msg, 0, sizeof(midi_message_t));
    
    // Populate message
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0x90 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = note;
    msg->data.bytes[1] = velocity;
    
    return ESP_OK;
}

/**
 * @brief Create a Note Off message
 */
esp_err_t midi_create_note_off(midi_message_t *msg, uint8_t channel,
                                uint8_t note, uint8_t velocity) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel > 15 || note > 127 || velocity > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(midi_message_t));
    
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0x80 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = note;
    msg->data.bytes[1] = velocity;
    
    return ESP_OK;
}

/**
 * @brief Create a Control Change message
 */
esp_err_t midi_create_control_change(midi_message_t *msg, uint8_t channel,
                                      uint8_t controller, uint8_t value) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel > 15 || controller > 127 || value > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(midi_message_t));
    
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0xB0 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = controller;
    msg->data.bytes[1] = value;
    
    return ESP_OK;
}

/**
 * @brief Create a Program Change message
 */
esp_err_t midi_create_program_change(midi_message_t *msg, uint8_t channel,
                                      uint8_t program) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel > 15 || program > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(midi_message_t));
    
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0xC0 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = program;
    msg->data.bytes[1] = 0;  // Unused
    
    return ESP_OK;
}

/**
 * @brief Create a Pitch Bend message
 */
esp_err_t midi_create_pitch_bend(midi_message_t *msg, uint8_t channel,
                                  uint16_t value) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel > 15 || value > 16383) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(midi_message_t));
    
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0xE0 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = value & 0x7F;        // LSB
    msg->data.bytes[1] = (value >> 7) & 0x7F; // MSB
    
    return ESP_OK;
}

/**
 * @brief Create Channel Pressure message
 */
esp_err_t midi_create_channel_pressure(midi_message_t *msg, uint8_t channel,
                                        uint8_t pressure) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel > 15 || pressure > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(midi_message_t));
    
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0xD0 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = pressure;
    msg->data.bytes[1] = 0;  // Unused
    
    return ESP_OK;
}

/**
 * @brief Create Polyphonic Key Pressure message
 */
esp_err_t midi_create_poly_pressure(midi_message_t *msg, uint8_t channel,
                                     uint8_t note, uint8_t pressure) {
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel > 15 || note > 127 || pressure > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(midi_message_t));
    
    msg->type = MIDI_MSG_TYPE_CHANNEL;
    msg->status = 0xA0 | channel;
    msg->channel = channel;
    msg->data.bytes[0] = note;
    msg->data.bytes[1] = pressure;
    
    return ESP_OK;
}

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
    if (msg->type == MIDI_MSG_TYPE_SYSTEM_EXCLUSIVE) {
        size_t total_size = 2 + msg->data.sysex.length; // 0xF0 + data + 0xF7
        
        if (buffer_size < total_size) {
            return ESP_ERR_NO_MEM;
        }
        
        buffer[0] = 0xF0;  // SysEx start
        if (msg->data.sysex.data && msg->data.sysex.length > 0) {
            memcpy(&buffer[1], msg->data.sysex.data, msg->data.sysex.length);
        }
        buffer[1 + msg->data.sysex.length] = 0xF7;  // SysEx end
        
        *bytes_written = total_size;
        return ESP_OK;
    }
    
    //-------------------------------------------------------------------------
    // All other messages (status + up to 2 data bytes)
    //-------------------------------------------------------------------------
    uint8_t msg_length = get_message_length(msg->status);
    
    if (buffer_size < msg_length) {
        return ESP_ERR_NO_MEM;
    }
    
    // Write status byte
    buffer[0] = msg->status;
    
    // Write data bytes (if any)
    if (msg_length > 1) {
        buffer[1] = msg->data.bytes[0];
    }
    if (msg_length > 2) {
        buffer[2] = msg->data.bytes[1];
    }
    
    *bytes_written = msg_length;
    return ESP_OK;
}

//=============================================================================
// Message Type Checking
//=============================================================================

/**
 * @brief Check if message is Note On (with velocity > 0)
 */
bool midi_is_note_on(const midi_message_t *msg) {
    if (!msg) {
        return false;
    }
    
    // Check for Note On status (0x90-0x9F)
    uint8_t status_type = msg->status & 0xF0;
    if (status_type == 0x90) {
        // Must have velocity > 0 (data.bytes[1] = velocity)
        return msg->data.bytes[1] > 0;
    }
    
    return false;
}

/**
 * @brief Check if message is Note Off (or Note On with velocity 0)
 */
bool midi_is_note_off(const midi_message_t *msg) {
    if (!msg) {
        return false;
    }
    
    uint8_t status_type = msg->status & 0xF0;
    
    // Explicit Note Off (0x80-0x8F)
    if (status_type == 0x80) {
        return true;
    }
    
    // Note On with velocity 0 (0x90-0x9F with vel=0)
    if (status_type == 0x90 && msg->data.bytes[1] == 0) {
        return true;
    }
    
    return false;
}

//=============================================================================
// Human-Readable Strings
//=============================================================================

/**
 * @brief Get human-readable message type string
 */
const char* midi_get_message_type_string(const midi_message_t *msg) {
    if (!msg) {
        return "NULL";
    }
    
    // System Real-Time
    if (msg->type == MIDI_MSG_TYPE_SYSTEM_REALTIME) {
        switch (msg->status) {
            case 0xF8: return "Timing Clock";
            case 0xFA: return "Start";
            case 0xFB: return "Continue";
            case 0xFC: return "Stop";
            case 0xFE: return "Active Sensing";
            case 0xFF: return "System Reset";
            default: return "Unknown Real-Time";
        }
    }
    
    // System Exclusive
    if (msg->type == MIDI_MSG_TYPE_SYSTEM_EXCLUSIVE) {
        return "System Exclusive";
    }
    
    // System Common
    if (msg->type == MIDI_MSG_TYPE_SYSTEM_COMMON) {
        switch (msg->status) {
            case 0xF1: return "MTC Quarter Frame";
            case 0xF2: return "Song Position";
            case 0xF3: return "Song Select";
            case 0xF6: return "Tune Request";
            default: return "Unknown System Common";
        }
    }
    
    // Channel Voice Messages
    if (msg->type == MIDI_MSG_TYPE_CHANNEL) {
        uint8_t status_type = msg->status & 0xF0;
        switch (status_type) {
            case 0x80: return "Note Off";
            case 0x90: 
                // Check velocity (bytes[1])
                return (msg->data.bytes[1] > 0) ? "Note On" : "Note Off (vel=0)";
            case 0xA0: return "Poly Pressure";
            case 0xB0: 
                // Check if Channel Mode (CC 120-127)
                return (msg->data.bytes[0] >= 120) ? "Channel Mode" : "Control Change";
            case 0xC0: return "Program Change";
            case 0xD0: return "Channel Pressure";
            case 0xE0: return "Pitch Bend";
            default: return "Unknown Channel Message";
        }
    }
    
    return "Unknown";
}
