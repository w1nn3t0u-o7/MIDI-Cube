#include "midi_usb_device.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
        vTaskDelay(pdMS_TO_TICKS(1));
        
        while (tud_midi_available()) {
            if (tud_midi_packet_read(packet)) {
                ESP_LOGI(TAG, "RX: %02X %02X %02X %02X", 
                        packet[0], packet[1], packet[2], packet[3]);

                // Extract cable and CIN
                cable = (packet[0] >> 4) & 0x0F;
                cin = packet[0] & 0x0F;

                msg.status = packet[1];
                msg.channel = packet[1] & 0x0F;
                msg.data.bytes[0] = packet[2];
                msg.data.bytes[1] = packet[3];

                
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
    
    // uint8_t *desc = midi_cfg_desc;
    
    // // Configuration descriptor
    // uint8_t const config_desc[] = {
    //     TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100)
    // };
    // memcpy(desc, config_desc, sizeof(config_desc));
    // desc += sizeof(config_desc);
    
    // // MIDI interface descriptor
    // uint8_t const midi_desc[] = {
    //     TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64)
    // };
    // memcpy(desc, midi_desc, sizeof(midi_desc));
    
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
        "midi_rx",
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

// esp_err_t midi_usbd_send_note_on(uint8_t cable, uint8_t channel, uint8_t note, uint8_t velocity)
// {
//     if (!tud_midi_mounted()) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     uint8_t msg[3] = {
//         MIDI_NOTE_ON | (channel & 0x0F),
//         note & 0x7F,
//         velocity & 0x7F
//     };
    
//     uint32_t written = tud_midi_stream_write(cable, msg, 3);
//     return (written == 3) ? ESP_OK : ESP_FAIL;
// }

// esp_err_t midi_usbd_send_note_off(uint8_t cable, uint8_t channel, uint8_t note, uint8_t velocity)
// {
//     if (!tud_midi_mounted()) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     uint8_t msg[3] = {
//         MIDI_NOTE_OFF | (channel & 0x0F),
//         note & 0x7F,
//         velocity & 0x7F
//     };
    
//     uint32_t written = tud_midi_stream_write(cable, msg, 3);
//     return (written == 3) ? ESP_OK : ESP_FAIL;
// }

// esp_err_t midi_usbd_send_control_change(uint8_t cable, uint8_t channel, uint8_t controller, uint8_t value)
// {
//     if (!tud_midi_mounted()) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     uint8_t msg[3] = {
//         MIDI_CONTROL_CHANGE | (channel & 0x0F),
//         controller & 0x7F,
//         value & 0x7F
//     };
    
//     uint32_t written = tud_midi_stream_write(cable, msg, 3);
//     return (written == 3) ? ESP_OK : ESP_FAIL;
// }

// esp_err_t midi_usbd_send_program_change(uint8_t cable, uint8_t channel, uint8_t program)
// {
//     if (!tud_midi_mounted()) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     uint8_t msg[2] = {
//         MIDI_PROGRAM_CHANGE | (channel & 0x0F),
//         program & 0x7F
//     };
    
//     uint32_t written = tud_midi_stream_write(cable, msg, 2);
//     return (written == 2) ? ESP_OK : ESP_FAIL;
// }

// esp_err_t usb_midi_send_raw(uint8_t cable, const uint8_t *data, uint8_t len)
// {
//     if (!data || len == 0) {
//         return ESP_ERR_INVALID_ARG;
//     }
    
//     if (!tud_midi_mounted()) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     uint32_t written = tud_midi_stream_write(cable, data, len);
//     return (written == len) ? ESP_OK : ESP_FAIL;
// }

bool usb_midi_is_mounted(void)
{
    return tud_midi_mounted();
}

