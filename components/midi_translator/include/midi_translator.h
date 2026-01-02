/**
 * @file midi_translator.h
 * @brief MIDI 1.0 ↔ MIDI 2.0 Protocol Translation
 * 
 * Implements bidirectional translation between MIDI 1.0 byte stream messages
 * and MIDI 2.0 Universal MIDI Packets per Appendix D of UMP specification
 */

#ifndef MIDI_TRANSLATOR_H
#define MIDI_TRANSLATOR_H

#include "esp_err.h"

#include "midi_message.h"
#include "ump_packet.h"

/**
 * @brief Translate MIDI 1.0 message to UMP (MIDI 2.0)
 * 
 * Performs upscaling of 7/14-bit values to 16/32-bit using Min-Center-Max algorithm
 * per spec Appendix D.1.3 and D.3
 * 
 * @param midi1_msg MIDI 1.0 message
 * @param ump_out Output UMP packet
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if message cannot be translated
 */
esp_err_t midi_translate_1to2(const midi1_message_t *midi1_msg, 
                               ump_packet_t *ump_out);

/**
 * @brief Translate UMP (MIDI 2.0) to MIDI 1.0 message
 * 
 * Performs downscaling of 16/32-bit values to 7/14-bit using bit shift
 * per spec Appendix D.1.4 and D.2
 * 
 * @param ump_in Input UMP packet
 * @param midi1_msg Output MIDI 1.0 message
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if message cannot be translated
 */
esp_err_t midi_translate_2to1(const ump_packet_t *ump_in,
                               midi1_message_t *midi1_msg);

/**
 * @brief Upscale 7-bit MIDI 1.0 value to 16-bit MIDI 2.0 value
 * 
 * Uses Min-Center-Max algorithm (spec Appendix D.1.3)
 * 
 * @param value7 7-bit input value (0-127)
 * @return 16-bit output value (0-65535)
 */
uint16_t midi_upscale_7to16(uint8_t value7);

/**
 * @brief Upscale 7-bit MIDI 1.0 value to 32-bit MIDI 2.0 value
 * 
 * Uses Min-Center-Max algorithm (spec Appendix D.3)
 * 
 * @param value_7 7-bit input value (0-127)
 * @return 32-bit output value (0-4294967295)
 */
uint32_t midi_upscale_7to32(uint8_t value_7);

/**
 * @brief Upscale 14-bit MIDI 1.0 value to 32-bit MIDI 2.0 value
 * 
 * @param value14 14-bit input value (0-16383)
 * @return 32-bit output value (0-4294967295)
 */
uint32_t midi_upscale_14to32(uint16_t value14);

/**
 * @brief Downscale 16-bit MIDI 2.0 value to 7-bit MIDI 1.0 value
 * 
 * Uses simple bit shift (spec Appendix D.1.4)
 * 
 * @param value16 16-bit input value (0-65535)
 * @return 7-bit output value (0-127)
 */
uint8_t midi_downscale_16to7(uint16_t value16);

/**
 * @brief Downscale 32-bit MIDI 2.0 value to 14-bit MIDI 1.0 value
 * 
 * @param value32 32-bit input value (0-4294967295)
 * @return 14-bit output value (0-16383)
 */
uint16_t midi_downscale_32to14(uint32_t value32);

/**
 * @brief Build a MIDI 2.0 Note On UMP packet
 * 
 * @param group UMP group (0-15)
 * @param channel MIDI channel (0-15)
 * @param note Note number (0-127)
 * @param velocity Velocity (16-bit)
 * @param attr_type Attribute type
 * @param attr_data Attribute data
 * @param packet Output UMP packet
 * @return ESP_OK on success
 */
esp_err_t ump_build_midi2_note_on(uint8_t group, uint8_t channel, 
                                   uint8_t note, uint16_t velocity,
                                   uint8_t attr_type, uint16_t attr_data,
                                   ump_packet_t *packet);

/**
 * @brief Build a MIDI 2.0 Control Change UMP packet
 * 
 * @param group UMP group (0-15)
 * @param channel MIDI channel (0-15)
 * @param controller Controller number (0-127)
 * @param value 32-bit controller value
 * @param packet Output UMP packet
 * @return ESP_OK on success
 */
esp_err_t ump_build_midi2_control_change(uint8_t group, uint8_t channel,
                                          uint8_t controller, uint32_t value,
                                          ump_packet_t *packet);

/**
 * @brief Build a MIDI 2.0 Pitch Bend UMP packet
 * 
 * @param group UMP group (0-15)
 * @param channel MIDI channel (0-15)
 * @param value 32-bit pitch bend value
 * @param packet Output UMP packet
 * @return ESP_OK on success
 */
esp_err_t ump_build_midi2_pitch_bend(uint8_t group, uint8_t channel,
                                      uint32_t value, ump_packet_t *packet);
                                
esp_err_t ump_build_midi2_note_off(uint8_t group, uint8_t channel,
                                    uint8_t note, uint16_t velocity,
                                    uint8_t attr_type, uint16_t attr_data,
                                    ump_packet_t *packet);

/**
 * @brief Build a MIDI 2.0 Polyphonic Pressure UMP packet
 * 
 * @param group UMP group (0-15)
 * @param channel MIDI channel (0-15)
 * @param note Note number (0-127)
 * @param pressure 32-bit pressure value
 * @param packet Output UMP packet
 * @return ESP_OK on success
 */
esp_err_t ump_build_midi2_poly_pressure(uint8_t group, uint8_t channel,
                                         uint8_t note, uint32_t pressure,
                                         ump_packet_t *packet);

/**
 * @brief Build a MIDI 2.0 Program Change UMP packet
 * 
 * @param group UMP group (0-15)
 * @param channel MIDI channel (0-15)
 * @param program Program number (0-127)
 * @param bank_valid Bank valid flag
 * @param bank_msb Bank MSB (0-127)
 * @param bank_lsb Bank LSB (0-127)
 * @param packet Output UMP packet
 * @return ESP_OK on success
 */
esp_err_t ump_build_midi2_program_change(uint8_t group, uint8_t channel,
                                          uint8_t program, bool bank_valid,
                                          uint8_t bank_msb, uint8_t bank_lsb,
                                          ump_packet_t *packet);

/**
 * @brief Build a MIDI 2.0 Channel Pressure UMP packet
 * 
 * @param group UMP group (0-15)
 * @param channel MIDI channel (0-15)
 * @param pressure 32-bit pressure value
 * @param packet Output UMP packet
 * @return ESP_OK on success
 */
esp_err_t ump_build_midi2_channel_pressure(uint8_t group, uint8_t channel,
                                            uint32_t pressure,
                                            ump_packet_t *packet);



#endif /* MIDI_TRANSLATOR_H */
