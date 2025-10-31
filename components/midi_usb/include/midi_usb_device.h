/**
 * @file midi_usb_device.h
 * @brief USB MIDI Device Mode - Internal API
 * 
 * Implements USB Device mode using TinyUSB stack.
 * ESP32-S3 acts as a USB MIDI device when connected to a PC.
 * 
 * Supports:
 * - USB-MIDI 1.0 (4-byte Event Packets) - Alternate Setting 0
 * - USB-MIDI 2.0 (UMP) - Alternate Setting 1
 * - Multiple virtual cables (1-16)
 * - Bidirectional streaming (IN/OUT endpoints)
 * 
 * Based on:
 * - USB Device Class Definition for MIDI Devices v1.0 (1999)
 * - USB Device Class Definition for MIDI Devices v2.0 (2020)
 * - TinyUSB MIDI device class driver
 */

#ifndef MIDI_USB_DEVICE_H
#define MIDI_USB_DEVICE_H

#include "midi_usb.h"
#include "esp_err.h"

/**
 * @brief Initialize USB Device mode
 * 
 * Configures TinyUSB stack, registers MIDI device class,
 * sets up USB descriptors for MIDI 1.0 and optionally MIDI 2.0
 * 
 * @param config USB MIDI configuration
 * @return ESP_OK on success
 */
esp_err_t midi_usb_device_init(const midi_usb_config_t *config);

/**
 * @brief Deinitialize USB Device mode
 * 
 * Stops TinyUSB, releases resources
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_usb_device_deinit(void);

/**
 * @brief Send packet in Device mode
 * 
 * Sends USB-MIDI Event Packet or UMP to PC via USB IN endpoint
 * 
 * @param packet USB MIDI packet
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if endpoint busy
 */
esp_err_t midi_usb_device_send_packet(const midi_usb_packet_t *packet);

/**
 * @brief Check if USB device is mounted (connected to PC)
 * 
 * @return true if PC has enumerated the device
 */
bool midi_usb_device_is_mounted(void);

/**
 * @brief Flush TX FIFO
 * 
 * Ensures all pending data is transmitted
 * 
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t midi_usb_device_flush(uint32_t timeout_ms);

#endif /* MIDI_USB_DEVICE_H */
