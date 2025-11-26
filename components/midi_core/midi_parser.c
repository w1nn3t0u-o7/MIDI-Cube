/**
 * @file midi_parser.c
 * @brief MIDI 1.0 Parser Implementation
 * 
 * Implements complete MIDI 1.0 byte stream parsing with:
 * - Running Status support
 * - Real-Time message handling
 * - System Exclusive parsing
 */

#include "esp_log.h"
#include "esp_timer.h"

#include "midi_parser.h"

static const char *TAG = "midi_parser";

esp_err_t midi1_parser_init(midi_parser_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(state, 0, sizeof(midi_parser_state_t));
    
    ESP_LOGD(TAG, "MIDI parser initialized");
    
    return ESP_OK;
}

esp_err_t midi1_parser_reset(midi_parser_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    state->running_status = 0;
    state->data_index = 0;
    state->expected_data_bytes = 0;
    state->in_sysex = false;
    
    ESP_LOGD(TAG, "Parser state reset");
    
    return ESP_OK;
}

esp_err_t midi1_parser_parse_byte(midi_parser_state_t *state, uint8_t byte,
                                 midi_message_t *msg, bool *message_complete)
{
    if (!state || !msg || !message_complete) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *message_complete = false;
    
    /* === SYSTEM REAL-TIME MESSAGES (0xF8-0xFF) === */
    /* Real-Time messages can occur at ANY time, even between status 
     * and data bytes. They must be processed immediately without 
     * affecting running status or current message assembly. 
     * Spec: Page 30, "System Real Time Messages" */
    if (midi1_is_realtime_message(byte)) {
        memset(msg, 0, sizeof(midi_message_t));
        
        msg->status_byte = byte;
        *message_complete = true;
        
        /* Running status and data collection NOT affected */
        return ESP_OK;
    }
    
    /* === STATUS BYTES (0x80-0xF7) === */
    if (midi1_is_status_byte(byte)) {
        
        /* === SYSTEM EXCLUSIVE START (0xF0) === */
        if (byte == MIDI1_STATUS_SYSEX_START) {
            state->in_sysex = true;
            state->running_status = 0;  // Clear running status (spec page 5)
            ESP_LOGW(TAG, "SysEx Start received. SysEx handling not implemented.");
            return ESP_ERR_NOT_SUPPORTED;
        }
        
        /* === SYSTEM EXCLUSIVE END (0xF7) === */
        if (byte == MIDI1_STATUS_SYSEX_END) {
            if (state->in_sysex) {
                state->in_sysex = false;
                
                /* Create SysEx message */
                memset(msg, 0, sizeof(midi_message_t));
                msg->status_byte = MIDI1_STATUS_SYSEX_START;
                
                *message_complete = true;
                
                ESP_LOGW(TAG, "SysEx End received. SysEx handling not implemented.");
            }
            return ESP_ERR_NOT_SUPPORTED;
        }
        
        /* === SYSTEM COMMON MESSAGES (0xF1-0xF6) === */
        /* System Common messages clear running status (spec page 5) */
        if (midi1_is_system_common_message(byte)) {
            state->in_sysex = false;  // Terminate SysEx if active
            state->running_status = 0;  // Clear running status
            state->data_index = 0;
            state->expected_data_bytes = midi1_get_data_byte_count(byte);
            
            memset(msg, 0, sizeof(midi_message_t));
            msg->status_byte = byte;
            
            /* Single-byte System Common messages */
            if (state->expected_data_bytes == 0) {
                *message_complete = true;
            }
            
            return ESP_OK;
        }
        
        /* === CHANNEL VOICE/MODE MESSAGES (0x80-0xEF) === */
        if (midi1_is_channel_message(byte)) {
            state->in_sysex = false;  // Terminate SysEx if active
            state->running_status = byte;  // Store for running status
            state->data_index = 0;
            state->expected_data_bytes = midi1_get_data_byte_count(byte);
            
            msg->status_byte = byte;
            
            return ESP_OK;
        }
        
        /* Undefined status bytes should be ignored (spec page 6) */
        ESP_LOGW(TAG, "Undefined status byte: 0x%02X", byte);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    /* === DATA BYTES (0x00-0x7F) === */
    if (midi1_is_data_byte(byte)) {
        
        /* Handle SysEx data */
        if (state->in_sysex) {
            ESP_LOGW(TAG, "SysEx data byte received. SysEx handling not implemented.");
            return ESP_ERR_NOT_SUPPORTED;
        }
        
        /* Data byte without valid running status - ignore (spec page 6) */
        if (state->running_status == 0 && state->expected_data_bytes == 0) {
            ESP_LOGD(TAG, "Data byte 0x%02X ignored (no running status)", byte);
            return ESP_ERR_NOT_SUPPORTED;
        }
        
        /* Collect data bytes */
        if (state->data_index < 2) {
            state->data_bytes[state->data_index++] = byte;
        }
        
        /* Check if message is complete */
        if (state->data_index >= state->expected_data_bytes) {
            
            memset(msg, 0, sizeof(midi_message_t));
            /* Message complete - fill in structure */
            msg->status_byte = state->running_status;
            msg->data[0] = (state->expected_data_bytes >= 1) ? state->data_bytes[0] : 0;
            msg->data[1] = (state->expected_data_bytes >= 2) ? state->data_bytes[1] : 0;
            
            *message_complete = true;
            state->data_index = 0;  // Ready for next message with running status
            
            return ESP_OK;
        }
    }

    return ESP_OK;
}
