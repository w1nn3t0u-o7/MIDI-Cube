/**
 * @file midi_message.h
 * @brief MIDI Message Structure Definition
 */

#ifndef MIDI_MESSAGE_H
#define MIDI_MESSAGE_H

#include "esp_err.h"

#include "midi_defs.h"

/**
 * @brief Complete MIDI 1.0 Message Structure
 * 
 * Represents a complete, parsed MIDI message 
 */
typedef struct midi_message {
    uint8_t status_byte;          /**< Full status byte (including channel) */
    uint8_t data[2];              /**< Up to 2 data bytes */
} midi_message_t;

/**
 * @brief Get expected data byte count for a status byte
 * 
 * @param status MIDI status byte
 * @return Number of expected data bytes (0, 1, or 2)
 */
uint8_t midi1_get_data_byte_count(uint8_t status);

/**
 * @brief Get total message length (status + data bytes)
 * 
 * @param status MIDI status byte
 * @return Total message length in bytes
 */
uint8_t midi1_get_message_length(uint8_t status);

/**
 * @brief Check if byte is a status byte
 * 
 * @param byte Byte to check
 * @return true if MSB is set (status byte)
 */
static inline bool midi1_is_status_byte(uint8_t byte) {
    return (byte & MIDI1_STATUS_BIT_MASK) != 0;
}

/**
 * @brief Check if byte is a data byte
 * 
 * @param byte Byte to check
 * @return true if MSB is clear (data byte)
 */
static inline bool midi1_is_data_byte(uint8_t byte) {
    return (byte & MIDI1_STATUS_BIT_MASK) == 0;
}

/**
 * @brief Check if status is a Real-Time message
 * 
 * @param status Status byte
 * @return true if Real-Time message (0xF8-0xFF)
 */
static inline bool midi1_is_realtime_message(uint8_t status) {
    return status >= MIDI1_STATUS_TIMING_CLOCK;
}

/**
 * @brief Check if status is a System Common message
 * 
 * @param status Status byte
 * @return true if System Common message (0xF0-0xF7)
 */
static inline bool midi1_is_system_common_message(uint8_t status) {
    return (status >= MIDI1_STATUS_SYSEX_START && 
            status <= MIDI1_STATUS_SYSEX_END);
}

/**
 * @brief Check if status is a Channel Voice/Mode message
 * 
 * @param status Status byte
 * @return true if Channel message (0x80-0xEF)
 */
static inline bool midi1_is_channel_message(uint8_t status) {
    return (status >= MIDI1_STATUS_NOTE_OFF && 
            status < MIDI1_STATUS_SYSEX_START);
}

/**
 * @brief Serialize MIDI message to byte array
 * 
 * Converts a MIDI message structure to raw MIDI bytes for transmission
 * 
 * @param msg Pointer to message structure
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param bytes_written Number of bytes written to buffer
 * @return ESP_OK on success
 */
esp_err_t midi1_message_to_bytes(const midi_message_t *msg, uint8_t *buffer,
                                size_t buffer_size, size_t *bytes_written);

#endif /* MIDI_MESSAGE_H */
