/**
 * @file midi_parser.c
 * @brief MIDI 1.0 Parser Implementation
 * 
 * Implements complete MIDI 1.0 byte stream parsing with:
 * - Running Status support
 * - Real-Time message handling
 * - System Exclusive parsing
 */

#include "midi_parser.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "midi_parser";

/**
 * @brief Get expected data byte count for status byte
 */
uint8_t midi_get_data_byte_count(uint8_t status) {
    uint8_t status_type = status & MIDI_STATUS_TYPE_MASK;
    
    /* Channel Voice/Mode Messages */
    if (midi_is_channel_message(status)) {
        switch (status_type) {
            case MIDI_STATUS_PROGRAM_CHANGE:
            case MIDI_STATUS_CHANNEL_PRESSURE:
                return 1;  // Single data byte
            default:
                return 2;  // Two data bytes
        }
    }
    
    /* System Common Messages */
    if (midi_is_system_common_message(status)) {
        switch (status) {
            case MIDI_STATUS_MTC_QUARTER_FRAME:
            case MIDI_STATUS_SONG_SELECT:
                return 1;
            case MIDI_STATUS_SONG_POSITION:
                return 2;
            case MIDI_STATUS_TUNE_REQUEST:
            case MIDI_STATUS_SYSEX_END:
                return 0;
            case MIDI_STATUS_SYSEX_START:
                return 0;  // Variable length
            default:
                return 0;
        }
    }
    
    /* Real-Time Messages (0xF8-0xFF) */
    return 0;  // No data bytes
}

/**
 * @brief Initialize parser state
 */
esp_err_t midi_parser_init(midi_parser_state_t *state,
                           uint8_t *sysex_buffer,
                           uint16_t sysex_buffer_size) {
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(midi_parser_state_t));
    
    state->sysex_buffer = sysex_buffer;
    state->sysex_buffer_size = sysex_buffer_size;
    
    ESP_LOGI(TAG, "MIDI parser initialized (SysEx buffer: %u bytes)",
             sysex_buffer_size);
    
    return ESP_OK;
}

/**
 * @brief Reset parser to initial state
 */
esp_err_t midi_parser_reset(midi_parser_state_t *state) {
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    state->running_status = 0;
    state->data_index = 0;
    state->expected_data_bytes = 0;
    state->in_sysex = false;
    state->sysex_index = 0;
    
    ESP_LOGD(TAG, "Parser state reset");
    
    return ESP_OK;
}

/**
 * @brief Parse incoming MIDI byte
 * 
 * Full implementation based on MIDI 1.0 spec section on Running Status
 * and message parsing (pages 5-6, A-2 to A-3 of spec)
 */
esp_err_t midi_parser_parse_byte(midi_parser_state_t *state,
                                 uint8_t byte,
                                 midi_message_t *msg,
                                 bool *message_complete) {
    if (!state || !msg || !message_complete) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *message_complete = false;
    
    /* === SYSTEM REAL-TIME MESSAGES (0xF8-0xFF) === */
    /* Real-Time messages can occur at ANY time, even between status 
     * and data bytes. They must be processed immediately without 
     * affecting running status or current message assembly. 
     * Spec: Page 30, "System Real Time Messages" */
    if (midi_is_realtime_message(byte)) {
        msg->type = MIDI_MSG_TYPE_SYSTEM_REALTIME;
        msg->status = byte;
        
        *message_complete = true;
        state->messages_parsed++;
        
        /* Running status and data collection NOT affected */
        return ESP_OK;
    }
    
    /* === STATUS BYTES (0x80-0xF7) === */
    if (midi_is_status_byte(byte)) {
        
        /* === SYSTEM EXCLUSIVE START (0xF0) === */
        if (byte == MIDI_STATUS_SYSEX_START) {
            state->in_sysex = true;
            state->sysex_index = 0;
            state->running_status = 0;  // Clear running status (spec page 5)
            ESP_LOGD(TAG, "SysEx Start");
            return ESP_OK;
        }
        
        /* === SYSTEM EXCLUSIVE END (0xF7) === */
        if (byte == MIDI_STATUS_SYSEX_END) {
            if (state->in_sysex) {
                state->in_sysex = false;
                
                /* Create SysEx message */
                msg->type = MIDI_MSG_TYPE_SYSTEM_EXCLUSIVE;
                msg->status = MIDI_STATUS_SYSEX_START;
                msg->data.sysex.data = state->sysex_buffer;
                msg->data.sysex.length = state->sysex_index;
                
                *message_complete = true;
                state->messages_parsed++;
                
                ESP_LOGD(TAG, "SysEx End (%u bytes)", state->sysex_index);
            }
            return ESP_OK;
        }
        
        /* === SYSTEM COMMON MESSAGES (0xF1-0xF6) === */
        /* System Common messages clear running status (spec page 5) */
        if (midi_is_system_common_message(byte)) {
            state->in_sysex = false;  // Terminate SysEx if active
            state->running_status = 0;  // Clear running status
            state->data_index = 0;
            state->expected_data_bytes = midi_get_data_byte_count(byte);
            
            msg->type = MIDI_MSG_TYPE_SYSTEM_COMMON;
            msg->status = byte;
            
            /* Single-byte System Common messages */
            if (state->expected_data_bytes == 0) {
                *message_complete = true;
                state->messages_parsed++;
            }
            
            return ESP_OK;
        }
        
        /* === CHANNEL VOICE/MODE MESSAGES (0x80-0xEF) === */
        if (midi_is_channel_message(byte)) {
            state->in_sysex = false;  // Terminate SysEx if active
            state->running_status = byte;  // Store for running status
            state->data_index = 0;
            state->expected_data_bytes = midi_get_data_byte_count(byte);
            
            msg->type = MIDI_MSG_TYPE_CHANNEL;
            msg->status = byte;
            msg->channel = byte & MIDI_CHANNEL_MASK;
            
            return ESP_OK;
        }
        
        /* Undefined status bytes should be ignored (spec page 6) */
        ESP_LOGW(TAG, "Undefined status byte: 0x%02X", byte);
        state->parse_errors++;
        return ESP_OK;
    }
    
    /* === DATA BYTES (0x00-0x7F) === */
    if (midi_is_data_byte(byte)) {
        
        /* Handle SysEx data */
        if (state->in_sysex) {
            if (state->sysex_buffer && state->sysex_index < state->sysex_buffer_size) {
                state->sysex_buffer[state->sysex_index++] = byte;
            } else if (!state->sysex_buffer) {
                ESP_LOGW(TAG, "SysEx data received but no buffer allocated");
            } else {
                ESP_LOGW(TAG, "SysEx buffer overflow");
                state->parse_errors++;
            }
            return ESP_OK;
        }
        
        /* Data byte without valid running status - ignore (spec page 6) */
        if (state->running_status == 0 && state->expected_data_bytes == 0) {
            ESP_LOGD(TAG, "Data byte 0x%02X ignored (no running status)", byte);
            return ESP_OK;
        }
        
        /* Collect data bytes */
        if (state->data_index < 2) {
            state->data_bytes[state->data_index++] = byte;
        }
        
        /* Check if message is complete */
        if (state->data_index >= state->expected_data_bytes) {
            /* Message complete - fill in structure */
            msg->status = state->running_status;
            msg->channel = state->running_status & MIDI_CHANNEL_MASK;
            msg->data.bytes[0] = (state->expected_data_bytes >= 1) ? state->data_bytes[0] : 0;
            msg->data.bytes[1] = (state->expected_data_bytes >= 2) ? state->data_bytes[1] : 0;
            
            *message_complete = true;
            state->messages_parsed++;
            state->data_index = 0;  // Ready for next message with running status
            
            return ESP_OK;
        }
    }
    
    return ESP_OK;
}

