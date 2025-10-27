/**
 * @file midi_defs.h
 * @brief MIDI 1.0 Protocol Definitions and Constants
 * 
 * Based on MIDI 1.0 Detailed Specification Document Version 4.2.1
 * Reference: M1_v4-2-1_MIDI_1-0_Detailed_Specification_96-1-4.pdf
 */

#ifndef MIDI_DEFS_H
#define MIDI_DEFS_H

//#include <stdint.h>
//#include <stdbool.h>

/**
 * @defgroup MIDI_STATUS_BYTES MIDI Status Bytes
 * @brief Status byte definitions for MIDI 1.0 messages
 * @{
 */

/* Channel Voice Messages (8n - En) */
#define MIDI_STATUS_NOTE_OFF            0x80  /**< Note Off message */
#define MIDI_STATUS_NOTE_ON             0x90  /**< Note On message */
#define MIDI_STATUS_POLY_PRESSURE       0xA0  /**< Polyphonic Key Pressure (Aftertouch) */
#define MIDI_STATUS_CONTROL_CHANGE      0xB0  /**< Control Change message */
#define MIDI_STATUS_PROGRAM_CHANGE      0xC0  /**< Program Change message */
#define MIDI_STATUS_CHANNEL_PRESSURE    0xD0  /**< Channel Pressure (Aftertouch) */
#define MIDI_STATUS_PITCH_BEND          0xE0  /**< Pitch Bend Change message */

/* System Common Messages (F0 - F7) */
#define MIDI_STATUS_SYSEX_START         0xF0  /**< System Exclusive Start */
#define MIDI_STATUS_MTC_QUARTER_FRAME   0xF1  /**< MIDI Time Code Quarter Frame */
#define MIDI_STATUS_SONG_POSITION       0xF2  /**< Song Position Pointer */
#define MIDI_STATUS_SONG_SELECT         0xF3  /**< Song Select */
#define MIDI_STATUS_UNDEFINED_F4        0xF4  /**< Undefined System Common */
#define MIDI_STATUS_UNDEFINED_F5        0xF5  /**< Undefined System Common */
#define MIDI_STATUS_TUNE_REQUEST        0xF6  /**< Tune Request */
#define MIDI_STATUS_SYSEX_END           0xF7  /**< End of System Exclusive (EOX) */

/* System Real-Time Messages (F8 - FF) */
#define MIDI_STATUS_TIMING_CLOCK        0xF8  /**< Timing Clock (24 per quarter note) */
#define MIDI_STATUS_UNDEFINED_F9        0xF9  /**< Undefined Real-Time */
#define MIDI_STATUS_START               0xFA  /**< Start */
#define MIDI_STATUS_CONTINUE            0xFB  /**< Continue */
#define MIDI_STATUS_STOP                0xFC  /**< Stop */
#define MIDI_STATUS_UNDEFINED_FD        0xFD  /**< Undefined Real-Time */
#define MIDI_STATUS_ACTIVE_SENSING      0xFE  /**< Active Sensing */
#define MIDI_STATUS_SYSTEM_RESET        0xFF  /**< System Reset */

/** @} */

/**
 * @defgroup MIDI_CONTROLLERS MIDI Controller Numbers
 * @brief Standard MIDI Controller Change numbers (CC 0-119)
 * @{
 */

/* MSB for Continuous Controllers (0-31) */
#define MIDI_CC_BANK_SELECT_MSB         0x00  /**< Bank Select MSB */
#define MIDI_CC_MODULATION_WHEEL_MSB    0x01  /**< Modulation Wheel MSB */
#define MIDI_CC_BREATH_CONTROLLER_MSB   0x02  /**< Breath Controller MSB */
#define MIDI_CC_FOOT_CONTROLLER_MSB     0x04  /**< Foot Controller MSB */
#define MIDI_CC_PORTAMENTO_TIME_MSB     0x05  /**< Portamento Time MSB */
#define MIDI_CC_DATA_ENTRY_MSB          0x06  /**< Data Entry MSB */
#define MIDI_CC_CHANNEL_VOLUME_MSB      0x07  /**< Channel Volume MSB */
#define MIDI_CC_BALANCE_MSB             0x08  /**< Balance MSB */
#define MIDI_CC_PAN_MSB                 0x0A  /**< Pan MSB */
#define MIDI_CC_EXPRESSION_MSB          0x0B  /**< Expression Controller MSB */
#define MIDI_CC_EFFECT_CONTROL_1_MSB    0x0C  /**< Effect Control 1 MSB */
#define MIDI_CC_EFFECT_CONTROL_2_MSB    0x0D  /**< Effect Control 2 MSB */
#define MIDI_CC_GENERAL_PURPOSE_1_MSB   0x10  /**< General Purpose Controller 1 MSB */
#define MIDI_CC_GENERAL_PURPOSE_2_MSB   0x11  /**< General Purpose Controller 2 MSB */
#define MIDI_CC_GENERAL_PURPOSE_3_MSB   0x12  /**< General Purpose Controller 3 MSB */
#define MIDI_CC_GENERAL_PURPOSE_4_MSB   0x13  /**< General Purpose Controller 4 MSB */

/* LSB for Continuous Controllers (32-63) */
#define MIDI_CC_BANK_SELECT_LSB         0x20  /**< Bank Select LSB */
#define MIDI_CC_MODULATION_WHEEL_LSB    0x21  /**< Modulation Wheel LSB */
#define MIDI_CC_BREATH_CONTROLLER_LSB   0x22  /**< Breath Controller LSB */
#define MIDI_CC_FOOT_CONTROLLER_LSB     0x24  /**< Foot Controller LSB */
#define MIDI_CC_PORTAMENTO_TIME_LSB     0x25  /**< Portamento Time LSB */
#define MIDI_CC_DATA_ENTRY_LSB          0x26  /**< Data Entry LSB */
#define MIDI_CC_CHANNEL_VOLUME_LSB      0x27  /**< Channel Volume LSB */
#define MIDI_CC_BALANCE_LSB             0x28  /**< Balance LSB */
#define MIDI_CC_PAN_LSB                 0x2A  /**< Pan LSB */
#define MIDI_CC_EXPRESSION_LSB          0x2B  /**< Expression Controller LSB */

/* Single Byte Controllers (64-119) */
#define MIDI_CC_SUSTAIN_PEDAL           0x40  /**< Sustain Pedal (Damper) */
#define MIDI_CC_PORTAMENTO_ONOFF        0x41  /**< Portamento On/Off */
#define MIDI_CC_SOSTENUTO               0x42  /**< Sostenuto */
#define MIDI_CC_SOFT_PEDAL              0x43  /**< Soft Pedal */
#define MIDI_CC_LEGATO_FOOTSWITCH       0x44  /**< Legato Footswitch */
#define MIDI_CC_HOLD_2                  0x45  /**< Hold 2 */

/* Sound Controllers (70-79) */
#define MIDI_CC_SOUND_VARIATION         0x46  /**< Sound Controller 1 (Sound Variation) */
#define MIDI_CC_TIMBRE_INTENSITY        0x47  /**< Sound Controller 2 (Timbre/Harmonic Intensity) */
#define MIDI_CC_RELEASE_TIME            0x48  /**< Sound Controller 3 (Release Time) */
#define MIDI_CC_ATTACK_TIME             0x49  /**< Sound Controller 4 (Attack Time) */
#define MIDI_CC_BRIGHTNESS              0x4A  /**< Sound Controller 5 (Brightness) */
#define MIDI_CC_SOUND_CONTROLLER_6      0x4B  /**< Sound Controller 6 */
#define MIDI_CC_SOUND_CONTROLLER_7      0x4C  /**< Sound Controller 7 */
#define MIDI_CC_SOUND_CONTROLLER_8      0x4D  /**< Sound Controller 8 */
#define MIDI_CC_SOUND_CONTROLLER_9      0x4E  /**< Sound Controller 9 */
#define MIDI_CC_SOUND_CONTROLLER_10     0x4F  /**< Sound Controller 10 */

/* General Purpose Controllers (80-83) */
#define MIDI_CC_GENERAL_PURPOSE_5       0x50  /**< General Purpose Controller 5 */
#define MIDI_CC_GENERAL_PURPOSE_6       0x51  /**< General Purpose Controller 6 */
#define MIDI_CC_GENERAL_PURPOSE_7       0x52  /**< General Purpose Controller 7 */
#define MIDI_CC_GENERAL_PURPOSE_8       0x53  /**< General Purpose Controller 8 */
#define MIDI_CC_PORTAMENTO_CONTROL      0x54  /**< Portamento Control */

/* Effects Depth (91-95) */
#define MIDI_CC_EFFECTS_DEPTH_1         0x5B  /**< Effects 1 Depth (External Effects) */
#define MIDI_CC_EFFECTS_DEPTH_2         0x5C  /**< Effects 2 Depth (Tremolo) */
#define MIDI_CC_EFFECTS_DEPTH_3         0x5D  /**< Effects 3 Depth (Chorus) */
#define MIDI_CC_EFFECTS_DEPTH_4         0x5E  /**< Effects 4 Depth (Celeste/Detune) */
#define MIDI_CC_EFFECTS_DEPTH_5         0x5F  /**< Effects 5 Depth (Phaser) */

/* Parameter Numbers (98-101) */
#define MIDI_CC_DATA_INCREMENT          0x60  /**< Data Increment */
#define MIDI_CC_DATA_DECREMENT          0x61  /**< Data Decrement */
#define MIDI_CC_NRPN_LSB                0x62  /**< Non-Registered Parameter Number LSB */
#define MIDI_CC_NRPN_MSB                0x63  /**< Non-Registered Parameter Number MSB */
#define MIDI_CC_RPN_LSB                 0x64  /**< Registered Parameter Number LSB */
#define MIDI_CC_RPN_MSB                 0x65  /**< Registered Parameter Number MSB */

/** @} */

/**
 * @defgroup MIDI_CHANNEL_MODES MIDI Channel Mode Messages
 * @brief Channel Mode message controller numbers (120-127)
 * @{
 */

#define MIDI_CC_ALL_SOUND_OFF           0x78  /**< All Sound Off (120) */
#define MIDI_CC_RESET_ALL_CONTROLLERS   0x79  /**< Reset All Controllers (121) */
#define MIDI_CC_LOCAL_CONTROL           0x7A  /**< Local Control (122) */
#define MIDI_CC_ALL_NOTES_OFF           0x7B  /**< All Notes Off (123) */
#define MIDI_CC_OMNI_MODE_OFF           0x7C  /**< Omni Mode Off (124) */
#define MIDI_CC_OMNI_MODE_ON            0x7D  /**< Omni Mode On (125) */
#define MIDI_CC_MONO_MODE_ON            0x7E  /**< Mono Mode On (Poly Off) (126) */
#define MIDI_CC_POLY_MODE_ON            0x7F  /**< Poly Mode On (Mono Off) (127) */

/** @} */

/**
 * @defgroup MIDI_RPN Registered Parameter Numbers
 * @brief Standard Registered Parameter Numbers (RPN)
 * @{
 */

#define MIDI_RPN_PITCH_BEND_SENSITIVITY_MSB  0x00  /**< Pitch Bend Sensitivity MSB */
#define MIDI_RPN_PITCH_BEND_SENSITIVITY_LSB  0x00  /**< Pitch Bend Sensitivity LSB */
#define MIDI_RPN_FINE_TUNING_MSB             0x00  /**< Fine Tuning MSB */
#define MIDI_RPN_FINE_TUNING_LSB             0x01  /**< Fine Tuning LSB */
#define MIDI_RPN_COARSE_TUNING_MSB           0x00  /**< Coarse Tuning MSB */
#define MIDI_RPN_COARSE_TUNING_LSB           0x02  /**< Coarse Tuning LSB */
#define MIDI_RPN_TUNING_PROGRAM_SELECT_MSB   0x00  /**< Tuning Program Select MSB */
#define MIDI_RPN_TUNING_PROGRAM_SELECT_LSB   0x03  /**< Tuning Program Select LSB */
#define MIDI_RPN_TUNING_BANK_SELECT_MSB      0x00  /**< Tuning Bank Select MSB */
#define MIDI_RPN_TUNING_BANK_SELECT_LSB      0x04  /**< Tuning Bank Select LSB */
#define MIDI_RPN_NULL_MSB                    0x7F  /**< RPN Null MSB */
#define MIDI_RPN_NULL_LSB                    0x7F  /**< RPN Null LSB */

/** @} */

/**
 * @defgroup MIDI_CONSTANTS MIDI Protocol Constants
 * @{
 */

#define MIDI_BAUD_RATE              31250     /**< MIDI standard baud rate */
#define MIDI_CHANNELS               16        /**< Number of MIDI channels (1-16) */
#define MIDI_NOTE_MIN               0         /**< Minimum note number */
#define MIDI_NOTE_MAX               127       /**< Maximum note number */
#define MIDI_NOTE_MIDDLE_C          60        /**< Middle C note number */
#define MIDI_VELOCITY_MIN           0         /**< Minimum velocity value */
#define MIDI_VELOCITY_MAX           127       /**< Maximum velocity value */
#define MIDI_VELOCITY_DEFAULT       64        /**< Default velocity (mezzo-forte) */
#define MIDI_DATA_BYTE_MAX          127       /**< Maximum data byte value (7-bit) */
#define MIDI_STATUS_BIT_MASK        0x80      /**< Status byte MSB mask */
#define MIDI_DATA_BIT_MASK          0x7F      /**< Data byte mask (7-bit) */
#define MIDI_CHANNEL_MASK           0x0F      /**< Channel number mask (4-bit) */
#define MIDI_STATUS_TYPE_MASK       0xF0      /**< Status type mask (upper nibble) */

/* Pitch Bend */
#define MIDI_PITCH_BEND_CENTER      0x2000    /**< Pitch bend center value (8192) */
#define MIDI_PITCH_BEND_MIN         0x0000    /**< Pitch bend minimum value */
#define MIDI_PITCH_BEND_MAX         0x3FFF    /**< Pitch bend maximum value (16383) */

/* Active Sensing Timing */
#define MIDI_ACTIVE_SENSING_TIMEOUT_MS  300   /**< Active sensing timeout (ms) */

/* Timing */
#define MIDI_CLOCKS_PER_QUARTER_NOTE    24    /**< MIDI clock pulses per quarter note */
#define MIDI_BYTE_TRANSMISSION_TIME_US  320   /**< Time to transmit one MIDI byte (μs) */

/** @} */


#endif /* MIDI_DEFS_H */
