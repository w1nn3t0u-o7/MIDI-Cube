void func(void);
#ifndef USB_MIDI_DEVICE_H
#define USB_MIDI_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "midi_types.h"

// Descriptor length
#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

// Interface counter
typedef enum {
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_COUNT
} midi_usbd_itf_t;

// USB Endpoint numbers
typedef enum {
    EP_EMPTY = 0,
    EPNUM_MIDI,
} midi_usbd_ep_t;

// Callback types
typedef void (*midi_usbd_rx_cb_t)(uint8_t cable, uint8_t cin, midi_message_t *msg);

// Configuration structure
typedef struct {
    bool is_initialized;
    const char *manufacturer;
    const char *product;
    const char *serial;
    const char *midi_interface_name;
    uint16_t vid;  // Vendor ID
    uint16_t pid;  // Product ID
    midi_usbd_rx_cb_t rx_callback;
} midi_usbd_config_t;

// Public API
esp_err_t midi_usbd_init(void);
esp_err_t midi_usbd_register_rx_callback(midi_usbd_rx_cb_t callback);
esp_err_t midi_usbd_send(uint8_t cable, const midi_message_t *msg);
bool midi_usbd_is_mounted(void);

#endif // USB_MIDI_DEVICE_H
