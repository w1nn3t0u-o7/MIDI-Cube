/**
 * @file midi_parser.h
 * @brief MIDI 1.0 Message Parser
 * 
 * Implements stateful MIDI byte stream parser with Running Status support
 * Based on MIDI 1.0 Detailed Specification v4.2.1
 */

#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include "midi_types.h"
#include "esp_err.h"

/**
 * @brief MIDI Parser State Machine
 * 
 * Maintains parser state for handling running status and multi-byte messages
 */
typedef struct {
    /* Running Status Buffer */
    uint8_t running_status;        /**< Last channel voice/mode status byte */
    
    /* Data byte collection */
    uint8_t data_bytes[2];         /**< Collected data bytes */
    uint8_t data_index;            /**< Current data byte index */
    uint8_t expected_data_bytes;   /**< Expected number of data bytes */
    
    /* System Exclusive handling */
    bool in_sysex;                 /**< Currently receiving SysEx */
    uint8_t *sysex_buffer;         /**< SysEx data buffer */
    uint16_t sysex_index;          /**< Current SysEx buffer position */
    uint16_t sysex_buffer_size;    /**< Size of SysEx buffer */
    
    /* Active Sensing */
    bool active_sensing_enabled;   /**< Active sensing detected */
    uint32_t last_message_time_us; /**< Last message timestamp */
    
    /* Statistics */
    uint32_t messages_parsed;      /**< Total messages parsed */
    uint32_t parse_errors;         /**< Parse error count */
} midi_parser_state_t;

/**
 * @brief Initialize MIDI parser state
 * 
 * @param state Pointer to parser state structure
 * @param sysex_buffer Optional buffer for SysEx data (NULL to disable SysEx)
 * @param sysex_buffer_size Size of SysEx buffer
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if state is NULL
 */
esp_err_t midi_parser_init(midi_parser_state_t *state, 
                           uint8_t *sysex_buffer, 
                           uint16_t sysex_buffer_size);

/**
 * @brief Reset parser state to initial conditions
 * 
 * Clears running status, data buffers, and SysEx state
 * 
 * @param state Pointer to parser state
 * @return ESP_OK on success
 */
esp_err_t midi_parser_reset(midi_parser_state_t *state);

/**
 * @brief Parse a single MIDI byte
 * 
 * Implements MIDI 1.0 byte stream parsing with full running status support.
 * Real-time messages can be received at any time and don't affect running status.
 * 
 * @param state Pointer to parser state
 * @param byte MIDI byte to parse
 * @param msg Pointer to message structure to fill (when complete)
 * @param message_complete Set to true when a complete message is available
 * @return ESP_OK on success, error code on failure
 */
esp_err_t midi_parser_parse_byte(midi_parser_state_t *state,
                                 uint8_t byte,
                                 midi_message_t *msg,
                                 bool *message_complete);

/**
 * @brief Check for Active Sensing timeout
 * 
 * Should be called periodically if Active Sensing is enabled
 * 
 * @param state Pointer to parser state
 * @param current_time_us Current timestamp in microseconds
 * @return true if Active Sensing timeout occurred
 */
bool midi_parser_check_active_sensing_timeout(midi_parser_state_t *state, 
                                               uint32_t current_time_us);

/**
 * @brief Get expected data byte count for a status byte
 * 
 * @param status MIDI status byte
 * @return Number of expected data bytes (0, 1, or 2)
 */
uint8_t midi_get_data_byte_count(uint8_t status);

/**
 * @brief Check if byte is a status byte
 * 
 * @param byte Byte to check
 * @return true if MSB is set (status byte)
 */
static inline bool midi_is_status_byte(uint8_t byte) {
    return (byte & MIDI_STATUS_BIT_MASK) != 0;
}

/**
 * @brief Check if byte is a data byte
 * 
 * @param byte Byte to check
 * @return true if MSB is clear (data byte)
 */
static inline bool midi_is_data_byte(uint8_t byte) {
    return (byte & MIDI_STATUS_BIT_MASK) == 0;
}

/**
 * @brief Check if status is a Real-Time message
 * 
 * @param status Status byte
 * @return true if Real-Time message (0xF8-0xFF)
 */
static inline bool midi_is_realtime_message(uint8_t status) {
    return status >= MIDI_STATUS_TIMING_CLOCK;
}

/**
 * @brief Check if status is a System Common message
 * 
 * @param status Status byte
 * @return true if System Common message (0xF0-0xF7)
 */
static inline bool midi_is_system_common_message(uint8_t status) {
    return (status >= MIDI_STATUS_SYSEX_START && 
            status <= MIDI_STATUS_SYSEX_END);
}

/**
 * @brief Check if status is a Channel Voice/Mode message
 * 
 * @param status Status byte
 * @return true if Channel message (0x80-0xEF)
 */
static inline bool midi_is_channel_message(uint8_t status) {
    return (status >= MIDI_STATUS_NOTE_OFF && 
            status < MIDI_STATUS_SYSEX_START);
}

#endif /* MIDI_PARSER_H */

