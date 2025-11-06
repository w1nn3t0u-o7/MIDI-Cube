#include "midi_network.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "string.h"
#include "arpa/inet.h"

static const char *TAG = "midi_net";

// ============ Helper Functions ============

static netmidi2_session_t* _find_or_alloc_session(netmidi2_ep_t *ep,
                                                   struct sockaddr_in *addr)
{
    // Try to find existing session
    for (int i = 0; i < NETMIDI2_MAX_SESSIONS; i++) {
        netmidi2_session_t *sess = &ep->sessions[i];
        if (sess->state != NETMIDI2_SESSION_IDLE &&
            sess->peer_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            sess->peer_addr.sin_port == addr->sin_port) {
            return sess;
        }
    }
    
    // Allocate new session
    for (int i = 0; i < NETMIDI2_MAX_SESSIONS; i++) {
        netmidi2_session_t *sess = &ep->sessions[i];
        if (sess->state == NETMIDI2_SESSION_IDLE) {
            memcpy(&sess->peer_addr, addr, sizeof(*addr));
            sess->state = NETMIDI2_SESSION_ESTABLISHED;
            sess->tx_ump_seq = 0;
            sess->rx_ump_seq = 0;
            ESP_LOGI(TAG, "New session: %s:%d",
                    inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
            return sess;
        }
    }
    
    return NULL;
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

static void _process_packet(netmidi2_ep_t *ep, uint8_t *data, size_t len,
                            struct sockaddr_in *peer_addr)
{
    if (len < 8 || memcmp(data, "MIDI", 4) != 0) {
        return;
    }
    
    size_t offset = 4;
    while (offset + 4 <= len) {
        uint8_t cmd_code = data[offset];
        uint8_t payload_words = data[offset + 1];
        uint16_t seq_num = ((uint16_t)data[offset + 2] << 8) | data[offset + 3];
        size_t payload_size = payload_words * 4;
        
        if (offset + 4 + payload_size > len) {
            break;
        }
        
        uint32_t *payload = (uint32_t *)&data[offset + 4];
        
        ESP_LOGD(TAG, "CMD 0x%02X from %s", cmd_code, inet_ntoa(peer_addr->sin_addr));
        
        switch (cmd_code) {
        case CMD_INVITATION: {
            netmidi2_session_t *sess = _find_or_alloc_session(ep, peer_addr);
            if (sess) {
                // Send invitation reply
                uint8_t reply_buf[256];
                const char *name = ep->name ? ep->name : "ESP32";
                const char *piid = ep->piid ? ep->piid : "unknown";
                size_t name_len = strlen(name);
                size_t piid_len = strlen(piid);
                size_t name_words = (name_len + 3) / 4;
                size_t piid_words = (piid_len + 3) / 4;
                
                size_t reply_offset = 0;
                memcpy(&reply_buf[reply_offset], "MIDI", 4);
                reply_offset += 4;
                reply_buf[reply_offset++] = CMD_INVITATION_REPLY_ACCEPTED;
                reply_buf[reply_offset++] = name_words + piid_words;
                reply_buf[reply_offset++] = 0;
                reply_buf[reply_offset++] = 0;
                
                memcpy(&reply_buf[reply_offset], name, name_len);
                reply_offset += name_len;
                while (name_len % 4) {
                    reply_buf[reply_offset++] = 0;
                    name_len++;
                }
                
                memcpy(&reply_buf[reply_offset], piid, piid_len);
                reply_offset += piid_len;
                while (piid_len % 4) {
                    reply_buf[reply_offset++] = 0;
                    piid_len++;
                }
                
                sendto(ep->sock, reply_buf, reply_offset, 0,
                      (struct sockaddr *)peer_addr, sizeof(*peer_addr));
            }
            break;
        }
        
        case CMD_UMP_DATA: {
            netmidi2_session_t *sess = _find_or_alloc_session(ep, peer_addr);
            if (sess && ep->rx_callback) {
                midi_ump_t ump;
                memcpy(ump.data, payload, payload_words * 4);
                ep->rx_callback(sess, &ump);
            }
            break;
        }
        
        case CMD_BYE: {
            for (int i = 0; i < NETMIDI2_MAX_SESSIONS; i++) {
                netmidi2_session_t *sess = &ep->sessions[i];
                if (sess->state != NETMIDI2_SESSION_IDLE &&
                    sess->peer_addr.sin_addr.s_addr == peer_addr->sin_addr.s_addr &&
                    sess->peer_addr.sin_port == peer_addr->sin_port) {
                    sess->state = NETMIDI2_SESSION_IDLE;
                    ESP_LOGI(TAG, "Session closed");
                }
            }
            break;
        }
        
        default:
            ESP_LOGD(TAG, "Unknown command: 0x%02X", cmd_code);
            break;
        }
        
        offset += 4 + payload_size;
    }
}

// ============ SINGLE RX TASK WITH select() ============

static void midi_net_rx_task(void *pvParameters)
{
    midi_net_ep_t *ep = (midi_net_ep_t *)pvParameters;
    uint8_t rx_buffer[MIDI_NET_BUFSIZE];
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len;
    
    ESP_LOGI(TAG, "RX Task started - listening on port %d", ep->local_addr.sin_port);
    
    while (1) {
        fd_set readset;
        struct timeval timeout;
        
        // Prepare fd_set with the endpoint socket
        FD_ZERO(&readset);
        FD_SET(ep->sock, &readset);
        
        // Set timeout to 1 second
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // Wait for socket to have data
        int select_ret = select(ep->sock + 1, &readset, NULL, NULL, &timeout);
        
        if (select_ret < 0) {
            ESP_LOGE(TAG, "select() error: %d", errno);
            continue;
        }
        
        if (select_ret == 0) {
            // Timeout - continue loop
            continue;
        }
        
        // Socket has data
        peer_addr_len = sizeof(peer_addr);
        int recv_len = recvfrom(ep->sock, rx_buffer, NETMIDI2_BUFSIZE, 0,
                               (struct sockaddr *)&peer_addr, &peer_addr_len);
        
        if (recv_len > 0) {
            ESP_LOGD(TAG, "Received %d bytes from %s:%d",
                    recv_len, inet_ntoa(peer_addr.sin_addr),
                    ntohs(peer_addr.sin_port));
            
            _process_packet(ep, rx_buffer, recv_len, &peer_addr);
        }
    }
}

// ============ PUBLIC API ============

esp_err_t midi_net_ep_init(midi_net_ep_t *ep, const char *name,
                           const char *piid, uint16_t port)
{
    if (ep == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ep->name = name;
    ep->product_instance_id = piid;
    ep->local_addr.sin_port = port ? port : MIDI_NET_PORT;
    
    //memset(ep->sessions, 0, sizeof(ep->sessions));
    
    ESP_LOGI(TAG, "Initialized endpoint: %s (PIID: %s) on port %d",
            ep->name, ep->product_instance_id, ep->local_addr.sin_port);
    
    return ESP_OK;
}

esp_err_t midi_net_ep_start(midi_net_ep_t *ep)
{
    if (ep == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
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
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
    addr.sin_port = htons(ep->local_addr.sin_port);
    
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(sock);
        return ESP_FAIL;
    }
    
    // Create RX task
    BaseType_t ret = xTaskCreatePinnedToCore(
        _netmidi2_rx_task,
        "netmidi2_rx",
        4096,
        ep,
        5,
        &ep->rx_task_handle,
        1  // Core 1
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        close(sock);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP MIDI listening on port %d (all interfaces)",
            ep->local_addr.sin_port);
    
    return ESP_OK;
}

esp_err_t netmidi2_send(netmidi2_session_t *session, const midi_ump_t *ump)
{
    if (session == NULL || session->state == NETMIDI2_SESSION_IDLE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Determine UMP size (1-4 words)
    uint8_t ump_words = 1;
    for (int i = 3; i >= 0; i--) {
        if (ump->data[i] != 0) {
            ump_words = i + 1;
            break;
        }
    }
    
    uint8_t buf[NETMIDI2_BUFSIZE];
    size_t len = _build_ump_command(buf, CMD_UMP_DATA, session->tx_ump_seq++,
                                    ump->data, ump_words);
    
    // Get socket from session (we'll fix this)
    int ret = sendto(-1, buf, len, 0,  // TODO: pass ep->sock
                    (struct sockaddr *)&session->peer_addr,
                    sizeof(session->peer_addr));
    
    return ret > 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t netmidi2_broadcast(netmidi2_ep_t *ep, const midi_ump_t *ump)
{
    if (ep == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < NETMIDI2_MAX_SESSIONS; i++) {
        if (ep->sessions[i].state == NETMIDI2_SESSION_ESTABLISHED) {
            netmidi2_send(&ep->sessions[i], ump);
        }
    }
    
    return ESP_OK;
}

esp_err_t netmidi2_ep_stop(netmidi2_ep_t *ep)
{
    if (ep == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ep->rx_task_handle) {
        vTaskDelete(ep->rx_task_handle);
        ep->rx_task_handle = NULL;
    }
    
    if (ep->sock >= 0) {
        close(ep->sock);
        ep->sock = -1;
    }
    
    return ESP_OK;
}
