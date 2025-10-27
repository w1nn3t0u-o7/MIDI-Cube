# ESP32-S3 MIDI Core Implementation - Review & Improvements

**Project**: Multi-Transport MIDI Interface (UART/DIN, USB 1.0/2.0, Ethernet, WiFi)  
**Date**: October 27, 2025  
**Component**: midi_core

---

## Table of Contents

1. [UMP Bit Field Bug Fix](#1-ump-bit-field-bug-fix)
2. [Comparison with AM_MIDI2.0Lib](#2-comparison-with-am_midi20lib)
3. [Missing MIDI 2.0 Features](#3-missing-midi-20-features)
4. [Implementation Roadmap](#4-implementation-roadmap)
5. [Code Quality Checklist](#5-code-quality-checklist)

---

## 1. UMP Bit Field Bug Fix

### Problem Identified

The original UMP MIDI 2.0 Note On parsing had incorrect bit field extraction. The note value was being read from the wrong bit positions.

### Root Cause

MIDI 2.0 Channel Voice Messages (MT 0x4, 64-bit) have this structure:
```
Word 0 (32 bits):
┌────────┬────────┬────────────────────────┬────────┬────────┐
│ MT │ Group │ Status + Channel │ Note │Index/0 │
│ 4 bits │ 4 bits │ 8 bits (combined) │ 8 bits │ 8 bits │
│ 31-28 │ 27-24 │ 23-16 │ 15-8 │ 7-0 │
└────────┴────────┴────────────────────────┴────────┴────────┘
0x4 0x0 0x90 (Status+Ch=0) 0x3C 0x00
```
```
Word 1 (32 bits):
┌─────────────────┬────────────┬─────────────────┐
│ Velocity │ Attr Type │ Attribute Data │
│ 16 bits │ 8 bits │ 16 bits │
│ 31-16 │ 15-8 │ 7-0 │
└─────────────────┴────────────┴─────────────────┘
```

**Key insight**: Status (0x9) and Channel (0x0) are **combined** into a single byte at bits 23-16.

### Corrected Macros

Add to `ump_defs.h`:
```c
/**

    @defgroup UMP_M2CV_EXTRACT MIDI 2.0 Channel Voice Field Extraction

    Based on UMP spec and AM_MIDI2.0Lib

    @{
    */

// Word 0 extraction
#define UMP_M2CV_GET_MT(w0) (((w0) >> 28) & 0x0F)
#define UMP_M2CV_GET_GROUP(w0) (((w0) >> 24) & 0x0F)
#define UMP_M2CV_GET_STATUS_BYTE(w0) (((w0) >> 16) & 0xFF) // Full status
#define UMP_M2CV_GET_STATUS_TYPE(w0) (((w0) >> 20) & 0x0F) // Upper nibble
#define UMP_M2CV_GET_CHANNEL(w0) (((w0) >> 16) & 0x0F) // Lower nibble
#define UMP_M2CV_GET_NOTE(w0) (((w0) >> 8) & 0xFF)
#define UMP_M2CV_GET_INDEX(w0) ((w0) & 0xFF)

// Word 1 extraction (Note On/Off specific)
#define UMP_M2CV_NOTE_GET_VELOCITY(w1) (((w1) >> 16) & 0xFFFF)
#define UMP_M2CV_NOTE_GET_ATTR_TYPE(w1) (((w1) >> 8) & 0xFF)
#define UMP_M2CV_NOTE_GET_ATTR_DATA(w1) ((w1) & 0xFFFF)

// Word 0 construction
#define UMP_M2CV_BUILD_W0(mt, grp, status, note, idx)
(((uint32_t)(mt) << 28) | ((uint32_t)(grp) << 24) |
((uint32_t)(status) << 16) | ((uint32_t)(note) << 8) |
((uint32_t)(idx) << 0))

// Word 1 construction (Note On/Off)
#define UMP_M2CV_NOTE_BUILD_W1(vel, attr_type, attr_data)
(((uint32_t)(vel) << 16) | ((uint32_t)(attr_type) << 8) |
((uint32_t)(attr_data) << 0))

/** @} */
```

### Fixed UMP Builder Function

Update `ump_message.c`:
```c
esp_err_t ump_build_midi2_note_on(
uint8_t group, uint8_t channel, uint8_t note,
uint16_t velocity16, uint8_t attr_type, uint16_t attr_data,
ump_packet_t *packet_out)
{
if (!packet_out || group > 15 || channel > 15 || note > 127) {
return ESP_ERR_INVALID_ARG;
}

text
// Build status byte (Note On = 0x90 + channel)
uint8_t status = 0x90 | (channel & 0x0F);

// Word 0: MT + Group + Status + Note + Index
packet_out->words = UMP_M2CV_BUILD_W0(
    UMP_MT_MIDI2_CHANNEL_VOICE,  // 0x4
    group,
    status,   // Full status byte (0x90-0x9F)
    note,
    0         // Index (attribute index, usually 0)
);

// Word 1: Velocity + Attr Type + Attr Data
packet_out->words = UMP_M2CV_NOTE_BUILD_W1([1]
    velocity16,
    attr_type,
    attr_data
);

packet_out->num_words = 2;
packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
packet_out->group = group;
packet_out->timestamp_us = 0;

return ESP_OK;

}
```

### Fixed Test Code

Update `main.c` test:
```c
void test_ump_parser_midi2_note(void) {
ESP_LOGI(TAG, "=== Test 4: UMP Parser - MIDI 2.0 Note On ===");

text
// Construct using builder (tests both building and parsing)
ump_packet_t built_packet;
esp_err_t err = ump_build_midi2_note_on(
    0,      // group
    0,      // channel
    60,     // note (Middle C)
    32768,  // velocity (center)
    0,      // attribute type
    0,      // attribute data
    &built_packet
);

TEST_ASSERT_EQUAL(ESP_OK, err);

ESP_LOGI(TAG, "Built UMP:");
ESP_LOGI(TAG, "  Word 0: 0x%08lX", built_packet.words);
ESP_LOGI(TAG, "  Word 1: 0x%08lX", built_packet.words);[1]

// Parse it back
ump_packet_t parsed_packet;
err = ump_parser_parse_packet(built_packet.words, &parsed_packet);
TEST_ASSERT_EQUAL(ESP_OK, err);

// Extract and verify using correct macros
uint8_t note = UMP_M2CV_GET_NOTE(parsed_packet.words);
uint16_t velocity = UMP_M2CV_NOTE_GET_VELOCITY(parsed_packet.words);[1]
uint8_t channel = UMP_M2CV_GET_CHANNEL(parsed_packet.words);

ESP_LOGI(TAG, "Parsed: Note=%d, Velocity=%d, Channel=%d", 
         note, velocity, channel);

TEST_ASSERT_EQUAL_UINT8(60, note);
TEST_ASSERT_EQUAL_UINT16(32768, velocity);
TEST_ASSERT_EQUAL_UINT8(0, channel);

ESP_LOGI(TAG, "✓✓ Round-trip test passed!");

}
```

---

## 2. Comparison with AM_MIDI2.0Lib

### What is AM_MIDI2.0Lib?

- Official MIDI Association C++ reference implementation
- ~10KB compiled, ~1KB RAM
- Stream-based processing
- Modular architecture

### Key Differences

| Aspect | Your Implementation | AM_MIDI2.0Lib |
|--------|---------------------|---------------|
| **Language** | C (embedded-friendly) | C++ (type safety) |
| **Architecture** | Direct packet parsing | Stream processing |
| **Memory** | Stack-only, explicit | Some heap usage |
| **Target** | ESP32-S3 specific | Platform-agnostic |
| **Integration** | ESP-IDF native | Requires porting |

### What Your Code Does Better

1. **ESP-IDF Integration** - Native use of `esp_err_t`, logging, FreeRTOS
2. **Explicit Memory Management** - No hidden allocations, real-time safe
3. **Clear Separation** - MIDI 1.0 vs MIDI 2.0 clearly delineated
4. **Embedded Optimization** - Designed for resource-constrained systems

### What to Adopt from AM_MIDI2.0Lib

#### 1. Validation Functions

Add to `ump_parser.c`:
```c
/**

    @brief Validate UMP packet structure
    */
    bool ump_is_valid(const ump_packet_t *packet) {
    if (!packet) return false;

    uint8_t mt = UMP_GET_MT(packet->words);

    // Check word count matches MT
    uint8_t expected_words = ump_get_packet_size(mt);
    if (packet->num_words != expected_words) return false;

    // Check group is valid (0-15)
    if (packet->group > 15) return false;

    return true;
    }

/**

    @brief Get human-readable message type string
    /
    const char ump_get_mt_string(uint8_t mt) {
    static const char *mt_strings[] = {
    "Utility", "System", "MIDI 1.0 CV", "Data64",
    "MIDI 2.0 CV", "Data128", "Reserved6", "Reserved7",
    "Reserved8", "Reserved9", "ReservedA", "ReservedB",
    "ReservedC", "Flex Data", "ReservedE", "Stream"
    };
    return (mt < 16) ? mt_strings[mt] : "Invalid";
    }
```

#### 2. Helper Types

Add to `ump_types.h`:
```c
/**

    @brief MIDI 2.0 velocity with conversion helpers
    */
    typedef struct {
    uint16_t value; // 16-bit (0-65535)
    } midi2_velocity_t;

// Create from 7-bit MIDI 1.0 velocity
static inline midi2_velocity_t midi2_velocity_from_7bit(uint8_t vel7) {
return (midi2_velocity_t){ .value = midi_upscale_7to16(vel7) };
}

// Convert to 7-bit for MIDI 1.0
static inline uint8_t midi2_velocity_to_7bit(midi2_velocity_t vel) {
return midi_downscale_16to7(vel.value);
}
```

---

## 3. Missing MIDI 2.0 Features

### Current Implementation Status ✅

- ✅ Basic UMP packet structure
- ✅ MIDI 1.0 byte stream parser with running status
- ✅ MIDI 2.0 Note On/Off (64-bit, MT 0x4)
- ✅ Basic translation (MIDI 1.0 ↔ MIDI 2.0)
- ✅ Min-Center-Max upscaling algorithm

### Tier 1: Essential Features (HIGH PRIORITY) 🔴

#### Missing MIDI 2.0 Channel Voice Messages

| Message | Status | Resolution | Implementation |
|---------|--------|------------|----------------|
| Poly Pressure | 0xA0 | 32-bit | ❌ Missing |
| Control Change | 0xB0 | 32-bit | ❌ Missing |
| Program Change | 0xC0 | Bank Select | ❌ Missing |
| Channel Pressure | 0xD0 | 32-bit | ❌ Missing |
| Pitch Bend | 0xE0 | 32-bit | ❌ Missing |

#### Implementation Examples

**Control Change (32-bit resolution)**:
```c
esp_err_t ump_build_midi2_control_change(
uint8_t group, uint8_t channel, uint8_t controller,
uint32_t value32, // Full 32-bit resolution!
ump_packet_t *packet_out)
{
if (!packet_out || group > 15 || channel > 15 || controller > 127) {
return ESP_ERR_INVALID_ARG;
}

text
uint8_t status = 0xB0 | (channel & 0x0F);

packet_out->words = 
    ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
    ((uint32_t)group << 24) |
    ((uint32_t)status << 16) |
    ((uint32_t)controller << 8) |
    0x00;

packet_out->words = value32;[1]

packet_out->num_words = 2;
packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
packet_out->group = group;

return ESP_OK;

}
```

**Pitch Bend (32-bit)**:
```c
esp_err_t ump_build_midi2_pitch_bend(
uint8_t group, uint8_t channel,
uint32_t value32, // Center = 0x80000000
ump_packet_t *packet_out)
{
if (!packet_out || group > 15 || channel > 15) {
return ESP_ERR_INVALID_ARG;
}

text
uint8_t status = 0xE0 | (channel & 0x0F);

packet_out->words = 
    ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
    ((uint32_t)group << 24) |
    ((uint32_t)status << 16) |
    0x0000;

packet_out->words = value32;[1]

packet_out->num_words = 2;
packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
packet_out->group = group;

return ESP_OK;

}
```

**Program Change with Bank Select**:
```c
esp_err_t ump_build_midi2_program_change(
uint8_t group, uint8_t channel, uint8_t program,
bool bank_valid, uint8_t bank_msb, uint8_t bank_lsb,
ump_packet_t *packet_out)
{
if (!packet_out || group > 15 || channel > 15 || program > 127) {
return ESP_ERR_INVALID_ARG;
}
```
```c
uint8_t status = 0xC0 | (channel & 0x0F);
uint8_t options = bank_valid ? 0x01 : 0x00;

packet_out->words = 
    ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
    ((uint32_t)group << 24) |
    ((uint32_t)status << 16) |
    ((uint32_t)program << 8) |
    options;

packet_out->words =[1]
    ((uint32_t)bank_msb << 8) |
    ((uint32_t)bank_lsb << 0);

packet_out->num_words = 2;
packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
packet_out->group = group;

return ESP_OK;

}
```

#### System Real-Time Messages (MT 0x1)

Critical for synchronization:
```c
esp_err_t ump_build_system_realtime(
uint8_t group, uint8_t status, // 0xF8-0xFF
ump_packet_t *packet_out)
{
if (!packet_out || status < 0xF8) {
return ESP_ERR_INVALID_ARG;
}
```
```c
packet_out->words = 
    ((uint32_t)UMP_MT_SYSTEM << 28) |
    ((uint32_t)group << 24) |
    ((uint32_t)status << 16) |
    0x0000;

packet_out->num_words = 1;
packet_out->message_type = UMP_MT_SYSTEM;
packet_out->group = group;

return ESP_OK;

}
```

### Tier 2: Advanced Features (MEDIUM PRIORITY) 🟡

#### Per-Note Controllers

Revolutionary MIDI 2.0 feature - independent control per note:
```c
/**

    @brief MIDI 2.0 Per-Note Pitch Bend
    */
    typedef struct {
    uint8_t group;
    uint8_t channel;
    uint8_t note; // Which note to bend
    uint32_t pitch_bend; // 32-bit (center = 0x80000000)
    } midi2_per_note_pitch_bend_t;

/**

    @brief Registered Per-Note Controller (RPNC)
    */
    typedef struct {
    uint8_t group;
    uint8_t channel;
    uint8_t note;
    uint8_t controller; // 0-255
    uint32_t value; // 32-bit value
    } midi2_per_note_controller_t;
```

#### Registered/Assignable Controllers

Unified RPN/NRPN in single message:
```c
esp_err_t ump_build_midi2_registered_controller(
uint8_t group, uint8_t channel,
uint8_t bank, uint8_t index,
uint32_t value32,
ump_packet_t *packet_out)
{
uint8_t status = 0x20 | (channel & 0x0F); // RPN status
```
```c
packet_out->words = 
    ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
    ((uint32_t)group << 24) |
    ((uint32_t)status << 16) |
    ((uint32_t)bank << 8) |
    index;

packet_out->words = value32;[1]

packet_out->num_words = 2;
packet_out->message_type = UMP_MT_MIDI2_CHANNEL_VOICE;
packet_out->group = group;

return ESP_OK;

}
```

### Tier 3: UMP Stream (LOW PRIORITY) 🟢

Required for true MIDI 2.0 device implementation:
```c
/**

    @brief Endpoint Info Notification
    */
    typedef struct {
    uint8_t ump_version_major;
    uint8_t ump_version_minor;
    uint8_t num_function_blocks;
    bool static_function_blocks;
    bool midi2_protocol;
    bool midi1_protocol;
    bool rx_jr_timestamp;
    bool tx_jr_timestamp;
    } ump_endpoint_info_t;
```

---

## 4. Implementation Roadmap

### Phase 1: Complete Basic MIDI 2.0 Channel Voice (2 weeks)

- ✅ Note On/Off (done)
- ⬜ Poly Pressure
- ⬜ Control Change (32-bit)
- ⬜ Program Change (with Bank Select)
- ⬜ Channel Pressure
- ⬜ Pitch Bend (32-bit)
- ⬜ System Real-Time (Clock, Start, Stop)

### Phase 2: SysEx Support (1 week)

- ⬜ SysEx 7-bit (MT 0x3)
- ⬜ SysEx 8-bit (MT 0x5)
- ⬜ Fragmentation handling

### Phase 3: Advanced Controllers (2 weeks)

- ⬜ Registered Controllers (RPN)
- ⬜ Assignable Controllers (NRPN)
- ⬜ Per-Note Controllers
- ⬜ Relative Controllers

### Phase 4: UMP Stream (When needed)

- ⬜ Endpoint Discovery
- ⬜ Function Block Discovery
- ⬜ Jitter Reduction Timestamps


### Recommended Code Structure
```
components/midi_core/
├── src/
│ ├── midi_parser.c // MIDI 1.0 byte stream
│ ├── ump_parser.c // UMP packet parsing
│ ├── ump_channel_voice.c // MT 0x4 messages ← ADD
│ ├── ump_system.c // MT 0x1 messages ← ADD
│ ├── ump_sysex.c // MT 0x3, 0x5 ← ADD
│ ├── ump_stream.c // MT 0xF (later)
│ ├── ump_utility.c // MT 0x0 (later)
│ └── midi_translator.c // Translation logic
```

---

## 5. Code Quality Checklist

### ✅ What's Good

- [x] Clean separation of MIDI 1.0 and MIDI 2.0
- [x] ESP-IDF integration is proper
- [x] Memory management is explicit and safe
- [x] Translation layer exists
- [x] Modular architecture

### ⚠️ What Needs Fixing

- [ ] UMP bit field extraction (STATUS_BYTE vs STATUS_NIBBLE)
- [ ] Missing validation functions
- [ ] Need more helper macros
- [ ] Test coverage for edge cases

### 📚 Lessons Learned

1. **Binary Protocol Gotcha**: Bit field positioning must match spec exactly
2. **C vs C++**: C is fine for embedded; don't need C++ overhead
3. **Official References**: AM_MIDI2.0Lib is excellent reference
4. **Incremental Development**: Start with core, add features as needed