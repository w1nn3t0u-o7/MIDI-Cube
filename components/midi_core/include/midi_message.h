/**
 * @file midi_message.h
 * @brief MIDI Message Creation and Manipulation Functions
 */

#ifndef MIDI_MESSAGE_H
#define MIDI_MESSAGE_H

#include "midi_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a Note On message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param note Note number (0-127)
 * @param velocity Velocity (0-127, 0 = Note Off)
 * @return ESP_OK on success
 */
esp_err_t midi_create_note_on(midi_message_t *msg, uint8_t channel, 
                               uint8_t note, uint8_t velocity);

/**
 * @brief Create a Note Off message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param note Note number (0-127)
 * @param velocity Release velocity (0-127)
 * @return ESP_OK on success
 */
esp_err_t midi_create_note_off(midi_message_t *msg, uint8_t channel,
                                uint8_t note, uint8_t velocity);

/**
 * @brief Create a Control Change message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param controller Controller number (0-119)
 * @param value Controller value (0-127)
 * @return ESP_OK on success
 */
esp_err_t midi_create_control_change(midi_message_t *msg, uint8_t channel,
                                      uint8_t controller, uint8_t value);

/**
 * @brief Create a Program Change message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param program Program number (0-127)
 * @return ESP_OK on success
 */
esp_err_t midi_create_program_change(midi_message_t *msg, uint8_t channel,
                                      uint8_t program);

/**
 * @brief Create a Pitch Bend message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param value 14-bit pitch bend value (0-16383, center=8192)
 * @return ESP_OK on success
 */
esp_err_t midi_create_pitch_bend(midi_message_t *msg, uint8_t channel,
                                  uint16_t value);

/**
 * @brief Create Channel Pressure (Aftertouch) message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param pressure Pressure value (0-127)
 * @return ESP_OK on success
 */
esp_err_t midi_create_channel_pressure(midi_message_t *msg, uint8_t channel,
                                        uint8_t pressure);

/**
 * @brief Create Polyphonic Key Pressure message
 * 
 * @param msg Pointer to message structure
 * @param channel MIDI channel (0-15)
 * @param note Note number (0-127)
 * @param pressure Pressure value (0-127)
 * @return ESP_OK on success
 */
esp_err_t midi_create_poly_pressure(midi_message_t *msg, uint8_t channel,
                                     uint8_t note, uint8_t pressure);

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
esp_err_t midi_message_to_bytes(const midi_message_t *msg,
                                uint8_t *buffer,
                                size_t buffer_size,
                                size_t *bytes_written);

/**
 * @brief Extract note information from message
 * 
 * @param msg Source MIDI message
 * @param note_msg Destination note message structure
 * @return ESP_OK if message is Note On/Off, ESP_ERR_INVALID_ARG otherwise
 */
esp_err_t midi_message_to_note(const midi_message_t *msg,
                                midi_note_message_t *note_msg);

/**
 * @brief Check if message is Note On (with velocity > 0)
 * 
 * @param msg MIDI message
 * @return true if Note On with velocity > 0
 */
bool midi_is_note_on(const midi_message_t *msg);

/**
 * @brief Check if message is Note Off (or Note On with velocity 0)
 * 
 * @param msg MIDI message
 * @return true if Note Off
 */
bool midi_is_note_off(const midi_message_t *msg);

/**
 * @brief Get human-readable message type string
 * 
 * @param msg MIDI message
 * @return String describing message type
 */
const char* midi_get_message_type_string(const midi_message_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_MESSAGE_H */
