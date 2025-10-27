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
    uint32_t timestamp_us;           /**< Timestamp in microseconds (optional) */
} ump_packet_t;

/**
 * @brief MIDI 2.0 Note On/Off Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  note;                   /**< Note number (0-127) */
    uint16_t velocity;               /**< 16-bit velocity (0-65535) */
    uint8_t  attribute_type;         /**< Attribute type (0-3) */
    uint16_t attribute_data;         /**< Attribute data (16-bit) */
    bool     is_note_on;             /**< true = Note On, false = Note Off */
} midi2_note_message_t;

/**
 * @brief MIDI 2.0 Control Change Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  controller;             /**< Controller number (0-119) */
    uint32_t value;                  /**< 32-bit controller value */
} midi2_control_change_t;

/**
 * @brief MIDI 2.0 Program Change Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  program;                /**< Program number (0-127) */
    bool     bank_valid;             /**< Bank select valid flag */
    uint8_t  bank_msb;               /**< Bank MSB (0-127) if valid */
    uint8_t  bank_lsb;               /**< Bank LSB (0-127) if valid */
    uint8_t  options;                /**< Option flags */
} midi2_program_change_t;

/**
 * @brief MIDI 2.0 Pitch Bend Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint32_t value;                  /**< 32-bit pitch bend (0-4294967295, center=2147483648) */
} midi2_pitch_bend_t;

/**
 * @brief MIDI 2.0 Channel Pressure Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint32_t pressure;               /**< 32-bit pressure value */
} midi2_channel_pressure_t;

/**
 * @brief MIDI 2.0 Poly Pressure Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  note;                   /**< Note number (0-127) */
    uint32_t pressure;               /**< 32-bit pressure value */
} midi2_poly_pressure_t;

/**
 * @brief MIDI 2.0 Registered/Assignable Controller (RPN/NRPN)
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  bank;                   /**< Bank (0-127) */
    uint8_t  index;                  /**< Index (0-127) */
    uint32_t data;                   /**< 32-bit data value */
    bool     is_registered;          /**< true = RPN, false = NRPN */
} midi2_parameter_controller_t;

/**
 * @brief MIDI 2.0 Per-Note Controller
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  note;                   /**< Note number (0-127) */
    uint8_t  controller;             /**< Controller index */
    uint32_t value;                  /**< 32-bit controller value */
    bool     is_registered;          /**< true = RPNC, false = APNC */
} midi2_per_note_controller_t;

/**
 * @brief MIDI 2.0 Per-Note Management Message
 */
typedef struct {
    uint8_t  group;                  /**< Group (0-15) */
    uint8_t  channel;                /**< Channel (0-15) */
    uint8_t  note;                   /**< Note number (0-127) */
    bool     detach;                 /**< Detach per-note controllers */
    bool     reset;                  /**< Reset per-note controllers to default */
} midi2_per_note_management_t;

/**
 * @brief UMP Jitter Reduction Timestamp
 */
typedef struct {
    uint16_t timestamp;              /**< JR timestamp value */
} ump_jr_timestamp_t;

/**
 * @brief UMP Endpoint Information
 */
typedef struct {
    uint8_t  ump_version_major;      /**< UMP major version */
    uint8_t  ump_version_minor;      /**< UMP minor version */
    uint8_t  num_function_blocks;    /**< Number of function blocks (0-32) */
    bool     static_function_blocks; /**< Function blocks are static */
    bool     midi2_protocol;         /**< Supports MIDI 2.0 Protocol */
    bool     midi1_protocol;         /**< Supports MIDI 1.0 Protocol */
    bool     rx_jr_timestamp;        /**< Can receive JR timestamps */
    bool     tx_jr_timestamp;        /**< Can transmit JR timestamps */
} ump_endpoint_info_t;

#endif /* UMP_TYPES_H */
