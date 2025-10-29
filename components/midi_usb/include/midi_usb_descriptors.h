/**
 * @file midi_usb_descriptors.h
 * @brief USB MIDI Descriptors - Internal API
 * 
 * Provides USB descriptors for:
 * - Device identification
 * - Configuration
 * - Audio Control interface (required for MIDI)
 * - MIDI Streaming interface (Alternate Settings 0 and 1)
 * - MIDI 1.0 (Alternate Setting 0)
 * - MIDI 2.0 with UMP (Alternate Setting 1)
 */

#ifndef MIDI_USB_DESCRIPTORS_H
#define MIDI_USB_DESCRIPTORS_H

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

/**
 * @brief Get device descriptor
 * 
 * @return Pointer to device descriptor
 */
const uint8_t* midi_usb_get_device_descriptor(void);

/**
 * @brief Get configuration descriptor
 * 
 * @param midi2_enabled true if MIDI 2.0 support enabled
 * @return Pointer to configuration descriptor
 */
const uint8_t* midi_usb_get_configuration_descriptor(bool midi2_enabled);

/**
 * @brief Get string descriptors
 * 
 * @return Pointer to string descriptor array
 */
const char** midi_usb_get_string_descriptors(void);

#endif /* MIDI_USB_DESCRIPTORS_H */
