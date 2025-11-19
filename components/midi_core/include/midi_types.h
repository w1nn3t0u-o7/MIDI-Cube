/**
 * @file midi_types.h
 * @brief MIDI Data Type Definitions
 * 
 * Core MIDI message structures and enumerations based on MIDI 1.0 specification
 */

#ifndef MIDI_TYPES_H
#define MIDI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief MIDI Message Type Classification
 */
typedef enum {
    MIDI_MSG_TYPE_CHANNEL,            /**< Channel Message */
    MIDI_MSG_TYPE_SYSTEM_COMMON,      /**< System Common Message */
    MIDI_MSG_TYPE_SYSTEM_REALTIME,    /**< System Real-Time Message */
    MIDI_MSG_TYPE_SYSTEM_EXCLUSIVE,   /**< System Exclusive Message */
    MIDI_MSG_TYPE_UNKNOWN             /**< Unknown/Invalid Message */
} midi_message_type_t;

/**
 * @brief Complete MIDI 1.0 Message Structure
 * 
 * Represents a complete, parsed MIDI message with all relevant data
 */
typedef struct midi_message {
    /* Message Classification */
    midi_message_type_t type;      /**< Message type classification */
    uint8_t status;                /**< Full status byte (including channel) */
    uint8_t channel;               /**< MIDI channel (0-15, representing 1-16) */
    
    union {
        uint8_t bytes[2];            /**< Data bytes (up to 2 for most messages) */

        /* System Exclusive */
        struct {
            uint8_t manufacturer_id; /**< Manufacturer ID (1 or 3 bytes(NOT SUPPORTED)) */
            uint8_t *data;         /**< Pointer to SysEx data */
        uint16_t length;       /**< Length of SysEx data */
        } sysex;
    } data;
} midi_message_t;

#endif /* MIDI_TYPES_H */