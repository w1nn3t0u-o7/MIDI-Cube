/**
 * @file midi_translator.h
 * @brief MIDI 1.0 ↔ MIDI 2.0 Protocol Translation
 * 
 * Implements bidirectional translation between MIDI 1.0 byte stream messages
 * and MIDI 2.0 Universal MIDI Packets per Appendix D of UMP specification
 */

#ifndef MIDI_TRANSLATOR_H
#define MIDI_TRANSLATOR_H

#include "midi_types.h"
#include "ump_types.h"
#include "esp_err.h"

/**
 * @brief Translation Mode
 */
typedef enum {
    MIDI_TRANSLATE_DEFAULT,      /**< Default translation per spec Appendix D */
    MIDI_TRANSLATE_MPE,          /**< MPE-aware translation */
    MIDI_TRANSLATE_CUSTOM        /**< Custom translation (user-defined) */
} midi_translate_mode_t;

/**
 * @brief Translator Configuration
 */
typedef struct {
    midi_translate_mode_t mode;  /**< Translation mode */
    uint8_t default_group;       /**< Default UMP group for MIDI 1.0 → 2.0 */
    bool    preserve_timing;     /**< Preserve timing information */
} midi_translator_config_t;

/**
 * @brief Initialize translator with configuration
 * 
 * @param config Translator configuration
 * @return ESP_OK on success
 */
esp_err_t midi_translator_init(const midi_translator_config_t *config);

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
esp_err_t midi_translate_1to2(const midi_message_t *midi1_msg, 
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
                               midi_message_t *midi1_msg);

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

#endif /* MIDI_TRANSLATOR_H */
