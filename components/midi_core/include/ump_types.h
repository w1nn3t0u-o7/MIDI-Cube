/**
 * @file ump_types.h
 * @brief Universal MIDI Packet Data Types
 * 
 * Structures for UMP packets and MIDI 2.0 messages
 */

#ifndef UMP_TYPES_H
#define UMP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "ump_defs.h"
#include "midi_types.h"  // For backwards compatibility

/**
 * @brief Universal MIDI Packet (UMP) Structure
 * 
 * A UMP can be 32, 64, 96, or 128 bits (1-4 words)
 * All UMPs are represented as 4 words, with unused words set to 0
 */
typedef struct {
    uint32_t words[UMP_MAX_WORDS];  /**< Up to 4 32-bit words */
    uint8_t  num_words;              /**< Actual number of words (1-4) */
    uint8_t  message_type;           /**< Message Type (MT) field */
    uint8_t  group;                  /**< Group number (0-15), 0xFF if groupless */
} ump_packet_t;

#endif /* UMP_TYPES_H */
