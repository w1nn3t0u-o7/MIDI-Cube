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
 * @brief Complete MIDI 1.0 Message Structure
 * 
 * Represents a complete, parsed MIDI message with all relevant data
 */
typedef struct midi_message {
    uint8_t status_byte;                /**< Full status byte (including channel) */
    //uint8_t channel;               /**< MIDI channel (0-15, representing 1-16) */
    uint8_t data[2];              /**< Up to 2 data bytes */
} midi_message_t;

#endif /* MIDI_TYPES_H */