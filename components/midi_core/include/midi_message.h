/**
 * @file midi_message.h
 * @brief MIDI Message Serialization
 */

#ifndef MIDI_MESSAGE_H
#define MIDI_MESSAGE_H

#include "esp_err.h"

#include "midi_types.h"

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

#endif /* MIDI_MESSAGE_H */
