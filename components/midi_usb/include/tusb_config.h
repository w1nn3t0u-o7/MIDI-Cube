/**
 * @file tusb_config.h
 * @brief TinyUSB Configuration for ESP32-S3 MIDI Device
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

//--------------------------------------------------------------------
// Board Configuration
//--------------------------------------------------------------------
#define CFG_TUSB_MCU                OPT_MCU_ESP32S3

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

#define CFG_TUSB_OS                 OPT_OS_FREERTOS
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))

//--------------------------------------------------------------------
// Device Configuration
//--------------------------------------------------------------------
#define CFG_TUD_ENDPOINT0_SIZE      64

// Buffer sizes
#define CFG_TUD_MIDI                1
#define CFG_TUD_MIDI_RX_BUFSIZE     64
#define CFG_TUD_MIDI_TX_BUFSIZE     64

// Optional: Enable other USB classes if needed
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_VENDOR              0

//--------------------------------------------------------------------
// Device Stack Configuration
//--------------------------------------------------------------------
#define CFG_TUD_TASK_QUEUE_SZ       16

#endif /* TUSB_CONFIG_H */
