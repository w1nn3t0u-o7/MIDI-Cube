/**
 * @file test_midi_core.c
 * @brief Interactive test harness for midi_core component
 * 
 * Call midi_core_run_tests() from main.c to execute all tests
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

// Include midi_core headers
#include "midi_defs.h"
#include "midi_types.h"
#include "midi_parser.h"
#include "midi_message.h"
#include "ump_defs.h"
#include "ump_types.h"
#include "ump_parser.h"
#include "midi_translator.h"

static const char *TAG = "midi_test";

// Test data: Simulated MIDI byte streams
static const uint8_t test_note_on[] = {0x90, 0x3C, 0x64};
static const uint8_t test_running_status[] = {0x90, 0x3C, 0x64, 0x40, 0x70};
static const uint8_t test_realtime[] = {0x90, 0x3C, 0xF8, 0x64};

/**
 * @brief Test 1: MIDI 1.0 Parser - Single Message
 */
void test_midi_parser_single_message(void) {
    ESP_LOGI(TAG, "=== Test 1: MIDI 1.0 Parser - Single Message ===");
    
    midi_parser_state_t parser;
    uint8_t sysex_buffer[128];
    midi_parser_init(&parser, sysex_buffer, sizeof(sysex_buffer));
    
    midi_message_t msg;
    bool complete;
    
    // Parse Note On byte by byte
    for (int i = 0; i < sizeof(test_note_on); i++) {
        esp_err_t err = midi_parser_parse_byte(&parser, test_note_on[i], &msg, &complete);
        
        ESP_LOGI(TAG, "Byte %d: 0x%02X - Complete: %s", 
                 i, test_note_on[i], complete ? "YES" : "NO");
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Parse error at byte %d", i);
            return;
        }
    }
    
    // Verify result
    if (complete) {
        ESP_LOGI(TAG, "✓ Message parsed successfully!");
        ESP_LOGI(TAG, "  Status: 0x%02X", msg.status);
        ESP_LOGI(TAG, "  Channel: %d", msg.channel);
        ESP_LOGI(TAG, "  data.bytes[0] (Note): %d", msg.data.bytes[0]);
        ESP_LOGI(TAG, "  data.bytes[1] (Velocity): %d", msg.data.bytes[1]);
        
        // Validate values
        if (msg.status == 0x90 && msg.channel == 0 && 
            msg.data.bytes[0] == 60 && msg.data.bytes[1] == 100) {
            ESP_LOGI(TAG, "✓✓ All values correct!");
        } else {
            ESP_LOGE(TAG, "✗ Values incorrect!");
        }
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Test 2: MIDI 1.0 Parser - Running Status
 */
void test_midi_parser_running_status(void) {
    ESP_LOGI(TAG, "=== Test 2: MIDI 1.0 Parser - Running Status ===");
    
    midi_parser_state_t parser;
    uint8_t sysex_buffer[128];
    midi_parser_init(&parser, sysex_buffer, sizeof(sysex_buffer));
    
    midi_message_t msg;
    bool complete;
    int message_count = 0;
    
    // Parse bytes: [0x90, 0x3C, 0x64, 0x40, 0x70]
    // Should produce TWO Note On messages
    for (int i = 0; i < sizeof(test_running_status); i++) {
        esp_err_t err = midi_parser_parse_byte(&parser, test_running_status[i], &msg, &complete);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Parse error at byte %d", i);
            return;
        }
        
        if (complete) {
            message_count++;
            ESP_LOGI(TAG, "Message %d: Note %d, Velocity %d", 
                     message_count, msg.data.bytes[0], msg.data.bytes[1]);
        }
    }
    
    if (message_count == 2) {
        ESP_LOGI(TAG, "✓ Running Status works correctly!");
    } else {
        ESP_LOGE(TAG, "✗ Expected 2 messages, got %d", message_count);
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Test 3: MIDI 1.0 Parser - Real-Time Message Injection
 */
void test_midi_parser_realtime(void) {
    ESP_LOGI(TAG, "=== Test 3: MIDI 1.0 Parser - Real-Time Injection ===");
    
    midi_parser_state_t parser;
    uint8_t sysex_buffer[128];
    midi_parser_init(&parser, sysex_buffer, sizeof(sysex_buffer));
    
    midi_message_t msg;
    bool complete;
    int note_msg_count = 0;
    int realtime_msg_count = 0;
    
    // Parse bytes: [0x90, 0x3C, 0xF8, 0x64]
    // Should produce: 1 Clock message, then 1 Note On message
    for (int i = 0; i < sizeof(test_realtime); i++) {
        esp_err_t err = midi_parser_parse_byte(&parser, test_realtime[i], &msg, &complete);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Parse error at byte %d", i);
            return;
        }
        
        if (complete) {
            if (msg.status == 0xF8) {
                realtime_msg_count++;
                ESP_LOGI(TAG, "  Clock message received (correct!)");
            } else if (msg.status == 0x90) {
                note_msg_count++;
                ESP_LOGI(TAG, "  Note On received: Note %d, Vel %d", msg.data.bytes[0], msg.data.bytes[1]);
            }
        }
    }
    
    if (note_msg_count == 1 && realtime_msg_count == 1) {
        ESP_LOGI(TAG, "✓ Real-Time message handling correct!");
    } else {
        ESP_LOGE(TAG, "✗ Expected 1 Note + 1 Clock, got %d Note + %d Clock", 
                 note_msg_count, realtime_msg_count);
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Test 4: UMP Parser - MIDI 2.0 Note On
 */
void test_ump_parser_midi2_note(void) {
    ESP_LOGI(TAG, "=== Test 4: UMP Parser - MIDI 2.0 Note On ===");
    
    // Construct MIDI 2.0 Note On UMP packet
    // MT=0x4, Group=0, Status=0x90, Channel=0, Note=60
    uint32_t words[2] = {
        0x49003C00,  // Word 0
        0x80000000   // Word 1: Velocity=32768 (center)
    };
    
    ump_packet_t packet;
    esp_err_t err = ump_parser_parse_packet(words, &packet);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ UMP parse failed!");
        return;
    }
    
    ESP_LOGI(TAG, "✓ UMP parsed successfully!");
    ESP_LOGI(TAG, "  Message Type: 0x%X", packet.message_type);
    ESP_LOGI(TAG, "  Group: %d", packet.group);
    ESP_LOGI(TAG, "  Num Words: %d", packet.num_words);
    ESP_LOGI(TAG, "  Word 0: 0x%08lX", packet.words[0]);
    ESP_LOGI(TAG, "  Word 1: 0x%08lX", packet.words[1]);
    
    // Extract note and velocity
    uint8_t status = (packet.words[0] >> 16) & 0xFF;
    uint8_t channel = (packet.words[0] >> 16) & 0x0F;
    uint8_t note = (packet.words[0] >> 8) & 0xFF;
    uint16_t velocity = packet.words[1] >> 16;
    
    ESP_LOGI(TAG, "  Decoded: Status=0x%02X, Ch=%d, Note=%d, Vel=%d", 
             status, channel, note, velocity);
    
    if (packet.message_type == UMP_MT_MIDI2_CHANNEL_VOICE && 
        packet.num_words == 2 && note == 60 && velocity == 32768) {
        ESP_LOGI(TAG, "✓✓ All UMP values correct!");
    } else {
        ESP_LOGE(TAG, "✗ UMP values incorrect!");
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Test 5: Translation - MIDI 1.0 to MIDI 2.0
 */
void test_translation_1to2(void) {
    ESP_LOGI(TAG, "=== Test 5: Translation - MIDI 1.0 to MIDI 2.0 ===");
    
    // Create MIDI 1.0 Note On
    midi_message_t midi1_msg = {
        .type = MIDI_MSG_TYPE_CHANNEL,
        .status = 0x90,
        .channel = 0,
        .data.bytes[0] = 60,    // Middle C
        .data.bytes[1] = 64,    // Center velocity (7-bit)
    };
    
    ESP_LOGI(TAG, "Input MIDI 1.0:");
    ESP_LOGI(TAG, "  Status: 0x%02X", midi1_msg.status);
    ESP_LOGI(TAG, "  Note: %d", midi1_msg.data.bytes[0]);
    ESP_LOGI(TAG, "  Velocity (7-bit): %d", midi1_msg.data.bytes[1]);
    
    // Translate to MIDI 2.0
    ump_packet_t ump_out;
    esp_err_t err = midi_translate_1to2(&midi1_msg, &ump_out);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Translation failed!");
        return;
    }
    
    ESP_LOGI(TAG, "✓ Translation successful!");
    ESP_LOGI(TAG, "Output UMP (MIDI 2.0):");
    ESP_LOGI(TAG, "  MT: 0x%X", ump_out.message_type);
    ESP_LOGI(TAG, "  Word 0: 0x%08lX", ump_out.words[0]);
    ESP_LOGI(TAG, "  Word 1: 0x%08lX", ump_out.words[1]);
    
    uint16_t velocity16 = ump_out.words[1] >> 16;
    ESP_LOGI(TAG, "  Velocity (16-bit): %d", velocity16);
    
    // Check if center value (64) mapped to center (32768)
    if (velocity16 == 32768) {
        ESP_LOGI(TAG, "✓✓ Center value preserved correctly! (64 → 32768)");
    } else {
        ESP_LOGW(TAG, "⚠ Center value not exact: expected 32768, got %d", velocity16);
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Test 6: Translation - MIDI 2.0 to MIDI 1.0
 */
void test_translation_2to1(void) {
    ESP_LOGI(TAG, "=== Test 6: Translation - MIDI 2.0 to MIDI 1.0 ===");
    
    // Create MIDI 2.0 Note On UMP
    ump_packet_t ump_in = {
        .words = {0x40906000, 0xCCCC0000, 0, 0},  // Velocity = 0xCCCC (52428)
        .num_words = 2,
        .message_type = UMP_MT_MIDI2_CHANNEL_VOICE,
        .group = 0
    };
    
    uint16_t velocity16 = ump_in.words[1] >> 16;
    ESP_LOGI(TAG, "Input UMP (MIDI 2.0):");
    ESP_LOGI(TAG, "  Velocity (16-bit): %d", velocity16);
    
    // Translate to MIDI 1.0
    midi_message_t midi1_out;
    esp_err_t err = midi_translate_2to1(&ump_in, &midi1_out);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Translation failed!");
        return;
    }
    
    ESP_LOGI(TAG, "✓ Translation successful!");
    ESP_LOGI(TAG, "Output MIDI 1.0:");
    ESP_LOGI(TAG, "  Status: 0x%02X", midi1_out.status);
    ESP_LOGI(TAG, "  Note: %d", midi1_out.data.bytes[0]);
    ESP_LOGI(TAG, "  Velocity (7-bit): %d", midi1_out.data.bytes[1]);
    
    // 52428 >> 9 = 102
    uint8_t expected = velocity16 >> 9;
    if (midi1_out.data.bytes[1] == expected) {
        ESP_LOGI(TAG, "✓✓ Downscaling correct! (52428 → %d)", expected);
    } else {
        ESP_LOGE(TAG, "✗ Expected %d, got %d", expected, midi1_out.data.bytes[1]);
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Test 7: Upscaling Algorithm Verification
 */
void test_upscaling_algorithm(void) {
    ESP_LOGI(TAG, "=== Test 7: Upscaling Algorithm - Critical Points ===");
    
    // Test critical values: 0, 64 (center), 127 (max)
    uint8_t test_values[] = {0, 1, 63, 64, 65, 126, 127};
    uint16_t expected[] = {0, 520, 32767, 32768, 33288, 65015, 65535};
    
    bool all_correct = true;
    
    for (int i = 0; i < sizeof(test_values); i++) {
        uint16_t result = midi_upscale_7to16(test_values[i]);
        bool correct = (result == expected[i]);
        
        ESP_LOGI(TAG, "  %3d → %5d %s (expected %5d)", 
                 test_values[i], result, 
                 correct ? "✓" : "✗", expected[i]);
        
        if (!correct) all_correct = false;
    }
    
    if (all_correct) {
        ESP_LOGI(TAG, "✓✓ Upscaling algorithm correct!");
    } else {
        ESP_LOGE(TAG, "✗ Upscaling algorithm has errors!");
    }
    
    ESP_LOGI(TAG, "");
}

/**
 * @brief Run all MIDI core tests
 * 
 * Call this from main() to run test suite
 */
void midi_core_run_tests(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "  MIDI Core Component Test Suite");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    test_midi_parser_single_message();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_midi_parser_running_status();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_midi_parser_realtime();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_ump_parser_midi2_note();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_translation_1to2();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_translation_2to1();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    test_upscaling_algorithm();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "  All Tests Complete!");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "");
}
