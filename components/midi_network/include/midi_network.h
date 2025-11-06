#ifndef MIDI_NETWORK_H
#define MIDI_NETWORK_H

#include "esp_err.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ump_types.h"

/**
 * @brief Network MIDI 2.0 UDP Transport Component
 * 
 * Handles UDP packet transmission/reception for Network MIDI 2.0
 * Implements command packets, session management, and UMP data exchange
 */

#define MIDI_NET_PORT CONFIG_MIDI_NET_PORT
#define MIDI_NET_MAX_SESSIONS CONFIG_MIDI_NET_MAX_SESSIONS

#define MIDI_NET_UDP_PACKET_START 0x4D494449  // "MIDI" in ASCII
#define MIDI_NET_BUFSIZE 256  // Keep under MTU(1400 bytes)
#define MIDI_NET_CRYPTO_NONCE_SIZE 16

// Command codes (from spec section 5.5)
#define MIDI_NET_CMD_INV 0x01
#define MIDI_NET_CMD_INV_WITH_AUTH 0x02
#define MIDI_NET_CMD_INV_WITH_USER_AUTH 0x03
#define MIDI_NET_CMD_INV_REPLY_ACCEPTED 0x10
#define MIDI_NET_CMD_INV_REPLY_PENDING 0x11
#define MIDI_NET_CMD_INV_REPLY_AUTH_REQ 0x12
#define MIDI_NET_CMD_INV_REPLY_USER_AUTH_REQ 0x13
#define MIDI_NET_CMD_PING 0x20
#define MIDI_NET_CMD_PING_REPLY 0x21
#define MIDI_NET_CMD_RETRANSMIT_REQUEST 0x80
#define MIDI_NET_CMD_RETRANSMIT_ERROR 0x81
#define MIDI_NET_CMD_SESSION_RESET 0x82
#define MIDI_NET_CMD_SESSION_RESET_REPLY 0x83
#define MIDI_NET_CMD_NAK 0x8F
#define MIDI_NET_CMD_BYE 0xF0
#define MIDI_NET_CMD_BYE_REPLY 0xF1
#define MIDI_NET_CMD_UMP_DATA 0xFF

// Session states
typedef enum {
    MIDI_NET_SESSION_NOT_INIT,
    MIDI_NET_SESSION_IDLE,
    MIDI_NET_SESSION_PENDING_INV,
    MIDI_NET_SESSION_AUTH_REQUIRED,
    MIDI_NET_SESSION_ESTABLISHED,
    MIDI_NET_SESSION_PENDING_RESET,
    MIDI_NET_SESSION_PENDING_BYE,
} midi_net_session_state_t;

typedef struct midi_net_ep midi_net_ep_t;
typedef struct midi_net_session midi_net_session_t;

// Session structure
typedef struct midi_net_session {
    midi_net_session_state_t state;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len;
    uint16_t tx_ump_seq;  // Transmit sequence number
    uint16_t rx_ump_seq;  // Receive sequence number
    struct midi_net_ep *ep;
} midi_net_session_t;

// Endpoint structure
typedef struct midi_net_ep {
    const char *name;           // Endpoint name
    const char *product_instance_id;           // Product Instance ID
    struct sockaddr_in local_addr; // Local address
    //struct pollfd socket_fd;    // Socket file descriptor
    midi_net_session_t sessions[MIDI_NET_MAX_SESSIONS];
    
    // Callback for received UMP packets
    void (*rx_callback)(midi_net_session_t *session, const ump_packet_t *ump);
    TaskHandle_t rx_task_handle; // RX task handle
} midi_net_ep_t;

/**
 * @brief Initialize UDP MIDI transport endpoint
 */
esp_err_t midi_net_ep_init(midi_net_ep_t *ep, const char *name,
                           const char *piid, uint16_t port);

/**
 * @brief Start listening for connections
 */
esp_err_t midi_net_ep_start(midi_net_ep_t *ep);

/**
 * @brief Send UMP packet to a session
 */
esp_err_t midi_net_send_ump(midi_net_session_t *session, const ump_packet_t *ump);

/**
 * @brief Broadcast UMP packet to all established sessions
 */
esp_err_t midi_net_broadcast_ump(midi_net_ep_t *ep, const ump_packet_t *ump);

/**
 * @brief Process incoming UDP packet
 */
void netmidi2_handle_packet(midi_net_ep_t *ep, uint8_t *data, size_t len, 
                            struct sockaddr_in *peer_addr);

#endif // MIDI_NETWORK_H