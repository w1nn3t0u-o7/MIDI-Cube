#include "midi_network.h"
#include "midi_router.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "string.h"
#include "arpa/inet.h"
#include "mdns.h"

static const char *TAG = "midi_net";

midi_net_ep_t midi_server;

// ============ Helper Functions ============ //

static inline const char *midi_net_ep_get_name(const midi_net_ep_t *ep)
{
	return (ep->name == NULL) ? "" : ep->name;
}

#if defined(CONFIG_MIDI2_UMP_STREAM_RESPONDER)
#define DEFAULT_PIID ump_product_instance_id()
#else
#define DEFAULT_PIID ""
#endif

static inline const char *midi_net_ep_get_piid(const midi_net_ep_t *ep)
{
	return (ep->product_instance_id == NULL) ? DEFAULT_PIID : ep->product_instance_id;
}

/**
 * @brief Send a command packet to a peer without session state
 * 
 * @param sock_fd Socket file descriptor
 * @param peer_addr Destination address (struct sockaddr_in *)
 * @param command_code Command code (e.g., 0x10 for INVITATION_REPLY_ACCEPTED)
 * @param command_specific_data 16-bit command-specific data field
 * @param payload Pointer to payload words (32-bit each), can be NULL
 * @param payload_len_words Number of 32-bit words in payload (0-255)
 * @return Number of bytes sent, or -1 on error
 */
static int midi_net_quick_reply(
    midi_net_ep_t *ep,
    const struct sockaddr_in *peer_addr,
    uint8_t command_code,
    uint16_t command_specific_data,
    const uint32_t *payload,
    uint8_t payload_len_words
) {
    uint8_t buf[24];
    size_t offset = 0;
    
    // Check if payload will fit
    size_t packet_size = 8 + (payload_len_words * 4);
    if (packet_size > sizeof(buf)) {
        ESP_LOGE("midi_net", "Payload too large: %d words", payload_len_words);
        return -1;
    }
    
    memcpy(&buf[offset], "MIDI", 4);
    offset += 4;
    
    // Byte 0: Command code
    buf[offset++] = command_code;
    
    // Byte 1: Payload length in words (0-255)
    buf[offset++] = payload_len_words;
    
    // Bytes 2-3: Command-specific data (big-endian 16-bit)
    buf[offset++] = (command_specific_data >> 8) & 0xFF;  // High byte
    buf[offset++] = command_specific_data & 0xFF;         // Low byte
    
    for (int i = 0; i < payload_len_words; i++) {
        uint32_t word = payload ? payload[i] : 0;

        // Convert to big-endian (network byte order)
        buf[offset++] = (word >> 24) & 0xFF;
        buf[offset++] = (word >> 16) & 0xFF;
        buf[offset++] = (word >> 8) & 0xFF;
        buf[offset++] = word & 0xFF;
    }
    
    int sent = sendto(ep->sock_fd, buf, offset, 0,
                     (struct sockaddr *)peer_addr, sizeof(*peer_addr));
    
    if (sent < 0) {
        ESP_LOGE("midi_net", "Failed to send command 0x%02X: %d", 
                command_code, errno);
        return -1;
    }
    
    ESP_LOGD("midi_net", "Sent command 0x%02X (%d bytes) to %s:%d",
            command_code, sent, inet_ntoa(peer_addr->sin_addr), 
            ntohs(peer_addr->sin_port));
    
    return sent;
}

/**
 * @brief      Quickly send a NAK message to a remote without client session
 * @param[in]  ep               The endpoint sending the NAK
 * @param[in]  peer_addr        The peer address
 * @param[in]  peer_addr_len    The peer address length
 * @param[in]  nak_reason       The NAK reason
 * @param[in]  nakd_cmd_header  The command packet header this NAK is replying to
 */
static inline int midi_net_quick_nak(midi_net_ep_t *ep,
				     const struct sockaddr_in *peer_addr,
				     const socklen_t peer_addr_len,
				     const uint8_t nak_reason,
				     const uint32_t nakd_cmd_header)
{
	return midi_net_quick_reply(ep, peer_addr,
				    MIDI_NET_CMD_NAK, nak_reason << 8,
				    &nakd_cmd_header, 1);
}

static inline void midi_net_free_session(midi_net_session_t *session)
{
	ESP_LOGI(TAG, "Free client session");
    
    session->state = MIDI_NET_SESSION_NOT_INIT;
    session->tx_ump_seq = 0;
    session->rx_ump_seq = 0;
    memset(&session->peer_addr, 0, sizeof(session->peer_addr));
}

static inline midi_net_session_t *midi_net_match_session(midi_net_ep_t *ep,
							      struct sockaddr_in *peer_addr,
							      socklen_t peer_addr_len)
{
	for (size_t i = 0; i < CONFIG_MIDI_NET_MAX_SESSIONS; i++) {
		if (ep->sessions[i].peer_addr_len == peer_addr_len &&
		    memcmp(&ep->sessions[i].peer_addr, peer_addr, peer_addr_len) == 0) {
			ESP_LOGI("midi_net", "Found matching client session %d", i);
			return &ep->sessions[i];
		}
	}

	return NULL;
}

static inline void midi_net_free_inactive_sessions(midi_net_ep_t *ep)
{
    const uint8_t bye_timeout[] = {
        'M', 'I', 'D', 'I',      // Signature
        MIDI_NET_CMD_BYE,        // Command
        0,                       // Payload = 0 words
        0x04, 0                  // Timeout reason
    };
    
    for (size_t i = 0; i < MIDI_NET_MAX_SESSIONS; i++) {
        midi_net_session_t *sess = &ep->sessions[i];
        
        if (sess->state != MIDI_NET_SESSION_IDLE &&
            sess->state != MIDI_NET_SESSION_ESTABLISHED) {
            
            ESP_LOGW(TAG, "Cleanup inactive session %d", i);
            
            int sent = sendto(ep->sock_fd, bye_timeout, sizeof(bye_timeout), 0,
                             (struct sockaddr *)&sess->peer_addr, 
                             sizeof(sess->peer_addr));
            
            if (sent < 0) {
                ESP_LOGE(TAG, "Failed to send BYE: %d", errno);
            }
            
            midi_net_free_session(sess);
        }
    }
}

static inline midi_net_session_t *midi_net_try_alloc_session(midi_net_ep_t *ep,
								  struct sockaddr_in *peer_addr,
								  socklen_t peer_addr_len)
{
	midi_net_session_t *sess;
	for (size_t i = 0; i < CONFIG_MIDI_NET_MAX_SESSIONS; i++) {
		sess = &ep->sessions[i];
		if (sess->state == MIDI_NET_SESSION_NOT_INIT) {
			sess->state = MIDI_NET_SESSION_IDLE;
			sess->peer_addr_len = peer_addr_len;
			sess->ep = ep;
			memcpy(&sess->peer_addr, peer_addr, peer_addr_len);
			ESP_LOGI("midi_net", "new client session (%d)", i);
			return sess;
		}
	}

	return NULL;
}

static inline midi_net_session_t *midi_net_alloc_session(midi_net_ep_t *ep,
							      struct sockaddr_in *peer_addr,
							      socklen_t peer_addr_len)
{
	midi_net_session_t *session;
	session = midi_net_try_alloc_session(ep, peer_addr, peer_addr_len);
	if (session == NULL) {
		midi_net_free_inactive_sessions(ep);
		session = midi_net_try_alloc_session(ep, peer_addr, peer_addr_len);
	}

	if (session == NULL) {
		ESP_LOGE("midi_net", "No available client session");
	}

	return session;
}

/**
 * @brief Send INVITATION_REPLY_ACCEPTED immediately (no buffering)
 * 
 * @param session The session to send reply to
 * @param authentication_state Authentication state (0x00 for no auth)
 * @return 0 on success, -1 on error
 */
static int midi_net_send_inv_reply(midi_net_session_t *session,
                                    uint8_t authentication_state) 
{
    // Get endpoint name and product instance ID
    const char *name = midi_net_ep_get_name(session->ep);
    const char *piid = midi_net_ep_get_piid(session->ep);
    const size_t namelen = strlen(name);
    const size_t piidlen = strlen(piid);
    
    // Calculate word counts (padded to 4-byte boundaries)
    const size_t namelen_words = DIV_ROUND_UP(namelen, 4);
    const size_t piidlen_words = DIV_ROUND_UP(piidlen, 4);
    const size_t total_words = namelen_words + piidlen_words;
    
    // Build command-specific data: name_words in high byte, auth_state in low byte
    const uint16_t specific_data = (namelen_words << 8) | authentication_state;
    
    // Build packet directly in stack buffer
    uint8_t buf[256];
    size_t offset = 0;
    
    // Add "MIDI" signature
    memcpy(&buf[offset], "MIDI", 4);
    offset += 4;
    
    // Add command header
    buf[offset++] = MIDI_NET_CMD_INV_REPLY_ACCEPTED;
    buf[offset++] = total_words;
    buf[offset++] = (specific_data >> 8) & 0xFF;
    buf[offset++] = specific_data & 0xFF;
    
    // Add endpoint name (padded to 4-byte boundary)
    memcpy(&buf[offset], name, namelen);
    offset += namelen;
    size_t name_padding = (4 - (namelen % 4)) % 4;
    for (size_t i = 0; i < name_padding; i++) {
        buf[offset++] = 0;
    }
    
    // Add product instance ID (padded to 4-byte boundary)
    memcpy(&buf[offset], piid, piidlen);
    offset += piidlen;
    size_t piid_padding = (4 - (piidlen % 4)) % 4;
    for (size_t i = 0; i < piid_padding; i++) {
        buf[offset++] = 0;
    }
    
    // Send immediately
    int sent = sendto(session->ep->sock_fd, buf, offset, 0,
                     (struct sockaddr *)&session->peer_addr,
                     sizeof(session->peer_addr));
    
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send INV_REPLY: %d", errno);
        return -1;
    }
    
    ESP_LOGI(TAG, "Sent INVITATION_REPLY_ACCEPTED (%d bytes) to %s:%d",
            sent, inet_ntoa(session->peer_addr.sin_addr),
            ntohs(session->peer_addr.sin_port));
    
    return 0;
}

/**
 * @brief Send a command packet to a session
 * Builds and sends packet immediately (no buffering)
 * 
 * @param session Session to send to
 * @param command_code Command code (e.g., PING_REPLY, SESSION_RESET_REPLY)
 * @param command_specific_data 16-bit command-specific data
 * @param payload Pointer to payload words (32-bit each), can be NULL
 * @param payload_len_words Number of 32-bit words in payload (0-255)
 * @return 0 on success, -1 on error
 */
static int midi_net_session_sendcmd(
    midi_net_session_t *session,
    uint8_t command_code,
    uint16_t command_specific_data,
    const uint32_t *payload,
    uint8_t payload_len_words
) {
    if (session == NULL || session->ep == NULL) {
        ESP_LOGE(TAG, "Invalid session");
        return -1;
    }
    
    // Build packet in stack buffer
    uint8_t buf[256];  // Max packet size
    size_t offset = 0;
    
    // Check payload fits
    size_t packet_size = 8 + (payload_len_words * 4);
    if (packet_size > sizeof(buf)) {
        ESP_LOGE(TAG, "Payload too large: %d words", payload_len_words);
        return -1;
    }
    
    // Add "MIDI" signature
    memcpy(&buf[offset], "MIDI", 4);
    offset += 4;
    
    // Add command header
    buf[offset++] = command_code;
    buf[offset++] = payload_len_words;
    buf[offset++] = (command_specific_data >> 8) & 0xFF;
    buf[offset++] = command_specific_data & 0xFF;
    
    // Add payload words (convert to network byte order)
    for (int i = 0; i < payload_len_words; i++) {
        uint32_t word = payload ? payload[i] : 0;
        buf[offset++] = (word >> 24) & 0xFF;
        buf[offset++] = (word >> 16) & 0xFF;
        buf[offset++] = (word >> 8) & 0xFF;
        buf[offset++] = word & 0xFF;
    }
    
    // Send immediately
    int sent = sendto(session->ep->sock_fd, buf, offset, 0,
                     (struct sockaddr *)&session->peer_addr,
                     sizeof(session->peer_addr));
    
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send command 0x%02X: %d", command_code, errno);
        return -1;
    }
    
    ESP_LOGD(TAG, "Sent command 0x%02X (%d bytes) to %s:%d",
            command_code, sent, inet_ntoa(session->peer_addr.sin_addr),
            ntohs(session->peer_addr.sin_port));
    
    return 0;
}



static size_t _build_ump_command(uint8_t *buf, uint8_t cmd_code,
                                 uint16_t seq_num, const uint32_t *payload,
                                 uint8_t payload_words)
{
    size_t offset = 0;
    
    memcpy(&buf[offset], "MIDI", 4);
    offset += 4;
    buf[offset++] = cmd_code;
    buf[offset++] = payload_words;
    buf[offset++] = (seq_num >> 8) & 0xFF;
    buf[offset++] = seq_num & 0xFF;
    
    for (int i = 0; i < payload_words; i++) {
        uint32_t word = payload ? payload[i] : 0;
        buf[offset++] = (word >> 24) & 0xFF;
        buf[offset++] = (word >> 16) & 0xFF;
        buf[offset++] = (word >> 8) & 0xFF;
        buf[offset++] = word & 0xFF;
    }
    
    return offset;
}

static void midi_net_proc_udp_packet(midi_net_ep_t *ep, uint8_t *data, size_t len,
                            struct sockaddr_in *peer_addr, socklen_t peer_addr_len)
{
    if (len < 8 || memcmp(data, "MIDI", 4) != 0) {
        return;
    }
    
    midi_net_session_t *session;
    size_t offset = 4;
    while (offset + 4 <= len) {

        uint8_t cmd_code = data[offset];
        uint8_t payload_words = data[offset + 1];
        uint16_t cmd_data = ((uint16_t)data[offset + 2] << 8) | data[offset + 3];
        uint32_t cmd_header = 
            ((uint32_t)cmd_code << 24) |
            ((uint32_t)payload_words << 16) |
            (uint32_t)cmd_data;
        size_t payload_size = payload_words * 4;
        
        if (offset + 4 + payload_size > len) {
            midi_net_quick_nak(ep, peer_addr, peer_addr_len,
				   NAK_CMD_MALFORMED, cmd_header);
            ESP_LOGE("midi_net", "Incomplete UDP MIDI command packet payload");
            break;
        }
        
        ESP_LOGI(TAG, "CMD 0x%02X from %s", cmd_code, inet_ntoa(peer_addr->sin_addr));
        
        switch (cmd_code) {
        case MIDI_NET_CMD_PING: {
            // Simple reply with 1 word from the PING request
            if (payload_words != 1) {
                midi_net_quick_nak(ep, peer_addr, peer_addr_len,
                                   NAK_CMD_MALFORMED, cmd_header);
                ESP_LOGE(TAG, "Invalid payload length for PING packet");
                break;
            }
            uint32_t ping_word = 
                ((uint32_t)data[offset + 4] << 24) |
                ((uint32_t)data[offset + 5] << 16) |
                ((uint32_t)data[offset + 6] << 8) |
                (uint32_t)data[offset + 7];
            
            midi_net_quick_reply(ep, peer_addr,
                                 MIDI_NET_CMD_PING_REPLY, 0,
                                 &ping_word, 1);
            break;
        }
        case MIDI_NET_CMD_INV: {

            session = midi_net_alloc_session(ep, peer_addr, peer_addr_len);

            if (session == NULL) {
			    break;
		    }

		    session->state = MIDI_NET_SESSION_ESTABLISHED;
		    midi_net_send_inv_reply(session, AUTH_STATE_FIRST_REQUEST);
		    break;
        }
        case MIDI_NET_CMD_UMP_DATA: {
            session = midi_net_match_session(ep, peer_addr, peer_addr_len);
            if (!SESSION_HAS_STATE(session, MIDI_NET_SESSION_ESTABLISHED)) {
                midi_net_quick_nak(ep, peer_addr, peer_addr_len,
                        NAK_CMD_NOT_EXPECTED, cmd_header);
                ESP_LOGW("midi_net", "Receiving UMP data without established session");
                break;
            }

            if (session->rx_ump_seq == cmd_data) {
                session->rx_ump_seq++;
            } else {
                ESP_LOGW("midi_net", "UMP Rx sequence mismatch (got %d, expected %d)",
                        cmd_data, session->rx_ump_seq);
                session->rx_ump_seq = 1 + cmd_data;
            }

            if (payload_words < 1 || payload_words > 4) {
                midi_net_quick_nak(ep, peer_addr, peer_addr_len,
                        NAK_CMD_MALFORMED, cmd_header);
                ESP_LOGE("midi_net", "Invalid UMP length");
                break;
            }

            ump_packet_t ump = {0};
            size_t ump_offset = offset + 4;  // Skip command header
    
            for (size_t i = 0; i < payload_words; i++) {
                uint32_t word = 
                    ((uint32_t)data[ump_offset + 0] << 24) |
                    ((uint32_t)data[ump_offset + 1] << 16) |
                    ((uint32_t)data[ump_offset + 2] << 8) |
                    (uint32_t)data[ump_offset + 3];
                ump.words[i] = word;  // Already in host byte order
                ump_offset += 4;
            }

            if (UMP_GET_NUM_WORDS(ump) != payload_words) {
                midi_net_quick_nak(ep, peer_addr, peer_addr_len,
                        NAK_CMD_MALFORMED, cmd_header);
                ESP_LOGE("midi_net", "Invalid UMP payload size for its message type");
                break;
            }

            midi_net_broadcast_ump(ep, &ump);

            if (ep->rx_callback != NULL) {
                ep->rx_callback(session, &ump);
            }
            break;
        }
        
        case MIDI_NET_CMD_BYE: {
            session = midi_net_match_session(ep, peer_addr, peer_addr_len);
		    if (session == NULL) {
			    midi_net_quick_nak(ep, peer_addr, peer_addr_len,
					                NAK_CMD_NOT_EXPECTED, cmd_header);
			    ESP_LOGW("midi_net", "Receiving BYE without session");
			    break;
		    }
		    //net_buf_pull(rx, payload_len);
		    midi_net_quick_reply(ep, peer_addr, MIDI_NET_CMD_BYE_REPLY, 0, NULL, 0);
		    midi_net_free_session(session);
		    break;
        }
        case MIDI_NET_CMD_SESSION_RESET: {
            session = midi_net_match_session(ep, peer_addr, peer_addr_len);
            if (!SESSION_HAS_STATE(session, MIDI_NET_SESSION_ESTABLISHED)) {
                ESP_LOGW("midi_net", "Receiving session reset without established session");
                midi_net_quick_nak(ep, peer_addr, peer_addr_len,
                        NAK_CMD_NOT_EXPECTED, cmd_header);
                break;
            }

            session->tx_ump_seq = 0;
            session->rx_ump_seq = 0;
            ESP_LOGI(TAG, "Reset session");
            midi_net_session_sendcmd(session, MIDI_NET_CMD_SESSION_RESET_REPLY, 0, NULL, 0);
            break;
        }
        default:
            ESP_LOGD(TAG, "Unknown command: 0x%02X", cmd_code);
            midi_net_quick_nak(ep, peer_addr, peer_addr_len,
				   NAK_CMD_NOT_SUPPORTED, cmd_header);
            break;
        }
        
        offset += 4 + payload_size;
    }
}

// ============ SINGLE RX TASK WITH select() ============

static void midi_net_rx_task(void *pvParameters)
{
    midi_net_ep_t *ep = (midi_net_ep_t *)pvParameters;
    static uint8_t rx_buffer[MIDI_NET_BUFSIZE];
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    
    ESP_LOGI(TAG, "RX Task started - listening on port %d", ntohs(ep->local_addr.sin_port));
    
    while (1) {
        fd_set readset;
        struct timeval timeout;
        
        // Prepare fd_set with the endpoint socket
        FD_ZERO(&readset);
        FD_SET(ep->sock_fd, &readset);
        
        // Set timeout to 1 second
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;
        
        // Wait for socket to have data
        int select_ret = select(ep->sock_fd + 1, &readset, NULL, NULL, &timeout);
        
        if (select_ret < 0) {
            ESP_LOGE(TAG, "select() error: %d", errno);
            continue;
        }

        if (FD_ISSET(ep->sock_fd, &readset)) {
            // Socket has data
            int recv_len = recvfrom(ep->sock_fd, rx_buffer, MIDI_NET_BUFSIZE, 0,
                                (struct sockaddr *)&peer_addr, &peer_addr_len);
            
            if (recv_len > 0) {
                /* Check for magic header */
	            if (recv_len < 4 || memcmp(rx_buffer, "MIDI", 4) != 0) {
                    ESP_LOGW(TAG, "Not a MIDI packet");
                    continue;
                }

                ESP_LOGI(TAG, "Received %d bytes from %s:%d",
                        recv_len, inet_ntoa(peer_addr.sin_addr),
                        ntohs(peer_addr.sin_port));

                midi_net_proc_udp_packet(ep, rx_buffer, recv_len, &peer_addr, peer_addr_len);
            }
        }
    }
}

// ============ PUBLIC API ============

esp_err_t midi_net_ep_init(const char *name, const char *piid, uint16_t port)
{
    if (name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    midi_server.name = name;
    midi_server.product_instance_id = piid;
    midi_server.local_addr.sin_port = port ? port : MIDI_NET_PORT;
    midi_server.sock_fd = -1;
    midi_server.rx_callback = midi_net_rx_callback;

    memset(midi_server.sessions, 0, sizeof(midi_server.sessions));
    
    ESP_LOGI(TAG, "Initialized endpoint: %s (PIID: %s) on port %d",
            midi_server.name, midi_server.product_instance_id, midi_server.local_addr.sin_port);

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return ESP_FAIL;
    }
    
    // Allow reusing address
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR");
    }
    
    // Bind to INADDR_ANY on our port (listens on ALL interfaces)
    midi_server.local_addr.sin_family = AF_INET;
    midi_server.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
    midi_server.local_addr.sin_port = htons(midi_server.local_addr.sin_port);

    if (bind(sock, (struct sockaddr *)&midi_server.local_addr, sizeof(midi_server.local_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(sock);
        return ESP_FAIL;
    }

    midi_server.sock_fd = sock;
    
    // Create RX task
    BaseType_t ret = xTaskCreatePinnedToCore(
        midi_net_rx_task,
        "midi_net_rx",
        4096,
        &midi_server,
        5,
        &midi_server.rx_task_handle,
        1  // Core 1
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        close(sock);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP MIDI listening on port %d (all interfaces)",
            ntohs(midi_server.local_addr.sin_port));
    
    return ESP_OK;
}

esp_err_t midi_net_send_ump(midi_net_session_t *session, const ump_packet_t *ump)
{
    if (session == NULL || session->state == MIDI_NET_SESSION_IDLE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Determine UMP size (1-4 words)
    uint8_t ump_words = 1;
    for (int i = 3; i >= 0; i--) {
        if (ump->words[i] != 0) {
            ump_words = i + 1;
            break;
        }
    }
    
    uint8_t buf[MIDI_NET_BUFSIZE];
    size_t len = _build_ump_command(buf, MIDI_NET_CMD_UMP_DATA, session->tx_ump_seq++,
                                    ump->words, ump_words);


    int ret = sendto(midi_server.sock_fd, buf, len, 0,
                    (struct sockaddr *)&session->peer_addr,
                    sizeof(session->peer_addr));
    
    return ret > 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t midi_net_broadcast_ump(midi_net_ep_t *ep, const ump_packet_t *ump)
{
    if (ep == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < MIDI_NET_MAX_SESSIONS; i++) {
        if (ep->sessions[i].state == MIDI_NET_SESSION_ESTABLISHED) {
            midi_net_send_ump(&ep->sessions[i], ump);
        }
    }
    
    return ESP_OK;
}

esp_err_t midi_net_ep_stop(void)
{
    if (midi_server.rx_task_handle) {
        vTaskDelete(midi_server.rx_task_handle);
        midi_server.rx_task_handle = NULL;
    }

    if (midi_server.sock_fd >= 0) {
        close(midi_server.sock_fd);
        midi_server.sock_fd = -1;
    }
    
    return ESP_OK;
}

esp_err_t midi_net_register_mdns(const char *hostname)
{
    if (hostname == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize mDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %d", err);
        return err;
    }
    
    // Set hostname
    err = mdns_hostname_set(hostname);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS hostname: %d", err);
        return err;
    }
    
    // Set instance name (used in PTR record)
    char instance_name[64];
    snprintf(instance_name, sizeof(instance_name), "%s-MIDI", hostname);
    err = mdns_instance_name_set(instance_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS instance name: %d", err);
        return err;
    }
    
    // Add _midi2._udp service
    uint16_t port = ntohs(midi_server.local_addr.sin_port);
    err = mdns_service_add(NULL, "_midi2", "_udp", port, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add mDNS service: %d", err);
        return err;
    }
    
    // Add TXT records (per spec section 4.4)
    mdns_txt_item_t txt_records[2] = {
        {"UMPEndpointName", midi_server.name},
        {"ProductInstanceId", midi_server.product_instance_id ? 
                             midi_server.product_instance_id : "ESP32-MIDI"}
    };
    
    err = mdns_service_txt_set("_midi2", "_udp", txt_records, 2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set TXT records: %d", err);
        return err;
    }
    
    ESP_LOGI(TAG, "mDNS service registered: %s._midi2._udp.local (port %d)",
            instance_name, port);
    ESP_LOGI(TAG, "  UMPEndpointName: %s", midi_server.name);
    ESP_LOGI(TAG, "  ProductInstanceId: %s", 
            midi_server.product_instance_id ? midi_server.product_instance_id : "ESP32-MIDI");
    
    return ESP_OK;
}
