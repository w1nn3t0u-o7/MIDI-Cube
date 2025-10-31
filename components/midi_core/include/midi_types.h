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
typedef struct {
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
    // /* Type-Specific Data (union - only one active at a time) */
    // union {
    //     /* Note On/Off */
    //     struct {
    //         uint8_t note;          /**< Note number (0-127) */
    //         uint8_t velocity;      /**< Velocity (0-127) */
    //     } note;
        
    //     /* Control Change */
    //     struct {
    //         uint8_t controller;    /**< Controller number (0-119) */
    //         uint8_t value;         /**< Controller value (0-127) */
    //     } control_change;
        
    //     /* Program Change */
    //     struct {
    //         uint8_t program;       /**< Program number (0-127) */
    //     } program_change;
        
    //     /* Pitch Bend */
    //     struct {
    //         uint16_t value;        /**< 14-bit value (0-16383, center=8192) */
    //     } pitch_bend;
        
    //     /* Polyphonic Pressure (Aftertouch) */
    //     struct {
    //         uint8_t note;          /**< Note number (0-127) */
    //         uint8_t pressure;      /**< Pressure value (0-127) */
    //     } poly_pressure;
        
    //     /* Channel Pressure */
    //     struct {
    //         uint8_t pressure;      /**< Pressure value (0-127) */
    //     } channel_pressure;

    //     /* Channel Mode */
    //     struct {
    //         uint8_t controller;    /**< Controller number (120-127) */
    //         uint8_t value;         /**< Controller value (0-127) */
    //     } channel_mode;
        
    //     /* System Exclusive */
    //     struct {
    //         uint8_t manufacturer_id; /**< Manufacturer ID (1 or 3 bytes(NOT SUPPORTED)) */
    //         uint8_t *data;         /**< Pointer to SysEx data */
    //         uint16_t length;       /**< Length of SysEx data */
    //     } sysex;
        
    //     /* System Common */
    //     struct {
    //         union {
    //             /* MTC Quarter Frame */
    //             struct {
    //                 uint8_t piece;         /**< MTC data piece (0-7) */
    //                 uint8_t value;         /**< 4-bit value */
    //             } mtc;

    //             /* Song Position Pointer */
    //             uint16_t position;     /**< 14-bit position (0-16383) */

    //             /* Song Select */
    //             uint8_t song;          /**< Song number (0-127) */
    //         } data;
    //     } system_common;
    // } data;  /**< Type-specific message data */
} midi_message_t;

/* OPTIONAL STRUCTS UNCOMMENT IF NEEDED */
// /**
//  * @brief MIDI Channel Mode enumeration
//  * 
//  * Four modes based on Omni On/Off and Poly/Mono
//  */
// typedef enum {
//     MIDI_MODE_OMNI_ON_POLY = 1,   /**< Mode 1: Omni On, Poly */
//     MIDI_MODE_OMNI_ON_MONO = 2,   /**< Mode 2: Omni On, Mono */
//     MIDI_MODE_OMNI_OFF_POLY = 3,  /**< Mode 3: Omni Off, Poly */
//     MIDI_MODE_OMNI_OFF_MONO = 4   /**< Mode 4: Omni Off, Mono */
// } midi_channel_mode_t;

// /**
//  * @brief System Real-Time Message Types
//  */
// typedef enum {
//     MIDI_RT_TIMING_CLOCK = 0xF8,
//     MIDI_RT_START = 0xFA,
//     MIDI_RT_CONTINUE = 0xFB,
//     MIDI_RT_STOP = 0xFC,
//     MIDI_RT_ACTIVE_SENSING = 0xFE,
//     MIDI_RT_SYSTEM_RESET = 0xFF
// } midi_realtime_type_t;

#endif /* MIDI_TYPES_H */