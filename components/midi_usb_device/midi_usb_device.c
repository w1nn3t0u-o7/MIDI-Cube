#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "midi_usb_device.h"

static const char *TAG = "midi_usb_device";

// Static variables
static midi_usbd_config_t config = {
    .manufacturer = "MIDI Cube",
    .product = "USB MIDI Device",
    .serial = "0001",
    .midi_interface_name = "MIDI Interface",
    .vid = 0x1234,
    .pid = 0x5678,
    .rx_callback = NULL
};

static const char *str_desc[5];

static const uint8_t midi_cfg_desc[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64),
};

#if (TUD_OPT_HIGH_SPEED)
static uint8_t *s_midi_hs_cfg_desc = NULL;
#endif

// MIDI receive task
static void midi_rx_task(void *arg)
{
    uint8_t packet[4];
    midi_message_t msg;
    uint8_t cable;
    uint8_t cin;
    
    while (1) {
        vTaskDelay(1);
        
        while (tud_midi_available()) {
            if (tud_midi_packet_read(packet)) {
                ESP_LOGI(TAG, "RX: %02X %02X %02X %02X", 
                        packet[0], packet[1], packet[2], packet[3]);

                // Extract cable and CIN
                cable = (packet[0] >> 4) & 0x0F;
                cin = packet[0] & 0x0F;

                msg.status_byte = packet[1];
                msg.data[0] = packet[2];
                msg.data[1] = packet[3];

                
                if (config.rx_callback) {
                    config.rx_callback(cable, cin, &msg);
                }

            }
        }
    }
}

esp_err_t midi_usbd_init(void)
{
    if (config.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing USB MIDI device");
    
    str_desc[0] = (char[]){0x09, 0x04}; // Language: English
    str_desc[1] = config.manufacturer;
    str_desc[2] = config.product;
    str_desc[3] = config.serial;
    str_desc[4] = config.midi_interface_name;
    
#if (TUD_OPT_HIGH_SPEED)
    // High-speed configuration
    s_midi_hs_cfg_desc = malloc(TUSB_DESCRIPTOR_TOTAL_LEN);
    if (!s_midi_hs_cfg_desc) {
        free(s_str_desc);
        free(s_midi_cfg_desc);
        return ESP_ERR_NO_MEM;
    }
    
    desc = s_midi_hs_cfg_desc;
    memcpy(desc, config_desc, sizeof(config_desc));
    desc += sizeof(config_desc);
    
    uint8_t const midi_hs_desc[] = {
        TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 512)
    };
    memcpy(desc, midi_hs_desc, sizeof(midi_hs_desc));
#endif
    
    // Initialize TinyUSB
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.string = str_desc;
    tusb_cfg.descriptor.string_count = sizeof(str_desc) / sizeof(str_desc[0]);
    tusb_cfg.descriptor.full_speed_config = midi_cfg_desc;

#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = s_midi_cfg_desc,
        .hs_configuration_descriptor = s_midi_hs_cfg_desc,
        .qualifier_descriptor = NULL,
#endif
    
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(ret));
#if (TUD_OPT_HIGH_SPEED)
        free(s_midi_hs_cfg_desc);
#endif
        return ret;
    }
    
    // Start RX task
    BaseType_t task_ret = xTaskCreate(
        midi_rx_task,
        "midi_usb_rx",
        4096,
        NULL,
        5,
        NULL
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MIDI RX task");
        return ESP_FAIL;
    }
    
    config.is_initialized = true;
    ESP_LOGI(TAG, "USB MIDI device initialized successfully");
    return ESP_OK;
}

esp_err_t midi_usbd_register_rx_callback(midi_usbd_rx_cb_t callback)
{
    config.rx_callback = callback;
    return ESP_OK;
}

esp_err_t midi_usbd_send(uint8_t cable, const midi_message_t *msg)
{
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!tud_midi_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Buffer for MIDI message (max 3 bytes for standard messages)
    uint8_t buffer[3];
    uint8_t len = 0;
    
    // First byte is always the status byte
    buffer[len++] = msg->status_byte;
    
    // Determine number of data bytes based on status byte
    uint8_t status_type = MIDI1_MSG_GET_STATUS(msg);
    
    // Channel Voice Messages (0x80 - 0xEF)
    if (status_type >= 0x80 && status_type <= 0xE0) {
        switch (status_type) {
            case MIDI1_STATUS_NOTE_OFF: // Note Off (3 bytes)
            case MIDI1_STATUS_NOTE_ON: // Note On (3 bytes)
            case MIDI1_STATUS_POLY_PRESSURE: // Poly Pressure (3 bytes)
            case MIDI1_STATUS_CONTROL_CHANGE: // Control Change (3 bytes)
            case MIDI1_STATUS_PITCH_BEND: // Pitch Bend (3 bytes)
                buffer[len++] = msg->data[0] & 0x7F;
                buffer[len++] = msg->data[1] & 0x7F;
                break;
                
            case MIDI1_STATUS_PROGRAM_CHANGE: // Program Change (2 bytes)
            case MIDI1_STATUS_CHANNEL_PRESSURE: // Channel Pressure (2 bytes)
                buffer[len++] = msg->data[0] & 0x7F;
                break;
                
            default:
                return ESP_ERR_INVALID_ARG;
        }
    }
    // System Messages (0xF0 - 0xFF)
    else if (msg->status_byte >= 0xF0) {
        switch (msg->status_byte) {
            case MIDI1_STATUS_SYSEX_START: // SysEx Start (handled separately)
            case MIDI1_STATUS_SYSEX_END: // SysEx End (handled separately)
                return ESP_ERR_NOT_SUPPORTED; // Use separate SysEx function
                
            case MIDI1_STATUS_MTC_QUARTER_FRAME: // MIDI Time Code Quarter Frame (2 bytes)
            case MIDI1_STATUS_SONG_SELECT: // Song Select (2 bytes)
                buffer[len++] = msg->data[0] & 0x7F;
                break;
                
            case MIDI1_STATUS_SONG_POSITION: // Song Position Pointer (3 bytes)
                buffer[len++] = msg->data[0] & 0x7F;
                buffer[len++] = msg->data[1] & 0x7F;
                break;
                
            case MIDI1_STATUS_TUNE_REQUEST: // Tune Request (1 byte)
            case MIDI1_STATUS_TIMING_CLOCK: // Timing Clock (1 byte)
            case MIDI1_STATUS_START: // Start (1 byte)
            case MIDI1_STATUS_CONTINUE: // Continue (1 byte)
            case MIDI1_STATUS_STOP: // Stop (1 byte)
            case MIDI1_STATUS_ACTIVE_SENSING: // Active Sensing (1 byte)
            case MIDI1_STATUS_SYSTEM_RESET: // System Reset (1 byte)
                // No data bytes, just status
                break;
                
            default:
                return ESP_ERR_INVALID_ARG;
        }
    }
    else {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Send via TinyUSB
    uint32_t written = tud_midi_stream_write(cable & 0x0F, buffer, len);
    
    if (written != len) {
        ESP_LOGW(TAG, "USB MIDI write incomplete: %lu/%d bytes", written, len);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "USB MIDI sent: cable=%d, len=%d, data=[%02X %02X %02X]",
             cable, len, 
             buffer[0], 
             len > 1 ? buffer[1] : 0, 
             len > 2 ? buffer[2] : 0);
    
    return ESP_OK;
}

bool midi_usbd_is_mounted(void)
{
    return tud_midi_mounted();
}

