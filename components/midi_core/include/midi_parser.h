/**
 * @file midi_parser.h
 * @brief MIDI 1.0 Message Parser
 * 
 * Implements stateful MIDI byte stream parser with Running Status support
 * Based on MIDI 1.0 Detailed Specification v4.2.1
 */

#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include "esp_err.h"

#include "midi_defs.h"
#include "midi_message.h"

/**
 * @brief MIDI Parser State Machine
 * 
 * Maintains parser state for handling running status and multi-byte messages
 */
typedef struct midi_parser_state {
    /* Running Status Buffer */
    uint8_t running_status;        /**< Last channel voice/mode status byte */
    
    /* Data byte collection */
    uint8_t data_bytes[2];         /**< Collected data bytes */
    uint8_t data_index;            /**< Current data byte index */
    uint8_t expected_data_bytes;   /**< Expected number of data bytes */
    
    /* System Exclusive handling */
    bool in_sysex;                 /**< Currently receiving SysEx */
} midi_parser_state_t;

/**
 * @brief Initialize MIDI parser state
 * 
 * @param state Pointer to parser state structure
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if state is NULL
 */
esp_err_t midi1_parser_init(midi_parser_state_t *state);

/**
 * @brief Reset parser state to initial conditions
 * 
 * Clears running status, data buffers, and SysEx state
 * 
 * @param state Pointer to parser state
 * @return ESP_OK on success
 */
esp_err_t midi1_parser_reset(midi_parser_state_t *state);

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
esp_err_t midi1_parser_parse_byte(midi_parser_state_t *state, uint8_t byte,
                                 midi_message_t *msg, bool *message_complete);

#endif /* MIDI_PARSER_H */

