/**
 * @file ump_defs.h
 * @brief Universal MIDI Packet (UMP) Format Constants
 * 
 * Based on UMP Format and MIDI 2.0 Protocol Specification v1.1.2
 * Reference: M2-104-UM_v1-1-2_UMP_and_MIDI_2-0_Protocol_Specification.pdf
 */

#ifndef UMP_DEFS_H
#define UMP_DEFS_H

#include <stdint.h>

#define UMP_MAX_WORDS               4     /**< Maximum words in a UMP */

/**
 * @defgroup UMP_MACROS UMP Helper Macros
 * @brief Macros for extracting UMP fields
 * @{
 */
#define UMP_GET_MT(ump) \
        ((ump).words[0] >> 28)

#define UMP_GET_GROUP(ump) \
        (((ump).words[0] >> 24) & 0x0F)

#define UMP_GET_STATUS_BYTE(ump) \
        (((ump).words[0] >> 16) & 0xFF)

#define UMP_GET_CHANNEL(ump) \
        (((ump).words[0] >> 16) & 0x0F)
 
#define UMP_NUM_WORDS_LOOKUP_TABLE \
        ((0U << 0) | (0U << 2) | (0U << 4) | (1U << 6) | \
        (1U << 8) | (3U << 10) | (0U << 12) | (0U << 14) | \
        (1U << 16) | (1U << 18) | (1U << 20) | (2U << 22) | \
        (2U << 24) | (3U << 26) | (3U << 28) | (3U << 30))
 
#define UMP_GET_NUM_WORDS(ump) \
        (1 + ((UMP_NUM_WORDS_LOOKUP_TABLE >> (2 * UMP_GET_MT(ump))) & 3))

/** @} */

/**
 * @defgroup UMP_MESSAGE_TYPES UMP Message Type (MT) Definitions
 * @brief Message Type field values (bits 31-28 of first word)
 * @{
 */
#define UMP_MT_UTILITY              0x0   /**< Utility Messages (32-bit) */
#define UMP_MT_SYSTEM               0x1   /**< System Real Time and System Common (32-bit) */
#define UMP_MT_MIDI1_CHANNEL_VOICE  0x2   /**< MIDI 1.0 Channel Voice Messages (32-bit) */
#define UMP_MT_DATA_64              0x3   /**< Data Messages including SysEx (64-bit) */
#define UMP_MT_MIDI2_CHANNEL_VOICE  0x4   /**< MIDI 2.0 Channel Voice Messages (64-bit) */
#define UMP_MT_DATA_128             0x5   /**< Data Messages: SysEx 8, Mixed Data Set (128-bit) */
#define UMP_MT_RESERVED_6           0x6   /**< Reserved for future use (32-bit) */
#define UMP_MT_RESERVED_7           0x7   /**< Reserved for future use (32-bit) */
#define UMP_MT_RESERVED_8           0x8   /**< Reserved for future use (64-bit) */
#define UMP_MT_RESERVED_9           0x9   /**< Reserved for future use (64-bit) */
#define UMP_MT_RESERVED_A           0xA   /**< Reserved for future use (64-bit) */
#define UMP_MT_RESERVED_B           0xB   /**< Reserved for future use (96-bit) */
#define UMP_MT_RESERVED_C           0xC   /**< Reserved for future use (96-bit) */
#define UMP_MT_FLEX_DATA            0xD   /**< Flex Data Messages (128-bit) */
#define UMP_MT_RESERVED_E           0xE   /**< Reserved for future use (128-bit) */
#define UMP_MT_UMP_STREAM           0xF   /**< UMP Stream Messages (128-bit) */

/** @} */

/**
 * @defgroup MIDI2_CHANNEL_VOICE MIDI 2.0 Channel Voice Message Status
 * @{
 */

#define MIDI2_STATUS_NOTE_OFF       0x80  /**< Note Off */
#define MIDI2_STATUS_NOTE_ON        0x90  /**< Note On */
#define MIDI2_STATUS_POLY_PRESSURE  0xA0  /**< Poly Pressure */
#define MIDI2_STATUS_RPN            0x20  /**< Registered Per-Note Controller */
#define MIDI2_STATUS_NRPN           0x30  /**< Assignable Per-Note Controller */
#define MIDI2_STATUS_PER_NOTE_MGMT  0xF0  /**< Per-Note Management */
#define MIDI2_STATUS_CONTROL_CHANGE 0xB0  /**< Control Change */
#define MIDI2_STATUS_RPN_CTRL       0x20  /**< Registered Controller (RPN) */
#define MIDI2_STATUS_NRPN_CTRL      0x30  /**< Assignable Controller (NRPN) */
#define MIDI2_STATUS_REL_RPN        0x40  /**< Relative Registered Controller */
#define MIDI2_STATUS_REL_NRPN       0x50  /**< Relative Assignable Controller */
#define MIDI2_STATUS_PROGRAM_CHANGE 0xC0  /**< Program Change */
#define MIDI2_STATUS_CHANNEL_PRESSURE 0xD0 /**< Channel Pressure */
#define MIDI2_STATUS_PITCH_BEND     0xE0  /**< Pitch Bend */
#define MIDI2_STATUS_PER_NOTE_PITCH 0x60  /**< Per-Note Pitch Bend */

/** @} */

/**
 * @defgroup UMP_UTILITY_STATUS Utility Message Status Values
 * @{
 */
#define UMP_UTILITY_NOOP            0x00  /**< No Operation */
#define UMP_UTILITY_JR_CLOCK        0x01  /**< Jitter Reduction Clock */
#define UMP_UTILITY_JR_TIMESTAMP    0x02  /**< Jitter Reduction Timestamp */
#define UMP_UTILITY_DCTPQ           0x03  /**< Delta Clockstamp Ticks Per Quarter Note */
#define UMP_UTILITY_DC_TICKS        0x04  /**< Delta Clockstamp */

/** @} */

/**
 * @defgroup MIDI2_NOTE_ATTRIBUTES MIDI 2.0 Note On/Off Attribute Types
 * @{
 */
#define MIDI2_ATTR_NONE             0x00  /**< No attribute */
#define MIDI2_ATTR_MANUFACTURER     0x01  /**< Manufacturer specific */
#define MIDI2_ATTR_PROFILE          0x02  /**< Profile specific */
#define MIDI2_ATTR_PITCH            0x03  /**< Pitch 7.9 format */

/** @} */

/**
 * @defgroup UMP_STREAM_STATUS UMP Stream Message Status
 * @{
 */

#define UMP_STREAM_ENDPOINT_DISCOVERY      0x00  /**< Endpoint Discovery */
#define UMP_STREAM_ENDPOINT_INFO           0x01  /**< Endpoint Info Notification */
#define UMP_STREAM_DEVICE_IDENTITY         0x02  /**< Device Identity Notification */
#define UMP_STREAM_ENDPOINT_NAME           0x03  /**< Endpoint Name Notification */
#define UMP_STREAM_PRODUCT_INSTANCE_ID     0x04  /**< Product Instance Id */
#define UMP_STREAM_CONFIGURATION_REQUEST   0x05  /**< Stream Configuration Request */
#define UMP_STREAM_CONFIGURATION_NOTIFY    0x06  /**< Stream Configuration Notification */
#define UMP_STREAM_FUNCTION_BLOCK_DISCOVERY 0x10 /**< Function Block Discovery */
#define UMP_STREAM_FUNCTION_BLOCK_INFO     0x11  /**< Function Block Info Notification */
#define UMP_STREAM_FUNCTION_BLOCK_NAME     0x12  /**< Function Block Name Notification */
#define UMP_STREAM_START_OF_CLIP           0x20  /**< Start of Clip */
#define UMP_STREAM_END_OF_CLIP             0x21  /**< End of Clip */

/** @} */

/**
 * @defgroup UMP_FORMAT UMP Format Field Values
 * @{
 */

#define UMP_FORMAT_COMPLETE         0x0   /**< Complete message in one UMP */
#define UMP_FORMAT_START            0x1   /**< Start of multi-UMP message */
#define UMP_FORMAT_CONTINUE         0x2   /**< Continue multi-UMP message */
#define UMP_FORMAT_END              0x3   /**< End of multi-UMP message */

/** @} */

#endif /* UMP_DEFS_H */
