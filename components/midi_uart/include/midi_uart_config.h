/**
 * @file midi_uart_config.h
 * @brief MIDI UART Hardware Configuration
 * 
 * DIN-5 MIDI 1.0 over UART at 31,250 baud
 * Based on MIDI 1.0 Electrical Specification
 */

#ifndef MIDI_UART_CONFIG_H
#define MIDI_UART_CONFIG_H

#include "sdkconfig.h"

/**
 * @defgroup MIDI_UART_HW Hardware Configuration
 * @{
 */

// MIDI Standard Baud Rate (MIDI 1.0 Spec)
#define MIDI_UART_BAUD_RATE         31250

// UART Configuration
#define MIDI_UART_PORT              CONFIG_MIDI_UART_PORT_NUM
#define MIDI_UART_TX_PIN            CONFIG_MIDI_UART_TX_PIN
#define MIDI_UART_RX_PIN            CONFIG_MIDI_UART_RX_PIN

// Buffer Sizes
#define MIDI_UART_RX_BUF_SIZE       CONFIG_MIDI_UART_RX_BUFFER_SIZE
#define MIDI_UART_TX_BUF_SIZE       CONFIG_MIDI_UART_TX_BUFFER_SIZE
#define MIDI_UART_EVENT_QUEUE_SIZE  CONFIG_MIDI_UART_EVENT_QUEUE_SIZE

// Task Configuration
#define MIDI_UART_TASK_STACK_SIZE   CONFIG_MIDI_UART_TASK_STACK_SIZE
#define MIDI_UART_TASK_PRIORITY     CONFIG_MIDI_UART_TASK_PRIORITY
#define MIDI_UART_TASK_CORE         1  // Run on Core 1 (Core 0 = WiFi)

// MIDI Timing (from MIDI 1.0 Spec)
#define MIDI_BYTE_TIME_US           320   // Time to transmit 1 byte at 31250 baud
#define MIDI_3BYTE_MSG_TIME_US      960   // Time for 3-byte message

/** @} */

#endif /* MIDI_UART_CONFIG_H */
