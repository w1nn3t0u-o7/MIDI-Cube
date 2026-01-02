/**
 * @file ump_types.h
 * @brief Universal MIDI Packet Data Types
 * 
 * Structures for UMP packets and MIDI 2.0 messages
 */

#ifndef UMP_PACKET_H
#define UMP_PACKET_H

#include <stdint.h>
#include "ump_defs.h"

/**
 * @brief Universal MIDI Packet (UMP) Structure
 * 
 * A UMP can be 32, 64, 96, or 128 bits (1-4 words)
 */
typedef struct {
    uint32_t words[4];  /**< Up to 4 32-bit words */
} ump_packet_t;

#endif /* UMP_PACKET_H */
