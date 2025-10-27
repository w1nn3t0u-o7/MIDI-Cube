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
#include "midi_defs.h"

/**
 * @brief MIDI Message Type Classification
 */
typedef enum {
    MIDI_MSG_TYPE_CHANNEL_VOICE,      /**< Channel Voice Message */
    MIDI_MSG_TYPE_CHANNEL_MODE,       /**< Channel Mode Message */
    MIDI_MSG_TYPE_SYSTEM_COMMON,      /**< System Common Message */
    MIDI_MSG_TYPE_SYSTEM_REALTIME,    /**< System Real-Time Message */
    MIDI_MSG_TYPE_SYSTEM_EXCLUSIVE,   /**< System Exclusive Message */
    MIDI_MSG_TYPE_UNKNOWN             /**< Unknown/Invalid Message */
} midi_message_type_t;

/**
 * @brief MIDI Channel Voice Message Types
 */
typedef enum {
    MIDI_VOICE_NOTE_OFF = 0x80,
    MIDI_VOICE_NOTE_ON = 0x90,
    MIDI_VOICE_POLY_PRESSURE = 0xA0,
    MIDI_VOICE_CONTROL_CHANGE = 0xB0,
    MIDI_VOICE_PROGRAM_CHANGE = 0xC0,
    MIDI_VOICE_CHANNEL_PRESSURE = 0xD0,
    MIDI_VOICE_PITCH_BEND = 0xE0
} midi_voice_message_type_t;

/**
 * @brief MIDI Channel Mode enumeration
 * 
 * Four modes based on Omni On/Off and Poly/Mono
 */
typedef enum {
    MIDI_MODE_OMNI_ON_POLY = 1,   /**< Mode 1: Omni On, Poly */
    MIDI_MODE_OMNI_ON_MONO = 2,   /**< Mode 2: Omni On, Mono */
    MIDI_MODE_OMNI_OFF_POLY = 3,  /**< Mode 3: Omni Off, Poly */
    MIDI_MODE_OMNI_OFF_MONO = 4   /**< Mode 4: Omni Off, Mono */
} midi_channel_mode_t;

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
    
    /* Message Data */
    uint8_t data1;                 /**< First data byte */
    uint8_t data2;                 /**< Second data byte (if applicable) */
    uint8_t length;                /**< Total message length in bytes (1-3) */
    
    /* Timestamps (optional, for sequencing) */
    uint32_t timestamp_us;         /**< Timestamp in microseconds */
    
    /* System Exclusive specific */
    uint8_t *sysex_data;           /**< Pointer to SysEx data (if applicable) */
    uint16_t sysex_length;         /**< Length of SysEx data */
} midi_message_t;

/**
 * @brief Specialized Note On/Off Message Structure
 */
typedef struct {
    uint8_t channel;               /**< MIDI channel (0-15) */
    uint8_t note;                  /**< Note number (0-127) */
    uint8_t velocity;              /**< Note velocity (0-127) */
    bool is_note_on;               /**< true = Note On, false = Note Off */
} midi_note_message_t;

/**
 * @brief Control Change Message Structure
 */
typedef struct {
    uint8_t channel;               /**< MIDI channel (0-15) */
    uint8_t controller;            /**< Controller number (0-119) */
    uint8_t value;                 /**< Controller value (0-127) */
} midi_control_change_t;

/**
 * @brief Program Change Message Structure
 */
typedef struct {
    uint8_t channel;               /**< MIDI channel (0-15) */
    uint8_t program;               /**< Program number (0-127) */
} midi_program_change_t;

/**
 * @brief Pitch Bend Message Structure
 */
typedef struct {
    uint8_t channel;               /**< MIDI channel (0-15) */
    uint16_t value;                /**< 14-bit pitch bend value (0-16383, center=8192) */
    int16_t signed_value;          /**< Signed pitch bend (-8192 to +8191) */
} midi_pitch_bend_t;

/**
 * @brief Aftertouch (Pressure) Message Structure
 */
typedef struct {
    uint8_t channel;               /**< MIDI channel (0-15) */
    uint8_t note;                  /**< Note number (for poly pressure, 0 for channel) */
    uint8_t pressure;              /**< Pressure value (0-127) */
    bool is_polyphonic;            /**< true = Poly Pressure, false = Channel Pressure */
} midi_aftertouch_t;

/**
 * @brief System Real-Time Message Types
 */
typedef enum {
    MIDI_RT_TIMING_CLOCK = 0xF8,
    MIDI_RT_START = 0xFA,
    MIDI_RT_CONTINUE = 0xFB,
    MIDI_RT_STOP = 0xFC,
    MIDI_RT_ACTIVE_SENSING = 0xFE,
    MIDI_RT_SYSTEM_RESET = 0xFF
} midi_realtime_type_t;

#endif /* MIDI_TYPES_H */